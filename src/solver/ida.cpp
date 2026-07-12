#include "solver/ida.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

#include "solver/coords.h"
#include "solver/move_tables.h"
#include "solver/pruning.h"

namespace cube::solver {

namespace {

constexpr int MAX_DEPTH_PHASE1 = 25;
constexpr int MAX_DEPTH_PHASE2 = 25;
constexpr int SEARCH_FAILED = std::numeric_limits<int>::max();

inline int face_of(int move_index) { return move_index / 3; }

// pick one canonical order per pair to prune equivalent branches, U/D, R/L, F/B are parallel pairs
inline bool same_axis(int f1, int f2) { return (f1 / 2) == (f2 / 2); }

struct Phase1Node {
    Coord co, eo, sl;
};

// bundle each phase's tables into a Ctx so the DFS reads them via one
// stashed reference instead of hitting five static-local getters per node
struct Phase1Ctx {
    const CornerOriTable& co_moves;
    const EdgeOriTable& eo_moves;
    const SliceTable& sl_moves;
    const CornerOriSliceTable& co_sl_prune;
    const EdgeOriSliceTable& eo_sl_prune;
};

inline int phase1_h(const Phase1Ctx& c, Phase1Node n) {
    return std::max<int>(c.co_sl_prune[n.co][n.sl], c.eo_sl_prune[n.eo][n.sl]);
}

inline bool phase1_is_goal(Phase1Node n) {
    return n.co == 0 && n.eo == 0 && n.sl == SLICE_GOAL;
}

inline Phase1Node phase1_apply(const Phase1Ctx& c, Phase1Node n, int m) {
    return {
        c.co_moves[n.co][m],
        c.eo_moves[n.eo][m],
        c.sl_moves[n.sl][m],
    };
}

struct Phase2Node {
    Coord cp, ep, sp;
};

struct Phase2Ctx {
    const CornerPermTable& cp_moves;
    const EdgePermUDTable& ep_moves;
    const SlicePermTable& sp_moves;
    const CornerPermSlicePermTable& cp_sp_prune;
    const EdgePermSlicePermTable& ep_sp_prune;
};

inline int phase2_h(const Phase2Ctx& c, Phase2Node n) {
    return std::max<int>(c.cp_sp_prune[n.cp][n.sp], c.ep_sp_prune[n.ep][n.sp]);
}

inline bool phase2_is_goal(Phase2Node n) {
    return n.cp == 0 && n.ep == 0 && n.sp == 0;
}

inline Phase2Node phase2_apply(const Phase2Ctx& c, Phase2Node n, int i) {
    return {
        c.cp_moves[n.cp][i],
        c.ep_moves[n.ep][i],
        c.sp_moves[n.sp][i],
    };
}

// bool-returning DFS: pushes onto `path` descending, pops on backtrack.
// updates `next_bound` with the smallest f-value that exceeded `bound`.
bool phase1_dfs(const Phase1Ctx& c, Phase1Node n, int g, int bound, int& next_bound, int last_face, int last_axis_partner_face, std::vector<int>& path)
{
    int f = g + phase1_h(c, n);
    if (f > bound) {
        if (f < next_bound) next_bound = f;
        return false;
    }
    if (phase1_is_goal(n)) return true;

    for (int m = 0; m < NUM_MOVES; ++m) {
        int face = face_of(m);
        if (face == last_face) continue;
        if (same_axis(face, last_face) && face < last_face) continue;
        if (face == last_axis_partner_face) continue;

        Phase1Node next = phase1_apply(c, n, m);
        int new_partner = same_axis(face, last_face) ? last_face : -1;
        path.push_back(m);
        if (phase1_dfs(c, next, g + 1, bound, next_bound, face, new_partner, path)) {
            return true;
        }
        path.pop_back();
    }
    return false;
}

bool phase2_dfs(const Phase2Ctx& c, Phase2Node n, int g, int bound, int& next_bound, int last_face, int last_axis_partner_face, std::vector<int>& path)
{
    int f = g + phase2_h(c, n);
    if (f > bound) {
        if (f < next_bound) next_bound = f;
        return false;
    }
    if (phase2_is_goal(n)) return true;

    for (int i = 0; i < NUM_G1_MOVES; ++i) {
        int face = face_of(static_cast<int>(G1_MOVES[i]));
        if (face == last_face) continue;
        if (same_axis(face, last_face) && face < last_face) continue;
        if (face == last_axis_partner_face) continue;

        Phase2Node next = phase2_apply(c, n, i);
        int new_partner = same_axis(face, last_face) ? last_face : -1;
        path.push_back(i);
        if (phase2_dfs(c, next, g + 1, bound, next_bound, face, new_partner, path)) {
            return true;
        }
        path.pop_back();
    }
    return false;
}

}  // namespace

std::vector<Move> ida_phase1(const CubeState& state) {
    const Phase1Ctx ctx = {
        corner_ori_move_table(),
        edge_ori_move_table(),
        slice_move_table(),
        corner_ori_slice_pruning(),
        edge_ori_slice_pruning(),
    };
    Phase1Node start = {
        encode_corner_ori(state),
        encode_edge_ori(state),
        encode_slice(state),
    };
    if (phase1_is_goal(start)) return {};

    std::vector<int> path;
    int bound = phase1_h(ctx, start);
    while (bound <= MAX_DEPTH_PHASE1) {
        int next_bound = SEARCH_FAILED;
        path.clear();
        if (phase1_dfs(ctx, start, 0, bound, next_bound, -1, -1, path)) {
            std::vector<Move> out;
            out.reserve(path.size());
            for (int m : path) out.push_back(static_cast<Move>(m));
            return out;
        }
        if (next_bound == SEARCH_FAILED) return {};   // exhausted
        bound = next_bound;
    }
    return {};
}

std::vector<Move> ida_phase2(const CubeState& state) {
    const Phase2Ctx ctx = {
        corner_perm_move_table(),
        edge_perm_ud_move_table(),
        slice_perm_move_table(),
        corner_perm_slice_perm_pruning(),
        edge_perm_slice_perm_pruning(),
    };
    Phase2Node start = {
        encode_corner_perm(state),
        encode_edge_perm_ud(state),
        encode_slice_perm(state),
    };
    if (phase2_is_goal(start)) return {};

    std::vector<int> path;
    int bound = phase2_h(ctx, start);
    while (bound <= MAX_DEPTH_PHASE2) {
        int next_bound = SEARCH_FAILED;
        path.clear();
        if (phase2_dfs(ctx, start, 0, bound, next_bound, -1, -1, path)) {
            std::vector<Move> out;
            out.reserve(path.size());
            for (int i : path) out.push_back(G1_MOVES[i]);
            return out;
        }
        if (next_bound == SEARCH_FAILED) return {};
        bound = next_bound;
    }
    return {};
}

std::vector<Move> solve(const CubeState& scramble) {
    std::vector<Move> solution = ida_phase1(scramble);

    CubeState intermediate = scramble;
    for (Move m : solution) apply_move(intermediate, m);

    std::vector<Move> p2 = ida_phase2(intermediate);
    solution.insert(solution.end(), p2.begin(), p2.end());
    return solution;
}

}
