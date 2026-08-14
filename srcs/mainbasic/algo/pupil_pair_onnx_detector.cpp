#include "pupil_pair_onnx_detector.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <sstream>

#include <opencv2/imgproc.hpp>

namespace
{
const double kPi = 3.14159265358979323846;
}

PupilPairOnnxDetector::PupilPairOnnxDetector(Layout layout,
                                             int tileWidth,
                                             int tileHeight)
    : m_layout(layout),
      m_tileWidth(std::max(1, tileWidth)),
      m_tileHeight(std::max(1, tileHeight))
{
}

bool PupilPairOnnxDetector::load(const std::string &modelPath,
                                 std::string *errorMessage)
{
    try
    {
        cv::dnn::Net net = cv::dnn::readNetFromONNX(modelPath);
        if (net.empty())
        {
            setError(errorMessage, "OpenCV returned an empty pair network.");
            return false;
        }
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::lock_guard<std::mutex> lock(m_netMutex);
        m_net = net;
        return true;
    }
    catch (const cv::Exception &exception)
    {
        setError(errorMessage,
                 std::string("OpenCV failed to load pair ONNX: ") +
                     exception.what());
    }
    catch (const std::exception &exception)
    {
        setError(errorMessage,
                 std::string("Failed to load pair ONNX: ") + exception.what());
    }
    return false;
}

bool PupilPairOnnxDetector::infer(const cv::Mat &firstImage,
                                  const cv::Mat &secondImage,
                                  PupilPairResult *result,
                                  float logitThreshold,
                                  int minimumComponentArea,
                                  std::string *errorMessage)
{
    if (result == nullptr || firstImage.empty() || secondImage.empty())
    {
        setError(errorMessage, "Pair input or result is invalid.");
        return false;
    }
    *result = PupilPairResult();
    const int64 totalStart = cv::getTickCount();

    cv::Mat blob;
    std::array<LetterboxInfo, 2> infos;
    try
    {
        const int64 start = cv::getTickCount();
        blob = makePairBlob(firstImage, secondImage, &infos);
        result->preprocessMs = elapsedMs(start, cv::getTickCount());
    }
    catch (const cv::Exception &exception)
    {
        setError(errorMessage,
                 std::string("Pair preprocessing failed: ") + exception.what());
        return false;
    }

    cv::Mat output;
    try
    {
        const int64 start = cv::getTickCount();
        {
            std::lock_guard<std::mutex> lock(m_netMutex);
            if (m_net.empty())
            {
                setError(errorMessage, "Pair network is not loaded.");
                return false;
            }
            m_net.setInput(blob);
            output = m_net.forward();
        }
        result->forwardMs = elapsedMs(start, cv::getTickCount());
    }
    catch (const cv::Exception &exception)
    {
        setError(errorMessage,
                 std::string("Pair forward failed: ") + exception.what());
        return false;
    }

    const int expectedChannels = m_layout == ChannelStack ? 8 : 4;
    const int expectedWidth =
        m_layout == ChannelStack ? m_tileWidth : m_tileWidth * 2;
    if (output.dims != 4 || output.type() != CV_32F ||
        output.size[0] != 1 || output.size[1] != expectedChannels ||
        output.size[2] != m_tileHeight || output.size[3] != expectedWidth)
    {
        std::ostringstream stream;
        stream << "Unexpected pair output tensor, expected FP32 [1,"
               << expectedChannels << "," << m_tileHeight << ","
               << expectedWidth << "].";
        setError(errorMessage, stream.str());
        return false;
    }

    try
    {
        const int64 start = cv::getTickCount();
        for (int frame = 0; frame < 2; ++frame)
        {
            const int channelOffset = m_layout == ChannelStack ? frame * 4 : 0;
            for (int eye = 0; eye < 2; ++eye)
            {
                const int fullChannel = channelOffset + 2 + eye;
                cv::Mat fullLogits(m_tileHeight,
                                   expectedWidth,
                                   CV_32F,
                                   output.ptr<float>(0, fullChannel));
                cv::Mat tileLogits =
                    m_layout == SpatialConcat
                        ? fullLogits(cv::Rect(frame * m_tileWidth,
                                             0,
                                             m_tileWidth,
                                             m_tileHeight))
                        : fullLogits;
                PupilPairEyeResult eyeResult =
                    extractEye(tileLogits,
                               infos[frame],
                               logitThreshold,
                               minimumComponentArea);
                if (eye == 0)
                {
                    result->frames[frame].subjectRight = eyeResult;
                }
                else
                {
                    result->frames[frame].subjectLeft = eyeResult;
                }
            }
        }
        result->postprocessMs = elapsedMs(start, cv::getTickCount());
    }
    catch (const cv::Exception &exception)
    {
        setError(errorMessage,
                 std::string("Pair postprocessing failed: ") + exception.what());
        return false;
    }

    result->totalMs = elapsedMs(totalStart, cv::getTickCount());
    return true;
}

cv::Mat PupilPairOnnxDetector::makeCanvas(const cv::Mat &image,
                                          LetterboxInfo *info) const
{
    // 400×160小图会直接进入模型画布，不再发生第二次缩放；原图输入
    // 仍沿用既有letterbox，并由extractEye映射回调用者输入坐标。
    cv::Mat gray;
    if (image.channels() == 1)
    {
        gray = image;
    }
    else if (image.channels() == 3)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else if (image.channels() == 4)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    }
    else
    {
        CV_Error(cv::Error::StsBadArg, "Unsupported pair image channels.");
    }

    info->originalSize = gray.size();
    info->scale = std::min(static_cast<double>(m_tileWidth) / gray.cols,
                           static_cast<double>(m_tileHeight) / gray.rows);
    info->resizedWidth =
        std::max(1, static_cast<int>(std::round(gray.cols * info->scale)));
    info->resizedHeight =
        std::max(1, static_cast<int>(std::round(gray.rows * info->scale)));
    info->resizedWidth = std::min(m_tileWidth, info->resizedWidth);
    info->resizedHeight = std::min(m_tileHeight, info->resizedHeight);
    info->padLeft = (m_tileWidth - info->resizedWidth) / 2;
    info->padTop = (m_tileHeight - info->resizedHeight) / 2;

    cv::Mat normalized;
    if (gray.depth() == CV_8U)
    {
        gray.convertTo(normalized, CV_32F, 1.0 / 255.0);
    }
    else if (gray.depth() == CV_16U)
    {
        gray.convertTo(normalized, CV_32F, 1.0 / 65535.0);
    }
    else
    {
        gray.convertTo(normalized, CV_32F);
    }

    const cv::Size targetSize(info->resizedWidth, info->resizedHeight);
    cv::Mat resized;
    // 正式路径传入的400×160小图已经是模型目标尺寸，避免无意义的
    // 400×160到400×160二次插值；原图输入仍保持原有letterbox缩放。
    if (normalized.size() == targetSize) {
        resized = normalized;
    } else {
        cv::resize(normalized,
                   resized,
                   targetSize,
                   0.0,
                   0.0,
                   cv::INTER_LINEAR);
    }
    cv::Mat canvas = cv::Mat::zeros(m_tileHeight, m_tileWidth, CV_32FC1);
    resized.copyTo(canvas(cv::Rect(info->padLeft,
                                  info->padTop,
                                  info->resizedWidth,
                                  info->resizedHeight)));
    return canvas;
}

cv::Mat PupilPairOnnxDetector::makePairBlob(
    const cv::Mat &first,
    const cv::Mat &second,
    std::array<LetterboxInfo, 2> *infos) const
{
    const cv::Mat firstCanvas = makeCanvas(first, &(*infos)[0]);
    const cv::Mat secondCanvas = makeCanvas(second, &(*infos)[1]);
    if (m_layout == SpatialConcat)
    {
        cv::Mat joined;
        cv::hconcat(firstCanvas, secondCanvas, joined);
        return cv::dnn::blobFromImage(joined,
                                      1.0,
                                      joined.size(),
                                      cv::Scalar(),
                                      false,
                                      false,
                                      CV_32F);
    }

    const int sizes[4] = {1, 2, m_tileHeight, m_tileWidth};
    cv::Mat blob(4, sizes, CV_32F, cv::Scalar(0.0F));
    cv::Mat firstChannel(m_tileHeight,
                         m_tileWidth,
                         CV_32F,
                         blob.ptr<float>(0, 0));
    cv::Mat secondChannel(m_tileHeight,
                          m_tileWidth,
                          CV_32F,
                          blob.ptr<float>(0, 1));
    firstCanvas.copyTo(firstChannel);
    secondCanvas.copyTo(secondChannel);
    return blob;
}

PupilPairEyeResult PupilPairOnnxDetector::extractEye(
    const cv::Mat &logits,
    const LetterboxInfo &info,
    float threshold,
    int minimumArea)
{
    PupilPairEyeResult result;
    cv::Mat binary;
    cv::compare(logits, cv::Scalar(threshold), binary, cv::CMP_GE);
    cv::Mat contentOnly = cv::Mat::zeros(logits.size(), CV_8UC1);
    const cv::Rect content(info.padLeft,
                           info.padTop,
                           info.resizedWidth,
                           info.resizedHeight);
    binary(content).copyTo(contentOnly(content));

    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const int count = cv::connectedComponentsWithStats(
        contentOnly, labels, stats, centroids, 8, CV_32S);
    int best = -1;
    int bestArea = 0;
    for (int label = 1; label < count; ++label)
    {
        const int area = stats.at<int>(label, cv::CC_STAT_AREA);
        if (area > bestArea)
        {
            best = label;
            bestArea = area;
        }
    }
    if (best < 0 || bestArea < minimumArea || info.scale <= 0.0)
    {
        return result;
    }

    result.detected = true;
    result.center.x = static_cast<float>(
        (centroids.at<double>(best, 0) - info.padLeft) / info.scale);
    result.center.y = static_cast<float>(
        (centroids.at<double>(best, 1) - info.padTop) / info.scale);
    result.equivalentRadius =
        static_cast<float>(std::sqrt(bestArea / kPi) / info.scale);
    return result;
}

double PupilPairOnnxDetector::elapsedMs(int64 startTick, int64 endTick)
{
    return (endTick - startTick) * 1000.0 / cv::getTickFrequency();
}

void PupilPairOnnxDetector::setError(std::string *errorMessage,
                                     const std::string &message)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}
