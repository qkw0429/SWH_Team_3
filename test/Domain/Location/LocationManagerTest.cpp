#include <gtest/gtest.h>
#include "Domain/Location/LocationManager.h"
#include <list>

using namespace customException;

TEST(LocationManagerTest, calculateNearest_단일요소_반환) {
    LocationManager manager = LocationManager(0, 0);
    std::list<ResponseStockDTO> list;

    ResponseStockDTO dto(0, 0, 3, 4);
    dto.setSrcAndDst(100, 0);  // srcId=100, dstId는 테스트에선 사용하지 않음
    list.push_back(dto);

    DVMInfoDTO result = manager.calculateNearest(list);
    EXPECT_EQ(result.getX(), 3);
    EXPECT_EQ(result.getY(), 4);
    EXPECT_EQ(result.getPrePaymentDvmId(), 100);
}

TEST(LocationManagerTest, calculateNearest_여러요소_가장가까운반환) {
    LocationManager manager = LocationManager(0, 0);
    std::list<ResponseStockDTO> list;

    ResponseStockDTO a(0, 0, 5,  5); a.setSrcAndDst(1, 0);
    ResponseStockDTO b(0, 0, 1,  1); b.setSrcAndDst(2, 0);
    ResponseStockDTO c(0, 0, -2, 2); c.setSrcAndDst(3, 0);

    list.push_back(a);
    list.push_back(b);
    list.push_back(c);

    // (0,0) 기준 거리: (5,5)=7.07, (1,1)=1.41, (-2,2)=2.83 → srcId=2가 가장 가까움
    DVMInfoDTO result = manager.calculateNearest(list);
    EXPECT_EQ(result.getX(), 1);
    EXPECT_EQ(result.getY(), 1);
    EXPECT_EQ(result.getPrePaymentDvmId(), 2);
}

TEST(LocationManagerTest, calculateNearest_음수좌표_반환) {
    LocationManager manager = LocationManager(0, 0);
    std::list<ResponseStockDTO> list;

    ResponseStockDTO dto(0, 0, -3, -4);
    dto.setSrcAndDst(42, 0);
    list.push_back(dto);

    DVMInfoDTO result = manager.calculateNearest(list);
    EXPECT_EQ(result.getX(), -3);
    EXPECT_EQ(result.getY(), -4);
    EXPECT_EQ(result.getPrePaymentDvmId(), 42);
}
