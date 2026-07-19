#include <gtest/gtest.h>

#include "cube_nxn/cube_nxn.h"

using cube_nxn::Face;
using cube_nxn::NxNCube;
using cube_nxn::NUM_FACES;

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