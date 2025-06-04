#pragma once
#include <string>
#include <Exception/CustomException.h>

class CreditCard {
private:
    string cardNumber;
    int balance;

public:
    CreditCard(string cardNumber, int balance) : cardNumber(cardNumber), balance(balance) {}
    virtual bool validateBalance(int price);
    void reduceBalance(int price);
    bool isValid();
    string& getCardNumber() { return cardNumber; }
    int getBalance() { return balance; }
};