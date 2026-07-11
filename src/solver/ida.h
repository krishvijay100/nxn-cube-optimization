#pragma once

#include <vector>

#include "cube/cube.h"

namespace cube::solver {

// returns the move sequence that transforms scramble into the solved cube; returns empty on malformed
// input or if the internal depth cap is hit (never expected for valid states)
std::vector<Move> solve(const CubeState& scramble);

// individual phase solvers, exposed so tests can exercise them in isolation
std::vector<Move> ida_phase1(const CubeState& state);
std::vector<Move> ida_phase2(const CubeState& state);

}
