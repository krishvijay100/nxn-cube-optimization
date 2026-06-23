#pragma once

#include <array>
#include <cstdint>

namespace cube {

constexpr int NUM_CORNERS = 8;
constexpr int NUM_EDGES = 12;

struct CubeState {
    std::array<uint8_t, NUM_CORNERS> corner_positions;
    std::array<uint8_t, NUM_CORNERS> corner_orientations;
    std::array<uint8_t, NUM_EDGES> edge_positions;
    std::array<uint8_t, NUM_EDGES> edge_orientations;
};

CubeState solved_cube();

bool is_solved(const CubeState& c);

}
