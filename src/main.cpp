#include "Services/Controller/EnterAuthCodeController.h"
#include "Services/Controller/RequestPrePaymentController.h"
#include "Services/Controller/ResponsePrePaymentController.h"
#include "Services/Controller/ResponseStockController.h"
#include "Services/Controller/SelectBeverageController.h"
#include "Services/Controller/RequestPaymentController.h"

#include "Domain/Socket/SocketManager.h"
#include "Domain/Auth/AuthCodeManager.h"
#include "Domain/Beverage/BeverageManager.h"
#include "Domain/Credit/Bank.h"
#include "Domain/Credit/CreditCard.h"
#include "Domain/Location/LocationManager.h"
#include "Domain/Beverage/Beverage.h"
#include "Exception/CustomException.h"
#include <iostream>
#include <cstdlib>

bool isInteger(const std::string& str) {
  if(str.empty()) {
    return false;
  }
    for (char c : str) {
        if (!std::isdigit(c)) {
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {

    SocketManager* socketManager = new SocketManager(atoi(argv[1]), atoi(argv[2]));
    AuthCodeManager* authCodeManager = new AuthCodeManager();
    Bank* bank = new Bank();
    BeverageManager* beverageManager = new BeverageManager();
    LocationManager* locationManager = new LocationManager(0, 0);   // 현재 DVM 위치 (0, 0) -> 임시로 설정
    
    SelectBeverageController* selectBeverageController = new SelectBeverageController(locationManager, beverageManager, socketManager);
    RequestPrePaymentController* requestPrePaymentController = new RequestPrePaymentController(authCodeManager, bank, socketManager, beverageManager);
    EnterAuthCodeController* enterAuthCodeController = new EnterAuthCodeController(beverageManager, authCodeManager);
    ResponsePrePaymentController* responsePrePaymentController = new ResponsePrePaymentController(beverageManager, authCodeManager);
    RequestPaymentController* requestPaymentController = new RequestPaymentController(beverageManager, bank);
    ResponseStockController* responseStockController = new ResponseStockController(locationManager, beverageManager);

    socketManager->setController(responseStockController, responsePrePaymentController);

    authCodeManager->saveAuthCode(1, 2, "AB123");
    map<int, pair<string, int>> beverageMap;
    beverageMap[1] = make_pair("콜라", 1200);
    beverageMap[2] = make_pair("사이다", 1100);
    beverageMap[3] = make_pair("녹차", 1600);
    beverageMap[4] = make_pair("홍차", 1300);
    beverageMap[5] = make_pair("밀크티", 1400);
    beverageMap[6] = make_pair("탄산수", 2000);
    beverageMap[7] = make_pair("보리차", 2500);
    beverageMap[8] = make_pair("캔커피", 2600);
    beverageMap[9] = make_pair("물", 1500);
    beverageMap[10] = make_pair("에너지드링크", 1700);
    beverageMap[11] = make_pair("유자차", 1800);
    beverageMap[12] = make_pair("식혜", 1900);
    beverageMap[13] = make_pair("아이스티", 2000);
    beverageMap[14] = make_pair("딸기주스", 2100);
    beverageMap[15] = make_pair("오렌지주스", 2200);
    beverageMap[16] = make_pair("포도주스", 2300);
    beverageMap[17] = make_pair("이온음료", 2400);
    beverageMap[18] = make_pair("아메리카노", 2500);
    beverageMap[19] = make_pair("핫초코", 2600);
    beverageMap[20] = make_pair("카페라떼", 2700);
    vector<int> beverageIds;
    for(int i = 0; i < 7; i++){
      // 랜덤하게 인풋을 추가
      beverageIds.push_back(i);
    }
    for (const auto& pair : beverageMap) {
        int id = pair.first;
        string name = pair.second.first;
        int price = pair.second.second;
        // 보유중인 음료
        if(find(beverageIds.begin(), beverageIds.end(), id) != beverageIds.end()){
          beverageManager->addBeverage(Beverage(id, name, id + 10, price));
        }
        // 보유중이지 않은 음료
        else{
          beverageManager->addBeverage(Beverage(id, name, 0, price));
        }
    }

    int beverageId = -1;      
    int quantity = -1;
    bool status = false;
    int menu = -1;
    string menuStr="";

    while (true) {
      cout << "===============================\n";
      cout << "| " << setw(2) << left << "ID"
          << " | " << setw(12) << left << "상품명"
          << " | " << setw(7) << right << "가격  |\n";
      cout << "-------------------------------\n";

      // 메뉴 출력
      for (const auto& item : beverageMap) {
          cout << "| "
              << setw(2) << right << item.first << " "
              << "| "
              << setw(13) << left << item.second.first
              << "| "
              << setw(5) << right << item.second.second << "₩ |\n";
      }
      cout << "===============================\n";

      cout << "메뉴를 선택하세요 (1: 음료 선택, 2: 선결제 코드 입력, 0: 종료): ";
      getline(cin, menuStr);
      if(!isInteger(menuStr)) {
          cout << "잘못된 메뉴 입니다. 정수를 입력하세요. (0, 1, 2)" << endl;
          continue;
      }
      menu = stoi(menuStr);
      if (menu < 0 || menu > 2) {
          cout << "잘못된 메뉴입니다. 범위를 확인하세요. (0 ~ 2)" << endl;
          continue;
      }
      // 메뉴 선택
      if (menu == 0) {
          cout << "프로그램을 종료합니다." << endl;
          break;
      } else if (menu == 2) {
        // 인증 코드 입력
        cout << "인증 코드를 입력하세요: ";
        string authCode;
        getline(cin, authCode); // 개행 문자 제거
        try {
          Beverage beverage = enterAuthCodeController->enterAuthCode(authCode);
          cout << "인증 코드 확인 성공! 음료를 받으세요 : " << beverage.getId() << endl; 
          continue;
        } catch (const customException::InvalidException& e) {
            // 인증 코드 3회 실패한 경우
            std::cout << e.what() << std::endl;
            continue;
        }
      } else if (menu == 1){ // 1일때는 이하 실행 (else에 안걸리도록 넣음)
      } else {
          cout << "잘못된 메뉴입니다. 다시 선택하세요." << endl;
          continue;
      }

      // uc1
      try {
        string beverageIdStr;
        string quantityStr;
        cout << "음료 아이디를 입력하세요 (1~20): ";
        getline(cin, beverageIdStr);
        if(!isInteger(beverageIdStr)) {
          cout << "잘못된 음료 아이디 입력입니다. 정수를 입력하세요. (1 ~ 20)" << endl;
          continue;
        }
        beverageId = stoi(beverageIdStr);
        Beverage beverage = beverageManager->getBeverage(beverageId);

        for(int i = 0; i< 3; i++){
          if(beverage.getStock() > 0)
          {
            cout << "수량을 입력하세요 (1~" << beverage.getStock() << "): ";
          }else{
            cout << "수량을 입력하세요 (재고 없음, 선결제 진행): "; 
          }
        
          getline(cin, quantityStr);
          if(!isInteger(quantityStr)) {
            cout << "잘못된 음료 아이디 입력입니다. 정수를 입력하세요. (1~" << beverage.getStock() << ")" << endl;
            continue;
          }else{
            try{
              quantity = stoi(quantityStr);
            } catch (const std::invalid_argument& e) {
              cout << "잘못된 음료 아이디 입력입니다. 정수를 입력하세요. (1~" << beverage.getStock() << ")" << endl;
            }
            break;
          }
      }
        selectBeverageController->selectBeverage(beverageId, quantity);
      } catch (const customException::NotFoundException& e) {
          std::cout << e.what() << "음료를 찾을 수 없습니다. 다시 입력하세요." << std::endl;
          continue;
      } catch (const customException::InvalidException& e) {
          std::cout << e.what() << " 다시 입력하세요." << std::endl;
          continue;
      } catch (const customException::DVMInfoException& e) {
          // uc3
          DVMInfoDTO nearestDVM = e.getNearestDVM();
          
          cout << "음료 선결제" << endl;
          cout << "가장 가까운 DVM 정보: DvmId = " << nearestDVM.getPrePaymentDvmId() << ", 위치 = (" << nearestDVM.getX() << ", " << nearestDVM.getY() << ")" << endl;
          cout << "선결제 의사를 입력하세요 (1: 선결제 진행, 0: 선결제 진행 안함): ";
          int intention;
          string intentionStr;
          getline(cin, intentionStr);
          if(!isInteger(intentionStr)) {
            cout << "잘못된 의사입니다. 정수를 입력하세요. (0 ~ 1)" << endl;
            continue;
          }
          intention = stoi(intentionStr);
          if (intention < 0 || intention > 1) {
            cout << "잘못된 의사입니다. 다시 입력하세요. (0 ~ 1)" << endl;
            continue;
          }
          bool intentionBool = (intention == 1);
          string cardNumber;

          try {
            requestPrePaymentController->enterPrePayIntention(intentionBool);
            Beverage beverage = beverageManager->getBeverage(beverageId);

            string authCode = requestPrePaymentController->enterCardNumber(cardNumber, beverage, quantity, nearestDVM.getPrePaymentDvmId());
            cout << "선결제 성공: " << authCode << endl;
          } catch (const customException::InvalidException& e) {
              // 선결제 의사 없는 경우 or 카드번호 3회 실패한 경우
              continue;
          } catch (const customException::NotEnoughBalanceException& e) {
              // 카드 잔액 부족한 경우
              cout << e.what() << " 다시 입력하세요." << endl;
              continue;
          } catch (const customException::FailedToPrePaymentException& e) {
              // 선결제 실패한 경우
              cout << e.what() << " 다시 입력하세요." << endl;
              continue;
          } catch (const customException::FileOpenException& e) {
            // 카드 DB 파일 열기 실패한 경우
            cerr << e.what() << endl;
            exit(EXIT_FAILURE);
        }
      }

      // uc2
      cout << "음료 결제" << endl;
      string cardNumber;
      try{
          Beverage beverage = requestPaymentController->enterCardNumber(cardNumber, beverageId, quantity);
          cout << "결제 성공: " << beverage.getId() << endl;
      } catch (const RequestPaymentController::CardNotFoundException& e) {
          cout << e.what() << endl;
      } catch (const RequestPaymentController::InsufficientBalanceException& e) {
        cout << e.what() << endl;
      } catch (const RequestPaymentController::BeverageReductionException& e) {
        cout << e.what() << endl;
      }

    }

    delete socketManager;
    delete authCodeManager;
    delete bank;
    delete beverageManager;
    delete locationManager;
    delete selectBeverageController;
    delete requestPrePaymentController;
    delete enterAuthCodeController;
    delete responsePrePaymentController;
    delete requestPaymentController;
    delete responseStockController;


  return 0;
}