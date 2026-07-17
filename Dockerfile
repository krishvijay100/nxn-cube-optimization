# multi-stage build:
#   stage 1 (frontend-builder) — node builds the react/three.js visualizer into
#     static files (discarded after stage 3 copies bundle)
#   stage 2 (builder) — full c++ toolchain compiles the pybind module (discarded after stage 3 copies bundle)
#   stage 3 (runtime) — slim python base + fastapi + the compiled .so + the
#     built frontend. one image serves both the api and the ui, so a single
#     `docker run -p 8000:8000 nxn-cube` opens localhost:8000 as full demo

# stage 1: frontend builder
FROM node:20-slim AS frontend-builder

WORKDIR /web

# copy just the lockfiles first so unchanged deps reuse the npm-install cache
COPY web/package.json web/package-lock.json ./
RUN npm ci

COPY web/ ./
RUN npm run build   # emits /web/dist/

# stage 2: c++ builder
FROM python:3.12-bookworm AS builder

# system deps for compiling the c++20 solver + pybind11 module
# clean apt lists after install to shrink this stage's layers
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# copy just the c++ source + build config first so a code-only change doesn't
# invalidate the pip-install layer downstream
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY tests/ ./tests/
COPY benchmarks/ ./benchmarks/
COPY bindings/ ./bindings/
COPY app/ ./app/
COPY tools/ ./tools/

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target nxn_cube_py

# stage 3: runtime
FROM python:3.12-slim-bookworm AS runtime

RUN useradd --create-home --shell /bin/bash app

WORKDIR /app

# install runtime python deps first, this layer only re-runs if pyproject.toml
# changes so day-to-day code edits reuse it from cache
COPY service/pyproject.toml ./service/pyproject.toml
RUN pip install --no-cache-dir \
        "fastapi>=0.115" \
        "uvicorn[standard]>=0.30" \
        "pydantic>=2.7"

COPY --from=builder /src/build/bindings/nxn_cube_py*.so /app/nxn_cube_py.so

COPY service/app.py ./service/app.py

COPY --from=frontend-builder /web/dist /app/static

ENV PYTHONPATH=/app
EXPOSE 8000

USER app

CMD ["uvicorn", "app:app", "--app-dir", "service", "--host", "0.0.0.0", "--port", "8000"]