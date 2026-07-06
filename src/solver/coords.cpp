#include "solver/coords.h"

namespace cube::solver {

Coord encode_corner_ori(const CubeState& c) {
    Coord coord = 0;
    for (uint8_t i = 0; i < NUM_CORNERS - 1; ++i) {
        coord = coord * 3 + c.corner_orientations[i];
    }
    return coord;
}

std::array<uint8_t, NUM_CORNERS> decode_corner_ori(Coord coord) {
    std::array<uint8_t, NUM_CORNERS> ori{};
    uint8_t sum = 0;
    for (int i = NUM_CORNERS - 2; i >= 0; --i) {
        ori[i] = coord % 3;
        coord /= 3;
        sum += ori[i];
    }
    ori[NUM_CORNERS - 1] = (3 - sum % 3) % 3;
    return ori;
}

Coord encode_edge_ori(const CubeState& c) {
    Coord coord = 0;
    for (uint8_t i = 0; i < NUM_EDGES - 1; ++i) {
        coord = (coord << 1) | c.edge_orientations[i];
    }
    return coord;
}

std::array<uint8_t, NUM_EDGES> decode_edge_ori(Coord coord) {
    std::array<uint8_t, NUM_EDGES> ori{};
    uint8_t parity = 0;
    for (int i = NUM_EDGES - 2; i >= 0; --i) {
        ori[i] = coord & 1;
        coord >>= 1;
        parity ^= ori[i];
    }
    ori[NUM_EDGES - 1] = parity;
    return ori;
}

Phase1Coords phase1_coords_of(const CubeState& c) {
    return {
        encode_corner_ori(c),
        encode_edge_ori(c),
    };
}

}
