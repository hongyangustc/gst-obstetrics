#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "utils.h"
#include "config_parser.h"

namespace obstetrics {

class StandardPlane
{
public:
    StandardPlane(const std::string spl_cfg_path);
    ~StandardPlane() {};

    void apply(std::string spl_name, std::vector<Detect> &dets, std::vector<SPlane> &splanes);
    void cal_ios(Detect anatomy, Detect organ, float &ios);
    bool identify(Detect organ, std::vector<Detect> dets, Json::Value spl_data, 
                  std::map<std::string, std::vector<Detect>> &anatomy_dets, std::map<std::string, int> &anatomy_num,
                  std::map<std::string, std::vector<Detect>> &error_anatomy_dets, float &score, float &threshold, float &sum_score); 

private:
    std::map<std::string, Json::Value> spl_cfg_data;
};

}