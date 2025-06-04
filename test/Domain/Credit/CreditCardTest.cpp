#include "Domain/Credit/CreditCard.h"
#include "Domain/Credit/Bank.h"
#include "Exception/CustomException.h"
#include <gtest/gtest.h>

using namespace customException;

class CreditCardTest : public ::testing::Test {
protected:
    CreditCard* card;

    void SetUp() override {
        card = new CreditCard("1234567890", 5000);
    }
};

TEST_F(CreditCardTest, ValidateBalance_0_성공) {
    EXPECT_TRUE(card->validateBalance(0));
}

TEST_F(CreditCardTest, ValidateBalance_유효한값_성공) {
    EXPECT_TRUE(card->validateBalance(3000));
}

TEST_F(CreditCardTest, ValidateBalance_경곗값_max_성공) {
    EXPECT_TRUE(card->validateBalance(5000));
}

TEST_F(CreditCardTest, ValidateBalance_경계값_5001_예외) {
    EXPECT_FALSE(card->validateBalance(5001));
}

TEST_F(CreditCardTest, ValidateBalance_초과값_예외) {
    EXPECT_FALSE(card->validateBalance(99999));
}

TEST_F(CreditCardTest, ReduceBalance_잔액감소) {
    card->reduceBalance(1000);
    EXPECT_TRUE(card->validateBalance(4000)); // 5000 - 1000 = 4000
}

TEST_F(CreditCardTest, IsValid_있는번호_성공) {
    EXPECT_TRUE(card->isValid());
}

TEST_F(CreditCardTest, IsValid_없는번호_실패) {
    CreditCard* emptyCard = new CreditCard("", 1000);
    EXPECT_FALSE(emptyCard->isValid());
}