#include "Bank.h"
#include "CreditCard.h"

CreditCard* Bank::requestCard(string cardNumber) {
    string path = filesystem::current_path().string() + "/card_db.txt";
    ifstream file(path);
    if (!file.is_open()) {
        throw customException::FileOpenException("카드 DB 파일 열기 실패 : " + path);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string storedCardNumber;
        int balance;

        if (!(iss >> storedCardNumber >> balance)) {
            continue;
        }

        if (storedCardNumber == cardNumber) {
            cout << "[Bank] 카드 조회 성공: " << storedCardNumber << " (잔액: " << balance << ")" << std::endl;
            return new CreditCard(storedCardNumber, balance);
        }
    }

    cout << "[Bank] 해당 카드 번호 없음: " << cardNumber << std::endl;

    throw customException::NotFoundException("해당 카드 번호 없음 : " + cardNumber);
}

void Bank::saveCreditCard(CreditCard creditCard) {
    string path = filesystem::current_path().string() + "/card_db.txt";
    string tempPath = filesystem::current_path().string() + "/card_db_temp.txt";

    ifstream inFile(path);
    ofstream outFile(tempPath);

    if (!inFile.is_open() || !outFile.is_open()) {
        cerr << "[Bank] 카드 데이터 파일 열기 실패 (읽기 또는 쓰기)" << endl;
        return;
    }

    string line;
    bool updated = false;
    while (getline(inFile, line)) {
        istringstream iss(line);
        string storedCardNumber;
        int balance;

        if (!(iss >> storedCardNumber >> balance)) {
            outFile << line << endl;
            continue;
        }

        if (storedCardNumber == creditCard.getCardNumber()) {
            outFile << storedCardNumber << " " << creditCard.getBalance() << endl;
            updated = true;
        } else {
            outFile << line << endl;
        }
    }

    if (!updated) {
        outFile << creditCard.getCardNumber() << " " << creditCard.getBalance() << endl;
    }

    inFile.close();
    outFile.close();

    // 기존 파일을 덮어쓰기
    filesystem::remove(path);
    filesystem::rename(tempPath, path);
}
