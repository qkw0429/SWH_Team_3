#include <gtest/gtest.h>
#include "Domain/Credit/Bank.h"
#include "Domain/Credit/CreditCard.h"
#include "Exception/CustomException.h"

#include <fstream>

using namespace customException;

class BankTest : public ::testing::Test {
protected:
    Bank bank;

    void SetUp() override {
        // 테스트 전용 카드 데이터 생성
        ofstream file("card_db.txt");
        file << "1234-5678-9012-3456 15000\n";
        file << "9876-5432-1098-7654 5000\n";
        file << "0000-0000-0000-0000 0\n";
        file.close();
    }

    void TearDown() override {
        // 테스트 후 카드 데이터 삭제
        remove("card_db.txt");
    }
};

TEST_F(BankTest, RequestCard_있는번호_조회성공) {
    CreditCard* card = bank.requestCard("1234-5678-9012-3456");
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->getCardNumber(), "1234-5678-9012-3456");
    delete card;
}

TEST_F(BankTest, RequestCard_없는번호_조회실패) {
    EXPECT_THROW(bank.requestCard("1111-2222-3333-4444"), NotFoundException);
}

TEST_F(BankTest, SaveCreditCard_기존카드_잔액업데이트) {
    // 기존 카드의 잔액을 변경하고 저장한 뒤, requestCard로 확인
    CreditCard updatedCard("1234-5678-9012-3456", 20000);
    bank.saveCreditCard(updatedCard);

    CreditCard* card = bank.requestCard("1234-5678-9012-3456");
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->getCardNumber(), "1234-5678-9012-3456");
    EXPECT_EQ(card->getBalance(), 20000);
    delete card;
}

TEST_F(BankTest, SaveCreditCard_신규카드_추가후조회) {
    // DB에 없는 새로운 카드 정보를 저장하면, 이후 조회가 가능해야 한다
    CreditCard newCard("1111-2222-3333-4444", 3000);
    bank.saveCreditCard(newCard);

    CreditCard* card = bank.requestCard("1111-2222-3333-4444");
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->getCardNumber(), "1111-2222-3333-4444");
    EXPECT_EQ(card->getBalance(), 3000);
    delete card;
}

TEST_F(BankTest, RequestCard_파일열기실패_예외발생) {
    // DB 파일을 삭제한 뒤 조회를 시도하면 FileOpenException 발생
    remove("card_db.txt");
    EXPECT_THROW(bank.requestCard("1234-5678-9012-3456"), FileOpenException);
}
