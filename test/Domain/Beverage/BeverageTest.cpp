#include "Domain/Beverage/Beverage.h"
#include <gtest/gtest.h>

TEST(BeverageTest, Beverage_getId_성공) {
    Beverage b(42, "Cola", 10, 1500);
    EXPECT_EQ(b.getId(), 42);
}

TEST(BeverageTest, Beverage_getPrice_성공) {
    Beverage b(42, "Cola", 10, 1500);
    EXPECT_EQ(b.getPrice(), 1500);
}

TEST(BeverageTest, hasEnoughStock_0이면_TRUE반환) {
    Beverage b(1, "Test", 5, 1000);
    EXPECT_TRUE(b.hasEnoughStock(0));
}

TEST(BeverageTest, hasEnoughStock_재고보다작으면_TRUE반환) {
    Beverage b(1, "Test", 5, 1000);
    EXPECT_TRUE(b.hasEnoughStock(3));
}

TEST(BeverageTest, hasEnoughStock_재고보다_1만큼_작아도_TRUE반환_경계값_테스트) {
    Beverage b(1, "Test", 5, 1000);
    EXPECT_TRUE(b.hasEnoughStock(4));
}

TEST(BeverageTest, hasEnoughStock_재고와같으면_TRUE반환) {
    Beverage b(1, "Test", 5, 1000);
    EXPECT_TRUE(b.hasEnoughStock(5));
}

TEST(BeverageTest, hasEnoughStock_재고보다크면_FALSE반환) {
    Beverage b(1, "Test", 5, 1000);
    EXPECT_FALSE(b.hasEnoughStock(8));
}

TEST(BeverageTest, hasEnoughStock_재고보다_1만큼_커도_FALSE반환_경계값_테스트) {
    Beverage b(1, "Test", 5, 1000);
    EXPECT_FALSE(b.hasEnoughStock(6));
}

TEST(BeverageTest, hasEnoughStock_음수요청이면_TRUE반환) {
    // 현재 구현상은 음수 요청에 대해 TRUE를 반환하도록 되어 있음
    // TODO : exception throw 하도록 수정 필요
    Beverage b(1, "Test", 5, 1000);
    EXPECT_TRUE(b.hasEnoughStock(-1));
}

TEST(BeverageTest, reduceQuantity_0이면_TRUE반환) {
    Beverage b(2, "Tea", 7, 900);
    EXPECT_TRUE(b.reduceQuantity(0));
}

TEST(BeverageTest, reduceQuantity_재고보다_작으면_TRUE반환) {
    Beverage b(2, "Tea", 7, 900);
    EXPECT_TRUE(b.reduceQuantity(5));
}

TEST(BeverageTest, reduceQuantity_재고보다_1만큼_작아도_TRUE반환) {
    Beverage b(2, "Tea", 7, 900);
    EXPECT_TRUE(b.reduceQuantity(6));
}

TEST(BeverageTest, reduceQuantity_재고와_같으면_TRUE반환) {
    Beverage b(2, "Tea", 7, 900);
    EXPECT_TRUE(b.reduceQuantity(7));
}

TEST(BeverageTest, reduceQuantity_재고보다_크면_FALSE반환) {
    Beverage b(2, "Tea", 7, 900);
    EXPECT_FALSE(b.reduceQuantity(10));
}

TEST(BeverageTest, reduceQuantity_재고보다_1만큼_커도_FALSE반환) {
    Beverage b(2, "Tea", 7, 900);
    EXPECT_FALSE(b.reduceQuantity(8));
}
