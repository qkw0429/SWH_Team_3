#pragma once
#include <string>
#include <map>
#include <random>
using namespace std;

class AuthCodeManager {
private:
    map<string, pair<int, int>> authCodeMap;

public:
    AuthCodeManager() = default;
    bool validateAuthCode(string authCode);
    int getBeverageId(string authCode);
    void saveAuthCode(int beverageId, int quantity, string authCode);
    void deleteAuthCode(string authCode);
    string generateAuthCode();
};