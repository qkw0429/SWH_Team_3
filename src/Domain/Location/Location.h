
#pragma once
#include <iostream>
#include <cmath>

class Location{
    private:
        int x;
        int y;

    public:
        Location() = default;
        Location(int x, int y) : x(x), y(y) {}

        int getX(){
            return this->x;
        }

        int getY(){
            return this->y;
        }

        double distanceTo(int otherX, int otherY) const {
            int dx = this->x - otherX;
            int dy = this->y - otherY;
            return std::sqrt(dx * dx + dy * dy);
        }
};