#include "Domain/Auth/AuthCodeManager.h"
#include "Exception/CustomException.h"
#include <gtest/gtest.h>
using namespace customException;
class AuthCodeManagerTest : public ::testing::Test {
protected:
    AuthCodeManager manager;

    void SetUp() override {
        // 테스트를 위한 인증 코드 등록
        manager.saveAuthCode(1,2,"ABC12");
        manager.saveAuthCode(2,5,"ABC34");
    }
};

TEST_F(AuthCodeManagerTest, ValidateAuthCode_등록된코드_True반환) {
    EXPECT_TRUE(manager.validateAuthCode("ABC12"));
    EXPECT_TRUE(manager.validateAuthCode("ABC34"));
}

TEST_F(AuthCodeManagerTest, ValidateAuthCode_없는코드_False반환) {
    EXPECT_THROW(manager.validateAuthCode("INVALID999"), NotFoundException);
    EXPECT_THROW(manager.validateAuthCode(""), NotFoundException);
}

TEST_F(AuthCodeManagerTest, GetBeverageId_등록된코드_Id반환) {
    EXPECT_EQ(manager.getBeverageId("ABC12"), 1);
    EXPECT_EQ(manager.getBeverageId("ABC34"), 2);
}

TEST_F(AuthCodeManagerTest, GetBeverageId_없는코드_exception_반환) {
    EXPECT_THROW(manager.getBeverageId("INVALID999"), NotFoundException);
    EXPECT_THROW(manager.getBeverageId(""), NotFoundException);
}

TEST_F(AuthCodeManagerTest, GenerateAuthCode_생성된_인증코드_길이는_5) {
    string code = manager.generateAuthCode();
    EXPECT_EQ(code.length(), 5);
}

TEST_F(AuthCodeManagerTest, GenerateAuthCode_영어와_숫자만포함) {
    string code = manager.generateAuthCode();
    EXPECT_TRUE(all_of(code.begin(), code.end(), [](char c) {
        return isalnum(static_cast<unsigned char>(c));
    }));
}

TEST_F(AuthCodeManagerTest, DeleteAuthCode_등록된코드_성공) {
    // 정상 삭제
    EXPECT_NO_THROW(manager.deleteAuthCode("ABC12"));
    // 삭제된 코드는 더 이상 유효하지 않음
    EXPECT_THROW(manager.validateAuthCode("ABC12"), NotFoundException);
    EXPECT_THROW(manager.getBeverageId("ABC12"), NotFoundException);
}

TEST_F(AuthCodeManagerTest, DeleteAuthCode_없는코드_exception) {
    EXPECT_THROW(manager.deleteAuthCode("NO_SUCH_CODE"), NotFoundException);
}

TEST_F(AuthCodeManagerTest, SaveAuthCode_새로운코드_저장후_조회성공) {
    const string newCode = "XYZ99";
    manager.saveAuthCode(7, 3, newCode);
    EXPECT_TRUE(manager.validateAuthCode(newCode));
    EXPECT_EQ(manager.getBeverageId(newCode), 7);
}

TEST_F(AuthCodeManagerTest, DeleteAuthCode_하나삭제_다른코드유효) {
    manager.deleteAuthCode("ABC12");
    // ABC12 만 삭제됐으니 ABC34 는 여전히 유효해야 함
    EXPECT_TRUE(manager.validateAuthCode("ABC34"));
    EXPECT_EQ(manager.getBeverageId("ABC34"), 2);
}

TEST_F(AuthCodeManagerTest, SaveAuthCode_중복키_기존값유지) {
    // 현재는 map.insert() 만 쓰므로 덮어쓰지 않고 무시됨
    manager.saveAuthCode(9, 9, "ABC12");
    // 원래 저장된 (1,2) 정보가 그대로 남아 있어야 함
    EXPECT_EQ(manager.getBeverageId("ABC12"), 1);
}
