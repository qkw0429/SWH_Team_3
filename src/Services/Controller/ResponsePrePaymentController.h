#pragma once
#include "../../Domain/Beverage/BeverageManager.h"
#include "../../Domain/Auth/AuthCodeManager.h"
#include "../../Domain/DTO/ResponsePrePaymentDTO.h"

class ResponsePrePaymentController {
    private:
        BeverageManager* beverageManager;
        AuthCodeManager* authCodeManager;

    public:
        ResponsePrePaymentController() = default;
        ResponsePrePaymentController(BeverageManager* beverageManager, AuthCodeManager* authCodeManager);
        ResponsePrePaymentDTO responsePrePay(int beverageId, int quantity, string authCode);
};
