#include <gtest/gtest.h>

#include <cstdlib>

#include "solver/coords.h"
#include "solver/move_tables.h"
#include "solver/pruning.h"

using cube::Move;
using cube::NUM_MOVES;
using cube::solver::Coord;
using cube::solver::CORNER_ORI_COORDS;
using cube::solver::EDGE_ORI_COORDS;
using cube::solver::SLICE_COORDS;
using cube::solver::SLICE_GOAL;
using cube::solver::CORNER_PERM_COORDS;
using cube::solver::EDGE_PERM_UD_COORDS;
using cube::solver::SLICE_PERM_COORDS;
using cube::solver::NUM_G1_MOVES;

// ---------- phase 1: corner_ori × slice ----------

TEST(CornerOriSlicePruning, GoalIsZero) {
    const auto& t = cube::solver::corner_ori_slice_pruning();
    EXPECT_EQ(t[0][SLICE_GOAL], 0u);
}

TEST(CornerOriSlicePruning, AllEntriesFilled) {
    const auto& t = cube::solver::corner_ori_slice_pruning();
    for (Coord co = 0; co < CORNER_ORI_COORDS; ++co)
        for (Coord sl = 0; sl < SLICE_COORDS; ++sl)
            ASSERT_NE(t[co][sl], 0xFF) << "unvisited at (" << co << "," << sl << ")";
}

TEST(CornerOriSlicePruning, OneMoveFromGoalIsAtMostOne) {
    const auto& t = cube::solver::corner_ori_slice_pruning();
    const auto& co_moves = cube::solver::corner_ori_move_table();
    const auto& sl_moves = cube::solver::slice_move_table();
    for (int m = 0; m < NUM_MOVES; ++m) {
        Coord nco = co_moves[0][m];
        Coord nsl = sl_moves[SLICE_GOAL][m];
        EXPECT_LE(t[nco][nsl], 1u) << "move " << m;
    }
}

TEST(CornerOriSlicePruning, DistanceChangesByAtMostOnePerMove) {
    // sampled — full sweep is 2187*495*18 ~= 19M table lookups
    const auto& t = cube::solver::corner_ori_slice_pruning();
    const auto& co_moves = cube::solver::corner_ori_move_table();
    const auto& sl_moves = cube::solver::slice_move_table();
    for (Coord co = 0; co < CORNER_ORI_COORDS; co += 7) {
        for (Coord sl = 0; sl < SLICE_COORDS; sl += 3) {
            for (int m = 0; m < NUM_MOVES; ++m) {
                Coord nco = co_moves[co][m];
                Coord nsl = sl_moves[sl][m];
                int diff = std::abs(int(t[co][sl]) - int(t[nco][nsl]));
                ASSERT_LE(diff, 1)
                    << "(" << co << "," << sl << ") -m" << m << "-> ("
                    << nco << "," << nsl << ")";
            }
        }
    }
}

// ---------- phase 1: edge_ori × slice ----------

TEST(EdgeOriSlicePruning, GoalIsZero) {
    const auto& t = cube::solver::edge_ori_slice_pruning();
    EXPECT_EQ(t[0][SLICE_GOAL], 0u);
}

TEST(EdgeOriSlicePruning, AllEntriesFilled) {
    const auto& t = cube::solver::edge_ori_slice_pruning();
    for (Coord eo = 0; eo < EDGE_ORI_COORDS; ++eo)
        for (Coord sl = 0; sl < SLICE_COORDS; ++sl)
            ASSERT_NE(t[eo][sl], 0xFF) << "unvisited at (" << eo << "," << sl << ")";
}

TEST(EdgeOriSlicePruning, OneMoveFromGoalIsAtMostOne) {
    const auto& t = cube::solver::edge_ori_slice_pruning();
    const auto& eo_moves = cube::solver::edge_ori_move_table();
    const auto& sl_moves = cube::solver::slice_move_table();
    for (int m = 0; m < NUM_MOVES; ++m) {
        Coord neo = eo_moves[0][m];
        Coord nsl = sl_moves[SLICE_GOAL][m];
        EXPECT_LE(t[neo][nsl], 1u) << "move " << m;
    }
}

TEST(EdgeOriSlicePruning, DistanceChangesByAtMostOnePerMove) {
    const auto& t = cube::solver::edge_ori_slice_pruning();
    const auto& eo_moves = cube::solver::edge_ori_move_table();
    const auto& sl_moves = cube::solver::slice_move_table();
    for (Coord eo = 0; eo < EDGE_ORI_COORDS; eo += 7) {
        for (Coord sl = 0; sl < SLICE_COORDS; sl += 3) {
            for (int m = 0; m < NUM_MOVES; ++m) {
                Coord neo = eo_moves[eo][m];
                Coord nsl = sl_moves[sl][m];
                int diff = std::abs(int(t[eo][sl]) - int(t[neo][nsl]));
                ASSERT_LE(diff, 1)
                    << "(" << eo << "," << sl << ") -m" << m << "-> ("
                    << neo << "," << nsl << ")";
            }
        }
    }
}

// ---------- phase 2: corner_perm × slice_perm ----------

TEST(CornerPermSlicePermPruning, GoalIsZero) {
    const auto& t = cube::solver::corner_perm_slice_perm_pruning();
    EXPECT_EQ(t[0][0], 0u);
}

TEST(CornerPermSlicePermPruning, AllEntriesFilled) {
    const auto& t = cube::solver::corner_perm_slice_perm_pruning();
    for (Coord cp = 0; cp < CORNER_PERM_COORDS; ++cp)
        for (Coord sp = 0; sp < SLICE_PERM_COORDS; ++sp)
            ASSERT_NE(t[cp][sp], 0xFF) << "unvisited at (" << cp << "," << sp << ")";
}

TEST(CornerPermSlicePermPruning, OneG1MoveFromGoalIsAtMostOne) {
    const auto& t = cube::solver::corner_perm_slice_perm_pruning();
    const auto& cp_moves = cube::solver::corner_perm_move_table();
    const auto& sp_moves = cube::solver::slice_perm_move_table();
    for (int m = 0; m < NUM_G1_MOVES; ++m) {
        Coord ncp = cp_moves[0][m];
        Coord nsp = sp_moves[0][m];
        EXPECT_LE(t[ncp][nsp], 1u) << "g1 move " << m;
    }
}

TEST(CornerPermSlicePermPruning, DistanceChangesByAtMostOnePerMove) {
    const auto& t = cube::solver::corner_perm_slice_perm_pruning();
    const auto& cp_moves = cube::solver::corner_perm_move_table();
    const auto& sp_moves = cube::solver::slice_perm_move_table();
    for (Coord cp = 0; cp < CORNER_PERM_COORDS; cp += 137) {
        for (Coord sp = 0; sp < SLICE_PERM_COORDS; ++sp) {
            for (int m = 0; m < NUM_G1_MOVES; ++m) {
                Coord ncp = cp_moves[cp][m];
                Coord nsp = sp_moves[sp][m];
                int diff = std::abs(int(t[cp][sp]) - int(t[ncp][nsp]));
                ASSERT_LE(diff, 1)
                    << "(" << cp << "," << sp << ") -g1_" << m << "-> ("
                    << ncp << "," << nsp << ")";
            }
        }
    }
}

// ---------- phase 2: edge_perm_ud × slice_perm ----------

TEST(EdgePermSlicePermPruning, GoalIsZero) {
    const auto& t = cube::solver::edge_perm_slice_perm_pruning();
    EXPECT_EQ(t[0][0], 0u);
}

TEST(EdgePermSlicePermPruning, AllEntriesFilled) {
    const auto& t = cube::solver::edge_perm_slice_perm_pruning();
    for (Coord ep = 0; ep < EDGE_PERM_UD_COORDS; ++ep)
        for (Coord sp = 0; sp < SLICE_PERM_COORDS; ++sp)
            ASSERT_NE(t[ep][sp], 0xFF) << "unvisited at (" << ep << "," << sp << ")";
}

TEST(EdgePermSlicePermPruning, OneG1MoveFromGoalIsAtMostOne) {
    const auto& t = cube::solver::edge_perm_slice_perm_pruning();
    const auto& ep_moves = cube::solver::edge_perm_ud_move_table();
    const auto& sp_moves = cube::solver::slice_perm_move_table();
    for (int m = 0; m < NUM_G1_MOVES; ++m) {
        Coord nep = ep_moves[0][m];
        Coord nsp = sp_moves[0][m];
        EXPECT_LE(t[nep][nsp], 1u) << "g1 move " << m;
    }
}

TEST(EdgePermSlicePermPruning, DistanceChangesByAtMostOnePerMove) {
    const auto& t = cube::solver::edge_perm_slice_perm_pruning();
    const auto& ep_moves = cube::solver::edge_perm_ud_move_table();
    const auto& sp_moves = cube::solver::slice_perm_move_table();
    for (Coord ep = 0; ep < EDGE_PERM_UD_COORDS; ep += 137) {
        for (Coord sp = 0; sp < SLICE_PERM_COORDS; ++sp) {
            for (int m = 0; m < NUM_G1_MOVES; ++m) {
                Coord nep = ep_moves[ep][m];
                Coord nsp = sp_moves[sp][m];
                int diff = std::abs(int(t[ep][sp]) - int(t[nep][nsp]));
                ASSERT_LE(diff, 1)
                    << "(" << ep << "," << sp << ") -g1_" << m << "-> ("
                    << nep << "," << nsp << ")";
            }
        }
    }
}
