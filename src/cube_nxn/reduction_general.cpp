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

}
}
