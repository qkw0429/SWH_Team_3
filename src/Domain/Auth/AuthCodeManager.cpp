#include "AuthCodeManager.h"
#include "Exception/CustomException.h"
#include <iostream>

using namespace customException;

bool AuthCodeManager::validateAuthCode(string authCode) {
    if(authCodeMap.find(authCode) == authCodeMap.end()){
        throw NotFoundException("Auth code not found");
    }
    return true;
}

int AuthCodeManager::getBeverageId(string authCode) {
    map<string, pair<int, int>>::iterator auth = authCodeMap.find(authCode);
    
    if(auth == authCodeMap.end()){
        throw NotFoundException("Auth code not found");
    }

    return auth->second.first;
}

void AuthCodeManager::saveAuthCode(int beverageId, int quantity, string authCode) {
    this->authCodeMap.insert({authCode, {beverageId, quantity}});
}

string AuthCodeManager::generateAuthCode() {
    const std::string CHARACTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::string authCode;

    std::random_device rd;  // 시스템에서 안전한 시드 생성
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, CHARACTERS.size() - 1);

    for (int i = 0; i < 5; ++i) {
        authCode += CHARACTERS[dist(gen)];
    }
    return authCode;
}

void AuthCodeManager::deleteAuthCode(string authCode) {
    if(authCodeMap.find(authCode) == authCodeMap.end()){
        throw NotFoundException("Auth code not found");
    }
    authCodeMap.erase(authCode);
}