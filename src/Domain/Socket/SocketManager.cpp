#include "SocketManager.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <list>
#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <vector>

std::string receive_msg(int socket){
    // 1. 4바이트 길이 읽기
    int32_t len;
    int r1 = recv(socket, &len, sizeof(len), MSG_WAITALL);
    // cout << "<<<<<<<<<<<receive data>>>>>>>>>>>>>\n";
    if(r1 != sizeof(len)) {
        cout << "예상치 못한 이상한 메시지 수신\n";
        return "";
    }
    len = ntohl(len); // 다시 호스트 바이트 오더로
    // cout << r1 << " " << len << '\n';

    // 2. 본문 데이터를 len 바이트만큼 채워서 읽기
    std::vector<char> _buffer(len);
    int valread = recv(socket, _buffer.data(), len, MSG_WAITALL);
    if(valread != len) { 
        cout << "예상치 못한 이상한 json 메시지 수신\n";
        return "";
    }       

    std::string json_str(_buffer.data(), valread);
    // cout << json_str << '\n';
    return json_str;
}

//other DVM socket 미리 다 생성해놓기 (client socket)
void SocketManager::connectOtherDVMSocket(){
    for(auto dvmInfo : this->otherDVMInfo){
        int srdId = dvmInfo.first;
        string address = dvmInfo.second.first;
        int port = dvmInfo.second.second;

        int sock;

        //연결 시도
        bool connected = false;
        for(int attempt=0; attempt < 100; ++attempt){ //최대 100번 시도
            //socket 기본 정보를 생성하는 것
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                std::cout << "\nClient Socket 생성 실패(" << address << ":" << port << ")\n";
                // exit(EXIT_FAILURE);
                continue; // 소켓 생성 실패 시에도 계속 진행
            }

            //socket 설정정보를 위한 구조체
            struct sockaddr_in serv_addr = {};;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(port);
            if (inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr) <= 0) {
                cout << "\n올바르지 않는 client socket 주소와 포트번호(" << address << ":" << port << ")\n";
                // exit(EXIT_FAILURE);
                close(sock); // 소켓 닫기
                continue; // 주소 변환 실패 시에도 계속 진행
            }


            // printf("[DEBUG] connect to %s:%d (htons:%x)\n", address.c_str(), port, serv_addr.sin_port);
            // printf("[DEBUG] socket fd: %d\n", sock);
            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
                connected = true;
                break;
            } else {
                // perror("connect 실패");
                // std::cout << "\nClient Socket 연결실패(" << address << ":" << port 
                //         << "), 재시도: " << (attempt+1) << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        if (!connected) {
            close(sock);
            continue; // 실패 시 그냥 넘어감
        }

        this->otherDVMSockets.insert({srdId, sock});
        // cout << srdId << " : socket 연결 성공\n"; 
    }
}

//우리 DVM socket 생성해놓기 (server socket)
void SocketManager::openServerSocket(){
    //socket 기본 정보를 생성하는 것
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        perror("\nServer Socket 생성 실패\n");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    // 소켓 옵션 (바인드 에러 방지용)
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    //socket 설정정보를 위한 구조체
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(this->serverPort);

    if (::bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("\n바인딩 실패\n");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 3) < 0) {
        perror("\nlisten 실패\n");
        exit(EXIT_FAILURE);
    }

    this->serverSocket = server_socket;
}

void SocketManager::waitingRequest(){
    while(true){
        // cout << "server가 request 연결 대기중\n";
        int client_socket = accept(this->serverSocket, nullptr, nullptr);
        // cout << "server 연결 성공 \n";
        if (client_socket < 0) {
            perror("accept 실패");
            continue;
        }

        std::thread t([=]() {
            while(true){
                auto json_str = receive_msg(client_socket);

                try {
                    // cout << json_str << '\n';
                    nlohmann::json j = nlohmann::json::parse(json_str);

                    // 특정 JSON 값으로 처리 분기
                    if (j.contains("msg_type") && j["msg_type"] == "req_stock") {
                        // 스레드로 처리
                        std::thread t(&SocketManager::requestBeverageInfo,this, j["msg_content"]["item_code"], j["msg_content"]["item_num"], j["src_id"], j["dst_id"], client_socket);
                        t.detach();
                    }
                    if (j.contains("msg_type") && j["msg_type"] == "req_prepay") {
                        // 스레드로 처리
                        std::thread t(&SocketManager::requestPrePay,this, j["msg_content"]["item_code"], j["msg_content"]["item_num"], j["msg_content"]["cert_code"], j["src_id"], j["dst_id"], client_socket);
                        t.detach();
                    }
                } catch (std::exception& e) {
                    std::cerr << "JSON parse error: " << e.what() << std::endl;
                }
            }
        });
        t.detach();
    }
}


SocketManager::SocketManager(int srcId, int serverPort){
    otherDVMInfo.insert({1, {"127.0.0.1", 9001}});
    //.... 미리 알고 있다고 가정했음.
    this->serverPort = serverPort;
    this->srcId = srcId;

    openServerSocket();
    thread t(&SocketManager::waitingRequest, this);
    t.detach();
    thread t2(&SocketManager::connectOtherDVMSocket, this);
    t2.detach();
}

void SocketManager::setController(ResponseStockController *responseStockController, ResponsePrePaymentController *responsePrePaymentController){
    this->responseStockController = responseStockController;
    this->responsePrePaymentController = responsePrePaymentController;
};


// (해석) UC1에서 SelectBeverageController에 의해서 호출 당하는 것
// (변경) SelectBeverageController에 의해 호출 당하고 자체적으로 stream에 json 변환된거 흘려 보내는 것까지. 그리고 응답을 다 받아서 list로 줘야 함.
list<ResponseStockDTO> SocketManager::requestBeverageStockToOthers(int beverageId, int quantity) {

    list<ResponseStockDTO> result;

    for(auto dvmSocket : otherDVMSockets){
        RequestStockDTO requestStockDTO(beverageId, quantity, this->srcId, 0); // PFR에 broadcast는 dstId 0으로 하랬음.
        json j = requestStockDTO;

        //보내기
        std::string json_str = j.dump();
        int32_t length = json_str.size();
        int32_t net_length = htonl(length);

        send(dvmSocket.second, &net_length, sizeof(net_length), 0);
        send(dvmSocket.second, json_str.data(), json_str.size(), 0);


        //읽기
        auto json_string = receive_msg(dvmSocket.second);

        //json으로 바꾸어서 DTO에 mapping
        nlohmann::json rj = nlohmann::json::parse(json_string);
        ResponseStockDTO responseStockDTO = rj;

        result.push_back(responseStockDTO);
    }

    return result;
}

// (해석) UC1에서 다른 DVM의 이 함수를 호출, 그리고 UC5에서 이 함수가 호출 당하며 시작
// (변경) SocketManager의 stream 관찰 함수의 분기처리에 의해 thread가 새로이 생성되어 호출되는 것. 응답으로 다시 stream에 json 흘려보내야 함.
ResponseStockDTO SocketManager::requestBeverageInfo(int beverageId, int quantity, int srcId, int dstId, int clientSocket) {
    ResponseStockDTO responseStockDTO = this->responseStockController->responseBeverageStock(beverageId, quantity);
    responseStockDTO.setSrcAndDst(this->srcId, srcId);

    // send response back to requester
    json j = responseStockDTO;
    std::string jsonStr = j.dump();
    int32_t length = jsonStr.size();
    int32_t net_length = htonl(length);

    send(clientSocket, &net_length, sizeof(net_length), 0);
    send(clientSocket, jsonStr.data(), jsonStr.size(), 0);

    // close(clientSocket);

    return responseStockDTO;
}

// (해석) UC3에서 RequestPrepaymentController에 의해서 호출 당하는 것
// (변경) RequestPrepaymentController에 의해 호출 당하고 자체적으로 stream에 json 변환된거 흘려 보내는 것까지. 그리고 응답을 받아서 boolean 반환.
bool SocketManager::requestPrePayment(int beverageId, int quantity, string authCode, int dstId) {
    RequestPrePaymentDTO requestPrePaymentDTO(beverageId, quantity, authCode, this->srcId, dstId);
    json j = requestPrePaymentDTO;

    //보내기
    int dvmSocket = otherDVMSockets[dstId];

    std::string jsonStr = j.dump();
    int32_t length = jsonStr.size();
    int32_t net_length = htonl(length);

    send(dvmSocket, &net_length, sizeof(net_length), 0);
    send(dvmSocket, jsonStr.data(), jsonStr.size(), 0);

    //읽기
    auto json_string = receive_msg(dvmSocket);

    //json으로 바꾸어서 DTO에 mapping
    nlohmann::json rj = nlohmann::json::parse(json_string);
    ResponsePrePaymentDTO responsePrePaymentDTO = rj;

    return responsePrePaymentDTO.getAvailability();
}

// (해석) UC3에서 다른 DVM의 이 함수를 호출, 그리고 UC6에서 이 함수가 호출 당하며 시작
// (변경) SocketManager의 stream 관찰 함수의 분기처리에 의해 thread가 새로이 생성되어 호출되는 것. 응답으로 다시 stream에 json 흘려보내야 함.
ResponsePrePaymentDTO SocketManager::requestPrePay(int beverageId, int quantity, string authCode, int srcId, int dstId, int clientSocket) {
    ResponsePrePaymentDTO responsePrePaymentDTO =  this->responsePrePaymentController->responsePrePay(beverageId, quantity, authCode);
    responsePrePaymentDTO.setSrcAndDst(this->srcId, srcId);

    // send response back to requester
    json j = responsePrePaymentDTO;
    std::string jsonStr = j.dump();
    int32_t length = jsonStr.size();
    int32_t net_length = htonl(length);

    send(clientSocket, &net_length, sizeof(net_length), 0);
    send(clientSocket, jsonStr.data(), jsonStr.size(), 0);

    // close(clientSocket);

    return responsePrePaymentDTO;
}
