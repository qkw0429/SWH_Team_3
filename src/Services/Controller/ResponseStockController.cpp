#include "ResponseStockController.h"

ResponseStockController::ResponseStockController(LocationManager* locationManager, BeverageManager* beverageManager)
: locationManager(locationManager), beverageManager(beverageManager){}

ResponseStockDTO ResponseStockController::responseBeverageStock(int beverageId, int quantity) {
    Location location = this->locationManager->getLocation();

    return ResponseStockDTO(beverageId, quantity, location.getX(), location.getY());
}