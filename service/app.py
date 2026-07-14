"""FastAPI service exposing the C++ Kociemba solver.

single POST /solve endpoint that takes a scramble (list of move tokens) and
returns a solution
"""

from __future__ import annotations

from typing import Annotated

from fastapi import FastAPI, HTTPException
from fastapi.concurrency import run_in_threadpool
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


app = FastAPI(title="nxn-cube-service", version="0.1.0")


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
