#pragma once
#include <string>
#include <nlohmann/json.hpp>

using nlohmann::json;
using namespace std;

class Msg_content_RequestPrePaymentDTO{
    public:
        int item_code;
        int item_num;
        string cert_code;

        Msg_content_RequestPrePaymentDTO() = default;

        Msg_content_RequestPrePaymentDTO(int item_code, int item_num, string cert_code){
            this->item_code = item_code;
            this->item_num = item_num;
            this->cert_code =cert_code;
        }
};

class RequestPrePaymentDTO{
    public: 
        string msg_type;
        int src_id;
        int dst_id;
        Msg_content_RequestPrePaymentDTO msg_content;

        RequestPrePaymentDTO() = default;

        RequestPrePaymentDTO(int beverageId, int quantity, string cert_code, int src_id, int dst_id){
            this->msg_type = "req_prepay";
            this->src_id = src_id;
            this->dst_id = dst_id;
            this->msg_content = Msg_content_RequestPrePaymentDTO(beverageId, quantity, cert_code);
        }
};

// to_json, from_json 함수 정의
inline void to_json(json& j, const Msg_content_RequestPrePaymentDTO& mc) {
    j = json{{"item_code", mc.item_code}, {"item_num", mc.item_num}, {"cert_code", mc.cert_code}};
}

inline void from_json(const json& j, Msg_content_RequestPrePaymentDTO& mc) {
    j.at("item_code").get_to(mc.item_code);
    j.at("item_num").get_to(mc.item_num);
    j.at("cert_code").get_to(mc.cert_code);
}

inline void to_json(json& j, const RequestPrePaymentDTO& dto) {
    j = json{
        {"msg_type", dto.msg_type},
        {"src_id", dto.src_id},
        {"dst_id", dto.dst_id},
        {"msg_content", dto.msg_content}
    };
}

inline void from_json(const json& j, RequestPrePaymentDTO& dto) {
    j.at("msg_type").get_to(dto.msg_type);
    j.at("src_id").get_to(dto.src_id);
    j.at("dst_id").get_to(dto.dst_id);
    j.at("msg_content").get_to(dto.msg_content);
}