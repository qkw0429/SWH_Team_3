#pragma once
#include <list>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "CreditCard.h"
#include "Exception/CustomException.h"

using namespace std;
namespace fs = std::filesystem;

class Bank {
private:
    list<CreditCard> cards;

public:
    virtual CreditCard* requestCard(string cardNumber);
    virtual void saveCreditCard(CreditCard creditCard);
};