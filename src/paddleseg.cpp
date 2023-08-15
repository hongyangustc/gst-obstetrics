#include "paddleseg.h"

namespace obstetrics {

// 初始化
PaddleSeg::PaddleSeg(const std::string &engine_file, int width, int height, int class_num, char const * input_name, char const * output_name)
{
    // 设置网络输入输出大小、类别、输入输出名称
    this->INPUT_W = width;
    this->INPUT_H = height;
    this->CLASS_NUM = class_num;
    this->INPUT_BLOB_NAME = input_name;
    this->OUTPUT_BLOB_NAME = output_name;

    // 设置设备
    cudaSetDevice(DEVICE);
    // 读取模型文件
    char *trtModelStream{nullptr};
    size_t size{0};
    std::ifstream file(engine_file, std::ios::binary);
    if (file.good())
    {
        file.seekg(0, file.end);
        size = file.tellg();
        file.seekg(0, file.beg);
        trtModelStream = new char[size];
        assert(trtModelStream);
        file.read(trtModelStream, size);
        file.close();
    }

    // 初始化模型
    runtime = createInferRuntime(gLogger);
    assert(runtime != nullptr);
    engine = runtime->deserializeCudaEngine(trtModelStream, size);
    assert(engine != nullptr);
    context = engine->createExecutionContext();
    assert(context != nullptr);
    delete[] trtModelStream;
}

PaddleSeg::~PaddleSeg()
{
    // Destroy the engine
    context->destroy();
    engine->destroy();
    runtime->destroy();
}

// 图像前处理
cv::Mat PaddleSeg::preprocess(cv::Mat &img)
{   
    this->width = img.cols;
    this->height = img.rows;
    cv::Mat img_resize;
    cv::resize(img, img_resize, cv::Size(INPUT_W, INPUT_H));
    return img_resize;
}

void PaddleSeg::doInference(IExecutionContext &context, float *input, int *output)
{
    const ICudaEngine &engine = context.getEngine();

    // Pointers to input and output device buffers to pass to engine.
    // Engine requires exactly IEngine::getNbBindings() number of buffers.
    assert(engine.getNbBindings() == 2);
    void *buffers[2];

    // In order to bind the buffers, we need to know the names of the input and output tensors.
    // Note that indices are guaranteed to be less than IEngine::getNbBindings()
    const int inputIndex = engine.getBindingIndex(INPUT_BLOB_NAME);
    const int outputIndex = engine.getBindingIndex(OUTPUT_BLOB_NAME);

    // Create GPU buffers on device
    CHECK(cudaMalloc(&buffers[inputIndex], BATCH_SIZE * 3 * INPUT_H * INPUT_W * sizeof(float)));
    CHECK(cudaMalloc(&buffers[outputIndex], BATCH_SIZE * INPUT_H * INPUT_W * sizeof(int)));

    // Create stream
    cudaStream_t stream;
    CHECK(cudaStreamCreate(&stream));

    // DMA input batch data to device, infer on the batch asynchronously, and DMA output back to host
    CHECK(cudaMemcpyAsync(buffers[inputIndex], input, BATCH_SIZE * 3 * INPUT_H * INPUT_W * sizeof(float), cudaMemcpyHostToDevice, stream));

    context.enqueue(BATCH_SIZE, buffers, stream, nullptr);

    CHECK(cudaMemcpyAsync(output, buffers[outputIndex], BATCH_SIZE * INPUT_H * INPUT_W * sizeof(int), cudaMemcpyDeviceToHost, stream));

    //流同步：通过cudaStreamSynchronize()来协调。
    cudaStreamSynchronize(stream);

    // Release stream and buffers
    cudaStreamDestroy(stream);
    CHECK(cudaFree(buffers[inputIndex]));
    CHECK(cudaFree(buffers[outputIndex]));
}

std::vector<cv::Mat> PaddleSeg::postprocess(cv::Mat &img)
{
    float input[BATCH_SIZE * 3 * INPUT_H * INPUT_W];
    int output[BATCH_SIZE * INPUT_H * INPUT_W];

    int i = 0;
    for (int row = 0; row < INPUT_H; ++row) 
    {
        uchar* uc_pixel = img.data + row * img.step;
        for (int col = 0; col < INPUT_W; ++col) 
        {
            input[i] = (float)uc_pixel[2] / 127.5 - 1.0;
            input[i + INPUT_H * INPUT_W] = (float)uc_pixel[1] / 127.5 - 1.0;
            input[i + 2 * INPUT_H * INPUT_W] = (float)uc_pixel[0] / 127.5 - 1.0;
            uc_pixel += 3;
            ++i;
        }
    }

    auto start = std::chrono::system_clock::now();
    doInference(*context, input, output);
    auto end = std::chrono::system_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

    std::vector<cv::Mat> mask;
    for (int cls = 0; cls < CLASS_NUM; cls++)
    {
        cv::Mat mask_mat = cv::Mat::zeros(INPUT_H, INPUT_W, CV_8UC1);
        uchar *ptmp = NULL;
        for (int i = 0; i < INPUT_H; i++)
        {
            ptmp = mask_mat.ptr<uchar>(i);
            for (int j = 0; j < INPUT_W; j++)
            {
                if (output[i * INPUT_W + j] == cls + 1)
                {
                    ptmp[j] = 255;
                }
            }
        }

        cv::Mat mask_mat_resize;
        cv::resize(mask_mat, mask_mat_resize, cv::Size(this->width, this->height));
        mask.push_back(mask_mat_resize);
    }

    return mask;
}

std::vector<cv::Mat> PaddleSeg::seginfer(cv::Mat &img) {
    cv::Mat img_preprocess;
    img_preprocess = preprocess(img);
    std::vector<cv::Mat> mask;
    mask = postprocess(img_preprocess);

    return mask;
}

}