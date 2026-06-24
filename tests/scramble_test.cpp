#include <gtest/gtest.h>

#include "cube/cube.h"
#include "cube/scramble.h"
#include "cube_test_helpers.h"

namespace {

uint8_t face_of(cube::Move m) {
    return static_cast<uint8_t>(m) / 3;
}

cube::Move invert(cube::Move m) {
    uint8_t v = static_cast<uint8_t>(m);
    uint8_t face = v / 3;
    uint8_t variant = v % 3;
    // 0 (quarter) <-> 1 (prime); 2 (half) is its own inverse.
    uint8_t inv_variant = variant == 0 ? 1 : (variant == 1 ? 0 : 2);
    return static_cast<cube::Move>(face * 3 + inv_variant);
}

void apply_all(cube::CubeState& c, const std::vector<cube::Move>& seq) {
    for (auto m : seq) cube::apply_move(c, m);
}

}  // namespace

TEST(Scramble, ReturnsRequestedLength) {
    auto s = cube::random_scramble(25, 42);
    EXPECT_EQ(s.size(), 25u);
}

TEST(Scramble, ZeroLengthReturnsEmpty) {
    auto s = cube::random_scramble(0, 42);
    EXPECT_TRUE(s.empty());
}

TEST(Scramble, NegativeLengthReturnsEmpty) {
    auto s = cube::random_scramble(-5, 42);
    EXPECT_TRUE(s.empty());
}

TEST(Scramble, SameSeedSameSequence) {
    auto a = cube::random_scramble(25, 12345);
    auto b = cube::random_scramble(25, 12345);
    EXPECT_EQ(a, b);
}

TEST(Scramble, DifferentSeedsLikelyDiffer) {
    auto a = cube::random_scramble(25, 1);
    auto b = cube::random_scramble(25, 2);
    EXPECT_NE(a, b);
}

TEST(Scramble, NoConsecutiveSameFace) {
    auto s = cube::random_scramble(100, 9999);
    for (size_t i = 1; i < s.size(); ++i) {
        EXPECT_NE(face_of(s[i]), face_of(s[i - 1]))
            << "same-face repeat at index " << i;
    }
}

TEST(Scramble, ProducesUnsolvedState) {
    auto s = cube::random_scramble(25, 42);
    cube::CubeState c = cube::solved_cube();
    apply_all(c, s);
    EXPECT_FALSE(cube::is_solved(c));
}

TEST(Scramble, ScrambleThenInverseReturnsToSolved) {
    auto s = cube::random_scramble(25, 42);

    cube::CubeState c = cube::solved_cube();
    apply_all(c, s);

    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        cube::apply_move(c, invert(*it));
    }

    EXPECT_TRUE(cube::is_solved(c));
}

TEST(Scramble, ConservationLawsHoldAcrossManySeeds) {
    for (uint64_t seed = 1; seed <= 200; ++seed) {
        auto s = cube::random_scramble(25, seed);
        cube::CubeState c = cube::solved_cube();
        apply_all(c, s);
        EXPECT_TRUE(cube_test::laws_hold(c))
            << "conservation laws broken for seed " << seed;
    }
}

TEST(Scramble, NoConsecutiveSameFaceAcrossManySeeds) {
    for (uint64_t seed = 1; seed <= 100; ++seed) {
        auto s = cube::random_scramble(50, seed);
        for (size_t i = 1; i < s.size(); ++i) {
            ASSERT_NE(face_of(s[i]), face_of(s[i - 1]))
                << "same-face repeat at index " << i << " for seed " << seed;
        }
    }
}

// Across enough total moves, every one of the 18 moves must appear at least
// once. Catches bugs like "a face or variant is unreachable from the RNG."
TEST(Scramble, AllMovesAppearAcrossManyScrambles) {
    std::array<int, 18> counts{};
    for (uint64_t seed = 1; seed <= 50; ++seed) {
        auto s = cube::random_scramble(25, seed);
        for (auto m : s) counts[static_cast<uint8_t>(m)]++;
    }
    for (uint8_t i = 0; i < 18; ++i) {
        EXPECT_GT(counts[i], 0)
            << "move index " << static_cast<int>(i) << " never appeared";
    }
}
