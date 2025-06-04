#pragma once
#include "../../Domain/Location/LocationManager.h"
#include "../../Domain/Beverage/BeverageManager.h"
#include "../../Domain/DTO/ResponseStockDTO.h"

class ResponseStockController {
private:
    LocationManager* locationManager;
    BeverageManager* beverageManager;

public:
    ResponseStockController() = default;
    ResponseStockController(LocationManager * LocationManager, BeverageManager* beverageManager);
    ResponseStockDTO responseBeverageStock(int beverageId, int quantity);
};