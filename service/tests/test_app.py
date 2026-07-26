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
        assert body["n"] == 3
        assert body["num_moves"] == len(body["solution"])
        assert nc.verify(scramble, body["solution"]), (
            f"solution didn't solve scramble (seed={seed})"
        )


def test_solve_solved_cube_returns_empty_solution():
    r = client.post("/solve", json={"scramble": []})
    assert r.status_code == 200
    assert r.json() == {"n": 3, "solution": [], "num_moves": 0}


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


def test_scramble_returns_valid_moves():
    r = client.get("/scramble?length=20")
    assert r.status_code == 200
    body = r.json()
    assert body["n"] == 3
    assert len(body["scramble"]) == 20
    # feed the returned scramble straight to /solve to prove it round-trips
    r2 = client.post("/solve", json={"scramble": body["scramble"]})
    assert r2.status_code == 200
    assert nc.verify(body["scramble"], r2.json()["solution"])


def test_scramble_rejects_bad_length():
    assert client.get("/scramble?length=0").status_code == 422
    assert client.get("/scramble?length=41").status_code == 422


def test_roundtrip_every_supported_cube_size():
    for n in range(3, 8):
        scramble_response = client.get(f"/scramble?n={n}&length=20")
        assert scramble_response.status_code == 200
        scramble = scramble_response.json()["scramble"]

        solve_response = client.post(
            "/solve",
            json={"n": n, "scramble": scramble},
        )
        assert solve_response.status_code == 200, solve_response.text
        solution = solve_response.json()["solution"]
        assert nc.verify(scramble, solution, n)


def test_rejects_unsupported_cube_size():
    assert client.get("/scramble?n=8").status_code == 422
    assert client.post("/solve", json={"n": 2, "scramble": []}).status_code == 422


def test_rejects_move_deeper_than_selected_cube():
    response = client.post("/solve", json={"n": 5, "scramble": ["6Rw"]})
    assert response.status_code == 422
    response = client.post("/solve", json={"n": 5, "scramble": ["5R"]})
    assert response.status_code == 422


def test_rejects_fixed_center_moves_on_odd_cubes():
    for n, move in ((3, "2L"), (5, "3L"), (7, "4F'")):
        response = client.post(
            "/solve",
            json={"n": n, "scramble": ["U2", move, "R"]},
        )
        assert response.status_code == 422
