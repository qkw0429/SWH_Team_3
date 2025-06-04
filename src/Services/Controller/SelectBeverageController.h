#pragma once
#include "Domain/Location/LocationManager.h"
#include "Domain/Beverage/BeverageManager.h"
#include "Domain/Socket/SocketManager.h"
#include "Exception/CustomException.h"
#include <stdexcept>

class SelectBeverageController {
private:
    LocationManager* locationManager;
    BeverageManager* beverageManager;
    SocketManager* socketManager;

public:
    SelectBeverageController(LocationManager* locationManager, BeverageManager* beverageManager, SocketManager* socketManager);
    void selectBeverage(int beverageId, int quantity);
};