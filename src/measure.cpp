#include "measure.h"

namespace obstetrics {

// 查找轮廓降序排列
void Measure::findsortContours(cv::Mat mask, std::vector<std::vector<cv::Point>> &contours)
{
    // 二值化
    cv::Mat thresh;
    cv::threshold(mask, thresh, 127, 255, cv::THRESH_BINARY);
    // 查找轮廓
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(thresh, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_NONE);
    std::sort(contours.begin(), contours.end(), [](std::vector<cv::Point> contour1, std::vector<cv::Point> contour2)
              { return cv::contourArea(contour1) > cv::contourArea(contour2); });
}

// 直线方程一般式
void Measure::GeneralEquation(cv::Point pnt1, cv::Point pnt2, std::vector<float> &ABC)
{
    ABC = {(float)(pnt2.y - pnt1.y),
           (float)(pnt1.x - pnt2.x),
           (float)(pnt2.x * pnt1.y - pnt1.x * pnt2.y)};
}

/*********************************************/
// 腹围测量
/********************************************/
bool Measure::ACMeasure(cv::Mat mask, double &AC, cv::RotatedRect &ellipse, cv::Mat &img) {

    // 椭圆拟合
    mask.convertTo(mask, CV_8U);
    std::vector<std::vector<cv::Point>> contours;
    findsortContours(mask, contours);
    if (contours.size() < 1)
        return false;
    // cv::drawContours(img, contours, 0, cv::Scalar(255, 0, 0), 1);
    ellipse = cv::fitEllipse(contours[0]);
    cv::ellipse(img, ellipse, cv::Scalar(0, 0, 255));
    cv::Size2f size = ellipse.size;
    std::cout << "长轴：" << size.height << "；短轴：" << size.width << std::endl;
    float angle = ellipse.angle;
    std::cout << "角度：" << angle << std::endl;

    // 腹围
    AC = CV_2PI*size.width/2 + 4*(size.height-size.width)/2;
    std::cout << "腹围：" << AC << std::endl;
    return true;
}

// 角度转弧度
void Measure::angle2radian(float &angle) {
    angle *= CV_PI/180;
}

// 弧度转角度
void Measure::radian2angle(float &angle) {
    angle *= 180/CV_PI;
}

// 求两点距离
void Measure::Point2PointDist(cv::Point& a, cv::Point& b, double &dist) {
    dist = sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}
double Measure::Point2PointDist(cv::Point& a, cv::Point& b) {
    double dist = sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
    return dist;
}

// 找最远点对
void Measure::findFurthestPointPair(std::vector<cv::Point> points, cv::Point &pnt1, cv::Point &pnt2) {
    int len = points.size();
    if (len < 2) return;
    double dist;
    double max_dist = 0;
    int max_i, max_j;
    for (int i=0; i<len; i++) {
        for (int j=i+1; j<len; j++) {
            Point2PointDist(points[i], points[j], dist);
            if (dist>max_dist) {
                max_dist = dist;
                max_i = i;
                max_j = j;
            }
        }
    }
    pnt1 = points[max_i];
    pnt2 = points[max_j];
}

// 找最近点对
void Measure::findNearestPointPair(std::vector<cv::Point> points, cv::Point &pnt1, cv::Point &pnt2) {
    int len = points.size();
    if (len < 2) return;
    double dist;
    double max_dist = 0;
    int max_i, max_j;
    for (int i=0; i<len; i++) {
        for (int j=i+1; j<len; j++) {
            Point2PointDist(points[i], points[j], dist);
            if (dist>max_dist) {
                max_dist = dist;
                max_i = i;
                max_j = j;
            }
        }
    }
    pnt1 = points[max_i];
    pnt2 = points[max_j];
}

// 头围和双顶径测量
bool Measure::HCBPDMeasure(cv::Mat inner_mask, cv::Mat outer_mask, double &HC, double &BPD, std::pair<cv::Point, cv::Point> &bpd_line, cv::RotatedRect &ellipse, cv::Mat &img) {

    /***************** 外轮廓 *******************/
    outer_mask.convertTo(outer_mask, CV_8U);
    std::vector<std::vector<cv::Point>> outer_contours;
    findsortContours(outer_mask, outer_contours);
    if (outer_contours.size() < 1)
        return false;
    cv::drawContours(img, outer_contours, 0, cv::Scalar(255, 0, 0), 1);
    ellipse = cv::fitEllipse(outer_contours[0]);
    cv::ellipse(img, ellipse, cv::Scalar(0, 0, 255));
    cv::Point2f center = ellipse.center;
    cv::Size2f size = ellipse.size;
    std::cout << "长轴：" << size.height << "；短轴：" << size.width << std::endl;
    float angle = ellipse.angle;
    std::cout << "角度：" << angle << std::endl;
    // 角度转弧度
    angle2radian(angle);

    // 短轴
    cv::Point minor_point1;
    minor_point1.x = center.x -0.5*size.width*cos(angle);
    minor_point1.y = center.y -0.5*size.width*sin(angle);
    cv::Point minor_point2; 
    minor_point2.x = center.x + 0.5*size.width*cos(angle);
    minor_point2.y = center.y + 0.5*size.width*sin(angle);

    // 头围
    HC = CV_2PI*size.width/2 + 4*(size.height-size.width)/2;
    std::cout << "头围：" << HC << std::endl;

    /***************** 内轮廓 *******************/
    inner_mask.convertTo(inner_mask, CV_8U);
    std::vector<std::vector<cv::Point>> inner_contours;
    findsortContours(inner_mask, inner_contours);
    if (inner_contours.size() < 1)
        return false;
    cv::drawContours(img, inner_contours, 0, cv::Scalar(0, 255, 0), 1);

    // 短轴直线方程
    std::vector<float> ABC;
    GeneralEquation(minor_point1, minor_point2, ABC);
    
    // 直线与内轮廓交点
    std::vector<cv::Point> points;
    cv::Point point1, point2;
    int cnt_len = inner_contours[0].size();
    for (int i = 0; i < cnt_len; i++)
    {
        point1 = inner_contours[0][i];
        point2 = inner_contours[0][(i + 1) % cnt_len];
        if (((ABC[0] * point1.x + ABC[1] * point1.y + ABC[2]) * (ABC[0] * point2.x + ABC[1] * point2.y + ABC[2])) <= 0)
        {
            points.push_back(point1);
        }
    }
    cv::Point inner_point1;
    cv::Point inner_point2;
    findFurthestPointPair(points, inner_point1, inner_point2);
    // cv::line(img, inner_point1, inner_point2, cv::Scalar(255, 0, 0), 1);

    // 直线与外轮廓交点
    points.clear();
    cnt_len = outer_contours[0].size();
    for (int i = 0; i < cnt_len; i++)
    {
        point1 = outer_contours[0][i];
        point2 = outer_contours[0][(i + 1) % cnt_len];
        if (((ABC[0] * point1.x + ABC[1] * point1.y + ABC[2]) * (ABC[0] * point2.x + ABC[1] * point2.y + ABC[2])) <= 0)
        {
            points.push_back(point1);
        }
    }
    cv::Point outer_point1;
    cv::Point outer_point2;
    findFurthestPointPair(points, outer_point1, outer_point2);
    // cv::line(img, outer_point1, outer_point2, cv::Scalar(255, 0, 0), 1);

    points.clear();
    points.push_back(inner_point1);
    points.push_back(inner_point2);
    points.push_back(outer_point1);
    points.push_back(outer_point2);
    // std::sort(points.begin(), points.end(), [](cv::Point point1, cv::Point point2) \
    //           { return pow(point1.x, 2)+pow(point1.y, 2) > pow(point2.x, 2)+pow(point2.y, 2); });
    // 使直线上的点按照顺序排列，如果头部刚好水平失效
    std::sort(points.begin(), points.end(), [](cv::Point point1, cv::Point point2) { return point1.y > point2.y; });
    float len1, len2;
    len1 = sqrt(pow(points[0].x - points[1].x, 2) + pow(points[0].y - points[1].y, 2));
    len2 = sqrt(pow(points[2].x - points[3].x, 2) + pow(points[2].y - points[3].y, 2));

    cv::Point bpd_point1, bpd_point2;
    if (len1 > len2) {
        bpd_point1 = points[1];
        bpd_point2 = points[3];
    } else {
        bpd_point1 = points[0];
        bpd_point2 = points[2];
    }
    cv::line(img, bpd_point1, bpd_point2, cv::Scalar(255, 0, 0), 1);

    bpd_line.first = bpd_point1;
    bpd_line.second = bpd_point2;

    // 双顶径
    BPD = sqrt(pow(bpd_point1.x - bpd_point2.x, 2) + pow(bpd_point1.y - bpd_point2.y, 2));
    std::cout << "双顶径: " << BPD << std::endl;
    return true;

}

// 求两个向量的叉积
double Measure::CrossProduct(cv::Point p0, cv::Point p1, cv::Point p2) {
    return (p1-p0).cross((p2-p0));
}

// 旋转卡壳
void Measure::RotateCalipers(std::vector<cv::Point> cnts, std::pair<cv::Point, cv::Point>& line, double& length) {
    int n = cnts.size();
    double max_dist = 0;
    cnts.push_back(cnts[0]);
    int j = 1;
    for (int i=0; i<n; i++) {
        while(CrossProduct(cnts[i], cnts[i+1], cnts[j+1]) > CrossProduct(cnts[i], cnts[i+1], cnts[j])) {
            j = (j+1) % n;
        }
        double dist1;
        Point2PointDist(cnts[j], cnts[i], dist1);
        double dist2;
        Point2PointDist(cnts[j], cnts[i+1], dist2);
        double tmp_dist = std::max(dist1, dist2);
        if (max_dist < tmp_dist) {
            max_dist = tmp_dist;
            length = max_dist;
            line.first = cnts[j];
            if (dist1 > dist2) {
                line.second = cnts[i];
            } else {
                line.second = cnts[i+1];
            }
        }
    }
}

// 股骨测量
bool Measure::FLMeasure(cv::Mat mask, double &FL, std::pair<cv::Point, cv::Point> &line, cv::Mat &img) {
    mask.convertTo(mask, CV_8U);
    std::vector<std::vector<cv::Point>> contours;
    findsortContours(mask, contours);
    if (contours.size() < 1)
        return false;
    cv::drawContours(img, contours, 0, cv::Scalar(255, 0, 0), 1);

    // 凸包, 减少点数
    std::vector<cv::Point> hull;
    cv::convexHull(contours[0], hull);
    std::cout << hull.size() << std::endl;

    /************** 旋转卡壳，求凸包最大直径 ***************/
    RotateCalipers(hull, line, FL);
    cv::line(img, line.first, line.second, cv::Scalar(0, 255, 0), 1);
    return true;
}

void Measure::get_max_len(uchar* ptr, Line& max_line, int len) {
    Line line;
    bool flag = false;
    for (int j=0; j<len; j++) {
        if (!flag && ptr[j] == 255) {
            flag = true;
            line.start = j;
        }
        if (flag && ptr[j] != 255) {
            line.len = line.end - line.start;
            flag = false;
            if (line.len > max_line.len) {
                max_line = line;
            }
        }
        if (flag && ptr[j] == 255) line.end = j;
    }
}

// 羊水深度测量
bool Measure::AFVMeasure(cv::Mat mask, double &AFV, std::pair<cv::Point, cv::Point>& vertical, cv::Mat& img) {

    // 查找羊水分割最大区域
    std::vector<std::vector<cv::Point>> contours;
    findsortContours(mask, contours);
    if (contours.size() < 1)
        return false;
    // cv::drawContours(img, contours, 0, cv::Scalar(0, 255, 0), 1);

    // 羊水分割最大区域外接矩形
    cv::Rect rect = cv::boundingRect(contours[0]);
    // cv::rectangle(img, rect, cv::Scalar(0, 0, 255), 1);

    // 羊水分割最大区域
    mask = 0;
    cv::drawContours(mask, contours, 0, 255, -1);

    // 羊水图像与分割crop
    cv::Mat img_crop = img(rect);
    cv::Mat mask_crop = mask(rect);
    cv::Mat gray;
    cv::cvtColor(img_crop, gray, cv::COLOR_BGR2GRAY);

    // OTSU二值化+开运算
    cv::Mat thresh;
    cv::threshold(gray, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    thresh.setTo(0, mask_crop==0);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::Mat close;
    cv::morphologyEx(thresh, close, cv::MORPH_CLOSE, kernel); // 闭运算去除细小区域
    cv::transpose(close, mask_crop);

    // 查找
    int width = rect.width;
    int height = rect.height;
    Line max_line;
    int max_w = 0;
    for (int i=0; i<width; i++) {
        uchar* ptr = mask_crop.ptr<uchar>(i);
        Line line;
        get_max_len(ptr, line, height);
        if (line.len > max_line.len) {
            max_line = line;
            max_w = i;
        }
    }
    vertical.first.x = vertical.second.x = max_w+rect.x;
    vertical.first.y = max_line.start+rect.y;
    vertical.second.y = max_line.end+rect.y;
    cv::line(img, vertical.first, vertical.second, cv::Scalar(0, 255, 0), 1);

    AFV = abs(vertical.second.y - vertical.first.y);
    std::cout << "羊水深度: " << AFV << std::endl;
    return true;
}

// 直线点斜式转两点式
void Measure::linePS2TP(cv::Point pnt, float angle, cv::Point& point1, cv::Point& point2, int rows, int cols) {
    int x = pnt.x;
    int y = pnt.y;
    float k = -tan(angle*CV_PI/180);
    if (fabs(k) < 1) {
        int lefty = int(y-k*x);
        int righty = int((cols - x) * k + y);
        point1.x = cols;
        point1.y = righty;
        point2.x = 0;
        point2.y = lefty;
    } else {
        int upx = int(-y / k + x);
        int downx = int((rows - y)/k + x);
        point1.x = upx;
        point1.y = 0;
        point2.x = downx;
        point2.y = rows;
    }
}

// 胎盘厚度测量
bool Measure::PTMeasure(cv::Mat mask, double &PT, std::pair<cv::Point, cv::Point>& thickness, cv::Mat& img) {
    int rows = img.rows;
    int cols = img.cols;

    // 查找胎盘分割最大区域
    std::vector<std::vector<cv::Point>> contours;
    findsortContours(mask, contours);
    if (contours.size() < 1)
        return false;
    cv::drawContours(img, contours, 0, cv::Scalar(0, 255, 0), 1);

    // 胎盘分割最大区域
    mask = 0;
    cv::drawContours(mask, contours, 0, 255, -1);

    //  图像距离变换
    cv::Mat dist_trans;
    cv::distanceTransform(mask, dist_trans, cv::DIST_L2, cv::DIST_MASK_3, cv::DIST_LABEL_PIXEL);// 距离变换
    double min_val, max_val;
    cv::Point min_loc, max_loc;
    cv::minMaxLoc(dist_trans, &min_val, &max_val, &min_loc, &max_loc);
    cv::circle(img, max_loc, 2, cv::Scalar(255, 0, 0), 2);

    float min_dist = sqrt(pow(rows, 2) + pow(cols, 2));
    cv::Point min_point1, min_point2;
    int cnt_len = contours[0].size();
    std::vector<cv::Point> points;
    cv::Point point1, point2;

    for (float angle=0; angle <=360; ) {

        linePS2TP(max_loc, angle, point1, point2, rows, cols);
        // 指针直线方程一般式
        std::vector<float> ABC;
        GeneralEquation(point1, point2, ABC);

        // 直线与轮廓交点
        points.clear();
        for (int i = 0; i < cnt_len; i++) {
            point1 = contours[0][i];
            point2 = contours[0][(i + 1) % cnt_len];
            if (((ABC[0] * point1.x + ABC[1] * point1.y + ABC[2]) * (ABC[0] * point2.x + ABC[1] * point2.y + ABC[2])) <= 0)
            {
                points.push_back(point1);
            }
        }

        // 轮廓交点到max_loc的距离排序
        std::sort(points.begin(), points.end(), [&](cv::Point pt1, cv::Point pt2) 
        { return Point2PointDist(pt1, max_loc) < Point2PointDist(pt2, max_loc); });

        // 选择两个异向距离最近的交点
        point1 = points[0]; 
        for (auto point : points) {
            if ((point1-max_loc).dot(point-max_loc) < 0) {
                point2 = point;
                break;
            }
        }

        double dist;
        Point2PointDist(point1, point2, dist);

        if (dist < min_dist) {
            min_point1 = point1;
            min_point2 = point2;
            min_dist = dist;
        }

        angle = angle + 1;
    }
    thickness.first = min_point1;
    thickness.second = min_point2;
    PT = min_dist;
    std::cout << "胎盘厚度: " << PT << std::endl;
    cv::line(img, thickness.first, thickness.second, cv::Scalar(0, 0, 255), 1);
    return true;
}

// 侧脑室宽度测量
bool Measure::LVMeasure(cv::Mat mask, double &LV, std::pair<cv::Point, cv::Point>& thickness, cv::Mat& img) {
    int rows = img.rows;
    int cols = img.cols;

    // 查找胎盘分割最大区域
    std::vector<std::vector<cv::Point>> contours;
    findsortContours(mask, contours);
    if (contours.size() < 1)
        return false;
    cv::drawContours(img, contours, 0, cv::Scalar(0, 255, 0), 1);

    // 胎盘分割最大区域
    mask = 0;
    cv::drawContours(mask, contours, 0, 255, -1);

    // 侧脑室图像与分割crop
    cv::Rect rect = cv::boundingRect(contours[0]);
    cv::Mat img_crop = img(rect);
    cv::Mat mask_crop = mask(rect);
    cv::Mat gray;
    cv::cvtColor(img_crop, gray, cv::COLOR_BGR2GRAY);

    // OTSU二值化+开运算
    cv::Mat thresh;
    cv::threshold(gray, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    thresh.setTo(0, mask_crop==0);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::Mat close;
    cv::morphologyEx(thresh, close, cv::MORPH_CLOSE, kernel); // 闭运算去除细小区域
    close.copyTo(mask(rect));

    //  图像距离变换
    cv::Mat dist_trans;
    cv::distanceTransform(mask, dist_trans, cv::DIST_L2, cv::DIST_MASK_3, cv::DIST_LABEL_PIXEL);// 距离变换
    double min_val, max_val;
    cv::Point min_loc, max_loc;
    cv::minMaxLoc(dist_trans, &min_val, &max_val, &min_loc, &max_loc);
    cv::circle(img, max_loc, 2, cv::Scalar(255, 0, 0), 2);

    float min_dist = sqrt(pow(rows, 2) + pow(cols, 2));
    cv::Point min_point1, min_point2;
    int cnt_len = contours[0].size();
    std::vector<cv::Point> points;
    cv::Point point1, point2;

    for (float angle=0; angle <=360; ) {

        linePS2TP(max_loc, angle, point1, point2, rows, cols);
        // 指针直线方程一般式
        std::vector<float> ABC;
        GeneralEquation(point1, point2, ABC);

        // 直线与轮廓交点
        points.clear();
        for (int i = 0; i < cnt_len; i++) {
            point1 = contours[0][i];
            point2 = contours[0][(i + 1) % cnt_len];
            if (((ABC[0] * point1.x + ABC[1] * point1.y + ABC[2]) * (ABC[0] * point2.x + ABC[1] * point2.y + ABC[2])) <= 0)
            {
                points.push_back(point1);
            }
        }

        // 轮廓交点到max_loc的距离排序
        std::sort(points.begin(), points.end(), [&](cv::Point pt1, cv::Point pt2) 
        { return Point2PointDist(pt1, max_loc) < Point2PointDist(pt2, max_loc); });

        // 选择两个异向距离最近的交点
        point1 = points[0]; 
        for (auto point : points) {
            if ((point1-max_loc).dot(point-max_loc) < 0) {
                point2 = point;
                break;
            }
        }

        double dist;
        Point2PointDist(point1, point2, dist);

        if (dist < min_dist) {
            min_point1 = point1;
            min_point2 = point2;
            min_dist = dist;
        }

        angle = angle + 1;
    }
    thickness.first = min_point1;
    thickness.second = min_point2;
    LV = min_dist;
    std::cout << "侧脑室宽度: " << LV << std::endl;
    cv::line(img, thickness.first, thickness.second, cv::Scalar(0, 0, 255), 1);
    return true;
}

// 旋转图像
void Measure::rotateImg(cv::Mat src, cv::Mat &dst, double angle, cv::Point center) {
    cv::Mat matrix = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::warpAffine(src, dst, matrix, src.size(), cv::INTER_NEAREST);
}

// 旋转点
void Measure::rotatePoint(cv::Point src, cv::Point &dst, cv::Point center, double angle) {
    angle = angle*CV_PI/180;
    dst.x = center.x + (src.x-center.x)*std::cos(angle) + (src.y-center.y)*std::sin(angle);
    dst.y = center.y - (src.x-center.x)*std::sin(angle) + (src.y-center.y)*std::cos(angle);
}
 
// 小脑横径测量
bool Measure::CBMeasure(cv::Mat mask, double &CB, std::pair<cv::Point, cv::Point>& line, cv::Mat &img) {
    // 查找小脑分割最大区域
    std::vector<std::vector<cv::Point>> contours;
    findsortContours(mask, contours);
    if (contours.size() < 1)
        return false;
    cv::drawContours(img, contours, 0, cv::Scalar(0, 255, 0), 1);

    cv::RotatedRect rect;
    rect = cv::minAreaRect(contours[0]);
    int w = rect.size.width;
    int h = rect.size.height;
    int cx = rect.center.x;
    int cy = rect.center.y;
    double angle = rect.angle;
    
    // opencv版本大于4.5.1
    // if (h > w) angle += -90;
    // opencv版本小于4.5.1
    if (w > h) angle = 90+angle;

    mask = 0;
    cv::drawContours(mask, contours, 0, 255, -1);
    rotateImg(mask, mask, angle, rect.center);

    int tmp_w = std::min(w, h);
    int tmp_h = std::max(w, h);
    w = tmp_w;
    h = tmp_h;
    int x = int(cx - w/2);
    int y = int(cy - h/2);
    cv::circle(img, cv::Point(x, y), 2, cv::Scalar(255, 0, 0), 2);

    int max_dist = 0;
    int max_i = 0;
    int max_j1 = 0;
    int max_j2 = 0;

    for (int i=0; i < w; i++) {
        int j1 = y;
        int j2 = y;

        for (int j=0; j < h; j++) {
            uint value1 = mask.at<uchar>(j+y, i+x);
            if (value1 > 0) {
                j1 = j+y;
                break;
            }
        }

        for (int j=0; j < h; j++) {
            uint value2 = mask.at<uchar>(y+h-j, i+x);
            if (value2 > 0) {
                j2 = y+h-j;
                break;
            }
        }

        int dist = j2-j1;
        if (dist > max_dist) {
            max_dist = dist;
            max_i = i+x;
            max_j1 = j1;
            max_j2 = j2;
        }

    }
    CB = max_dist;
    cv::Point pnt1(max_i, max_j1);
    cv::Point pnt2(max_i, max_j2);
    rotatePoint(pnt1, pnt1, rect.center, -angle);
    rotatePoint(pnt2, pnt2, rect.center, -angle);
    line.first = pnt1;
    line.second = pnt2;
    cv::line(img, pnt1, pnt2, cv::Scalar(0, 0, 255), 1);
    return true;
}

void Measure::apply(std::map<std::string, cv::Mat> &mask_dict, MResult &result, cv::Mat &img, std::string name) {
    if ( name.compare("ACMeasure")==0 ) {
        cv::Mat mask = mask_dict["腹围"];
        double AC;
        cv::RotatedRect ac_ellipse;
        bool flag = ACMeasure(mask, AC, ac_ellipse, img);
        if (flag) {
            result.flag = true;
            result.biological_indicators.push_back(std::make_pair("AC", AC));
            result.ellipses.push_back(std::make_pair("AC", ac_ellipse));
        } else {
            result.flag = false;
        }
    }

    if ( name.compare("HCBPDMeasure")==0 ) {
        cv::Mat inner_mask = mask_dict["颅骨内缘"]+mask_dict["小脑"]+mask_dict["侧脑室"];
        cv::Mat outer_mask = mask_dict["颅骨外缘"];
        outer_mask = inner_mask+outer_mask;
        double HC;
        double BPD;
        std::pair<cv::Point, cv::Point> bpd_line;
        cv::RotatedRect ellipse;
        bool flag = HCBPDMeasure(inner_mask, outer_mask, HC, BPD, bpd_line, ellipse, img);
        if (flag) {
            result.flag = true;
            result.biological_indicators.push_back(std::make_pair("HC", HC));
            result.biological_indicators.push_back(std::make_pair("BPD", BPD));
            result.ellipses.push_back(std::make_pair("HC", ellipse));
            result.lines.push_back(std::make_pair("BPD", bpd_line));
        } else {
            result.flag = false;
        }
    }

    if ( name.compare("FLMeasure")==0 ) {
        cv::Mat mask = mask_dict["股骨"];
        double FL;
        std::pair<cv::Point, cv::Point> fl_line;
        bool flag = FLMeasure(mask, FL, fl_line, img);
        if (flag) {
            result.flag = true;
            result.biological_indicators.push_back(std::make_pair("FL", FL));
            result.lines.push_back(std::make_pair("FL", fl_line));
        } else {
            result.flag = false;
        }
    }

    if ( name.compare("AFVMeasure")==0 ) {
        cv::Mat mask = mask_dict["羊水"];
        double AFV;
        std::pair<cv::Point, cv::Point> vertical;
        bool flag = AFVMeasure(mask, AFV, vertical, img);
        if (flag) {
            result.flag = true;
            result.biological_indicators.push_back(std::make_pair("AFV", AFV));
            result.lines.push_back(std::make_pair("AFV", vertical));
        } else {
            result.flag = false;
        } 
    }

    if ( name.compare("PTMeasure")==0 ) {
        cv::Mat mask = mask_dict["胎盘"];
        double PT;
        std::pair<cv::Point, cv::Point> thickness;
        bool flag = PTMeasure(mask, PT, thickness, img);
        if (flag) {
            result.flag = true;
            result.biological_indicators.push_back(std::make_pair("PT", PT));
            result.lines.push_back(std::make_pair("PT", thickness));
        } else {
            result.flag = false;
        }
    }

    if ( name.compare("LVMeasure")==0 ) {
        cv::Mat mask = mask_dict["侧脑室"];
        double LV;
        std::pair<cv::Point, cv::Point> thickness;
        bool flag = LVMeasure(mask, LV, thickness, img);
        if (flag) {
            result.flag = true;
            result.biological_indicators.push_back(std::make_pair("LV", LV));
            result.lines.push_back(std::make_pair("LV", thickness));
        } else {
            result.flag = false;
        }
    }

    if ( name.compare("CBMeasure")==0 ) {
        cv::Mat mask = mask_dict["小脑"];
        double CB;
        std::pair<cv::Point, cv::Point> line;
        bool flag = CBMeasure(mask, CB, line, img);
        if (flag) {
            result.flag = true;
            result.biological_indicators.push_back(std::make_pair("CB", CB));
            result.lines.push_back(std::make_pair("CB", line));
        } else {
            result.flag = false;
        }
    }
    
}

}