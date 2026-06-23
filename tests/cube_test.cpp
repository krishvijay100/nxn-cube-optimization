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
