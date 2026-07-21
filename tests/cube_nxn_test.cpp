#include <gtest/gtest.h>

#include "cube_nxn/cube_nxn.h"

using cube_nxn::Face;
using cube_nxn::NxNCube;
using cube_nxn::NUM_FACES;
using cube_nxn::Turn;
using cube_nxn::rotate_face;
using cube_nxn::apply_outer_move;
using cube_nxn::apply_wide_move;
using cube_nxn::Move;
using cube_nxn::apply_move;
using cube_nxn::parse_move;
using cube_nxn::parse_scramble;
using cube_nxn::format_move;
using cube_nxn::random_scramble;
using cube_nxn::Stage;
using cube_nxn::MoveStep;
using cube_nxn::legal_move_steps_for_stage;
using cube_nxn::apply_move_step;
using cube_nxn::reduce_bfs;
using cube_nxn::solve_centers_n4;
using cube_nxn::collapse_redundant_moves;
using cube_nxn::solve_edges_n4_algo;
using cube_nxn::EdgePairResult;

namespace {

void paint_face_indexed(NxNCube& c, int face) {
    const int n = c.n();
    for (int r = 0; r < n; ++r) {
        for (int col = 0; col < n; ++col) {
            c.set_sticker(face, r, col, static_cast<uint8_t>(r * n + col));
        }
    }
}

}

TEST(NxNCubeConstruct, SolvedByDefault) {
    for (int n = 2; n <= 7; ++n) {
        NxNCube c(n);
        EXPECT_EQ(c.n(), n);
        EXPECT_EQ(c.face_size(), n * n);
        EXPECT_EQ(c.num_stickers(), 6 * n * n);
        EXPECT_TRUE(c.is_solved());

        for (int f = 0; f < NUM_FACES; ++f) {
            for (int r = 0; r < n; ++r) {
                for (int col = 0; col < n; ++col) {
                    EXPECT_EQ(c.sticker(f, r, col), static_cast<uint8_t>(f))
                        << "n=" << n << " f=" << f << " r=" << r << " c=" << col;
                }
            }
        }
    }
}

TEST(NxNCubeSolvedCheck, DetectsMutation) {
    NxNCube c(4);
    ASSERT_TRUE(c.is_solved());

    c.set_sticker(2, 1, 1, 5);   // paint one F sticker as B
    EXPECT_FALSE(c.is_solved());

    c.set_sticker(2, 1, 1, 2);   // restore
    EXPECT_TRUE(c.is_solved());
}

TEST(NxNCubeAccessors, FaceDataMatchesRaw) {
    NxNCube c(5);
    uint8_t* raw = c.raw();
    for (int f = 0; f < NUM_FACES; ++f) {
        EXPECT_EQ(c.face_data(f), raw + f * c.face_size());
    }

    // mutate through face_data and read through sticker()
    c.face_data(1)[7] = 3;  // row 1 col 2 of face R (n=5)
    EXPECT_EQ(c.sticker(1, 1, 2), 3);
}

TEST(RotateFace, FourCWIsIdentity) {
    for (int n = 2; n <= 7; ++n) {
        for (int f = 0; f < NUM_FACES; ++f) {
            NxNCube c(n);
            paint_face_indexed(c, f);

            NxNCube before = c;
            for (int i = 0; i < 4; ++i) rotate_face(c, f, Turn::CW);

            for (int i = 0; i < c.num_stickers(); ++i) {
                EXPECT_EQ(c.raw()[i], before.raw()[i])
                    << "n=" << n << " f=" << f << " i=" << i;
            }
        }
    }
}

TEST(RotateFace, TwoCWEqualsHalf) {
    for (int n = 2; n <= 7; ++n) {
        NxNCube a(n), b(n);
        paint_face_indexed(a, 0);
        paint_face_indexed(b, 0);

        rotate_face(a, 0, Turn::CW);
        rotate_face(a, 0, Turn::CW);
        rotate_face(b, 0, Turn::Half);

        for (int i = 0; i < a.num_stickers(); ++i) {
            EXPECT_EQ(a.raw()[i], b.raw()[i]) << "n=" << n << " i=" << i;
        }
    }
}

TEST(RotateFace, CWThenCCWIsIdentity) {
    for (int n = 2; n <= 7; ++n) {
        NxNCube c(n);
        paint_face_indexed(c, 0);
        NxNCube before = c;

        rotate_face(c, 0, Turn::CW);
        rotate_face(c, 0, Turn::CCW);

        for (int i = 0; i < c.num_stickers(); ++i) {
            EXPECT_EQ(c.raw()[i], before.raw()[i]) << "n=" << n << " i=" << i;
        }
    }
}

TEST(RotateFace, CWMovesTopRowToRightColumn) {
    const int n = 4;
    NxNCube c(n);
    paint_face_indexed(c, 0);

    std::vector<uint8_t> before(n * n);
    for (int r = 0; r < n; ++r) {
        for (int col = 0; col < n; ++col) {
            before[r * n + col] = c.sticker(0, r, col);
        }
    }

    rotate_face(c, 0, Turn::CW);

    // every (r, c) should now live at (c, n-1-r)
    for (int r = 0; r < n; ++r) {
        for (int col = 0; col < n; ++col) {
            int nr = col;
            int nc = n - 1 - r;
            EXPECT_EQ(c.sticker(0, nr, nc), before[r * n + col])
                << "(r=" << r << ", c=" << col << ") -> (" << nr << "," << nc << ")";
        }
    }
}

TEST(OuterMove, FourCWIsIdentity) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (int n = 2; n <= 7; ++n) {
        for (Face f : faces) {
            NxNCube c(n);
            NxNCube before = c;
            for (int i = 0; i < 4; ++i) apply_outer_move(c, f, Turn::CW);
            for (int i = 0; i < c.num_stickers(); ++i) {
                EXPECT_EQ(c.raw()[i], before.raw()[i])
                    << "n=" << n << " face=" << static_cast<int>(f) << " i=" << i;
            }
        }
    }
}

TEST(OuterMove, CWThenCCWIsIdentity) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (int n = 2; n <= 7; ++n) {
        for (Face f : faces) {
            NxNCube c(n);
            NxNCube before = c;
            apply_outer_move(c, f, Turn::CW);
            apply_outer_move(c, f, Turn::CCW);
            for (int i = 0; i < c.num_stickers(); ++i) {
                EXPECT_EQ(c.raw()[i], before.raw()[i])
                    << "n=" << n << " face=" << static_cast<int>(f) << " i=" << i;
            }
        }
    }
}

TEST(OuterMove, TwoCWEqualsHalf) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (int n = 2; n <= 7; ++n) {
        for (Face f : faces) {
            NxNCube a(n), b(n);
            apply_outer_move(a, f, Turn::CW);
            apply_outer_move(a, f, Turn::CW);
            apply_outer_move(b, f, Turn::Half);
            for (int i = 0; i < a.num_stickers(); ++i) {
                EXPECT_EQ(a.raw()[i], b.raw()[i])
                    << "n=" << n << " face=" << static_cast<int>(f) << " i=" << i;
            }
        }
    }
}

TEST(OuterMove, DisturbsSolved) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (int n = 2; n <= 7; ++n) {
        for (Face f : faces) {
            NxNCube c(n);
            apply_outer_move(c, f, Turn::CW);
            EXPECT_FALSE(c.is_solved())
                << "n=" << n << " face=" << static_cast<int>(f);
        }
    }
}

TEST(OuterMove, SexyMoveOrderSixIdentity) {
    for (int n = 2; n <= 7; ++n) {
        NxNCube c(n);
        NxNCube before = c;
        for (int i = 0; i < 6; ++i) {
            apply_outer_move(c, Face::R, Turn::CW);
            apply_outer_move(c, Face::U, Turn::CW);
            apply_outer_move(c, Face::R, Turn::CCW);
            apply_outer_move(c, Face::U, Turn::CCW);
        }
        EXPECT_TRUE(c.is_solved()) << "n=" << n;
        for (int i = 0; i < c.num_stickers(); ++i) {
            EXPECT_EQ(c.raw()[i], before.raw()[i]) << "n=" << n << " i=" << i;
        }
    }
}

TEST(OuterMove, UClockwiseCycleOnSolvedCube) {
    NxNCube c(3);
    apply_outer_move(c, Face::U, Turn::CW);
    for (int col = 0; col < 3; ++col) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::F), 0, col), 1) << "F top col " << col;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::L), 0, col), 2) << "L top col " << col;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::B), 0, col), 4) << "B top col " << col;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::R), 0, col), 5) << "R top col " << col;
    }
}

TEST(OuterMove, DClockwiseCycleOnSolvedCube) {
    NxNCube c(3);
    apply_outer_move(c, Face::D, Turn::CW);
    for (int col = 0; col < 3; ++col) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::F), 2, col), 4) << "F bot col " << col;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::R), 2, col), 2) << "R bot col " << col;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::B), 2, col), 1) << "B bot col " << col;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::L), 2, col), 5) << "L bot col " << col;
    }
}

TEST(OuterMove, RClockwiseCycleOnSolvedCube) {
    NxNCube c(3);
    apply_outer_move(c, Face::R, Turn::CW);

    for (int r = 0; r < 3; ++r) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::F), r, 2), 3) << "F col 2 row " << r;
    }
    for (int r = 0; r < 3; ++r) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::U), r, 2), 2) << "U col 2 row " << r;
    }
    for (int r = 0; r < 3; ++r) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::B), r, 0), 0) << "B col 0 row " << r;
    }
    for (int r = 0; r < 3; ++r) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::D), r, 2), 5) << "D col 2 row " << r;
    }
}

TEST(WideMove, MatchesOuterAtDepthZero) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    const Turn turns[] = {Turn::CW, Turn::Half, Turn::CCW};
    for (int n = 2; n <= 7; ++n) {
        for (Face f : faces) {
            for (Turn t : turns) {
                NxNCube a(n), b(n);
                apply_outer_move(a, f, t);
                apply_wide_move(b, f, 0, 0, t);
                for (int i = 0; i < a.num_stickers(); ++i) {
                    EXPECT_EQ(a.raw()[i], b.raw()[i])
                        << "n=" << n << " f=" << static_cast<int>(f) << " t=" << static_cast<int>(t);
                }
            }
        }
    }
}

TEST(WideMove, FourCWIsIdentity) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (int n = 3; n <= 7; ++n) {
        for (Face f : faces) {
            for (int outer = 0; outer <= n / 2; ++outer) {
                for (int inner = outer; inner <= n / 2; ++inner) {
                    NxNCube c(n);
                    NxNCube before = c;
                    for (int i = 0; i < 4; ++i) apply_wide_move(c, f, outer, inner, Turn::CW);
                    for (int i = 0; i < c.num_stickers(); ++i) {
                        EXPECT_EQ(c.raw()[i], before.raw()[i])
                            << "n=" << n << " f=" << static_cast<int>(f)
                            << " outer=" << outer << " inner=" << inner << " i=" << i;
                    }
                }
            }
        }
    }
}

TEST(WideMove, CWThenCCWIsIdentity) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (int n = 3; n <= 7; ++n) {
        for (Face f : faces) {
            for (int outer = 0; outer <= n / 2; ++outer) {
                for (int inner = outer; inner <= n / 2; ++inner) {
                    NxNCube c(n);
                    NxNCube before = c;
                    apply_wide_move(c, f, outer, inner, Turn::CW);
                    apply_wide_move(c, f, outer, inner, Turn::CCW);
                    for (int i = 0; i < c.num_stickers(); ++i) {
                        EXPECT_EQ(c.raw()[i], before.raw()[i])
                            << "n=" << n << " f=" << static_cast<int>(f)
                            << " outer=" << outer << " inner=" << inner;
                    }
                }
            }
        }
    }
}

TEST(WideMove, TwoCWEqualsHalf) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    for (int n = 3; n <= 7; ++n) {
        for (Face f : faces) {
            for (int outer = 0; outer <= n / 2; ++outer) {
                for (int inner = outer; inner <= n / 2; ++inner) {
                    NxNCube a(n), b(n);
                    apply_wide_move(a, f, outer, inner, Turn::CW);
                    apply_wide_move(a, f, outer, inner, Turn::CW);
                    apply_wide_move(b, f, outer, inner, Turn::Half);
                    for (int i = 0; i < a.num_stickers(); ++i) {
                        EXPECT_EQ(a.raw()[i], b.raw()[i])
                            << "n=" << n << " f=" << static_cast<int>(f)
                            << " outer=" << outer << " inner=" << inner;
                    }
                }
            }
        }
    }
}

TEST(WideMove, RwOnSolved5x5MovesTwoColumns) {
    NxNCube c(5);
    apply_wide_move(c, Face::R, 0, 1, Turn::CW);

    for (int r = 0; r < 5; ++r) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::F), r, 4), 3) << "F col 4 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::F), r, 3), 3) << "F col 3 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::U), r, 4), 2) << "U col 4 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::U), r, 3), 2) << "U col 3 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::B), r, 0), 0) << "B col 0 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::B), r, 1), 0) << "B col 1 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::D), r, 4), 5) << "D col 4 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::D), r, 3), 5) << "D col 3 row " << r;
    }
}

TEST(WideMove, InnerSlice3ROnSolved5x5DoesNotRotateFace) {
    NxNCube c(5);
    apply_wide_move(c, Face::R, 2, 2, Turn::CW);

    // R face untouched; inner slice does not rotate the outer face
    for (int r = 0; r < 5; ++r) {
        for (int col = 0; col < 5; ++col) {
            EXPECT_EQ(c.sticker(static_cast<int>(Face::R), r, col), 1)
                << "R face touched at (" << r << "," << col << ")";
        }
    }

    for (int r = 0; r < 5; ++r) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::F), r, 4), 2) << "F col 4 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::F), r, 3), 2) << "F col 3 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::U), r, 4), 0) << "U col 4 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::U), r, 3), 0) << "U col 3 row " << r;
    }

    for (int r = 0; r < 5; ++r) {
        EXPECT_EQ(c.sticker(static_cast<int>(Face::F), r, 2), 3) << "F col 2 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::U), r, 2), 2) << "U col 2 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::D), r, 2), 5) << "D col 2 row " << r;
        EXPECT_EQ(c.sticker(static_cast<int>(Face::B), r, 2), 0) << "B col 2 row " << r;
    }
}

// helpers for the parse/format/scramble suites
namespace {

Turn inverse_turn(Turn t) {
    switch (t) {
        case Turn::CW:   return Turn::CCW;
        case Turn::CCW:  return Turn::CW;
        case Turn::Half: return Turn::Half;
    }
    return Turn::CW;
}

Move inverse_move(const Move& m) {
    return {m.face, m.outer_depth, m.inner_depth, inverse_turn(m.turn)};
}

}

TEST(ParseMove, RoundTripsCanonicalForms) {
    const char* canonical[] = {
        "R", "R'", "R2",
        "U", "F", "D", "L", "B",
        "Rw", "Rw'", "Rw2",
        "3Rw", "3Rw'", "3Rw2",
        "3R", "3R'", "3R2",
        "4Rw", "5Rw2",
    };
    for (const char* s : canonical) {
        auto m = parse_move(s);
        ASSERT_TRUE(m.has_value()) << "failed to parse: " << s;
        EXPECT_EQ(format_move(*m), s) << "round-trip mismatch for: " << s;
    }
}

TEST(ParseMove, BareWideEqualsTwoLayerWide) {
    auto a = parse_move("Rw");
    auto b = parse_move("2Rw");
    ASSERT_TRUE(a && b);
    EXPECT_EQ(a->face, b->face);
    EXPECT_EQ(a->outer_depth, b->outer_depth);
    EXPECT_EQ(a->inner_depth, b->inner_depth);
    EXPECT_EQ(a->turn, b->turn);
}

TEST(ParseMove, DepthSemanticsAreCorrect) {
    auto basic = parse_move("R");
    ASSERT_TRUE(basic);
    EXPECT_EQ(basic->outer_depth, 0);
    EXPECT_EQ(basic->inner_depth, 0);

    auto wide2 = parse_move("Rw");
    ASSERT_TRUE(wide2);
    EXPECT_EQ(wide2->outer_depth, 0);
    EXPECT_EQ(wide2->inner_depth, 1);

    auto wide3 = parse_move("3Rw");
    ASSERT_TRUE(wide3);
    EXPECT_EQ(wide3->outer_depth, 0);
    EXPECT_EQ(wide3->inner_depth, 2);

    auto inner3 = parse_move("3R");
    ASSERT_TRUE(inner3);
    EXPECT_EQ(inner3->outer_depth, 2);
    EXPECT_EQ(inner3->inner_depth, 2);
}

TEST(ParseMove, RejectsMalformed) {
    const char* bad[] = {
        "",       // empty
        "r",      // lowercase face
        "X",      // unknown face
        "RR",     // trailing junk
        "R3",     // invalid modifier
        "R''",    // double modifier
        "0R",     // depth 0 doesn't make sense as inner slice
        "1R",     // depth 1 == plain R, redundant prefix disallowed
        "w",      // no face
        "R w",    // whitespace inside a token
    };
    for (const char* s : bad) {
        EXPECT_FALSE(parse_move(s).has_value()) << "should have rejected: '" << s << "'";
    }
}

TEST(ParseScramble, SplitsAndPropagatesErrors) {
    auto ok = parse_scramble("R U R' U' Rw2 3Rw");
    ASSERT_TRUE(ok);
    EXPECT_EQ(ok->size(), 6u);

    auto empty = parse_scramble("");
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->empty());

    auto ws = parse_scramble("   \t  ");
    ASSERT_TRUE(ws);
    EXPECT_TRUE(ws->empty());

    auto bad = parse_scramble("R U r' U'");   // lowercase r is invalid
    EXPECT_FALSE(bad.has_value());
}

TEST(ApplyMove, DispatchesToWideMove) {
    const Face faces[] = {Face::U, Face::R, Face::F, Face::D, Face::L, Face::B};
    const Turn turns[] = {Turn::CW, Turn::Half, Turn::CCW};
    for (int n = 3; n <= 5; ++n) {
        for (Face f : faces) {
            for (Turn t : turns) {
                for (int outer = 0; outer <= n / 2; ++outer) {
                    for (int inner = outer; inner <= n / 2; ++inner) {
                        NxNCube a(n), b(n);
                        apply_move(a, Move{f, outer, inner, t});
                        apply_wide_move(b, f, outer, inner, t);
                        for (int i = 0; i < a.num_stickers(); ++i) {
                            EXPECT_EQ(a.raw()[i], b.raw()[i])
                                << "n=" << n << " f=" << static_cast<int>(f)
                                << " outer=" << outer << " inner=" << inner;
                        }
                    }
                }
            }
        }
    }
}

TEST(RandomScramble, HonorsLengthAndNoRepeats) {
    for (int n = 3; n <= 7; ++n) {
        auto seq = random_scramble(n, 40, 12345);
        EXPECT_EQ(static_cast<int>(seq.size()), 40);
        for (size_t i = 1; i < seq.size(); ++i) {
            EXPECT_NE(seq[i].face, seq[i - 1].face)
                << "n=" << n << " consecutive same-face at i=" << i;
        }
    }
}

TEST(RandomScramble, InverseUndoesScramble) {
    for (int n = 3; n <= 7; ++n) {
        NxNCube c(n);
        auto seq = random_scramble(n, 40, 42);
        for (const auto& m : seq) apply_move(c, m);
        for (auto it = seq.rbegin(); it != seq.rend(); ++it) apply_move(c, inverse_move(*it));
        EXPECT_TRUE(c.is_solved()) << "n=" << n;
    }
}

TEST(RandomScramble, EveryMoveIsFormatRoundTrippable) {
    for (int n = 3; n <= 7; ++n) {
        auto seq = random_scramble(n, 30, 99);
        for (const auto& m : seq) {
            std::string s = format_move(m);
            auto parsed = parse_move(s);
            ASSERT_TRUE(parsed.has_value()) << "format produced unparsable token: " << s;
            EXPECT_EQ(parsed->face, m.face);
            EXPECT_EQ(parsed->outer_depth, m.outer_depth);
            EXPECT_EQ(parsed->inner_depth, m.inner_depth);
            EXPECT_EQ(parsed->turn, m.turn);
        }
    }
}

namespace {

bool centers_solved(const NxNCube& c) {
    const int n = c.n();
    if (n < 3) return true;   // no interior on n=2
    for (int f = 0; f < NUM_FACES; ++f) {
        const uint8_t expected = static_cast<uint8_t>(f);
        for (int r = 1; r < n - 1; ++r) {
            for (int col = 1; col < n - 1; ++col) {
                if (c.sticker(f, r, col) != expected) return false;
            }
        }
    }
    return true;
}

}

TEST(LegalMoveSteps, ParityStageIsOnlyOuterTurns) {
    for (int n = 3; n <= 7; ++n) {
        auto steps = legal_move_steps_for_stage(n, Stage::Parity);
        EXPECT_EQ(steps.size(), 18u) << "n=" << n;
        for (const auto& step : steps) {
            ASSERT_EQ(step.size(), 1u);
            EXPECT_EQ(step[0].outer_depth, 0);
            EXPECT_EQ(step[0].inner_depth, 0);
        }
    }
}

TEST(LegalMoveSteps, CentersStageIncludesWidesAndSlicesForBigCubes) {
    auto s3 = legal_move_steps_for_stage(3, Stage::Centers);
    EXPECT_EQ(s3.size(), 18u);

    auto s4 = legal_move_steps_for_stage(4, Stage::Centers);
    EXPECT_EQ(s4.size(), 18u + 18u + 18u);
}

// the load-bearing safety invariant: every step in the Edges-stage move set
// must preserve centers when applied to a solved cube
TEST(LegalMoveSteps, EdgesStageStepsPreserveCenters) {
    for (int n = 4; n <= 7; ++n) {
        auto steps = legal_move_steps_for_stage(n, Stage::Edges);
        for (size_t i = 0; i < steps.size(); ++i) {
            NxNCube c(n);
            apply_move_step(c, steps[i]);
            EXPECT_TRUE(centers_solved(c))
                << "n=" << n << " step index=" << i
                << " (step length=" << steps[i].size() << ")";
        }
    }
}

// tighter check on the Edges move set for n=5: after every single legal step,
// the entire cube state is a permutation of the solved state's stickers per face
TEST(LegalMoveSteps, EdgesStagePreservesFaceColorMultisetsOnN5) {
    const int n = 5;
    auto steps = legal_move_steps_for_stage(n, Stage::Edges);
    for (size_t i = 0; i < steps.size(); ++i) {
        NxNCube c(n);
        apply_move_step(c, steps[i]);
        int face_totals[NUM_FACES][NUM_FACES] = {{0}};
        for (int f = 0; f < NUM_FACES; ++f) {
            for (int r = 0; r < n; ++r) {
                for (int col = 0; col < n; ++col) {
                    ++face_totals[f][c.sticker(f, r, col)];
                }
            }
        }
        int global[NUM_FACES] = {0};
        for (int f = 0; f < NUM_FACES; ++f) {
            for (int cl = 0; cl < NUM_FACES; ++cl) global[cl] += face_totals[f][cl];
        }
        for (int cl = 0; cl < NUM_FACES; ++cl) {
            EXPECT_EQ(global[cl], n * n) << "step " << i << " color " << cl;
        }
    }
}

TEST(ApplyMoveStep, MatchesSequentialApplyMove) {
    auto steps = legal_move_steps_for_stage(5, Stage::Edges);
    MoveStep conj;
    for (const auto& s : steps) if (s.size() == 3) { conj = s; break; }
    ASSERT_EQ(conj.size(), 3u);

    NxNCube a(5), b(5);
    apply_move_step(a, conj);
    for (const auto& m : conj) apply_move(b, m);
    for (int i = 0; i < a.num_stickers(); ++i) {
        EXPECT_EQ(a.raw()[i], b.raw()[i]) << "i=" << i;
    }
}

namespace {

// 64-bit fnv-1a hash of the full sticker array
uint64_t hash_full_state(const NxNCube& c) {
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint8_t* p = c.raw();
    const int n = c.num_stickers();
    for (int i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

struct MatchesTarget {
    NxNCube target;
    bool operator()(const NxNCube& c) const {
        return std::equal(c.raw(), c.raw() + c.num_stickers(), target.raw());
    }
};

}

TEST(ReduceBFS, EmptyPathWhenAlreadyAtGoal) {
    NxNCube start(3);
    auto moves = legal_move_steps_for_stage(3, Stage::Parity);
    auto goal = [](const NxNCube&) { return true; };
    auto result = reduce_bfs(start, goal, hash_full_state, moves, 5);
    EXPECT_TRUE(result.found);
    EXPECT_EQ(result.sequence.size(), 0u);
}

TEST(ReduceBFS, FindsOneStepSolution) {
    NxNCube start(3);
    NxNCube target = start;
    apply_move(target, Move{Face::R, 0, 0, Turn::CW});

    auto moves = legal_move_steps_for_stage(3, Stage::Parity);
    auto result = reduce_bfs(start, MatchesTarget{target}, hash_full_state, moves, 3);
    ASSERT_TRUE(result.found);
    EXPECT_EQ(result.sequence.size(), 1u);
    NxNCube reproduced(3);
    for (const auto& s : result.sequence) apply_move_step(reproduced, s);
    for (int i = 0; i < reproduced.num_stickers(); ++i) {
        EXPECT_EQ(reproduced.raw()[i], target.raw()[i]);
    }
}

TEST(ReduceBFS, FindsShortestTwoStepSolution) {
    NxNCube start(3);
    NxNCube target = start;
    apply_move(target, Move{Face::R, 0, 0, Turn::CW});
    apply_move(target, Move{Face::U, 0, 0, Turn::CW});

    auto moves = legal_move_steps_for_stage(3, Stage::Parity);
    auto result = reduce_bfs(start, MatchesTarget{target}, hash_full_state, moves, 4);
    ASSERT_TRUE(result.found);
    EXPECT_EQ(result.sequence.size(), 2u);
    NxNCube reproduced(3);
    for (const auto& s : result.sequence) apply_move_step(reproduced, s);
    for (int i = 0; i < reproduced.num_stickers(); ++i) {
        EXPECT_EQ(reproduced.raw()[i], target.raw()[i]);
    }
}

TEST(ReduceBFS, HonorsDepthBound) {
    NxNCube start(3);
    NxNCube target = start;
    apply_move(target, Move{Face::R, 0, 0, Turn::CW});
    apply_move(target, Move{Face::U, 0, 0, Turn::CW});
    apply_move(target, Move{Face::F, 0, 0, Turn::CW});

    auto moves = legal_move_steps_for_stage(3, Stage::Parity);
    auto result = reduce_bfs(start, MatchesTarget{target}, hash_full_state, moves, 2);
    EXPECT_FALSE(result.found);
}

TEST(ReduceBFS, HonorsNodeCap) {
    NxNCube start(3);
    NxNCube target = start;
    apply_move(target, Move{Face::R, 0, 0, Turn::CW});
    apply_move(target, Move{Face::U, 0, 0, Turn::CW});
    apply_move(target, Move{Face::F, 0, 0, Turn::CW});

    auto moves = legal_move_steps_for_stage(3, Stage::Parity);
    auto result = reduce_bfs(start, MatchesTarget{target}, hash_full_state, moves, 5, /*max_nodes=*/50);
    EXPECT_FALSE(result.found);
    EXPECT_LE(result.nodes_explored, 50u + moves.size());   // may overshoot slightly per outer iteration
}

namespace {

bool all_centers_solved_n4(const NxNCube& c) {
    if (c.n() != 4) return false;
    const int slots[4][2] = {{1,1}, {1,2}, {2,1}, {2,2}};
    for (int f = 0; f < NUM_FACES; ++f) {
        const uint8_t expected = static_cast<uint8_t>(f);
        for (const auto& s : slots) {
            if (c.sticker(f, s[0], s[1]) != expected) return false;
        }
    }
    return true;
}

}

TEST(SolveCentersN4, MonochromeAfterShortScrambles) {
    for (uint64_t seed = 1; seed <= 3; ++seed) {
        NxNCube c(4);
        auto scramble = random_scramble(4, 8, seed);
        for (const auto& m : scramble) apply_move(c, m);

        auto sequence = solve_centers_n4(c);
        EXPECT_FALSE(sequence.empty()) << "seed=" << seed << " (BFS failed)";
        EXPECT_TRUE(all_centers_solved_n4(c))
            << "seed=" << seed << " (centers not monochrome after solve)";
    }
}

TEST(CollapseRedundantMoves, InverseQuarterTurnsCancel) {
    std::vector<Move> input = {
        {Face::U, 0, 1, Turn::CW},
        {Face::U, 0, 1, Turn::CCW},
    };
    auto out = collapse_redundant_moves(input);
    EXPECT_TRUE(out.empty());
}

TEST(CollapseRedundantMoves, TwoQuartersEqualHalf) {
    std::vector<Move> input = {
        {Face::F, 0, 1, Turn::CCW},
        {Face::F, 0, 1, Turn::CCW},
    };
    auto out = collapse_redundant_moves(input);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].turn, Turn::Half);
    EXPECT_EQ(out[0].face, Face::F);
    EXPECT_EQ(out[0].outer_depth, 0);
    EXPECT_EQ(out[0].inner_depth, 1);
}

TEST(CollapseRedundantMoves, ThreeQuartersEqualPrime) {
    std::vector<Move> input = {
        {Face::R, 0, 0, Turn::CW},
        {Face::R, 0, 0, Turn::CW},
        {Face::R, 0, 0, Turn::CW},
    };
    auto out = collapse_redundant_moves(input);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].turn, Turn::CCW);
    EXPECT_EQ(out[0].face, Face::R);
}

TEST(CollapseRedundantMoves, CascadingCollapse) {
    std::vector<Move> input = {
        {Face::R, 0, 0, Turn::CW},
        {Face::R, 0, 0, Turn::CW},
        {Face::R, 0, 0, Turn::CCW},
    };
    auto out = collapse_redundant_moves(input);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].turn, Turn::CW);
    EXPECT_EQ(out[0].face, Face::R);
}

TEST(CollapseRedundantMoves, DifferentLayersDoNotCollapse) {
    std::vector<Move> input = {
        {Face::R, 0, 0, Turn::CW},   // R
        {Face::R, 0, 1, Turn::CW},   // Rw
    };
    auto out = collapse_redundant_moves(input);
    EXPECT_EQ(out.size(), 2u);
}

TEST(CollapseRedundantMoves, ApplyingCollapsedMatchesOriginal) {
    for (uint64_t seed = 1; seed <= 3; ++seed) {
        NxNCube scratch(4);
        auto scramble = random_scramble(4, 8, seed);
        for (const auto& m : scramble) apply_move(scratch, m);
        auto steps = solve_centers_n4(scratch);
        ASSERT_FALSE(steps.empty());

        std::vector<Move> raw;
        for (const auto& s : steps) for (const auto& m : s) raw.push_back(m);

        auto collapsed = collapse_redundant_moves(raw);
        EXPECT_LE(collapsed.size(), raw.size());

        NxNCube verify(4);
        for (const auto& m : scramble) apply_move(verify, m);
        for (const auto& m : collapsed) apply_move(verify, m);

        for (int i = 0; i < verify.num_stickers(); ++i) {
            EXPECT_EQ(verify.raw()[i], scratch.raw()[i])
                << "seed=" << seed << " sticker index=" << i;
        }
    }
}

TEST(SolveEdgesN4Algo, SmokeTestSmallScramble) {
    NxNCube cube(4);
    auto scramble = random_scramble(4, 3, 7);
    for (const auto& m : scramble) apply_move(cube, m);

    auto centers_seq = solve_centers_n4(cube);
    ASSERT_FALSE(centers_seq.empty()) << "stage 1 failed";
    ASSERT_TRUE(all_centers_solved_n4(cube)) << "stage 1 left centers broken";

    auto edges = solve_edges_n4_algo(cube);
    EXPECT_GE(edges.edges_paired, 11) << "expected >=11 paired, got " << edges.edges_paired;
    EXPECT_TRUE(all_centers_solved_n4(cube)) << "algo stage 2 broke centers";
}

TEST(SolveEdgesN4Algo, MediumScrambleAcrossSeeds) {
    for (uint64_t seed = 1; seed <= 5; ++seed) {
        NxNCube cube(4);
        auto scramble = random_scramble(4, 8, seed);
        for (const auto& m : scramble) apply_move(cube, m);

        auto centers_seq = solve_centers_n4(cube);
        ASSERT_FALSE(centers_seq.empty()) << "stage 1 failed seed=" << seed;
        ASSERT_TRUE(all_centers_solved_n4(cube)) << "stage 1 broke centers seed=" << seed;

        auto edges = solve_edges_n4_algo(cube);
        EXPECT_GE(edges.edges_paired, 11) << "seed=" << seed;
        EXPECT_TRUE(all_centers_solved_n4(cube)) << "algo stage 2 broke centers seed=" << seed;
    }
}

TEST(SolveEdgesN4Algo, PropertyManySeeds) {
    if (std::getenv("CUBE_N4_HEAVY") == nullptr) {
        GTEST_SKIP() << "set CUBE_N4_HEAVY=1 to run the 40-trial property test";
    }
    const int TRIALS = 40;
    const int SCRAMBLE_LEN = 30;
    int total_paired_12 = 0;
    int total_paired_11 = 0;
    int failures = 0;
    for (int t = 0; t < TRIALS; ++t) {
        uint64_t seed = 1000ULL + t;
        NxNCube cube(4);
        auto scramble = random_scramble(4, SCRAMBLE_LEN, seed);
        for (const auto& m : scramble) apply_move(cube, m);

        auto centers_seq = solve_centers_n4(cube);
        if (centers_seq.empty() || !all_centers_solved_n4(cube)) {
            ADD_FAILURE() << "stage 1 failed seed=" << seed;
            ++failures;
            continue;
        }

        auto edges = solve_edges_n4_algo(cube);
        if (!all_centers_solved_n4(cube)) {
            ADD_FAILURE() << "algo stage 2 broke centers seed=" << seed;
            ++failures;
            continue;
        }
        if (edges.edges_paired == 12) ++total_paired_12;
        else if (edges.edges_paired == 11) ++total_paired_11;
        else {
            ADD_FAILURE() << "seed=" << seed << " paired=" << edges.edges_paired;
            ++failures;
        }
    }
    std::cerr << "[PropertyManySeeds] 12-paired=" << total_paired_12
              << " 11-paired=" << total_paired_11
              << " failures=" << failures << " / " << TRIALS << "\n";
    EXPECT_EQ(failures, 0);
    EXPECT_EQ(total_paired_12 + total_paired_11, TRIALS);
}

TEST(SolveEdgesN4Algo, HeavyProperty) {
    if (std::getenv("CUBE_N4_HEAVY") == nullptr) {
        GTEST_SKIP() << "set CUBE_N4_HEAVY=1 to run the 200-trial property test";
    }
    const int TRIALS = 200;
    const int SCRAMBLE_LEN = 30;
    int total_paired_12 = 0;
    int total_paired_11 = 0;
    int failures = 0;
    for (int t = 0; t < TRIALS; ++t) {
        uint64_t seed = 500000ULL + t;
        NxNCube cube(4);
        auto scramble = random_scramble(4, SCRAMBLE_LEN, seed);
        for (const auto& m : scramble) apply_move(cube, m);

        auto centers_seq = solve_centers_n4(cube);
        if (centers_seq.empty() || !all_centers_solved_n4(cube)) {
            ADD_FAILURE() << "stage 1 failed seed=" << seed;
            ++failures;
            continue;
        }

        EdgePairResult edges = solve_edges_n4_algo(cube);
        if (!all_centers_solved_n4(cube)) {
            ADD_FAILURE() << "algo stage 2 broke centers seed=" << seed;
            ++failures;
            continue;
        }
        if (edges.edges_paired == 12) ++total_paired_12;
        else if (edges.edges_paired == 11) ++total_paired_11;
        else {
            ADD_FAILURE() << "seed=" << seed << " paired=" << edges.edges_paired;
            ++failures;
        }
    }
    std::cerr << "[HeavyProperty] 12-paired=" << total_paired_12
              << " 11-paired=" << total_paired_11
              << " failures=" << failures << " / " << TRIALS << "\n";
    EXPECT_EQ(failures, 0);
}

namespace {

void induce_parity_variant_a(NxNCube& c) {
    const uint8_t a = c.sticker(static_cast<int>(Face::L), 1, 3);
    const uint8_t b = c.sticker(static_cast<int>(Face::F), 1, 0);
    c.set_sticker(static_cast<int>(Face::L), 1, 3, b);
    c.set_sticker(static_cast<int>(Face::F), 1, 0, a);
}

}

TEST(SolveEdgesN4Algo, HandConstructedParityVariantA) {
    NxNCube cube(4);
    induce_parity_variant_a(cube);
    ASSERT_TRUE(all_centers_solved_n4(cube)) << "hand-construction touched centers";

    auto edges = solve_edges_n4_algo(cube);
    EXPECT_EQ(edges.edges_paired, 11)
        << "expected exact 11 (OLL-parity code path), got " << edges.edges_paired;
    EXPECT_TRUE(all_centers_solved_n4(cube)) << "algo broke centers on parity input";
}