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

// Equator-slice position: which 4 of the 12 edge slots hold equator edges
// (originally in slots 8..11 = FR, FL, BL, BR). Order within the slice is
// ignored — this is a subset, not a permutation. Range: 0..494 (C(12,4)).
//
// Combinadic (colex) ranking: given the 4 slots c3 > c2 > c1 > c0 currently
// holding equator edges, coord = C(c3,4) + C(c2,3) + C(c1,2) + C(c0,1).
// note: the phase 1 goal state (slots {8,9,10,11}) is coord 494 (max).
constexpr Coord SLICE_COORDS = 495;
constexpr Coord SLICE_GOAL   = 494;
Coord encode_slice(const CubeState& c);
std::array<uint8_t, 4> decode_slice(Coord coord);

// Aggregator: bundle Phase 1's three coords together for search callers.
struct Phase1Coords {
    Coord corner_ori;
    Coord edge_ori;
    Coord slice;
};

Phase1Coords phase1_coords_of(const CubeState& c);

// ---------- Phase 2 permutation coords ----------
//
// Once inside G1, the remaining work is permutation only:
//   - Corner permutation: 8 corners in slots 0-7              [0..40319 (8!)]
//   - U/D edge permutation: 8 non-equator edges in slots 0-7  [0..40319 (8!)]
//   - Slice permutation: 4 equator edges in slots 8-11        [0..23    (4!)]
//
// All three use Lehmer-code + factorial-base encoding, so the solved cube
// is coord 0 for each of them (aka identity permutation)

constexpr Coord CORNER_PERM_COORDS  = 40320;
constexpr Coord EDGE_PERM_UD_COORDS = 40320;
constexpr Coord SLICE_PERM_COORDS   = 24;

Coord encode_corner_perm(const CubeState& c);
std::array<uint8_t, NUM_CORNERS> decode_corner_perm(Coord coord);

Coord encode_edge_perm_ud(const CubeState& c);
std::array<uint8_t, 8> decode_edge_perm_ud(Coord coord);

Coord encode_slice_perm(const CubeState& c);
std::array<uint8_t, 4> decode_slice_perm(Coord coord);

// Aggregator: bundle Phase 2's three coords together for search callers
struct Phase2Coords {
    Coord corner_perm;
    Coord edge_perm_ud;
    Coord slice_perm;
};

Phase2Coords phase2_coords_of(const CubeState& c);

}
