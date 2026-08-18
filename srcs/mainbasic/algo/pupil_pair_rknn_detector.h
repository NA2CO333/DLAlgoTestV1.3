#ifndef PUPIL_PAIR_RKNN_DETECTOR_H
#define PUPIL_PAIR_RKNN_DETECTOR_H

#include "pupil_pair_onnx_detector.h"
#include "rknn_api.h"

#include <mutex>
#include <string>

// C800的RKNN/NPU推理后端。预处理和后处理复用ONNX检测器的公共张量
// 编解码接口，因此上层收到的PupilPairResult与CPU路径保持一致。
class PupilPairRknnDetector
{
public:
    PupilPairRknnDetector(int tileWidth, int tileHeight);
    ~PupilPairRknnDetector();

    bool load(const std::string &modelPath,
              std::string *errorMessage = nullptr);
    bool infer(const cv::Mat &firstImage,
               const cv::Mat &secondImage,
               PupilPairResult *result,
               float logitThreshold,
               int minimumComponentArea,
               std::string *errorMessage = nullptr);

    bool isLoaded() const;
    std::string apiVersion() const;
    std::string driverVersion() const;

private:
    void unloadLocked();
    static void setError(std::string *errorMessage,
                         const std::string &message);
    static double elapsedMs(int64 startTick, int64 endTick);

    int m_tileWidth;
    int m_tileHeight;
    PupilPairOnnxDetector m_tensorCodec;
    mutable std::mutex m_contextMutex;
    rknn_context m_context = 0;
    bool m_loaded = false;
    std::string m_apiVersion;
    std::string m_driverVersion;
};

#endif // PUPIL_PAIR_RKNN_DETECTOR_H
