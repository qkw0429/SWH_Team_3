#include "EnterAuthCodeController.h"
#include "Exception/CustomException.h"
using namespace customException;

EnterAuthCodeController::EnterAuthCodeController(BeverageManager* beverageManager, AuthCodeManager* authCodeManager)
    : beverageManager(beverageManager), authCodeManager(authCodeManager) {
}

Beverage EnterAuthCodeController::enterAuthCode(string authCode) {
    const int MAX_ATTEMPTS = 3;

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
        try {
            // 유효성 검사: 예외가 없으면 성공
            authCodeManager->validateAuthCode(authCode);

            // 검증 성공 시 뒤처리
            int beverageId = authCodeManager->getBeverageId(authCode);
            authCodeManager->deleteAuthCode(authCode);
            return beverageManager->getBeverage(beverageId);

        } catch (const NotFoundException& e) {
            // 마지막 시도가 아니면 재입력 요청
            if (attempt < MAX_ATTEMPTS) {
                std::cerr << "[" << attempt << "번째 실패] 유효하지 않은 인증 코드입니다. 다시 입력해주세요: ";
                // std::cin  >> authCode;
                authCode = inputAuthCode(); // 개행 문자 제거
            } else {
                // 3회 모두 실패한 경우
                throw InvalidException("인증코드를 3회 모두 실패했습니다.");
            }
        }
    }
}

string EnterAuthCodeController::inputAuthCode() {
    string authCode;
    getline(std::cin, authCode); // 개행 문자 제거
    return authCode;
}