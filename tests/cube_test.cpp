#include <gtest/gtest.h>
#include "cube/cube.h"

TEST(Placeholder, AddsTwoNumbers) {
    EXPECT_EQ(cube::placeholder_add(2, 3), 5);
    EXPECT_EQ(cube::placeholder_add(-1, 1), 0);
}
