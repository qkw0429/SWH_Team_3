#include "gtest/gtest.h"
#include "Services/Controller/RequestPrePaymentController.h"
#include "Domain/Beverage/Beverage.h"
#include "Domain/Socket/SocketManager.h"
#include "Domain/Credit/Bank.h"
#include "Domain/Auth/AuthCodeManager.h"
#include "Domain/Beverage/BeverageManager.h"
#include "Domain/Credit/CreditCard.h"
#include "Exception/CustomException.h"
#include <queue>
#include <stdexcept>
#include <string>

using namespace std;
using namespace customException;

// ----------------- Mock SocketManager ------------------
class MockSocketManager : public SocketManager {
private:
    bool expectedAvailability = false;

public:
    MockSocketManager() : SocketManager(0, 0) {}

    void setExpectedAvailability(bool available) {
        expectedAvailability = available;
    }

    bool requestPrePayment(int beverageId, int quantity, string authCode, int dstId) override {
        return expectedAvailability;
    }
};

// ------------- Testable Controller with Mocked Input -------------
class TestableRequestPrePaymentController : public RequestPrePaymentController {
private:
    queue<string> mockInputs;

public:
    TestableRequestPrePaymentController(AuthCodeManager* auth, Bank* bank,
        SocketManager* socket, BeverageManager* bev)
        : RequestPrePaymentController(auth, bank, socket, bev) {}

    void setMockInputs(const vector<string>& inputs) {
        mockInputs = queue<string>(deque<string>(inputs.begin(), inputs.end()));
    }

protected:
    string inputCardNumber() override {
        if (mockInputs.empty()) throw runtime_error("No more mock inputs");
        string result = mockInputs.front();
        mockInputs.pop();
        return result;
    }
};

// ----------------------------- Test Suite -----------------------------
class RequestPrePaymentControllerTest : public ::testing::Test {
protected:
    AuthCodeManager authCodeManager;
    Bank bank;
    MockSocketManager socketManager;
    BeverageManager beverageManager;
};

TEST_F(RequestPrePaymentControllerTest, responsePrePay_잘못된_카드번호_예외) {
    Beverage beverage(15, "오렌지주스", 0, 2200);
    TestableRequestPrePaymentController controller(&authCodeManager, &bank, &socketManager, &beverageManager);
    controller.setMockInputs({"invalid-1", "invalid-2", "invalid-3"});

    EXPECT_THROW(controller.enterCardNumber("", beverage, 10, 1), InvalidException);
}

TEST_F(RequestPrePaymentControllerTest, responsePrePay_잔액부족_예외) {
    Beverage beverage(15, "오렌지주스", 0, 2200);
    socketManager.setExpectedAvailability(false);

    TestableRequestPrePaymentController controller(&authCodeManager, &bank, &socketManager, &beverageManager);
    controller.setMockInputs({"1111-2222-3333-4444"});

    EXPECT_THROW(controller.enterCardNumber("", beverage, 10, 1), FailedToPrePaymentException);
}

TEST_F(RequestPrePaymentControllerTest, responsePrePay_선결제_성공) {
    Beverage beverage(15, "오렌지주스", 0, 220);
    socketManager.setExpectedAvailability(true);

    TestableRequestPrePaymentController controller(&authCodeManager, &bank, &socketManager, &beverageManager);
    controller.setMockInputs({"1111-2222-3333-4444"});

    string authCode = controller.enterCardNumber("", beverage, 1, 1);
    EXPECT_FALSE(authCode.empty()); // AUTH1234처럼 고정값이면 EXPECT_EQ(authCode, "AUTH1234");
}