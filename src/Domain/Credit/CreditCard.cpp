#include "CreditCard.h"

bool CreditCard::validateBalance(int price) {
    return price <= balance;
}

void CreditCard::reduceBalance(int price) {
    if (price < 0) {
        throw customException::InvalidException("Price cannot be negative");
    }

    if (balance < price) {
        throw customException::NotEnoughBalanceException("잔액이 부족합니다.");
    }
    balance -= price;
}

bool CreditCard::isValid(){
    return !this->cardNumber.empty();
}

