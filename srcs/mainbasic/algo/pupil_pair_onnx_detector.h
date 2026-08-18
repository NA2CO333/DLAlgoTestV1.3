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

    // ONNX与RKNN后端必须共用完全相同的letterbox和后处理规则，避免
    // 因为后端切换造成瞳孔坐标、半径或门禁口径变化。
    struct LetterboxInfo
    {
        double scale = 1.0;
        int resizedWidth = 0;
        int resizedHeight = 0;
        int padLeft = 0;
        int padTop = 0;
        cv::Size originalSize;
    };

    bool load(const std::string &modelPath, std::string *errorMessage = nullptr);
    // 正式算法传入缓存的400×160灰度图时，输出坐标仍保持在该输入图坐标系；
    // 旧调用传入原图时，makeCanvas负责兼容性缩放和坐标回映射。
    bool infer(const cv::Mat &firstImage,
               const cv::Mat &secondImage,
               PupilPairResult *result,
               float logitThreshold,
               int minimumComponentArea,
               std::string *errorMessage = nullptr);

    // 为RKNN后端提供公共输入编码和输出解码。接口只处理张量，不持有
    // RKNN上下文，因此不会改变现有OpenCV DNN的线程和加载语义。
    cv::Mat preparePairBlob(
        const cv::Mat &first,
        const cv::Mat &second,
        std::array<LetterboxInfo, 2> *infos) const;
    bool decodePairOutput(
        const float *outputData,
        int outputChannels,
        int outputHeight,
        int outputWidth,
        const std::array<LetterboxInfo, 2> &infos,
        PupilPairResult *result,
        float logitThreshold,
        int minimumComponentArea,
        std::string *errorMessage = nullptr) const;

private:
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
