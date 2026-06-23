#include "cube/cube.h"

namespace cube {

CubeState solved_cube() {
    CubeState c{};
    for (uint8_t i = 0; i < NUM_CORNERS; ++i) {
        c.corner_positions[i] = i;
        c.corner_orientations[i] = 0;
    }
    for (uint8_t i = 0; i < NUM_EDGES; ++i) {
        c.edge_positions[i] = i;
        c.edge_orientations[i] = 0;
    }
    return c;
}

bool is_solved(const CubeState& c) {
    for (uint8_t i = 0; i < NUM_CORNERS; ++i) {
        if (c.corner_positions[i] != i) return false;
        if (c.corner_orientations[i] != 0) return false;
    }
    for (uint8_t i = 0; i < NUM_EDGES; ++i) {
        if (c.edge_positions[i] != i) return false;
        if (c.edge_orientations[i] != 0) return false;
    }
    return true;
}

}
