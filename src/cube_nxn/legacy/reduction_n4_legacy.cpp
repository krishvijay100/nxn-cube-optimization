#include "cube_nxn/legacy/reduction_n4_legacy.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace cube_nxn::legacy_n4 {
namespace {

constexpr int N4_CENTER_SLOTS[4][2] = {{1,1}, {1,2}, {2,1}, {2,2}};

bool face_centers_monochrome(const NxNCube& cube, Face face) {
    const uint8_t color = static_cast<uint8_t>(face);
    for (const auto& slot : N4_CENTER_SLOTS) {
        if (cube.sticker(static_cast<int>(face), slot[0], slot[1]) != color) {
            return false;
        }
    }
    return true;
}

uint64_t fnv1a_full(const NxNCube& cube) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (int i = 0; i < cube.num_stickers(); ++i) {
        hash ^= cube.raw()[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

std::vector<MoveStep> bfs_with_deepening(
    const NxNCube& start,
    const std::function<bool(const NxNCube&)>& is_goal,
    const std::vector<MoveStep>& moves) {
    for (int depth : {5, 8, 12}) {
        BFSResult result =
            reduce_bfs(start, is_goal, fnv1a_full, moves, depth);
        if (result.found) return result.sequence;
    }
    return {};
}

struct Sticker {
    Face face;
    int row;
    int col;
};

struct WingPair {
    Sticker a;
    Sticker b;
};

struct EdgeSlot {
    WingPair w1;
    WingPair w2;
};

constexpr EdgeSlot N4_EDGES[12] = {
    { {{Face::U, 3, 1}, {Face::F, 0, 1}}, {{Face::U, 3, 2}, {Face::F, 0, 2}} },
    { {{Face::U, 1, 3}, {Face::R, 0, 1}}, {{Face::U, 2, 3}, {Face::R, 0, 2}} },
    { {{Face::U, 0, 2}, {Face::B, 0, 1}}, {{Face::U, 0, 1}, {Face::B, 0, 2}} },
    { {{Face::U, 2, 0}, {Face::L, 0, 1}}, {{Face::U, 1, 0}, {Face::L, 0, 2}} },
    { {{Face::D, 0, 1}, {Face::F, 3, 1}}, {{Face::D, 0, 2}, {Face::F, 3, 2}} },
    { {{Face::D, 1, 3}, {Face::R, 3, 2}}, {{Face::D, 2, 3}, {Face::R, 3, 1}} },
    { {{Face::D, 3, 2}, {Face::B, 3, 1}}, {{Face::D, 3, 1}, {Face::B, 3, 2}} },
    { {{Face::D, 2, 0}, {Face::L, 3, 2}}, {{Face::D, 1, 0}, {Face::L, 3, 1}} },
    { {{Face::F, 1, 3}, {Face::R, 1, 0}}, {{Face::F, 2, 3}, {Face::R, 2, 0}} },
    { {{Face::R, 1, 3}, {Face::B, 1, 0}}, {{Face::R, 2, 3}, {Face::B, 2, 0}} },
    { {{Face::B, 1, 3}, {Face::L, 1, 0}}, {{Face::B, 2, 3}, {Face::L, 2, 0}} },
    { {{Face::L, 1, 3}, {Face::F, 1, 0}}, {{Face::L, 2, 3}, {Face::F, 2, 0}} },
};

bool edge_slot_paired(const NxNCube& cube, const EdgeSlot& edge) {
    const uint8_t a1 = cube.sticker(
        static_cast<int>(edge.w1.a.face), edge.w1.a.row, edge.w1.a.col);
    const uint8_t b1 = cube.sticker(
        static_cast<int>(edge.w1.b.face), edge.w1.b.row, edge.w1.b.col);
    const uint8_t a2 = cube.sticker(
        static_cast<int>(edge.w2.a.face), edge.w2.a.row, edge.w2.a.col);
    const uint8_t b2 = cube.sticker(
        static_cast<int>(edge.w2.b.face), edge.w2.b.row, edge.w2.b.col);
    return a1 == a2 && b1 == b2 && a1 != b1;
}

MoveStep macro_from_str(std::string_view text) {
    auto moves = parse_scramble(text);
    assert(moves.has_value() && "legacy N4 macro failed to parse");
    return *moves;
}

std::vector<MoveStep> build_algo_edge_moves() {
    std::vector<MoveStep> out;
    for (Face face : {Face::U, Face::R, Face::F,
                      Face::D, Face::L, Face::B}) {
        for (Turn turn : {Turn::CW, Turn::Half, Turn::CCW}) {
            out.push_back({Move{face, 0, 0, turn}});
        }
    }

    const char* macros[] = {
        "Uw R U R' Uw'",    "Uw' L' U' L Uw",
        "Uw F U F' Uw'",    "Uw' F' U' F Uw",
        "Uw B U B' Uw'",    "Uw' B' U' B Uw",
        "Uw L U L' Uw'",    "Uw' R' U' R Uw",
        "Dw R' U' R Dw'",   "Dw' L U L' Dw",
        "Dw F' U' F Dw'",   "Dw' F U F' Dw",
        "Dw B' U' B Dw'",   "Dw' B U B' Dw",
        "Rw U R U' Rw'",    "Rw' U' R' U Rw",
        "Rw F R F' Rw'",    "Rw' F' R' F Rw",
        "Lw U' L' U Lw'",   "Lw' U L U' Lw",
        "Lw F' L' F Lw'",   "Lw' F L F' Lw",
    };
    for (const char* macro : macros) out.push_back(macro_from_str(macro));
    return out;
}

MoveStep parse_alg(const char* text) {
    auto moves = parse_scramble(text);
    assert(moves.has_value() && "legacy N4 parity algorithm failed to parse");
    return *moves;
}

const MoveStep& oll_parity_alg() {
    static const MoveStep algorithm = parse_alg(
        "Rw2 B2 U2 Lw U2 Rw' U2 Rw U2 F2 Rw F2 Lw' B2 Rw2");
    return algorithm;
}

const MoveStep& pll_parity_alg() {
    static const MoveStep algorithm =
        parse_alg("2R2 U2 2R2 Uw2 2R2 Uw2");
    return algorithm;
}

}  // namespace

std::vector<MoveStep> solve_centers_n4(NxNCube& cube) {
    assert(cube.n() == 4);
    const auto moves = legal_move_steps_for_stage(4, Stage::Centers);

    std::vector<MoveStep> total_sequence;
    const Face face_order[6] = {
        Face::U, Face::D, Face::F, Face::B, Face::L, Face::R,
    };
    std::vector<Face> completed;

    for (Face face : face_order) {
        const uint8_t face_color = static_cast<uint8_t>(face);
        const int face_idx = static_cast<int>(face);
        std::vector<std::pair<int, int>> placed_this_face;

        for (const auto& slot : N4_CENTER_SLOTS) {
            if (cube.sticker(face_idx, slot[0], slot[1]) == face_color) {
                placed_this_face.push_back({slot[0], slot[1]});
                continue;
            }

            const int target_row = slot[0];
            const int target_col = slot[1];
            const auto preserve = placed_this_face;
            const auto preserve_faces = completed;
            auto is_goal = [
                face_color, face_idx, target_row, target_col,
                preserve, preserve_faces
            ](const NxNCube& candidate) {
                if (candidate.sticker(
                        face_idx, target_row, target_col) != face_color) {
                    return false;
                }
                for (const auto& saved : preserve) {
                    if (candidate.sticker(
                            face_idx, saved.first, saved.second) != face_color) {
                        return false;
                    }
                }
                for (Face saved_face : preserve_faces) {
                    if (!face_centers_monochrome(candidate, saved_face)) {
                        return false;
                    }
                }
                return true;
            };

            const auto sequence =
                bfs_with_deepening(cube, is_goal, moves);
            if (sequence.empty()) return {};
            for (const auto& step : sequence) apply_move_step(cube, step);
            total_sequence.insert(
                total_sequence.end(), sequence.begin(), sequence.end());
            placed_this_face.push_back({target_row, target_col});
        }
        completed.push_back(face);
    }
    return total_sequence;
}

EdgePairResult solve_edges_n4_algo(NxNCube& cube) {
    assert(cube.n() == 4);
    static const std::vector<MoveStep> moves = build_algo_edge_moves();

    EdgePairResult result{{}, 0};
    std::vector<int> already_paired;
    for (int edge = 0; edge < 12; ++edge) {
        if (edge_slot_paired(cube, N4_EDGES[edge])) {
            already_paired.push_back(edge);
            ++result.edges_paired;
            continue;
        }

        const auto preserve = already_paired;
        auto is_goal = [edge, preserve](const NxNCube& candidate) {
            if (!edge_slot_paired(candidate, N4_EDGES[edge])) return false;
            for (int saved : preserve) {
                if (!edge_slot_paired(candidate, N4_EDGES[saved])) {
                    return false;
                }
            }
            return true;
        };

        std::vector<MoveStep> sequence;
        for (int depth : {2, 3, 5}) {
            BFSResult search =
                reduce_bfs(cube, is_goal, fnv1a_full, moves, depth);
            if (search.found) {
                sequence = std::move(search.sequence);
                break;
            }
        }
        if (sequence.empty()) return result;
        for (const auto& step : sequence) apply_move_step(cube, step);
        result.sequence.insert(
            result.sequence.end(), sequence.begin(), sequence.end());
        already_paired.push_back(edge);
        ++result.edges_paired;
    }
    return result;
}

ParityState detect_parity_n4(const NxNCube& cube) {
    assert(cube.n() == 4);
    return detect_parity_general(cube);
}

std::vector<MoveStep> fix_parity_n4(NxNCube& cube) {
    std::vector<MoveStep> out;
    const ParityState parity = detect_parity_n4(cube);
    if (parity == ParityState::OLL || parity == ParityState::Both) {
        apply_move_step(cube, oll_parity_alg());
        out.push_back(oll_parity_alg());
    }
    if (parity == ParityState::PLL || parity == ParityState::Both) {
        apply_move_step(cube, pll_parity_alg());
        out.push_back(pll_parity_alg());
    }
    return out;
}

}  // namespace cube_nxn::legacy_n4
