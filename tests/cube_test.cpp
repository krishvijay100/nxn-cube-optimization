#include <gtest/gtest.h>
#include "cube/cube.h"

TEST(SolvedCube, IsRecognizedAsSolved) {
    cube::CubeState c = cube::solved_cube();
    EXPECT_TRUE(cube::is_solved(c));
}

TEST(SolvedCube, CornerPositionsAreIdentity) {
    cube::CubeState c = cube::solved_cube();
    for (uint8_t i = 0; i < cube::NUM_CORNERS; ++i) {
        EXPECT_EQ(c.corner_positions[i], i);
    }
}

TEST(SolvedCube, EdgePositionsAreIdentity) {
    cube::CubeState c = cube::solved_cube();
    for (uint8_t i = 0; i < cube::NUM_EDGES; ++i) {
        EXPECT_EQ(c.edge_positions[i], i);
    }
}

TEST(SolvedCube, AllOrientationsZero) {
    cube::CubeState c = cube::solved_cube();
    for (uint8_t i = 0; i < cube::NUM_CORNERS; ++i) {
        EXPECT_EQ(c.corner_orientations[i], 0);
    }
    for (uint8_t i = 0; i < cube::NUM_EDGES; ++i) {
        EXPECT_EQ(c.edge_orientations[i], 0);
    }
}

TEST(IsSolved, DetectsUnsolvedFromCornerPositionSwap) {
    cube::CubeState c = cube::solved_cube();
    std::swap(c.corner_positions[0], c.corner_positions[1]);
    EXPECT_FALSE(cube::is_solved(c));
}

TEST(IsSolved, DetectsUnsolvedFromOrientationFlip) {
    cube::CubeState c = cube::solved_cube();
    c.edge_orientations[5] = 1;
    EXPECT_FALSE(cube::is_solved(c));
}

TEST(CubeState, FitsInExpectedByteCount) {
    EXPECT_EQ(sizeof(cube::CubeState), 40u);
}

namespace {

bool states_equal(const cube::CubeState& a, const cube::CubeState& b) {
    return a.corner_positions    == b.corner_positions
        && a.corner_orientations == b.corner_orientations
        && a.edge_positions      == b.edge_positions
        && a.edge_orientations   == b.edge_orientations;
}

constexpr cube::Move ALL_MOVES[] = {
    cube::Move::U, cube::Move::U_prime, cube::Move::U2,
    cube::Move::D, cube::Move::D_prime, cube::Move::D2,
    cube::Move::R, cube::Move::R_prime, cube::Move::R2,
    cube::Move::L, cube::Move::L_prime, cube::Move::L2,
    cube::Move::F, cube::Move::F_prime, cube::Move::F2,
    cube::Move::B, cube::Move::B_prime, cube::Move::B2,
};

constexpr cube::Move QUARTER_MOVES[] = {
    cube::Move::U, cube::Move::D, cube::Move::R,
    cube::Move::L, cube::Move::F, cube::Move::B,
};

constexpr cube::Move PRIME_OF[] = {
    cube::Move::U_prime, cube::Move::D_prime, cube::Move::R_prime,
    cube::Move::L_prime, cube::Move::F_prime, cube::Move::B_prime,
};

constexpr cube::Move HALF_OF[] = {
    cube::Move::U2, cube::Move::D2, cube::Move::R2,
    cube::Move::L2, cube::Move::F2, cube::Move::B2,
};

}  // namespace

TEST(Moves, QuarterFollowedByPrimeReturnsToSolved) {
    for (size_t i = 0; i < 6; ++i) {
        cube::CubeState c = cube::solved_cube();
        cube::apply_move(c, QUARTER_MOVES[i]);
        cube::apply_move(c, PRIME_OF[i]);
        EXPECT_TRUE(cube::is_solved(c))
            << "quarter+prime failed for face index " << i;
    }
}

TEST(Moves, FourQuarterTurnsReturnsToSolved) {
    for (cube::Move q : QUARTER_MOVES) {
        cube::CubeState c = cube::solved_cube();
        for (int k = 0; k < 4; ++k) cube::apply_move(c, q);
        EXPECT_TRUE(cube::is_solved(c))
            << "four-of-a-quarter failed for move " << static_cast<int>(q);
    }
}

TEST(Moves, HalfTurnEqualsTwoQuarterTurns) {
    for (size_t i = 0; i < 6; ++i) {
        cube::CubeState a = cube::solved_cube();
        cube::apply_move(a, HALF_OF[i]);

        cube::CubeState b = cube::solved_cube();
        cube::apply_move(b, QUARTER_MOVES[i]);
        cube::apply_move(b, QUARTER_MOVES[i]);

        EXPECT_TRUE(states_equal(a, b))
            << "half != quarter+quarter for face index " << i;
    }
}

TEST(Moves, HalfTurnAppliedTwiceReturnsToSolved) {
    for (cube::Move h : HALF_OF) {
        cube::CubeState c = cube::solved_cube();
        cube::apply_move(c, h);
        cube::apply_move(c, h);
        EXPECT_TRUE(cube::is_solved(c))
            << "half+half failed for move " << static_cast<int>(h);
    }
}

TEST(Moves, SexyMoveSixTimesReturnsToSolved) {
    cube::CubeState c = cube::solved_cube();
    for (int rep = 0; rep < 6; ++rep) {
        cube::apply_move(c, cube::Move::R);
        cube::apply_move(c, cube::Move::U);
        cube::apply_move(c, cube::Move::R_prime);
        cube::apply_move(c, cube::Move::U_prime);
    }
    EXPECT_TRUE(cube::is_solved(c));
}

TEST(Moves, SingleMoveChangesState) {
    for (cube::Move m : ALL_MOVES) {
        cube::CubeState c = cube::solved_cube();
        cube::apply_move(c, m);
        EXPECT_FALSE(cube::is_solved(c))
            << "move " << static_cast<int>(m) << " left state solved";
    }
}

namespace {

// A fixed, deliberately varied scramble that touches every face in both
// directions and with half turns. Used by all invariant tests so that any
// failure points at the same reproducible state.
constexpr cube::Move SCRAMBLE[] = {
    cube::Move::R,  cube::Move::U,  cube::Move::F2, cube::Move::D_prime,
    cube::Move::L,  cube::Move::B,  cube::Move::U2, cube::Move::R_prime,
    cube::Move::F,  cube::Move::D,  cube::Move::L2, cube::Move::B_prime,
    cube::Move::U_prime, cube::Move::R2, cube::Move::F_prime, cube::Move::L_prime,
    cube::Move::B2, cube::Move::D2,
};

int corner_orient_sum(const cube::CubeState& c) {
    int s = 0;
    for (auto v : c.corner_orientations) s += v;
    return s;
}

int edge_orient_sum(const cube::CubeState& c) {
    int s = 0;
    for (auto v : c.edge_orientations) s += v;
    return s;
}

// Parity of a permutation: count inversions and take mod 2.
template <size_t N>
int permutation_parity(const std::array<uint8_t, N>& p) {
    int inversions = 0;
    for (size_t i = 0; i < N; ++i)
        for (size_t j = i + 1; j < N; ++j)
            if (p[i] > p[j]) ++inversions;
    return inversions % 2;
}

}  // namespace

TEST(Invariants, CornerOrientationSumIsZeroMod3AfterAnySingleMove) {
    for (cube::Move m : ALL_MOVES) {
        cube::CubeState c = cube::solved_cube();
        cube::apply_move(c, m);
        EXPECT_EQ(corner_orient_sum(c) % 3, 0)
            << "broken by move " << static_cast<int>(m);
    }
}

TEST(Invariants, EdgeOrientationSumIsZeroMod2AfterAnySingleMove) {
    for (cube::Move m : ALL_MOVES) {
        cube::CubeState c = cube::solved_cube();
        cube::apply_move(c, m);
        EXPECT_EQ(edge_orient_sum(c) % 2, 0)
            << "broken by move " << static_cast<int>(m);
    }
}

TEST(Invariants, CornerOrientationSumIsZeroMod3AfterScramble) {
    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : SCRAMBLE) cube::apply_move(c, m);
    EXPECT_EQ(corner_orient_sum(c) % 3, 0);
}

TEST(Invariants, EdgeOrientationSumIsZeroMod2AfterScramble) {
    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : SCRAMBLE) cube::apply_move(c, m);
    EXPECT_EQ(edge_orient_sum(c) % 2, 0);
}

TEST(Invariants, PermutationParityMatchesAfterScramble) {
    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : SCRAMBLE) cube::apply_move(c, m);
    EXPECT_EQ(permutation_parity(c.corner_positions),
              permutation_parity(c.edge_positions));
}
