#include <gtest/gtest.h>

#include <random>

#include "cube/cube.h"
#include "solver/coords.h"
#include "solver/move_tables.h"

using cube::CubeState;
using cube::Move;
using cube::solver::Coord;
using cube::solver::G1_MOVES;
using cube::solver::NUM_G1_MOVES;
using cube::solver::g1_index;


TEST(G1Index, RoundTripsAllG1Moves) {
    for (int i = 0; i < NUM_G1_MOVES; ++i) {
        EXPECT_EQ(g1_index(G1_MOVES[i]), i);
    }
}

TEST(G1Index, ReturnsMinusOneForNonG1Move) {
    EXPECT_EQ(g1_index(Move::R),       -1);
    EXPECT_EQ(g1_index(Move::R_prime), -1);
    EXPECT_EQ(g1_index(Move::F_prime), -1);
    EXPECT_EQ(g1_index(Move::B),       -1);
}

// Phase 1 table tests

TEST(CornerOriTable, IdentityRowFromSolvedCube) {
    const auto& t = cube::solver::corner_ori_move_table();
    for (Move m : {Move::U, Move::U_prime, Move::U2, Move::D, Move::D_prime, Move::D2}) {
        EXPECT_EQ(t[0][static_cast<uint8_t>(m)], 0u)
            << "U/D move " << static_cast<int>(m) << " changed corner ori from solved";
    }
    // R quarter-turn from solved -> the known coord 1494
    EXPECT_EQ(t[0][static_cast<uint8_t>(Move::R)], 1494u);
}

TEST(EdgeOriTable, OnlyFBMovesFlipEdgesFromSolved) {
    const auto& t = cube::solver::edge_ori_move_table();
    for (uint8_t m = 0; m < cube::NUM_MOVES; ++m) {
        Move mv = static_cast<Move>(m);
        bool is_fb_quarter = (mv == Move::F || mv == Move::F_prime ||
                              mv == Move::B || mv == Move::B_prime);
        if (is_fb_quarter) continue;
        EXPECT_EQ(t[0][m], 0u)
            << "move " << static_cast<int>(m) << " flipped edges from solved";
    }
    // F quarter from solved -> the known coord 550
    EXPECT_EQ(t[0][static_cast<uint8_t>(Move::F)], 550u);
}

TEST(SliceTable, GoalCoordFromF) {
    const auto& t = cube::solver::slice_move_table();
    EXPECT_EQ(t[cube::solver::SLICE_GOAL][static_cast<uint8_t>(Move::F)], 461u);
}

// Phase 2 table tests

TEST(CornerPermTable, USolvedRow) {
    const auto& t = cube::solver::corner_perm_move_table();
    EXPECT_EQ(t[0][g1_index(Move::U)], 15120u);
    // confirming U2 twice equals identity
    Coord after_u2 = t[0][g1_index(Move::U2)];
    EXPECT_EQ(t[after_u2][g1_index(Move::U2)], 0u);
}

TEST(EdgePermUDTable, USolvedRow) {
    const auto& t = cube::solver::edge_perm_ud_move_table();
    EXPECT_EQ(t[0][g1_index(Move::U)], 15120u);
}

TEST(SlicePermTable, U2LeavesSliceUnchanged) {
    const auto& t = cube::solver::slice_perm_move_table();
    for (Move m : {Move::U, Move::U_prime, Move::U2, Move::D, Move::D_prime, Move::D2}) {
        EXPECT_EQ(t[0][g1_index(m)], 0u)
            << "U/D move " << static_cast<int>(m) << " changed slice perm from identity";
    }
}

TEST(SlicePermTable, R2IsAnInvolution) {
    const auto& t = cube::solver::slice_perm_move_table();
    Coord after = t[0][g1_index(Move::R2)];
    EXPECT_EQ(t[after][g1_index(Move::R2)], 0u);
}

// ---------- Cross-check: tables agree with the reference engine ----------
//
// The strongest test: for many random states, applying a move via the table
// must give the same next coord as applying the move via the reference
// engine and then re-encoding. This catches every possible table-build bug.

namespace {

CubeState random_state(std::mt19937& rng) {
    CubeState c = cube::solved_cube();
    std::uniform_int_distribution<int> move_dist(0, cube::NUM_MOVES - 1);
    for (int i = 0; i < 30; ++i) {
        cube::apply_move(c, static_cast<Move>(move_dist(rng)));
    }
    return c;
}

CubeState random_g1_state(std::mt19937& rng) {
    CubeState c = cube::solved_cube();
    std::uniform_int_distribution<int> idx(0, NUM_G1_MOVES - 1);
    for (int i = 0; i < 30; ++i) {
        cube::apply_move(c, G1_MOVES[idx(rng)]);
    }
    return c;
}

}  // namespace

TEST(MoveTables, Phase1TablesMatchReferenceEngine) {
    const auto& corner_ori_t = cube::solver::corner_ori_move_table();
    const auto& edge_ori_t   = cube::solver::edge_ori_move_table();
    const auto& slice_t      = cube::solver::slice_move_table();

    std::mt19937 rng(20260706);
    for (int trial = 0; trial < 500; ++trial) {
        CubeState c = random_state(rng);
        Coord co = cube::solver::encode_corner_ori(c);
        Coord eo = cube::solver::encode_edge_ori(c);
        Coord sl = cube::solver::encode_slice(c);

        for (uint8_t m = 0; m < cube::NUM_MOVES; ++m) {
            CubeState next = c;
            cube::apply_move(next, static_cast<Move>(m));
            ASSERT_EQ(corner_ori_t[co][m], cube::solver::encode_corner_ori(next))
                << "corner_ori disagreement, trial " << trial << " move " << int(m);
            ASSERT_EQ(edge_ori_t[eo][m], cube::solver::encode_edge_ori(next))
                << "edge_ori disagreement, trial " << trial << " move " << int(m);
            ASSERT_EQ(slice_t[sl][m], cube::solver::encode_slice(next))
                << "slice disagreement, trial " << trial << " move " << int(m);
        }
    }
}

TEST(MoveTables, Phase2TablesMatchReferenceEngine) {
    const auto& corner_perm_t  = cube::solver::corner_perm_move_table();
    const auto& edge_perm_ud_t = cube::solver::edge_perm_ud_move_table();
    const auto& slice_perm_t   = cube::solver::slice_perm_move_table();

    std::mt19937 rng(20260706);
    for (int trial = 0; trial < 500; ++trial) {
        CubeState c = random_g1_state(rng);
        Coord cp = cube::solver::encode_corner_perm(c);
        Coord ep = cube::solver::encode_edge_perm_ud(c);
        Coord sp = cube::solver::encode_slice_perm(c);

        for (int i = 0; i < NUM_G1_MOVES; ++i) {
            CubeState next = c;
            cube::apply_move(next, G1_MOVES[i]);
            ASSERT_EQ(corner_perm_t[cp][i], cube::solver::encode_corner_perm(next))
                << "corner_perm disagreement, trial " << trial << " g1 " << i;
            ASSERT_EQ(edge_perm_ud_t[ep][i], cube::solver::encode_edge_perm_ud(next))
                << "edge_perm_ud disagreement, trial " << trial << " g1 " << i;
            ASSERT_EQ(slice_perm_t[sp][i], cube::solver::encode_slice_perm(next))
                << "slice_perm disagreement, trial " << trial << " g1 " << i;
        }
    }
}
