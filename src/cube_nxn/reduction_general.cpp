#include "cube_nxn/reduction_general.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <map>
#include <queue>
#include <unordered_set>

namespace cube_nxn {

namespace {

CenterFlavor classify_center(int dr, int dc) {
    const int adr = std::abs(dr);
    const int adc = std::abs(dc);
    if (dr == 0 && dc == 0) return CenterFlavor::Fixed;
    if (dr == 0 || dc == 0) return CenterFlavor::PlusCenter;
    if (adr == adc)         return CenterFlavor::XCenter;
    return CenterFlavor::Oblique;
}

int center_ring(int dr, int dc) {
    const int adr = std::abs(dr);
    const int adc = std::abs(dc);
    return (adr > adc) ? adr : adc;
}

}

std::vector<CenterSlot> enumerate_center_slots(int n) {
    assert(n >= 3);
    std::vector<CenterSlot> out;
    for (int f = 0; f < NUM_FACES; ++f) {
        for (int r = 1; r <= n - 2; ++r) {
            for (int c = 1; c <= n - 2; ++c) {
                const int dr = 2 * r - (n - 1);
                const int dc = 2 * c - (n - 1);
                out.push_back(CenterSlot{
                    static_cast<Face>(f), r, c,
                    classify_center(dr, dc), center_ring(dr, dc),
                });
            }
        }
    }
    return out;
}

std::vector<EdgeSlotG> enumerate_edge_slots(int n) {
    assert(n >= 3);
    std::vector<EdgeSlotG> out;
    const int wing_pairs = (n - 2) / 2;
    const bool has_middle = (n % 2 == 1);
    for (int edge_id = 0; edge_id < 12; ++edge_id) {
        for (int w = 0; w < wing_pairs; ++w) {
            for (int side = 0; side < 2; ++side) {
                out.push_back(EdgeSlotG{edge_id, EdgeFlavor::Wing, w, side});
            }
        }
        if (has_middle) {
            out.push_back(EdgeSlotG{edge_id, EdgeFlavor::Middle, -1, 0});
        }
    }
    return out;
}

namespace {

inline Move mk_outer(Face f, Turn t) { return Move{f, 0, 0, t}; }

inline Move mk_slice(Face f, int d, Turn t) {
    assert(d >= 1);
    return Move{f, d - 1, d - 1, t};
}

inline Move mk_wide(Face f, int d, Turn t) {
    assert(d >= 2);
    return Move{f, 0, d - 1, t};
}

inline Turn inverse_turn(Turn t) {
    if (t == Turn::CW)  return Turn::CCW;
    if (t == Turn::CCW) return Turn::CW;
    return Turn::Half;
}

inline Move inverse_move(const Move& m) {
    return Move{m.face, m.outer_depth, m.inner_depth, inverse_turn(m.turn)};
}

MoveStep inverse_step(const MoveStep& step) {
    MoveStep out;
    out.reserve(step.size());
    for (auto it = step.rbegin(); it != step.rend(); ++it) out.push_back(inverse_move(*it));
    return out;
}

// commutator templates
MoveStep xcenter_3cycle(int d) {
    return {
        mk_slice(Face::R, d, Turn::CCW),
        mk_outer(Face::D, Turn::CCW),
        mk_slice(Face::B, d, Turn::CW),
        mk_outer(Face::D, Turn::CW),
        mk_slice(Face::R, d, Turn::CW),
        mk_outer(Face::D, Turn::CCW),
        mk_slice(Face::B, d, Turn::CCW),
        mk_outer(Face::D, Turn::CW),
    };
}

MoveStep wing_3cycle(int d) {
    assert(d >= 2);
    auto wide_or_outer = [](int width, Turn t) -> Move {
        if (width == 1) return mk_outer(Face::R, t);
        return mk_wide(Face::R, width, t);
    };
    return {
        wide_or_outer(d - 1, Turn::CW),
        mk_wide(Face::R, d, Turn::CCW),
        mk_outer(Face::U, Turn::CW),
        mk_outer(Face::R, Turn::CW),
        mk_outer(Face::U, Turn::CCW),
        mk_wide(Face::R, d, Turn::CW),
        wide_or_outer(d - 1, Turn::CCW),
        mk_outer(Face::U, Turn::CW),
        mk_outer(Face::R, Turn::CCW),
        mk_outer(Face::U, Turn::CCW),
    };
}

MoveStep plus_center_3cycle(int d, int n) {
    assert(n % 2 == 1);
    const int e_depth = (n + 1) / 2;
    return {
        mk_outer(Face::U, Turn::CW),
        mk_slice(Face::R, d, Turn::CW),
        mk_slice(Face::D, e_depth, Turn::Half),
        mk_slice(Face::R, d, Turn::CCW),
        mk_outer(Face::U, Turn::CCW),
        mk_slice(Face::R, d, Turn::CW),
        mk_slice(Face::D, e_depth, Turn::Half),
        mk_slice(Face::R, d, Turn::CCW),
    };
}

// oblique 3-cycle commutator derivation, self-verifying and returns the first one that works for that exact input
static MoveStep find_clean_oblique_alg(int n, int a, int b, int chir) {
    auto is_target = [&](int dr, int dc) {
        if (classify_center(dr, dc) != CenterFlavor::Oblique) return false;
        int adr = std::abs(dr), adc = std::abs(dc);
        if (std::min(adr, adc) != a || std::max(adr, adc) != b) return false;
        return (dr * dc > 0 ? 1 : -1) == chir;
    };
    auto is_clean = [&](const MoveStep& alg) {
        NxNCube c(n);
        apply_move_step(c, alg);
        int target_moved = 0, other_moved = 0;
        for (int f = 0; f < NUM_FACES; ++f) {
            for (int r = 0; r < n; ++r) {
                for (int col = 0; col < n; ++col) {
                    if (c.sticker(f, r, col) == static_cast<uint8_t>(f)) continue;
                    const int dr = 2 * r - (n - 1), dc = 2 * col - (n - 1);
                    if (is_target(dr, dc)) ++target_moved;
                    else ++other_moved;
                }
            }
        }
        return other_moved == 0 && target_moved == 3;
    };

    const Face faces[6] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    const Turn qturns[2] = {Turn::CW, Turn::CCW};
    for (Face fa : faces) {
        for (int da = 1; da <= n; ++da) {
            for (Turn ta : qturns) {
                const Move A = mk_slice(fa, da, ta);
                const Move Ainv = mk_slice(fa, da, ta == Turn::CW ? Turn::CCW : Turn::CW);
                for (Face fb : faces) {
                    if (fb == fa) continue;
                    for (Turn tb : qturns) {
                        const Turn tbInv = (tb == Turn::CW) ? Turn::CCW : Turn::CW;
                        for (Face fc : faces) {
                            for (int dc_ = 1; dc_ <= n; ++dc_) {
                                for (Turn tc : qturns) {
                                    const Turn tcInv = (tc == Turn::CW) ? Turn::CCW : Turn::CW;
                                    MoveStep candidate = {
                                        A,
                                        mk_outer(fb, tb), mk_slice(fc, dc_, tc), mk_outer(fb, tbInv),
                                        Ainv,
                                        mk_outer(fb, tb), mk_slice(fc, dc_, tcInv), mk_outer(fb, tbInv),
                                    };
                                    if (is_clean(candidate)) return candidate;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return {};  // no clean candidate found in this family
}

MoveStep middle_edge_3cycle(int n) {
    assert(n % 2 == 1 && n >= 5);
    const int m_depth = (n + 1) / 2;
    return {
        mk_slice(Face::L, m_depth, Turn::Half),
        mk_outer(Face::U, Turn::CW),
        mk_slice(Face::L, m_depth, Turn::CW),
        mk_outer(Face::U, Turn::Half),
        mk_slice(Face::L, m_depth, Turn::CCW),
        mk_outer(Face::U, Turn::CW),
        mk_slice(Face::L, m_depth, Turn::Half),
    };
}

// flip exactly two middle-edges in place
MoveStep middle_edge_flip(int n) {
    assert(n % 2 == 1 && n >= 5);
    const int m = (n + 1) / 2;
    return {
        mk_slice(Face::R, m, Turn::CCW), mk_outer(Face::U, Turn::CW),
        mk_slice(Face::R, m, Turn::CCW), mk_outer(Face::U, Turn::CW),
        mk_slice(Face::R, m, Turn::CCW), mk_outer(Face::U, Turn::Half),
        mk_slice(Face::R, m, Turn::CW),  mk_outer(Face::U, Turn::CW),
        mk_slice(Face::R, m, Turn::CW),  mk_outer(Face::U, Turn::CW),
        mk_slice(Face::R, m, Turn::CW),  mk_outer(Face::U, Turn::Half),
    };
}

MoveStep oll_parity_general(int k) {
    const int d = k + 1;   // inner-slice depth
    return {
        mk_slice(Face::R, d, Turn::Half),
        mk_outer(Face::B, Turn::Half),
        mk_outer(Face::U, Turn::Half),
        mk_slice(Face::L, d, Turn::CW),
        mk_outer(Face::U, Turn::Half),
        mk_slice(Face::R, d, Turn::CCW),
        mk_outer(Face::U, Turn::Half),
        mk_slice(Face::R, d, Turn::CW),
        mk_outer(Face::U, Turn::Half),
        mk_outer(Face::F, Turn::Half),
        mk_slice(Face::R, d, Turn::CW),
        mk_outer(Face::F, Turn::Half),
        mk_slice(Face::L, d, Turn::CCW),
        mk_outer(Face::B, Turn::Half),
        mk_slice(Face::R, d, Turn::Half),
    };
}

std::vector<Move> setup_moves_for_n(int n) {
    std::vector<Move> moves;
    for (int f = 0; f < 6; ++f) {
        for (Turn t : {Turn::CW, Turn::Half, Turn::CCW}) {
            moves.push_back(mk_outer(static_cast<Face>(f), t));
        }
    }
    for (int f = 0; f < 6; ++f) {
        for (int d = 2; d < n; ++d) {
            for (Turn t : {Turn::CW, Turn::Half, Turn::CCW}) {
                moves.push_back(mk_slice(static_cast<Face>(f), d, t));
            }
        }
    }
    return moves;
}

// a position within an orbit packed into a single uint16
using Pos = uint16_t;
// encode (face, r, c) into a Pos
inline Pos encode_pos(int face, int r, int c) {
    return static_cast<Pos>((face << 12) | ((r & 0x3F) << 6) | (c & 0x3F));
}

inline void decode_pos(Pos p, int& face, int& r, int& c) {
    face = (p >> 12) & 0xF;
    r = (p >> 6) & 0x3F;
    c = p & 0x3F;
}

// maps each position in the orbit to an index
struct OrbitGraph {
    std::vector<Pos>   positions;
    std::vector<int>   pos_to_index;
    std::vector<Move>  moves;
    std::vector<std::vector<int>> perm;
    int  n_cube = 0;
};

// permutation + the concrete cube-move word realizing it
struct PermWord {
    std::vector<int> perm;
    MoveStep word;
};

std::vector<int> identity_perm(int n) {
    std::vector<int> p(n);
    for (int i = 0; i < n; ++i) p[i] = i;
    return p;
}

bool is_identity_perm(const std::vector<int>& p) {
    for (int i = 0; i < static_cast<int>(p.size()); ++i)
        if (p[i] != i) return false;
    return true;
}

std::vector<int> compose_perm(const std::vector<int>& a,
                              const std::vector<int>& b) {
    assert(a.size() == b.size());
    std::vector<int> out(a.size());
    for (int i = 0; i < static_cast<int>(a.size()); ++i) out[i] = b[a[i]];
    return out;
}

PermWord inverse_pw(const PermWord& a) {
    PermWord out;
    out.perm.resize(a.perm.size());
    for (int i = 0; i < static_cast<int>(a.perm.size()); ++i) out.perm[a.perm[i]] = i;
    out.word = inverse_step(a.word);
    return out;
}

OrbitGraph build_orbit_graph(int n, const std::function<bool(int,int,int)>& in_orbit,
                             const std::vector<Move>& move_set) {
    OrbitGraph g;
    g.n_cube = n;
    g.pos_to_index.assign(1 << 16, -1);
    g.moves = move_set;
    for (int f = 0; f < NUM_FACES; ++f) {
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) {
                if (in_orbit(f, r, c)) {
                    Pos p = encode_pos(f, r, c);
                    g.pos_to_index[p] = static_cast<int>(g.positions.size());
                    g.positions.push_back(p);
                }
            }
        }
    }
    const int M = static_cast<int>(move_set.size());
    g.perm.assign(g.positions.size(), std::vector<int>(M, -1));
    assert(g.positions.size() < 128);
    for (int m_idx = 0; m_idx < M; ++m_idx) {
        NxNCube marker(n);
        for (int i = 0; i < static_cast<int>(g.positions.size()); ++i) {
            int f, r, c; decode_pos(g.positions[i], f, r, c);
            marker.set_sticker(f, r, c, static_cast<uint8_t>(128 + i));
        }
        apply_move(marker, move_set[m_idx]);
        for (int dst_idx = 0; dst_idx < static_cast<int>(g.positions.size()); ++dst_idx) {
            int f, r, c; decode_pos(g.positions[dst_idx], f, r, c);
            uint8_t v = marker.sticker(f, r, c);
            if (v >= 128 && v < 128 + (int)g.positions.size()) {
                int src = v - 128;
                g.perm[src][m_idx] = dst_idx;
            }
        }
    }
    return g;
}


// discover the 3 action slots of commutator on a solved cube
struct ActionTriple { int source_idx; int target_idx; int buffer_idx; };

ActionTriple detect_action_triple(const OrbitGraph& g, const MoveStep& alg) {
    const int n = g.n_cube;
    NxNCube marker(n);
    assert(g.positions.size() < 128);
    for (int i = 0; i < static_cast<int>(g.positions.size()); ++i) {
        int f, r, c; decode_pos(g.positions[i], f, r, c);
        marker.set_sticker(f, r, c, static_cast<uint8_t>(128 + i));
    }
    for (const auto& m : alg) apply_move(marker, m);
    std::vector<int> perm_after(g.positions.size(), -1);
    for (int dst = 0; dst < static_cast<int>(g.positions.size()); ++dst) {
        int f, r, c; decode_pos(g.positions[dst], f, r, c);
        uint8_t v = marker.sticker(f, r, c);
        if (v >= 128 && v < 128 + (int)g.positions.size()) perm_after[dst] = v - 128;
    }
    ActionTriple t{-1, -1, -1};
    for (int i = 0; i < static_cast<int>(g.positions.size()); ++i) {
        if (perm_after[i] == -1 || perm_after[i] == i) continue;
        int a = i;
        int b = -1, c = -1;
        for (int j = 0; j < static_cast<int>(g.positions.size()); ++j) {
            if (perm_after[j] == a) { b = j; break; }
        }
        if (b < 0 || b == a) continue;
        for (int j = 0; j < static_cast<int>(g.positions.size()); ++j) {
            if (perm_after[j] == b) { c = j; break; }
        }
        if (c < 0 || c == a || c == b) continue;
        if (perm_after[a] != c) continue;
        t = {a, b, c};
        break;
    }
    return t;
}

// home face for a position in an orbit (the face on which its color matches)
inline int home_face(const OrbitGraph& g, int pos_idx) {
    int f, r, c; decode_pos(g.positions[pos_idx], f, r, c);
    (void)r; (void)c;
    return f;
}

// read the color currently at orbit position pos_idx
inline uint8_t read_color(const NxNCube& cube, const OrbitGraph& g, int pos_idx) {
    int f, r, c; decode_pos(g.positions[pos_idx], f, r, c);
    return cube.sticker(f, r, c);
}

struct DisjointSet {
    std::vector<int> parent, rank;
    explicit DisjointSet(int n) : parent(n), rank(n, 0) {
        for (int i = 0; i < n; ++i) parent[i] = i;
    }
    int find(int x) { return parent[x] == x ? x : parent[x] = find(parent[x]); }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (rank[a] < rank[b]) std::swap(a, b);
        parent[b] = a;
        if (rank[a] == rank[b]) ++rank[a];
        return true;
    }
};

std::vector<PermWord> build_connected_conjugate_generators(
        const OrbitGraph& g, const MoveStep& alg, const ActionTriple& slots) {
    const int K = static_cast<int>(g.positions.size());
    const int64_t state_count = static_cast<int64_t>(K) * K * K;
    auto encode = [K](int a, int b, int c) {
        return static_cast<int64_t>(a) * K * K + static_cast<int64_t>(b) * K + c;
    };
    auto decode = [K](int64_t s, int& a, int& b, int& c) {
        c = static_cast<int>(s % K);
        b = static_cast<int>((s / K) % K);
        a = static_cast<int>(s / (K * K));
    };

    const int64_t start = encode(slots.source_idx, slots.target_idx, slots.buffer_idx);
    std::vector<int64_t> parent(state_count, -1);
    std::vector<int16_t> parent_move(state_count, -1);
    std::vector<uint8_t> visited(state_count, 0);
    std::queue<int64_t> q;
    q.push(start);
    visited[start] = 1;

    DisjointSet dsu(K);
    int components = K;
    std::vector<PermWord> generators;

    auto maybe_add = [&](int64_t state) {
        int a, b, c; decode(state, a, b, c);
        bool joined = false;
        if (dsu.unite(a, b)) { --components; joined = true; }
        if (dsu.unite(b, c)) { --components; joined = true; }
        if (!joined && !generators.empty()) return;

        MoveStep setup_from_base;
        for (int64_t s = state; s != start; s = parent[s])
            setup_from_base.push_back(g.moves[parent_move[s]]);
        std::reverse(setup_from_base.begin(), setup_from_base.end());

        MoveStep word = inverse_step(setup_from_base);
        word.insert(word.end(), alg.begin(), alg.end());
        word.insert(word.end(), setup_from_base.begin(), setup_from_base.end());
        std::vector<int> perm = identity_perm(K);
        perm[a] = b; perm[b] = c; perm[c] = a;
        generators.push_back(PermWord{std::move(perm), std::move(word)});
    };

    maybe_add(start);
    while (!q.empty() && components > 1) {
        const int64_t cur = q.front(); q.pop();
        int a, b, c; decode(cur, a, b, c);
        for (int m = 0; m < static_cast<int>(g.moves.size()); ++m) {
            const int na = g.perm[a][m], nb = g.perm[b][m], nc = g.perm[c][m];
            if (na < 0 || nb < 0 || nc < 0) continue;
            const int64_t ns = encode(na, nb, nc);
            if (visited[ns]) continue;
            visited[ns] = 1;
            parent[ns] = cur;
            parent_move[ns] = static_cast<int16_t>(m);
            q.push(ns);
            maybe_add(ns);
        }
    }
    if (components != 1) {
        if (std::getenv("CUBE_DEBUG_EXACT_CENTERS"))
            std::fprintf(stderr, "[exact-centers] K=%d conjugate hypergraph components=%d\n",
                         K, components);
        return {};
    }
    return generators;
}

bool permutation_is_even(const std::vector<int>& p) {
    int inversions = 0;
    for (int i = 0; i < static_cast<int>(p.size()); ++i)
        for (int j = i + 1; j < static_cast<int>(p.size()); ++j)
            if (p[i] > p[j]) inversions ^= 1;
    return inversions == 0;
}

std::array<int,3> triple_from_3cycle(const std::vector<int>& p) {
    for (int a = 0; a < static_cast<int>(p.size()); ++a) {
        if (p[a] == a) continue;
        const int b = p[a], c = p[b];
        if (a != b && b != c && c != a && p[c] == a)
            return {a, b, c};
    }
    return {-1, -1, -1};
}

std::vector<int> make_3cycle_perm(int K, int a, int b, int c) {
    std::vector<int> p = identity_perm(K);
    p[a] = b; p[b] = c; p[c] = a;
    return p;
}

// decompose an arbitrary even permutation into oriented 3-cycles
bool decompose_even_perm(const std::vector<int>& target,
                         std::vector<std::array<int,3>>& cycles_out) {
    if (!permutation_is_even(target)) return false;
    const int K = static_cast<int>(target.size());
    std::vector<int> rem = target;
    std::vector<std::vector<int>> eliminators;

    for (;;) {
        int a = -1;
        for (int i = 0; i < K; ++i) {
            if (rem[i] != i && rem[rem[i]] != i) { a = i; break; }
        }
        if (a < 0) break;
        const int b = rem[a], c = rem[b];
        auto op = make_3cycle_perm(K, b, a, c);
        rem = compose_perm(rem, op);
        eliminators.push_back(std::move(op));
    }

    std::vector<std::pair<int,int>> swaps;
    std::vector<uint8_t> used(K, 0);
    for (int i = 0; i < K; ++i) {
        if (rem[i] == i || used[i]) continue;
        const int j = rem[i];
        if (j < 0 || j >= K || rem[j] != i) return false;
        used[i] = used[j] = 1;
        swaps.emplace_back(i, j);
    }
    if (swaps.size() % 2 != 0) return false;

    for (size_t s = 0; s < swaps.size(); s += 2) {
        const int v[4] = {swaps[s].first, swaps[s].second,
                          swaps[s+1].first, swaps[s+1].second};
        std::vector<int> pair_perm = identity_perm(K);
        pair_perm[v[0]] = v[1]; pair_perm[v[1]] = v[0];
        pair_perm[v[2]] = v[3]; pair_perm[v[3]] = v[2];
        std::vector<std::vector<int>> candidates;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) if (j != i)
                for (int k = 0; k < 4; ++k) if (k != i && k != j)
                    candidates.push_back(make_3cycle_perm(K, v[i], v[j], v[k]));
        bool found = false;
        for (const auto& a : candidates) {
            for (const auto& b : candidates) {
                if (is_identity_perm(compose_perm(compose_perm(pair_perm, a), b))) {
                    rem = compose_perm(compose_perm(rem, a), b);
                    eliminators.push_back(a);
                    eliminators.push_back(b);
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (!found) return false;
    }
    if (!is_identity_perm(rem)) return false;

    cycles_out.clear();
    for (auto it = eliminators.rbegin(); it != eliminators.rend(); ++it) {
        auto inv = inverse_pw(PermWord{*it, {}}).perm;
        auto triple = triple_from_3cycle(inv);
        if (triple[0] < 0) return false;
        cycles_out.push_back(triple);
    }

    std::vector<int> check = identity_perm(K);
    for (const auto& t : cycles_out)
        check = compose_perm(check, make_3cycle_perm(K, t[0], t[1], t[2]));
    return check == target;
}

// turn the abstract 3-cycle decomposition into cube moves
bool realize_3cycles(const std::vector<PermWord>& generators,
                     const std::vector<std::array<int,3>>& requested,
                     MoveStep& solution_out) {
    if (generators.empty()) return false;
    const int K = static_cast<int>(generators.front().perm.size());
    std::vector<PermWord> alpha;
    alpha.reserve(generators.size() * 2);
    for (const auto& g : generators) {
        alpha.push_back(g);
        alpha.push_back(inverse_pw(g));
    }
    const auto base = triple_from_3cycle(generators.front().perm);
    if (base[0] < 0) return false;
    auto encode = [K](int a, int b, int c) {
        return static_cast<int64_t>(a) * K * K + static_cast<int64_t>(b) * K + c;
    };
    auto decode = [K](int64_t s, int& a, int& b, int& c) {
        c = static_cast<int>(s % K);
        b = static_cast<int>((s / K) % K);
        a = static_cast<int>(s / (K * K));
    };
    const int64_t count = static_cast<int64_t>(K) * K * K;
    const int64_t start = encode(base[0], base[1], base[2]);
    std::vector<int64_t> parent(count, -1);
    std::vector<int16_t> parent_gen(count, -1);
    std::vector<uint8_t> visited(count, 0), needed(count, 0);
    int remaining = 0;
    for (const auto& t : requested) {
        const int64_t s = encode(t[0], t[1], t[2]);
        if (!needed[s]) { needed[s] = 1; ++remaining; }
    }
    std::queue<int64_t> q;
    q.push(start); visited[start] = 1;
    if (needed[start]) { needed[start] = 0; --remaining; }
    while (!q.empty() && remaining > 0) {
        const int64_t cur = q.front(); q.pop();
        int a, b, c; decode(cur, a, b, c);
        for (int gi = 0; gi < static_cast<int>(alpha.size()); ++gi) {
            const auto& p = alpha[gi].perm;
            const int64_t ns = encode(p[a], p[b], p[c]);
            if (visited[ns]) continue;
            visited[ns] = 1;
            parent[ns] = cur;
            parent_gen[ns] = static_cast<int16_t>(gi);
            q.push(ns);
            if (needed[ns]) { needed[ns] = 0; --remaining; }
        }
    }
    if (remaining != 0) return false;

    solution_out.clear();
    for (const auto& t : requested) {
        const int64_t goal = encode(t[0], t[1], t[2]);
        std::vector<int> path;
        for (int64_t s = goal; s != start; s = parent[s]) {
            if (s < 0 || parent[s] < 0) return false;
            path.push_back(parent_gen[s]);
        }
        std::reverse(path.begin(), path.end());
        MoveStep conjugator;
        for (int gi : path)
            conjugator.insert(conjugator.end(), alpha[gi].word.begin(), alpha[gi].word.end());
        MoveStep inv = inverse_step(conjugator);
        solution_out.insert(solution_out.end(), inv.begin(), inv.end());
        solution_out.insert(solution_out.end(), generators.front().word.begin(),
                            generators.front().word.end());
        solution_out.insert(solution_out.end(), conjugator.begin(), conjugator.end());
    }
    return true;
}

// complete center-orbit solver
// note that same-colored centers are interchangeable, so if
// arbitrary assignment is odd, we will swap the homes of two equal-colored
// stickers to resolve parity if needed, without affecting the solve
bool solve_center_orbit_exact(NxNCube& cube, std::vector<MoveStep>& out_seq,
                              const OrbitGraph& g, const MoveStep& alg) {
    const int K = static_cast<int>(g.positions.size());
    if (K == 0) return true;
    const ActionTriple slots = detect_action_triple(g, alg);
    if (slots.source_idx < 0) return false;

    std::array<std::vector<int>, NUM_FACES> current_by_color;
    std::array<std::vector<int>, NUM_FACES> homes_by_color;
    for (int i = 0; i < K; ++i) {
        const int color = static_cast<int>(read_color(cube, g, i));
        if (color < 0 || color >= NUM_FACES) return false;
        current_by_color[color].push_back(i);
        homes_by_color[home_face(g, i)].push_back(i);
    }

    std::vector<int> target = identity_perm(K);  // current position -> assigned home
    for (int color = 0; color < NUM_FACES; ++color) {
        if (current_by_color[color].size() != homes_by_color[color].size()) return false;
        for (int j = 0; j < static_cast<int>(current_by_color[color].size()); ++j)
            target[current_by_color[color][j]] = homes_by_color[color][j];
    }
    if (!permutation_is_even(target)) {
        bool fixed = false;
        for (int color = 0; color < NUM_FACES && !fixed; ++color) {
            if (current_by_color[color].size() < 2) continue;
            std::swap(target[current_by_color[color][0]], target[current_by_color[color][1]]);
            fixed = true;
        }
        if (!fixed || !permutation_is_even(target)) return false;
    }

    const auto generators = build_connected_conjugate_generators(g, alg, slots);
    if (generators.empty()) return false;
    std::vector<std::array<int,3>> cycles;
    if (!decompose_even_perm(target, cycles)) return false;
    MoveStep solution;
    if (!realize_3cycles(generators, cycles, solution)) return false;

    NxNCube trial = cube;
    apply_move_step(trial, solution);
    for (int i = 0; i < K; ++i)
        if (read_color(trial, g, i) != static_cast<uint8_t>(home_face(g, i))) return false;
    cube = std::move(trial);
    if (!solution.empty()) out_seq.push_back(std::move(solution));
    return true;
}

std::vector<int> conjugate_component_vertices(const OrbitGraph& g,
                                               const ActionTriple& slots) {
    const int K = static_cast<int>(g.positions.size());
    auto encode = [K](int a, int b, int c) {
        return static_cast<int64_t>(a) * K * K + static_cast<int64_t>(b) * K + c;
    };
    auto decode = [K](int64_t s, int& a, int& b, int& c) {
        c = static_cast<int>(s % K);
        b = static_cast<int>((s / K) % K);
        a = static_cast<int>(s / (K * K));
    };
    const int64_t count = static_cast<int64_t>(K) * K * K;
    const int64_t start = encode(slots.source_idx, slots.target_idx, slots.buffer_idx);
    std::vector<uint8_t> visited(count, 0), vertex_seen(K, 0);
    std::queue<int64_t> q;
    visited[start] = 1; q.push(start);
    while (!q.empty()) {
        const int64_t cur = q.front(); q.pop();
        int a, b, c; decode(cur, a, b, c);
        vertex_seen[a] = vertex_seen[b] = vertex_seen[c] = 1;
        for (int m = 0; m < static_cast<int>(g.moves.size()); ++m) {
            const int na = g.perm[a][m], nb = g.perm[b][m], nc = g.perm[c][m];
            if (na < 0 || nb < 0 || nc < 0) continue;
            const int64_t ns = encode(na, nb, nc);
            if (!visited[ns]) { visited[ns] = 1; q.push(ns); }
        }
    }
    std::vector<int> out;
    for (int i = 0; i < K; ++i) if (vertex_seen[i]) out.push_back(i);
    return out;
}

OrbitGraph induced_orbit_graph(const OrbitGraph& full, const std::vector<int>& vertices) {
    OrbitGraph sub;
    sub.n_cube = full.n_cube;
    sub.moves = full.moves;
    sub.pos_to_index.assign(1 << 16, -1);
    std::vector<int> old_to_new(full.positions.size(), -1);
    for (int old : vertices) {
        const int ni = static_cast<int>(sub.positions.size());
        old_to_new[old] = ni;
        sub.positions.push_back(full.positions[old]);
        sub.pos_to_index[full.positions[old]] = ni;
    }
    sub.perm.assign(vertices.size(), std::vector<int>(sub.moves.size(), -1));
    for (int ni = 0; ni < static_cast<int>(vertices.size()); ++ni) {
        const int old = vertices[ni];
        for (int m = 0; m < static_cast<int>(sub.moves.size()); ++m) {
            const int old_dst = full.perm[old][m];
            if (old_dst >= 0) sub.perm[ni][m] = old_to_new[old_dst];
        }
    }
    return sub;
}


}

std::vector<MoveStep> solve_centers_general(NxNCube& cube) {
    const int n = cube.n();
    assert(n >= 3);
    std::vector<MoveStep> out;
    if (n == 3) return out;

    // max slice depth for X-center rings
    const int max_regular_d = (n % 2 == 1) ? (n - 1) / 2 : n / 2;

    // x-centers, ring-by-ring (smallest ring first for smaller orbit size)
    for (int d = max_regular_d; d >= 2; --d) {
        const int ring = n + 1 - 2 * d;
        auto in_orbit = [ring, n](int, int r, int c) {
            const int dr = 2 * r - (n - 1);
            const int dc = 2 * c - (n - 1);
            if (classify_center(dr, dc) != CenterFlavor::XCenter) return false;
            return center_ring(dr, dc) == ring;
        };
        OrbitGraph g = build_orbit_graph(n, in_orbit, setup_moves_for_n(n));
        if (!solve_center_orbit_exact(cube, out, g, xcenter_3cycle(d))) return out;
    }
    // plus-centers (odd N)
    if (n % 2 == 1) {
        for (int d = max_regular_d; d >= 2; --d) {
            const int ring = n + 1 - 2 * d;
            auto in_orbit = [ring, n](int, int r, int c) {
                const int dr = 2 * r - (n - 1);
                const int dc = 2 * c - (n - 1);
                if (classify_center(dr, dc) != CenterFlavor::PlusCenter) return false;
                return center_ring(dr, dc) == ring;
            };
            OrbitGraph g = build_orbit_graph(n, in_orbit, setup_moves_for_n(n));
            if (!solve_center_orbit_exact(cube, out, g, plus_center_3cycle(d, n))) return out;
        }
    }

    // true obliques may split into independent components even when their
    // coordinate taxonomy puts them in one ring; discover those components
    // from the conjugate action and solve each one independently
    if (n >= 6) {
        std::vector<int> oblique_rings;
        for (int r = 1; r <= n - 2; ++r) {
            for (int c = 1; c <= n - 2; ++c) {
                const int dr = 2 * r - (n - 1);
                const int dc = 2 * c - (n - 1);
                if (classify_center(dr, dc) != CenterFlavor::Oblique) continue;
                int ring_key = std::abs(dr) * 100 + std::abs(dc);
                if (std::abs(dr) > std::abs(dc)) ring_key = std::abs(dc) * 100 + std::abs(dr);
                if (std::find(oblique_rings.begin(), oblique_rings.end(), ring_key) == oblique_rings.end()) {
                    oblique_rings.push_back(ring_key);
                }
            }
        }
        for (int ring_key : oblique_rings) {
            int a = ring_key / 100;
            int b = ring_key % 100;
            auto in_orbit = [n, a, b](int, int r, int c) {
                const int dr = 2 * r - (n - 1);
                const int dc = 2 * c - (n - 1);
                if (classify_center(dr, dc) != CenterFlavor::Oblique) return false;
                int adr = std::abs(dr), adc = std::abs(dc);
                return std::min(adr, adc) == a && std::max(adr, adc) == b;
            };
            OrbitGraph full = build_orbit_graph(n, in_orbit, setup_moves_for_n(n));
            
            std::vector<uint8_t> covered(full.positions.size(), 0);
            std::unordered_set<std::string> component_keys;
            for (int chir : {1, -1}) {
                MoveStep alg = find_clean_oblique_alg(n, a, b, chir);
                if (alg.empty()) continue;
                ActionTriple action = detect_action_triple(full, alg);
                if (action.source_idx < 0) continue;
                std::vector<int> vertices = conjugate_component_vertices(full, action);
                std::sort(vertices.begin(), vertices.end());
                std::string key;
                for (int v : vertices) key.push_back(static_cast<char>(v));
                if (!component_keys.insert(key).second) continue;
                OrbitGraph component = induced_orbit_graph(full, vertices);
                if (!solve_center_orbit_exact(cube, out, component, alg)) return out;
                for (int v : vertices) covered[v] = 1;
            }
            if (std::find(covered.begin(), covered.end(), 0) != covered.end()) return out;
        }
    }

    return out;
}

bool centers_reduced_general(const NxNCube& cube) {
    const int n = cube.n();
    for (int f = 0; f < NUM_FACES; ++f)
        for (int r = 1; r < n - 1; ++r)
            for (int c = 1; c < n - 1; ++c)
                if (cube.sticker(f, r, c) != static_cast<uint8_t>(f)) return false;
    return true;
}

// edge geometry for general N

struct EdgeSticker { Face face; int row; int col; };
struct EdgeSlotGeom {
    Face face_a, face_b;
    std::function<EdgeSticker(int t, int n)> stickers_a;
    std::function<EdgeSticker(int t, int n)> stickers_b;
};

std::vector<EdgeSlotGeom> build_edge_geom() {
    std::vector<EdgeSlotGeom> out(12);
    out[0] = {Face::U, Face::F,
              [](int t, int n){ return EdgeSticker{Face::U, n-1, t}; },
              [](int t, int){ return EdgeSticker{Face::F, 0,   t}; }};
    out[1] = {Face::U, Face::R,
              [](int t, int n){ return EdgeSticker{Face::U, n-1-t, n-1}; },
              [](int t, int){ return EdgeSticker{Face::R, 0, t}; }};
    out[2] = {Face::U, Face::B,
              [](int t, int n){ return EdgeSticker{Face::U, 0, n-1-t}; },
              [](int t, int){ return EdgeSticker{Face::B, 0, t}; }};
    out[3] = {Face::U, Face::L,
              [](int t, int){ return EdgeSticker{Face::U, t, 0}; },
              [](int t, int){ return EdgeSticker{Face::L, 0, t}; }};
    out[4] = {Face::D, Face::F,
              [](int t, int){ return EdgeSticker{Face::D, 0, t}; },
              [](int t, int n){ return EdgeSticker{Face::F, n-1, t}; }};
    out[5] = {Face::D, Face::R,
              [](int t, int n){ return EdgeSticker{Face::D, t, n-1}; },
              [](int t, int n){ return EdgeSticker{Face::R, n-1, t}; }};
    out[6] = {Face::D, Face::B,
              [](int t, int n){ return EdgeSticker{Face::D, n-1, n-1-t}; },
              [](int t, int n){ return EdgeSticker{Face::B, n-1, t}; }};
    out[7] = {Face::D, Face::L,
              [](int t, int n){ return EdgeSticker{Face::D, n-1-t, 0}; },
              [](int t, int n){ return EdgeSticker{Face::L, n-1, t}; }};
    out[8] = {Face::F, Face::R,
              [](int t, int n){ return EdgeSticker{Face::F, t, n-1}; },
              [](int t, int){ return EdgeSticker{Face::R, t, 0}; }};
    out[9] = {Face::R, Face::B,
              [](int t, int n){ return EdgeSticker{Face::R, t, n-1}; },
              [](int t, int){ return EdgeSticker{Face::B, t, 0}; }};
    out[10] = {Face::B, Face::L,
              [](int t, int n){ return EdgeSticker{Face::B, t, n-1}; },
              [](int t, int){ return EdgeSticker{Face::L, t, 0}; }};
    out[11] = {Face::L, Face::F,
              [](int t, int n){ return EdgeSticker{Face::L, t, n-1}; },
              [](int t, int){ return EdgeSticker{Face::F, t, 0}; }};
    return out;
}
inline std::pair<int,int> wing_ts(int w, int n) {
    return {w + 1, n - 2 - w};
}

}
