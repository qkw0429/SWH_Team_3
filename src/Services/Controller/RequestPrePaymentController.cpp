#include "RequestPrePaymentController.h"
#include "Exception/CustomException.h"
using namespace customException;
RequestPrePaymentController::RequestPrePaymentController(AuthCodeManager* authCodeManager, Bank* bank, SocketManager* socketManager, BeverageManager* beverageManager)
    : authCodeManager(authCodeManager), bank(bank), socketManager(socketManager), beverageManager(beverageManager) {}

void RequestPrePaymentController::enterPrePayIntention(bool intention) {
    if (!intention) {
        // uc1
        throw InvalidException("Invalid intention");
    }
    cout << "선결제 의사를 확인했습니다." << endl;
    cout << "선결제를 진행합니다." << endl;
}

string RequestPrePaymentController::enterCardNumber(string cardNumber, Beverage beverage, int quantity, int dstId) {
    for (int i = 0; i < 3; i++) {
        cardNumber = inputCardNumber();
        if (cardNumber.empty()) {
            cout << "카드 번호를 입력하지 않았습니다. 다시 입력하세요." << endl;
            continue;
        }
        cout << "CARD NUMBER" << cardNumber << endl;
        try {
            CreditCard* card = bank->requestCard(cardNumber);

            int price = beverage.getPrice() * quantity;
            bool isPayable = card->validateBalance(price);
            if (!isPayable) {
                throw NotEnoughBalanceException("카드 잔액이 부족합니다.");
            }

            string authCode = authCodeManager->generateAuthCode();
            
            bool isPrePayable = socketManager->requestPrePayment(beverage.getId(), quantity, authCode, dstId);
            if (!isPrePayable) {
                throw FailedToPrePaymentException("해당 음료는 선결제를 할 수 없습니다.");
            }

            // 선결제 성공시 카드 잔액 차감
            card->reduceBalance(price);
            bank->saveCreditCard(*card);
    
            return authCode;
        } catch (const NotFoundException& e) {
            // 카드 못찾은 경우 -> 최대 3회 입력
            cout << e.what() << " 다시 입력하세요." << endl;
            continue;
        } 
    }

    cout << "카드번호 3회 실패" << endl;
    // uc1
    throw InvalidException("카드 번호 3회 실패");
}

string RequestPrePaymentController::inputCardNumber() {
    string cardNumber;
    cout << "카드 번호를 입력하세요: ";
    // cin >> cardNumber;
    getline(cin, cardNumber);
    return cardNumber;
}