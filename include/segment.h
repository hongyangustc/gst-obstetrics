#pragma once

/*****************/
// boost::variant
// boost::any
// 反射
// 注册
/****************/
#include <iostream>
#include <string>
#include <vector>

#include "paddleseg.h"
#include "config_parser.h"

namespace obstetrics {
    
class Segment {
    
    public:
        Segment(const std::string seg_cfg_path);
        void apply(cv::Mat img, std::map<std::string, cv::Mat> &mask_dict, std::string name);
        ~Segment();
    private:
        std::map<std::string, Json::Value> seg_cfg_data;
        std::map<std::string, PaddleSeg *> paddleseg;
        // std::map<std::string, PaddleSeg *> mmseg;
 
};

}