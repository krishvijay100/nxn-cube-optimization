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
using cube::solver::encode_slice;
using cube::solver::decode_slice;
using cube::solver::CORNER_ORI_COORDS;
using cube::solver::EDGE_ORI_COORDS;
using cube::solver::SLICE_COORDS;
using cube::solver::SLICE_GOAL;
using cube::solver::encode_corner_perm;
using cube::solver::decode_corner_perm;
using cube::solver::encode_edge_perm_ud;
using cube::solver::decode_edge_perm_ud;
using cube::solver::encode_slice_perm;
using cube::solver::decode_slice_perm;
using cube::solver::phase2_coords_of;
using cube::solver::CORNER_PERM_COORDS;
using cube::solver::EDGE_PERM_UD_COORDS;
using cube::solver::SLICE_PERM_COORDS;

TEST(CornerOriCoord, SolvedCubeIsZero) {
    EXPECT_EQ(encode_corner_ori(cube::solved_cube()), 0u);
}

TEST(CornerOriCoord, AfterRMoveHasExpectedCoord) {
    // R gives corner_orientations = {2,0,0,1,1,0,0,2}; dropping ori[7] and
    // taking base-3 of {2,0,0,1,1,0,0}: 2*729 + 1*27 + 1*9 = 1494
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::R);
    EXPECT_EQ(encode_corner_ori(c), 1494u);
}

TEST(CornerOriCoord, RoundTripAllCoords) {
    for (Coord coord = 0; coord < CORNER_ORI_COORDS; ++coord) {
        auto ori = decode_corner_ori(coord);
        uint16_t sum = 0;
        for (auto v : ori) sum += v;
        ASSERT_EQ(sum % 3, 0) << "decoded ori violates sum-mod-3 for coord " << coord;

        CubeState c = cube::solved_cube();
        c.corner_orientations = ori;
        EXPECT_EQ(encode_corner_ori(c), coord) << "round-trip failed for coord " << coord;
    }
}

TEST(EdgeOriCoord, SolvedCubeIsZero) {
    EXPECT_EQ(encode_edge_ori(cube::solved_cube()), 0u);
}

TEST(EdgeOriCoord, AfterFMoveHasExpectedCoord) {
    // F gives edge_orientations = {0,1,0,0,0,1,0,0,1,1,0,0}; dropping ori[11]
    // and taking base-2 of {0,1,0,0,0,1,0,0,1,1,0}: 0b01000100110 = 550
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::F);
    EXPECT_EQ(encode_edge_ori(c), 550u);
}

TEST(EdgeOriCoord, RoundTripAllCoords) {
    for (Coord coord = 0; coord < EDGE_ORI_COORDS; ++coord) {
        auto ori = decode_edge_ori(coord);
        uint8_t parity = 0;
        for (auto v : ori) parity ^= v;
        ASSERT_EQ(parity, 0) << "decoded ori violates parity for coord " << coord;

        CubeState c = cube::solved_cube();
        c.edge_orientations = ori;
        EXPECT_EQ(encode_edge_ori(c), coord) << "round-trip failed for coord " << coord;
    }
}

TEST(SliceCoord, SolvedCubeIsGoal) {
    EXPECT_EQ(encode_slice(cube::solved_cube()), SLICE_GOAL);
}

TEST(SliceCoord, LowestSubsetIsZero) {
    // equator edges in slots {0,1,2,3} => coord 0
    CubeState c = cube::solved_cube();
    c.edge_positions = {8, 9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7};
    EXPECT_EQ(encode_slice(c), 0u);
}

TEST(SliceCoord, AfterFMoveMatchesEquatorMovement) {
    // F sends FR(id 8): slot 8 -> 1, FL(id 9): slot 9 -> 5. equator edges now
    // occupy slots {1,5,10,11}: C(11,4)+C(10,3)+C(5,2)+C(1,1) = 461
    CubeState c = cube::solved_cube();
    cube::apply_move(c, cube::Move::F);
    EXPECT_EQ(encode_slice(c), 461u);
}

TEST(SliceCoord, RoundTripAllCoords) {
    for (Coord coord = 0; coord < SLICE_COORDS; ++coord) {
        auto slots = decode_slice(coord);

        ASSERT_LT(slots[0], slots[1]) << "coord " << coord;
        ASSERT_LT(slots[1], slots[2]) << "coord " << coord;
        ASSERT_LT(slots[2], slots[3]) << "coord " << coord;
        ASSERT_LT(slots[3], 12u)      << "coord " << coord;

        // rebuild a CubeState with equator edges at those slots (same shape
        // the move-table builder uses) and re-encode
        CubeState c = cube::solved_cube();
        for (uint8_t i = 0; i < 12; ++i) c.edge_positions[i] = 0;
        for (uint8_t k = 0; k < 4; ++k) {
            c.edge_positions[slots[k]] = static_cast<uint8_t>(8 + k);
        }
        uint8_t next_non_equator = 0;
        for (uint8_t i = 0; i < 12; ++i) {
            bool is_slice_slot = (i == slots[0] || i == slots[1] ||
                                  i == slots[2] || i == slots[3]);
            if (!is_slice_slot) {
                c.edge_positions[i] = next_non_equator++;
            }
        }

        EXPECT_EQ(encode_slice(c), coord) << "round-trip failed for coord " << coord;
    }
}

TEST(CornerPermCoord, SolvedCubeIsZero) {
    EXPECT_EQ(encode_corner_perm(cube::solved_cube()), 0u);
}

TEST(CornerPermCoord, AfterUMoveHasExpectedCoord) {
    // U -> corner_positions {3,0,1,2,4,5,6,7}, Lehmer {3,0,...}, coord 3*7! = 15120
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::U);
    EXPECT_EQ(encode_corner_perm(c), 15120u);
}

TEST(CornerPermCoord, RoundTripSampled) {
    for (Coord coord = 0; coord < CORNER_PERM_COORDS; ++coord) {
        auto perm = decode_corner_perm(coord);

        std::array<bool, 8> seen{};
        for (auto v : perm) {
            ASSERT_LT(v, 8u) << "coord " << coord;
            ASSERT_FALSE(seen[v]) << "duplicate value " << int(v) << " at coord " << coord;
            seen[v] = true;
        }

        CubeState c = cube::solved_cube();
        c.corner_positions = perm;
        ASSERT_EQ(encode_corner_perm(c), coord) << "round-trip failed for coord " << coord;
    }
}

TEST(EdgePermUDCoord, SolvedCubeIsZero) {
    EXPECT_EQ(encode_edge_perm_ud(cube::solved_cube()), 0u);
}

TEST(EdgePermUDCoord, AfterUMoveHasExpectedCoord) {
    // same shape as corner case: edge_positions[0..7] -> {3,0,1,2,4,5,6,7}, coord 15120
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::U);
    EXPECT_EQ(encode_edge_perm_ud(c), 15120u);
}

TEST(EdgePermUDCoord, RoundTripSampled) {
    for (Coord coord = 0; coord < EDGE_PERM_UD_COORDS; ++coord) {
        auto perm = decode_edge_perm_ud(coord);

        std::array<bool, 8> seen{};
        for (auto v : perm) {
            ASSERT_LT(v, 8u) << "coord " << coord;
            ASSERT_FALSE(seen[v]) << "duplicate value " << int(v) << " at coord " << coord;
            seen[v] = true;
        }

        // leave equator slots at solved defaults; encode reads only slots 0-7
        CubeState c = cube::solved_cube();
        for (size_t i = 0; i < 8; ++i) c.edge_positions[i] = perm[i];
        ASSERT_EQ(encode_edge_perm_ud(c), coord) << "round-trip failed for coord " << coord;
    }
}

TEST(SlicePermCoord, SolvedCubeIsZero) {
    EXPECT_EQ(encode_slice_perm(cube::solved_cube()), 0u);
}

TEST(SlicePermCoord, ReversedSliceHasMaxCoord) {
    // equator edges reversed: {11,10,9,8}, normalized {3,2,1,0}, coord = 4! - 1 = 23
    CubeState c = cube::solved_cube();
    c.edge_positions[8]  = 11;
    c.edge_positions[9]  = 10;
    c.edge_positions[10] = 9;
    c.edge_positions[11] = 8;
    EXPECT_EQ(encode_slice_perm(c), 23u);
}

TEST(SlicePermCoord, RoundTripAllCoords) {
    for (Coord coord = 0; coord < SLICE_PERM_COORDS; ++coord) {
        auto perm = decode_slice_perm(coord);

        std::array<bool, 4> seen{};
        for (auto v : perm) {
            ASSERT_LT(v, 4u) << "coord " << coord;
            ASSERT_FALSE(seen[v]) << "duplicate value " << int(v) << " at coord " << coord;
            seen[v] = true;
        }

        CubeState c = cube::solved_cube();
        for (size_t i = 0; i < 4; ++i) {
            c.edge_positions[8 + i] = static_cast<uint8_t>(perm[i] + 8);
        }
        ASSERT_EQ(encode_slice_perm(c), coord) << "round-trip failed for coord " << coord;
    }
}

TEST(Phase2Coords, SolvedCubeIsAllZero) {
    auto p = phase2_coords_of(cube::solved_cube());
    EXPECT_EQ(p.corner_perm,  0u);
    EXPECT_EQ(p.edge_perm_ud, 0u);
    EXPECT_EQ(p.slice_perm,   0u);
}

TEST(Phase2Coords, DelegatesToPrimitives) {
    // U is a G1 move so it's safe to apply here
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::U);
    cube::apply_move(c, Move::U);

    auto p = phase2_coords_of(c);
    EXPECT_EQ(p.corner_perm,  encode_corner_perm(c));
    EXPECT_EQ(p.edge_perm_ud, encode_edge_perm_ud(c));
    EXPECT_EQ(p.slice_perm,   encode_slice_perm(c));
}

TEST(Phase1Coords, SolvedCubeIsAtGoal) {
    auto p = phase1_coords_of(cube::solved_cube());
    EXPECT_EQ(p.corner_ori, 0u);
    EXPECT_EQ(p.edge_ori,   0u);
    EXPECT_EQ(p.slice,      SLICE_GOAL);
}

TEST(Phase1Coords, DelegatesToPrimitives) {
    CubeState c = cube::solved_cube();
    cube::apply_move(c, Move::R);
    cube::apply_move(c, Move::U);
    cube::apply_move(c, Move::F);

    auto p = phase1_coords_of(c);
    EXPECT_EQ(p.corner_ori, encode_corner_ori(c));
    EXPECT_EQ(p.edge_ori,   encode_edge_ori(c));
    EXPECT_EQ(p.slice,      encode_slice(c));
}
