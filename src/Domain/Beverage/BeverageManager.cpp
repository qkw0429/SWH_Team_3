#include "BeverageManager.h"
#include "Exception/CustomException.h"

using namespace customException;

bool BeverageManager::hasEnoughStock(int beverageId, int quantity) {
    if(this->beverages.find(beverageId) == beverages.end()){
        throw NotFoundException("beverageId에 해당하는 음료가 없습니다.");
    }

    Beverage beverage = this->beverages[beverageId];
    // cout << "음료 이름: " << beverage.getId() << "재고" << beverage.stock << endl;
    return beverage.hasEnoughStock(quantity);
}

bool BeverageManager::reduceQuantity(int beverageId, int quantity) {
    if (this->beverages.find(beverageId) == beverages.end()) {
        throw NotFoundException("beverageId에 해당하는 음료가 없습니다.");
    }

    Beverage& beverage = this->beverages[beverageId];
    return beverage.reduceQuantity(quantity);
}

Beverage BeverageManager::getBeverage(int beverageId) {
    if(this->beverages.find(beverageId) == beverages.end()){
        throw NotFoundException("beverageId에 해당하는 음료가 없습니다.");
    }
    // cout << "beverageId : " << beverageId << '\n';
    return this->beverages[beverageId];
}

int BeverageManager::getStock(int beverageId){
        return 0;
}

void BeverageManager::addBeverage(Beverage beverage) {
    beverages.insert({beverage.getId(), beverage});
}
