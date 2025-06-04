#include "Services/Controller/SelectBeverageController.h"
#include "Domain/Location/LocationManager.h"
#include "Domain/Beverage/BeverageManager.h"
#include "Domain/Socket/SocketManager.h"
#include "Exception/CustomException.h"
#include <gtest/gtest.h>
#include <vector>

using namespace customException;
using namespace std;

// BeverageManager를 스텁으로 대신하여 hasEnoughStock 호출 동작을 제어
class StubBeverageManager : public BeverageManager {
public:
    bool stockAvailable;
    bool throwNotFound;

    StubBeverageManager(bool stockAvailable = true, bool throwNotFound = false)
        : stockAvailable(stockAvailable), throwNotFound(throwNotFound) {}

    bool hasEnoughStock(int beverageId, int quantity) override {
        if (throwNotFound) {
            throw NotFoundException("음료 정보 없음");
        }
        return stockAvailable;
    }
};

// SocketManager를 스텁으로 대신하여 requestBeverageStockToOthers 호출 동작을 제어
class StubSocketManager : public SocketManager {
public:
    std::list<ResponseStockDTO> dummyResponses;

    // 기본 생성자 호출 → SocketManager() 를 타고 아무 것도 안 합니다
    StubSocketManager(const std::list<ResponseStockDTO>& responses = {})
      : SocketManager()   // 기본 생성자
      , dummyResponses(responses)
    {}

    // 반환 타입을 std::list<ResponseStockDTO>로 맞춥니다.
    std::list<ResponseStockDTO> requestBeverageStockToOthers(int beverageId, int quantity) override {
        return dummyResponses;
    }
};

// LocationManager를 스텁으로 대신하여 calculateNearest 호출 동작을 제어
class StubLocationManager : public LocationManager {
public:
    DVMInfoDTO nearest;

    // LocationManager(int x, int y) 생성자를 호출
    StubLocationManager(const DVMInfoDTO& dto)
      : LocationManager(/*x=*/0, /*y=*/0),nearest(dto){}

    DVMInfoDTO calculateNearest(const std::list<ResponseStockDTO> responses) override {
        return nearest;
    }
};

class SelectBeverageControllerTest : public ::testing::Test {
protected:
    SelectBeverageController* controller;
    StubBeverageManager* bevMgr;
    StubSocketManager* sockMgr;
    StubLocationManager* locMgr;

    void TearDown() override {
        delete controller;
        delete bevMgr;
        delete sockMgr;
        delete locMgr;
    }
};

// 성공 케이스: 재고가 충분해서 아무 예외 없이 리턴
TEST_F(SelectBeverageControllerTest, 재고충분_성공) {
    bevMgr  = new StubBeverageManager(/*stockAvailable=*/true, /*throwNotFound=*/false);
    sockMgr = new StubSocketManager();
    locMgr  = new StubLocationManager(DVMInfoDTO());  // 값은 사용되지 않음
    controller = new SelectBeverageController(locMgr, bevMgr, sockMgr);

    EXPECT_NO_THROW(controller->selectBeverage(5, 2));
}

// 실패 케이스: beverageId 범위 벗어남 → InvalidException
TEST_F(SelectBeverageControllerTest, 잘못된BeverageId_예외발생) {
    bevMgr  = new StubBeverageManager();
    sockMgr = new StubSocketManager();
    locMgr  = new StubLocationManager(DVMInfoDTO());
    controller = new SelectBeverageController(locMgr, bevMgr, sockMgr);

    EXPECT_THROW(controller->selectBeverage(0, 1), InvalidException);
    EXPECT_THROW(controller->selectBeverage(21, 1), InvalidException);
}

// 실패 케이스: quantity <= 0 → InvalidException
TEST_F(SelectBeverageControllerTest, 잘못된Quantity_예외발생) {
    bevMgr  = new StubBeverageManager();
    sockMgr = new StubSocketManager();
    locMgr  = new StubLocationManager(DVMInfoDTO());
    controller = new SelectBeverageController(locMgr, bevMgr, sockMgr);

    EXPECT_THROW(controller->selectBeverage(1, 0), InvalidException);
    EXPECT_THROW(controller->selectBeverage(1, -5), InvalidException);
}

// 실패 케이스: BeverageManager에서 NotFoundException 발생 → InvalidException으로 변환
TEST_F(SelectBeverageControllerTest, BeverageNotFound_InvalidException으로변환) {
    bevMgr  = new StubBeverageManager(/*stockAvailable=*/false, /*throwNotFound=*/true);
    sockMgr = new StubSocketManager();
    locMgr  = new StubLocationManager(DVMInfoDTO());
    controller = new SelectBeverageController(locMgr, bevMgr, sockMgr);

    EXPECT_THROW(controller->selectBeverage(3, 1), InvalidException);
}

// 재고 부족 시 DVMInfoException 발생
TEST_F(SelectBeverageControllerTest, 재고부족_DVMInfoException발생) {
    // 1) 재고 부족
    bevMgr  = new StubBeverageManager(/*stockAvailable=*/false, /*throwNotFound=*/false);
    // 2) SocketManager: 기본 빈 리스트
    sockMgr = new StubSocketManager();
    // 3) LocationManager: 특정 DVMInfoDTO 반환
    DVMInfoDTO expectedNearest(0, 0, 42);  // 예시 필드 설정
    locMgr  = new StubLocationManager(expectedNearest);

    controller = new SelectBeverageController(locMgr, bevMgr, sockMgr);

    // DVMInfoException 발생 여부만 검사
    EXPECT_THROW(controller->selectBeverage(7, 3), DVMInfoException);
}
