#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <cassert>

#include <jsoncpp/json/json.h>

namespace obstetrics {

// 读取json文件
void load_jsonf(std::string jsonfile, Json::Value& jsondata);
// 解析标准切面配置文件
void spl_parser(const std::string spl_cfg_path, std::map<std::string, Json::Value> &spl_cfg_data);
// 解析分割配置文件
void seg_parser(const std::string seg_cfg_path, std::map<std::string, Json::Value> &seg_cfg_data);

}
