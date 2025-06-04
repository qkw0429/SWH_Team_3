#include "ResponsePrePaymentController.h"

ResponsePrePaymentController::ResponsePrePaymentController(BeverageManager* beverageManager, AuthCodeManager* authCodeManager)
:beverageManager(beverageManager), authCodeManager(authCodeManager){}

ResponsePrePaymentDTO ResponsePrePaymentController::responsePrePay(int beverageId, int quantity, string authCode) {
    Beverage beverage = this->beverageManager->getBeverage(beverageId);

    bool isReduced = this->beverageManager->reduceQuantity(beverageId, quantity);
    
    if(isReduced){
        this->authCodeManager->saveAuthCode(beverageId, quantity, authCode);
    }

    // bool isReduced = true;
    ResponsePrePaymentDTO responsePrePaymentDTO(beverageId, quantity, isReduced);

    return responsePrePaymentDTO;
}