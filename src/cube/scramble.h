#pragma once

#include <cstdint>
#include <vector>

#include "cube/cube.h"

namespace cube {

constexpr int DEFAULT_SCRAMBLE_LENGTH = 25;

// Generates a random scramble of `length` moves using `seed` as the RNG seed.
// Same seed + same length always produces the same sequence.
// Guarantees no two consecutive moves are on the same face.
std::vector<Move> random_scramble(int length, uint64_t seed);

}
