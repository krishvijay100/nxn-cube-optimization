#include "solver/coords.h"

namespace cube::solver {

namespace {

// Binomial coefficients C(n, k) for n in [0..12], k in [0..4].
// Used by the slice coord's combinadic ranking; embed as
// a constexpr table so lookups are a single load in the search hot path.
constexpr std::array<std::array<uint16_t, 5>, 13> make_binom() {
    std::array<std::array<uint16_t, 5>, 13> t{};
    for (int n = 0; n <= 12; ++n) {
        t[n][0] = 1;
        for (int k = 1; k <= 4; ++k) {
            t[n][k] = (n == 0) ? 0 : (t[n - 1][k - 1] + t[n - 1][k]);
        }
    }
    return t;
}
constexpr auto BINOM = make_binom();

constexpr uint16_t C(int n, int k) {
    if (n < k || n < 0 || k < 0) return 0;
    return BINOM[n][k];
}

// any edge whose home slot is 8..11 (FR, FL, BL, BR).
constexpr bool is_equator_edge(uint8_t edge_id) {
    return edge_id >= 8;
}

}  // namespace

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

// Slice position: which 4 slots hold equator edges. Combinadic (colex) rank.
// coord = C(c3,4) + C(c2,3) + C(c1,2) + C(c0,1), where c3 > c2 > c1 > c0 are
// the slot indices (0..11) currently occupied by equator edges.
Coord encode_slice(const CubeState& c) {
    std::array<uint8_t, 4> slots{};
    uint8_t found = 0;
    for (uint8_t slot = 0; slot < NUM_EDGES; ++slot) {
        if (is_equator_edge(c.edge_positions[slot])) {
            slots[found++] = slot;
        }
    }
    // slots are already in ascending order; combinadic wants descending.
    // slots = {c0, c1, c2, c3}; formula uses C(c3,4)+C(c2,3)+C(c1,2)+C(c0,1).
    return static_cast<Coord>(
        C(slots[3], 4) + C(slots[2], 3) + C(slots[1], 2) + C(slots[0], 1));
}

std::array<uint8_t, 4> decode_slice(Coord coord) {
    // Greedy inverse: for k = 4 down to 1, find the largest c with C(c,k) <= coord.
    std::array<uint8_t, 4> slots{};
    int remaining = coord;
    for (int k = 4; k >= 1; --k) {
        int c = k - 1;
        while (c + 1 <= 11 && C(c + 1, k) <= remaining) ++c;
        slots[k - 1] = static_cast<uint8_t>(c);
        remaining -= C(c, k);
    }
    // slots is now descending {c3, c2, c1, c0}; return ascending for callers.
    return { slots[0], slots[1], slots[2], slots[3] };
}

Phase1Coords phase1_coords_of(const CubeState& c) {
    return {
        encode_corner_ori(c),
        encode_edge_ori(c),
        encode_slice(c),
    };
}

}
