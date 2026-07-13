#include <gtest/gtest.h>

#include <array>
#include <set>
#include <vector>

#include "cube/cube.h"
#include "cube/scramble.h"
#include "solver/coords.h"
#include "solver/move_tables.h"
#include "solver/symmetry.h"

namespace {

bool states_equal(const cube::CubeState& a, const cube::CubeState& b) {
    return a.corner_positions == b.corner_positions
        && a.corner_orientations == b.corner_orientations
        && a.edge_positions == b.edge_positions
        && a.edge_orientations == b.edge_orientations;
}

// serialize a state to a comparable byte string for group-closure checks
std::vector<uint8_t> state_bytes(const cube::CubeState& c) {
    std::vector<uint8_t> b;
    b.reserve(2 * (cube::NUM_CORNERS + cube::NUM_EDGES));
    for (auto v : c.corner_positions)    b.push_back(v);
    for (auto v : c.corner_orientations) b.push_back(v);
    for (auto v : c.edge_positions)      b.push_back(v);
    for (auto v : c.edge_orientations)   b.push_back(v);
    return b;
}

}  // namespace

// with the correct symmetry action (state conjugation), every element fixes the
// solved cube, so we fingerprint each element by its action on a fixed *scrambled*
// probe state instead. equal fingerprints => same group element
namespace {
cube::CubeState probe_state() {
    using namespace cube;
    auto seq = random_scramble(15, /*seed*/ 42);
    CubeState c = solved_cube();
    for (Move m : seq) apply_move(c, m);
    return c;
}
}  // namespace

// distinctness: every apply_element index must produce a distinct action on the
// probe state, otherwise our indexing collapses two group elements into one
TEST(Symmetry, ElementsAreDistinct) {
    cube::CubeState probe = probe_state();
    std::set<std::vector<uint8_t>> fingerprints;
    for (int s = 0; s < cube::solver::NUM_SYM; ++s) {
        fingerprints.insert(state_bytes(cube::solver::apply_symmetry(s, probe)));
    }
    EXPECT_EQ(int(fingerprints.size()), cube::solver::NUM_SYM)
        << "apply_element indices collapse (some indices produce identical actions)";
}

// group closure: composing any two elements' actions on the probe state must
// yield the action of another element in the group
TEST(Symmetry, GroupIsClosedUnderComposition) {
    cube::CubeState probe = probe_state();
    std::set<std::vector<uint8_t>> members;
    for (int s = 0; s < cube::solver::NUM_SYM; ++s) {
        members.insert(state_bytes(cube::solver::apply_symmetry(s, probe)));
    }
    for (int a = 0; a < cube::solver::NUM_SYM; ++a) {
        for (int b = 0; b < cube::solver::NUM_SYM; ++b) {
            cube::CubeState after = cube::solver::apply_symmetry(a, probe);
            after = cube::solver::apply_symmetry(b, after);
            EXPECT_TRUE(members.count(state_bytes(after)) > 0)
                << "sym " << b << " . sym " << a << " escapes the group";
        }
    }
}

// every element must have an inverse in the group: applying it, then some
// other element, returns the probe to itself
TEST(Symmetry, EveryElementHasAnInverseInTheGroup) {
    cube::CubeState probe = probe_state();
    for (int s = 0; s < cube::solver::NUM_SYM; ++s) {
        cube::CubeState transformed = cube::solver::apply_symmetry(s, probe);
        int inverses_found = 0;
        for (int t = 0; t < cube::solver::NUM_SYM; ++t) {
            cube::CubeState back = cube::solver::apply_symmetry(t, transformed);
            if (states_equal(back, probe)) ++inverses_found;
        }
        EXPECT_GE(inverses_found, 1) << "no inverse for element " << s;
    }
}

// solved cube is invariant under every phase-2 symmetry
TEST(Symmetry, SolvedCubeIsFixedByEveryElement) {
    cube::CubeState solved = cube::solved_cube();
    for (int s = 0; s < cube::solver::NUM_SYM; ++s) {
        cube::CubeState fixed = cube::solver::apply_symmetry(s, solved);
        EXPECT_TRUE(states_equal(fixed, solved))
            << "sym " << s << " does not fix the solved cube";
    }
}

// the core correctness property
//   apply_move(apply_sym(c, s), conj[s][m]) == apply_sym(apply_move(c, m), s)
TEST(Symmetry, ConjugationCommutesWithMoves) {
    using namespace cube;
    const auto& conj = cube::solver::sym_conj_move();
    for (int s = 0; s < cube::solver::NUM_SYM; ++s) {
        for (uint64_t seed = 1; seed <= 5; ++seed) {
            auto scramble = random_scramble(15, seed);
            CubeState base = solved_cube();
            for (Move m : scramble) apply_move(base, m);

            for (int mi = 0; mi < NUM_MOVES; ++mi) {
                Move m = static_cast<Move>(mi);
                Move conj_m = static_cast<Move>(conj[s][mi]);

                CubeState a = cube::solver::apply_symmetry(s, base);
                apply_move(a, conj_m);

                CubeState b = base;
                apply_move(b, m);
                b = cube::solver::apply_symmetry(s, b);

                EXPECT_TRUE(states_equal(a, b))
                    << "conjugation failure: sym=" << s << " move=" << mi
                    << " conj=" << int(conj[s][mi]) << " seed=" << seed;
                if (!states_equal(a, b)) return;
            }
        }
    }
}

// coord-level conjugation must commute with the phase-2 move tables:
//   move_table[sym_c[s][c]][conj[s][m]] == sym_c[s][move_table[c][m]]
TEST(Symmetry, CoordLevelConjugationCommutesWithG1Moves) {
    using namespace cube::solver;
    const auto& conj = sym_conj_move();
    const auto& sym_cp = sym_corner_perm();
    const auto& sym_ep = sym_edge_perm_ud();
    const auto& sym_sp = sym_slice_perm();
    const auto& cp_move = corner_perm_move_table();
    const auto& ep_move = edge_perm_ud_move_table();
    const auto& sp_move = slice_perm_move_table();

    for (int s = 0; s < NUM_SYM; ++s) {
        for (int i = 0; i < NUM_G1_MOVES; ++i) {
            int raw_move = static_cast<int>(G1_MOVES[i]);
            int conjugated = conj[s][raw_move];
            // conjugated move must also be a G1 move (phase-2 preservation)
            int conj_i = -1;
            for (int j = 0; j < NUM_G1_MOVES; ++j) {
                if (static_cast<int>(G1_MOVES[j]) == conjugated) { conj_i = j; break; }
            }
            ASSERT_NE(conj_i, -1) << "sym " << s << " maps G1 move " << i
                                  << " to non-G1 move " << conjugated;

            for (Coord cp : {Coord(0), Coord(1), Coord(1234), Coord(20000), Coord(40319)}) {
                Coord lhs = cp_move[sym_cp[s][cp]][conj_i];
                Coord rhs = sym_cp[s][cp_move[cp][i]];
                EXPECT_EQ(lhs, rhs) << "cp conj: s=" << s << " g1_i=" << i << " cp=" << cp;
            }
            for (Coord ep : {Coord(0), Coord(1), Coord(1234), Coord(20000), Coord(40319)}) {
                Coord lhs = ep_move[sym_ep[s][ep]][conj_i];
                Coord rhs = sym_ep[s][ep_move[ep][i]];
                EXPECT_EQ(lhs, rhs) << "ep conj: s=" << s << " g1_i=" << i << " ep=" << ep;
            }
            for (Coord sp = 0; sp < SLICE_PERM_COORDS; ++sp) {
                Coord lhs = sp_move[sym_sp[s][sp]][conj_i];
                Coord rhs = sym_sp[s][sp_move[sp][i]];
                EXPECT_EQ(lhs, rhs) << "sp conj: s=" << s << " g1_i=" << i << " sp=" << sp;
            }
        }
    }
}

TEST(Symmetry, IdentitySymCoordConjugationIsIdentity) {
    using cube::solver::sym_corner_perm;
    using cube::solver::sym_edge_perm_ud;
    using cube::solver::sym_slice_perm;

    const auto& cp = sym_corner_perm();
    const auto& ep = sym_edge_perm_ud();
    const auto& sp = sym_slice_perm();

    for (cube::solver::Coord i = 0; i < cube::solver::CORNER_PERM_COORDS; ++i) {
        ASSERT_EQ(cp[0][i], i);
    }
    for (cube::solver::Coord i = 0; i < cube::solver::EDGE_PERM_UD_COORDS; ++i) {
        ASSERT_EQ(ep[0][i], i);
    }
    for (cube::solver::Coord i = 0; i < cube::solver::SLICE_PERM_COORDS; ++i) {
        ASSERT_EQ(sp[0][i], i);
    }
}
