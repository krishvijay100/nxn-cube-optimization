set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINDINGS_DIR="$REPO_ROOT/build/bindings"
SERVICE_DIR="$REPO_ROOT/service"
PORT="${PORT:-8000}"
BASE_URL="http://127.0.0.1:$PORT"

if [[ ! -d "$BINDINGS_DIR" ]] || ! ls "$BINDINGS_DIR"/nxn_cube_py*.so >/dev/null 2>&1; then
    echo "error: compiled nxn_cube_py not found in $BINDINGS_DIR" >&2
    echo "run: cmake --build build --target nxn_cube_py" >&2
    exit 1
fi

if [[ ! -x "$SERVICE_DIR/.venv/bin/uvicorn" ]]; then
    echo "error: service venv not set up. run:" >&2
    echo "  cd service && python3 -m venv .venv && .venv/bin/pip install -e '.[dev]'" >&2
    exit 1
fi

echo "starting uvicorn on port $PORT..."
PYTHONPATH="$SERVICE_DIR:$BINDINGS_DIR" \
    "$SERVICE_DIR/.venv/bin/uvicorn" app:app \
    --port "$PORT" --log-level warning \
    --app-dir "$SERVICE_DIR" &
UVICORN_PID=$!
trap 'kill $UVICORN_PID 2>/dev/null; wait 2>/dev/null || true' EXIT

# wait up to 10s for /health to respond
for _ in $(seq 1 50); do
    if curl -sS "$BASE_URL/health" >/dev/null 2>&1; then break; fi
    sleep 0.2
done
if ! curl -sS "$BASE_URL/health" >/dev/null 2>&1; then
    echo "uvicorn didn't come up within 10s" >&2
    exit 1
fi

echo "running k6..."
BASE_URL="$BASE_URL" k6 run "$REPO_ROOT/loadtest/solve_load.js"
