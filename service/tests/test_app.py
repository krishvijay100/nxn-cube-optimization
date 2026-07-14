"""endpoint tests for the FastAPI service. uses TestClient so no live uvicorn.
covers the golden path (round-trip: scramble -> solve -> verify), 422 on bad
input, and a health probe
"""

from fastapi.testclient import TestClient
import nxn_cube_py as nc
from app import app

client = TestClient(app)


def test_health_returns_ok():
    r = client.get("/health")
    assert r.status_code == 200
    assert r.json() == {"status": "ok"}


def test_solve_roundtrip_deterministic_scrambles():
    for seed in range(1, 11):
        scramble = nc.random_scramble(20, seed)
        r = client.post("/solve", json={"scramble": scramble})
        assert r.status_code == 200, r.text
        body = r.json()
        assert body["num_moves"] == len(body["solution"])
        assert nc.verify(scramble, body["solution"]), (
            f"solution didn't solve scramble (seed={seed})"
        )


def test_solve_solved_cube_returns_empty_solution():
    r = client.post("/solve", json={"scramble": []})
    assert r.status_code == 200
    assert r.json() == {"solution": [], "num_moves": 0}


def test_solve_rejects_invalid_move_token():
    r = client.post("/solve", json={"scramble": ["R", "NOT_A_MOVE", "U"]})
    assert r.status_code == 422
    assert "invalid move tokens" in r.text


def test_solve_rejects_oversized_scramble():
    r = client.post("/solve", json={"scramble": ["R"] * 41})
    assert r.status_code == 422


def test_solve_rejects_wrong_shape():
    r = client.post("/solve", json={"scramble": "R U R'"})   # string, not list
    assert r.status_code == 422
