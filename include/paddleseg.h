#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

// opencv
#include <opencv2/opencv.hpp>

// cuda、tensorrt
#include <chrono>
#include "cuda_runtime_api.h"
#include "logging.h"
#include "NvInfer.h"

// 定义
#define CHECK(status)                                          \
    do                                                         \
    {                                                          \
        auto ret = (status);                                   \
        if (ret != 0)                                          \
        {                                                      \
            std::cerr << "Cuda failure: " << ret << std::endl; \
            abort();                                           \
        }                                                      \
    } while (0)

using namespace nvinfer1;

static Logger gLogger;

namespace obstetrics {

class PaddleSeg
{

public:
    PaddleSeg(const std::string &engine_file, int width, int height, int class_num, char const * input_name, char const * output_name);
    ~PaddleSeg();
    cv::Mat preprocess(cv::Mat &img);
    void doInference(IExecutionContext &context, float *input, int *output);
    std::vector<cv::Mat> postprocess(cv::Mat &img);
    std::vector<cv::Mat> seginfer(cv::Mat &img);

    // 输入图像原始宽高
    int width;
    int height;

private:
    // 网络输入大小
    int CLASS_NUM;
    int INPUT_H;
    int INPUT_W;

    // 模型输入输出名字
    const char *INPUT_BLOB_NAME;
    const char *OUTPUT_BLOB_NAME;

    // tensorrt
    static const int BATCH_SIZE = 1; // batch size
    static const int DEVICE = 0;     // 设备
    IRuntime *runtime = nullptr;
    ICudaEngine *engine = nullptr;
    IExecutionContext *context = nullptr;
};

}