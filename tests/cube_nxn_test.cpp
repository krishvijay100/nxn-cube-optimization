#include <gtest/gtest.h>

#include "cube_nxn/cube_nxn.h"

using cube_nxn::Face;
using cube_nxn::NxNCube;
using cube_nxn::NUM_FACES;
using cube_nxn::Turn;
using cube_nxn::rotate_face;
using cube_nxn::apply_outer_move;

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