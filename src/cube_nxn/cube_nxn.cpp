#include "cube_nxn/cube_nxn.h"

#include <cassert>
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

// where along the face this strip lives
enum class Edge : uint8_t { First, Last };

struct Strip {
    Face face;
    Dir  dir;
    Edge edge;
};

inline int edge_line(Edge e, int n) { return (e == Edge::First) ? 0 : n - 1; }

// read the n stickers of `s` into `out` in the direction dictated by s.dir
inline void read_strip(const NxNCube& cube, const Strip& s, uint8_t* out) {
    const int n = cube.n();
    const int line = edge_line(s.edge, n);
    const uint8_t* fdata = cube.face_data(static_cast<int>(s.face));
    switch (s.dir) {
        case Dir::RowLR: for (int i = 0; i < n; ++i) out[i] = fdata[line * n + i];         break;
        case Dir::RowRL: for (int i = 0; i < n; ++i) out[i] = fdata[line * n + (n - 1 - i)]; break;
        case Dir::ColTB: for (int i = 0; i < n; ++i) out[i] = fdata[i * n + line];         break;
        case Dir::ColBT: for (int i = 0; i < n; ++i) out[i] = fdata[(n - 1 - i) * n + line]; break;
    }
}

// write n stickers from `in` into `s` in the direction dictated by s.dir
inline void write_strip(NxNCube& cube, const Strip& s, const uint8_t* in) {
    const int n = cube.n();
    const int line = edge_line(s.edge, n);
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
    {Face::F, Dir::RowLR, Edge::First},
    {Face::L, Dir::RowLR, Edge::First},
    {Face::B, Dir::RowLR, Edge::First},
    {Face::R, Dir::RowLR, Edge::First},
}};

constexpr OuterMove R_MOVE = {{
    {Face::F, Dir::ColTB, Edge::Last},
    {Face::U, Dir::ColTB, Edge::Last},
    {Face::B, Dir::ColBT, Edge::First},
    {Face::D, Dir::ColTB, Edge::Last},
}};

constexpr OuterMove F_MOVE = {{
    {Face::U, Dir::RowLR, Edge::Last},
    {Face::R, Dir::ColTB, Edge::First},
    {Face::D, Dir::RowRL, Edge::First},
    {Face::L, Dir::ColBT, Edge::Last},
}};

constexpr OuterMove D_MOVE = {{
    {Face::F, Dir::RowLR, Edge::Last},
    {Face::R, Dir::RowLR, Edge::Last},
    {Face::B, Dir::RowLR, Edge::Last},
    {Face::L, Dir::RowLR, Edge::Last},
}};

constexpr OuterMove L_MOVE = {{
    {Face::F, Dir::ColTB, Edge::First},
    {Face::D, Dir::ColTB, Edge::First},
    {Face::B, Dir::ColBT, Edge::Last},
    {Face::U, Dir::ColTB, Edge::First},
}};

constexpr OuterMove B_MOVE = {{
    {Face::U, Dir::RowRL, Edge::First},
    {Face::L, Dir::ColTB, Edge::First},
    {Face::D, Dir::RowLR, Edge::Last},
    {Face::R, Dir::ColBT, Edge::Last},
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

void cycle_strips_once(NxNCube& cube, const OuterMove& m) {
    const int n = cube.n();
    std::vector<uint8_t> a(n), b(n), c(n), d(n);
    read_strip(cube, m.strips[0], a.data());
    read_strip(cube, m.strips[1], b.data());
    read_strip(cube, m.strips[2], c.data());
    read_strip(cube, m.strips[3], d.data());
    write_strip(cube, m.strips[1], a.data());
    write_strip(cube, m.strips[2], b.data());
    write_strip(cube, m.strips[3], c.data());
    write_strip(cube, m.strips[0], d.data());
}

}

void apply_outer_move(NxNCube& cube, Face f, Turn t) {
    rotate_face(cube, static_cast<int>(f), t);
    const OuterMove& m = outer_table(f);
    const int shifts = (t == Turn::CW) ? 1 : (t == Turn::Half) ? 2 : 3;
    for (int i = 0; i < shifts; ++i) cycle_strips_once(cube, m);
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

}