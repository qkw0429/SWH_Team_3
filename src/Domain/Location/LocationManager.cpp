#include "LocationManager.h"
#include <limits>

DVMInfoDTO LocationManager::calculateNearest(list<ResponseStockDTO> responseList) {
    // TODO : responseList가 empty한 경우 예외처리

    double minDist = std::numeric_limits<double>::max();
    
    DVMInfoDTO nearestDVM; 
    for (const auto& response : responseList) {
        double dist = location.distanceTo(response.getX(), response.getY());
        if (dist < minDist) {
            minDist = dist;
            nearestDVM = DVMInfoDTO(response.getX(), response.getY(), response.getSrcId());
        }
    }

    return nearestDVM;
}

Location LocationManager::getLocation() {
    return Location();
}