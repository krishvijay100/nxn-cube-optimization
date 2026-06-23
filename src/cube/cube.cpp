#include "cube/cube.h"

#include <array>

namespace cube {

CubeState solved_cube() {
    CubeState c{};
    for (uint8_t i = 0; i < NUM_CORNERS; ++i) {
        c.corner_positions[i] = i;
        c.corner_orientations[i] = 0;
    }
    for (uint8_t i = 0; i < NUM_EDGES; ++i) {
        c.edge_positions[i] = i;
        c.edge_orientations[i] = 0;
    }
    return c;
}

// Slot ordering:
//   Corners 0..7:  URF, UFL, ULB, UBR, DFR, DLF, DBL, DRB
//   Edges   0..11: UR,  UF,  UL,  UB,  DR,  DF,  DL,  DB,  FR, FL, BL, BR
//
// Convention B: corner_perm[i] = j means "after the move, slot i holds the
// piece that came from slot j." Same for edges.
//
// Orientation deltas are added (mod 3 for corners, mod 2 for edges) to the
// orientation of the piece moving INTO slot i.

struct MoveTable {
    std::array<uint8_t, NUM_CORNERS> corner_perm;
    std::array<uint8_t, NUM_CORNERS> corner_orient_delta;
    std::array<uint8_t, NUM_EDGES>   edge_perm;
    std::array<uint8_t, NUM_EDGES>   edge_orient_delta;
};

namespace {

// U: cycles top-layer corners URF -> UFL -> ULB -> UBR -> URF
//    cycles top-layer edges   UR  -> UF  -> UL  -> UB  -> UR
// Corner orientations unchanged (rotation around U/D axis).
// Edge orientations unchanged (Kociemba convention: only F/B flip edges).
constexpr MoveTable MOVE_U = {
    {3,0,1,2,4,5,6,7},
    {0,0,0,0,0,0,0,0},
    {3,0,1,2,4,5,6,7,8,9,10,11},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// D: cycles bottom-layer corners DFR -> DLF -> DBL -> DRB -> DFR
//    cycles bottom-layer edges   DR  -> DF  -> DL  -> DB  -> DR
constexpr MoveTable MOVE_D = {
    {0,1,2,3,7,4,5,6},
    {0,0,0,0,0,0,0,0},
    {0,1,2,3,7,4,5,6,8,9,10,11},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// R: cycles corners URF -> DRB -> DFR -> UBR (... slots 0,7,4,3 in our indexing)
//    Net: corner_perm = {DFR, UFL, ULB, URF, DRB, DLF, DBL, UBR}
//                      = {  4,   1,   2,   0,   7,   5,   6,   3}
//    Cycles edges UR -> BR -> DR -> FR -> UR (slots 0,11,4,8)
//    Net: edge_perm = {BR, UF, UL, UB, FR, DF, DL, DB, UR, FL, BL, DR}
//                   = {11,  1,  2,  3,  8,  5,  6,  7,  0,  9, 10,  4}
//    Corner orientations twist:
//      Slots gaining a U-face piece: rotation moves the U sticker off the U-face.
//      Standard Kociemba: URF +1, UBR +2, DFR +2, DRB +1.
constexpr MoveTable MOVE_R = {
    {4,1,2,0,7,5,6,3},
    {2,0,0,1,1,0,0,2},
    {11,1,2,3,8,5,6,7,0,9,10,4},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// L: cycles corners UFL -> ULB -> DBL -> DLF -> UFL (slots 1,2,6,5)
//    corner_perm = {URF, DLF, UFL, UBR, DFR, DBL, ULB, DRB}
//                = {  0,   5,   1,   3,   4,   6,   2,   7}
//    Cycles edges UL -> FL -> DL -> BL -> UL (slots 2,9,6,10)
//    edge_perm = {UR, UF, FL, UB, DR, DF, BL, DB, FR, DL, UL, BR}
//              = { 0,  1,  9,  3,  4,  5, 10,  7,  8,  6,  2, 11}
//    Corner orientation deltas: UFL +1, ULB +2, DBL +1, DLF +2.
constexpr MoveTable MOVE_L = {
    {0,5,1,3,4,6,2,7},
    {0,1,2,0,0,2,1,0},
    {0,1,9,3,4,5,10,7,8,6,2,11},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// F: cycles corners URF -> DFR -> DLF -> UFL -> URF (slots 0,4,5,1)
//    corner_perm = {UFL, DLF, ULB, UBR, URF, DFR, DBL, DRB}
//                = {  1,   5,   2,   3,   0,   4,   6,   7}
//    Cycles edges UF -> FR -> DF -> FL -> UF (slots 1,8,5,9)
//    edge_perm = {UR, FL, UL, UB, DR, FR, DL, DB, UF, DF, BL, BR}
//              = { 0,  9,  2,  3,  4,  8,  6,  7,  1,  5, 10, 11}
//    Corner deltas: URF +1, UFL +2, DLF +1, DFR +2.
//    Edge orientations: all four affected edges flip (+1 mod 2).
constexpr MoveTable MOVE_F = {
    {1,5,2,3,0,4,6,7},
    {1,2,0,0,2,1,0,0},
    {0,9,2,3,4,8,6,7,1,5,10,11},
    {0,1,0,0,0,1,0,0,1,1,0,0},
};

// B: cycles corners ULB -> UBR -> DRB -> DBL -> ULB (slots 2,3,7,6)
//    corner_perm = {URF, UFL, UBR, DRB, DFR, DLF, ULB, DBL}
//                = {  0,   1,   3,   7,   4,   5,   2,   6}
//    Cycles edges UB -> BL -> DB -> BR -> UB (slots 3,10,7,11)
//    edge_perm = {UR, UF, UL, BR, DR, DF, DL, BL, FR, FL, DB, UB}
//              = { 0,  1,  2, 11,  4,  5,  6, 10,  8,  9,  7,  3}
//    Corner deltas: ULB +1, UBR +2, DRB +1, DBL +2.
//    Edge orientations: all four affected edges flip.
constexpr MoveTable MOVE_B = {
    {0,1,3,7,4,5,2,6},
    {0,0,1,2,0,0,2,1},
    {0,1,2,11,4,5,6,10,8,9,7,3},
    {0,0,0,1,0,0,0,1,0,0,1,1},
};

constexpr std::array<MoveTable, 6> QUARTER_TURNS = {
    MOVE_U, MOVE_D, MOVE_R, MOVE_L, MOVE_F, MOVE_B,
};

CubeState apply_table(const CubeState& in, const MoveTable& t) {
    CubeState out{};
    for (uint8_t i = 0; i < NUM_CORNERS; ++i) {
        uint8_t src = t.corner_perm[i];
        out.corner_positions[i]    = in.corner_positions[src];
        out.corner_orientations[i] = (in.corner_orientations[src] + t.corner_orient_delta[i]) % 3;
    }
    for (uint8_t i = 0; i < NUM_EDGES; ++i) {
        uint8_t src = t.edge_perm[i];
        out.edge_positions[i]    = in.edge_positions[src];
        out.edge_orientations[i] = (in.edge_orientations[src] + t.edge_orient_delta[i]) % 2;
    }
    return out;
}

// Compose two move tables: first apply `a`, then apply `b`.
MoveTable compose(const MoveTable& a, const MoveTable& b) {
    MoveTable r{};
    for (uint8_t i = 0; i < NUM_CORNERS; ++i) {
        uint8_t mid = b.corner_perm[i];
        r.corner_perm[i] = a.corner_perm[mid];
        r.corner_orient_delta[i] =
            (a.corner_orient_delta[mid] + b.corner_orient_delta[i]) % 3;
    }
    for (uint8_t i = 0; i < NUM_EDGES; ++i) {
        uint8_t mid = b.edge_perm[i];
        r.edge_perm[i] = a.edge_perm[mid];
        r.edge_orient_delta[i] =
            (a.edge_orient_delta[mid] + b.edge_orient_delta[i]) % 2;
    }
    return r;
}

std::array<MoveTable, NUM_MOVES> build_all_move_tables() {
    std::array<MoveTable, NUM_MOVES> tables{};
    for (uint8_t face = 0; face < 6; ++face) {
        const MoveTable& q = QUARTER_TURNS[face];
        MoveTable q2 = compose(q, q);
        MoveTable q3 = compose(q2, q);
        tables[face * 3 + 0] = q;    // quarter clockwise
        tables[face * 3 + 1] = q3;   // prime = three quarter turns
        tables[face * 3 + 2] = q2;   // half turn
    }
    return tables;
}

const std::array<MoveTable, NUM_MOVES>& move_tables() {
    static const std::array<MoveTable, NUM_MOVES> tables = build_all_move_tables();
    return tables;
}

}  // namespace

void apply_move(CubeState& c, Move m) {
    const MoveTable& t = move_tables()[static_cast<uint8_t>(m)];
    c = apply_table(c, t);
}

bool is_solved(const CubeState& c) {
    for (uint8_t i = 0; i < NUM_CORNERS; ++i) {
        if (c.corner_positions[i] != i) return false;
        if (c.corner_orientations[i] != 0) return false;
    }
    for (uint8_t i = 0; i < NUM_EDGES; ++i) {
        if (c.edge_positions[i] != i) return false;
        if (c.edge_orientations[i] != 0) return false;
    }
    return true;
}

}
