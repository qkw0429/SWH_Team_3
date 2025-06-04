#include "Domain/Location/Location.h"
#include <cmath>
#include <gtest/gtest.h>

TEST(LocationTest, getX_정상작동) {
    Location loc(7, -3);
    EXPECT_EQ(loc.getX(), 7);
}

TEST(LocationTest, getY_정상작동) {
    Location loc(7, -3);
    EXPECT_EQ(loc.getY(), -3);
}

TEST(LocationTest, distanceTo_같은좌표_0반환) {
    Location loc(5, 5);
    EXPECT_DOUBLE_EQ(loc.distanceTo(5, 5), 0.0);
}

TEST(LocationTest, distanceTo_가로이동_거리계산) {
    Location loc(10, 2);
    EXPECT_DOUBLE_EQ(loc.distanceTo(4, 2), 6.0);
}

TEST(LocationTest, distanceTo_세로이동_거리계산) {
    Location loc(-1, 8);
    EXPECT_DOUBLE_EQ(loc.distanceTo(-1, 3), 5.0);
}

TEST(LocationTest, distanceTo_대각선이동_거리계산) {
    Location loc(0, 0);
    EXPECT_DOUBLE_EQ(loc.distanceTo(3, 4), 5.0);
}

TEST(LocationTest, distanceTo_음수좌표_거리계산) {
    Location loc(-2, -3);
    // dx = -2 - 1 = -3, dy = -3 - 1 = -4 → sqrt(9 + 16) = 5
    EXPECT_DOUBLE_EQ(loc.distanceTo(1, 1), 5.0);
}

TEST(LocationTest, distanceTo_큰차이_거리계산) {
    Location loc(1000, -1000);
    double expected = std::sqrt(std::pow(1000 + 500, 2) + std::pow(-1000 - 200, 2));
    EXPECT_DOUBLE_EQ(loc.distanceTo(-500, 200), expected);
}
