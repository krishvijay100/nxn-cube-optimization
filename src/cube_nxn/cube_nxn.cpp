#include "cube_nxn/cube_nxn.h"

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace cube_nxn {

NxNCube::NxNCube(int n) : n_(n), stickers_(NUM_FACES * n * n) {
    assert(n >= 2 && "n×n cube requires n >= 2");

    const int fs = n * n;
    for (int f = 0; f < NUM_FACES; ++f) {
        for (int i = 0; i < fs; ++i) {
            stickers_[f * fs + i] = static_cast<uint8_t>(f);
        }
    }
}

uint8_t NxNCube::sticker(int face, int row, int col) const {
    return stickers_[face * n_ * n_ + row * n_ + col];
}

void NxNCube::set_sticker(int face, int row, int col, uint8_t color) {
    stickers_[face * n_ * n_ + row * n_ + col] = color;
}

uint8_t* NxNCube::face_data(int face) {
    return stickers_.data() + face * n_ * n_;
}

const uint8_t* NxNCube::face_data(int face) const {
    return stickers_.data() + face * n_ * n_;
}

bool NxNCube::is_solved() const {
    const int fs = n_ * n_;
    for (int f = 0; f < NUM_FACES; ++f) {
        const uint8_t expected = static_cast<uint8_t>(f);
        for (int i = 0; i < fs; ++i) {
            if (stickers_[f * fs + i] != expected) return false;
        }
    }
    return true;
}

namespace {

enum class Dir : uint8_t {
    RowLR,   // row `line`, read left-to-right
    RowRL,   // row `line`, read right-to-left
    ColTB,   // column `line`, read top-to-bottom
    ColBT,   // column `line`, read bottom-to-top
};

enum class EdgeSide : uint8_t { FromFirst, FromLast };

struct Strip {
    Face face;
    Dir dir;
    EdgeSide side;
};

inline int strip_line(EdgeSide side, int depth, int n) {
    return (side == EdgeSide::FromFirst) ? depth : n - 1 - depth;
}

// read the n stickers of `s` at the given depth into `out` in the direction dictated by s.dir
inline void read_strip(const NxNCube& cube, const Strip& s, int depth, uint8_t* out) {
    const int n = cube.n();
    const int line = strip_line(s.side, depth, n);
    const uint8_t* fdata = cube.face_data(static_cast<int>(s.face));
    switch (s.dir) {
        case Dir::RowLR: for (int i = 0; i < n; ++i) out[i] = fdata[line * n + i];         break;
        case Dir::RowRL: for (int i = 0; i < n; ++i) out[i] = fdata[line * n + (n - 1 - i)]; break;
        case Dir::ColTB: for (int i = 0; i < n; ++i) out[i] = fdata[i * n + line];         break;
        case Dir::ColBT: for (int i = 0; i < n; ++i) out[i] = fdata[(n - 1 - i) * n + line]; break;
    }
}

// write n stickers from `in` into `s` at the given depth in the direction dictated by s.dir
inline void write_strip(NxNCube& cube, const Strip& s, int depth, const uint8_t* in) {
    const int n = cube.n();
    const int line = strip_line(s.side, depth, n);
    uint8_t* fdata = cube.face_data(static_cast<int>(s.face));
    switch (s.dir) {
        case Dir::RowLR: for (int i = 0; i < n; ++i) fdata[line * n + i]         = in[i]; break;
        case Dir::RowRL: for (int i = 0; i < n; ++i) fdata[line * n + (n - 1 - i)] = in[i]; break;
        case Dir::ColTB: for (int i = 0; i < n; ++i) fdata[i * n + line]         = in[i]; break;
        case Dir::ColBT: for (int i = 0; i < n; ++i) fdata[(n - 1 - i) * n + line] = in[i]; break;
    }
}

struct OuterMove {
    Strip strips[4];
};

constexpr OuterMove U_MOVE = {{
    {Face::F, Dir::RowLR, EdgeSide::FromFirst},
    {Face::L, Dir::RowLR, EdgeSide::FromFirst},
    {Face::B, Dir::RowLR, EdgeSide::FromFirst},
    {Face::R, Dir::RowLR, EdgeSide::FromFirst},
}};

constexpr OuterMove R_MOVE = {{
    {Face::F, Dir::ColTB, EdgeSide::FromLast},
    {Face::U, Dir::ColTB, EdgeSide::FromLast},
    {Face::B, Dir::ColBT, EdgeSide::FromFirst},
    {Face::D, Dir::ColTB, EdgeSide::FromLast},
}};

constexpr OuterMove F_MOVE = {{
    {Face::U, Dir::RowLR, EdgeSide::FromLast},
    {Face::R, Dir::ColTB, EdgeSide::FromFirst},
    {Face::D, Dir::RowRL, EdgeSide::FromFirst},
    {Face::L, Dir::ColBT, EdgeSide::FromLast},
}};

constexpr OuterMove D_MOVE = {{
    {Face::F, Dir::RowLR, EdgeSide::FromLast},
    {Face::R, Dir::RowLR, EdgeSide::FromLast},
    {Face::B, Dir::RowLR, EdgeSide::FromLast},
    {Face::L, Dir::RowLR, EdgeSide::FromLast},
}};

constexpr OuterMove L_MOVE = {{
    {Face::F, Dir::ColTB, EdgeSide::FromFirst},
    {Face::D, Dir::ColTB, EdgeSide::FromFirst},
    {Face::B, Dir::ColBT, EdgeSide::FromLast},
    {Face::U, Dir::ColTB, EdgeSide::FromFirst},
}};

constexpr OuterMove B_MOVE = {{
    {Face::U, Dir::RowRL, EdgeSide::FromFirst},
    {Face::L, Dir::ColTB, EdgeSide::FromFirst},
    {Face::D, Dir::RowLR, EdgeSide::FromLast},
    {Face::R, Dir::ColBT, EdgeSide::FromLast},
}};

inline const OuterMove& outer_table(Face f) {
    switch (f) {
        case Face::U: return U_MOVE;
        case Face::R: return R_MOVE;
        case Face::F: return F_MOVE;
        case Face::D: return D_MOVE;
        case Face::L: return L_MOVE;
        case Face::B: return B_MOVE;
    }
    return U_MOVE;   // unreachable
}

void cycle_strips_once(NxNCube& cube, const OuterMove& m, int depth) {
    const int n = cube.n();
    std::vector<uint8_t> a(n), b(n), c(n), d(n);
    read_strip(cube, m.strips[0], depth, a.data());
    read_strip(cube, m.strips[1], depth, b.data());
    read_strip(cube, m.strips[2], depth, c.data());
    read_strip(cube, m.strips[3], depth, d.data());
    write_strip(cube, m.strips[1], depth, a.data());
    write_strip(cube, m.strips[2], depth, b.data());
    write_strip(cube, m.strips[3], depth, c.data());
    write_strip(cube, m.strips[0], depth, d.data());
}

}

void apply_wide_move(NxNCube& cube, Face f, int outer_depth, int inner_depth, Turn t) {
    assert(0 <= outer_depth && outer_depth <= inner_depth);
    assert(inner_depth < cube.n());

    // rotate the outer face iff depth 0 is included in the range
    if (outer_depth == 0) {
        rotate_face(cube, static_cast<int>(f), t);
    }

    const OuterMove& m = outer_table(f);
    const int shifts = (t == Turn::CW) ? 1 : (t == Turn::Half) ? 2 : 3;
    for (int depth = outer_depth; depth <= inner_depth; ++depth) {
        for (int i = 0; i < shifts; ++i) cycle_strips_once(cube, m, depth);
    }
}

void apply_outer_move(NxNCube& cube, Face f, Turn t) {
    apply_wide_move(cube, f, 0, 0, t);
}

// write the rotated stickers into a temp array using turn formula then copy back
void rotate_face(NxNCube& cube, int face, Turn t) {
    const int n = cube.n();
    uint8_t* src = cube.face_data(face);
    std::vector<uint8_t> scratch(n * n);

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            int nr, nc;
            switch (t) {
                case Turn::CW:   nr = c;         nc = n - 1 - r; break;
                case Turn::CCW:  nr = n - 1 - c; nc = r;         break;
                case Turn::Half: nr = n - 1 - r; nc = n - 1 - c; break;
            }
            scratch[nr * n + nc] = src[r * n + c];
        }
    }

    for (int i = 0; i < n * n; ++i) src[i] = scratch[i];
}

void apply_move(NxNCube& cube, const Move& m) {
    apply_wide_move(cube, m.face, m.outer_depth, m.inner_depth, m.turn);
}

namespace {

std::optional<Face> parse_face(char c) {
    switch (c) {
        case 'U': return Face::U;
        case 'R': return Face::R;
        case 'F': return Face::F;
        case 'D': return Face::D;
        case 'L': return Face::L;
        case 'B': return Face::B;
    }
    return std::nullopt;
}

char face_char(Face f) {
    switch (f) {
        case Face::U: return 'U';
        case Face::R: return 'R';
        case Face::F: return 'F';
        case Face::D: return 'D';
        case Face::L: return 'L';
        case Face::B: return 'B';
    }
    return '?';
}

}

// grammar: [digits] face ['w'] ['2' | '\'']
// - no digits + no w: single outer layer  (outer=0, inner=0)
// - no digits + w:    2-layer wide         (outer=0, inner=1)
// - digits N + w:     N-layer wide         (outer=0, inner=N-1)
// - digits N + no w:  single inner slice at depth N-1  (outer=N-1, inner=N-1)
std::optional<Move> parse_move(std::string_view s) {
    if (s.empty()) return std::nullopt;

    size_t i = 0;

    // optional leading digits (depth prefix)
    int depth_prefix = 0;
    bool has_depth_prefix = false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        depth_prefix = depth_prefix * 10 + (s[i] - '0');
        has_depth_prefix = true;
        ++i;
    }

    // face letter
    if (i >= s.size()) return std::nullopt;
    auto f = parse_face(s[i]);
    if (!f) return std::nullopt;
    ++i;

    // optional wide marker
    bool wide = false;
    if (i < s.size() && s[i] == 'w') {
        wide = true;
        ++i;
    }

    // optional turn modifier
    Turn turn = Turn::CW;
    if (i < s.size()) {
        if (s[i] == '\'') { turn = Turn::CCW;  ++i; }
        else if (s[i] == '2') { turn = Turn::Half; ++i; }
        else return std::nullopt;
    }

    // trailing junk
    if (i != s.size()) return std::nullopt;

    // resolve to outer/inner depths per grammar rules
    Move m{*f, 0, 0, turn};
    if (wide) {
        // "Fw" == 2 layers by convention; "NFw" == N layers
        int layers = has_depth_prefix ? depth_prefix : 2;
        if (layers < 2) return std::nullopt;
        m.outer_depth = 0;
        m.inner_depth = layers - 1;
    } else if (has_depth_prefix) {
        // "NF" (digit, no w) == single inner slice at depth N-1
        if (depth_prefix < 2) return std::nullopt;
        m.outer_depth = depth_prefix - 1;
        m.inner_depth = depth_prefix - 1;
    }
    // else: bare face letter, m stays at outer=0, inner=0
    return m;
}

std::optional<std::vector<Move>> parse_scramble(std::string_view s) {
    std::vector<Move> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= s.size()) break;
        size_t start = i;
        while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        auto m = parse_move(s.substr(start, i - start));
        if (!m) return std::nullopt;
        out.push_back(*m);
    }
    return out;
}

std::string format_move(const Move& m) {
    std::string out;
    const bool wide      = m.outer_depth == 0 && m.inner_depth > 0;
    const bool inner_one = m.outer_depth > 0 && m.outer_depth == m.inner_depth;

    if (wide) {
        if (m.inner_depth > 1) out += std::to_string(m.inner_depth + 1);
    } else if (inner_one) {
        out += std::to_string(m.outer_depth + 1);
    }
    out += face_char(m.face);
    if (wide) out += 'w';
    if (m.turn == Turn::CCW)      out += '\'';
    else if (m.turn == Turn::Half) out += '2';
    return out;
}

namespace {

// splitmix64: tiny stateless PRNG
inline uint64_t splitmix64(uint64_t& state) {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

}

std::vector<Move> random_scramble(int n, int length, uint64_t seed) {
    assert(n >= 2 && length >= 0);
    std::vector<Move> out;
    out.reserve(length);

    const int max_extra_depth = (n >= 4) ? (n / 2 - 1) : 0;

    uint64_t st = seed ? seed : 0xdeadbeefULL;
    Face last_face = Face::U;
    bool have_last = false;

    while (static_cast<int>(out.size()) < length) {
        Face f = static_cast<Face>(splitmix64(st) % 6);
        if (have_last && f == last_face) continue;   // don't stack same-face moves

        int inner = 0;
        if (max_extra_depth > 0) {
            inner = static_cast<int>(splitmix64(st) % (max_extra_depth + 1));
        }
        int outer = 0;

        Turn t = static_cast<Turn>(splitmix64(st) % 3);
        out.push_back({f, outer, inner, t});
        last_face = f;
        have_last = true;
    }
    return out;
}

namespace {

constexpr Turn ALL_TURNS[3] = {Turn::CW, Turn::Half, Turn::CCW};

inline Turn inverse_turn(Turn t) {
    switch (t) {
        case Turn::CW:   return Turn::CCW;
        case Turn::CCW:  return Turn::CW;
        case Turn::Half: return Turn::Half;
    }
    return Turn::CW;
}

// the two axis faces of each face's rotation axis
inline void axis_faces_of(Face f, Face out[2]) {
    switch (f) {
        case Face::R: case Face::L: out[0] = Face::R; out[1] = Face::L; break;
        case Face::U: case Face::D: out[0] = Face::U; out[1] = Face::D; break;
        case Face::F: case Face::B: out[0] = Face::F; out[1] = Face::B; break;
    }
}

void append_outer_turns(std::vector<MoveStep>& out) {
    const Face faces[6] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (Face f : faces) {
        for (Turn t : ALL_TURNS) {
            out.push_back({Move{f, 0, 0, t}});
        }
    }
}

void append_wide_moves(std::vector<MoveStep>& out, int max_inner) {
    const Face faces[6] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (Face f : faces) {
        for (int inner = 1; inner <= max_inner; ++inner) {
            for (Turn t : ALL_TURNS) {
                out.push_back({Move{f, 0, inner, t}});
            }
        }
    }
}

void append_inner_slices(std::vector<MoveStep>& out, int min_depth, int max_depth) {
    const Face faces[6] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (Face f : faces) {
        for (int d = min_depth; d <= max_depth; ++d) {
            for (Turn t : ALL_TURNS) {
                out.push_back({Move{f, d, d, t}});
            }
        }
    }
}

// generate every safe slice-face-slice conjugate: S · X · S⁻¹
void append_slice_conjugates(std::vector<MoveStep>& out, int n) {
    const Face slice_faces[6] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    const int max_slice_depth = (n / 2) - 1;
    if (max_slice_depth < 1) return;

    for (Face s_face : slice_faces) {
        Face axis[2];
        axis_faces_of(s_face, axis);
        for (int d = 1; d <= max_slice_depth; ++d) {
            for (Turn s_turn : ALL_TURNS) {
                Move s_move{s_face, d, d, s_turn};
                Move s_inv {s_face, d, d, inverse_turn(s_turn)};
                for (Face x_face : axis) {
                    for (Turn x_turn : ALL_TURNS) {
                        Move x_move{x_face, 0, 0, x_turn};
                        out.push_back({s_move, x_move, s_inv});
                    }
                }
            }
        }
    }
}

}

void apply_move_step(NxNCube& cube, const MoveStep& step) {
    for (const Move& m : step) apply_move(cube, m);
}

std::vector<MoveStep> legal_move_steps_for_stage(int n, Stage stage) {
    std::vector<MoveStep> out;
    switch (stage) {
        case Stage::Centers: {
            append_outer_turns(out);
            if (n >= 4) {
                const int max_wide_inner = (n / 2) - 1;
                if (max_wide_inner >= 1) append_wide_moves(out, max_wide_inner);
                append_inner_slices(out, 1, (n / 2) - 1);
            }
            break;
        }
        case Stage::Edges: {
            append_outer_turns(out);
            if (n >= 4) append_slice_conjugates(out, n);
            break;
        }
        case Stage::Parity: {
            append_outer_turns(out);
            break;
        }
    }
    return out;
}

}