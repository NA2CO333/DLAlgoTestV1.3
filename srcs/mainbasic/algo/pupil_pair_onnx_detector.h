#ifndef PUPIL_PAIR_ONNX_DETECTOR_H
#define PUPIL_PAIR_ONNX_DETECTOR_H

#include <array>
#include <mutex>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

struct PupilPairEyeResult
{
    bool detected = false;
    cv::Point2f center = cv::Point2f(-1.0F, -1.0F);
    float equivalentRadius = 0.0F;
};

struct PupilPairFrameResult
{
    PupilPairEyeResult subjectRight;
    PupilPairEyeResult subjectLeft;
};

struct PupilPairResult
{
    std::array<PupilPairFrameResult, 2> frames;
    double preprocessMs = 0.0;
    double forwardMs = 0.0;
    double postprocessMs = 0.0;
    double totalMs = 0.0;
};

class PupilPairOnnxDetector
{
public:
    enum Layout
    {
        ChannelStack,
        SpatialConcat
    };

    PupilPairOnnxDetector(Layout layout, int tileWidth, int tileHeight);

    bool load(const std::string &modelPath, std::string *errorMessage = nullptr);
    // 正式算法传入缓存的400×160灰度图时，输出坐标仍保持在该输入图坐标系；
    // 旧调用传入原图时，makeCanvas负责兼容性缩放和坐标回映射。
    bool infer(const cv::Mat &firstImage,
               const cv::Mat &secondImage,
               PupilPairResult *result,
               float logitThreshold,
               int minimumComponentArea,
               std::string *errorMessage = nullptr);

private:
    struct LetterboxInfo
    {
        double scale = 1.0;
        int resizedWidth = 0;
        int resizedHeight = 0;
        int padLeft = 0;
        int padTop = 0;
        cv::Size originalSize;
    };

    cv::Mat makeCanvas(const cv::Mat &image, LetterboxInfo *info) const;
    cv::Mat makePairBlob(const cv::Mat &first,
                         const cv::Mat &second,
                         std::array<LetterboxInfo, 2> *infos) const;
    static PupilPairEyeResult extractEye(const cv::Mat &logits,
                                         const LetterboxInfo &info,
                                         float threshold,
                                         int minimumArea);
    static double elapsedMs(int64 startTick, int64 endTick);
    static void setError(std::string *errorMessage, const std::string &message);

    Layout m_layout;
    int m_tileWidth;
    int m_tileHeight;
    std::mutex m_netMutex;
    cv::dnn::Net m_net;
};

#endif // PUPIL_PAIR_ONNX_DETECTOR_H
