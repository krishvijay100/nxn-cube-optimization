#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cube_nxn {

// wca face convention; solved cube has stickers[face_id * n*n + i] == face_id for every i
enum class Face : uint8_t {
    U = 0,
    R = 1,
    F = 2,
    D = 3,
    L = 4,
    B = 5,
};

constexpr int NUM_FACES = 6;

// index of the sticker at (face, row, col) is face*n*n + row*n + col
class NxNCube {
public:
    explicit NxNCube(int n);

    int n() const { return n_; }
    int face_size() const { return n_ * n_; }
    int num_stickers() const { return NUM_FACES * n_ * n_; }

    uint8_t sticker(int face, int row, int col) const;
    void set_sticker(int face, int row, int col, uint8_t color);

    uint8_t* face_data(int face);
    const uint8_t* face_data(int face) const;

    uint8_t* raw() { return stickers_.data(); }
    const uint8_t* raw() const { return stickers_.data(); }

    bool is_solved() const;

private:
    int n_;
    std::vector<uint8_t> stickers_;
};

enum class Turn : uint8_t {
    CW = 0,
    Half = 1,
    CCW = 2,
};

// rotate the stickers of one face in place by the given amount; does NOT touch any other face
void rotate_face(NxNCube& cube, int face, Turn t);

// apply a single outer-face move at the given turn amount; rotates the target face's own stickers AND cycles the neighboring strip on each of the four adjacent faces
void apply_outer_move(NxNCube& cube, Face f, Turn t);

// for wide or inner-slice move
void apply_wide_move(NxNCube& cube, Face f, int outer_depth, int inner_depth, Turn t);

struct Move {
    Face face;
    int  outer_depth;
    int  inner_depth;
    Turn turn;
};

void apply_move(NxNCube& cube, const Move& m);

std::optional<Move> parse_move(std::string_view s);

std::optional<std::vector<Move>> parse_scramble(std::string_view s);

std::string format_move(const Move& m);

std::vector<Move> random_scramble(int n, int length, uint64_t seed);

// stage of the reduction pipeline being solved; determines the move set the BFS is allowed to explore
enum class Stage : uint8_t {
    Centers,   // no prior-stage constraints; all moves legal
    Edges,     // centers must remain solved
    Parity,    // centers + edges preserved; 3x3-equivalent move set
};

using MoveStep = std::vector<Move>;

std::vector<MoveStep> legal_move_steps_for_stage(int n, Stage stage);
void apply_move_step(NxNCube& cube, const MoveStep& step);

}