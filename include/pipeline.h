#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "standard_plane.h"
#include "measure.h"
#include "segment.h"
#include "config_parser.h"
#include "utils.h"

namespace obstetrics {
    
class Pipeline {
    public:
        Pipeline(const std::string pipeline_cfg_path);
        ~Pipeline();
        void apply(std::vector<PResult> &presults, std::vector<Detect> &dets, bool reset, cv::Mat &img);
        void manualmeasure(MResult &mresult, std::string measure_name, std::map<std::string, std::string> &measure_name_dict, cv::Mat &img);
        void get_roi_img(cv::Rect bbox, cv::Rect &up_bbox, float up_ratio, cv::Mat &roi_img, cv::Mat &img);
    private:
        Json::Value jsondata;
        StandardPlane *spl = nullptr;
        Measure *measure = nullptr;
        Segment *segment = nullptr;
        std::vector<std::string> spl_det_names;
        // 用于更新score更大切面
        std::map<std::string, float> spl_exist_dict;
};

}