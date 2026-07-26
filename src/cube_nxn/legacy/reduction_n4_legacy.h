#pragma once

#include <vector>

#include "cube_nxn/cube_nxn.h"

namespace cube_nxn::legacy_n4 {

std::vector<MoveStep> solve_centers_n4(NxNCube& cube);

struct EdgePairResult {
    std::vector<MoveStep> sequence;
    int edges_paired;
};

EdgePairResult solve_edges_n4_algo(NxNCube& cube);

ParityState detect_parity_n4(const NxNCube& cube);
std::vector<MoveStep> fix_parity_n4(NxNCube& cube);

}  // namespace cube_nxn::legacy_n4
