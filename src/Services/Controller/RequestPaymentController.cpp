#include "RequestPaymentController.h"

RequestPaymentController::RequestPaymentController(BeverageManager* beverageManager, 
                                                       Bank* bank)
    : beverageManager(beverageManager), bank(bank) {}

Beverage RequestPaymentController::enterCardNumber(string cardNumber, int beverageId, int quantity) {
    const int MAX_ATTEMPTS = 3;
    Beverage beverage = beverageManager->getBeverage(beverageId);
    int price = beverage.getPrice() * quantity;
    CreditCard* card = nullptr;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
        cardNumber = inputCardNumber();
        if (cardNumber.empty()) {
            cout << "카드 번호를 입력하지 않았습니다. 다시 입력하세요." << endl;
            continue;
        }

        cout << "CARD NUMBER" << cardNumber << endl;
        try{
            card = bank->requestCard(cardNumber);
            break;      // 카드 조회 성공 시 루프 종료
        }catch(customException::NotFoundException e){
            if (attempt < MAX_ATTEMPTS) {
                cerr << "[" << attempt << "번 실패] 올바르지 않은 카드번호입니다. 다시 입력하세요." << endl;
            }
        }
    }

    // 카드 조회를 3회 모두 실패한 경우
    if (card == nullptr) {
        throw CardNotFoundException();
    }

    // 잔액 검증
    bool isPayable = card->validateBalance(price);
    if (!isPayable) {
        throw InsufficientBalanceException();
    }

    
    // 재고 차감
    if (!beverageManager->reduceQuantity(beverageId, quantity)) {
        throw BeverageReductionException();
    }

    // 결제 처리
    card->reduceBalance(price);
    bank->saveCreditCard(*card);

    // 구매한 음료 반환
    return beverageManager->getBeverage(beverageId);
}

string RequestPaymentController::inputCardNumber() {
    string cardNumber;
    cout << "카드 번호를 입력하세요: ";
    // cin >> cardNumber;
    getline(cin, cardNumber); // 개행 문자 제거
    return cardNumber;
}