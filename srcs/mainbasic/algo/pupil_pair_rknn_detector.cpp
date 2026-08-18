#include "pupil_pair_rknn_detector.h"

#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

PupilPairRknnDetector::PupilPairRknnDetector(int tileWidth, int tileHeight)
    : m_tileWidth(tileWidth),
      m_tileHeight(tileHeight),
      m_tensorCodec(PupilPairOnnxDetector::SpatialConcat,
                    tileWidth,
                    tileHeight)
{
}

PupilPairRknnDetector::~PupilPairRknnDetector()
{
    std::lock_guard<std::mutex> lock(m_contextMutex);
    unloadLocked();
}

bool PupilPairRknnDetector::load(const std::string &modelPath,
                                 std::string *errorMessage)
{
    std::ifstream stream(modelPath.c_str(),
                         std::ios::in | std::ios::binary | std::ios::ate);
    if (!stream)
    {
        setError(errorMessage, "Failed to open RKNN model: " + modelPath);
        return false;
    }
    const std::streamoff modelSize = stream.tellg();
    if (modelSize <= 0
        || static_cast<unsigned long long>(modelSize)
           > static_cast<unsigned long long>(
               std::numeric_limits<uint32_t>::max()))
    {
        setError(errorMessage, "RKNN model size is invalid.");
        return false;
    }
    stream.seekg(0, std::ios::beg);
    std::vector<unsigned char> modelData(static_cast<size_t>(modelSize));
    if (!stream.read(reinterpret_cast<char *>(modelData.data()),
                     static_cast<std::streamsize>(modelSize)))
    {
        setError(errorMessage, "Failed to read RKNN model.");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_contextMutex);
    unloadLocked();
    int ret = rknn_init(&m_context,
                        modelData.data(),
                        static_cast<uint32_t>(modelData.size()),
                        0,
                        nullptr);
    if (ret != RKNN_SUCC)
    {
        m_context = 0;
        std::ostringstream message;
        message << "rknn_init failed: " << ret;
        setError(errorMessage, message.str());
        return false;
    }

    rknn_input_output_num ioNumber;
    std::memset(&ioNumber, 0, sizeof(ioNumber));
    ret = rknn_query(m_context,
                     RKNN_QUERY_IN_OUT_NUM,
                     &ioNumber,
                     sizeof(ioNumber));
    if (ret != RKNN_SUCC || ioNumber.n_input != 1 || ioNumber.n_output < 1)
    {
        std::ostringstream message;
        message << "Invalid RKNN IO count: ret=" << ret
                << ",input=" << ioNumber.n_input
                << ",output=" << ioNumber.n_output;
        setError(errorMessage, message.str());
        unloadLocked();
        return false;
    }

    rknn_tensor_attr inputAttr;
    std::memset(&inputAttr, 0, sizeof(inputAttr));
    inputAttr.index = 0;
    ret = rknn_query(m_context,
                     RKNN_QUERY_INPUT_ATTR,
                     &inputAttr,
                     sizeof(inputAttr));
    const bool inputValid = ret == RKNN_SUCC
        && inputAttr.n_dims == 4
        && inputAttr.fmt == RKNN_TENSOR_NHWC
        && inputAttr.dims[0] == 1
        && inputAttr.dims[1] == static_cast<uint32_t>(m_tileHeight)
        && inputAttr.dims[2] == static_cast<uint32_t>(m_tileWidth * 2)
        && inputAttr.dims[3] == 1;
    if (!inputValid)
    {
        setError(errorMessage, "Unexpected RKNN C800 input tensor.");
        unloadLocked();
        return false;
    }

    rknn_tensor_attr outputAttr;
    std::memset(&outputAttr, 0, sizeof(outputAttr));
    outputAttr.index = 0;
    ret = rknn_query(m_context,
                     RKNN_QUERY_OUTPUT_ATTR,
                     &outputAttr,
                     sizeof(outputAttr));
    const bool outputValid = ret == RKNN_SUCC
        && outputAttr.n_dims == 4
        && outputAttr.fmt == RKNN_TENSOR_NCHW
        && outputAttr.dims[0] == 1
        && outputAttr.dims[1] == 4
        && outputAttr.dims[2] == static_cast<uint32_t>(m_tileHeight)
        && outputAttr.dims[3] == static_cast<uint32_t>(m_tileWidth * 2);
    if (!outputValid)
    {
        setError(errorMessage, "Unexpected RKNN C800 output tensor.");
        unloadLocked();
        return false;
    }

    rknn_sdk_version version;
    std::memset(&version, 0, sizeof(version));
    if (rknn_query(m_context,
                   RKNN_QUERY_SDK_VERSION,
                   &version,
                   sizeof(version)) == RKNN_SUCC)
    {
        m_apiVersion = version.api_version;
        m_driverVersion = version.drv_version;
    }
    m_loaded = true;
    return true;
}

bool PupilPairRknnDetector::infer(const cv::Mat &firstImage,
                                  const cv::Mat &secondImage,
                                  PupilPairResult *result,
                                  float logitThreshold,
                                  int minimumComponentArea,
                                  std::string *errorMessage)
{
    if (result == nullptr || firstImage.empty() || secondImage.empty())
    {
        setError(errorMessage, "RKNN pair input or result is invalid.");
        return false;
    }
    *result = PupilPairResult();
    const int64 totalStart = cv::getTickCount();

    cv::Mat blob;
    std::array<PupilPairOnnxDetector::LetterboxInfo, 2> infos;
    try
    {
        const int64 preprocessStart = cv::getTickCount();
        blob = m_tensorCodec.preparePairBlob(firstImage, secondImage, &infos);
        if (!blob.isContinuous())
        {
            blob = blob.clone();
        }
        result->preprocessMs = elapsedMs(preprocessStart, cv::getTickCount());
    }
    catch (const cv::Exception &exception)
    {
        setError(errorMessage,
                 std::string("RKNN preprocessing failed: ")
                     + exception.what());
        return false;
    }

    std::lock_guard<std::mutex> lock(m_contextMutex);
    if (!m_loaded || m_context == 0)
    {
        setError(errorMessage, "RKNN context is not loaded.");
        return false;
    }

    rknn_input input;
    std::memset(&input, 0, sizeof(input));
    input.index = 0;
    input.buf = blob.ptr<float>();
    input.size = static_cast<uint32_t>(blob.total() * blob.elemSize());
    input.pass_through = 0;
    input.type = RKNN_TENSOR_FLOAT32;
    // C800是单通道输入，NCHW与NHWC的像素内存顺序相同；这里必须
    // 按RKNN模型声明使用NHWC，避免运行库normalize阶段拒绝输入。
    input.fmt = RKNN_TENSOR_NHWC;

    const int64 forwardStart = cv::getTickCount();
    int ret = rknn_inputs_set(m_context, 1, &input);
    if (ret == RKNN_SUCC)
    {
        ret = rknn_run(m_context, nullptr);
    }

    rknn_output output;
    std::memset(&output, 0, sizeof(output));
    output.index = 0;
    output.want_float = 1;
    output.is_prealloc = 0;
    if (ret == RKNN_SUCC)
    {
        ret = rknn_outputs_get(m_context, 1, &output, nullptr);
    }
    result->forwardMs = elapsedMs(forwardStart, cv::getTickCount());
    if (ret != RKNN_SUCC || output.buf == nullptr)
    {
        if (output.buf != nullptr)
        {
            rknn_outputs_release(m_context, 1, &output);
        }
        std::ostringstream message;
        message << "RKNN inference failed: " << ret;
        setError(errorMessage, message.str());
        return false;
    }

    const size_t expectedOutputElements = static_cast<size_t>(4)
        * static_cast<size_t>(m_tileHeight)
        * static_cast<size_t>(m_tileWidth * 2);
    const bool outputSizeValid = output.size
        == expectedOutputElements * sizeof(float);
    bool decodeSucceeded = false;
    if (outputSizeValid)
    {
        try
        {
            const int64 postprocessStart = cv::getTickCount();
            decodeSucceeded = m_tensorCodec.decodePairOutput(
                static_cast<const float *>(output.buf),
                4,
                m_tileHeight,
                m_tileWidth * 2,
                infos,
                result,
                logitThreshold,
                minimumComponentArea,
                errorMessage);
            result->postprocessMs = elapsedMs(postprocessStart,
                                              cv::getTickCount());
        }
        catch (const cv::Exception &exception)
        {
            setError(errorMessage,
                     std::string("RKNN postprocessing failed: ")
                         + exception.what());
        }
        catch (const std::exception &exception)
        {
            setError(errorMessage,
                     std::string("RKNN postprocessing failed: ")
                         + exception.what());
        }
    }
    else
    {
        setError(errorMessage, "RKNN output size is invalid.");
    }
    rknn_outputs_release(m_context, 1, &output);
    result->totalMs = elapsedMs(totalStart, cv::getTickCount());
    return decodeSucceeded;
}

bool PupilPairRknnDetector::isLoaded() const
{
    std::lock_guard<std::mutex> lock(m_contextMutex);
    return m_loaded && m_context != 0;
}

std::string PupilPairRknnDetector::apiVersion() const
{
    std::lock_guard<std::mutex> lock(m_contextMutex);
    return m_apiVersion;
}

std::string PupilPairRknnDetector::driverVersion() const
{
    std::lock_guard<std::mutex> lock(m_contextMutex);
    return m_driverVersion;
}

void PupilPairRknnDetector::unloadLocked()
{
    if (m_context != 0)
    {
        rknn_destroy(m_context);
    }
    m_context = 0;
    m_loaded = false;
    m_apiVersion.clear();
    m_driverVersion.clear();
}

void PupilPairRknnDetector::setError(std::string *errorMessage,
                                     const std::string &message)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

double PupilPairRknnDetector::elapsedMs(int64 startTick, int64 endTick)
{
    return (endTick - startTick) * 1000.0 / cv::getTickFrequency();
}
