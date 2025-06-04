#include "SelectBeverageController.h"

SelectBeverageController::SelectBeverageController(LocationManager* locationManager, BeverageManager* beverageManager, SocketManager* socketManager)
    : locationManager(locationManager), beverageManager(beverageManager), socketManager(socketManager) {
}

void SelectBeverageController::selectBeverage(int beverageId, int quantity) {
    // 1. 입력 검증
    if (beverageId <= 0 || beverageId > 20) {
        throw customException::InvalidException("음료 아이디는 1~20 사이의 값이어야 합니다.");
    }
    if (quantity <= 0) {
        throw customException::InvalidException("quantity는 1 이상이어야 합니다.");
    }

    // 2. 로컬 재고 확인
    bool hasStock = false;
    try {
        hasStock = beverageManager->hasEnoughStock(beverageId, quantity);
    } catch (const customException::NotFoundException& e) {
        throw customException::InvalidException("존재하지 않는 beverageId 입니다: " + std::to_string(beverageId));
    }
    // cout << "hasStock: " << hasStock << endl;
    if (hasStock) {
        // 3. 재고가 충분하면 아무 것도 하지 않고 종료
        return;
    }

    // 4. 재고 부족 → 다른 DVM에 재고 요청
    auto responseList = socketManager->requestBeverageStockToOthers(beverageId, quantity);

    // 5. 가장 가까운 DVM 계산
    DVMInfoDTO nearestDVM = locationManager->calculateNearest(responseList);
    
    // 6. 계산된 DVM 정보를 예외로 던져서 호출 쪽에서 처리하도록 함
    throw customException::DVMInfoException(nearestDVM);
}
