import sys
import unittest

import nxn_cube_py as nc


class RoundTrip(unittest.TestCase):
    def test_solves_deterministic_scrambles(self):
        # multiple seeds so we're not relying on a lucky one
        for seed in range(1, 11):
            scramble = nc.random_scramble(25, seed=seed)
            solution = nc.solve(scramble)
            self.assertTrue(
                nc.verify(scramble, solution),
                f"seed={seed}: solution {solution} did not solve scramble {scramble}",
            )
            self.assertLess(len(solution), 30, f"seed={seed}: unusually long solution")

    def test_solves_every_supported_cube_size(self):
        for n in range(3, 8):
            scramble = nc.random_scramble(20, seed=100 + n, n=n)
            solution = nc.solve(scramble, n=n)
            self.assertTrue(
                nc.verify(scramble, solution, n=n),
                f"{n}x{n} solution did not solve the generated scramble",
            )

    def test_same_seed_same_scramble(self):
        a = nc.random_scramble(20, seed=7)
        b = nc.random_scramble(20, seed=7)
        self.assertEqual(a, b)

    def test_solved_input_returns_empty(self):
        # scramble of length 0 leaves the cube solved; solver should return nothing
        self.assertEqual(nc.solve([]), [])


class ErrorPaths(unittest.TestCase):
    def test_unknown_move_raises(self):
        with self.assertRaises(ValueError):
            nc.solve(["U", "not-a-move", "R"])

    def test_negative_length_raises(self):
        with self.assertRaises(ValueError):
            nc.random_scramble(-1, seed=0)

    def test_rejects_unsupported_cube_size(self):
        with self.assertRaises(ValueError):
            nc.random_scramble(20, seed=0, n=8)

    def test_rejects_move_deeper_than_cube(self):
        with self.assertRaises(ValueError):
            nc.solve(["7Rw"], n=5)

        with self.assertRaises(ValueError):
            nc.solve(["5R"], n=5)

    def test_rejects_fixed_center_moves_on_odd_cubes(self):
        for n, move in ((3, "2L"), (5, "3L"), (7, "4F'")):
            with self.assertRaises(ValueError):
                nc.solve(["U2", move, "R"], n=n)


if __name__ == "__main__":
    unittest.main(verbosity=2)
