#pragma once
#include <string>
#include <nlohmann/json.hpp>

using nlohmann::json;
using namespace std;

class Msg_content_ResponsePrePaymentDTO{
    public:
        int item_code;
        int item_num;
        bool availability;

        Msg_content_ResponsePrePaymentDTO() = default;

        Msg_content_ResponsePrePaymentDTO(int item_code, int item_num, bool availability){
            this->item_code = item_code;
            this->item_num = item_num;
            this->availability =availability;
        }

        bool getAvailability(){
            return this->availability;
        }
};

class ResponsePrePaymentDTO{
    public: 
        string msg_type;
        int src_id;
        int dst_id;
        Msg_content_ResponsePrePaymentDTO msg_content;
        
        ResponsePrePaymentDTO() = default;

        ResponsePrePaymentDTO(int beverageId, int quantity, bool availability){
            this->msg_type = "resp_prepay";
            this->msg_content = Msg_content_ResponsePrePaymentDTO(beverageId, quantity, availability);
        }

        bool getAvailability(){
            return this->msg_content.getAvailability();
        }

        void setSrcAndDst(int src_id, int dst_id){
            this->src_id = src_id;
            this->dst_id = dst_id;
        }
};

// to_json, from_json 함수 정의
inline void to_json(json& j, const Msg_content_ResponsePrePaymentDTO& mc) {
    j = json{{"item_code", mc.item_code}, {"item_num", mc.item_num}, {"availability", mc.availability}};
}

inline void from_json(const json& j, Msg_content_ResponsePrePaymentDTO& mc) {
    j.at("item_code").get_to(mc.item_code);
    j.at("item_num").get_to(mc.item_num);
    j.at("availability").get_to(mc.availability);
}

inline void to_json(json& j, const ResponsePrePaymentDTO& dto) {
    j = json{
        {"msg_type", dto.msg_type},
        {"src_id", dto.src_id},
        {"dst_id", dto.dst_id},
        {"msg_content", dto.msg_content}
    };
}

inline void from_json(const json& j, ResponsePrePaymentDTO& dto) {
    j.at("msg_type").get_to(dto.msg_type);
    j.at("src_id").get_to(dto.src_id);
    j.at("dst_id").get_to(dto.dst_id);
    j.at("msg_content").get_to(dto.msg_content);
}
