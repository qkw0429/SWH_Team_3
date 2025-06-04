#include "Beverage.h"

Beverage::Beverage():id(0), name("name"), stock(0), price(0){};

Beverage::Beverage(int id, const string& name, int stock, int price)
    :id(id), name(name), stock(stock), price(price){};

bool Beverage::hasEnoughStock(int quantity){
    if (stock < quantity) {
        return false;
    }
    return true;
}

bool Beverage::reduceQuantity(int quantity){
    if (quantity < 0) {
        return false;
    }

    if (stock < quantity) {
        return false;
    }

    stock -= quantity;
    return true;
}

int Beverage::getId(){
    return id;
}

int Beverage::getPrice(){
    return price;
}

int Beverage::getStock(){
    return stock;
}