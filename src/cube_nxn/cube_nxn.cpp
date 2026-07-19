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