#pragma once

#include <array>
#include <cstdint>

#include "solver/coords.h"
#include "solver/move_tables.h"

namespace cube::solver {

// pruning tables give lower bound on remaining moves to phase goal
// each is indexed by pair (main coord, slice coord); IDA* takes max
// of the two per-phase tables as heuristic

using Distance = uint8_t;

// phase 1: goal is (corner_ori=0, edge_ori=0, slice=SLICE_GOAL)
using CornerOriSliceTable = std::array<std::array<Distance, SLICE_COORDS>, CORNER_ORI_COORDS>;
using EdgeOriSliceTable = std::array<std::array<Distance, SLICE_COORDS>, EDGE_ORI_COORDS>;

const CornerOriSliceTable& corner_ori_slice_pruning();
const EdgeOriSliceTable&   edge_ori_slice_pruning();

// phase 2: goal is all-zero
using CornerPermSlicePermTable = std::array<std::array<Distance, SLICE_PERM_COORDS>, CORNER_PERM_COORDS>;
using EdgePermSlicePermTable = std::array<std::array<Distance, SLICE_PERM_COORDS>, EDGE_PERM_UD_COORDS>;

const CornerPermSlicePermTable& corner_perm_slice_perm_pruning();
const EdgePermSlicePermTable&   edge_perm_slice_perm_pruning();

}
