#include "Domain/Auth/AuthCodeManager.h"
#include "Domain/Beverage/BeverageManager.h"
#include "Services/Controller/EnterAuthCodeController.h"
#include "Domain/Beverage/Beverage.h"
#include "Exception/CustomException.h"
#include <gtest/gtest.h>
#include <queue>
#include <deque>
#include <vector>

using namespace customException;
using namespace std;
class TestableEnterAuthCodeController : public EnterAuthCodeController {
private:
    queue<string> mockInputs;

public:
    TestableEnterAuthCodeController(BeverageManager* bev, AuthCodeManager* auth)
        : EnterAuthCodeController(bev, auth) {}

    void setMockInputs(const vector<string>& inputs) {
        mockInputs = queue<string>(deque<string>(inputs.begin(), inputs.end()));
    }

protected:
    string inputAuthCode() override {
        if (mockInputs.empty()) throw runtime_error("No more mock auth codes");
        string code = mockInputs.front();
        mockInputs.pop();
        return code;
    }
};
class EnterAuthCodeControllerTest : public ::testing::Test {
protected:
    AuthCodeManager authCodeManager;
    BeverageManager beverageManager;
    TestableEnterAuthCodeController* controller;

    void SetUp() override {
        beverageManager.addBeverage(Beverage(1, "콜라", 1000, 10));
        beverageManager.addBeverage(Beverage(2, "사이다", 1000, 10));

        authCodeManager.saveAuthCode(1, 3, "ABC12");

        controller = new TestableEnterAuthCodeController(&beverageManager, &authCodeManager);
    }

    void TearDown() override {
        delete controller;
    }
};

// 성공 케이스 테스트
TEST_F(EnterAuthCodeControllerTest, EnterAuthCode_유효한코드_음료반환) {
    Beverage beverage = controller->enterAuthCode("ABC12");
    EXPECT_EQ(beverage.getId(), 1);
}

// 실패 케이스 테스트 (3회 실패 → 예외 발생)
TEST_F(EnterAuthCodeControllerTest, EnterAuthCode_잘못된코드_예외발생) {
    vector<string> wrongCodes = {"WRONG1", "WRONG2", "WRONG3"};
    controller->setMockInputs(wrongCodes);

    EXPECT_THROW({
        controller->enterAuthCode("INVALID_CODE");
    }, InvalidException);
}
