#include "pipeline.h"

namespace obstetrics {

Pipeline::Pipeline(const std::string pipeline_cfg_path) {

    load_jsonf(pipeline_cfg_path, jsondata);
    
    for (int i=0; i<jsondata["spl_det_names"].size(); i++) {
        spl_det_names.push_back(jsondata["spl_det_names"][i].asString());
    }

    spl = new StandardPlane(jsondata["spl_cfg_path"].asString());
    segment = new Segment(jsondata["seg_cfg_path"].asCString());
    measure = new Measure();

    spl_exist_dict.clear();
}

Pipeline::~Pipeline() {

    delete spl;
    delete segment;
    delete measure;

}

void Pipeline::get_roi_img(cv::Rect bbox, cv::Rect &up_bbox, float up_ratio, cv::Mat &roi_img, cv::Mat &img) {
    cv::Size img_size = img.size();
    int x = bbox.x;
    int y = bbox.y;
    int width = bbox.width;
    int height = bbox.height;
    float cx = x + width/2;
    float cy = y + height/2;
    int x1 = std::max(0, int(cx-width*up_ratio/2));
    int x2 = std::min(img_size.width, int(cx+width*up_ratio/2));
    int y1 = std::max(0, int(cy-height*up_ratio/2));
    int y2 = std::min(img_size.height, int(cy+height*up_ratio/2));
    up_bbox = cv::Rect(x1, y1, x2-x1, y2-y1);
    roi_img = img(up_bbox);
}

void Pipeline::manualmeasure(MResult &mresult, std::string measure_name, std::map<std::string, std::string> &measure_name_dict, cv::Mat &img) {
    Json::Value pipelines = jsondata["pipelines"];
    // 包含标准面和测量的pipeline结果
    for (int i=0; i<pipelines.size(); i++) {
        // 标准切面名称
        std::string spl_name = pipelines[i]["name"].asString();
        if (spl_name.compare(measure_name) != 0) continue;

        std::string seg_name;
        std::string measure_type;
        bool roi_flag;
        float up_ratio;

        // 分割模型名称
        seg_name = pipelines[i]["measure"]["segment"].asString();
        // 测量参数
        measure_type = pipelines[i]["measure"]["type"].asString();
        // 测量框子类别
        roi_flag = pipelines[i]["measure"]["roi_flag"].asBool();
        // up_ratio
        up_ratio = pipelines[i]["measure"]["up_ratio"].asFloat();
        Json::Value measure_name_data;
        measure_name_data = pipelines[i]["measure"]["measure_name"];

        std::map<std::string, cv::Mat> mask_dict;
        segment->apply(img, mask_dict, seg_name);
        measure->apply(mask_dict, mresult, img, measure_type);

        Json::Value::Members keys = measure_name_data.getMemberNames();
        for (auto key : keys) {
            measure_name_dict.insert(std::pair<std::string, std::string>(key, measure_name_data[key].asString()));
        }
    }
}

void Pipeline::apply(std::vector<PResult> &presults, std::vector<Detect> &dets, bool reset, cv::Mat &img) {
        
    // 清空spl_exist_dict
    if (reset) {
        spl_exist_dict.clear();
    }

    Json::Value pipelines = jsondata["pipelines"];
    
    // 包含标准面和测量的pipeline结果
    for (int i=0; i<pipelines.size(); i++) {

        /*********************** 参数 *************************/
        // 标准切面名称
        std::string spl_name = pipelines[i]["name"].asString();
        // 是否需要标准切面判定
        bool spl_flag = pipelines[i]["standard_plane"].asBool();

        // 测量参数
        bool measure_flag = pipelines[i].isMember("measure");
        std::string seg_name;
        std::string measure_type;
        bool roi_flag;
        float up_ratio;
        Json::Value measure_name_data;
        if (measure_flag) {
            // 分割模型名称
            seg_name = pipelines[i]["measure"]["segment"].asString();
            // 测量参数
            measure_type = pipelines[i]["measure"]["type"].asString();
            // 测量框子类别
            roi_flag = pipelines[i]["measure"]["roi_flag"].asBool();
            // up_ratio
            up_ratio = pipelines[i]["measure"]["up_ratio"].asFloat();
            // measure_name
            measure_name_data = pipelines[i]["measure"]["measure_name"];
        }
        /*************************************************/ 

        
        /*********** 情况1 ************/
        // 没有标准面，只测量，例如羊水和胎盘
        /*****************************/
        // if (spl_flag == false) {
        //     if (measure_flag) {
        //         std::map<std::string, cv::Mat> mask_dict;
        //         MResult mresult;
        //         segment->apply(img, mask_dict, seg_name);
        //         measure->apply(mask_dict, mresult, img, measure_type);
        //         if ((mresult.biological_indicators.size()!=0) || (mresult.ellipses.size()!=0) || (mresult.lines.size()!=0)) {
        //             PResult presult;
        //             presult.name = spl_name;
        //             presult.has_spl = false;
        //             presult.has_measure = true;
        //             presult.mresult = mresult;
        //             presults.push_back(presult);
        //             continue;
        //         }
        //     }
        // }

        // 标准面结果
        std::vector<SPlane> splanes{};
        // 标准切面识别
        if (spl_flag == true) {
            spl->apply(spl_name, dets, splanes);
        }

        // 删除非标准面
        // std::vector<SPlane>::iterator iter;
        // for (iter=splanes.begin(); iter!=splanes.end();) {
        //     if (iter->flag==false) {
        //         splanes.erase(iter);
        //         continue;
        //     }
        //     iter++;
        // }

        /*********** 情况2 ************/
        // 只有标准面，没有测量
        /*****************************/
        if (measure_flag==false) {
            for (auto &splane : splanes) {
                PResult presult;
                presult.name = spl_name;
                presult.has_spl = true;
                presult.spl = splane;
                presult.has_measure = false;
                if (splane.flag) {
                    if (spl_exist_dict.count(spl_name) != 0) {
                        if (spl_exist_dict[spl_name] < splane.threshold - 0.1f*splane.sum_score) {
                            presult.need_send = true;
                            spl_exist_dict[spl_name] = splane.threshold;
                        }
                    } else {
                        presult.need_send = true;
                        spl_exist_dict[spl_name] = splane.threshold;
                    }
                } else {
                    if (spl_exist_dict.count(spl_name) != 0) continue;
                }
                presults.push_back(presult);
            }
            continue;
        }

        /*********** 情况3 ************/
        // 标准面和测量都存在
        /*****************************/

        // 标准面内测量
        for (auto &splane : splanes) {
            PResult presult;
            presult.name = spl_name;
            presult.has_spl = true;
            presult.spl = splane;
            presult.has_measure = true;
            if (splane.flag) {
                if (spl_exist_dict.count(spl_name) != 0) {
                    if (spl_exist_dict[spl_name] > splane.threshold - 0.1f*splane.sum_score) {
                        presults.push_back(presult);
                        continue;
                    }
                }
                presult.need_send = true;
                cv::Rect bbox = splane.organ.rect;
                cv::Mat roi_img;
                std::map<std::string, cv::Mat> mask_dict;
                MResult mresult;
                cv::Rect up_bbox;
                get_roi_img(bbox, up_bbox, up_ratio, roi_img, img);
                segment->apply(roi_img, mask_dict, seg_name);
                measure->apply(mask_dict, mresult, img, measure_type);

                std::map<std::string, std::string> measure_name_dict;
                Json::Value::Members keys = measure_name_data.getMemberNames();
                for (auto key : keys) {
                    measure_name_dict.insert(std::pair<std::string, std::string>(key, measure_name_data[key].asString()));
                }
                presult.measure_name_dict = measure_name_dict;

                // 直线增加roi
                for (auto &line : mresult.lines) {
                    line.second.first.x += up_bbox.x;
                    line.second.first.y += up_bbox.y;
                    line.second.second.x += up_bbox.x;
                    line.second.second.y += up_bbox.y;
                }
                // 椭圆增加roi
                for (auto &ellipse : mresult.ellipses) {
                    ellipse.second.center.x += up_bbox.x;
                    ellipse.second.center.y += up_bbox.y;
                }
                presult.mresult = mresult;
                spl_exist_dict[spl_name] = splane.threshold;
            } else {
                if (spl_exist_dict.count(spl_name) != 0) continue;
            }
            presults.push_back(presult);
        }
    }

    // 删除切面检测框
    std::vector<Detect>::iterator det_iter=dets.begin();
    for (; det_iter!=dets.end();) {
        int count = std::count(spl_det_names.begin(), spl_det_names.end(), det_iter->name);
        if (count != 0) {
            dets.erase(det_iter);
            continue;
        }
        det_iter++;
    }
}

}