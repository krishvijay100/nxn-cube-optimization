"""FastAPI service exposing the C++ Kociemba solver.

single POST /solve endpoint that takes a scramble (list of move tokens) and
returns a solution
"""

from __future__ import annotations

import os
import re
import secrets
from typing import Annotated

from fastapi import FastAPI, HTTPException
from fastapi.concurrency import run_in_threadpool
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field, model_validator

import nxn_cube_py as nc


MAX_SCRAMBLE_LEN = 40
MIN_CUBE_SIZE = 3
MAX_CUBE_SIZE = 7
MOVE_RE = re.compile(r"^(?:(\d+))?([URFDLB])(w)?([2']?)$")


def _invalid_move(token: str, n: int) -> bool:
    match = MOVE_RE.fullmatch(token)
    if match is None:
        return True
    prefix, _face, wide, _turn = match.groups()
    if prefix is None:
        layers = 2 if wide else 1
    else:
        layers = int(prefix)
        if layers < 2:
            return True
    if layers >= n:
        return True

    outer_depth = 0 if wide or prefix is None else layers - 1
    inner_depth = layers - 1 if wide or prefix is not None else 0
    middle_depth = n // 2
    return (
        n % 2 == 1
        and outer_depth <= middle_depth <= inner_depth
    )


class SolveRequest(BaseModel):
    n: Annotated[int, Field(ge=MIN_CUBE_SIZE, le=MAX_CUBE_SIZE)] = 3
    scramble: Annotated[list[str], Field(max_length=MAX_SCRAMBLE_LEN)]

    @model_validator(mode="after")
    def _validate_moves(self) -> "SolveRequest":
        bad = [token for token in self.scramble if _invalid_move(token, self.n)]
        if bad:
            raise ValueError(f"invalid move tokens: {bad}")
        return self


class SolveResponse(BaseModel):
    n: int
    solution: list[str]
    num_moves: int


class ScrambleResponse(BaseModel):
    n: int
    scramble: list[str]
    seed: int


app = FastAPI(title="nxn-cube-service", version="0.1.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_methods=["GET", "POST"],
    allow_headers=["Content-Type"],
)


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.post("/solve", response_model=SolveResponse)
async def solve(req: SolveRequest) -> SolveResponse:
    # off-load the blocking C++ solve to the default threadpool so the async
    # event loop stays responsive for other concurrent requests
    try:
        solution = await run_in_threadpool(nc.solve, req.scramble, req.n)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"solver error: {e}") from e
    return SolveResponse(n=req.n, solution=solution, num_moves=len(solution))


DEFAULT_SCRAMBLE_LEN = 20


@app.get("/scramble", response_model=ScrambleResponse)
async def scramble(
    length: int = DEFAULT_SCRAMBLE_LEN,
    n: int = 3,
) -> ScrambleResponse:
    if not 1 <= length <= MAX_SCRAMBLE_LEN:
        raise HTTPException(status_code=422, detail=f"length must be in [1, {MAX_SCRAMBLE_LEN}]")
    if not MIN_CUBE_SIZE <= n <= MAX_CUBE_SIZE:
        raise HTTPException(
            status_code=422,
            detail=f"n must be in [{MIN_CUBE_SIZE}, {MAX_CUBE_SIZE}]",
        )
    seed = secrets.randbits(63)
    tokens = await run_in_threadpool(nc.random_scramble, length, seed, n)
    return ScrambleResponse(n=n, scramble=tokens, seed=seed)


# serve the built frontend at "/" when it's present (eg inside the docker
# image, where stage 1 emits /app/static). in local dev the vite server owns
# the ui on :5173 and this directory doesn't exist, so we skip the mount
_STATIC_DIR = os.environ.get("STATIC_DIR", "/app/static")
if os.path.isdir(_STATIC_DIR):
    app.mount("/", StaticFiles(directory=_STATIC_DIR, html=True), name="frontend")
