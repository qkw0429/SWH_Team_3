#include "Services/Controller/RequestPaymentController.h"
#include "Domain/Beverage/BeverageManager.h"
#include "Domain/Credit/Bank.h"
#include "Domain/Credit/CreditCard.h"
#include <gtest/gtest.h>
#include <deque>
#include <queue>
#include <vector>

using namespace std;
using namespace customException;

// 1) 입력을 시뮬레이션하기 위한 서브클래스
class TestableRequestPaymentController : public RequestPaymentController {
private:
    queue<string> mockInputs;

public:
    TestableRequestPaymentController(BeverageManager* bm, Bank* b)
      : RequestPaymentController(bm, b) {}

    void setMockInputs(const vector<string>& inputs) {
        mockInputs = queue<string>(deque<string>(inputs.begin(), inputs.end()));
    }

protected:
    // override 가능한 virtual 함수여야 합니다!
    string inputCardNumber() override {
        if (mockInputs.empty()) return "";
        string s = mockInputs.front();
        mockInputs.pop();
        return s;
    }
};

// 2) BeverageManager 스텁: getBeverage, reduceQuantity를 제어
class StubBeverageManager : public BeverageManager {
private:
    Beverage bev;
    bool reduceResult;

public:
    StubBeverageManager(const Beverage& b, bool reduceResult = true)
      : bev(b), reduceResult(reduceResult) {}

    Beverage getBeverage(int beverageId) override {
        return bev;
    }

    bool reduceQuantity(int beverageId, int quantity) override {
        return reduceResult;
    }
};

// 3) Bank 스텁: requestCard, saveCreditCard를 제어
class StubBank : public Bank {
private:
    CreditCard* cardToReturn;
    bool throwNotFound;
    vector<CreditCard> savedCards;

public:
    // cardToReturn이 nullptr이면 NotFoundException 던짐
    StubBank(CreditCard* cardToReturn, bool throwNotFound = false)
      : cardToReturn(cardToReturn), throwNotFound(throwNotFound) {}

    CreditCard* requestCard(const string cardNumber) override {
        if (throwNotFound) {
            throw NotFoundException("카드가 없습니다");
        }
        return cardToReturn;
    }

    void saveCreditCard(const CreditCard card) override {
        savedCards.push_back(card);
    }

    const vector<CreditCard>& getSavedCards() const {
        return savedCards;
    }
};

class RequestPaymentControllerTest : public ::testing::Test {
protected:
    // 공통으로 쓰일 객체들
    Beverage testBev{1, "테스트음료", 1500, 10};
    StubBeverageManager* bevMgr;
    CreditCard* realCard;
    StubBank* bank;
    TestableRequestPaymentController* controller;

    void SetUp() override {
        // 잔액 10000짜리 카드, 정상 반환
        realCard = new CreditCard("1111-2222-3333-4444", 10000);
        bevMgr   = new StubBeverageManager(testBev, /*reduceResult=*/true);
        bank     = new StubBank(realCard, /*throwNotFound=*/false);
        controller = new TestableRequestPaymentController(bevMgr, bank);
    }

    void TearDown() override {
        delete controller;
        delete bevMgr;
        delete bank;
        delete realCard;
    }
};

// 성공 케이스: 유효한 카드, 잔액 충분, 재고 차감 성공 → Beverage 리턴
TEST_F(RequestPaymentControllerTest, 성공_유효카드_잔액충분_재고차감성공) {
    controller->setMockInputs({"1111-2222-3333-4444"});
    Beverage result = controller->enterCardNumber("ignored", testBev.getId(), 2);
    EXPECT_EQ(result.getId(), testBev.getId());
    // saveCreditCard() 호출됐는지 확인
    EXPECT_EQ(bank->getSavedCards().size(), 1);
}

// 카드 3회 모두 잘못 입력 → CardNotFoundException
TEST_F(RequestPaymentControllerTest, 카드3회실패_예외발생) {
    // “requestCard”를 호출할 때마다 NotFoundException을 던지도록 bank stub 생성
    StubBank failingBank(/*cardToReturn=*/nullptr, /*throwNotFound=*/true);
    TestableRequestPaymentController failingController(bevMgr, &failingBank);

    failingController.setMockInputs({"bad1", "bad2", "bad3"});
    EXPECT_THROW(
        failingController.enterCardNumber("ignored", testBev.getId(), 1),
        RequestPaymentController::CardNotFoundException
    );
}

// 잔액 부족 → InsufficientBalanceException
TEST_F(RequestPaymentControllerTest, 잔액부족_예외발생) {
    // 1) 잔액이 부족하도록 validateBalance만 false를 반환하게끔 CreditCard 서브클래스
    class LowBalanceCard : public CreditCard {
    public:
        LowBalanceCard(const string& num, int bal) : CreditCard(num, bal) {}
        bool validateBalance(int price) override { return false; }
    };

    // 2) LowBalanceCard 인스턴스와 bank stub 생성
    LowBalanceCard lowCard("1111-2222-3333-4444", /*balance=*/1000);
    StubBank lowBank(&lowCard, /*throwNotFound=*/false);

    // 3) 테스트 전용 컨트롤러에 주입
    TestableRequestPaymentController lowController(bevMgr, &lowBank);
    lowController.setMockInputs({"1111-2222-3333-4444"});

    // 4) InsufficientBalanceException이 던져지는지 확인
    EXPECT_THROW(
        lowController.enterCardNumber("ignored", testBev.getId(), 1),
        RequestPaymentController::InsufficientBalanceException
    );
}

// 재고 차감 실패 → BeverageReductionException
TEST_F(RequestPaymentControllerTest, 재고차감실패_예외발생) {
    // 재고 차감 실패하도록 StubBeverageManager 재설정
    delete bevMgr;
    bevMgr = new StubBeverageManager(testBev, /*reduceResult=*/false);
    controller = new TestableRequestPaymentController(bevMgr, bank);

    controller->setMockInputs({"1111-2222-3333-4444"});
    EXPECT_THROW(
        controller->enterCardNumber("ignored", testBev.getId(), 1),
        RequestPaymentController::BeverageReductionException
    );
}