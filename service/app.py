"""FastAPI service exposing the C++ Kociemba solver.

single POST /solve endpoint that takes a scramble (list of move tokens) and
returns a solution
"""

from __future__ import annotations

import secrets
from typing import Annotated

from fastapi import FastAPI, HTTPException
from fastapi.concurrency import run_in_threadpool
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field, field_validator

import nxn_cube_py as nc


LEGAL_MOVES = frozenset({
    "U", "U'", "U2",
    "D", "D'", "D2",
    "R", "R'", "R2",
    "L", "L'", "L2",
    "F", "F'", "F2",
    "B", "B'", "B2",
})

MAX_SCRAMBLE_LEN = 40


class SolveRequest(BaseModel):
    scramble: Annotated[list[str], Field(max_length=MAX_SCRAMBLE_LEN)]

    @field_validator("scramble")
    @classmethod
    def _validate_moves(cls, tokens: list[str]) -> list[str]:
        bad = [t for t in tokens if t not in LEGAL_MOVES]
        if bad:
            raise ValueError(f"invalid move tokens: {bad}")
        return tokens


class SolveResponse(BaseModel):
    solution: list[str]
    num_moves: int


class ScrambleResponse(BaseModel):
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
        solution = await run_in_threadpool(nc.solve, req.scramble)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"solver error: {e}") from e
    return SolveResponse(solution=solution, num_moves=len(solution))


DEFAULT_SCRAMBLE_LEN = 20


@app.get("/scramble", response_model=ScrambleResponse)
async def scramble(length: int = DEFAULT_SCRAMBLE_LEN) -> ScrambleResponse:
    if not 1 <= length <= MAX_SCRAMBLE_LEN:
        raise HTTPException(status_code=422, detail=f"length must be in [1, {MAX_SCRAMBLE_LEN}]")
    seed = secrets.randbits(63)
    tokens = await run_in_threadpool(nc.random_scramble, length, seed)
    return ScrambleResponse(scramble=tokens, seed=seed)
