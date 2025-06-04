#pragma once
#include <string>

#include "../../Domain/Credit/CreditCard.h"
#include "../../Domain/Credit/Bank.h"
#include "../../Domain/Socket/SocketManager.h"
#include "../../Domain/Auth/AuthCodeManager.h"
#include "../../Domain/DTO/ResponseStockDTO.h"
#include "../../Domain/Beverage/BeverageManager.h"

using namespace std;

class RequestPrePaymentController {
private:
    AuthCodeManager* authCodeManager;
    Bank* bank;
    SocketManager* socketManager;
    BeverageManager* beverageManager;

protected:
    virtual string inputCardNumber();

public:
    RequestPrePaymentController(AuthCodeManager* authCodeManager, Bank* bank, SocketManager* socketManager, BeverageManager* beverageManager);
    void enterPrePayIntention(bool intention);
    string enterCardNumber(string cardNumber, Beverage beverage, int quantity, int dstId);
};