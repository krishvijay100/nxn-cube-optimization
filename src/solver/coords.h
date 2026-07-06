#pragma once

#include <array>
#include <cstdint>

#include "cube/cube.h"

namespace cube::solver {

// Coordinate encodings compress the parts of a CubeState that Kociemba's search
// actually cares about into small integers. Each coord captures ONE independent
// aspect of the state.
//
// Encoding conventions:
//   - MSD = slot 0. ori[0] gets the highest digit weight.
//   - Redundant final digit is dropped: the parity/sum constraint recovers it
//     on decode. This shrinks the coord range (and the pruning table indexed
//     by it) with zero information loss.
//   - All coord values are stored as uint16_t (widest coord is 40319, fits).

using Coord = uint16_t;

// Corner orientation: 8 twists in {0,1,2}, sum mod 3 == 0.
// Encode ori[0..6] as base-3 digits (drop ori[7]). Range: 0..2186 (3^7).
constexpr Coord CORNER_ORI_COORDS = 2187;
Coord encode_corner_ori(const CubeState& c);
std::array<uint8_t, NUM_CORNERS> decode_corner_ori(Coord coord);

// Edge orientation: 12 flips in {0,1}, sum mod 2 == 0.
// Encode ori[0..10] as base-2 digits (drop ori[11]). Range: 0..2047 (2^11).
constexpr Coord EDGE_ORI_COORDS = 2048;
Coord encode_edge_ori(const CubeState& c);
std::array<uint8_t, NUM_EDGES> decode_edge_ori(Coord coord);

// Aggregator: bundle Phase 1's three coords together for search callers.
// (Slice coord added in a later commit — for now this is a placeholder shape.)
struct Phase1Coords {
    Coord corner_ori;
    Coord edge_ori;
    // Coord slice;   // to come
};

Phase1Coords phase1_coords_of(const CubeState& c);

}
