#pragma once
#include <iostream>
#include <nlohmann/json.hpp>

using nlohmann::json;
using namespace std;

class Msg_content_RequestStockDTO{
    public:
        int item_code;
        int item_num;

        Msg_content_RequestStockDTO() = default;

        Msg_content_RequestStockDTO(int item_code, int item_num){
            this->item_code = item_code;
            this->item_num = item_num;
        }
};

class RequestStockDTO{
    public:
        string msg_type;
        int src_id;
        int dst_id;
        Msg_content_RequestStockDTO msg_content;

        RequestStockDTO() = default;

        RequestStockDTO(int beverageId, int quantity, int srcId, int dstId){
            this->msg_type = "req_stock";
            this->src_id = src_id;
            this->dst_id = dst_id;
            this->msg_content = Msg_content_RequestStockDTO(beverageId, quantity);
        }
};


// to_json, from_json 함수 정의
inline void to_json(json& j, const Msg_content_RequestStockDTO& mc) {
    j = json{{"item_code", mc.item_code}, {"item_num", mc.item_num}};
}

inline void from_json(const json& j, Msg_content_RequestStockDTO& mc) {
    j.at("item_code").get_to(mc.item_code);
    j.at("item_num").get_to(mc.item_num);
}

inline void to_json(json& j, const RequestStockDTO& dto) {
    j = json{
        {"msg_type", dto.msg_type},
        {"src_id", dto.src_id},
        {"dst_id", dto.dst_id},
        {"msg_content", dto.msg_content}
    };
}

inline void from_json(const json& j, RequestStockDTO& dto) {
    j.at("msg_type").get_to(dto.msg_type);
    j.at("src_id").get_to(dto.src_id);
    j.at("dst_id").get_to(dto.dst_id);
    j.at("msg_content").get_to(dto.msg_content);
}