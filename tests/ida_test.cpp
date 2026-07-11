#include <gtest/gtest.h>

#include <random>

#include "cube/cube.h"
#include "cube/scramble.h"
#include "solver/coords.h"
#include "solver/ida.h"

using cube::CubeState;
using cube::Move;

namespace {

CubeState apply_all(CubeState c, const std::vector<Move>& seq) {
    for (Move m : seq) cube::apply_move(c, m);
    return c;
}

}  // namespace

// ---------- trivial: solved cube ----------

TEST(IDASolve, SolvedCubeReturnsEmpty) {
    auto s = cube::solver::solve(cube::solved_cube());
    EXPECT_TRUE(s.empty());
}

TEST(IDAPhase1, SolvedCubeReturnsEmpty) {
    auto s = cube::solver::ida_phase1(cube::solved_cube());
    EXPECT_TRUE(s.empty());
}

TEST(IDAPhase2, SolvedCubeReturnsEmpty) {
    auto s = cube::solver::ida_phase2(cube::solved_cube());
    EXPECT_TRUE(s.empty());
}

// ---------- optimality on tiny scrambles ----------
//
// cube states that are 1 move away from solved, ida_phase1 must find the shortest path of length 1
TEST(IDAPhase1, SingleMoveScrambleSolvesInOne) {
    for (uint8_t m = 0; m < cube::NUM_MOVES; ++m) {
        CubeState c = cube::solved_cube();
        cube::apply_move(c, static_cast<Move>(m));
        auto sol = cube::solver::ida_phase1(c);
        EXPECT_LE(sol.size(), 1u) << "move " << int(m);
    }
}

// two-phase solver isn't globally optimal since a single-move scramble may
// need up to a handful of moves because phase 1 finds the shortest path
// back into G1, not necessarily the one that lands on fully solved
// so condition: solution actually solves it, and stays short
TEST(IDASolve, SingleMoveScrambleSolvesShortly) {
    for (uint8_t m = 0; m < cube::NUM_MOVES; ++m) {
        CubeState c = cube::solved_cube();
        cube::apply_move(c, static_cast<Move>(m));
        auto sol = cube::solver::solve(c);
        EXPECT_LE(sol.size(), 5u) << "move " << int(m);
        CubeState result = apply_all(c, sol);
        EXPECT_TRUE(cube::is_solved(result)) << "move " << int(m);
    }
}

// ---------- correctness: solve actually solves ----------

TEST(IDASolve, RandomScrambleSolvesToSolvedState) {
    std::mt19937_64 rng(20260711);
    for (int trial = 0; trial < 25; ++trial) {
        auto scramble = cube::random_scramble(20, rng());
        CubeState c = apply_all(cube::solved_cube(), scramble);

        auto solution = cube::solver::solve(c);
        CubeState result = apply_all(c, solution);
        EXPECT_TRUE(cube::is_solved(result))
            << "trial " << trial << " solution length " << solution.size();
    }
}

TEST(IDASolve, SolutionLengthIsReasonable) {
    // real kociemba averages ~22 moves; any solution over 30 indicates a bug
    std::mt19937_64 rng(20260711);
    for (int trial = 0; trial < 25; ++trial) {
        auto scramble = cube::random_scramble(25, rng());
        CubeState c = apply_all(cube::solved_cube(), scramble);
        auto solution = cube::solver::solve(c);
        EXPECT_LE(solution.size(), 30u)
            << "trial " << trial << " solution length " << solution.size();
    }
}

// ---------- phase transition: phase 1 output actually reaches G1 ----------

TEST(IDAPhase1, OutputStateIsInG1) {
    std::mt19937_64 rng(20260711);
    for (int trial = 0; trial < 10; ++trial) {
        auto scramble = cube::random_scramble(20, rng());
        CubeState c = apply_all(cube::solved_cube(), scramble);
        auto p1 = cube::solver::ida_phase1(c);
        CubeState after_p1 = apply_all(c, p1);

        auto coords = cube::solver::phase1_coords_of(after_p1);
        EXPECT_EQ(coords.corner_ori, 0u) << "trial " << trial;
        EXPECT_EQ(coords.edge_ori,   0u) << "trial " << trial;
        EXPECT_EQ(coords.slice, cube::solver::SLICE_GOAL) << "trial " << trial;
    }
}
