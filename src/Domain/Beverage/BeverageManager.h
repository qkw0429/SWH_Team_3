#pragma once
#include <map>
#include "Beverage.h"
using namespace std;

class BeverageManager {
private:
    map<int, Beverage> beverages;

public:
    BeverageManager() = default;
    virtual bool hasEnoughStock(int beverageId, int quantity);
    virtual bool reduceQuantity(int beverageId, int quantity);
    virtual Beverage getBeverage(int beverageId);
    void addBeverage(Beverage beverage);
    int getStock(int beverageId);
};