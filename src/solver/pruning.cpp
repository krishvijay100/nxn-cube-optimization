#include "solver/pruning.h"

#include <cstddef>
#include <memory>

namespace cube::solver {

namespace {

constexpr Distance UNVISITED = 0xFF;

// backward BFS over the (main coord, slice coord) product space
// applies each legal move via the two move tables and marks newly-reached pairs at depth+1
template <typename Table, size_t MainCount, size_t SliceCount, typename MainMoveTable, typename SliceMoveTable>
std::unique_ptr<Table> build_pair_table(
    Coord goal_main, Coord goal_slice,
    const MainMoveTable& main_moves,
    const SliceMoveTable& slice_moves,
    int legal_moves)
{
    auto t = std::make_unique<Table>();
    for (auto& row : *t) row.fill(UNVISITED);

    (*t)[goal_main][goal_slice] = 0;

    size_t total = MainCount * SliceCount;
    size_t filled = 1;
    Distance depth = 0;
    while (filled < total) {
        bool any_new = false;
        for (size_t mi = 0; mi < MainCount; ++mi) {
            for (size_t si = 0; si < SliceCount; ++si) {
                if ((*t)[mi][si] != depth) continue;
                for (int m = 0; m < legal_moves; ++m) {
                    Coord nm = main_moves[mi][m];
                    Coord ns = slice_moves[si][m];
                    if ((*t)[nm][ns] == UNVISITED) {
                        (*t)[nm][ns] = depth + 1;
                        ++filled;
                        any_new = true;
                    }
                }
            }
        }
        if (!any_new) break;
        ++depth;
    }
    return t;
}

std::unique_ptr<CornerOriSliceTable> build_corner_ori_slice() {
    return build_pair_table<CornerOriSliceTable, CORNER_ORI_COORDS, SLICE_COORDS>(
        0, SLICE_GOAL,
        corner_ori_move_table(),
        slice_move_table(),
        NUM_MOVES);
}

std::unique_ptr<EdgeOriSliceTable> build_edge_ori_slice() {
    return build_pair_table<EdgeOriSliceTable, EDGE_ORI_COORDS, SLICE_COORDS>(
        0, SLICE_GOAL,
        edge_ori_move_table(),
        slice_move_table(),
        NUM_MOVES);
}

std::unique_ptr<CornerPermSlicePermTable> build_corner_perm_slice_perm() {
    return build_pair_table<
        CornerPermSlicePermTable, CORNER_PERM_COORDS, SLICE_PERM_COORDS>(
        0, 0,
        corner_perm_move_table(),
        slice_perm_move_table(),
        NUM_G1_MOVES);
}

std::unique_ptr<EdgePermSlicePermTable> build_edge_perm_slice_perm() {
    return build_pair_table<
        EdgePermSlicePermTable, EDGE_PERM_UD_COORDS, SLICE_PERM_COORDS>(
        0, 0,
        edge_perm_ud_move_table(),
        slice_perm_move_table(),
        NUM_G1_MOVES);
}

}  // namespace

const CornerOriSliceTable& corner_ori_slice_pruning() {
    static const auto table = build_corner_ori_slice();
    return *table;
}

const EdgeOriSliceTable& edge_ori_slice_pruning() {
    static const auto table = build_edge_ori_slice();
    return *table;
}

const CornerPermSlicePermTable& corner_perm_slice_perm_pruning() {
    static const auto table = build_corner_perm_slice_perm();
    return *table;
}

const EdgePermSlicePermTable& edge_perm_slice_perm_pruning() {
    static const auto table = build_edge_perm_slice_perm();
    return *table;
}

}
