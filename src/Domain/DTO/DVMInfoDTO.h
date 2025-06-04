#pragma once
#include <iostream>

class DVMInfoDTO{
    private:
        int x;
        int y;
        int prePaymentDvmId;
    public:
        DVMInfoDTO() : x(0), y(0), prePaymentDvmId(0) {}
        DVMInfoDTO(int x, int y, int prePaymentDvmId) : x(x), y(y), prePaymentDvmId(prePaymentDvmId) {}
        int getX() const { return x; }
        int getY() const { return y; }
        int getPrePaymentDvmId() const { return prePaymentDvmId; }
};