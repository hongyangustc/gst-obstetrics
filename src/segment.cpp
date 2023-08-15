#include "segment.h"

namespace obstetrics {

    Segment::Segment(const std::string seg_cfg_path) {
        seg_parser(seg_cfg_path, seg_cfg_data);
        std::map<std::string, Json::Value>::iterator iter;
        for (iter = seg_cfg_data.begin(); iter != seg_cfg_data.end(); iter++) {
            // 初始化所有paddleseg类型模型
            std::string model = iter->second["model"].asString();
            if (model.compare("PaddleSeg") == 0) {
                std::string engine_file = iter->second["args"]["engine_file"].asString();
                int width = iter->second["args"]["width"].asInt();
                int height = iter->second["args"]["height"].asInt();
                int class_num = iter->second["args"]["class_num"].asInt();
                char const * input_name = iter->second["args"]["input_name"].asCString();
                char const * output_name = iter->second["args"]["output_name"].asCString();
                paddleseg.insert(std::pair<std::string, PaddleSeg *> (iter->first, new PaddleSeg(engine_file, width, height, class_num, input_name, output_name)));
            }
            // mmseg
        }
    }

    Segment::~Segment() {
        // paddleseg
        std::map<std::string, PaddleSeg *>::iterator iter;
        for (iter = paddleseg.begin(); iter != paddleseg.end(); iter++) {
            delete iter->second;
        }
        // mmseg
    }

    void Segment::apply(cv::Mat img, std::map<std::string, cv::Mat> &mask_dict, std::string name) {
        // paddleseg
        if (paddleseg.count(name) != 0) {
            std::vector<cv::Mat> masks = paddleseg[name]->seginfer(img);
            Json::Value class_name = seg_cfg_data[name]["args"]["class_name"];
            for (int i=0; i < class_name.size(); i++) {
                mask_dict.insert(std::pair<std::string, cv::Mat>(class_name[i].asString(), masks[i]));
            }
        }
        // mmseg
    }

}
