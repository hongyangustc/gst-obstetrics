#include "config_parser.h"

namespace obstetrics
{

    void load_jsonf(const std::string jsonfile, Json::Value &jsondata) {
        std::ifstream ifs;
        ifs.open(jsonfile);

        Json::CharReaderBuilder builder;
        builder["collectComments"] = true;
        JSONCPP_STRING errs;
        if (!parseFromStream(builder, ifs, &jsondata, &errs))
        {
            std::cout << errs << std::endl;
            return;
        }
    }

    void spl_parser(const std::string spl_cfg_path, std::map<std::string, Json::Value> &spl_cfg_data) {
        Json::Value jsondata;
        load_jsonf(spl_cfg_path, jsondata);
        
        assert(jsondata["planes"].isArray());
        Json::Value planes = jsondata["planes"];
        for (int i=0; i<planes.size(); i++) {
            spl_cfg_data.insert(std::pair<std::string, Json::Value>(planes[i]["name"].asString(), planes[i]));
        }
        std::cout << "标准面配置文件加载完成!" << std::endl;
    }

    void seg_parser(const std::string seg_cfg_path, std::map<std::string, Json::Value> &seg_cfg_data) {
        Json::Value jsondata;
        load_jsonf(seg_cfg_path, jsondata);
        
        assert(jsondata["segments"].isArray());
        Json::Value segments = jsondata["segments"];
        for (int i=0; i<segments.size(); i++) {
            seg_cfg_data.insert(std::pair<std::string, Json::Value>(segments[i]["name"].asString(), segments[i]));
        }
        std::cout << "分割配置文件加载完成!" << std::endl;
    }

}