#pragma once
#include <string>
#include <nlohmann/json.hpp>

using nlohmann::json;
using namespace std;

class Msg_content_ResponseStockDTO{
    public:
        int item_code;
        int item_num;
        int coor_x;
        int coor_y;

        Msg_content_ResponseStockDTO() = default;

        Msg_content_ResponseStockDTO(int beverageId, int quantity, int x, int y){
            this->item_code = beverageId;
            this->item_num = quantity;
            this->coor_x = x;
            this->coor_y = y;
        }
};

class ResponseStockDTO{
    public:
        string msg_type;
        int src_id;
        int dst_id;
        Msg_content_ResponseStockDTO msg_content;

        ResponseStockDTO() = default;

        ResponseStockDTO(int beverageId, int quantity, int x, int y){
            this->msg_type = "resp_stock";
            this->msg_content = Msg_content_ResponseStockDTO(beverageId, quantity, x,y);
        }

        void setSrcAndDst(int srcId, int dstId){
            this->src_id = srcId;
            this->dst_id = dstId;
        }

        int getX() const {
            return msg_content.coor_x;
        }

        int getY() const {
            return msg_content.coor_y;
        }

        int getSrcId() const {
            return src_id;      // 해당 DTO를 보낸 DVM의 ID
        }
};

// to_json, from_json 함수 정의
inline void to_json(json& j, const Msg_content_ResponseStockDTO& mc) {
    j = json{{"item_code", mc.item_code}, {"item_num", mc.item_num}, {"coor_x", mc.coor_x}, {"coor_y", mc.coor_y}};
}

inline void from_json(const json& j, Msg_content_ResponseStockDTO& mc) {
    j.at("item_code").get_to(mc.item_code);
    j.at("item_num").get_to(mc.item_num);
    j.at("coor_x").get_to(mc.coor_x);
    j.at("coor_y").get_to(mc.coor_y);
}

inline void to_json(json& j, const ResponseStockDTO& dto) {
    j = json{
        {"msg_type", dto.msg_type},
        {"src_id", dto.src_id},
        {"dst_id", dto.dst_id},
        {"msg_content", dto.msg_content}
    };
}

inline void from_json(const json& j, ResponseStockDTO& dto) {
    j.at("msg_type").get_to(dto.msg_type);
    j.at("src_id").get_to(dto.src_id);
    j.at("dst_id").get_to(dto.dst_id);
    j.at("msg_content").get_to(dto.msg_content);
}
