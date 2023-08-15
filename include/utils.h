#pragma once

#include <iostream>
#include <string>
#include <vector>

// opencv
#include <opencv2/opencv.hpp>

// 检测
struct Detect
{
    std::string name;
    float prob;
    cv::Rect rect;
};

// 标准切面
struct SPlane
{
    bool flag = false; // 切面是否成立
    std::string name; // 切面名称
    Detect organ; // 切面框
    float score; // 切面得分
    float threshold; // 切面更新分数
    float sum_score; // 切面总分
    std::vector<std::string> anatomy; // 切面引导显示
    std::map<std::string, std::vector<Detect>> anatomy_dets; // 解剖结构检测框
    std::map<std::string, int> anatomy_num; // 满足标准切面的解剖结构数量
    std::vector<std::string> error_anatomy; // 错误的解剖结构
};

// 测量结果
struct MResult
{
    bool flag = false;
    // 生物学指标
    std::vector<std::pair<std::string, double>> biological_indicators;
    // 测量线
    std::vector<std::pair<std::string, std::pair<cv::Point, cv::Point>>> lines;
    // 轮廓
    // std::vector<std::pair<std::string, std::vector<cv::Point>>> contours;
    // 头围和腹围拟合椭圆
    std::vector<std::pair<std::string, cv::RotatedRect>> ellipses;
};

// 包含标准面和测量的pipeline结果
struct PResult
{
    bool need_send = false;
    std::string name;
    // 标准面信息
    bool has_spl = false;
    SPlane spl;
    // 测量信息
    bool has_measure = false;
    MResult mresult;
    std::map<std::string, std::string> measure_name_dict;
};