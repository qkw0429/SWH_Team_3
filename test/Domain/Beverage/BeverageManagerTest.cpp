#include "Domain/Beverage/BeverageManager.h"
#include "Exception/CustomException.h"
#include <gtest/gtest.h>

using namespace customException;

class BeverageManagerTest : public ::testing::Test {
protected:
    BeverageManager manager;
    Beverage beverage1;
    Beverage beverage2;

    void SetUp() override {
        beverage1 = Beverage(1, "콜라", 5, 1000);
        beverage2 = Beverage(2, "사이다", 10, 500);
        manager.addBeverage(beverage1);
        manager.addBeverage(beverage2);
    }
};

TEST_F(BeverageManagerTest, GetBeverage_성공_Beverage반환) {
    Beverage result = manager.getBeverage(1);
    EXPECT_EQ(result.getId(), 1);
}

TEST_F(BeverageManagerTest, GetBeverage_실패_문자입력_예외) {
    EXPECT_THROW(manager.getBeverage('a'), NotFoundException);
}

TEST_F(BeverageManagerTest, GetBeverage_실패_음수입력_예외) {
    EXPECT_THROW(manager.getBeverage(-1), NotFoundException);
}

TEST_F(BeverageManagerTest, GetBeverage_실패_0입력_예외) {
    EXPECT_THROW(manager.getBeverage(0), NotFoundException);
}

TEST_F(BeverageManagerTest, GetBeverage_실패_초과입력_예외) {
    EXPECT_THROW(manager.getBeverage(999), NotFoundException);
}

TEST_F(BeverageManagerTest, HasEnoughStock_0인경우_TRUE반환) {
    EXPECT_TRUE(manager.hasEnoughStock(1, 0));
}

TEST_F(BeverageManagerTest, HasEnoughStock_재고보다작을경우_TRUE반환) {
    EXPECT_TRUE(manager.hasEnoughStock(1, 3));
}

TEST_F(BeverageManagerTest, HasEnoughStock_재고와같을경우_TRUE반환) {
    EXPECT_TRUE(manager.hasEnoughStock(2, 10));
}

TEST_F(BeverageManagerTest, HasEnoughStock_재고보다클경우_FALSE반환) {
    EXPECT_FALSE(manager.hasEnoughStock(2, 20));
}

TEST_F(BeverageManagerTest, HasEnoughStock_음수요청이면_TRUE반환) {
    // 현재 구현상은 음수 요청에 대해 TRUE를 반환하도록 되어 있음
    // TODO : exception throw 하도록 수정 필요
    EXPECT_TRUE(manager.hasEnoughStock(1, -5));
}

TEST_F(BeverageManagerTest, HasEnoughStock_존재하지않는ID이면_예외) {
    EXPECT_THROW(manager.hasEnoughStock(999, 1), NotFoundException);
    EXPECT_THROW(manager.hasEnoughStock(0, 1), NotFoundException);
    EXPECT_THROW(manager.hasEnoughStock(-1, 1), NotFoundException);
}

TEST_F(BeverageManagerTest, ReduceQuantity_0인경우_TRUE반환) {
    EXPECT_TRUE(manager.reduceQuantity(1, 0));
}

TEST_F(BeverageManagerTest, ReduceQuantity_재고보다작을경우_TRUE반환) {
    EXPECT_TRUE(manager.reduceQuantity(1, 3));
}

TEST_F(BeverageManagerTest, ReduceQuantity_재고와같을경우_TRUE반환) {
    EXPECT_TRUE(manager.reduceQuantity(2, 10));
}

TEST_F(BeverageManagerTest, ReduceQuantity_음수요청이면_FALSE반환) {
    EXPECT_FALSE(manager.reduceQuantity(1, -2));
}

TEST_F(BeverageManagerTest, ReduceQuantity_존재하지않는ID이면_예외) {
    EXPECT_THROW(manager.reduceQuantity(999, 1), NotFoundException);
    EXPECT_THROW(manager.reduceQuantity(0, 1), NotFoundException);
    EXPECT_THROW(manager.reduceQuantity(-1, 1), NotFoundException);
}
