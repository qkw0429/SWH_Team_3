#pragma once
#include <list>
#include "Location.h"
#include "../DTO/DVMInfoDTO.h"
#include "../DTO/ResponseStockDTO.h"
#include "Exception/CustomException.h"

using namespace std;

class LocationManager {
private:
    Location location;

public:
    LocationManager(int x, int y) : location(x, y) {}
    virtual DVMInfoDTO calculateNearest(list<ResponseStockDTO> responseList);
    Location getLocation();
};