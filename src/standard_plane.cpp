#include "standard_plane.h"

namespace obstetrics {

StandardPlane::StandardPlane(const std::string spl_cfg_path) {
    spl_parser(spl_cfg_path, spl_cfg_data);
};

// 标准面识别
void StandardPlane::apply(std::string spl_name, std::vector<Detect> &dets, std::vector<SPlane> &splanes) {

    // 获得配置文件中当前标准面的信息
    Json::Value spl_data = spl_cfg_data[spl_name]; 

    std::string organ_name = spl_data["organ"].asString();

    // 统计所有的标准面数量
    SPlane splane;
    std::vector<Detect>::iterator det_iter=dets.begin();
    for (; det_iter!=dets.end(); det_iter++) {
        Detect det = *det_iter;
        if (organ_name.compare(det.name) == 0) {
            // 正确的解剖结构
            std::map<std::string, std::vector<Detect>> anatomy_dets;
            // 解剖结构数量
            std::map<std::string, int> anatomy_num;
            // 错误的解剖结构
            std::map<std::string, std::vector<Detect>> error_anatomy_dets;
            float score = 0.0f;
            float threshold = 0.0f;
            float sum_score = 0.0f;
            bool flag = identify(det, dets, spl_data, anatomy_dets, anatomy_num, error_anatomy_dets, score, threshold, sum_score);
            splane.flag = flag;
            splane.name = spl_name;
            splane.organ = det;
            splane.score = score;
            splane.threshold = threshold;
            splane.sum_score = sum_score;
            splane.anatomy_dets = anatomy_dets;
            splane.anatomy_num = anatomy_num;
            
            // 统计正确解剖结构
            std::map<std::string, std::vector<Detect>>::iterator iter;
            for (iter = anatomy_dets.begin(); iter != anatomy_dets.end(); iter++) {
                for (int i=0; i<iter->second.size(); i++) {
                    splane.anatomy.push_back(iter->first);
                }
            }

            // 必须包含的解剖结构, 用于降低非标准面输出频率
            bool filter_flag = false;
            if (flag==false) {
                for (int i=0; i<spl_data["necessary"].size(); i++) {
                    std::string necessary = spl_data["necessary"][i].asString();
                    int count = std::count(splane.anatomy.begin(), splane.anatomy.end(), necessary);
                    if (count == 0) {
                        filter_flag = true;
                        break;
                    }
                }
            }
            if (filter_flag) break;

            // 统计错误解剖结构
            for (iter = error_anatomy_dets.begin(); iter != error_anatomy_dets.end(); iter++) {
                for (int i=0; i<iter->second.size(); i++) {
                    splane.error_anatomy.push_back(iter->first);
                }
            }
            splanes.push_back(splane);
            
            // 如果标准面成立, 去除掉organ框
            // if (flag==true) {
            //     dets.erase(det_iter);
            //     --det_iter; // 必须--，删除后后面元素向前移动了
            // }
        }
    }

    // 按得分对标准切面进行排序
    // std::sort(splanes.begin(), splanes.end(), [](SPlane splane1, SPlane splane2) { return splane1.score > splane2.score; });

    // 处理头部三个标准切面
    int num = spl_data["num"].asInt();
    if ((num == 1) && (spl_name.compare(organ_name) != 0)) {
        for (auto &splane : splanes) {
            // 将原始organ框子名称改为标准面名称
            splane.organ.name = splane.name;
        }
    }

}

// 计算解剖结构框和标准面框的交集与自身的占比
void StandardPlane::cal_ios(Detect anatomy, Detect organ, float &ios) {
    ios = (anatomy.rect & organ.rect).area() * 1.0f / anatomy.rect.area();
}

// 过滤掉与标准面框的相交度较低的解剖结构框
bool StandardPlane::identify(Detect organ, std::vector<Detect> dets, Json::Value spl_data, 
                             std::map<std::string, std::vector<Detect>> &anatomy_dets, std::map<std::string, int> &anatomy_num,
                             std::map<std::string, std::vector<Detect>> &error_anatomy_dets, float &score, float &threshold, float &sum_score) {
    
    // 按照prob置信度进行排序
    std::sort(dets.begin(), dets.end(), [](Detect det1, Detect det2) { return det1.prob > det2.prob; });
    
    // 正确的解剖结构
    Json::Value anatomy_array = spl_data["anatomy"];
    // 将array转化为dict
    assert(anatomy_array.isArray());
    std::map<std::string, Json::Value> anatomy_dict;
    for (int i=0; i<anatomy_array.size(); i++) {
        anatomy_dict.insert(std::pair<std::string, Json::Value>(anatomy_array[i]["name"].asString(), anatomy_array[i]));
        // 初始化空的解剖结构检测
        anatomy_dets.insert(std::pair<std::string, std::vector<Detect>>(anatomy_array[i]["name"].asString(), {}));
    }

    // 统计满足当前标准面的正确解剖结构
    for (Detect &det : dets) {
        // 过滤不是当前标准面需要的解剖结构
        if (anatomy_dets.count(det.name) == 0) continue;
        // 解剖结构与标准面ios小于设定值不保存
        float ios;
        cal_ios(det, organ, ios);
        if (ios < anatomy_dict[det.name]["ios"].asFloat()) continue;
        // 当检测到的解剖结构数量超过需求时不在保存
        if (anatomy_dets[det.name].size() == anatomy_dict[det.name]["num"].asInt()) continue;
        anatomy_dets[det.name].push_back(det);
        score += anatomy_dict[det.name]["score"].asFloat();
        threshold += det.prob*anatomy_dict[det.name]["score"].asFloat();
    }

    // 错误的解剖结构
    Json::Value error_anatomy_array = spl_data["error_anatomy"];
    // 将array转化为dict
    assert(error_anatomy_array.isArray());
    std::map<std::string, Json::Value> error_anatomy_dict = {};
    for (int i=0; i<error_anatomy_array.size(); i++) {
        error_anatomy_dict.insert(std::pair<std::string, Json::Value>(error_anatomy_array[i]["name"].asString(), error_anatomy_array[i]));
        // 初始化空的解剖结构检测
        error_anatomy_dets.insert(std::pair<std::string, std::vector<Detect>>(error_anatomy_array[i]["name"].asString(), {}));
    }

    // 统计满足当前标准面的错误解剖结构
    for (Detect &det : dets) {
        // 过滤不是当前标准面需要的解剖结构
        if (error_anatomy_dets.count(det.name) == 0) continue;
        // 解剖结构与标准面ios小于设定值不保存
        float ios;
        cal_ios(det, organ, ios);
        if (ios < error_anatomy_dict[det.name]["ios"].asFloat()) continue;
        // 当检测到的解剖结构数量超过需求时不在保存
        if (error_anatomy_dets[det.name].size() == error_anatomy_dict[det.name]["num"].asInt()) continue;
        error_anatomy_dets[det.name].push_back(det);
    }

    // 判断解剖结构数量是否满足要求
    bool flag = true;
    std::map<std::string, std::vector<Detect>>::iterator iter;
    // 正确的解剖结构满足要求
    for (iter = anatomy_dets.begin(); iter != anatomy_dets.end(); iter++) {
        // 解剖结构数量
        anatomy_num[iter->first] = anatomy_dict[iter->first]["num"].asInt();
        // 标准面总分
        sum_score += anatomy_dict[iter->first]["num"].asInt()*anatomy_dict[iter->first]["score"].asFloat();
        if(iter->second.size() != anatomy_dict[iter->first]["num"].asInt()) {
            flag = false;
        }
    }
    // 错误的解剖结构不能有
    for (iter = error_anatomy_dets.begin(); iter != error_anatomy_dets.end(); iter++) {
        if(iter->second.size()>0) {
            flag = false;
            break;
        }
    }
    return flag;
}

}