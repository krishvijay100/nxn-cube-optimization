#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cube/cube.h"

namespace cube_test {

inline int corner_orient_sum(const cube::CubeState& c) {
    int s = 0;
    for (auto v : c.corner_orientations) s += v;
    return s;
}

inline int edge_orient_sum(const cube::CubeState& c) {
    int s = 0;
    for (auto v : c.edge_orientations) s += v;
    return s;
}

template <std::size_t N>
inline int permutation_parity(const std::array<uint8_t, N>& p) {
    int inversions = 0;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = i + 1; j < N; ++j)
            if (p[i] > p[j]) ++inversions;
    return inversions % 2;
}

inline bool laws_hold(const cube::CubeState& c) {
    return corner_orient_sum(c) % 3 == 0
        && edge_orient_sum(c) % 2 == 0
        && permutation_parity(c.corner_positions) ==
           permutation_parity(c.edge_positions);
}

}
