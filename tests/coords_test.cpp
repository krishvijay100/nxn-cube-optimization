#include <gtest/gtest.h>

#include "cube/cube.h"
#include "solver/coords.h"

using cube::CubeState;
using cube::Move;
using cube::solver::Coord;
using cube::solver::encode_corner_ori;
using cube::solver::decode_corner_ori;
using cube::solver::encode_edge_ori;
using cube::solver::decode_edge_ori;
using cube::solver::phase1_coords_of;
using cube::solver::CORNER_ORI_COORDS;
using cube::solver::EDGE_ORI_COORDS;

// ---------- Corner orientation ----------

TEST(CornerOriCoord, SolvedCubeIsZero) {
    EXPECT_EQ(encode_corner_ori(cube::solved_cube()), 0u);
}

TEST(CornerOriCoord, AfterRMoveHasExpectedCoord) {
    // R sets corner_orientations = {2,0,0,1,1,0,0,2} (ori[7] = 2, dropped).
    // Base-3 (MSD = slot 0) of the first 7 digits {2,0,0,1,1,0,0}:
    //   2*729 + 0*243 + 0*81 + 1*27 + 1*9 + 0*3 + 0*1 = 1494.
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::R);
    EXPECT_EQ(encode_corner_ori(c), 1494u);
}

TEST(CornerOriCoord, RoundTripAllCoords) {
    for (Coord coord = 0; coord < CORNER_ORI_COORDS; ++coord) {
        auto ori = decode_corner_ori(coord);
        // Sum must obey the parity constraint.
        uint16_t sum = 0;
        for (auto v : ori) sum += v;
        ASSERT_EQ(sum % 3, 0) << "decoded ori violates sum-mod-3 for coord " << coord;

        // Re-encode via a CubeState.
        CubeState c = cube::solved_cube();
        c.corner_orientations = ori;
        EXPECT_EQ(encode_corner_ori(c), coord) << "round-trip failed for coord " << coord;
    }
}

// ---------- Edge orientation ----------

TEST(EdgeOriCoord, SolvedCubeIsZero) {
    EXPECT_EQ(encode_edge_ori(cube::solved_cube()), 0u);
}

TEST(EdgeOriCoord, AfterFMoveHasExpectedCoord) {
    // F sets edge_orientations = {0,1,0,0,0,1,0,0,1,1,0,0} (ori[11] = 0, dropped).
    // Base-2 (MSD = slot 0) of the first 11 digits {0,1,0,0,0,1,0,0,1,1,0}:
    //   0b01000100110 = 550.
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::F);
    EXPECT_EQ(encode_edge_ori(c), 550u);
}

TEST(EdgeOriCoord, RoundTripAllCoords) {
    for (Coord coord = 0; coord < EDGE_ORI_COORDS; ++coord) {
        auto ori = decode_edge_ori(coord);
        // Parity constraint: XOR of all flips is 0.
        uint8_t parity = 0;
        for (auto v : ori) parity ^= v;
        ASSERT_EQ(parity, 0) << "decoded ori violates parity for coord " << coord;

        CubeState c = cube::solved_cube();
        c.edge_orientations = ori;
        EXPECT_EQ(encode_edge_ori(c), coord) << "round-trip failed for coord " << coord;
    }
}

// ---------- Aggregator ----------

TEST(Phase1Coords, SolvedCubeIsAllZero) {
    auto p = phase1_coords_of(cube::solved_cube());
    EXPECT_EQ(p.corner_ori, 0u);
    EXPECT_EQ(p.edge_ori, 0u);
}

TEST(Phase1Coords, DelegatesToPrimitives) {
    // The aggregator must return exactly what the primitives return, for the
    // same state. Any drift means the two APIs disagree.
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::R);
    cube::apply_move(c, Move::U);
    cube::apply_move(c, Move::F);

    auto p = phase1_coords_of(c);
    EXPECT_EQ(p.corner_ori, encode_corner_ori(c));
    EXPECT_EQ(p.edge_ori,   encode_edge_ori(c));
}
