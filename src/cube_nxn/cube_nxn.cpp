#include "cube_nxn/cube_nxn.h"

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

#include "solver/ida.h"

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

BFSResult reduce_bfs(
    const NxNCube&                                  start,
    const std::function<bool(const NxNCube&)>&      is_goal,
    const std::function<uint64_t(const NxNCube&)>&  state_hash,
    const std::vector<MoveStep>&                    moves,
    int                                             max_depth,
    size_t                                          max_nodes)
{
    // early-out if the start already satisfies the goal
    if (is_goal(start)) {
        return {true, {}, 1};
    }

    // naive path-in-queue BFS; switch to parent-link reconstruction later if a stage blows the memory budget
    struct Frame {
        NxNCube cube;
        std::vector<MoveStep> path;
    };

    std::queue<Frame> q;
    std::unordered_set<uint64_t> seen;

    q.push({start, {}});
    seen.insert(state_hash(start));
    size_t explored = 1;

    while (!q.empty()) {
        Frame cur = std::move(q.front());
        q.pop();

        const int depth = static_cast<int>(cur.path.size());
        if (depth >= max_depth) continue;

        for (const MoveStep& step : moves) {
            NxNCube next = cur.cube;
            apply_move_step(next, step);

            uint64_t h = state_hash(next);
            if (!seen.insert(h).second) continue;
            ++explored;

            if (is_goal(next)) {
                std::vector<MoveStep> out = cur.path;
                out.push_back(step);
                return {true, std::move(out), explored};
            }

            if (explored >= max_nodes) {
                // hard node cap, bail
                return {false, {}, explored};
            }

            std::vector<MoveStep> next_path = cur.path;
            next_path.push_back(step);
            q.push({std::move(next), std::move(next_path)});
        }
    }

    return {false, {}, explored};
}

namespace {

constexpr int N4_CENTER_SLOTS[4][2] = {{1,1}, {1,2}, {2,1}, {2,2}};

bool face_centers_monochrome(const NxNCube& cube, Face face) {
    const uint8_t color = static_cast<uint8_t>(face);
    for (const auto& slot : N4_CENTER_SLOTS) {
        if (cube.sticker(static_cast<int>(face), slot[0], slot[1]) != color) return false;
    }
    return true;
}

// 64-bit fnv-1a of the full sticker array
uint64_t fnv1a_full(const NxNCube& c) {
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint8_t* p = c.raw();
    const int n = c.num_stickers();
    for (int i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

// BFS wrapper that retries at progressively deeper bounds
std::vector<MoveStep> bfs_with_deepening(
    const NxNCube& start,
    const std::function<bool(const NxNCube&)>& is_goal,
    const std::vector<MoveStep>& moves)
{
    const int depths[] = {5, 8, 12};
    for (int d : depths) {
        BFSResult r = reduce_bfs(start, is_goal, fnv1a_full, moves, d);
        if (r.found) return r.sequence;
    }
    return {};
}

}

std::vector<MoveStep> solve_centers_n4(NxNCube& cube) {
    assert(cube.n() == 4 && "solve_centers_n4 currently supports N=4 only");
    const auto moves = legal_move_steps_for_stage(4, Stage::Centers);

    std::vector<MoveStep> total_sequence;
    const Face face_order[6] = {Face::U, Face::D, Face::F, Face::B, Face::L, Face::R};
    std::vector<Face> completed;   // faces already fully solved

    for (Face face : face_order) {
        const uint8_t face_color = static_cast<uint8_t>(face);
        const int face_idx = static_cast<int>(face);

        // slots already filled on the current face during this pass
        std::vector<std::pair<int, int>> placed_this_face;

        for (const auto& slot : N4_CENTER_SLOTS) {
            // skip if slot already correct
            if (cube.sticker(face_idx, slot[0], slot[1]) == face_color) {
                placed_this_face.push_back({slot[0], slot[1]});
                continue;
            }

            const int target_row = slot[0];
            const int target_col = slot[1];
            std::vector<std::pair<int, int>> preserve = placed_this_face;
            std::vector<Face> preserve_faces = completed;

            auto is_goal = [face_color, face_idx, target_row, target_col,
                            preserve, preserve_faces](const NxNCube& c) {
                if (c.sticker(face_idx, target_row, target_col) != face_color) return false;
                for (const auto& s : preserve) {
                    if (c.sticker(face_idx, s.first, s.second) != face_color) return false;
                }
                for (Face f : preserve_faces) {
                    if (!face_centers_monochrome(c, f)) return false;
                }
                return true;
            };

            auto seq = bfs_with_deepening(cube, is_goal, moves);
            if (seq.empty()) {
                // depth 12 was insufficient
                return {};
            }
            for (const auto& s : seq) apply_move_step(cube, s);
            total_sequence.insert(total_sequence.end(), seq.begin(), seq.end());
            placed_this_face.push_back({target_row, target_col});
        }

        completed.push_back(face);
    }
    return total_sequence;
}

namespace {

inline int turn_quarters(Turn t) {
    switch (t) {
        case Turn::CW:   return 1;
        case Turn::Half: return 2;
        case Turn::CCW:  return 3;
    }
    return 0;
}

inline std::optional<Turn> turn_from_quarters(int q) {
    q = ((q % 4) + 4) % 4;
    switch (q) {
        case 0: return std::nullopt;  // net-zero rotation, drop the move
        case 1: return Turn::CW;
        case 2: return Turn::Half;
        case 3: return Turn::CCW;
    }
    return std::nullopt;
}

inline bool same_layer(const Move& a, const Move& b) {
    return a.face == b.face
        && a.outer_depth == b.outer_depth
        && a.inner_depth == b.inner_depth;
}

}

std::vector<Move> collapse_redundant_moves(const std::vector<Move>& moves) {
    std::vector<Move> out;
    out.reserve(moves.size());
    for (const Move& m : moves) {
        if (!out.empty() && same_layer(out.back(), m)) {
            int q = turn_quarters(out.back().turn) + turn_quarters(m.turn);
            auto composed = turn_from_quarters(q);
            if (composed.has_value()) {
                out.back().turn = *composed;
            } else {
                out.pop_back();
            }
        } else {
            out.push_back(m);
        }
    }
    return out;
}

namespace {

struct Sticker { Face face; int row; int col; };
struct WingPair { Sticker a; Sticker b; };
struct EdgeSlot { WingPair w1; WingPair w2; Face face_a; Face face_b; };

constexpr EdgeSlot N4_EDGES[12] = {
    // top layer (U with F/R/B/L)
    { {{Face::U, 3, 1}, {Face::F, 0, 1}}, {{Face::U, 3, 2}, {Face::F, 0, 2}}, Face::U, Face::F },
    { {{Face::U, 1, 3}, {Face::R, 0, 1}}, {{Face::U, 2, 3}, {Face::R, 0, 2}}, Face::U, Face::R },
    { {{Face::U, 0, 2}, {Face::B, 0, 1}}, {{Face::U, 0, 1}, {Face::B, 0, 2}}, Face::U, Face::B },
    { {{Face::U, 2, 0}, {Face::L, 0, 1}}, {{Face::U, 1, 0}, {Face::L, 0, 2}}, Face::U, Face::L },
    // bottom layer (D with F/R/B/L)
    { {{Face::D, 0, 1}, {Face::F, 3, 1}}, {{Face::D, 0, 2}, {Face::F, 3, 2}}, Face::D, Face::F },
    { {{Face::D, 1, 3}, {Face::R, 3, 2}}, {{Face::D, 2, 3}, {Face::R, 3, 1}}, Face::D, Face::R },
    { {{Face::D, 3, 2}, {Face::B, 3, 1}}, {{Face::D, 3, 1}, {Face::B, 3, 2}}, Face::D, Face::B },
    { {{Face::D, 2, 0}, {Face::L, 3, 2}}, {{Face::D, 1, 0}, {Face::L, 3, 1}}, Face::D, Face::L },
    // middle layer (F/R, R/B, B/L, L/F verticals)
    { {{Face::F, 1, 3}, {Face::R, 1, 0}}, {{Face::F, 2, 3}, {Face::R, 2, 0}}, Face::F, Face::R },
    { {{Face::R, 1, 3}, {Face::B, 1, 0}}, {{Face::R, 2, 3}, {Face::B, 2, 0}}, Face::R, Face::B },
    { {{Face::B, 1, 3}, {Face::L, 1, 0}}, {{Face::B, 2, 3}, {Face::L, 2, 0}}, Face::B, Face::L },
    { {{Face::L, 1, 3}, {Face::F, 1, 0}}, {{Face::L, 2, 3}, {Face::F, 2, 0}}, Face::L, Face::F },
};

bool edge_slot_paired(const NxNCube& cube, const EdgeSlot& e) {
    const uint8_t a1 = cube.sticker(static_cast<int>(e.w1.a.face), e.w1.a.row, e.w1.a.col);
    const uint8_t b1 = cube.sticker(static_cast<int>(e.w1.b.face), e.w1.b.row, e.w1.b.col);
    const uint8_t a2 = cube.sticker(static_cast<int>(e.w2.a.face), e.w2.a.row, e.w2.a.col);
    const uint8_t b2 = cube.sticker(static_cast<int>(e.w2.b.face), e.w2.b.row, e.w2.b.col);
    return a1 == a2 && b1 == b2;
}

uint64_t fnv1a_full_edge(const NxNCube& c) {
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint8_t* p = c.raw();
    const int n = c.num_stickers();
    for (int i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

}

namespace {

MoveStep macro_from_str(std::string_view s) {
    auto v = parse_scramble(s);
    assert(v.has_value() && "macro string failed to parse");
    return *v;
}

std::vector<MoveStep> build_algo_edge_moves() {
    std::vector<MoveStep> out;
    append_outer_turns(out);

    // center-preserving pair macros
    const char* macros[] = {
        // U-axis conjugates (Uw/Dw + face turns on R/L/F/B)
        "Uw R U R' Uw'",
        "Uw' L' U' L Uw",
        "Uw F U F' Uw'",
        "Uw' F' U' F Uw",
        "Uw B U B' Uw'",
        "Uw' B' U' B Uw",
        "Uw L U L' Uw'",
        "Uw' R' U' R Uw",
        // D-axis (mirror of U)
        "Dw R' U' R Dw'",
        "Dw' L U L' Dw",
        "Dw F' U' F Dw'",
        "Dw' F U F' Dw",
        "Dw B' U' B Dw'",
        "Dw' B U B' Dw",
        // R-axis
        "Rw U R U' Rw'",
        "Rw' U' R' U Rw",
        "Rw F R F' Rw'",
        "Rw' F' R' F Rw",
        // L-axis (mirror of R)
        "Lw U' L' U Lw'",
        "Lw' U L U' Lw",
        "Lw F' L' F Lw'",
        "Lw' F L F' Lw",
    };
    for (const char* s : macros) {
        out.push_back(macro_from_str(s));
    }
    return out;
}

}

EdgePairResult solve_edges_n4_algo(NxNCube& cube) {
    assert(cube.n() == 4 && "solve_edges_n4_algo currently supports N=4 only");

    static const std::vector<MoveStep> moves = build_algo_edge_moves();

    EdgePairResult result{{}, 0};
    std::vector<int> already_paired_indices;

    for (int i = 0; i < 12; ++i) {
        if (edge_slot_paired(cube, N4_EDGES[i])) {
            already_paired_indices.push_back(i);
            ++result.edges_paired;
            continue;
        }

        std::vector<int> preserve = already_paired_indices;
        int target = i;
        auto is_goal = [target, preserve](const NxNCube& c) {
            if (!edge_slot_paired(c, N4_EDGES[target])) return false;
            for (int idx : preserve) {
                if (!edge_slot_paired(c, N4_EDGES[idx])) return false;
            }
            return true;
        };

        std::vector<MoveStep> seq;
        for (int d : {2, 3, 5}) {
            BFSResult r = reduce_bfs(cube, is_goal, fnv1a_full_edge, moves, d);
            if (r.found) { seq = std::move(r.sequence); break; }
        }
        if (seq.empty()) {
            return result;
        }
        for (const auto& s : seq) apply_move_step(cube, s);
        result.sequence.insert(result.sequence.end(), seq.begin(), seq.end());
        already_paired_indices.push_back(i);
        ++result.edges_paired;
    }
    return result;
}

namespace {

struct Edge3x3 {
    const char* name;
    uint8_t     color_a;         // face_a color (also the slot's primary face)
    uint8_t     color_b;         // face_b color
    uint8_t     primary_color;   // color that means "oriented"
};

constexpr Edge3x3 EDGES_3x3[12] = {
    {"UF", 0, 2, 0}, {"UR", 0, 1, 0}, {"UB", 0, 5, 0}, {"UL", 0, 4, 0},
    {"DF", 3, 2, 3}, {"DR", 3, 1, 3}, {"DB", 3, 5, 3}, {"DL", 3, 4, 3},
    {"FR", 2, 1, 2}, {"FL", 2, 4, 2}, {"BR", 5, 1, 5}, {"BL", 5, 4, 5},
};

struct Corner3x3 {
    const char* name;
    uint8_t     color_primary;   // U or D color — primary sticker
    uint8_t     color_b;
    uint8_t     color_c;
    Sticker     s[3];            // sticker positions in canonical order (primary first)
};

constexpr Corner3x3 CORNERS_3x3[8] = {
    {"UFR", 0, 2, 1, {{Face::U, 3, 3}, {Face::F, 0, 3}, {Face::R, 0, 0}}},
    {"URB", 0, 1, 5, {{Face::U, 0, 3}, {Face::R, 0, 3}, {Face::B, 0, 0}}},
    {"UBL", 0, 5, 4, {{Face::U, 0, 0}, {Face::B, 0, 3}, {Face::L, 0, 0}}},
    {"ULF", 0, 4, 2, {{Face::U, 3, 0}, {Face::L, 0, 3}, {Face::F, 0, 0}}},
    {"DRF", 3, 1, 2, {{Face::D, 0, 3}, {Face::R, 3, 0}, {Face::F, 3, 3}}},
    {"DBR", 3, 5, 1, {{Face::D, 3, 3}, {Face::B, 3, 0}, {Face::R, 3, 3}}},
    {"DLB", 3, 4, 5, {{Face::D, 3, 0}, {Face::L, 3, 0}, {Face::B, 3, 3}}},
    {"DFL", 3, 2, 4, {{Face::D, 0, 0}, {Face::F, 3, 0}, {Face::L, 3, 3}}},
};

inline bool same_color_pair(uint8_t a1, uint8_t b1, uint8_t a2, uint8_t b2) {
    return (a1 == a2 && b1 == b2) || (a1 == b2 && b1 == a2);
}

bool identify_edge_at_slot(const NxNCube& cube, int slot_idx, int& home_idx, int& orient) {
    const EdgeSlot& slot = N4_EDGES[slot_idx];
    const uint8_t ca = cube.sticker(static_cast<int>(slot.w1.a.face),
                                     slot.w1.a.row, slot.w1.a.col);
    const uint8_t cb = cube.sticker(static_cast<int>(slot.w1.b.face),
                                     slot.w1.b.row, slot.w1.b.col);
    for (int i = 0; i < 12; ++i) {
        if (same_color_pair(ca, cb, EDGES_3x3[i].color_a, EDGES_3x3[i].color_b)) {
            home_idx = i;
            const uint8_t home_primary = EDGES_3x3[i].primary_color;
            if (ca == home_primary)      orient = 0;
            else if (cb == home_primary) orient = 1;
            else return false;
            return true;
        }
    }
    return false;
}

bool identify_corner_at_slot(const NxNCube& cube, int slot_idx, int& home_idx, int& orient) {
    const Corner3x3& slot = CORNERS_3x3[slot_idx];
    uint8_t cols[3];
    for (int k = 0; k < 3; ++k) {
        cols[k] = cube.sticker(static_cast<int>(slot.s[k].face),
                                slot.s[k].row, slot.s[k].col);
    }
    for (int i = 0; i < 8; ++i) {
        const uint8_t h[3] = {CORNERS_3x3[i].color_primary, CORNERS_3x3[i].color_b, CORNERS_3x3[i].color_c};
        int match = 0;
        bool used[3] = {false, false, false};
        for (int t = 0; t < 3; ++t) {
            for (int k = 0; k < 3; ++k) {
                if (!used[k] && cols[k] == h[t]) {
                    used[k] = true;
                    ++match;
                    break;
                }
            }
        }
        if (match == 3) {
            home_idx = i;
            for (int k = 0; k < 3; ++k) {
                if (cols[k] == CORNERS_3x3[i].color_primary) {
                    orient = k;
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

// parity sign of a permutation via cycle decomposition; +1 for even, -1 for odd
int permutation_sign(const int* perm, int n) {
    std::vector<bool> visited(n, false);
    int sign = 1;
    for (int i = 0; i < n; ++i) {
        if (visited[i]) continue;
        int length = 0, j = i;
        while (!visited[j]) {
            visited[j] = true;
            j = perm[j];
            ++length;
        }
        if (length % 2 == 0) sign = -sign;
    }
    return sign;
}

struct Analysis {
    int edge_perm[12];
    int edge_orient[12];
    int corner_perm[8];
    int corner_orient[8];
    int edge_orient_sum;
    int corner_orient_sum;
    int edge_perm_sign;
    int corner_perm_sign;
};

bool analyze_3x3_state(const NxNCube& cube, Analysis& out) {
    for (int i = 0; i < 12; ++i) {
        if (!identify_edge_at_slot(cube, i, out.edge_perm[i], out.edge_orient[i])) {
            return false;
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (!identify_corner_at_slot(cube, i, out.corner_perm[i], out.corner_orient[i])) {
            return false;
        }
    }
    int eo_sum = 0, co_sum = 0;
    for (int i = 0; i < 12; ++i) eo_sum += out.edge_orient[i];
    for (int i = 0; i < 8;  ++i) co_sum += out.corner_orient[i];
    out.edge_orient_sum   = eo_sum % 2;
    out.corner_orient_sum = co_sum % 3;
    out.edge_perm_sign    = permutation_sign(out.edge_perm, 12);
    out.corner_perm_sign  = permutation_sign(out.corner_perm, 8);
    return true;
}

// standard 4x4 parity fix algorithms
MoveStep parse_alg(const char* s) {
    auto v = parse_scramble(s);
    assert(v.has_value() && "parity alg failed to parse");
    return *v;
}

const MoveStep& oll_parity_alg() {
    static const MoveStep alg = parse_alg("Rw2 B2 U2 Lw U2 Rw' U2 Rw U2 F2 Rw F2 Lw' B2 Rw2");
    return alg;
}

const MoveStep& pll_parity_alg() {
    static const MoveStep alg = parse_alg("2R2 U2 2R2 Uw2 2R2 Uw2");
    return alg;
}

}

ParityState detect_parity_n4(const NxNCube& cube) {
    assert(cube.n() == 4 && "detect_parity_n4 currently supports N=4 only");
    Analysis a;
    if (!analyze_3x3_state(cube, a)) {
        return ParityState::Both;
    }
    const bool oll = (a.edge_orient_sum != 0);
    const bool pll = (a.edge_perm_sign != a.corner_perm_sign);
    if (oll && pll) return ParityState::Both;
    if (oll)        return ParityState::OLL;
    if (pll)        return ParityState::PLL;
    return ParityState::Valid;
}

std::vector<MoveStep> fix_parity_n4(NxNCube& cube) {
    std::vector<MoveStep> out;
    const ParityState p = detect_parity_n4(cube);
    if (p == ParityState::OLL || p == ParityState::Both) {
        const MoveStep& alg = oll_parity_alg();
        apply_move_step(cube, alg);
        out.push_back(alg);
    }
    if (p == ParityState::PLL || p == ParityState::Both) {
        const MoveStep& alg = pll_parity_alg();
        apply_move_step(cube, alg);
        out.push_back(alg);
    }
    return out;
}

namespace {

constexpr int N4_EDGE_SLOT_TO_CUBE[12]     = {1, 0, 3, 2, 5, 4, 7, 6, 8, 11, 10, 9};
constexpr int EDGES_3X3_HOME_TO_CUBE[12]   = {1, 0, 3, 2, 5, 4, 7, 6, 8, 9, 11, 10};
constexpr int CORNER_SLOT_REMAP[8]         = {0, 3, 2, 1, 4, 7, 6, 5};
constexpr int N4_EDGE_ORIENT_FLIP[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};

}

std::optional<cube::CubeState> to_cube_state_3x3(const NxNCube& cube) {
    assert(cube.n() == 4 && "to_cube_state_3x3 currently supports N=4 only");
    Analysis a;
    if (!analyze_3x3_state(cube, a)) return std::nullopt;

    cube::CubeState c{};
    for (int i = 0; i < 12; ++i) {
        const int dst = N4_EDGE_SLOT_TO_CUBE[i];
        c.edge_positions[dst]    = static_cast<uint8_t>(EDGES_3X3_HOME_TO_CUBE[a.edge_perm[i]]);
        c.edge_orientations[dst] = static_cast<uint8_t>(a.edge_orient[i] ^ N4_EDGE_ORIENT_FLIP[i]);
    }
    for (int i = 0; i < 8; ++i) {
        const int dst = CORNER_SLOT_REMAP[i];
        c.corner_positions[dst]    = static_cast<uint8_t>(CORNER_SLOT_REMAP[a.corner_perm[i]]);
        c.corner_orientations[dst] = static_cast<uint8_t>((3 - a.corner_orient[i]) % 3);
    }
    return c;
}

Move to_nxn_move(cube::Move m) {
    const uint8_t v = static_cast<uint8_t>(m);
    const Face face_map[6] = {Face::U, Face::D, Face::R, Face::L, Face::F, Face::B};
    const Turn turn_map[3] = {Turn::CW, Turn::CCW, Turn::Half};
    return Move{face_map[v / 3], 0, 0, turn_map[v % 3]};
}

std::vector<Move> finish_as_3x3(NxNCube& cube) {
    auto state_opt = to_cube_state_3x3(cube);
    if (!state_opt) return {};

    const auto solution_3x3 = cube::solver::solve_parallel(*state_opt, 8);
    if (solution_3x3.empty() && !cube::is_solved(*state_opt)) return {};

    std::vector<Move> out;
    out.reserve(solution_3x3.size());
    for (cube::Move m3 : solution_3x3) {
        Move mn = to_nxn_move(m3);
        apply_move(cube, mn);
        out.push_back(mn);
    }
    return out;
}

}