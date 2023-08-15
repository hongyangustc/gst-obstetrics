#pragma once

#include <iostream>
#include <string>
#include <vector>

// opencv
#include <opencv2/opencv.hpp>

#include "utils.h"

namespace obstetrics {

// 羊水深度定义直线
struct Line 
{
    int start=0;
    int end=0;
    int len=0;
};

class Measure {
    public:
        Measure() {};
        ~Measure() {};
        // 腹围测量
        bool ACMeasure(cv::Mat mask, double &AC, cv::RotatedRect &ellipse, cv::Mat &img);
        // 头围和双顶径测量
        bool HCBPDMeasure(cv::Mat inner_mask, cv::Mat outer_mask, double &HC, double &BPD, std::pair<cv::Point, cv::Point> &bpd_line, cv::RotatedRect &ellipse, cv::Mat &img);
        // 股骨测量
        bool FLMeasure(cv::Mat mask, double &FL, std::pair<cv::Point, cv::Point> &line, cv::Mat &img);
        // 羊水深度测量
        bool AFVMeasure(cv::Mat mask, double &AFV, std::pair<cv::Point, cv::Point>& vertical, cv::Mat& img);
        // 胎盘厚度测量
        bool PTMeasure(cv::Mat mask, double &PT, std::pair<cv::Point, cv::Point>& thickness, cv::Mat& img);
        // 侧脑室宽度测量
        bool LVMeasure(cv::Mat mask, double &LV, std::pair<cv::Point, cv::Point>& thickness, cv::Mat& img);
        // 小脑横径测量
        bool CBMeasure(cv::Mat mask, double &CB, std::pair<cv::Point, cv::Point>& line, cv::Mat &img);
        void apply(std::map<std::string, cv::Mat> &mask_dict, MResult &result, cv::Mat &img, std::string name);

    private:
        // 查找轮廓降序排列
        void findsortContours(cv::Mat mask, std::vector<std::vector<cv::Point>> &contours);
        // 直线方程一般式
        void GeneralEquation(cv::Point pnt1, cv::Point pnt2, std::vector<float> &ABC);
        // 角度转弧度
        void angle2radian(float &angle);
        // 弧度转角度
        void radian2angle(float &angle);
        // 求两点距离
        void Point2PointDist(cv::Point& a, cv::Point& b, double &dist);
        double Point2PointDist(cv::Point& a, cv::Point& b);
        // 找最远点对
        void findFurthestPointPair(std::vector<cv::Point> points, cv::Point &pnt1, cv::Point &pnt2);
        // 找最近点对
        void findNearestPointPair(std::vector<cv::Point> points, cv::Point &pnt1, cv::Point &pnt2);
        // 求两个向量的叉积
        double CrossProduct(cv::Point p0, cv::Point p1, cv::Point p2);
        // 旋转卡壳
        void RotateCalipers(std::vector<cv::Point> cnts, std::pair<cv::Point, cv::Point>& line, double& length);
        void get_max_len(uchar* ptr, Line& max_line, int len);
        // 直线点斜式转两点式
        void linePS2TP(cv::Point pnt, float angle, cv::Point& point1, cv::Point& point2, int rows, int cols);
        // 旋转图像
        void rotateImg(cv::Mat src, cv::Mat &dst, double angle, cv::Point center);
        // 旋转点
        void rotatePoint(cv::Point src, cv::Point &dst, cv::Point center, double angle);

};

}