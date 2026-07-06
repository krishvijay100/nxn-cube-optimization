#include <gtest/gtest.h>
#include "cube/cube.h"
#include "cube/notation.h"
#include "cube_test_helpers.h"

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

TEST(IsSolved, DetectsUnsolvedFromCornerOrientationTwist) {
    cube::CubeState c = cube::solved_cube();
    c.corner_orientations[3] = 1;
    EXPECT_FALSE(cube::is_solved(c));
}

TEST(IsSolved, DetectsUnsolvedFromEdgePositionSwap) {
    cube::CubeState c = cube::solved_cube();
    std::swap(c.edge_positions[4], c.edge_positions[7]);
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

bool states_match(const cube::CubeState& a, const cube::CubeState& b) {
    return a.corner_positions    == b.corner_positions
        && a.corner_orientations == b.corner_orientations
        && a.edge_positions      == b.edge_positions
        && a.edge_orientations   == b.edge_orientations;
}

}  // namespace

// derived prime tables (from compose()) must match 3 direct quarter turns
TEST(Moves, PrimeEqualsThreeQuarterTurns) {
    for (size_t i = 0; i < 6; ++i) {
        cube::CubeState a = cube::solved_cube();
        cube::apply_move(a, PRIME_OF[i]);

        cube::CubeState b = cube::solved_cube();
        for (int k = 0; k < 3; ++k) cube::apply_move(b, QUARTER_MOVES[i]);

        EXPECT_TRUE(states_match(a, b))
            << "prime != quarter^3 for face index " << i;
    }
}

// locks the string<->enum contract end-to-end
TEST(Notation, ParsedMoveMatchesEnumApplication) {
    for (cube::Move m : ALL_MOVES) {
        auto parsed = cube::parse_move(cube::to_string(m));
        ASSERT_TRUE(parsed.has_value());

        cube::CubeState via_enum = cube::solved_cube();
        cube::apply_move(via_enum, m);

        cube::CubeState via_string = cube::solved_cube();
        cube::apply_move(via_string, *parsed);

        EXPECT_TRUE(states_match(via_enum, via_string))
            << "parsed move differs from enum for " << cube::to_string(m);
    }
}


namespace {

// touches every face in both directions plus half turns; shared across all
// invariant tests so any failure points at one reproducible state
constexpr cube::Move SCRAMBLE[] = {
    cube::Move::R,  cube::Move::U,  cube::Move::F2, cube::Move::D_prime,
    cube::Move::L,  cube::Move::B,  cube::Move::U2, cube::Move::R_prime,
    cube::Move::F,  cube::Move::D,  cube::Move::L2, cube::Move::B_prime,
    cube::Move::U_prime, cube::Move::R2, cube::Move::F_prime, cube::Move::L_prime,
    cube::Move::B2, cube::Move::D2,
};

}  // namespace

TEST(Invariants, CornerOrientationSumIsZeroMod3AfterAnySingleMove) {
    for (cube::Move m : ALL_MOVES) {
        cube::CubeState c = cube::solved_cube();
        cube::apply_move(c, m);
        EXPECT_EQ(cube_test::corner_orient_sum(c) % 3, 0)
            << "broken by move " << static_cast<int>(m);
    }
}

TEST(Invariants, EdgeOrientationSumIsZeroMod2AfterAnySingleMove) {
    for (cube::Move m : ALL_MOVES) {
        cube::CubeState c = cube::solved_cube();
        cube::apply_move(c, m);
        EXPECT_EQ(cube_test::edge_orient_sum(c) % 2, 0)
            << "broken by move " << static_cast<int>(m);
    }
}

TEST(Invariants, CornerOrientationSumIsZeroMod3AfterScramble) {
    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : SCRAMBLE) cube::apply_move(c, m);
    EXPECT_EQ(cube_test::corner_orient_sum(c) % 3, 0);
}

TEST(Invariants, EdgeOrientationSumIsZeroMod2AfterScramble) {
    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : SCRAMBLE) cube::apply_move(c, m);
    EXPECT_EQ(cube_test::edge_orient_sum(c) % 2, 0);
}

TEST(Invariants, PermutationParityMatchesAfterScramble) {
    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : SCRAMBLE) cube::apply_move(c, m);
    EXPECT_EQ(cube_test::permutation_parity(c.corner_positions),
              cube_test::permutation_parity(c.edge_positions));
}

TEST(Notation, ToStringMatchesExpectedNames) {
    EXPECT_EQ(cube::to_string(cube::Move::U),       "U");
    EXPECT_EQ(cube::to_string(cube::Move::U_prime), "U'");
    EXPECT_EQ(cube::to_string(cube::Move::U2),      "U2");
    EXPECT_EQ(cube::to_string(cube::Move::B2),      "B2");
}

TEST(Notation, ParseMoveAcceptsValidStrings) {
    EXPECT_EQ(cube::parse_move("U"),  cube::Move::U);
    EXPECT_EQ(cube::parse_move("U'"), cube::Move::U_prime);
    EXPECT_EQ(cube::parse_move("U2"), cube::Move::U2);
    EXPECT_EQ(cube::parse_move("B'"), cube::Move::B_prime);
}

TEST(Notation, ParseMoveRejectsInvalid) {
    EXPECT_FALSE(cube::parse_move("").has_value());
    EXPECT_FALSE(cube::parse_move("X").has_value());
    EXPECT_FALSE(cube::parse_move("U3").has_value());
    EXPECT_FALSE(cube::parse_move("UU").has_value());
    EXPECT_FALSE(cube::parse_move("u").has_value());
    EXPECT_FALSE(cube::parse_move("R'2").has_value());
}

TEST(Notation, RoundTripForAllMoves) {
    for (cube::Move m : ALL_MOVES) {
        auto parsed = cube::parse_move(cube::to_string(m));
        ASSERT_TRUE(parsed.has_value()) << "failed for " << static_cast<int>(m);
        EXPECT_EQ(*parsed, m);
    }
}

TEST(Notation, ParseSequenceHandlesSpacing) {
    auto seq = cube::parse_sequence("R U R' U'");
    ASSERT_TRUE(seq.has_value());
    ASSERT_EQ(seq->size(), 4u);
    EXPECT_EQ((*seq)[0], cube::Move::R);
    EXPECT_EQ((*seq)[1], cube::Move::U);
    EXPECT_EQ((*seq)[2], cube::Move::R_prime);
    EXPECT_EQ((*seq)[3], cube::Move::U_prime);
}

TEST(Notation, ParseSequenceHandlesExtraWhitespace) {
    auto seq = cube::parse_sequence("  R   U  R'\tU'  ");
    ASSERT_TRUE(seq.has_value());
    EXPECT_EQ(seq->size(), 4u);
}

TEST(Notation, ParseSequenceEmptyIsEmptyVector) {
    auto seq = cube::parse_sequence("");
    ASSERT_TRUE(seq.has_value());
    EXPECT_EQ(seq->size(), 0u);

    auto seq2 = cube::parse_sequence("   ");
    ASSERT_TRUE(seq2.has_value());
    EXPECT_EQ(seq2->size(), 0u);
}

TEST(Notation, ParseSequenceRejectsBadToken) {
    EXPECT_FALSE(cube::parse_sequence("R U X U'").has_value());
}

TEST(Notation, ParsedSexyMoveSolvesAfterSixReps) {
    auto seq = cube::parse_sequence("R U R' U'");
    ASSERT_TRUE(seq.has_value());

    cube::CubeState c = cube::solved_cube();
    for (int rep = 0; rep < 6; ++rep) {
        for (cube::Move m : *seq) cube::apply_move(c, m);
    }
    EXPECT_TRUE(cube::is_solved(c));
}

// Sune (R U R' U R U2 R'): 4-corner cycle + 3-edge cycle from solved, order 6
// cross-verified against pycuber and a from-scratch simulator
//   corner_positions    = {2,3,0,1,4,5,6,7}
//   corner_orientations = {1,1,1,0,0,0,0,0}
//   edge_positions      = {2,1,3,0,4,5,6,7,8,9,10,11}
TEST(KnownPositions, SuneProducesExpectedState) {
    auto seq = cube::parse_sequence("R U R' U R U2 R'");
    ASSERT_TRUE(seq.has_value());

    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : *seq) cube::apply_move(c, m);

    const std::array<uint8_t, 8> expected_cp = {2,3,0,1,4,5,6,7};
    const std::array<uint8_t, 8> expected_co = {1,1,1,0,0,0,0,0};
    const std::array<uint8_t, 12> expected_ep = {2,1,3,0,4,5,6,7,8,9,10,11};

    for (uint8_t i = 0; i < cube::NUM_CORNERS; ++i) {
        EXPECT_EQ(c.corner_positions[i], expected_cp[i])
            << "sune corner_positions mismatch at slot " << static_cast<int>(i);
        EXPECT_EQ(c.corner_orientations[i], expected_co[i])
            << "sune corner_orientations mismatch at slot " << static_cast<int>(i);
    }
    for (uint8_t i = 0; i < cube::NUM_EDGES; ++i) {
        EXPECT_EQ(c.edge_positions[i], expected_ep[i])
            << "sune edge_positions mismatch at slot " << static_cast<int>(i);
        EXPECT_EQ(c.edge_orientations[i], 0)
            << "sune edge_orientations mismatch at slot " << static_cast<int>(i);
    }

    // 6 reps returns to solved
    cube::CubeState c2 = cube::solved_cube();
    int order = 0;
    for (int rep = 1; rep <= 12; ++rep) {
        for (cube::Move m : *seq) cube::apply_move(c2, m);
        if (cube::is_solved(c2)) { order = rep; break; }
    }
    EXPECT_EQ(order, 6) << "sune should have order 6";
}

// T-perm: pure double transposition, order 2, verified against pycuber
// corner swap URF(0) <-> UBR(3), edge swap UR(0) <-> UL(2), no ori changes
TEST(KnownPositions, TPermProducesExpectedState) {
    auto seq = cube::parse_sequence("R U R' U' R' F R2 U' R' U' R U R' F'");
    ASSERT_TRUE(seq.has_value());

    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : *seq) cube::apply_move(c, m);

    EXPECT_EQ(c.corner_positions[0], 3) << "T-perm: URF slot should hold UBR piece";
    EXPECT_EQ(c.corner_positions[3], 0) << "T-perm: UBR slot should hold URF piece";
    for (uint8_t i : {1, 2, 4, 5, 6, 7}) {
        EXPECT_EQ(c.corner_positions[i], i)
            << "T-perm should not move corner slot " << static_cast<int>(i);
    }
    for (uint8_t i = 0; i < cube::NUM_CORNERS; ++i) {
        EXPECT_EQ(c.corner_orientations[i], 0)
            << "T-perm should not twist corner slot " << static_cast<int>(i);
    }

    EXPECT_EQ(c.edge_positions[0], 2) << "T-perm: UR slot should hold UL piece";
    EXPECT_EQ(c.edge_positions[2], 0) << "T-perm: UL slot should hold UR piece";
    for (uint8_t i : {1, 3, 4, 5, 6, 7, 8, 9, 10, 11}) {
        EXPECT_EQ(c.edge_positions[i], i)
            << "T-perm should not move edge slot " << static_cast<int>(i);
    }
    for (uint8_t i = 0; i < cube::NUM_EDGES; ++i) {
        EXPECT_EQ(c.edge_orientations[i], 0)
            << "T-perm should not flip edge slot " << static_cast<int>(i);
    }

    // 2 reps returns to solved
    cube::CubeState c2 = cube::solved_cube();
    for (int rep = 0; rep < 2; ++rep) {
        for (cube::Move m : *seq) cube::apply_move(c2, m);
    }
    EXPECT_TRUE(cube::is_solved(c2)) << "T-perm^2 should return to solved";
}

// Ua-perm: pure 3-edge cycle on U-layer, order 3, verified against pycuber
//   edge_positions = {1,2,0,3, ...} (3-cycle on edges 0,1,2), corners untouched
TEST(KnownPositions, UaPermProducesExpectedState) {
    auto seq = cube::parse_sequence("R U' R U R U R U' R' U' R2");
    ASSERT_TRUE(seq.has_value());

    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : *seq) cube::apply_move(c, m);

    const std::array<uint8_t, 12> expected_ep = {1,2,0,3,4,5,6,7,8,9,10,11};
    for (uint8_t i = 0; i < cube::NUM_EDGES; ++i) {
        EXPECT_EQ(c.edge_positions[i], expected_ep[i])
            << "Ua-perm edge_positions mismatch at slot " << static_cast<int>(i);
        EXPECT_EQ(c.edge_orientations[i], 0)
            << "Ua-perm edge_orientations mismatch at slot " << static_cast<int>(i);
    }
    for (uint8_t i = 0; i < cube::NUM_CORNERS; ++i) {
        EXPECT_EQ(c.corner_positions[i], i)
            << "Ua-perm moved corner slot " << static_cast<int>(i);
        EXPECT_EQ(c.corner_orientations[i], 0)
            << "Ua-perm twisted corner slot " << static_cast<int>(i);
    }
    // 3 reps returns to solved
    cube::CubeState c2 = cube::solved_cube();
    int order = 0;
    for (int rep = 1; rep <= 6; ++rep) {
        for (cube::Move m : *seq) cube::apply_move(c2, m);
        if (cube::is_solved(c2)) { order = rep; break; }
    }
    EXPECT_EQ(order, 3) << "Ua-perm should have order 3";
}

// Y-perm: corner swap URF(0)<->ULB(2), edge swap UL(2)<->UB(3), order 2
// verified against pycuber. no orientation changes
TEST(KnownPositions, YPermProducesExpectedState) {
    auto seq = cube::parse_sequence("F R U' R' U' R U R' F' R U R' U' R' F R F'");
    ASSERT_TRUE(seq.has_value());

    cube::CubeState c = cube::solved_cube();
    for (cube::Move m : *seq) cube::apply_move(c, m);

    const std::array<uint8_t, 8> expected_cp = {2,1,0,3,4,5,6,7};
    const std::array<uint8_t, 12> expected_ep = {0,1,3,2,4,5,6,7,8,9,10,11};
    for (uint8_t i = 0; i < cube::NUM_CORNERS; ++i) {
        EXPECT_EQ(c.corner_positions[i], expected_cp[i])
            << "Y-perm corner_positions mismatch at slot " << static_cast<int>(i);
        EXPECT_EQ(c.corner_orientations[i], 0)
            << "Y-perm corner_orientations mismatch at slot " << static_cast<int>(i);
    }
    for (uint8_t i = 0; i < cube::NUM_EDGES; ++i) {
        EXPECT_EQ(c.edge_positions[i], expected_ep[i])
            << "Y-perm edge_positions mismatch at slot " << static_cast<int>(i);
        EXPECT_EQ(c.edge_orientations[i], 0)
            << "Y-perm edge_orientations mismatch at slot " << static_cast<int>(i);
    }
    // 2 reps returns to solved
    cube::CubeState c2 = cube::solved_cube();
    for (int rep = 0; rep < 2; ++rep) {
        for (cube::Move m : *seq) cube::apply_move(c2, m);
    }
    EXPECT_TRUE(cube::is_solved(c2)) << "Y-perm^2 should return to solved";
}
