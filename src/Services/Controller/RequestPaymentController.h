#pragma once
#include <string>
#include "../../Domain/Beverage/BeverageManager.h"
#include "../../Domain/Credit/Bank.h"
#include "../../Domain/Credit/CreditCard.h"
#include "../../Domain/Beverage/Beverage.h"
using namespace std;

class RequestPaymentController {
private:
    BeverageManager* beverageManager;
    Bank* bank;

protected:
    virtual string inputCardNumber();

public:
    // 생성자: 모든 private 멤버를 초기화
    RequestPaymentController(BeverageManager* beverageManager, 
                             Bank* bank);

    Beverage enterCardNumber(string cardNumber, int beverageId, int quantity);

    class CardNotFoundException : public exception {
        public:
            const char* what() const noexcept override {
                return "카드 조회 3회 모두 실패하였습니다.";
            }
    };

    class InsufficientBalanceException : public exception {
        public:
            const char* what() const noexcept override {
                return "잔액이 부족합니다.";
            }
    };

    class BeverageReductionException : public exception {
        public:
            const char* what() const noexcept override {
                return "음료 재고 차감에 실패했습니다.";
            }
    };
};