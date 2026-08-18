#include "algo.h"

#include <sys/prctl.h>

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QMutex>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QApplication>
#include <memory>
#include <random>
#include <array>
#include <bitset>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <utility>
#include <map>
#include "cascadepool.h"
#include "logger.h"
#include "util-common.h"
#include "ransac.h"
#include "refractionstrategy.h"
#include "perftimer.h"
#include "pupil_pair_onnx_detector.h"
#if defined(ENABLE_RKNN_C800) && ENABLE_RKNN_C800
#include "pupil_pair_rknn_detector.h"
#endif
#include "pupil_light_tracker.h"
#include "pupil_cross_round_tracker.h"
#include <atomic>
#include <QRandomGenerator>
#include <opencv2/imgproc.hpp>

#ifndef ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
// 正常照片的逐张模板/任务日志默认关闭；需要现场诊断时可由编译选项打开。
#define ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG 0
#endif

using namespace cv;
// =================================================================================

extern enOpticalPathType g_opticalPathType;

// 手持普通光路容易在相邻物理轮之间发生整体平移，正式异步流程每轮
// 先重新调用模型建立当前轮锚点，失败时才回退已提交的跨轮源；箱体
// 光路继续按既有规则复用跨轮源。
static bool isFormalHandheldMode()
{
    return opticalPathType_General == g_opticalPathType;
}

// 正式流程的跨轮状态由m_mutex统一保护。手持普通光路每轮先重新调用模型，
// 仅在本轮重锚失败时回退已提交的跨轮源；箱体光路仍按既有规则复用跨轮源。
// 旧整轮A/B路径仍保留但不参与当前逐照片正式入口。
struct FormalCrossRoundSourceCandidate
{
    int roundIndex = -1;
    std::vector<cv::Mat> images;
    std::vector<PupilLightFrame> refinedFrames;
    bool coordinatesComplete = false;

    void clear()
    {
        roundIndex = -1;
        images.clear();
        refinedFrames.clear();
        coordinatesComplete = false;
    }
};

struct FormalCrossRoundState
{
    bool ready = false;
    int sourceRoundIndex = -1;
    std::vector<cv::Mat> sourceImages;
    // 跨轮源图对应的400×160灰度缓存，避免每张目标照片重复缩放。
    std::vector<cv::Mat> sourceSmallImages;
    std::vector<PupilLightFrame> sourceFrames;
    // 轮次结算完成前的候选源，不能参与下一轮跟踪。
    std::map<int, FormalCrossRoundSourceCandidate> pendingCandidates;
    // 稀疏与44次完整匹配分别缓存，避免其中一个路径覆盖另一个的模板。
    PupilCrossRoundCache sparseTemplateCache;
    PupilCrossRoundCache fullTemplateCache;

    void clear()
    {
        ready = false;
        sourceRoundIndex = -1;
        sourceImages.clear();
        sourceSmallImages.clear();
        sourceFrames.clear();
        pendingCandidates.clear();
        sparseTemplateCache.clear();
        fullTemplateCache.clear();
    }
};

// 流式任务状态：第1轮主识别图片到齐后先运行一次C800；后续轮不重复模型，
// 而是在每张照片到达时生成跨轮目标梯度，第22张到齐后直接进行少量匹配结算。
struct FormalStreamingRoundState
{
    bool anchorsReady = false;
    bool modelRuntimeFailed = false;
    std::vector<PupilLightFrame> trackedFrames;
    PupilLightTrackerCache gradientCache;
    // 跨轮目标梯度缓存；正式逐照片路径实际使用400×160整图梯度。
    PupilCrossRoundTargetGradientCache crossRoundGradientCache;
    int crossRoundGradientFrameCount = 0;
    double crossRoundGradientTotalMs = 0.0;
    std::bitset<FRAME_ARRAY_SIZE> attemptedFrames;
    int dynamicRecoveryCallCount = 0;
    int gradientFrameCount = 0;
    int modelCallCount = 0;
    double modelTotalMs = 0.0;
    double modelPreprocessMs = 0.0;
    double modelForwardMs = 0.0;
    double modelPostprocessMs = 0.0;
    double gradientTotalMs = 0.0;
    double gradientCropMs = 0.0;
    double gradientResizeMs = 0.0;
    double gradientBlurMs = 0.0;
    double gradientSobelMs = 0.0;
    double gradientMagnitudeMs = 0.0;
    double gradientNormalizeMs = 0.0;
    std::string failureReason;
};

namespace {

// 统一处理线程池任务的所有退出路径，避免nullptr unique_ptr不触发deleter。
class FormalScopeExit
{
public:
    explicit FormalScopeExit(std::function<void()> function)
        : m_function(std::move(function)), m_active(true)
    {
    }

    ~FormalScopeExit()
    {
        if (m_active && m_function) {
            m_function();
        }
    }

    FormalScopeExit(const FormalScopeExit&) = delete;
    FormalScopeExit& operator=(const FormalScopeExit&) = delete;

private:
    std::function<void()> m_function;
    bool m_active;
};

#if ENABLE_PREVIEW_DIAG_LOG
bool shouldLogPreviewDiag(bool finalSucc)
{
    static std::atomic<int> previewDiagCounter(0);
    static std::atomic<int> lastPreviewSucc(-1);
    const int count = previewDiagCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    const int currentSucc = finalSucc ? 1 : 0;
    const int prevSucc = lastPreviewSucc.exchange(currentSucc, std::memory_order_relaxed);
    // 预览诊断日志用于定位卡点：前几帧完整输出，后续限流，避免串口刷屏导致预览卡顿。
    return count <= 20 || prevSucc != currentSucc || (count % 15 == 0);
}

QString pupilBrief(PupilInfo& pupil)
{
    return QString("eye=%1,center=(%2,%3),r=%4")
            .arg(pupil.whichEye == whichEye_Right ? "R" : "L")
            .arg(pupil.center().x, 0, 'f', 1)
            .arg(pupil.center().y, 0, 'f', 1)
            .arg(pupil.radius(), 0, 'f', 1);
}

QString previewEyeRejectReason(PupilInfo& pupil, enWhichEye targetEye, bool roughOk, bool regionOk)
{
    if (roughOk && regionOk) {
        return "ok";
    }
    if (pupil.radius() <= 0.0f) {
        return "no_pupil_or_eye";
    }
    if (pupil.whichEye != targetEye) {
        return "wrong_eye_side";
    }
    if (opticalPathType_LShape == g_opticalPathType && !regionOk) {
        return "lshape_region_reject";
    }
    if (!isNormalPupil(pupil.center(), targetEye)) {
        return "normal_region_reject";
    }
    if (!regionOk) {
        return "preview_region_reject";
    }
    if (!roughOk) {
        return "rough_reject";
    }
    return "unknown";
}
#endif

class CWriteAlgoStatusTask : public QRunnable
{
public:
    CWriteAlgoStatusTask(const QString &_usb_root,
                         const QString &_patient_dir,
                         const QString &_batch_dir,
                         const QString &_source_dir,
                         int _round_idx,
                         const QString &_content)
        : m_usbRoot(_usb_root),
          m_patientDir(_patient_dir),
          m_batchDir(_batch_dir),
          m_sourceDir(_source_dir),
          m_roundIdx(_round_idx),
          m_content(_content)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (m_content.isEmpty()) {
            return;
        }

        // 与同轮图片保存共用测量开始时的 U 盘路径快照，不在状态线程中查询 mount -l。
        if (m_usbRoot.isEmpty() || !QDir(m_usbRoot).exists()) {
            return;
        }

        const QString roundDir = QString("%1/screener_images/%2/%3/%4/round_%5")
                .arg(m_usbRoot)
                .arg(m_patientDir)
                .arg(m_batchDir)
                .arg(m_sourceDir)
                .arg(m_roundIdx, 2, 10, QLatin1Char('0'));
        QDir().mkpath(roundDir);

        QFile file(QString("%1/algo_status.txt").arg(roundDir));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            ALGO_ERROR_LOG(
                qCritical() << "CWriteAlgoStatusTask: failed to open"
                            << file.fileName()
            );
            return;
        }

        const QByteArray bytes = m_content.toUtf8();
        const qint64 written = file.write(bytes);
        if (written != bytes.size()) {
            ALGO_ERROR_LOG(
                qCritical() << "CWriteAlgoStatusTask: incomplete write"
                            << file.fileName() << ", written =" << written
                            << ", expected =" << bytes.size()
            );
        }
        file.flush();
        file.close();
    }

private:
    QString m_usbRoot;
    QString m_patientDir;
    QString m_batchDir;
    QString m_sourceDir;
    int m_roundIdx {-1};
    QString m_content;
};

static void writeAlgoStatusAsync(const QString &usbRoot,
                                 const QString &patientDir,
                                 const QString &batchDir,
                                 const QString &sourceDir,
                                 int roundIdx,
                                 const QString &content)
{
    // 算法线程只投递小文本，U 盘 IO 放到后台单线程，避免拖慢出结果回调。
    static QThreadPool statusWritePool;
    static bool isStatusWritePoolInited = false;
    if (!isStatusWritePoolInited) {
        statusWritePool.setMaxThreadCount(1);
        statusWritePool.setExpiryTimeout(30000);
        isStatusWritePoolInited = true;
    }

    statusWritePool.start(new CWriteAlgoStatusTask(usbRoot,
                                                   patientDir,
                                                   batchDir,
                                                   sourceDir,
                                                   roundIdx,
                                                   content),
                          -1);
}

static cv::Rect previewLShapeEffectiveRoi()
{
    // 采用 code 工程已验证的 L 型有效区域，覆盖更靠左、靠下的眼位。
    return cv::Rect(static_cast<int>(IMG_WIDTH * 0.09f),
                    static_cast<int>(IMG_HEIGHT * 0.25f),
                    static_cast<int>(IMG_WIDTH * 0.77f),
                    static_cast<int>(IMG_HEIGHT * 0.75f))
            & cv::Rect(0, 0, IMG_WIDTH, IMG_HEIGHT);
}

static cv::Rect previewExpectedEyeRegion(enWhichEye whichEye)
{
    if (opticalPathType_LShape == g_opticalPathType) {
        const cv::Rect effectiveRoi = previewLShapeEffectiveRoi();
        const int halfWidth = effectiveRoi.width / 2;
        // 图像左半区对应受检者右眼，图像右半区对应受检者左眼。
        if (whichEye == whichEye_Right) {
            return cv::Rect(effectiveRoi.x, effectiveRoi.y, halfWidth, effectiveRoi.height);
        }
        return cv::Rect(effectiveRoi.x + halfWidth,
                        effectiveRoi.y,
                        effectiveRoi.width - halfWidth,
                        effectiveRoi.height);
    }

    // 非 L 型继续沿用旧版左右半区安全范围，不额外引入红框限制。
    if (whichEye == whichEye_Right) {
        return cv::Rect(ROI_WIDTH_HALF,
                        ROI_HEIGHT_HALF,
                        IMG_WIDTH / 2 - ROI_WIDTH,
                        IMG_HEIGHT - ROI_HEIGHT);
    }
    return cv::Rect(IMG_WIDTH / 2 + ROI_WIDTH_HALF,
                    ROI_HEIGHT_HALF,
                    IMG_WIDTH / 2 - ROI_WIDTH,
                    IMG_HEIGHT - ROI_HEIGHT);
}

static bool containsPupilCenter(const cv::Rect& region, const cv::Point2f& center)
{
    return !cvRectEmpty(region)
            && center.x > region.x
            && center.x < region.x + region.width
            && center.y > region.y
            && center.y < region.y + region.height;
}

static bool isPreviewPupilInLShapeEffectiveRoi(PupilInfo& pupil, enWhichEye whichEye)
{
    return pupil.radius() > 0.0f
            && pupil.whichEye == whichEye
            && containsPupilCenter(previewLShapeEffectiveRoi(), pupil.center());
}

static bool isPreviewPupilInExpectedRegion(PupilInfo& pupil, enWhichEye whichEye)
{
    if (opticalPathType_LShape == g_opticalPathType) {
        // L 型预览只要求瞳孔在有效红框内；双眼左右关系交给后续几何校验，避免 ROI 中线误杀。
        return isPreviewPupilInLShapeEffectiveRoi(pupil, whichEye);
    }

    return pupil.radius() > 0.0f
            && pupil.whichEye == whichEye
            && containsPupilCenter(previewExpectedEyeRegion(whichEye), pupil.center());
}

static bool isPreviewPupilInSingleEyeRegion(PupilInfo& pupil, enWhichEye whichEye)
{
    if (opticalPathType_LShape != g_opticalPathType) {
        return isPreviewPupilInExpectedRegion(pupil, whichEye);
    }

    if (!isPreviewPupilInLShapeEffectiveRoi(pupil, whichEye)) {
        return false;
    }

    const cv::Rect effectiveRoi = previewLShapeEffectiveRoi();
    const float overlapRatio = 0.75f;
    // 单眼模式没有另一只眼互证，保留宽松方向限制；中间 50% 区域允许重叠，兼容不同机器 ROI 偏差。
    if (whichEye == whichEye_Right) {
        return pupil.center().x <= effectiveRoi.x + effectiveRoi.width * overlapRatio;
    }
    return pupil.center().x >= effectiveRoi.x + effectiveRoi.width * (1.0f - overlapRatio);
}

static constexpr float MIN_PREVIEW_EYE_X_DIFF = 250.0f;
static constexpr float MAX_PREVIEW_EYE_X_DIFF = 950.0f;
static constexpr float MAX_PREVIEW_EYE_Y_DIFF = 220.0f;
static constexpr int PUPIL_PAIR_TILE_WIDTH = 400;
static constexpr int PUPIL_PAIR_TILE_HEIGHT = 160;
static constexpr int PUPIL_MODEL_CPU_THREADS = 4;
static constexpr int PREVIEW_MODEL_MIN_INTERVAL_MS = 1000;
static constexpr float FORMAL_SMALL_SCALE_X = 3.2F;
static constexpr float FORMAL_SMALL_SCALE_Y = 3.2F;
// 正式轻量匹配统一在400×160小图上执行：原始图约为1280×512。
static constexpr float FORMAL_TRACK_SCALE = 0.3125f;
static constexpr float FORMAL_TRACK_RELIABLE_SCORE = 0.70f;
// v1.8在8个板端会话中验证通过的balanced ROI参数。
// 400×160小图上模板边长约15～27像素，搜索边距按小图坐标设置。
static constexpr int FORMAL_LIGHT_TEMPLATE_HALF = 13;
static constexpr int FORMAL_LIGHT_SEARCH_MARGIN = 10;
static constexpr int FORMAL_LIGHT_MOVEMENT_PADDING = 16;
static constexpr int FORMAL_CROSS_TEMPLATE_HALF = 42;
static constexpr int FORMAL_CROSS_SEARCH_MARGIN = 40;
static constexpr float FORMAL_CROSS_MINIMUM_SCORE = 0.60f;

// 正式路径所有轻量处理共用这一份400×160灰度图；只有这里负责缩放。
// 该缓存只用于模板匹配和梯度缓存，正式C800仍接收原图。
static cv::Mat makeFormalSmallGray(const cv::Mat& image)
{
#if ENABLE_ALGO_TIMING_LOG
    const auto started = std::chrono::steady_clock::now();
    const auto recordResize = [&started]() {
        // 正式流程的小图生成只做诊断记录，不参与后续匹配决策。
        if (AlgoTiming::currentPhase() == AlgoTimingPhase_Formal) {
            AlgoTiming::recordMilliseconds(
                    AlgoTimingStage_FormalSmallResize,
                    std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - started).count());
        }
    };
#endif
    if (image.empty()) {
#if ENABLE_ALGO_TIMING_LOG
        recordResize();
#endif
        return cv::Mat();
    }
    cv::Mat gray;
    if (image.channels() == 1) {
        gray = image;
    } else if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    } else {
#if ENABLE_ALGO_TIMING_LOG
        recordResize();
#endif
        return cv::Mat();
    }
    if (gray.size() == cv::Size(PUPIL_PAIR_TILE_WIDTH,
                                PUPIL_PAIR_TILE_HEIGHT)) {
        cv::Mat small = gray.clone();
#if ENABLE_ALGO_TIMING_LOG
        recordResize();
#endif
        return small;
    }
    cv::Mat small;
    cv::resize(gray, small,
               cv::Size(PUPIL_PAIR_TILE_WIDTH, PUPIL_PAIR_TILE_HEIGHT),
               0.0, 0.0, cv::INTER_AREA);
#if ENABLE_ALGO_TIMING_LOG
    recordResize();
#endif
    return small;
}

static void mapFormalSmallFrameToOriginal(PupilLightFrame* frame,
                                          const cv::Size& originalSize)
{
    if (frame == nullptr
            || (!frame->subjectRight.detected
                && !frame->subjectLeft.detected)) {
        return;
    }
    const float scaleX = originalSize.width == 1280
            ? FORMAL_SMALL_SCALE_X
            : static_cast<float>(originalSize.width)
                / static_cast<float>(PUPIL_PAIR_TILE_WIDTH);
    const float scaleY = originalSize.height == 512
            ? FORMAL_SMALL_SCALE_Y
            : static_cast<float>(originalSize.height)
                / static_cast<float>(PUPIL_PAIR_TILE_HEIGHT);
    const float radiusScale = (scaleX + scaleY) * 0.5F;
    auto mapEye = [scaleX, scaleY, radiusScale](PupilLightEye& eye) {
        if (!eye.detected) {
            return;
        }
        eye.center.x *= scaleX;
        eye.center.y *= scaleY;
        eye.radius *= radiusScale;
    };
    mapEye(frame->subjectRight);
    mapEye(frame->subjectLeft);
}

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
// 小图匹配诊断格式化只服务于测试日志，不参与任何算法计算。
static QString formatSmallMatchRect(const cv::Rect& rect)
{
    return QString("(%1,%2,%3,%4)")
            .arg(rect.x).arg(rect.y).arg(rect.width).arg(rect.height);
}

static QString formatSmallMatchPoint(const cv::Point2f& point)
{
    return QString("(%1,%2)")
            .arg(point.x, 0, 'f', 1).arg(point.y, 0, 'f', 1);
}

static QString formatSmallMatchSize(const cv::Size& size)
{
    return QString("(%1,%2)").arg(size.width).arg(size.height);
}
#endif

static PupilLightFrame makeFormalSmallPupilFrame(
        const PupilLightFrame& originalFrame,
        const cv::Size& originalSize)
{
    PupilLightFrame smallFrame = originalFrame;
    const float scaleX = originalSize.width == 1280
            ? 1.0F / FORMAL_SMALL_SCALE_X
            : static_cast<float>(PUPIL_PAIR_TILE_WIDTH)
                / static_cast<float>(originalSize.width);
    const float scaleY = originalSize.height == 512
            ? 1.0F / FORMAL_SMALL_SCALE_Y
            : static_cast<float>(PUPIL_PAIR_TILE_HEIGHT)
                / static_cast<float>(originalSize.height);
    const float radiusScale = (scaleX + scaleY) * 0.5F;
    auto mapEye = [scaleX, scaleY, radiusScale](PupilLightEye& eye) {
        if (!eye.detected) {
            return;
        }
        eye.center.x *= scaleX;
        eye.center.y *= scaleY;
        eye.radius *= radiusScale;
    };
    mapEye(smallFrame.subjectRight);
    mapEye(smallFrame.subjectLeft);
    return smallFrame;
}
// 儿童手持和快速靠近设备时，相邻轮之间可能超过常规±40px搜索范围。
// 先在1/4尺度整图中做大范围粗定位，再回到上述高分辨率窗口精匹配；
// 粗定位不直接产出瞳孔坐标，也不进入DS计算。
static constexpr bool FORMAL_CROSS_ENABLE_COARSE_PREALIGNMENT = true;
static constexpr float FORMAL_CROSS_COARSE_PROCESSING_SCALE = 0.25f;
static constexpr int FORMAL_CROSS_COARSE_SEARCH_MARGIN = 224;
static constexpr float FORMAL_CROSS_COARSE_MINIMUM_SCORE = 0.42f;
// 单眼完整匹配仅允许复用两张位移一致的近阈值轨迹；该阈值不会放宽
// 正常稀疏快速校正，且非锚点插值仍按低可信排除在DS计算之外。
static constexpr float FORMAL_CROSS_SINGLE_EYE_FALLBACK_MINIMUM_SCORE = 0.55f;
// 儿童手持时，一眼被眼睑遮挡并不罕见。仅当另一眼已有两张一致轨迹、
// 被遮挡眼仍有一张局部弱证据时，允许先完成本轮定位；此类坐标保持低可信，
// 但会作为 DS 候选交给既有的 ROI 与屈光质量结算，不能仅凭模板分数直接采纳。
static constexpr float FORMAL_CROSS_SINGLE_EYE_CARRY_MINIMUM_SCORE = 0.45f;
static constexpr float FORMAL_CROSS_SINGLE_EYE_CARRY_MAX_DELTA_DIFFERENCE = 12.0f;
static constexpr float FORMAL_CROSS_SINGLE_EYE_CARRY_MAX_DISPLACEMENT = 24.0f;
// 跨轮快速校正按“相机实际拍摄照片编号”配置，不使用灯位或内部槽位。
// 运行时会根据当前光路反查为images数组索引。
static const std::array<int, 3> FORMAL_CROSS_SPARSE_CAPTURE_NUMBERS =
        {{1, 11, 17}};
// 第2、17张各自缩放到400×160，再左右拼接成800×160；模型一次输出两张图的四个瞳孔。
static const char PUPIL_PAIR_ONNX_MODEL_PATH[] =
        "/root/screener-models/pupil/releases/spatial_concat_800x160_dataset12/model_fp32.onnx";
#if defined(ENABLE_RKNN_C800) && ENABLE_RKNN_C800
static const char PUPIL_PAIR_RKNN_MODEL_PATH[] =
        "/root/screener-models/pupil/releases/spatial_concat_800x160_dataset12/c800_fp16.rknn";
#endif
// 正常样本只调用这一组经过多轮验证的主锚点。L型箱完成光路转换前，
// 它对应相机实际拍摄的第1张+第11张；其它光路仍由原有转换层统一处理。
static const std::array<int, 2> FORMAL_PRIMARY_MODEL_ANCHORS = {{2, 17}};
// 正式逐照片异步路径只在首轮首张有效照片上调用一次C800；锚点建立后，
// 后续照片和轮次只使用匹配、129 ROI精修及既有DS流程。
// 旧整轮兼容函数仍需要该上限；正式逐照片异步路径不读取它。
static constexpr int FORMAL_MAX_MODEL_CALLS_PER_ROUND = 3;
static constexpr int FORMAL_HAAR_FALLBACK_MAX_IMAGES = 4;
// 正式真人测量默认固定使用逐照片异步实现；旧整轮函数仅保留作A/B或故障回退。
static constexpr bool FORMAL_PER_FRAME_ASYNC_ENABLED = true;

static std::mutex g_pupilModelThreadConfigMutex;

class ScopedOpenCvThreadCount
{
public:
    explicit ScopedOpenCvThreadCount(int threadCount)
        : m_lock(g_pupilModelThreadConfigMutex),
          m_previousCount(cv::getNumThreads())
    {
        // OpenCV 线程数是进程级配置；模型/跟踪阶段串行切到 4 线程，结束后恢复传统算法配置。
        cv::setNumThreads(std::max(1, threadCount));
    }

    ~ScopedOpenCvThreadCount()
    {
        cv::setNumThreads(std::max(1, m_previousCount));
    }

private:
    std::unique_lock<std::mutex> m_lock;
    int m_previousCount;
};

static bool pairModelEyeResultIsUsable(const PupilPairEyeResult& eye,
                                       enWhichEye whichEye)
{
    if (!eye.detected || eye.equivalentRadius < 6.0f
            || eye.equivalentRadius > 64.0f) {
        return false;
    }
    return isNormalPupil(eye.center, whichEye);
}

// 正式模板和跨轮定位共用同一套瞳孔几何门禁，先拦截非有限坐标，
// 再检查半径范围和既有眼别区域规则，避免把异常点保存成模板。
static bool formalPupilResultIsUsable(const stPupilInfo& pupil,
                                      enWhichEye whichEye)
{
    return std::isfinite(pupil.center.x)
            && std::isfinite(pupil.center.y)
            && std::isfinite(pupil.radius)
            && pupil.radius >= 6.0
            && pupil.radius <= 64.0
            && isNormalPupil(pupil.center, whichEye);
}

// 限量Haar安全网沿用正式计算的半径与眼别几何门禁，不能因兜底而
// 放宽深度模型的验收标准。
static bool traditionalPupilResultIsUsable(const stPupilInfo& pupil,
                                           enWhichEye whichEye)
{
    return formalPupilResultIsUsable(pupil, whichEye);
}

struct FormalFallbackCandidate
{
    int imageNumber = -1;
    int captureNumber = -1;
    double quality = 0.0;
};

// 正式算法内部保存的是光路转换后的固定槽位。动态选图只在排序时还原
// 实际拍摄照片序号；所有对外日志都使用本函数返回值，不使用内部槽位号。
static int formalCaptureNumber(int imageNumber)
{
    if (imageNumber < 1 || imageNumber > FRAMES_PER_ROUND) {
        return imageNumber;
    }

    if (opticalPathType_LShape == g_opticalPathType) {
        static const int captureByImage[FRAME_ARRAY_SIZE] = {
            0,
            2, 1, 4, 3, 6, 5,
            13, 14, 15, 16, 17, 18,
            7, 8, 9, 10, 11, 12,
            21, 22, 19, 20
        };
        return captureByImage[imageNumber];
    }

    if (opticalPathType_Square == g_opticalPathType) {
        // 方形箱按相邻两张交换。
        return imageNumber % 2 == 0 ? imageNumber - 1 : imageNumber + 1;
    }
    return imageNumber;
}

// 跨轮策略只配置“实际拍摄第几张”，而图像缓存只能按内部槽位访问。
// 这里集中完成反查，禁止各处手写L型箱/方形箱的槽位映射。
static int formalImageNumberByCaptureNumber(int captureNumber)
{
    if (captureNumber < 1 || captureNumber > FRAMES_PER_ROUND) {
        return -1;
    }
    for (int imageNumber = 1;
         imageNumber <= FRAMES_PER_ROUND;
         ++imageNumber) {
        if (formalCaptureNumber(imageNumber) == captureNumber) {
            return imageNumber;
        }
    }
    return -1;
}

static double formalEyeRegionQuality(const cv::Mat& image,
                                     const cv::Rect& requestedRegion)
{
    if (image.empty() || image.type() != CV_8UC1) {
        return -1e9;
    }
    const cv::Rect imageBounds(0, 0, image.cols, image.rows);
    const cv::Rect region = requestedRegion & imageBounds;
    if (region.width < 16 || region.height < 16) {
        return -1e9;
    }

    cv::Mat reduced;
    cv::resize(image(region), reduced, cv::Size(128, 80),
               0.0, 0.0, cv::INTER_AREA);

    cv::Scalar meanValue;
    cv::Scalar stdValue;
    cv::meanStdDev(reduced, meanValue, stdValue);

    cv::Mat gradX;
    cv::Mat gradY;
    cv::Mat absX;
    cv::Mat absY;
    cv::Mat edge;
    cv::Sobel(reduced, gradX, CV_16S, 1, 0, 3);
    cv::Sobel(reduced, gradY, CV_16S, 0, 1, 3);
    cv::convertScaleAbs(gradX, absX);
    cv::convertScaleAbs(gradY, absY);
    cv::addWeighted(absX, 0.5, absY, 0.5, 0.0, edge);
    const double edgeMean = cv::mean(edge)[0];

    cv::Mat darkMask;
    cv::Mat brightMask;
    cv::compare(reduced, cv::Scalar(2), darkMask, cv::CMP_LE);
    cv::compare(reduced, cv::Scalar(253), brightMask, cv::CMP_GE);
    const double clippedRatio =
            static_cast<double>(cv::countNonZero(darkMask)
                                + cv::countNonZero(brightMask))
            / static_cast<double>(reduced.total());
    const double exposureScore = std::max(0.0, 1.0 - clippedRatio);

    // 排序以局部对比度和边缘为主，曝光项只用于压低全黑、全白图片。
    return stdValue[0] * 1.5 + edgeMean + exposureScore * 20.0;
}

static double formalFallbackImageQuality(const cv::Mat& image,
                                         bool needRight,
                                         bool needLeft)
{
    std::vector<double> eyeScores;
    if (needRight) {
        eyeScores.push_back(formalEyeRegionQuality(
                image, previewExpectedEyeRegion(whichEye_Right)));
    }
    if (needLeft) {
        eyeScores.push_back(formalEyeRegionQuality(
                image, previewExpectedEyeRegion(whichEye_Left)));
    }
    if (eyeScores.empty()) {
        return -1e9;
    }
    if (eyeScores.size() == 1) {
        return eyeScores.front();
    }

    // 双眼模式要求同一张图片的两眼都能作为可靠锚点，因此以较差眼为主，
    // 同时保留少量平均质量，避免单眼特别清晰掩盖另一眼严重异常。
    const double minimum = std::min(eyeScores[0], eyeScores[1]);
    const double average = (eyeScores[0] + eyeScores[1]) * 0.5;
    return minimum * 0.75 + average * 0.25;
}

static bool selectFormalDynamicFallbackPair(
        const std::vector<cv::Mat>& images,
        const std::bitset<FRAME_ARRAY_SIZE>& attemptedFrames,
        bool needRight,
        bool needLeft,
        std::array<int, 2>* selectedImageNumbers,
        std::array<double, 2>* selectedQualities)
{
    if (selectedImageNumbers == nullptr || selectedQualities == nullptr) {
        return false;
    }

    std::vector<FormalFallbackCandidate> candidates;
    for (int imageNumber = 1;
         imageNumber <= FRAMES_PER_ROUND;
         ++imageNumber) {
        if (attemptedFrames.test(imageNumber)
                || imageNumber > static_cast<int>(images.size())
                || images[imageNumber - 1].empty()) {
            continue;
        }
        FormalFallbackCandidate candidate;
        candidate.imageNumber = imageNumber;
        candidate.captureNumber = formalCaptureNumber(imageNumber);
        candidate.quality = formalFallbackImageQuality(
                images[imageNumber - 1], needRight, needLeft);
        candidates.push_back(candidate);
    }
    if (candidates.size() < 2) {
        return false;
    }

    double bestPairScore = -1e18;
    size_t bestFirst = 0;
    size_t bestSecond = 1;
    for (size_t first = 0; first < candidates.size(); ++first) {
        for (size_t second = first + 1;
             second < candidates.size();
             ++second) {
            const int captureGap = std::abs(
                    candidates[first].captureNumber
                    - candidates[second].captureNumber);
            const double gapReward =
                    std::min(10, captureGap) * 2.0;
            const double adjacentPenalty =
                    captureGap < 3 ? (3 - captureGap) * 15.0 : 0.0;
            const double score =
                    candidates[first].quality
                    + candidates[second].quality
                    + gapReward
                    - adjacentPenalty;
            if (score > bestPairScore) {
                bestPairScore = score;
                bestFirst = first;
                bestSecond = second;
            }
        }
    }

    (*selectedImageNumbers)[0] = candidates[bestFirst].imageNumber;
    (*selectedImageNumbers)[1] = candidates[bestSecond].imageNumber;
    (*selectedQualities)[0] = candidates[bestFirst].quality;
    (*selectedQualities)[1] = candidates[bestSecond].quality;
    return true;
}

static stPupilInfo makePairModelPupilInfo(const PupilPairEyeResult& eye,
                                         int fallbackType)
{
    stPupilInfo info;
    info.center = eye.center;
    info.spotPt = eye.center;
    info.radius = eye.equivalentRadius;
    info.rect = cv::Rect(cvRound(eye.center.x - info.radius),
                         cvRound(eye.center.y - info.radius),
                         cvRound(info.radius * 2.0f),
                         cvRound(info.radius * 2.0f));
    info.area = CV_PI * info.radius * info.radius;
    info.perimeter = 2.0 * CV_PI * info.radius;
    info.circularity = 1.0;
    info.dx = 0.0f;
    info.dy = 0.0f;
    info.fallbackType = fallbackType;
    info.eyeRectSource = PupilEyeRect_Base;
    info.coordinateSource = PupilSource_DeepModel;
    return info;
}

static stPupilInfo makeTrackedPupilInfo(const PupilLightEye& eye)
{
    stPupilInfo info;
    info.center = eye.center;
    info.spotPt = eye.center;
    info.radius = eye.radius;
    info.rect = cv::Rect(cvRound(eye.center.x - eye.radius),
                         cvRound(eye.center.y - eye.radius),
                         cvRound(eye.radius * 2.0f),
                         cvRound(eye.radius * 2.0f));
    info.area = CV_PI * eye.radius * eye.radius;
    info.perimeter = 2.0 * CV_PI * eye.radius;
    info.circularity = 1.0;
    info.dx = 0.0f;
    info.dy = 0.0f;
    info.fallbackType = PupilFallback_DeepTrack;
    info.eyeRectSource = PupilEyeRect_Base;
    info.coordinateSource = eye.source;
    return info;
}

static const char* coordinateSourceName(int source)
{
    switch (source) {
    case PupilSource_DeepModel:
        return "deep_model";
    case PupilSource_LightTrack:
        return "light_track";
    case PupilSource_CrossRoundLocal:
        return "cross_round_local";
    case PupilSource_CrossRoundInterpolated:
        return "cross_round_interpolated";
    case PupilSource_CrossRoundOtherEye:
        return "cross_round_other_eye";
    case PupilSource_CrossRoundDirectReuse:
        return "cross_round_direct_reuse";
    case PupilSource_TraditionalFallback:
        return "traditional_fallback";
    case PupilSource_RoiRefined:
        return "roi_refined";
    default:
        return "unknown";
    }
}

} // namespace

static void appendLe16(QByteArray &data, quint16 value)
{
    char bytes[2] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff)
    };
    data.append(bytes, sizeof(bytes));
}

static void appendLe32(QByteArray &data, quint32 value)
{
    char bytes[4] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff)
    };
    data.append(bytes, sizeof(bytes));
}

static bool writeGrayBmpFile(const QString &filePath, const cv::Mat &gray)
{
    if (gray.empty() || gray.channels() != 1 || gray.depth() != CV_8U) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        ALGO_ERROR_LOG(
            qCritical() << "writeGrayBmpFile open failed:" << filePath
                        << file.errorString()
        );
        return false;
    }

    const int width = gray.cols;
    const int height = gray.rows;
    const int rowStride = (width + 3) & ~3;
    const int pixelBytes = rowStride * height;
    const int paletteBytes = 256 * 4;
    const int pixelOffset = 14 + 40 + paletteBytes;
    const int fileBytes = pixelOffset + pixelBytes;

    QByteArray header;
    header.reserve(pixelOffset);
    header.append('B');
    header.append('M');
    appendLe32(header, static_cast<quint32>(fileBytes));
    appendLe16(header, 0);
    appendLe16(header, 0);
    appendLe32(header, static_cast<quint32>(pixelOffset));

    appendLe32(header, 40);
    appendLe32(header, static_cast<quint32>(width));
    appendLe32(header, static_cast<quint32>(height));
    appendLe16(header, 1);
    appendLe16(header, 8);
    appendLe32(header, 0);
    appendLe32(header, static_cast<quint32>(pixelBytes));
    appendLe32(header, 0);
    appendLe32(header, 0);
    appendLe32(header, 256);
    appendLe32(header, 256);

    for (int i = 0; i < 256; ++i) {
        header.append(static_cast<char>(i));
        header.append(static_cast<char>(i));
        header.append(static_cast<char>(i));
        header.append(static_cast<char>(0));
    }

    if (file.write(header) != header.size()) {
        ALGO_ERROR_LOG(
            qCritical() << "writeGrayBmpFile header write failed:" << filePath
                        << file.errorString()
        );
        file.close();
        return false;
    }

    QByteArray row(rowStride, 0);
    for (int y = height - 1; y >= 0; --y) {
        const uchar *src = gray.ptr<uchar>(y);
        std::memcpy(row.data(), src, static_cast<size_t>(width));
        if (rowStride > width) {
            std::memset(row.data() + width, 0, static_cast<size_t>(rowStride - width));
        }
        if (file.write(row) != row.size()) {
            ALGO_ERROR_LOG(
                qCritical() << "writeGrayBmpFile pixel write failed:" << filePath
                            << file.errorString()
            );
            file.close();
            return false;
        }
    }

    file.flush();
    file.close();
    return QFileInfo(filePath).size() > 0;
}

// 获取 U 盘根路径，找不到返回空
QString getUSBDrivePath()
{
    QStringList usbPaths;
    usbPaths << "/media" << "/mnt";

    foreach (const QString &base, usbPaths) {
        QDir dir(base);
        if (!dir.exists()) {
            continue;
        }

        QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        foreach (const QString &sub, subDirs) {
            if (!sub.startsWith("usb", Qt::CaseInsensitive)) {
                continue;
            }

            QString path = base + "/" + sub;
            QFileInfo info(path);
            if (info.isWritable()) {
                return path;
            }
        }
    }

    return QString();
}

// 保存原始图片，优先写入 U 盘；无 U 盘时回退到原算法图片目录
void saveImage(const cv::Mat& image,
               int round_idx,
               int img_idx,
               QString _sub_dir_name,
               int mode = 0)
{
    QString saveImagePath;

    QString usbRoot = getUSBDrivePath();
    qDebug() << "usbRoot:" << usbRoot;
    if (!usbRoot.isEmpty()) {
        saveImagePath = usbRoot + "/screener_images";

        QDir usbDir(saveImagePath);
        if (!usbDir.exists()) {
            if (!usbDir.mkpath(saveImagePath)) {
                qDebug() << "Failed to create screener_images on USB";
                return;
            }
        }

        saveImagePath += QString("/%1/%2/")
                            .arg(_sub_dir_name)
                            .arg(round_idx);
    } else {
        saveImagePath = QString::fromStdString(CAlgoIntf::getImageDirPath())
                        + QString("/%1/%2/")
                              .arg(_sub_dir_name)
                              .arg(round_idx);
    }

    qDebug() << "saveImagePath:" << saveImagePath;

    QDir myDir(saveImagePath);
    if (!myDir.exists()) {
        if (!myDir.mkpath(saveImagePath)) {
            return;
        }
    }

    QString NumCount = QString("%1").arg(img_idx, 2, 10, QLatin1Char('0'));
    QString finalbuff = saveImagePath + "/temp" + NumCount + ".bmp";

    cv::Mat saveMat;
    if (mode == 1) {
        cv::Mat roi(image, cv::Rect(IMG_WIDTH / 2, 0, IMG_WIDTH / 2, IMG_HEIGHT));
        saveMat = roi.clone();
    } else if (mode == 2) {
        cv::Mat roi(image, cv::Rect(0, 0, IMG_WIDTH / 2, IMG_HEIGHT));
        saveMat = roi.clone();
    } else {
        saveMat = image.isContinuous() ? image : image.clone();
    }

    bool saveSucc = writeGrayBmpFile(finalbuff, saveMat);
    if (!usbRoot.isEmpty()) {
        Util::CUDisk::sync();
    }

    qDebug() << "saveImage file:" << finalbuff << "succ:" << saveSucc
             << "fileBytes:" << QFileInfo(finalbuff).size()
             << "size:" << saveMat.cols << "x" << saveMat.rows
             << "channels:" << saveMat.channels();
}

//测试过程中保存22张图片
void testSaveByteImageinFolder(unsigned char *pFrameBuffer, int index, QString _sub_dir_name, int mode)
{
    //qDebug()<<"testSaveImageinFolder*********************************************";
    QString strName = _sub_dir_name;
    QString finalbuff;

    QString saveImagePath = "";
    if(mode >= 1 && mode <= 3)
        saveImagePath = QString::fromStdString(CAlgoIntf::getImageDirPath()) + QString("/%1").arg(strName);    //保存图片位置
    else
        saveImagePath = QString::fromStdString(CAlgoIntf::getImageDirPath()) + QString("/%1/succ_pictures").arg(strName);
    QDir myDir(saveImagePath);
    if(!myDir.exists()){
        if(!myDir.mkpath(saveImagePath))
            return;
    }

    QString NumCount = QString("%1").arg(index,2,10,QLatin1Char('0')); //如果是个位数时,前面补0
    finalbuff = saveImagePath + "/temp" + NumCount + ".jpg";

    if(mode == 1){      //左眼模式
        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
        cv::Mat roi(image, cv::Rect(IMG_WIDTH / 2, 0, IMG_WIDTH / 2, IMG_HEIGHT));
        imwrite(finalbuff.toStdString(),roi);
        return;
    }
    else if(mode == 2){  //右眼模式
        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
        cv::Mat roi(image, cv::Rect(0, 0, IMG_WIDTH / 2, IMG_HEIGHT));
        imwrite(finalbuff.toStdString(),roi);
        return;
    }
    else{               //双眼模式
        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
        imwrite(finalbuff.toStdString(),image);
        return;
    }
    //qDebug() << "ba.data():" << ba.data();
}

//保存失败的图像
void SaveByteFaultImageinFolder(unsigned char *pFrameBuffer, int index, QString _sub_dir_name, int mode, QString strPath)
{
    //qDebug()<<"********************* save fault image ************************";
    QString strName = _sub_dir_name;
    if(mode >= 1 && mode <= 3)
        strName = QString::fromStdString(CAlgoIntf::getImageDirPath()) + QString("/%1").arg(strName);    //保存图片位置
    else
        strName = strPath;

    QDir myDir(strName);
    if(!myDir.exists()){
        if(!myDir.mkpath(strName))
            return;
    }

    QString NumCount = QString("%1").arg(index,2,10,QLatin1Char('0')); //如果是个位数时,前面补0
    QString finalbuff = strName + "/temp" + NumCount + ".jpg";       //QString::number(aeValue,'f',1) 保留1位小数

    if(mode == 1){      //左眼模式
        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1, pFrameBuffer).clone(); // make a copy
        cv::Mat roi(image, cv::Rect(IMG_WIDTH / 2, 0, IMG_WIDTH / 2, IMG_HEIGHT));
        imwrite(finalbuff.toStdString(),roi);
        return;
    }
    else if(mode == 2){  //右眼模式
        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
        cv::Mat roi(image, cv::Rect(0, 0, IMG_WIDTH / 2, IMG_HEIGHT));
        imwrite(finalbuff.toStdString(),roi);
        return;
    }
    else{               //双眼模式
        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
        imwrite(finalbuff.toStdString(),image);
        return;
    }
}

bool validatePupilData(
    const std::array<cv::Mat, FRAME_ARRAY_SIZE>& imgArray,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& infoArray,
    const std::bitset<FRAME_ARRAY_SIZE>& validMask)
{
    // 循环从 1 到 22，直接用 imgNo 作为索引检查 bitset
    for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
        if (!validMask.test(i)) {
            qDebug() << "validMask test failed at index" << i;
            return false;
        }
        if (imgArray[i].empty()) {
            qDebug() << "imgArray empty at index" << i;
            return false;
        }
    }
    return true;
}


struct QRunnableFunction : public QRunnable
{
    std::function<void()> fun;
    explicit QRunnableFunction(std::function<void()> f) : fun(std::move(f))
    { setAutoDelete(true); }
    void run() override { fun(); }
};



/** ===================================================================================
 * class CAlgo
 */
// 构造
CAlgo::CAlgo() : CAlgoIntf()
{
    qDebug() << QString(__PRETTY_FUNCTION__) << "20260414_1";
    pool = QThreadPool::globalInstance();

    const int hardwareThreads =
            static_cast<int>(std::thread::hardware_concurrency());

    // 至少给界面、采集及系统线程保留一个CPU核心。
    const int availableWorkerThreads =
            hardwareThreads > 1 ? hardwareThreads - 1 : 1;
    const int optimalThreads =
            std::min(availableWorkerThreads,
                     FORMAL_MAX_CONCURRENT_PHOTO_TASKS);

    // 正式照片任务最多并行3个，线程池容量必须同步达到该上限。
    pool->setMaxThreadCount(std::max(1, optimalThreads));
    qDebug() << "optimal_threads=" << std::max(1, optimalThreads);
    cv::setNumThreads(1);

    // 混合正式轮不能使用上面的多线程池，否则相邻轮可能并发运行整套模型与跟踪。
    // 专用池只保留一个工作线程，其内部队列按提交顺序处理各轮。
    m_formalHybridPool.setMaxThreadCount(1);
    m_formalHybridPool.setExpiryTimeout(30000);
    m_formalCrossRoundState.reset(new FormalCrossRoundState());

    //Cascade对象池
//    initializeRobustEyeCascadePool(22);
}

// 解析
CAlgo::~CAlgo()
{
    // 先丢弃尚未开始的正式混合任务，再等待正在运行的任务退出，
    // 防止异步任务在CAlgo析构后继续访问this。
    m_formalStreamingStop.store(true, std::memory_order_release);
    m_formalFrameCondition.notify_all();
    m_formalHybridPool.clear();
    m_formalHybridPool.waitForDone();
    // 新逐照片任务使用全局线程池，不能清空全局池；等待的范围仅限于
    // 本对象自己提交的任务，避免析构后任务继续访问this。
    {
        std::unique_lock<std::mutex> lock(m_formalAsyncWaitMutex);
        m_formalAsyncWaitCondition.wait(lock, [this]() {
            return m_formalAsyncTasksInFlight.load(
                    std::memory_order_acquire) == 0;
        });
    }
    m_formalHybridTasksInFlight.store(0, std::memory_order_release);
}

double CAlgo::calcImageClarity(const cv::Mat &_image) const
{
    if (_image.empty()) {
        return -1.0;
    }

    try {
        cv::Mat grayImage;
        if (_image.type() == CV_8UC1) {
            grayImage = _image;
        } else if (_image.type() == CV_8UC3) {
            cv::cvtColor(_image, grayImage, cv::COLOR_BGR2GRAY);
        } else {
            // 第一版只接受8位灰度图和8位BGR图，避免不同格式转换造成
            // 清晰度指标不可直接比较。
            return -1.0;
        }

        // 只计算中央80%，降低边缘暗角、固定边框和非标定板区域的干扰。
        const int marginX = grayImage.cols / 10;
        const int marginY = grayImage.rows / 10;
        const cv::Rect clarityRoi(
                marginX,
                marginY,
                grayImage.cols - marginX * 2,
                grayImage.rows - marginY * 2);
        if (clarityRoi.width < 16 || clarityRoi.height < 16) {
            return -1.0;
        }

        const cv::Mat roiImage = grayImage(clarityRoi);

        // 先做轻量高斯降噪，避免随机噪点导致拉普拉斯方差虚高。
        cv::Mat blurredImage;
        cv::GaussianBlur(
                roiImage,
                blurredImage,
                cv::Size(3, 3),
                0.0,
                0.0,
                cv::BORDER_DEFAULT);

        // 使用CV_64F保留正负二阶梯度，避免响应截断或溢出。
        cv::Mat laplacianImage;
        cv::Laplacian(
                blurredImage,
                laplacianImage,
                CV_64F,
                3,
                1.0,
                0.0,
                cv::BORDER_DEFAULT);

        cv::Scalar laplacianMean;
        cv::Scalar laplacianStdDev;
        cv::meanStdDev(
                laplacianImage,
                laplacianMean,
                laplacianStdDev);

        const double clarity =
                laplacianStdDev[0] * laplacianStdDev[0];
        if (!std::isfinite(clarity) || clarity < 0.0) {
            return -1.0;
        }
        return clarity;
    } catch (const cv::Exception &) {
        return -1.0;
    } catch (...) {
        return -1.0;
    }
}

bool CAlgo::ensurePupilModelLoaded()
{
    std::lock_guard<std::mutex> lock(m_pupilModelMutex);
    if (pupilPairModelAvailable()) {
        return true;
    }
    if (m_pupilModelLoadAttempted) {
        return false;
    }

    m_pupilModelLoadAttempted = true;

#if defined(ENABLE_RKNN_C800) && ENABLE_RKNN_C800
    std::unique_ptr<PupilPairRknnDetector> rknnDetector(
            new PupilPairRknnDetector(PUPIL_PAIR_TILE_WIDTH,
                                      PUPIL_PAIR_TILE_HEIGHT));
    std::string rknnError;
    if (rknnDetector->load(PUPIL_PAIR_RKNN_MODEL_PATH, &rknnError)) {
        const QString apiVersion =
                QString::fromStdString(rknnDetector->apiVersion());
        const QString driverVersion =
                QString::fromStdString(rknnDetector->driverVersion());
        m_pupilPairRknnDetector = std::move(rknnDetector);
        m_pupilPairRknnAvailable.store(true, std::memory_order_release);
        ALGO_KEY_LOG(
            qInfo().noquote()
                << QString("[DL_INIT] model=c800,backend=rknn_npu,"
                           "status=ready,api=%1,driver=%2")
                   .arg(apiVersion, driverVersion)
        );
    } else {
        ALGO_ERROR_LOG(
            qWarning().noquote()
                << QString("[DL_ERROR] stage=rknn_model_load,"
                           "action=use_onnx_cpu,reason=%1")
                   .arg(QString::fromStdString(rknnError))
        );
    }
#endif

    // CPU后端始终一并加载，既作为无NPU设备的正常实现，也保证NPU在
    // 运行期异常时可以立即回退，而不在拍摄线程中临时加载模型。
    std::unique_ptr<PupilPairOnnxDetector> pairDetector(
            new PupilPairOnnxDetector(PupilPairOnnxDetector::SpatialConcat,
                                      PUPIL_PAIR_TILE_WIDTH,
                                      PUPIL_PAIR_TILE_HEIGHT));
    std::string pairError;
    if (!pairDetector->load(PUPIL_PAIR_ONNX_MODEL_PATH, &pairError)) {
        ALGO_ERROR_LOG(
            qWarning().noquote()
                << QString("[DL_ERROR] stage=onnx_model_load,reason=%1")
                   .arg(QString::fromStdString(pairError))
        );
    } else {
        m_pupilPairOnnxDetector = std::move(pairDetector);
        ALGO_KEY_LOG(
            qInfo().noquote()
                << "[DL_INIT] model=c800,backend=onnx_cpu,status=ready"
        );
    }

    if (!pupilPairModelAvailable()) {
        ALGO_ERROR_LOG(
            qCritical().noquote()
                << "[DL_ERROR] stage=model_load,reason=no_available_backend"
        );
        return false;
    }
    return true;
}

bool CAlgo::pupilPairModelAvailable() const
{
#if defined(ENABLE_RKNN_C800) && ENABLE_RKNN_C800
    if (m_pupilPairRknnDetector
            && m_pupilPairRknnAvailable.load(std::memory_order_acquire)) {
        return true;
    }
#endif
    return static_cast<bool>(m_pupilPairOnnxDetector);
}

bool CAlgo::inferPupilPairModel(const cv::Mat& firstImage,
                                const cv::Mat& secondImage,
                                PupilPairResult* result,
                                float logitThreshold,
                                int minimumComponentArea,
                                std::string* errorMessage)
{
    std::string npuError;
#if defined(ENABLE_RKNN_C800) && ENABLE_RKNN_C800
    if (m_pupilPairRknnDetector
            && m_pupilPairRknnAvailable.load(std::memory_order_acquire)) {
        if (m_pupilPairRknnDetector->infer(firstImage,
                                           secondImage,
                                           result,
                                           logitThreshold,
                                           minimumComponentArea,
                                           &npuError)) {
            return true;
        }

        // 同一次进程中不反复调用已经失败的NPU上下文，避免每张照片
        // 都重复失败并拖慢拍摄；重启程序后会重新尝试初始化NPU。
        const bool wasAvailable = m_pupilPairRknnAvailable.exchange(
                    false, std::memory_order_acq_rel);
        if (wasAvailable) {
            ALGO_ERROR_LOG(
                qCritical().noquote()
                    << QString("[DL_ERROR] stage=rknn_infer,"
                               "action=disable_npu_and_fallback_cpu,reason=%1")
                       .arg(QString::fromStdString(npuError))
            );
        }
    }
#endif

    if (m_pupilPairOnnxDetector) {
        std::string onnxError;
        const bool succeeded = m_pupilPairOnnxDetector->infer(
                    firstImage,
                    secondImage,
                    result,
                    logitThreshold,
                    minimumComponentArea,
                    &onnxError);
        if (!succeeded && errorMessage) {
            *errorMessage = npuError.empty()
                    ? onnxError
                    : npuError + "; ONNX fallback failed: " + onnxError;
        }
        return succeeded;
    }

    if (errorMessage) {
        *errorMessage = npuError.empty()
                ? "No C800 inference backend is available." : npuError;
    }
    return false;
}

bool CAlgo::detectPupilByModelForPreview(const cv::Mat& img,
                                         enSingleDualEyeMode eyeMode,
                                         stPupilInfo& pupilRight,
                                         stPupilInfo& pupilLeft)
{
    const auto formalWorkIsActive = [this]() {
        return m_formalHybridTasksInFlight.load(std::memory_order_acquire) > 0
                || m_formalHybridPool.activeThreadCount() > 0
                || m_formalAsyncTasksInFlight.load(std::memory_order_acquire) > 0
                || (m_formalModelAttempted.load(std::memory_order_acquire)
                    && !m_formalModelFinished.load(std::memory_order_acquire));
    };
    if (formalWorkIsActive()) {
        if (!m_previewModelSuppressionLogged.exchange(
                    true, std::memory_order_acq_rel)) {
            qDebug() << "PreviewPupilC800: suppressed during formal round";
        }
        return false;
    }

    // 正式深度学习版本无需外部开关；模型不可用时自动保留传统算法结果。
    if (isSimulatedEye) {
        return false;
    }
    if (!ensurePupilModelLoaded()) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(m_pupilModelMutex);
        if (m_lastPreviewModelAttempt.time_since_epoch().count() != 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - m_lastPreviewModelAttempt).count();
            if (elapsed < PREVIEW_MODEL_MIN_INTERVAL_MS) {
                return false;
            }
        }
        m_lastPreviewModelAttempt = now;
    }

    PupilPairResult result;
    std::string error;
    {
        ScopedOpenCvThreadCount threadScope(PUPIL_MODEL_CPU_THREADS);
        // 预览请求可能在队列中等待；拿到共享CPU资源后必须再检查一次，
        // 防止正式测量已开始但旧预览请求继续执行C800推理。
        if (formalWorkIsActive()) {
            if (!m_previewModelSuppressionLogged.exchange(
                        true, std::memory_order_acq_rel)) {
                qDebug()
                        << "PreviewPupilC800: suppressed after waiting for formal CPU";
            }
            return false;
        }
        // C800统一承担预览兜底。第二张输入用同尺寸、同类型全黑图占位，
        // 只读取frames[0]，frames[1]即使产生输出也完全丢弃。
        const cv::Mat blackCompanion = cv::Mat::zeros(img.size(), img.type());
        if (!inferPupilPairModel(
                    img, blackCompanion, &result, 0.0f, 8, &error)) {
            ALGO_ERROR_LOG(
                qCritical().noquote()
                    << QString("[DL_ERROR] stage=model_infer,reason=%1")
                       .arg(QString::fromStdString(error))
            );
            return false;
        }
    }

    const auto eyeFlags = get_eye_flags(eyeMode);
    const PupilPairFrameResult& previewResult = result.frames[0];
    const bool rightOk = !eyeFlags.first
            || pairModelEyeResultIsUsable(
                    previewResult.subjectRight, whichEye_Right);
    const bool leftOk = !eyeFlags.second
            || pairModelEyeResultIsUsable(
                    previewResult.subjectLeft, whichEye_Left);
    if (!rightOk || !leftOk) {
        return false;
    }

    if (eyeFlags.first && eyeFlags.second) {
        const float xDiff = previewResult.subjectLeft.center.x
                - previewResult.subjectRight.center.x;
        const float yDiff = std::abs(previewResult.subjectRight.center.y
                                     - previewResult.subjectLeft.center.y);
        if (xDiff < MIN_PREVIEW_EYE_X_DIFF
                || xDiff > MAX_PREVIEW_EYE_X_DIFF
                || yDiff > MAX_PREVIEW_EYE_Y_DIFF) {
            return false;
        }
    }

    if (eyeFlags.first) {
        pupilRight = makePairModelPupilInfo(
                previewResult.subjectRight, PupilFallback_DeepModel);
    }
    if (eyeFlags.second) {
        pupilLeft = makePairModelPupilInfo(
                previewResult.subjectLeft, PupilFallback_DeepModel);
    }
    qDebug().noquote()
            << QString("PreviewPupilC800: black-pair fallback success, "
                       "forward=%1 ms")
               .arg(result.forwardMs, 0, 'f', 1);
    return true;
}

bool CAlgo::shouldUseFormalHybrid() const
{
    const auto eyeFlags = get_eye_flags(m_eye);
    // 正式阶段的定位口径固定为深度学习路径。模型文件缺失或加载失败时，
    // 仍进入该路径并将本轮快速标记为无效，绝不悄悄退回Haar全轮计算。
    // 模拟眼保留原有独立测试行为，不属于受检者正式测量流程。
    return !isSimulatedEye
            // 单眼和双眼共用正式深度学习主流程；跟踪器会按目标眼裁剪。
            && (eyeFlags.first || eyeFlags.second);
}

bool CAlgo::isFormalAsyncPupilMode() const
{
    // 该查询与正式入口使用同一开关，避免界面层和算法层采用不同口径。
    return FORMAL_PER_FRAME_ASYNC_ENABLED && shouldUseFormalHybrid();
}

void CAlgo::clearFormalCrossRoundState()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_formalCrossRoundState) {
        m_formalCrossRoundState->clear();
    }
}

void CAlgo::clearFormalMasterAnchorLocked(const char* reason)
{
    m_formalMasterAnchor.reset();
    m_formalMasterAnchorReady = false;
    m_formalMasterAnchorGeneration = 0;
    m_formalMasterAnchorMeasurementGeneration = 0;
    m_formalMasterAnchorRound = -1;
    m_formalMasterAnchorImgNo = 0;
#if ENABLE_ALGO_VERBOSE_LOG
    if (reason != nullptr) {
        qInfo().noquote()
                << QString("[DL_MASTER_ANCHOR] action=cleared reason=%1")
                   .arg(reason);
    }
#else
    Q_UNUSED(reason);
#endif
}

bool CAlgo::buildRefinedCrossRoundFramesLocked(
        int roundIdx,
        std::vector<PupilLightFrame>& refinedFrames) const
{
    refinedFrames.clear();
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return false;
    }

    const auto eyeFlags = get_eye_flags(m_eye);
    const MeasurementRound& round = m_rounds[roundIdx];
    refinedFrames.resize(FRAMES_PER_ROUND);
    bool complete = true;

    const auto fillEye = [&](int imgNo,
                             enWhichEye whichEye,
                             bool required,
                             const std::bitset<FRAME_ARRAY_SIZE>& validFlags,
                             const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& infos,
                             PupilLightEye& target) {
        if (!required) {
            return;
        }

        const stPupilInfo& info = infos[imgNo];
        const bool valid = round.frames[imgNo].processed
                && validFlags.test(imgNo)
                && std::isfinite(info.center.x)
                && std::isfinite(info.center.y)
                && std::isfinite(info.radius)
                && info.radius >= 6.0
                && info.radius <= 64.0
                && isNormalPupil(info.center, whichEye);
        if (!valid) {
            complete = false;
            return;
        }

        target.detected = true;
        target.reliable = true;
        target.center = info.center;
        target.radius = static_cast<float>(info.radius);
        target.score = 1.0F;
        // 该轨迹来自MeasurementRound最终结果，不再携带模型/插值来源。
        target.source = PupilSource_RoiRefined;
    };

    for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
        PupilLightFrame& frame = refinedFrames[imgNo - 1];
        frame.lampNumber = imgNo;
        fillEye(imgNo, whichEye_Right, eyeFlags.first,
                round.validRight, round.pupilInfoRight, frame.subjectRight);
        fillEye(imgNo, whichEye_Left, eyeFlags.second,
                round.validLeft, round.pupilInfoLeft, frame.subjectLeft);
        frame.isSubjectRightAnchor = !eyeFlags.first
                || frame.subjectRight.detected;
        frame.isSubjectLeftAnchor = !eyeFlags.second
                || frame.subjectLeft.detected;
        frame.isAnchor = frame.isSubjectRightAnchor
                && frame.isSubjectLeftAnchor;
    }
    return complete;
}

bool CAlgo::hasCompleteFormalCrossRoundSourceLocked() const
{
    if (!m_formalCrossRoundState
            || !m_formalCrossRoundState->ready
            || m_formalCrossRoundState->sourceImages.size()
                != FRAMES_PER_ROUND
            || m_formalCrossRoundState->sourceSmallImages.size()
                != FRAMES_PER_ROUND
            || m_formalCrossRoundState->sourceFrames.size()
                != FRAMES_PER_ROUND) {
        return false;
    }

    // 跨轮源必须在每个灯位同时具备当前测量要求的可靠眼；
    // 只有“ready”但缺少一只必需眼时，不能作为正式处理锚点。
    const auto eyeFlags = get_eye_flags(m_eye);
    for (const PupilLightFrame& frame : m_formalCrossRoundState->sourceFrames) {
        if (eyeFlags.first
                && (!frame.subjectRight.detected
                    || !frame.subjectRight.reliable)) {
            return false;
        }
        if (eyeFlags.second
                && (!frame.subjectLeft.detected
                    || !frame.subjectLeft.reliable)) {
            return false;
        }
    }
    return true;
}

void CAlgo::stageCrossRoundSourceCandidate(
        int roundIdx,
        const std::vector<cv::Mat>& images)
{
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return;
    }

    FormalCrossRoundSourceCandidate candidate;
    candidate.roundIndex = roundIdx;
    // cv::Mat采用引用计数，这里只暂存图像句柄，不复制像素。
    candidate.images = images;
    bool coordinatesComplete = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_formalCrossRoundState) {
            return;
        }
        candidate.coordinatesComplete =
                buildRefinedCrossRoundFramesLocked(
                        roundIdx, candidate.refinedFrames);
        coordinatesComplete = candidate.coordinatesComplete;
        // pendingCandidates与ready/sourceImages/sourceFrames使用同一把
        // m_mutex保护，避免结算线程和下一轮照片调度线程并发操作容器。
        m_formalCrossRoundState->pendingCandidates[roundIdx] =
                std::move(candidate);
    }
#if ENABLE_ALGO_VERBOSE_LOG
    qDebug().noquote()
            << QString("PupilCrossRoundSource: action=staged, round=%1, "
                       "coordinates_complete=%2")
               .arg(roundIdx)
               .arg(coordinatesComplete ? "yes" : "no");
#endif
}

void CAlgo::commitOrDiscardCrossRoundSourceCandidates()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_formalCrossRoundState) {
        return;
    }

    for (auto it = m_formalCrossRoundState->pendingCandidates.begin();
         it != m_formalCrossRoundState->pendingCandidates.end();) {
        const int roundIdx = it->first;
        const MeasurementRound& round = m_rounds[roundIdx];
        const bool accepted = round.result.valid;
        const bool rejected = round.rejected;

        if (!accepted && !rejected) {
            ++it;
            continue;
        }

        FormalCrossRoundSourceCandidate& candidate = it->second;
        if (accepted && candidate.coordinatesComplete) {
            const int previousRound =
                    m_formalCrossRoundState->sourceRoundIndex;
            m_formalCrossRoundState->ready = true;
            m_formalCrossRoundState->sourceRoundIndex = roundIdx;
            m_formalCrossRoundState->sourceImages = candidate.images;
            m_formalCrossRoundState->sourceSmallImages.clear();
            m_formalCrossRoundState->sourceSmallImages.reserve(
                    candidate.images.size());
            for (const cv::Mat& sourceImage : candidate.images) {
                m_formalCrossRoundState->sourceSmallImages.push_back(
                        makeFormalSmallGray(sourceImage));
            }
            m_formalCrossRoundState->sourceFrames = candidate.refinedFrames;
            const std::uint64_t sourceGeneration =
                    m_formalCrossRoundSourceGeneration.fetch_add(
                            1, std::memory_order_acq_rel) + 1;
            // 源坐标已更换为最终精修坐标，旧模板不得继续复用。
            m_formalCrossRoundState->sparseTemplateCache.clear();
            m_formalCrossRoundState->fullTemplateCache.clear();
#if ENABLE_ALGO_VERBOSE_LOG
            qDebug().noquote()
                    << QString("PupilCrossRoundSource: action=committed, "
                               "round=%1, previous_source_round=%2, "
                               "coordinate_basis=roi_refined, "
                               "template_cache_cleared=yes,source_generation=%3")
                       .arg(roundIdx)
                       .arg(previousRound)
                       .arg(static_cast<qulonglong>(sourceGeneration));
#endif
        } else if (rejected) {
            if (roundIdx == 0) {
                // 首轮被拒绝时，下一轮必须从本轮模型锚点重新开始。
                m_formalCrossRoundState->ready = false;
                m_formalCrossRoundState->sourceRoundIndex = -1;
                m_formalCrossRoundState->sourceImages.clear();
                m_formalCrossRoundState->sourceSmallImages.clear();
                m_formalCrossRoundState->sourceFrames.clear();
                m_formalCrossRoundState->sparseTemplateCache.clear();
                m_formalCrossRoundState->fullTemplateCache.clear();
            }
            qWarning().noquote()
                    << QString("PupilCrossRoundSource: action=discarded, "
                               "round=%1, reason=round_rejected")
                       .arg(roundIdx);
        } else {
            // accepted但坐标不完整：本轮结果可以保留，但不能污染跨轮模板。
            qWarning().noquote()
                    << QString("PupilCrossRoundSource: action=discarded, "
                               "round=%1, reason=coordinates_incomplete")
                       .arg(roundIdx);
        }
        it = m_formalCrossRoundState->pendingCandidates.erase(it);
    }
}

void CAlgo::setWHRatio(float _modeleye_wh_ratio,float _humaneye_wh_ratio) {
    if(_modeleye_wh_ratio>0){
        modeleye_wh_ratio=_modeleye_wh_ratio;
    }
    if(humaneye_wh_ratio>0){
        humaneye_wh_ratio=_humaneye_wh_ratio;
    }
}

stAlgoCommandResult CAlgo::handleAlgoCommand(
        const stAlgoCommand &command)
{
    switch (command.type) {
    case enAlgoCommandType::SetTurnLampSaveContext:
        setTurnLampSaveContext(command.patientImageDir,
                               command.batchDir,
                               command.sourceDir,
                               command.usbRoot);
        return stAlgoCommandResult::makeSuccess();

    case enAlgoCommandType::CancelMeasurementRuntime:
        resetMeasurementRuntime();
        return stAlgoCommandResult::makeSuccess();

    case enAlgoCommandType::FinishFormalRoundInput:
        if (command.roundIdx < 0) {
            return stAlgoCommandResult::makeFailure(
                    "invalid formal round index");
        }
        finishFormalRoundInput(command.roundIdx);
        return stAlgoCommandResult::makeSuccess();

    case enAlgoCommandType::QueryFormalAsyncPupilMode:
        return stAlgoCommandResult::makeSuccess(
                isFormalAsyncPupilMode());
    }

    return stAlgoCommandResult::makeFailure(
            "unsupported algorithm command");
}

void CAlgo::setTurnLampSaveContext(const std::string &_patient_img_dir,
                                   const std::string &_batch_dir,
                                   const std::string &_source_dir,
                                   const std::string &_usb_root)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_turnLampSavePatientDir = _patient_img_dir;
    m_turnLampSaveBatchDir = _batch_dir;
    m_turnLampSaveSourceDir = _source_dir;
    m_turnLampSaveUsbRoot = _usb_root;
}

/*================  1. 测量开始：清场 + 缓存参数  =================*/
bool CAlgo::calcVisionBegin(enAgeRange _age,
                           const std::string _subDir,
                           enSingleDualEyeMode _eye,int roundNo)
{
    qDebug()<<__PRETTY_FUNCTION__<<QString("roundNo = %1").arg(roundNo);

    // roundNo index安全检查
    if (roundNo < 0) {
        qDebug()<<"Round index 不能小于0!";
        return false;
    }

    if (roundNo >= MAX_ROUNDS) {
        qDebug()<<"Round index exceeds MAX_ROUNDS!";
        return false;
    }

    if (roundNo == 0) {
        // 新测量代际号先递增，再清理旧轮次；旧全局线程池任务即使稍后
        // 执行，也会因代际不一致而禁止写入新测量。
        m_formalMeasurementGeneration.fetch_add(1, std::memory_order_acq_rel);
        m_formalModelAttempted.store(false, std::memory_order_release);
        m_formalModelFinished.store(false, std::memory_order_release);
        // 新测量开始前必须先使上一测量的正式任务失效。
        // 仅waitForDone()会让“尚未开始的旧任务”逐个跑完，造成新受检者
        // 已开始转灯、后台却还在处理上一人的图像。先clear()丢弃排队任务，
        // 再等待当前正在运行的任务观察到停止标志并退出。
        m_formalStreamingStop.store(true, std::memory_order_release);
        m_formalFrameCondition.notify_all();
        const int pendingTaskCount = m_formalHybridTasksInFlight.load(
                std::memory_order_acquire);
        if (pendingTaskCount > 0 || m_formalHybridPool.activeThreadCount() > 0) {
            qWarning() << "PupilHybridQueue: cancel previous measurement before reset"
                       << "pending=" << pendingTaskCount
                       << "active=" << m_formalHybridPool.activeThreadCount();
        }
        m_formalHybridPool.clear();
        m_formalHybridPool.waitForDone();
        // waitForDone()返回后不存在运行/排队任务；此处归零可防止旧版队列
        // 被clear()时留下的计数影响预览模型的占用判断。
        m_formalHybridTasksInFlight.store(0, std::memory_order_release);
        // 新逐照片任务使用全局线程池，不能通过clear全局线程池清除。
        // 代际失效后等待本对象的任务全部退出，再重置轮次和新受检者参数，
        // 防止旧任务在129 ROI/传统兜底期间读取或写入新测量状态。
        {
            std::unique_lock<std::mutex> asyncLock(m_formalAsyncWaitMutex);
            m_formalAsyncWaitCondition.wait(asyncLock, [this]() {
                return m_formalAsyncTasksInFlight.load(
                        std::memory_order_acquire) == 0;
            });
        }
        clearFormalCrossRoundState();
        m_previewModelSuppressionLogged.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> cropLock(m_pupilCropCacheMutex);
            // 新受检者开始前丢弃上一位受检者的原图和结果可读标志。
            m_pupilCropCache.clear();
            m_pupilCropCache.measurementGeneration =
                    m_formalMeasurementGeneration.load(
                            std::memory_order_acquire);
            // 即使第一张正式照片完全缺失，结果页也必须得到固定的目标编号。
            m_pupilCropCache.roundIdx = 0;
            m_pupilCropCache.imgNo = 1;
        }

        // 正式深度学习版本在单眼/双眼真人测量时自动加载模型，无需板端开关。
        // 模型缺失或加载失败不会阻断测量，正式路径会把对应照片记为缺失。
        bool hybridReady = false;
        const auto requestedEyeFlags = get_eye_flags(_eye);
        if ((requestedEyeFlags.first || requestedEyeFlags.second)
                && !isSimulatedEye) {
            hybridReady = ensurePupilModelLoaded();
        }
        qDebug().noquote()
                << QString("PupilHybrid: formal deep-learning path %1")
                   .arg(hybridReady ? "ready"
                                    : "unavailable, formal frames will be marked missing");
    }

#if ENABLE_ALGO_TIMING_LOG
    // 第0轮代表一次新测量；重置总计时，并为当前轮建立独立统计上下文。
    if (roundNo == 0) {
        AlgoTiming::beginMeasurement();
        m_timingFormalFrameCount.store(0, std::memory_order_relaxed);
        m_timingRoundCount.store(0, std::memory_order_relaxed);
    }
    AlgoTiming::beginRound(roundNo);
    int currentTimingRoundCount = m_timingRoundCount.load(std::memory_order_relaxed);
    while (currentTimingRoundCount < roundNo + 1
           && !m_timingRoundCount.compare_exchange_weak(
               currentTimingRoundCount, roundNo + 1,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
#endif

    std::lock_guard<std::mutex> settlementLock(m_settlementMutex);
    std::lock_guard<std::mutex> lock(m_mutex);
    // 全新会话：重置全部状态
    if (roundNo == 0) {
        // 【新增】全新会话时，确保所有轮次都回到初始状态
        for (int i = 0; i < MAX_ROUNDS; ++i) {
            resetRoundState(i);  // 复用你现有的清理逻辑
        }
        clearDsPatchState();
        m_activeFormalProcessingRound = -1;
        m_activeFormalRounds.clear();
        m_formalInterleaveCursor = 0;
        // 上一测量的异步任务已在前面等待清零，此处开始新会话计数。
        m_formalPhotoTasksInFlight = 0;
        m_pendingFormalC800Starts.clear();
        m_formalC800TaskRunning = false;
        m_formalC800RunningRound = -1;
        m_formalSettlementTaskRunning = false;
        clearFormalMasterAnchorLocked();
        m_hasEmittedFinalResult = false;
        m_age = _age;
        m_subDir = _subDir;
        m_eye = _eye;
    }
    // 如果算法已成功，不再接受新轮（可选：返回 false 或警告）
    if (m_hasEmittedFinalResult) {
        // 可选：重置以支持重新测量
        // 但根据需求，这里假设一次测量只出一个结果
        return false;
    }

    resultState = calcResultState_Succ;
    resetRoundState(roundNo);
    m_rounds[roundNo].savePatientDir = m_turnLampSavePatientDir;
    m_rounds[roundNo].saveBatchDir = m_turnLampSaveBatchDir;
    m_rounds[roundNo].saveSourceDir = m_turnLampSaveSourceDir;
    m_rounds[roundNo].saveUsbRoot = m_turnLampSaveUsbRoot;

    if (roundNo == 0) {
        // 轮次和新受检者参数已经在锁内完成重置，最后才开放新一代任务。
        m_formalStreamingStop.store(false, std::memory_order_release);
    }

    qDebug()<<"m_hasEmittedFinalResult:"<<m_hasEmittedFinalResult<<"resultState"<<resultState<<endl;
    return true;
}

void CAlgo::resetRoundState(int roundNo)
{
    auto& r = m_rounds[roundNo];

    // 先清理旧数据
    r.clear();  // 调用你添加的 clear() 方法

    // 再重置索引和其他状态
    r.roundIndex = roundNo;
    r.result = RoundResult{};
    r.pupilRadiusAvgRight = r.pupilRadiusAvgLeft = -1;
    r.pupilSpotAvgRight = r.pupilSpotAvgLeft = CPointF{0.0, 0.0};
    m_roundDs[roundNo] = RoundDs{};
}

/* 3. 设置回调（主线程里用 lambda 捕获界面指针即可） */
void CAlgo::setVisionResultCallback(VisionCallback cb)
{
    m_visionCb = std::move(cb);
}

cv::Mat CAlgo::cropPupilOutputImage(const cv::Mat& source,
                                    const cv::Point2f& center) const
{
    if (source.empty()
            || !std::isfinite(center.x)
            || !std::isfinite(center.y)) {
        return cv::Mat();
    }

    const int centerX = cvRound(center.x);
    const int centerY = cvRound(center.y);
    const cv::Rect requestedRegion(
            centerX - PUPIL_OUTPUT_SOURCE_WIDTH / 2,
            centerY - PUPIL_OUTPUT_SOURCE_HEIGHT / 2,
            PUPIL_OUTPUT_SOURCE_WIDTH,
            PUPIL_OUTPUT_SOURCE_HEIGHT);
    const cv::Rect imageBounds(0, 0, source.cols, source.rows);
    const cv::Rect sourceRegion = requestedRegion & imageBounds;

    // 裁剪区域与原图完全不相交，说明瞳孔中心坐标无效。
    if (sourceRegion.empty()) {
        return cv::Mat();
    }

    // 只有部分区域越界时，先在较小原图区域内使用黑色填充。
    cv::Mat sourceCrop(PUPIL_OUTPUT_SOURCE_HEIGHT,
                       PUPIL_OUTPUT_SOURCE_WIDTH,
                       source.type(),
                       cv::Scalar::all(0));
    const cv::Rect outputRegion(
            sourceRegion.x - requestedRegion.x,
            sourceRegion.y - requestedRegion.y,
            sourceRegion.width,
            sourceRegion.height);
    source(sourceRegion).copyTo(sourceCrop(outputRegion));

    // 最终接口尺寸仍固定为140×225，仅改变画面内容的放大倍率。
    cv::Mat output;
    cv::resize(sourceCrop, output,
               cv::Size(PUPIL_OUTPUT_CROP_WIDTH,
                        PUPIL_OUTPUT_CROP_HEIGHT),
               0.0, 0.0, cv::INTER_CUBIC);
    return output;
}

void CAlgo::cacheFirstFramePupilCropSource(
        int roundIdx,
        int imgNo,
        const cv::Mat& source,
        bool rightValid,
        const stPupilInfo& rightPupil,
        bool leftValid,
        const stPupilInfo& leftPupil,
        std::uint64_t measurementGeneration,
        std::uint64_t roundGeneration)
{
    if (roundIdx != 0 || imgNo != 1 || source.empty()
            || measurementGeneration == 0) {
        return;
    }

    if (m_formalMeasurementGeneration.load(std::memory_order_acquire)
            != measurementGeneration) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_pupilCropCacheMutex);
        if (m_pupilCropCache.captured) {
            return;
        }

        // 只缓存原图引用和129 ROI结果；cv::Mat赋值不会复制整张原图。
        m_pupilCropCache.captured = true;
        m_pupilCropCache.resultReady = false;
        m_pupilCropCache.measurementGeneration = measurementGeneration;
        m_pupilCropCache.roundIdx = roundIdx;
        m_pupilCropCache.imgNo = imgNo;
        m_pupilCropCache.sourceImage = source;
        m_pupilCropCache.rightValid = rightValid
                && std::isfinite(rightPupil.center.x)
                && std::isfinite(rightPupil.center.y);
        m_pupilCropCache.leftValid = leftValid
                && std::isfinite(leftPupil.center.x)
                && std::isfinite(leftPupil.center.y);
        m_pupilCropCache.rightCenter = rightPupil.center;
        m_pupilCropCache.leftCenter = leftPupil.center;
    }
    Q_UNUSED(roundGeneration);
}

void CAlgo::markPupilCropResultReady()
{
    std::lock_guard<std::mutex> lock(m_pupilCropCacheMutex);
    // 最终失败也必须开放读取，让结果页能够立即得到双眼无效状态。
    m_pupilCropCache.resultReady = true;
}

bool CAlgo::getPupilCropResult(stPupilCropResult& result)
{
    PupilCropCache cache;
    {
        std::lock_guard<std::mutex> lock(m_pupilCropCacheMutex);
        if (!m_pupilCropCache.resultReady
                || m_pupilCropCache.measurementGeneration == 0) {
            return false;
        }
        // 新测量开始时会先清空缓存；这里复制当前已完成结果的快照。
        cache = m_pupilCropCache;
    }

    result = stPupilCropResult{};
    result.measurementGeneration = cache.measurementGeneration;
    result.roundIdx = cache.roundIdx;
    result.imgNo = cache.imgNo;
    result.cropWidth = PUPIL_OUTPUT_CROP_WIDTH;
    result.cropHeight = PUPIL_OUTPUT_CROP_HEIGHT;
    result.rightCenter = cache.rightCenter;
    result.leftCenter = cache.leftCenter;

    // 裁剪和缩放全部在缓存锁之外执行，避免阻塞算法状态和结果页其他访问。
    if (cache.captured && cache.rightValid) {
        result.rightImage = cropPupilOutputImage(
                cache.sourceImage, cache.rightCenter);
        result.rightValid = !result.rightImage.empty();
    }
    if (cache.captured && cache.leftValid) {
        result.leftImage = cropPupilOutputImage(
                cache.sourceImage, cache.leftCenter);
        result.leftValid = !result.leftImage.empty();
    }

    // 裁剪期间可能已经开始新测量并清空了缓存；此时不能把旧受检者的
    // 快照继续返回给结果页。重新加锁确认结果仍属于同一完成代际。
    {
        std::lock_guard<std::mutex> lock(m_pupilCropCacheMutex);
        if (!m_pupilCropCache.resultReady
                || m_pupilCropCache.measurementGeneration
                        != cache.measurementGeneration) {
            result = stPupilCropResult{};
            return false;
        }
    }
    return true;
}

void CAlgo::resetMeasurementRuntime()
{
    // UI返回预览、用户取消或一次测量结束时会调用。立即丢弃尚未开始的
    // 正式任务；正在运行的任务会在各阶段检查m_formalStreamingStop后退出。
    // 不在这里等待，避免阻塞UI；下一次calcVisionBegin(round=0)会最终等待清场。
    const bool finalResultWasEmitted =
            m_hasEmittedFinalResult.load(std::memory_order_acquire);
    m_formalStreamingStop.store(true, std::memory_order_release);
    // 取消也必须切换代际，防止已经离开线程池队列的旧任务回写轮次。
    m_formalMeasurementGeneration.fetch_add(1, std::memory_order_acq_rel);
    m_formalFrameCondition.notify_all();
    m_formalHybridPool.clear();
    m_previewModelSuppressionLogged.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeFormalProcessingRound = -1;
        m_activeFormalRounds.clear();
        m_formalInterleaveCursor = 0;
        m_pendingFormalC800Starts.clear();
        m_formalC800TaskRunning = false;
        m_formalC800RunningRound = -1;
        m_formalSettlementTaskRunning = false;
        clearFormalMasterAnchorLocked("measurement_cancelled");
    }
    if (!finalResultWasEmitted) {
        std::lock_guard<std::mutex> cropLock(m_pupilCropCacheMutex);
        // 中途取消时禁止结果页读取未完成测量的原图。
        m_pupilCropCache.clear();
    }
    int expected = 0;
    if (m_hasEmittedFinalResult.compare_exchange_strong(
                expected, 1, std::memory_order_acq_rel,
                std::memory_order_acquire)) {
        ALGO_KEY_LOG(qInfo().noquote() << "[DL_RESULT] state=cancelled");
    }
}

// 编译单元内显式声明一次，避免增量构建未刷新头文件依赖时找不到日志限流接口。
void setPupilFailDetailRoundLogLimiter(std::atomic<int>* counter, int maxLogsPerRound);

namespace {
static const int DS_ITEM_COUNT = 10;
static const int MAX_MISSING_DS_PER_EYE = 3;
static const int MAX_PUPIL_FAIL_DETAIL_LOGS_PER_ROUND = 2;

class PupilFailDetailRoundLogScope {
public:
    explicit PupilFailDetailRoundLogScope(std::atomic<int>* counter)
    {
        setPupilFailDetailRoundLogLimiter(counter, MAX_PUPIL_FAIL_DETAIL_LOGS_PER_ROUND);
    }

    ~PupilFailDetailRoundLogScope()
    {
        setPupilFailDetailRoundLogLimiter(nullptr, 0);
    }
};

// 正式混合任务从入队到退出期间持有一个计数。
// 最后一项任务结束后允许预览模型再次兜底，支持测量中途重新对准。
class FormalHybridTaskScope {
public:
    FormalHybridTaskScope(std::atomic<int>* taskCounter,
                          std::atomic<bool>* suppressionLogFlag)
        : m_taskCounter(taskCounter),
          m_suppressionLogFlag(suppressionLogFlag)
    {
    }

    ~FormalHybridTaskScope()
    {
        int remainingTaskCount = 0;
        if (m_taskCounter) {
            remainingTaskCount = m_taskCounter->fetch_sub(
                    1, std::memory_order_acq_rel) - 1;
        }
        // 仅最后一个正式任务离开时解除预览模型的抑制提示。这个对象通过
        // shared_ptr捕获在QRunnable中，因此队列clear()删除未执行任务时
        // 也会调用析构并正确回收计数。
        if (m_suppressionLogFlag && remainingTaskCount <= 0) {
            m_suppressionLogFlag->store(false, std::memory_order_release);
        }
    }

private:
    std::atomic<int>* m_taskCounter = nullptr;
    std::atomic<bool>* m_suppressionLogFlag = nullptr;
};

// 逐照片异步任务的轻量计数器。计数覆盖“已排队但尚未执行”的窗口，
// 便于预览模型避让正式任务，也便于日志确认没有隐藏的无限等待。
class FormalAsyncTaskScope {
public:
    FormalAsyncTaskScope(std::atomic<int>* taskCounter,
                         std::condition_variable* waitCondition = nullptr)
        : m_taskCounter(taskCounter),
          m_waitCondition(waitCondition)
    {
    }

    ~FormalAsyncTaskScope()
    {
        if (m_taskCounter) {
            m_taskCounter->fetch_sub(1, std::memory_order_acq_rel);
        }
        if (m_waitCondition) {
            m_waitCondition->notify_all();
        }
    }

private:
    std::atomic<int>* m_taskCounter = nullptr;
    std::condition_variable* m_waitCondition = nullptr;
};

const std::array<DsPairConfig, 9>& rightDsPairConfigs()
{
    static const std::array<DsPairConfig, 9> configs = {{
        {6, 5, -1.0}, {4, 3, -1.0}, {2, 1, -1.0},
        {12, 11, -1.0}, {10, 9, -1.0}, {8, 7, -1.0},
        {17, 18, 1.0}, {15, 16, 1.0}, {13, 14, 1.0}
    }};
    return configs;
}

const std::array<DsPairConfig, 9>& leftDsPairConfigs()
{
    static const std::array<DsPairConfig, 9> configs = {{
        {5, 6, 1.0}, {3, 4, 1.0}, {1, 2, 1.0},
        {11, 12, 1.0}, {9, 10, 1.0}, {7, 8, 1.0},
        {18, 17, -1.0}, {16, 15, -1.0}, {14, 13, -1.0}
    }};
    return configs;
}

QString dsEyeName(bool rightEye)
{
    return rightEye ? "right" : "left";
}

}

static QString eyeModeName(enSingleDualEyeMode mode)
{
    if (mode == singleDualEyeMode_Right) {
        return "right";
    }
    if (mode == singleDualEyeMode_Left) {
        return "left";
    }
    return "both";
}

void CAlgo::logRoundDiagnosisLocked(int roundIdx, const QString &trigger)
{
#if !ENABLE_ALGO_ROUND_DIAG_LOG
    // 诊断日志关闭时直接返回，避免额外统计和字符串拼接影响转灯计算。
    Q_UNUSED(roundIdx);
    Q_UNUSED(trigger);
    return;
#else
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return;
    }

    auto& round = m_rounds[roundIdx];
    if (round.diagnosisLogged) {
        return;
    }
    round.diagnosisLogged = true;

    int frameCount = 0;
    int processedCount = 0;
    int pupilDetectedCount = 0;
    int rightValidCount = 0;
    int leftValidCount = 0;
    QStringList rightMissing;
    QStringList leftMissing;
    QStringList failures;
    for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
        if (round.frames[i].hasFrame) {
            ++frameCount;
        }
        if (round.frames[i].processed) {
            ++processedCount;
        }
        if (round.frames[i].pupilDetected) {
            ++pupilDetectedCount;
        }
        if (round.validRight.test(i)) {
            ++rightValidCount;
        } else {
            rightMissing << QString::number(i);
        }
        if (round.validLeft.test(i)) {
            ++leftValidCount;
        } else {
            leftMissing << QString::number(i);
        }
        if (!round.frames[i].failureReason.isEmpty()) {
            failures << QString("%1:%2").arg(i).arg(round.frames[i].failureReason);
        }
    }
    const bool fullyValid = round.isFullyValid(m_eye);
    const bool needRight = (m_eye == singleDualEyeMode_Both || m_eye == singleDualEyeMode_Right);
    const bool needLeft = (m_eye == singleDualEyeMode_Both || m_eye == singleDualEyeMode_Left);
    const QString rightValidText = needRight
            ? QString("%1/%2").arg(rightValidCount).arg(FRAMES_PER_ROUND)
            : "n/a";
    const QString leftValidText = needLeft
            ? QString("%1/%2").arg(leftValidCount).arg(FRAMES_PER_ROUND)
            : "n/a";
    const QString rightMissingText = needRight ? rightMissing.join(",") : "n/a";
    const QString leftMissingText = needLeft ? leftMissing.join(",") : "n/a";

    // 轮次诊断日志默认关闭，避免连续转灯结算时串口输出过多。
    qDebug().noquote()
        << QString("TurnLampRoundDiagnosis: round=%1 trigger=%2 eye_mode=%3 "
                   "frames_received=%4 processed=%5 pupil_detected=%6 "
                   "right_valid=%7 left_valid=%8 fully_valid=%9 "
                   "right_missing=[%10] left_missing=[%11] failures=[%12]")
           .arg(roundIdx)
           .arg(trigger)
           .arg(eyeModeName(m_eye))
           .arg(frameCount)
           .arg(processedCount)
           .arg(pupilDetectedCount)
           .arg(rightValidText)
           .arg(leftValidText)
           .arg(fullyValid ? "true" : "false")
           .arg(rightMissingText)
           .arg(leftMissingText)
           .arg(failures.join(","));
#endif
}

void CAlgo::clearDsPatchState()
{
    for (int i = 0; i < MAX_ROUNDS; ++i) {
        m_roundDs[i] = RoundDs{};
    }
    m_pendingRoundIndices.clear();
}

static bool hasValidRoiImage(const std::array<cv::Mat, FRAME_ARRAY_SIZE>& images, int idx)
{
    return idx >= 1 && idx <= FRAMES_PER_ROUND && !images[idx].empty();
}

void CAlgo::calculateDsPairsForEyeLocked(const std::array<cv::Mat, FRAME_ARRAY_SIZE>& roiImages,
                                         const std::bitset<FRAME_ARRAY_SIZE>& validFrames,
                                         const std::array<DsPairConfig, 9>& configs,
                                         bool isRightEye,
                                         std::array<DsItem, 10>& items) const
{
    for (int i = 0; i < 9; ++i) {
        const DsPairConfig& config = configs[i];
        if (!validFrames.test(config.idx1) || !validFrames.test(config.idx2)
                || !hasValidRoiImage(roiImages, config.idx1)
                || !hasValidRoiImage(roiImages, config.idx2)) {
            continue;
        }

        cv::Mat img1 = roiImages[config.idx1];
        cv::Mat img2 = roiImages[config.idx2];
        items[i].value = config.sign * compareEach(img1, img2, DEFAULT_PARAMS);
        items[i].nativeValid = true;
        items[i].finalValid = true;
    }

    const int s0 = isRightEye ? 19 : 20;
    const int s1 = isRightEye ? 20 : 19;
    const int s2 = isRightEye ? 22 : 21;
    const int s3 = isRightEye ? 21 : 22;
    if (validFrames.test(s0) && validFrames.test(s1)
            && validFrames.test(s2) && validFrames.test(s3)
            && hasValidRoiImage(roiImages, s0)
            && hasValidRoiImage(roiImages, s1)
            && hasValidRoiImage(roiImages, s2)
            && hasValidRoiImage(roiImages, s3)) {
        cv::Mat img1 = roiImages[s0];
        cv::Mat img2 = roiImages[s1];
        cv::Mat img3 = roiImages[s2];
        cv::Mat img4 = roiImages[s3];
        const double slope1 = compareEach(img1, img2, DEFAULT_PARAMS);
        const double slope2 = compareEach(img3, img4, DEFAULT_PARAMS);
        items[9].value = isRightEye ? (slope1 - slope2) / 2.0
                                    : (-slope1 + slope2) / 2.0;
        items[9].nativeValid = true;
        items[9].finalValid = true;
    }
}

void CAlgo::applyLowConfidenceDsQualityGateLocked(
        int roundIdx,
        bool isRightEye,
        const std::bitset<FRAME_ARRAY_SIZE>& lowConfidenceFrames,
        std::array<DsItem, 10>& items) const
{
    // 这不是“低分就剔除”的门槛。只有同一屈光方向内至少存在两条
    // 高置信DS作参照，且低置信照片生成的DS残差明显异常时才剔除该DS条目。
    // 参照不足时宁可保留，交给原有跨轮补值和最终屈光结算继续判断。
    if (lowConfidenceFrames.none()) {
        return;
    }

    const std::array<DsPairConfig, 9>& configs =
            isRightEye ? rightDsPairConfigs() : leftDsPairConfigs();
    std::array<bool, DS_ITEM_COUNT> lowConfidenceItems = {};
    for (int i = 0; i < 9; ++i) {
        lowConfidenceItems[i] = lowConfidenceFrames.test(configs[i].idx1)
                || lowConfidenceFrames.test(configs[i].idx2);
    }

    const int s0 = isRightEye ? 19 : 20;
    const int s1 = isRightEye ? 20 : 19;
    const int s2 = isRightEye ? 22 : 21;
    const int s3 = isRightEye ? 21 : 22;
    lowConfidenceItems[9] = lowConfidenceFrames.test(s0)
            || lowConfidenceFrames.test(s1)
            || lowConfidenceFrames.test(s2)
            || lowConfidenceFrames.test(s3);

    bool hasCandidate = false;
    double dsValues[DS_ITEM_COUNT] = {0.0};
    for (int i = 0; i < DS_ITEM_COUNT; ++i) {
        if (!items[i].nativeValid) {
            // 本来就缺失的DS仍走既有补值流程，不能在这里伪造质量结论。
            return;
        }
        hasCandidate = hasCandidate || lowConfidenceItems[i];
        dsValues[i] = items[i].value;
    }
    if (!hasCandidate) {
        return;
    }

    RefractionFitDiagnostics diagnostics;
    calRefraction(m_age, isHmMode, dsValues, &diagnostics);
    static const int kDirectionItems[3][4] = {
        {0, 1, 2, 9}, {3, 4, 5, 9}, {6, 7, 8, 9}
    };

    std::array<bool, DS_ITEM_COUNT> rejected = {};
    for (int direction = 0; direction < 3; ++direction) {
        std::vector<double> trustedResiduals;
        for (int channel = 0; channel < 4; ++channel) {
            const int itemIdx = kDirectionItems[direction][channel];
            if (!lowConfidenceItems[itemIdx]) {
                trustedResiduals.push_back(
                        std::abs(diagnostics.signedResidual[direction][channel]));
            }
        }
        if (trustedResiduals.size() < 2U) {
            continue;
        }

        std::sort(trustedResiduals.begin(), trustedResiduals.end());
        const double median = trustedResiduals[trustedResiduals.size() / 2U];
        std::vector<double> deviations;
        deviations.reserve(trustedResiduals.size());
        for (const double value : trustedResiduals) {
            deviations.push_back(std::abs(value - median));
        }
        std::sort(deviations.begin(), deviations.end());
        const double mad = deviations[deviations.size() / 2U];

        // 阈值刻意偏宽：0.45是绝对地板，MAD仅在高置信DS自身存在波动时
        // 进一步放宽。这样只会拦截明显错误的ROI，而不是遮挡造成的轻微差异。
        const double allowedResidual = std::max(0.45, median + 4.0 * std::max(0.08, mad));
        for (int channel = 0; channel < 4; ++channel) {
            const int itemIdx = kDirectionItems[direction][channel];
            if (!lowConfidenceItems[itemIdx] || rejected[itemIdx]) {
                continue;
            }
            const double residual =
                    std::abs(diagnostics.signedResidual[direction][channel]);
            if (residual <= allowedResidual) {
                continue;
            }

            // finalValid=false后，该条DS会按原有规则尝试由后续轮补值；
            // nativeValid保持真实“本轮已计算”语义，避免改写历史原始数据。
            items[itemIdx].finalValid = false;
            rejected[itemIdx] = true;
            qWarning().noquote()
                    << QString("DsLowConfidenceOutlier: round=%1, eye=%2, "
                               "item=%3, residual=%4, limit=%5")
                       .arg(roundIdx)
                       .arg(isRightEye ? "right" : "left")
                       .arg(itemIdx)
                       .arg(residual, 0, 'f', 3)
                       .arg(allowedResidual, 0, 'f', 3);
        }
    }
}

void CAlgo::applyRiskyDsQualityGateLocked(
        int roundIdx,
        bool isRightEye,
        const std::bitset<FRAME_ARRAY_SIZE>& lowConfidenceFrames,
        const std::bitset<FRAME_ARRAY_SIZE>& outOfRangeFrames,
        std::array<DsItem, 10>& items) const
{
    // 先执行原有低置信一致性复核，再对距离超限照片参与的DS做同轮风险
    // 复核。超限图片仍然完整进入定位和DS候选，不在append阶段丢弃。
    applyLowConfidenceDsQualityGateLocked(
            roundIdx, isRightEye, lowConfidenceFrames, items);
    if (outOfRangeFrames.none()) {
        return;
    }

    const std::array<DsPairConfig, 9>& configs =
            isRightEye ? rightDsPairConfigs() : leftDsPairConfigs();
    std::array<bool, DS_ITEM_COUNT> affected = {};
    for (int i = 0; i < 9; ++i) {
        affected[i] = outOfRangeFrames.test(configs[i].idx1)
                || outOfRangeFrames.test(configs[i].idx2);
        items[i].rangeAffected = affected[i];
    }
    const int s0 = isRightEye ? 19 : 20;
    const int s1 = isRightEye ? 20 : 19;
    const int s2 = isRightEye ? 22 : 21;
    const int s3 = isRightEye ? 21 : 22;
    affected[9] = outOfRangeFrames.test(s0) || outOfRangeFrames.test(s1)
            || outOfRangeFrames.test(s2) || outOfRangeFrames.test(s3);
    items[9].rangeAffected = affected[9];

    bool anyAffected = false;
    bool allNative = true;
    double dsValues[DS_ITEM_COUNT] = {0.0};
    for (int i = 0; i < DS_ITEM_COUNT; ++i) {
        anyAffected = anyAffected || affected[i];
        allNative = allNative && items[i].nativeValid;
        dsValues[i] = items[i].value;
    }
    if (!anyAffected) {
        return;
    }

    // 没有完整的同轮DS参照时，无法证明超限照片没有污染当前项，
    // 让既有的最近轮patch机制接管该项，避免静默采纳风险数据。
    if (!allNative) {
        for (int i = 0; i < DS_ITEM_COUNT; ++i) {
            if (!affected[i] || !items[i].nativeValid) {
                continue;
            }
            items[i].finalValid = false;
            qWarning().noquote()
                    << QString("DsPatch: round=%1,eye=%2,item=%3,"
                               "reason=range_quality_rejected,reference=missing")
                       .arg(roundIdx)
                       .arg(isRightEye ? "right" : "left")
                       .arg(i);
        }
        return;
    }

    RefractionFitDiagnostics diagnostics;
    calRefraction(m_age, isHmMode, dsValues, &diagnostics);
    static const int kDirectionItems[3][4] = {
        {0, 1, 2, 9}, {3, 4, 5, 9}, {6, 7, 8, 9}
    };
    for (int direction = 0; direction < 3; ++direction) {
        std::vector<double> trustedResiduals;
        for (int channel = 0; channel < 4; ++channel) {
            const int itemIdx = kDirectionItems[direction][channel];
            if (!affected[itemIdx]) {
                trustedResiduals.push_back(
                        std::abs(diagnostics.signedResidual[direction][channel]));
            }
        }
        if (trustedResiduals.size() < 2U) {
            for (int channel = 0; channel < 4; ++channel) {
                const int itemIdx = kDirectionItems[direction][channel];
                if (affected[itemIdx]) {
                    items[itemIdx].finalValid = false;
                    qWarning().noquote()
                            << QString("DsPatch: round=%1,eye=%2,item=%3,"
                                       "reason=range_quality_rejected,"
                                       "reference=insufficient")
                               .arg(roundIdx)
                               .arg(isRightEye ? "right" : "left")
                               .arg(itemIdx);
                }
            }
            continue;
        }

        std::sort(trustedResiduals.begin(), trustedResiduals.end());
        const double median = trustedResiduals[trustedResiduals.size() / 2U];
        std::vector<double> deviations;
        for (double value : trustedResiduals) {
            deviations.push_back(std::abs(value - median));
        }
        std::sort(deviations.begin(), deviations.end());
        const double mad = deviations[deviations.size() / 2U];
        const double allowedResidual =
                std::max(0.60, median + 4.0 * std::max(0.08, mad));
        for (int channel = 0; channel < 4; ++channel) {
            const int itemIdx = kDirectionItems[direction][channel];
            if (!affected[itemIdx]) {
                continue;
            }
            const double residual =
                    std::abs(diagnostics.signedResidual[direction][channel]);
            if (residual <= allowedResidual) {
                continue;
            }
            items[itemIdx].finalValid = false;
            qWarning().noquote()
                    << QString("DsPatch: round=%1,eye=%2,item=%3,"
                               "reason=range_quality_rejected,residual=%4,limit=%5")
                       .arg(roundIdx)
                       .arg(isRightEye ? "right" : "left")
                       .arg(itemIdx)
                       .arg(residual, 0, 'f', 3)
                       .arg(allowedResidual, 0, 'f', 3);
        }
    }
}

int CAlgo::missingDsCount(const std::array<DsItem, 10>& items) const
{
    int missing = 0;
    for (int i = 0; i < DS_ITEM_COUNT; ++i) {
        if (!items[i].finalValid) {
            ++missing;
        }
    }
    return missing;
}

QString CAlgo::missingDsText(const std::array<DsItem, 10>& items) const
{
    QStringList missing;
    for (int i = 0; i < DS_ITEM_COUNT; ++i) {
        if (!items[i].finalValid) {
            missing << QString::number(i);
        }
    }
    return missing.join(",");
}

void CAlgo::writeRoundAlgoStatusLocked(int roundIdx,
                                       const QString &algoState,
                                       const QString &reason,
                                       bool isFinalResultRound,
                                       bool policyFinished,
                                       bool policyQuestionable) const
{
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return;
    }
    const auto& round = m_rounds[roundIdx];
    const std::string& patientDir = round.savePatientDir.empty() ? m_turnLampSavePatientDir : round.savePatientDir;
    const std::string& batchDir = round.saveBatchDir.empty() ? m_turnLampSaveBatchDir : round.saveBatchDir;
    const std::string& sourceDir = round.saveSourceDir.empty() ? m_turnLampSaveSourceDir : round.saveSourceDir;
    const std::string& usbRoot = round.saveUsbRoot.empty() ? m_turnLampSaveUsbRoot : round.saveUsbRoot;
    if (patientDir.empty()
            || batchDir.empty()
            || sourceDir.empty()
            || usbRoot.empty()) {
        return;
    }

    const auto& ds = m_roundDs[roundIdx];
    auto eyeFlags = get_eye_flags(m_eye);

    int frameCount = 0;
    int processedCount = 0;
    int pupilDetectedCount = 0;
    int rightValidCount = 0;
    int leftValidCount = 0;
    QStringList rightMissingFrames;
    QStringList leftMissingFrames;
    QStringList failures;
    for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
        if (round.frames[i].hasFrame) {
            ++frameCount;
        }
        if (round.frames[i].processed) {
            ++processedCount;
        }
        if (round.frames[i].pupilDetected) {
            ++pupilDetectedCount;
        }
        if (round.validRight.test(i)) {
            ++rightValidCount;
        } else {
            rightMissingFrames << QString::number(i);
        }
        if (round.validLeft.test(i)) {
            ++leftValidCount;
        } else {
            leftMissingFrames << QString::number(i);
        }
        if (!round.frames[i].failureReason.isEmpty()) {
            failures << QString("%1:%2").arg(i).arg(round.frames[i].failureReason);
        }
    }

    QStringList rightPatched;
    QStringList leftPatched;
    for (int i = 0; i < DS_ITEM_COUNT; ++i) {
        if (eyeFlags.first && ds.right[i].finalValid && !ds.right[i].nativeValid) {
            rightPatched << QString("%1<-round%2").arg(i).arg(ds.right[i].patchedFromRound);
        }
        if (eyeFlags.second && ds.left[i].finalValid && !ds.left[i].nativeValid) {
            leftPatched << QString("%1<-round%2").arg(i).arg(ds.left[i].patchedFromRound);
        }
    }

    QString content;
    QTextStream out(&content);
    out << "round_index=" << roundIdx << "\n";
    out << "algo_state=" << algoState << "\n";
    out << "reason=" << reason << "\n";
    out << "eye_mode=" << eyeModeName(m_eye) << "\n";
    out << "frames_received=" << frameCount << "\n";
    out << "processed=" << processedCount << "\n";
    out << "out_of_range_frame_count=" << round.outOfRangeFrameCount << "\n";
    out << "out_of_range_frames="
        << QString::fromStdString(round.outOfRangeFrames.to_string()) << "\n";
    out << "pupil_detected=" << pupilDetectedCount << "\n";
    out << "right_valid=" << (eyeFlags.first ? QString("%1/%2").arg(rightValidCount).arg(FRAMES_PER_ROUND) : QString("n/a")) << "\n";
    out << "left_valid=" << (eyeFlags.second ? QString("%1/%2").arg(leftValidCount).arg(FRAMES_PER_ROUND) : QString("n/a")) << "\n";
    out << "right_missing_frames=" << (eyeFlags.first ? rightMissingFrames.join(",") : QString("n/a")) << "\n";
    out << "left_missing_frames=" << (eyeFlags.second ? leftMissingFrames.join(",") : QString("n/a")) << "\n";
    out << "formal_anchor_right_state="
        << formalAnchorStateName(round.asyncState.rightAnchor.state) << "\n";
    out << "formal_anchor_left_state="
        << formalAnchorStateName(round.asyncState.leftAnchor.state) << "\n";
    out << "formal_anchor_right_failures="
        << round.asyncState.rightAnchor.consecutiveFailureCount << "\n";
    out << "formal_anchor_left_failures="
        << round.asyncState.leftAnchor.consecutiveFailureCount << "\n";
    out << "ds_generated=" << (ds.generated ? "true" : "false") << "\n";
    out << "ds_pending=" << (ds.pending ? "true" : "false") << "\n";
    out << "ds_submitted=" << (ds.submitted ? "true" : "false") << "\n";
    out << "ds_rejected=" << (ds.rejected ? "true" : "false") << "\n";
    out << "traditional_pair_fallback_calls="
        << round.traditionalPairFallbackCalls << "\n";
    out << "traditional_pair_fallback_elapsed_ms="
        << round.traditionalPairFallbackElapsedMs << "\n";
    out << "right_ds_missing=" << (eyeFlags.first ? missingDsText(ds.right) : QString("n/a")) << "\n";
    out << "left_ds_missing=" << (eyeFlags.second ? missingDsText(ds.left) : QString("n/a")) << "\n";
    out << "right_ds_patched=" << (eyeFlags.first ? rightPatched.join(",") : QString("n/a")) << "\n";
    out << "left_ds_patched=" << (eyeFlags.second ? leftPatched.join(",") : QString("n/a")) << "\n";
    out << "round_result_valid=" << (round.result.valid ? "true" : "false") << "\n";
    out << "is_final_result_round=" << (isFinalResultRound ? "true" : "false") << "\n";
    out << "policy_finished=" << (policyFinished ? "true" : "false") << "\n";
    out << "policy_questionable=" << (policyQuestionable ? "true" : "false") << "\n";
    out << "failures=" << failures.join(",") << "\n";
    out.flush();

    writeAlgoStatusAsync(QString::fromStdString(usbRoot),
                         QString::fromStdString(patientDir),
                         QString::fromStdString(batchDir),
                         QString::fromStdString(sourceDir),
                         roundIdx,
                         content);
}

bool CAlgo::buildRoundDsLocked(int roundIdx)
{
#if ENABLE_ALGO_TIMING_LOG
    AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
    ALGO_TIMING_SCOPE(AlgoTimingStage_DsBuild);
#endif
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return false;
    }

    auto& round = m_rounds[roundIdx];
    RoundDs ds;

    double rAvg = 0.0;
    double lAvg = 0.0;
    calculatePupilAverageRadius(m_eye,
                                round.pupilInfoRight, round.validRight, rAvg,
                                round.pupilInfoLeft, round.validLeft, lAvg);
    {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_ProcessPupilRoi);
#endif
        processPupilROI(m_eye,
                        round.pupilImgRight, rAvg, round.pupilROIImgRight,
                        round.pupilImgLeft, lAvg, round.pupilROIImgLeft);
    }

    auto eyeFlags = get_eye_flags(m_eye);
    if (eyeFlags.first) {
        calculateDsPairsForEyeLocked(round.pupilROIImgRight, round.validRight,
                                     rightDsPairConfigs(), true, ds.right);
#if ENABLE_ALGO_ROUND_DIAG_LOG
        qDebug().noquote() << QString("DsPartial: round=%1 eye=right native_valid=%2/10 missing=[%3]")
                              .arg(roundIdx)
                              .arg(DS_ITEM_COUNT - missingDsCount(ds.right))
                              .arg(missingDsText(ds.right));
#endif
    }
    if (eyeFlags.second) {
        calculateDsPairsForEyeLocked(round.pupilROIImgLeft, round.validLeft,
                                     leftDsPairConfigs(), false, ds.left);
#if ENABLE_ALGO_ROUND_DIAG_LOG
        qDebug().noquote() << QString("DsPartial: round=%1 eye=left native_valid=%2/10 missing=[%3]")
                              .arg(roundIdx)
                              .arg(DS_ITEM_COUNT - missingDsCount(ds.left))
                              .arg(missingDsText(ds.left));
#endif
    }

    // 低置信定位不再在“找瞳孔”阶段被直接拒绝；在DS真正计算完成后，
    // 仅对依赖低置信照片的DS条目进行同轮一致性复核。
    if (eyeFlags.first) {
        applyRiskyDsQualityGateLocked(
                roundIdx, true, round.lowConfidenceRight,
                round.outOfRangeFrames, ds.right);
    }
    if (eyeFlags.second) {
        applyRiskyDsQualityGateLocked(
                roundIdx, false, round.lowConfidenceLeft,
                round.outOfRangeFrames, ds.left);
    }

    ds.generated = true;
    m_roundDs[roundIdx] = ds;
    return true;
}

bool CAlgo::hasFuturePatchCandidate(int targetRoundIdx) const
{
    const int roundLimit = MAX_ROUNDS;
    for (int candidate = targetRoundIdx + 1; candidate < roundLimit; ++candidate) {
        if (!m_roundDs[candidate].generated && !m_roundDs[candidate].rejected) {
            return true;
        }
    }
    return false;
}

bool CAlgo::patchDsItemsFromNearestRound(std::array<DsItem, 10>& targetItems,
                                         int targetRoundIdx,
                                         bool rightEye)
{
    bool patched = false;
    for (int i = 0; i < DS_ITEM_COUNT; ++i) {
        if (targetItems[i].finalValid) {
            continue;
        }

        int bestRoundIdx = -1;
        const int roundLimit = MAX_ROUNDS;
        int bestDistance = roundLimit + 1;
        double bestValue = 0.0;

            // 全局扫描已生成轮；源轮不要求22张完整，但低置信一致性门已判为
            // 离群的DS条目不能再被借去修补其他轮。
        for (int sourceRoundIdx = 0; sourceRoundIdx < roundLimit; ++sourceRoundIdx) {
            if (sourceRoundIdx == targetRoundIdx) {
                continue;
            }

            const RoundDs& source = m_roundDs[sourceRoundIdx];
            if (!source.generated || source.rejected) {
                continue;
            }

            const std::array<DsItem, 10>& sourceItems = rightEye ? source.right : source.left;
            // 仅允许源轮本地原始且最终有效的同眼同项；任何补值结果
            // 即使被误标为finalValid，也不得再次作为跨轮源传播。
            if (!sourceItems[i].nativeValid || !sourceItems[i].finalValid
                    || sourceItems[i].patchedFromRound >= 0) {
                continue;
            }

            const int distance = std::abs(sourceRoundIdx - targetRoundIdx);
            const bool preferCurrent = distance < bestDistance
                    || (distance == bestDistance && sourceRoundIdx < targetRoundIdx);
            if (preferCurrent) {
                bestDistance = distance;
                bestRoundIdx = sourceRoundIdx;
                bestValue = sourceItems[i].value;
            }
        }

        if (bestRoundIdx >= 0) {
            targetItems[i].value = bestValue;
            targetItems[i].finalValid = true;
            targetItems[i].patchedFromRound = bestRoundIdx;
            patched = true;
#if ENABLE_ALGO_ROUND_DIAG_LOG
            qDebug().noquote() << QString("DsPatch: target_round=%1 eye=%2 ds=%3 from_round=%4")
                                  .arg(targetRoundIdx)
                                  .arg(dsEyeName(rightEye))
                                  .arg(i)
                                  .arg(bestRoundIdx);
#endif
        }
    }
    return patched;
}

bool CAlgo::tryPatchRoundDsLocked(int targetRoundIdx)
{
#if ENABLE_ALGO_TIMING_LOG
    AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, targetRoundIdx);
    ALGO_TIMING_SCOPE(AlgoTimingStage_DsPatch);
#endif
    RoundDs& target = m_roundDs[targetRoundIdx];
    if (!target.generated || target.rejected || target.submitted) {
        return false;
    }

    auto eyeFlags = get_eye_flags(m_eye);
    bool patched = false;
    if (eyeFlags.first) {
        patched |= patchDsItemsFromNearestRound(target.right, targetRoundIdx, true);
    }
    if (eyeFlags.second) {
        patched |= patchDsItemsFromNearestRound(target.left, targetRoundIdx, false);
    }
    return patched;
}

bool CAlgo::calculateRoundResultFromDsLocked(int roundIdx)
{
#if ENABLE_ALGO_TIMING_LOG
    AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
#endif
    RoundDs& ds = m_roundDs[roundIdx];
    auto eyeFlags = get_eye_flags(m_eye);

    double DSR[10] = {0.0};
    double DSL[10] = {0.0};
    if (eyeFlags.first) {
        if (missingDsCount(ds.right) > 0) {
            return false;
        }
        for (int i = 0; i < DS_ITEM_COUNT; ++i) {
            DSR[i] = ds.right[i].value;
        }
    }
    if (eyeFlags.second) {
        if (missingDsCount(ds.left) > 0) {
            return false;
        }
        for (int i = 0; i < DS_ITEM_COUNT; ++i) {
            DSL[i] = ds.left[i].value;
        }
    }

    auto& round = m_rounds[roundIdx];
    enCalcResultState outState = calculateRefraction(
            m_age, m_eye,
            round.pupilInfoRight, round.validRight,
            round.pupilInfoLeft, round.validLeft,
            DSR, DSL,
            round.result.vision, round.result.abnormal,
            false);

#if ENABLE_ALGO_ROUND_DIAG_LOG
    qDebug() << QString("roundIdx = %1").arg(roundIdx) << round.result.vision.toString();
#endif

    if (calcResultState_Succ == outState) {
        round.result.valid = true;
        ds.submitted = true;
        writeRoundAlgoStatusLocked(roundIdx, "accepted", "round_result_valid");
        return true;
    }
#if ENABLE_ALGO_ROUND_DIAG_LOG
    qDebug() << QString("DsRoundRejected: round=%1 reason=calculate_refraction_failed state=%2")
                .arg(roundIdx)
                .arg(outState);
#endif
    ds.rejected = true;
    round.rejected = true;
    writeRoundAlgoStatusLocked(roundIdx, "rejected", "calculate_refraction_failed");
    return false;
}

bool CAlgo::processRoundSettlementLocked(int roundIdx)
{
#if ENABLE_ALGO_TIMING_LOG
    AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
    ALGO_TIMING_SCOPE(AlgoTimingStage_RoundSettlement);
#endif
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return false;
    }
    if (m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
        return false;
    }

    RoundDs& ds = m_roundDs[roundIdx];
    auto& round = m_rounds[roundIdx];
    if (ds.submitted || ds.rejected || round.rejected) {
        return false;
    }
    if (!ds.generated && !buildRoundDsLocked(roundIdx)) {
        return false;
    }

    auto eyeFlags = get_eye_flags(m_eye);
    const int rightMissingNative = eyeFlags.first ? missingDsCount(ds.right) : 0;
    const int leftMissingNative = eyeFlags.second ? missingDsCount(ds.left) : 0;
    if (rightMissingNative > MAX_MISSING_DS_PER_EYE
            || leftMissingNative > MAX_MISSING_DS_PER_EYE) {
        ds.rejected = true;
        ds.pending = false;
        round.rejected = true;
        m_pendingRoundIndices.erase(std::remove(m_pendingRoundIndices.begin(),
                                                m_pendingRoundIndices.end(),
                                                roundIdx),
                                    m_pendingRoundIndices.end());
#if ENABLE_ALGO_ROUND_DIAG_LOG
        qDebug().noquote() << QString("DsRoundRejected: round=%1 reason=too_many_missing right_missing=%2 left_missing=%3")
                              .arg(roundIdx)
                              .arg(rightMissingNative)
                              .arg(leftMissingNative);
#endif
        writeRoundAlgoStatusLocked(roundIdx, "rejected", "too_many_missing");
        return false;
    }

    if (rightMissingNative > 0 || leftMissingNative > 0) {
        tryPatchRoundDsLocked(roundIdx);
    }

    const int rightMissingFinal = eyeFlags.first ? missingDsCount(ds.right) : 0;
    const int leftMissingFinal = eyeFlags.second ? missingDsCount(ds.left) : 0;
    if (rightMissingFinal == 0 && leftMissingFinal == 0) {
        const bool calculated = calculateRoundResultFromDsLocked(roundIdx);
        if (calculated) {
            ds.pending = false;
            m_pendingRoundIndices.erase(std::remove(m_pendingRoundIndices.begin(),
                                                    m_pendingRoundIndices.end(),
                                                    roundIdx),
                                        m_pendingRoundIndices.end());
        }
        return calculated;
    }

    if (hasFuturePatchCandidate(roundIdx)) {
        ds.pending = true;
        if (std::find(m_pendingRoundIndices.begin(), m_pendingRoundIndices.end(), roundIdx)
                == m_pendingRoundIndices.end()) {
            m_pendingRoundIndices.push_back(roundIdx);
        }
#if ENABLE_ALGO_ROUND_DIAG_LOG
        qDebug().noquote() << QString("DsRoundPending: round=%1 right_missing=[%2] left_missing=[%3]")
                              .arg(roundIdx)
                              .arg(eyeFlags.first ? missingDsText(ds.right) : QString("n/a"))
                              .arg(eyeFlags.second ? missingDsText(ds.left) : QString("n/a"));
#endif
        writeRoundAlgoStatusLocked(roundIdx, "pending", "waiting_for_future_patch_candidate");
        return false;
    }

    ds.rejected = true;
    ds.pending = false;
    round.rejected = true;
    m_pendingRoundIndices.erase(std::remove(m_pendingRoundIndices.begin(),
                                            m_pendingRoundIndices.end(),
                                            roundIdx),
                                m_pendingRoundIndices.end());
#if ENABLE_ALGO_ROUND_DIAG_LOG
    qDebug().noquote() << QString("DsRoundRejected: round=%1 reason=missing_not_patchable right_missing=[%2] left_missing=[%3]")
                          .arg(roundIdx)
                          .arg(eyeFlags.first ? missingDsText(ds.right) : QString("n/a"))
                          .arg(eyeFlags.second ? missingDsText(ds.left) : QString("n/a"));
#endif
    writeRoundAlgoStatusLocked(roundIdx, "rejected", "missing_not_patchable");
    return false;
}

void CAlgo::emitCurrentResultsLocked(bool forceFinished)
{
    if (m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
        return;
    }
    if (!m_visionCb) {
        return;
    }

    std::vector<stVisionValue> visions;
    std::vector<stVisionAbnormal> abnormals;
    for (int i = 0; i < MAX_ROUNDS; ++i) {
        if (m_rounds[i].result.valid) {
            visions.push_back(m_rounds[i].result.vision);
            abnormals.push_back(m_rounds[i].result.abnormal);
        }
    }
    if (visions.empty()) {
        return;
    }

    stVisionValue finalVision{};
    stVisionAbnormal finalAbnormal{};
    bool questionable = true;
    bool finished = false;
    {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_Policy);
#endif
        finished = RefractionStrategy::executeAlgoPolicy(
            visions, finalVision, finalAbnormal, questionable);
    }

    if (forceFinished && !finished) {
        // 达到最后物理轮且策略仍未完成时，已有有效轮只能作为可疑结果结束，
        // 不能继续让界面等待下一轮转灯。
        finished = true;
        questionable = true;
        qWarning() << "FormalAsyncFinalization: policy incomplete, "
                      "force questionable result";
    }

#if ENABLE_REFRACTION_POLICY_VERBOSE_LOG
#if ENABLE_ALGO_VERBOSE_LOG
    qDebug() << "RefractionStrategy::executeAlgoPolicy start---------------" << endl;
    qDebug() << "for start+++++++++++++++++" << endl;
    for (stVisionValue v : visions) {
        qDebug() << v.toString() << endl;
    }
    qDebug() << "for end+++++++++++++++++" << endl;
    qDebug() << finalVision.toString() << endl;
    qDebug() << "finished=" << finished << endl;
    qDebug() << "RefractionStrategy::executeAlgoPolicy end------------------";
#endif
#endif

    for (int i = 0; i < MAX_ROUNDS; ++i) {
        if (m_rounds[i].result.valid) {
            writeRoundAlgoStatusLocked(i,
                                       finished ? "final_success" : "accepted",
                                       finished ? "policy_finished" : "policy_waiting_more_rounds",
                                       finished,
                                       finished,
                                       questionable);
        }
    }

    if (finished) {
        int expected = 0;
        if (!m_hasEmittedFinalResult.compare_exchange_strong(expected, 1,
                                                            std::memory_order_acq_rel,
                                                            std::memory_order_acquire)) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_activeFormalProcessingRound = -1;
            m_activeFormalRounds.clear();
            m_pendingFormalC800Starts.clear();
            m_formalSettlementTaskRunning = false;
            clearFormalMasterAnchorLocked("measurement_finished");
        }
        ALGO_KEY_LOG(
            qInfo().noquote()
                << QString("[DL_RESULT] state=%1,rounds=%2")
                   .arg(questionable ? "questionable" : "success")
                   .arg(static_cast<int>(visions.size()))
        );
    }
#if ENABLE_ALGO_TIMING_LOG
    if (finished) {
        AlgoTiming::printMeasurementSummary(
                    m_timingRoundCount.load(std::memory_order_relaxed),
                    m_timingFormalFrameCount.load(std::memory_order_relaxed),
                    true);
    }
#endif
    if (finished) {
        // 先开放结果页读取，再调用最终结果回调，避免界面切换后仍不可读。
        markPupilCropResultReady();
    }
    // 最终结果不对应单一失败轮次，使用-1表示此字段仅供换轮失败通知使用。
    m_visionCb(calcResultState_Succ, -1, finalVision, finalAbnormal, visions, finished, questionable);
}

bool CAlgo::emitFormalPupilNotFoundOnce(
        int roundIdx, const QString& reason)
{
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS || !m_visionCb) {
        return false;
    }

    int expected = 0;
    if (!m_hasEmittedFinalResult.compare_exchange_strong(
                expected, 1, std::memory_order_acq_rel,
                std::memory_order_acquire)) {
        return false;
    }

    m_formalStreamingStop.store(true, std::memory_order_release);
    m_formalModelFinished.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // 先停止正式异步链路，再清理本轮任务状态；回调在解锁后执行。
        FormalAsyncRoundState& asyncState = m_rounds[roundIdx].asyncState;
        asyncState.modelFinished = true;
        asyncState.anchorTaskRunning = false;
        asyncState.anchorReady = false;
        writeRoundAlgoStatusLocked(roundIdx,
                                   QStringLiteral("final_failure"), reason,
                                   true, true, true);
        m_activeFormalProcessingRound = -1;
        m_activeFormalRounds.clear();
        m_pendingFormalC800Starts.clear();
        m_formalSettlementTaskRunning = false;
        clearFormalMasterAnchorLocked("measurement_finished");
    }

    ALGO_KEY_LOG(
        qInfo().noquote()
            << QString("[DL_RESULT] state=pupil_not_found,reason=%1,round=%2")
                   .arg(reason)
                   .arg(roundIdx));

    bool finished = true;
    bool questionable = true;
    markPupilCropResultReady();
    m_visionCb(calcResultState_PupilNotFound,
               roundIdx,
               stVisionValue{}, stVisionAbnormal{},
               std::vector<stVisionValue>{}, finished, questionable);
    return true;
}

void CAlgo::tryFinalizeRoundLocked(int roundIdx)
{
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return;
    }
    if (m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard<std::mutex> settlementLock(m_settlementMutex);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& round = m_rounds[roundIdx];
        if (round.isProcessing || round.result.valid || round.rejected) {
            return;
        }

        int processedCount = 0;
        int frameCount = 0;
        for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
            if (round.frames[i].hasFrame) {
                ++frameCount;
            }
            if (round.frames[i].processed) {
                ++processedCount;
            }
        }
        if (FORMAL_PER_FRAME_ASYNC_ENABLED && shouldUseFormalHybrid()
                && !round.asyncState.inputFinished) {
            return;
        }
        // 正常情况要求22张都收到；输入侧明确结束后，缺失灯位已由
        // finishFormalRoundInput标记为formal_frame_missing，可直接进入一次
        // 结算，不再依赖超时等待。
        if ((!round.asyncState.inputFinished
             && frameCount < FRAMES_PER_ROUND)
                || processedCount < FRAMES_PER_ROUND) {
            return;
        }

        round.isProcessing = true;
        round.diagnosisLogged = false;
#if ENABLE_ALGO_ROUND_DIAG_LOG
        logRoundDiagnosisLocked(roundIdx, "round_complete_processed");
#endif
    }

    bool hasNewResult = processRoundSettlementLocked(roundIdx);
    std::vector<int> pending = m_pendingRoundIndices;
    std::sort(pending.begin(), pending.end());
    for (int pendingRoundIdx : pending) {
        if (pendingRoundIdx == roundIdx) {
            continue;
        }
        hasNewResult |= processRoundSettlementLocked(pendingRoundIdx);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rounds[roundIdx].isProcessing = false;
    }
    // 本轮结算已经给出accepted/rejected/pending状态后，才处理跨轮源候选。
    // pending候选会保留到后续轮次再次触发结算，不能提前作为source使用。
    commitOrDiscardCrossRoundSourceCandidates();
    if (hasNewResult) {
#if ENABLE_ALGO_TIMING_LOG
        AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
#endif
        emitCurrentResultsLocked();
    }

    bool finalizationAtLastRound = false;
    int validRoundCount = 0;
    // 正式异步流程不再使用固定的早期物理轮强制封口。
    // 正常情况下由2个稳定有效轮或3个有效轮提前结束。
    // 只有达到MAX_ROUNDS安全上限时，才使用已有有效结果
    // 输出可疑结果，或者在完全没有有效结果时输出PupilNotFound。
    const bool lastPhysicalRound = roundIdx == MAX_ROUNDS - 1;
    if (lastPhysicalRound
            && !m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = 0; i <= roundIdx; ++i) {
            if (m_rounds[i].result.valid) {
                ++validRoundCount;
            }
        }
        // 最后物理轮结算后统一封口；历史pending轮不能继续把界面拖到超时。
        finalizationAtLastRound = true;
    }
    if (finalizationAtLastRound) {
        if (validRoundCount > 0) {
            // 最后一轮仍未达到正常策略门槛时，使用已有有效轮输出唯一的可疑结果。
            emitCurrentResultsLocked(true);
        } else {
            emitFormalPupilNotFoundOnce(
                    roundIdx,
                    QStringLiteral("last_physical_round_no_valid_result"));
        }
    }
#if ENABLE_ALGO_TIMING_LOG
    // 只在轮次真正接受或拒绝后打印；pending 轮可能被后续轮补齐，不能提前封口。
    std::vector<std::pair<int, const char*> > terminalRounds;
    const auto appendTerminalRound = [this, &terminalRounds](int candidateRoundIdx) {
        if (candidateRoundIdx < 0 || candidateRoundIdx >= MAX_ROUNDS) {
            return;
        }
        const MeasurementRound& candidateRound = m_rounds[candidateRoundIdx];
        if (candidateRound.result.valid) {
            terminalRounds.push_back(std::make_pair(candidateRoundIdx, "accepted"));
        } else if (candidateRound.rejected) {
            terminalRounds.push_back(std::make_pair(candidateRoundIdx, "rejected"));
        }
    };
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        appendTerminalRound(roundIdx);
        for (int pendingRoundIdx : pending) {
            if (pendingRoundIdx != roundIdx) {
                appendTerminalRound(pendingRoundIdx);
            }
        }
    }
    for (const auto& terminalRound : terminalRounds) {
        AlgoTiming::printRoundSummary(terminalRound.first, terminalRound.second);
    }
#endif
}

void CAlgo::processAndStoreLocatedFrame(int roundIdx,
                                        int imgNo,
                                        const cv::Mat& image,
                                        bool rightLocated,
                                        stPupilInfo pupilRight,
                                        bool leftLocated,
                                        stPupilInfo pupilLeft,
                                        std::uint64_t measurementGeneration,
                                        std::uint64_t roundGeneration,
                                        bool allowTraditionalFallback,
                                        bool onlyProcessMissingEyes)
{
    const auto generationIsCurrent = [this, roundIdx, measurementGeneration,
                                      roundGeneration]() {
        if (measurementGeneration == 0 || roundGeneration == 0) {
            return true;
        }
        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_formalMeasurementGeneration.load(
                        std::memory_order_acquire) != measurementGeneration) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
            return false;
        }
        const FormalAsyncRoundState& asyncState =
                m_rounds[roundIdx].asyncState;
        return asyncState.measurementGeneration == measurementGeneration
                && asyncState.roundGeneration == roundGeneration
                && !asyncState.earlyRetryRequested;
    };

    if (m_hasEmittedFinalResult.load(std::memory_order_acquire)
            || resultState != calcResultState_Succ) {
        return;
    }
    if (!generationIsCurrent()) {
        return;
    }

    // 恢复C800可能同时返回双眼，但已经通过129并写入本轮结果的眼
    // 必须保持不变；只在恢复调用中启用该门控。
    const auto eyeFlags = get_eye_flags(m_eye);
    bool rightAlreadyConfirmed = false;
    bool leftAlreadyConfirmed = false;
    if (onlyProcessMissingEyes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (roundIdx >= 0 && roundIdx < MAX_ROUNDS
                && imgNo >= 1 && imgNo <= FRAMES_PER_ROUND) {
            const MeasurementRound& round = m_rounds[roundIdx];
            rightAlreadyConfirmed = round.validRight.test(imgNo);
            leftAlreadyConfirmed = round.validLeft.test(imgNo);
        }
    }
    // 单眼测量中，对侧眼不是失败项，也不应参与耗时/失败统计。
    const bool needRight = eyeFlags.first
            && (!onlyProcessMissingEyes || !rightAlreadyConfirmed);
    const bool needLeft = eyeFlags.second
            && (!onlyProcessMissingEyes || !leftAlreadyConfirmed);
    const int angle = getImageAngle(imgNo);
    cv::Mat roiRight;
    cv::Mat roiLeft;

    PupilRoiRefineResult rightRefine;
    PupilRoiRefineResult leftRefine;
    stPupilInfo refinedRight;
    stPupilInfo refinedLeft;
    const stPupilInfo predictedRight = pupilRight;
    const stPupilInfo predictedLeft = pupilLeft;
    auto initializeDiagnostic = [](const stPupilInfo& predicted,
                                   bool located,
                                   PupilRoiRefineResult& diagnostic) {
        diagnostic.predictedCenter = predicted.center;
        diagnostic.predictedRadius = static_cast<float>(predicted.radius);
        if (!located) {
            diagnostic.rejectReason = QStringLiteral("prediction_not_detected");
        }
    };
    initializeDiagnostic(predictedRight, rightLocated, rightRefine);
    initializeDiagnostic(predictedLeft, leftLocated, leftRefine);

    bool rightRefined = needRight && rightLocated
            && refineFormalPupilPrediction(image, imgNo, whichEye_Right,
                                           pupilRight, refinedRight,
                                           rightRefine);
    bool leftRefined = needLeft && leftLocated
            && refineFormalPupilPrediction(image, imgNo, whichEye_Left,
                                           pupilLeft, refinedLeft,
                                           leftRefine);
    if (rightRefined) {
        pupilRight = refinedRight;
    }
    if (leftRefined) {
        pupilLeft = refinedLeft;
    }

    // 129 ROI失败或没有可用预测时，同一张照片只调用一次完整传统双眼路径，
    // 传统结果只补失败眼，不能覆盖已经由129 ROI确认成功的眼。
    const bool requestRightFallback = needRight && !rightRefined;
    const bool requestLeftFallback = needLeft && !leftRefined;
    if (allowTraditionalFallback
            && (requestRightFallback || requestLeftFallback)) {
        if (!generationIsCurrent()) {
            return;
        }
        TraditionalPupilPairResult pairResult;
        locatePupilPairByTraditionalEyePath(
                image, imgNo, humaneye_wh_ratio, modeleye_wh_ratio, pairResult);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_rounds[roundIdx].traditionalPairFallbackCalls += 1;
            m_rounds[roundIdx].traditionalPairFallbackElapsedMs
                    += pairResult.elapsedMs;
        }
        const QString callId = QString("round_%1_photo_%2")
                .arg(roundIdx).arg(imgNo);
        const auto applyTraditional =
                [&callId, &pairResult](bool requested,
                                       bool rightEye,
                                       bool& refined,
                                       stPupilInfo& pupil,
                                       PupilRoiRefineResult& diagnostic) {
            if (!requested) {
                return;
            }
            diagnostic.sharedFallback = true;
            diagnostic.sharedFallbackElapsedMs = pairResult.elapsedMs;
            diagnostic.sharedFallbackCallId = callId;
            const bool valid = rightEye ? pairResult.rightValid
                                        : pairResult.leftValid;
            if (valid) {
                refined = true;
                pupil = rightEye ? pairResult.right : pairResult.left;
                diagnostic.valid = true;
                diagnostic.refinedCenter = pupil.center;
                diagnostic.refinedRadius = static_cast<float>(pupil.radius);
                diagnostic.centerShift = static_cast<float>(std::hypot(
                        static_cast<double>(pupil.center.x
                                            - diagnostic.predictedCenter.x),
                        static_cast<double>(pupil.center.y
                                            - diagnostic.predictedCenter.y)));
                diagnostic.roiSize = 0;
                diagnostic.fallbackLevel = 3;
                diagnostic.contourArea = pupil.area;
                diagnostic.contourCircularity = pupil.circularity;
                diagnostic.rejectReason.clear();
            } else {
                if (!diagnostic.rejectReason.isEmpty()) {
                    diagnostic.rejectReason += QLatin1Char(';');
                }
                diagnostic.rejectReason += QStringLiteral("traditional_failed");
            }
        };
        applyTraditional(requestRightFallback, true, rightRefined,
                         pupilRight, rightRefine);
        applyTraditional(requestLeftFallback, false, leftRefined,
                         pupilLeft, leftRefine);
        qDebug().noquote()
                << QString("PupilTraditionalPairFallback: round=%1,photo=%2,"
                           "requested_right=%3,requested_left=%4,"
                           "right_success=%5,left_success=%6,elapsed_ms=%7,"
                           "call_id=%8")
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(imgNo))
                   .arg(requestRightFallback ? "yes" : "no")
                   .arg(requestLeftFallback ? "yes" : "no")
                   .arg(pairResult.rightValid ? "yes" : "no")
                   .arg(pairResult.leftValid ? "yes" : "no")
                   .arg(pairResult.elapsedMs, 0, 'f', 3)
                   .arg(callId);
    }

    if (!allowTraditionalFallback) {
        // 正式异步路径的失败必须保持为“缺失”，不能偷偷退回整套传统找眼。
        const auto markNoFallbackFailure = [](bool located,
                                               PupilRoiRefineResult& diagnostic) {
            if (!located) {
                diagnostic.rejectReason = QStringLiteral(
                        "prediction_not_detected_no_fallback");
                return;
            }
            if (diagnostic.rejectReason.isEmpty()) {
                diagnostic.rejectReason = QStringLiteral(
                        "roi_refine_failed_no_fallback");
            } else if (!diagnostic.rejectReason.contains(
                               QStringLiteral("no_fallback"))) {
                diagnostic.rejectReason += QStringLiteral(
                        ";roi_refine_failed_no_fallback");
            }
            diagnostic.fallbackLevel = 0;
            diagnostic.sharedFallback = false;
            diagnostic.sharedFallbackCallId.clear();
        };
        if (needRight && !rightRefined) {
            markNoFallbackFailure(rightLocated, rightRefine);
        }
        if (needLeft && !leftRefined) {
            markNoFallbackFailure(leftLocated, leftRefine);
        }
    }

#if ENABLE_ALGO_TIMING_LOG
    // 129 ROI精修结果在传统兜底/无兜底分支全部结束后记录，确保每只眼使用最终结果。
    if (needRight) {
        AlgoTiming::event(rightRefined
                              ? AlgoTimingEvent_FormalRoi129Success
                              : AlgoTimingEvent_FormalRoi129Failure);
    }
    if (needLeft) {
        AlgoTiming::event(leftRefined
                             ? AlgoTimingEvent_FormalRoi129Success
                             : AlgoTimingEvent_FormalRoi129Failure);
    }
#endif

    auto logRefine = [roundIdx, imgNo](const char* eye,
                                       const stPupilInfo& predicted,
                                       const PupilRoiRefineResult& diagnostic,
                                       bool success) {
        // 正常129 ROI不逐照片刷日志；失败必须保留，详细开关开启时
        // 才恢复成功明细，便于现场诊断而不影响正常测试耗时。
#if !ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
        if (success) {
            return;
        }
#endif
        // 129 ROI失败详情与本轮其他定位失败共用两条日志预算。
        if (!success && !shouldLogPupilFailDetail()) {
            return;
        }
        qDebug().noquote()
                << QString("PupilRoiRefine: round=%1,photo=%2,eye=%3,"
                           "source=%4,predicted=(%5,%6,r=%7),roi=%8,success=%9,"
                           "refined=(%10,%11,r=%12),shift=%13,area=%14,"
                           "ratio=%15,circularity=%16,fallback=%17,elapsed_ms=%18,"
                           "shared_fallback=%19,shared_call_id=%20,reason=%21")
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(imgNo))
                   .arg(eye)
                   .arg(coordinateSourceName(predicted.coordinateSource))
                   .arg(predicted.center.x, 0, 'f', 2)
                   .arg(predicted.center.y, 0, 'f', 2)
                   .arg(predicted.radius, 0, 'f', 2)
                   .arg(diagnostic.roiSize)
                   .arg(success ? "yes" : "no")
                   .arg(diagnostic.refinedCenter.x, 0, 'f', 2)
                   .arg(diagnostic.refinedCenter.y, 0, 'f', 2)
                   .arg(diagnostic.refinedRadius, 0, 'f', 2)
                   .arg(diagnostic.centerShift, 0, 'f', 2)
                   .arg(diagnostic.contourArea, 0, 'f', 1)
                   .arg(diagnostic.contourRatio, 0, 'f', 3)
                   .arg(diagnostic.contourCircularity, 0, 'f', 3)
                   .arg(diagnostic.fallbackLevel)
                   .arg(diagnostic.elapsedMs, 0, 'f', 3)
                   .arg(diagnostic.sharedFallback ? "yes" : "no")
                   .arg(diagnostic.sharedFallbackCallId)
                   .arg(diagnostic.rejectReason);
    };
    if (needRight) {
        logRefine("right", predictedRight, rightRefine, rightRefined);
    }
    if (needLeft) {
        logRefine("left", predictedLeft, leftRefine, leftRefined);
    }
    if (!allowTraditionalFallback) {
        if (needRight && !rightRefined) {
            ALGO_ERROR_LOG(
                if (shouldLogPupilFailDetail()) {
                    qWarning().noquote()
                            << QString("FormalPupilMissing: round=%1,photo=%2,eye=right,"
                                       "reason=%3,haar_fallback=disabled")
                               .arg(roundIdx)
                               .arg(formalCaptureNumber(imgNo))
                               .arg(rightRefine.rejectReason);
                }
            );
        }
        if (needLeft && !leftRefined) {
            ALGO_ERROR_LOG(
                if (shouldLogPupilFailDetail()) {
                    qWarning().noquote()
                            << QString("FormalPupilMissing: round=%1,photo=%2,eye=left,"
                                       "reason=%3,haar_fallback=disabled")
                               .arg(roundIdx)
                               .arg(formalCaptureNumber(imgNo))
                               .arg(leftRefine.rejectReason);
                }
            );
        }
    }
#if ENABLE_ALGO_TIMING_LOG
    AlgoTiming::event(((!needRight || rightRefined)
                       && (!needLeft || leftRefined))
                      ? AlgoTimingEvent_FormalPupilSuccess
                      : AlgoTimingEvent_FormalPupilFailure);
#endif
    bool rightProcessed = false;
    {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_ProcessRight);
#endif
        rightProcessed = needRight && rightRefined
                && processPicOfOneEye(image, imgNo, pupilRight, angle,
                                      whichEye_Right, roiRight);
    }
    bool leftProcessed = false;
    {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_ProcessLeft);
#endif
        leftProcessed = needLeft && leftRefined
                && processPicOfOneEye(image, imgNo, pupilLeft, angle,
                                      whichEye_Left, roiLeft);
    }

    if (!generationIsCurrent()) {
        return;
    }

    // 第一轮第1张只缓存原图和129 ROI状态，结果页读取时再裁剪放大。
    cacheFirstFramePupilCropSource(
            roundIdx, imgNo, image,
            needRight && rightRefined, pupilRight,
            needLeft && leftRefined, pupilLeft,
            measurementGeneration, roundGeneration);

    // 正式测量只接受当前图片 ROI 确认后的坐标；预测失败或ROI精修失败时
    // 不写入任何未确认的模型/插值坐标，也不偷偷启动传统找眼。

#if ENABLE_ALGO_TIMING_LOG
    if (needRight) {
        AlgoTiming::event(rightProcessed ? AlgoTimingEvent_ProcessSuccess
                                         : AlgoTimingEvent_ProcessFailure);
    }
    if (needLeft) {
        AlgoTiming::event(leftProcessed ? AlgoTimingEvent_ProcessSuccess
                                        : AlgoTimingEvent_ProcessFailure);
    }
#endif
    std::lock_guard<std::mutex> lock(m_mutex);
    MeasurementRound& round = m_rounds[roundIdx];
    if (rightProcessed) {
        round.setPupil(imgNo, roiRight, pupilRight, whichEye_Right);
    }
    if (leftProcessed) {
        round.setPupil(imgNo, roiLeft, pupilLeft, whichEye_Left);
    }
    // 恢复调用可能是同一照片的第二次处理；保留此前已成功的状态，
    // 不能因本次恢复没有补到另一眼而把整张照片清成失败。
    round.frames[imgNo].pupilDetected =
            round.frames[imgNo].pupilDetected
            || rightProcessed || leftProcessed;
    round.frames[imgNo].processed = true;
    const bool finalRightValid = !eyeFlags.first
            || round.validRight.test(imgNo);
    const bool finalLeftValid = !eyeFlags.second
            || round.validLeft.test(imgNo);
    if (finalRightValid && finalLeftValid) {
        round.frames[imgNo].failureReason.clear();
    } else if (!finalRightValid && !finalLeftValid) {
        round.frames[imgNo].failureReason = "hybrid_both_eyes_failed_or_refine_failed";
    } else if (!finalRightValid) {
        round.frames[imgNo].failureReason = "hybrid_right_eye_failed_or_refine_failed";
    } else {
        round.frames[imgNo].failureReason = "hybrid_left_eye_failed_or_refine_failed";
    }
}

bool CAlgo::refineFormalPupilPrediction(const cv::Mat& image,
                                        int imageNumber,
                                        enWhichEye whichEye,
                                        const stPupilInfo& predicted,
                                        stPupilInfo& refined,
                                        PupilRoiRefineResult& diagnostic) const
{
    // 该函数只执行129 ROI；正式异步路径的传统兜底由上层显式关闭。
#if ENABLE_ALGO_TIMING_LOG
    const auto started = std::chrono::steady_clock::now();
#endif
    const bool refinedOk = refinePupilInPredictedRoi(
            image, imageNumber, whichEye, predicted.center,
            static_cast<float>(predicted.radius), humaneye_wh_ratio, diagnostic);
#if ENABLE_ALGO_TIMING_LOG
    // 129 ROI失败也要保留耗时，便于区分“未调用”和“调用但未确认”。
    AlgoTiming::recordMilliseconds(
            whichEye == whichEye_Right
                    ? AlgoTimingStage_Roi129Right
                    : AlgoTimingStage_Roi129Left,
            std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - started).count());
#endif
    if (refinedOk) {
        refined = predicted;
        refined.center = diagnostic.refinedCenter;
        refined.spotPt = diagnostic.refinedCenter;
        refined.radius = diagnostic.refinedRadius;
        refined.rect = cv::Rect(cvRound(refined.center.x - refined.radius),
                                cvRound(refined.center.y - refined.radius),
                                cvRound(refined.radius * 2.0f),
                                cvRound(refined.radius * 2.0f));
        refined.area = diagnostic.contourArea;
        refined.perimeter = diagnostic.contourCircularity > 0.0
                ? std::sqrt(4.0 * CV_PI * refined.area
                            / diagnostic.contourCircularity)
                : 2.0 * CV_PI * refined.radius;
        refined.circularity = diagnostic.contourCircularity;
        refined.dx = 0.0f;
        refined.dy = 0.0f;
        refined.fallbackType = PupilFallback_RoiRefined;
        refined.eyeRectSource = PupilEyeRect_Base;
        return true;
    }
    return false;
}

const char* CAlgo::formalAnchorStateName(FormalAnchorState state)
{
    switch (state) {
    case FormalAnchor_NoAnchor:
        return "NoAnchor";
    case FormalAnchor_Confirmed:
        return "Confirmed";
    }
    return "Unknown";
}

void CAlgo::setFormalAnchorConfirmedLocked(int roundIdx,
                                           bool rightEye,
                                           const stPupilInfo& confirmed,
                                           const char* reason)
{
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return;
    }
    FormalEyeAnchorState& eyeState = rightEye
            ? m_rounds[roundIdx].asyncState.rightAnchor
            : m_rounds[roundIdx].asyncState.leftAnchor;
    const FormalAnchorState from = eyeState.state;
    eyeState.confirmedCoordinate = confirmed;
    eyeState.finalReliable = true;
    eyeState.confirmedInCurrentRound = true;
    eyeState.consecutiveFailureCount = 0;
    eyeState.state = FormalAnchor_Confirmed;
    if (from != eyeState.state) {
#if ENABLE_ALGO_VERBOSE_LOG
        qInfo().noquote()
                << QString("FormalAnchorState: round=%1,eye=%2,from=%3,to=%4,reason=%5")
                   .arg(roundIdx)
                   .arg(rightEye ? "right" : "left")
                   .arg(formalAnchorStateName(from))
                   .arg(formalAnchorStateName(eyeState.state))
                   .arg(QString::fromLatin1(reason ? reason : "roi_refined"));
#else
        Q_UNUSED(reason);
#endif
    }
}

bool CAlgo::processFormalCrossRound(int roundIdx,
                                    const std::vector<cv::Mat>& images,
                                    const PupilCrossRoundTargetGradientCache*
                                            targetGradientCache)
{
    if (m_formalStreamingStop.load(std::memory_order_acquire)
            || m_hasEmittedFinalResult.load(std::memory_order_acquire)
            || resultState != calcResultState_Succ) {
        return false;
    }
    if (!m_formalCrossRoundState
            || !m_formalCrossRoundState->ready
            // 只能由已完成的更早一轮作为源轮；通过质量门槛后会更新为
            // 相邻上一轮，减少第3轮相对第1轮的头位累计变化。
            || m_formalCrossRoundState->sourceRoundIndex < 0
            || m_formalCrossRoundState->sourceRoundIndex >= roundIdx
            || m_formalCrossRoundState->sourceImages.size() != FRAMES_PER_ROUND
            || m_formalCrossRoundState->sourceFrames.size() != FRAMES_PER_ROUND
            || images.size() != FRAMES_PER_ROUND) {
        return false;
    }

    PupilCrossRoundTracker tracker;
    PupilCrossRoundOptions options;
    const auto eyeFlags = get_eye_flags(m_eye);
    options.trackSubjectRight = eyeFlags.first;
    options.trackSubjectLeft = eyeFlags.second;
    options.processingScale = FORMAL_TRACK_SCALE;
    options.maximumTemplateHalf = FORMAL_CROSS_TEMPLATE_HALF;
    options.maximumSearchMargin = FORMAL_CROSS_SEARCH_MARGIN;
    options.minimumMatchScore = FORMAL_CROSS_MINIMUM_SCORE;
    // 正式流程启用已通过板端多会话复核的粗到细跨轮校正。仅稀疏照片
    // 需要粗定位；成功后仍由原高分辨率精匹配给出最终瞳孔位置。
    options.enableCoarsePrealignment = FORMAL_CROSS_ENABLE_COARSE_PREALIGNMENT;
    options.coarseProcessingScale = FORMAL_CROSS_COARSE_PROCESSING_SCALE;
    options.coarseSearchMargin = FORMAL_CROSS_COARSE_SEARCH_MARGIN;
    options.coarseMinimumMatchScore = FORMAL_CROSS_COARSE_MINIMUM_SCORE;
    // 正常样本仅匹配指定的三张“实际拍摄照片”。跟踪器内部使用数组索引，
    // 但配置和日志均保持实际照片编号，避免与灯位概念混淆。
    options.enableSparseAnchorFastPath = true;
    // 稀疏阶段恰好仅一眼失败时，成功眼可由两张一致的近阈值轨迹复用；
    // 另一眼保持完整22次匹配，避免遮挡眼拖慢整轮至双眼44次匹配。
    options.enableSingleEyeFullMatchFallback = true;
    options.singleEyeFallbackMinimumScore =
            FORMAL_CROSS_SINGLE_EYE_FALLBACK_MINIMUM_SCORE;
    // 正式双眼流程启用经离线八组会话验证的单眼带动快速校正。
    // 单眼测量没有另一眼可交叉验证，因此保持关闭并沿用原有安全兜底。
    options.enableSingleEyeSparseCarry = eyeFlags.first && eyeFlags.second;
    options.singleEyeCarryMinimumScore =
            FORMAL_CROSS_SINGLE_EYE_CARRY_MINIMUM_SCORE;
    options.singleEyeCarryMaximumDeltaDifference =
            FORMAL_CROSS_SINGLE_EYE_CARRY_MAX_DELTA_DIFFERENCE;
    options.singleEyeCarryMaximumDisplacement =
            FORMAL_CROSS_SINGLE_EYE_CARRY_MAX_DISPLACEMENT;
    for (int captureNumber : FORMAL_CROSS_SPARSE_CAPTURE_NUMBERS) {
        const int imageNumber =
                formalImageNumberByCaptureNumber(captureNumber);
        if (imageNumber < 1) {
            qWarning() << "PupilCrossRound: cannot map actual photo"
                       << captureNumber << "to internal image slot";
            return false;
        }
        options.sparseAnchorIndices.push_back(imageNumber - 1);
    }

    std::vector<PupilLightFrame> targetFrames;
    PupilCrossRoundSummary summary;
    std::string error;
    {
        ScopedOpenCvThreadCount threadScope(PUPIL_MODEL_CPU_THREADS);
        if (!tracker.run(m_formalCrossRoundState->sourceImages,
                         m_formalCrossRoundState->sourceFrames,
                         images,
                         &targetFrames,
                         options,
                         &summary,
                         &error,
                         &m_formalCrossRoundState->sparseTemplateCache,
                         &m_formalCrossRoundState->fullTemplateCache,
                         targetGradientCache)) {
            qWarning().noquote()
                    << QString("PupilCrossRound: round %1 failed (%2), "
                               "fall back to per-round hybrid")
                       .arg(roundIdx)
                       .arg(QString::fromStdString(error));
            return false;
        }
    }

    // tracker.run()不可被中途打断；返回后先检查取消标志，不能再把旧结果写回。
    if (m_formalStreamingStop.load(std::memory_order_acquire)
            || m_hasEmittedFinalResult.load(std::memory_order_acquire)
            || resultState != calcResultState_Succ) {
        return false;
    }

    if (targetFrames.size() != FRAMES_PER_ROUND) {
        qWarning() << "PupilCrossRound: invalid output count"
                   << targetFrames.size() << "round" << roundIdx;
        return false;
    }

    // 一轮仅统计当前测量模式要求的眼。若超过一半只能直接沿用首轮旧坐标，
    // 说明跨轮模板已经失效（常见于中途退回准备界面、头位重新调整）。
    // 此时不能把“几何仍合法”误当成定位成功，必须退回本轮模型锚点+轻量跟踪。
    const int totalEyeResultsPerRound = FRAMES_PER_ROUND
            * ((eyeFlags.first ? 1 : 0) + (eyeFlags.second ? 1 : 0));
    const int minPositionedCrossEyeResults = totalEyeResultsPerRound / 2;
    // 此门槛只判断跨轮坐标是否足以维持几何连续性；最终是否进入DS，
    // 仍由下方每张照片的reliable标志单独决定。
    const int positionedCrossEyeResults =
            summary.localMatchEyeCount
            + summary.interpolatedEyeCount
            + summary.otherEyeFallbackCount;
    const bool catastrophicCrossFailure =
            summary.directReuseFallbackCount > minPositionedCrossEyeResults
            || positionedCrossEyeResults < minPositionedCrossEyeResults;
    if (catastrophicCrossFailure) {
        qWarning().noquote()
                << QString("PupilCrossRound: reject round=%1 quality gate, "
                           "score_p05=%2, local=%3, interpolated=%4, "
                           "borrow_other_eye=%5, direct_reuse=%6, "
                           "positioned=%7/%8; "
                           "fall back to per-round hybrid")
                   .arg(roundIdx)
                   .arg(summary.scoreP05, 0, 'f', 3)
                   .arg(summary.localMatchEyeCount)
                   .arg(summary.interpolatedEyeCount)
                   .arg(summary.otherEyeFallbackCount)
                   .arg(summary.directReuseFallbackCount)
                   .arg(positionedCrossEyeResults)
                   .arg(totalEyeResultsPerRound);
        return false;
    }

    // reliable 只描述跟踪证据强弱，不再决定是否把坐标送入 DS。所有低分、
    // 插值、借眼和直接复用坐标都只能作为当前图片精修的搜索起点；精修与
    // 传统当前图兜底都失败时，processAndStoreLocatedFrame 会清除该眼有效性。
    int excludedLowConfidenceRight = 0;
    int excludedLowConfidenceLeft = 0;
    // 单眼带动的低可信坐标会进入既有DS结算；单独计数，避免日志误称“已剔除”。
    int dsCandidateRight = 0;
    int dsCandidateLeft = 0;
    for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
        const PupilLightFrame& tracked = targetFrames[imgNo - 1];
        const bool rightLocated = eyeFlags.first
                && tracked.subjectRight.detected
                && tracked.subjectRight.radius >= 6.0f
                && tracked.subjectRight.radius <= 64.0f
                && isNormalPupil(tracked.subjectRight.center, whichEye_Right);
        const bool leftLocated = eyeFlags.second
                && tracked.subjectLeft.detected
                && tracked.subjectLeft.radius >= 6.0f
                && tracked.subjectLeft.radius <= 64.0f
                && isNormalPupil(tracked.subjectLeft.center, whichEye_Left);
        if ((eyeFlags.first && !rightLocated)
                || (eyeFlags.second && !leftLocated)) {
            // 当前图坐标异常时仍交给processAndStoreLocatedFrame执行一次
            // 共享传统双眼兜底；不能因为预测几何失败而跳过图像确认。
            qWarning() << "PupilCrossRound: unusable predicted geometry, "
                       << "try current-image fallback, round" << roundIdx
                       << "image" << imgNo << "right" << rightLocated
                       << "left" << leftLocated;
        }
    }

    for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
        const PupilLightFrame& tracked = targetFrames[imgNo - 1];
        // 低置信来源仍进入统一精修，但仅在精修成功后才会被写入 DS。
        const bool rightObserved = eyeFlags.first
                && tracked.subjectRight.detected
                && tracked.subjectRight.reliable;
        const bool leftObserved = eyeFlags.second
                && tracked.subjectLeft.detected
                && tracked.subjectLeft.reliable;
        const bool rightDsCandidate = eyeFlags.first
                && tracked.subjectRight.detected
                && !tracked.subjectRight.reliable;
        const bool leftDsCandidate = eyeFlags.second
                && tracked.subjectLeft.detected
                && !tracked.subjectLeft.reliable;
        const bool rightAcceptedForDs = eyeFlags.first
                && tracked.subjectRight.detected;
        const bool leftAcceptedForDs = eyeFlags.second
                && tracked.subjectLeft.detected;
        if (rightDsCandidate) {
            ++dsCandidateRight;
        } else if (eyeFlags.first && !rightObserved) {
            ++excludedLowConfidenceRight;
        }
        if (leftDsCandidate) {
            ++dsCandidateLeft;
        } else if (eyeFlags.second && !leftObserved) {
            ++excludedLowConfidenceLeft;
        }
        stPupilInfo pupilRight;
        stPupilInfo pupilLeft;
        if (eyeFlags.first) {
            pupilRight = makeTrackedPupilInfo(tracked.subjectRight);
        }
        if (eyeFlags.second) {
            pupilLeft = makeTrackedPupilInfo(tracked.subjectLeft);
        }
        processAndStoreLocatedFrame(roundIdx, imgNo, images[imgNo - 1],
                                    rightAcceptedForDs, pupilRight,
                                    leftAcceptedForDs, pupilLeft);

        // 只有ROI已实际保存为有效帧时才标记低置信来源，避免几何成功但
        // ROI构造失败的照片在后续DS一致性复核中被误当作候选。
        if (rightDsCandidate || leftDsCandidate) {
            std::lock_guard<std::mutex> lock(m_mutex);
            MeasurementRound& round = m_rounds[roundIdx];
            if (rightDsCandidate && round.validRight.test(imgNo)) {
                round.lowConfidenceRight.set(imgNo);
            }
            if (leftDsCandidate && round.validLeft.test(imgNo)) {
                round.lowConfidenceLeft.set(imgNo);
            }
        }

        if (rightDsCandidate || leftDsCandidate) {
            qDebug().noquote()
                    << QString("PupilCrossRoundDsCandidate: round=%1, photo=%2, "
                               "right=%3, left=%4")
                       .arg(roundIdx)
                       .arg(formalCaptureNumber(imgNo))
                       .arg(rightDsCandidate ? "low_confidence" : "no")
                       .arg(leftDsCandidate ? "low_confidence" : "no");
        }

    }

#if ENABLE_ALGO_VERBOSE_LOG
    for (const PupilCrossRoundSparseDiagnostic& diagnostic
         : summary.sparseDiagnostics) {
        qDebug().noquote()
                << QString("PupilCrossRoundSparse: source_round=%1, "
                           "target_round=%2, photo=%3, "
                           "right_score=%4, right_delta=(%5,%6), right_used=%7, "
                           "left_score=%8, left_delta=(%9,%10), left_used=%11")
                   .arg(m_formalCrossRoundState->sourceRoundIndex)
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(diagnostic.imageIndex + 1))
                   .arg(diagnostic.subjectRightScore, 0, 'f', 3)
                   .arg(diagnostic.subjectRightDelta.x, 0, 'f', 1)
                   .arg(diagnostic.subjectRightDelta.y, 0, 'f', 1)
                   .arg(diagnostic.subjectRightAccepted ? "yes" : "no")
                   .arg(diagnostic.subjectLeftScore, 0, 'f', 3)
                   .arg(diagnostic.subjectLeftDelta.x, 0, 'f', 1)
                   .arg(diagnostic.subjectLeftDelta.y, 0, 'f', 1)
                   .arg(diagnostic.subjectLeftAccepted ? "yes" : "no");
    }

    qDebug().noquote()
            << QString("PupilCrossRound: round=%1, total=%2 ms "
                       "(template=%3, gradient=%4, match=%5), "
                       "source_round=%6, sparse_cache=%7, full_cache=%8, "
                       "sparse_attempt=%9, sparse_used=%10, full_fallback=%11, "
                       "sparse_reject=%12, score_p05=%13, local=%14, "
                       "interpolated=%15, borrow_other_eye=%16, "
                       "direct_reuse=%17, excluded_low_right=%18, "
                       "excluded_low_left=%19, single_eye_full=%20, "
                       "full_match_eyes=%21, position_only=%22, "
                       "single_eye_carry=%23, carry_weak_local=%24, "
                       "carry_position_only=%25, carry_reject=%26, "
                       "ds_candidate_right=%27, ds_candidate_left=%28, "
                       "coarse_to_fine=%29, coarse_eyes=%30, "
                       "coarse_gradient=%31 ms, coarse_match=%32 ms, "
                       "target_gradient_cache=%33")
               .arg(roundIdx)
               .arg(summary.totalMs, 0, 'f', 1)
               .arg(summary.sourceTemplateMs, 0, 'f', 1)
               .arg(summary.targetGradientMs, 0, 'f', 1)
               .arg(summary.matchMs, 0, 'f', 1)
               .arg(m_formalCrossRoundState->sourceRoundIndex)
               .arg(summary.sparseTemplateCacheReused ? "yes" : "no")
               .arg(summary.fullTemplateCacheReused ? "yes" : "no")
               .arg(summary.sparseFastPathAttempted ? "yes" : "no")
               .arg(summary.sparseFastPathUsed ? "yes" : "no")
               .arg(summary.fullMatchFallbackTriggered ? "yes" : "no")
               .arg(QString::fromStdString(summary.sparseRejectReason))
               .arg(summary.scoreP05, 0, 'f', 3)
               .arg(summary.localMatchEyeCount)
               .arg(summary.interpolatedEyeCount)
               .arg(summary.otherEyeFallbackCount)
               .arg(summary.directReuseFallbackCount)
               .arg(excludedLowConfidenceRight)
               .arg(excludedLowConfidenceLeft)
               .arg(summary.singleEyeFullMatchFallbackUsed ? "yes" : "no")
               .arg(summary.fullMatchRequestedEyeCount)
               .arg(summary.interpolatedPositionOnlyEyeCount)
               .arg(summary.singleEyeSparseCarryUsed ? "yes" : "no")
               .arg(summary.singleEyeCarryLocalEvidenceCount)
               .arg(summary.singleEyeCarryPositionOnlyEyeCount)
               .arg(QString::fromStdString(summary.singleEyeCarryRejectReason))
               .arg(dsCandidateRight)
               .arg(dsCandidateLeft)
               .arg(options.enableCoarsePrealignment ? "on" : "off")
               .arg(summary.coarsePrealignedEyeCount)
               .arg(summary.coarseGradientMs, 0, 'f', 1)
               .arg(summary.coarseMatchMs, 0, 'f', 1)
               .arg(targetGradientCache ? "streamed" : "realtime");
#endif

    // 不再依据原始tracker证据直接刷新source。此处只暂存候选，候选中的
    // sourceFrames由MeasurementRound最终精修坐标构建，并在结算后提交。
    stageCrossRoundSourceCandidate(roundIdx, images);
    return true;
}

void CAlgo::processFormalStreamingRound(int roundIdx)
{
    const auto streamingState =
            std::make_shared<FormalStreamingRoundState>();
    streamingState->trackedFrames.resize(FRAMES_PER_ROUND);
    for (int index = 0; index < FRAMES_PER_ROUND; ++index) {
        streamingState->trackedFrames[index].lampNumber = index + 1;
    }

    const auto eyeFlags = get_eye_flags(m_eye);
    // 旧整轮兼容路径只负责复用已提交的跨轮源；正式逐照片异步路径的
    // 手持重锚和轮内恢复C800由schedulePendingFormalFrames统一调度。
    if (roundIdx > 0 && roundIdx <= 2 && m_formalCrossRoundState
            && m_formalCrossRoundState->ready
            && m_formalCrossRoundState->sourceRoundIndex >= 0
            && m_formalCrossRoundState->sourceRoundIndex < roundIdx
            && m_formalCrossRoundState->sourceFrames.size()
                    == FRAMES_PER_ROUND) {
        PupilCrossRoundTracker crossRoundTracker;
        PupilCrossRoundOptions crossRoundOptions;
        crossRoundOptions.trackSubjectRight = eyeFlags.first;
        crossRoundOptions.trackSubjectLeft = eyeFlags.second;
        crossRoundOptions.processingScale = FORMAL_TRACK_SCALE;
        crossRoundOptions.maximumTemplateHalf = FORMAL_CROSS_TEMPLATE_HALF;
        crossRoundOptions.maximumSearchMargin = FORMAL_CROSS_SEARCH_MARGIN;
        crossRoundOptions.minimumMatchScore = FORMAL_CROSS_MINIMUM_SCORE;
        // 必须与processFormalCrossRound使用同一组选项，否则缓存会被运行时拒绝。
        // 流式预处理也必须生成1/4尺度整图梯度，否则第22张后的粗定位
        // 会退化为临时重算，失去把计算隐藏在转灯期间的意义。
        crossRoundOptions.enableCoarsePrealignment =
                FORMAL_CROSS_ENABLE_COARSE_PREALIGNMENT;
        crossRoundOptions.coarseProcessingScale =
                FORMAL_CROSS_COARSE_PROCESSING_SCALE;
        crossRoundOptions.coarseSearchMargin = FORMAL_CROSS_COARSE_SEARCH_MARGIN;
        crossRoundOptions.coarseMinimumMatchScore =
                FORMAL_CROSS_COARSE_MINIMUM_SCORE;
        crossRoundOptions.enableSparseAnchorFastPath = true;
        crossRoundOptions.enableSingleEyeFullMatchFallback = true;
        crossRoundOptions.singleEyeFallbackMinimumScore =
                FORMAL_CROSS_SINGLE_EYE_FALLBACK_MINIMUM_SCORE;
        crossRoundOptions.enableSingleEyeSparseCarry =
                eyeFlags.first && eyeFlags.second;
        crossRoundOptions.singleEyeCarryMinimumScore =
                FORMAL_CROSS_SINGLE_EYE_CARRY_MINIMUM_SCORE;
        crossRoundOptions.singleEyeCarryMaximumDeltaDifference =
                FORMAL_CROSS_SINGLE_EYE_CARRY_MAX_DELTA_DIFFERENCE;
        crossRoundOptions.singleEyeCarryMaximumDisplacement =
                FORMAL_CROSS_SINGLE_EYE_CARRY_MAX_DISPLACEMENT;

        int lastPreparedCount = 0;
        while (!m_formalStreamingStop.load(std::memory_order_acquire)) {
            std::vector<cv::Mat> newImages;
            int availableCount = 0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                const auto contiguousCount = [&]() {
                    int count = 0;
                    const MeasurementRound& round = m_rounds[roundIdx];
                    for (int imageNumber = 1;
                         imageNumber <= FRAMES_PER_ROUND;
                         ++imageNumber) {
                        if (!round.frames[imageNumber].hasFrame
                                || round.frames[imageNumber].frame.empty()) {
                            break;
                        }
                        count = imageNumber;
                    }
                    return count;
                };
                m_formalFrameCondition.wait(lock, [&]() {
                    return m_formalStreamingStop.load(
                                std::memory_order_acquire)
                            || contiguousCount() > lastPreparedCount;
                });
                if (m_formalStreamingStop.load(std::memory_order_acquire)) {
                    qDebug() << "PupilCrossRoundStream: cancelled round"
                             << roundIdx;
                    return;
                }
                availableCount = contiguousCount();
                newImages.reserve(availableCount - lastPreparedCount);
                for (int imageNumber = lastPreparedCount + 1;
                     imageNumber <= availableCount;
                     ++imageNumber) {
                    newImages.push_back(
                            m_rounds[roundIdx].frames[imageNumber].frame);
                }
            }

            bool batchReady = true;
            double batchGradientMs = 0.0;
            std::string gradientError;
            {
                ScopedOpenCvThreadCount threadScope(PUPIL_MODEL_CPU_THREADS);
                for (int offset = 0;
                     offset < static_cast<int>(newImages.size());
                     ++offset) {
                    const int imageIndex = lastPreparedCount + offset;
                    if (!crossRoundTracker.prepareTargetGradientFrame(
                                newImages[offset],
                                m_formalCrossRoundState->sourceFrames[imageIndex],
                                imageIndex, FRAMES_PER_ROUND,
                                crossRoundOptions,
                                &streamingState->crossRoundGradientCache,
                                &batchGradientMs, &gradientError)) {
                        batchReady = false;
                        break;
                    }
                }
            }
            streamingState->crossRoundGradientTotalMs += batchGradientMs;
            if (!batchReady) {
                streamingState->crossRoundGradientCache.clear();
                streamingState->crossRoundGradientFrameCount = 0;
                streamingState->failureReason = gradientError;
                qWarning().noquote()
                        << QString("PupilCrossRoundStream: gradient failed "
                                   "round=%1, photo=%2 (%3)")
                           .arg(roundIdx)
                           .arg(formalCaptureNumber(lastPreparedCount + 1))
                           .arg(QString::fromStdString(gradientError));
                // 缓存失败不阻断本轮；第22张后仍由跨轮函数即时计算，确保结果正确。
                break;
            }
            streamingState->crossRoundGradientFrameCount = availableCount;
            lastPreparedCount = availableCount;
            if (availableCount >= FRAMES_PER_ROUND) {
                break;
            }
        }
        if (m_formalStreamingStop.load(std::memory_order_acquire)) {
            return;
        }
#if ENABLE_ALGO_VERBOSE_LOG
        qDebug().noquote()
                << QString("PupilCrossRoundStream: round=%1, cached=%2/%3, "
                           "gradient=%4 ms")
                   .arg(roundIdx)
                   .arg(streamingState->crossRoundGradientFrameCount)
                   .arg(FRAMES_PER_ROUND)
                   .arg(streamingState->crossRoundGradientTotalMs, 0, 'f', 1);
#endif
        processFormalHybridRound(roundIdx, streamingState);
        return;
    }

    // 公共双眼锚点用于流式梯度缓存；按眼别锚点则允许左右眼来自不同图片。
    const auto stereoAnchorCount = [&]() {
        int count = 0;
        for (const PupilLightFrame& frame
             : streamingState->trackedFrames) {
            if ((!eyeFlags.first || frame.isSubjectRightAnchor)
                    && (!eyeFlags.second || frame.isSubjectLeftAnchor)) {
                ++count;
            }
        }
        return count;
    };
    const auto applyFrame =
            [&](int imageNumber,
                const PupilPairFrameResult& frameResult) -> int {
        PupilLightFrame& anchor =
                streamingState->trackedFrames[imageNumber - 1];
        int acceptedEyeCount = 0;
        if (eyeFlags.first && pairModelEyeResultIsUsable(
                    frameResult.subjectRight, whichEye_Right)) {
            anchor.subjectRight.detected = true;
            anchor.subjectRight.reliable = true;
            anchor.subjectRight.center =
                    frameResult.subjectRight.center;
            anchor.subjectRight.radius =
                    frameResult.subjectRight.equivalentRadius;
            anchor.subjectRight.score = 1.0f;
            anchor.subjectRight.source = PupilSource_DeepModel;
            anchor.isSubjectRightAnchor = true;
            ++acceptedEyeCount;
        }
        if (eyeFlags.second && pairModelEyeResultIsUsable(
                    frameResult.subjectLeft, whichEye_Left)) {
            anchor.subjectLeft.detected = true;
            anchor.subjectLeft.reliable = true;
            anchor.subjectLeft.center =
                    frameResult.subjectLeft.center;
            anchor.subjectLeft.radius =
                    frameResult.subjectLeft.equivalentRadius;
            anchor.subjectLeft.score = 1.0f;
            anchor.subjectLeft.source = PupilSource_DeepModel;
            anchor.isSubjectLeftAnchor = true;
            ++acceptedEyeCount;
        }
        // 旧流式路径仍只认完整双眼锚点，避免给增量缓存输入半组模板。
        anchor.isAnchor = (!eyeFlags.first || anchor.isSubjectRightAnchor)
                && (!eyeFlags.second || anchor.isSubjectLeftAnchor);
        return acceptedEyeCount;
    };
    const auto inferStreamingPair =
            [&](int firstImageNumber,
                int secondImageNumber,
                const char* reason) -> int {
        cv::Mat firstImage;
        cv::Mat secondImage;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const MeasurementRound& round = m_rounds[roundIdx];
            if (!round.frames[firstImageNumber].hasFrame
                    || round.frames[firstImageNumber].frame.empty()
                    || !round.frames[secondImageNumber].hasFrame
                    || round.frames[secondImageNumber].frame.empty()) {
                streamingState->failureReason =
                        "selected streaming images are not cached";
                return 0;
            }
            firstImage = round.frames[firstImageNumber].frame;
            secondImage = round.frames[secondImageNumber].frame;
        }
        streamingState->attemptedFrames.set(firstImageNumber);
        streamingState->attemptedFrames.set(secondImageNumber);

        // 正式深度学习版本不允许模型缺失时落回传统算法。这里返回失败，
        // 由末尾的快速无效轮逻辑释放队列并等待下一物理轮重新尝试。
        if (!pupilPairModelAvailable()) {
            streamingState->modelRuntimeFailed = true;
            streamingState->failureReason =
                    "C800 detector is unavailable";
            return 0;
        }

        ScopedOpenCvThreadCount threadScope(PUPIL_MODEL_CPU_THREADS);
        PupilPairResult pairResult;
        std::string pairError;
        ++streamingState->modelCallCount;
        const int64 pairAttemptStart = cv::getTickCount();
        const bool pairInferOk = inferPupilPairModel(
                firstImage,
                secondImage,
                &pairResult, 0.0f, 8, &pairError);
        const double pairAttemptMs =
                (cv::getTickCount() - pairAttemptStart) * 1000.0
                / cv::getTickFrequency();
#if ENABLE_ALGO_TIMING_LOG
        // 流式正式C800直接调用成对检测器，也纳入同一组分阶段统计。
        AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Total,
                                       pairAttemptMs);
        AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Preprocess,
                                       pairResult.preprocessMs);
        AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Forward,
                                       pairResult.forwardMs);
        AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Postprocess,
                                       pairResult.postprocessMs);
#endif
        streamingState->modelPreprocessMs += pairResult.preprocessMs;
        streamingState->modelForwardMs += pairResult.forwardMs;
        streamingState->modelPostprocessMs += pairResult.postprocessMs;
        streamingState->modelTotalMs += pairResult.totalMs > 0.0
                ? pairResult.totalMs : pairAttemptMs;

        if (!pairInferOk) {
            streamingState->modelRuntimeFailed = true;
            streamingState->failureReason = pairError.empty()
                    ? "streaming C800 inference failed" : pairError;
            return 0;
        }

        const int usableCount =
                applyFrame(firstImageNumber, pairResult.frames[0])
                + applyFrame(secondImageNumber, pairResult.frames[1]);
        if (usableCount == 0) {
            streamingState->failureReason =
                    "streaming C800 pair output has no usable eye";
        }
#if ENABLE_ALGO_VERBOSE_LOG
        qDebug().noquote()
                << QString("PupilHybridDynamic: stage=%1, "
                           "photo_pair=%2+%3, usable_eyes=%4, stereo_anchors=%5")
                   .arg(QString::fromLatin1(reason))
                   .arg(formalCaptureNumber(firstImageNumber))
                   .arg(formalCaptureNumber(secondImageNumber))
                   .arg(usableCount)
                   .arg(stereoAnchorCount());
#endif
        return usableCount;
    };

    // L型箱中，这组经过验证的主输入对应相机实际拍摄的第1张+第11张。
    inferStreamingPair(FORMAL_PRIMARY_MODEL_ANCHORS.front(),
                       FORMAL_PRIMARY_MODEL_ANCHORS.back(),
                       "primary");

    // 主识别不足时，立即利用模型运行期间新到达的图片。两个输入都必须
    // 尚未推理过，避免把已经失败的第1张反复塞进C800浪费一半输入。
    if (stereoAnchorCount() < 2
            && !streamingState->modelRuntimeFailed
            && streamingState->modelCallCount
               < FORMAL_MAX_MODEL_CALLS_PER_ROUND) {
        std::vector<cv::Mat> availableImages(FRAMES_PER_ROUND);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const MeasurementRound& round = m_rounds[roundIdx];
            for (int imageNumber = 1;
                 imageNumber <= FRAMES_PER_ROUND;
                 ++imageNumber) {
                if (round.frames[imageNumber].hasFrame
                        && !round.frames[imageNumber].frame.empty()) {
                    availableImages[imageNumber - 1] =
                            round.frames[imageNumber].frame;
                }
            }
        }
        std::array<int, 2> selected = {{-1, -1}};
        std::array<double, 2> qualities = {{0.0, 0.0}};
        if (selectFormalDynamicFallbackPair(
                    availableImages,
                    streamingState->attemptedFrames,
                    eyeFlags.first,
                    eyeFlags.second,
                    &selected,
                    &qualities)) {
            ++streamingState->dynamicRecoveryCallCount;
#if ENABLE_ALGO_VERBOSE_LOG
            qDebug().noquote()
                << QString("PupilHybridDynamic: select photo_pair=%1+%2, "
                               "quality=%3/%4, available=%5")
                       .arg(formalCaptureNumber(selected[0]))
                       .arg(formalCaptureNumber(selected[1]))
                       .arg(qualities[0], 0, 'f', 1)
                       .arg(qualities[1], 0, 'f', 1)
                       .arg(static_cast<int>(std::count_if(
                            availableImages.begin(),
                            availableImages.end(),
                            [](const cv::Mat& value) {
                                return !value.empty();
                            })));
#endif
            inferStreamingPair(selected[0], selected[1],
                               "dynamic_early");
        }
    }
    streamingState->anchorsReady = stereoAnchorCount() >= 2;

    PupilLightTracker tracker;
    PupilLightTrackerOptions trackerOptions;
    trackerOptions.trackSubjectRight = eyeFlags.first;
    trackerOptions.trackSubjectLeft = eyeFlags.second;
    trackerOptions.processingScale = FORMAL_TRACK_SCALE;
    trackerOptions.useFullSmallFrame = true;
    trackerOptions.maximumTemplateHalf = FORMAL_LIGHT_TEMPLATE_HALF;
    trackerOptions.maximumSearchMargin = FORMAL_LIGHT_SEARCH_MARGIN;
    trackerOptions.maximumMovementPadding =
            FORMAL_LIGHT_MOVEMENT_PADDING;

    int lastPreparedCount = 0;
    while (!m_formalStreamingStop.load(std::memory_order_acquire)) {
        std::vector<cv::Mat> availableImages;
        int availableCount = 0;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            const auto contiguousCount = [&]() {
                int count = 0;
                const MeasurementRound& round = m_rounds[roundIdx];
                for (int imageNumber = 1;
                     imageNumber <= FRAMES_PER_ROUND;
                     ++imageNumber) {
                    if (!round.frames[imageNumber].hasFrame
                            || round.frames[imageNumber].frame.empty()) {
                        break;
                    }
                    count = imageNumber;
                }
                return count;
            };
            m_formalFrameCondition.wait(lock, [&]() {
                return m_formalStreamingStop.load(
                            std::memory_order_acquire)
                        || contiguousCount() > lastPreparedCount;
            });
            if (m_formalStreamingStop.load(std::memory_order_acquire)) {
                qDebug() << "PupilHybridStream: cancelled round"
                         << roundIdx;
                return;
            }
            availableCount = contiguousCount();
            availableImages.reserve(availableCount);
            for (int imageNumber = 1;
                 imageNumber <= availableCount;
                 ++imageNumber) {
                availableImages.push_back(
                        m_rounds[roundIdx].frames[imageNumber].frame);
            }
        }

        if (streamingState->anchorsReady
                && availableCount > lastPreparedCount) {
            ScopedOpenCvThreadCount threadScope(PUPIL_MODEL_CPU_THREADS);
            PupilLightTrackerSummary incrementalSummary;
            std::string incrementalError;
            if (!tracker.extendGradientCache(
                        availableImages,
                        streamingState->trackedFrames,
                        trackerOptions,
                        &incrementalSummary,
                        &incrementalError,
                        &streamingState->gradientCache)) {
                streamingState->anchorsReady = false;
                streamingState->gradientCache.clear();
                streamingState->failureReason = incrementalError;
                qWarning().noquote()
                        << QString("PupilHybridStream: incremental gradient "
                                   "failed at frame %1 (%2)")
                           .arg(availableCount)
                           .arg(QString::fromStdString(incrementalError));
            } else {
                streamingState->gradientFrameCount = availableCount;
                streamingState->gradientTotalMs +=
                        incrementalSummary.gradientMs;
                streamingState->gradientCropMs +=
                        incrementalSummary.gradientCropMs;
                streamingState->gradientResizeMs +=
                        incrementalSummary.gradientResizeMs;
                streamingState->gradientBlurMs +=
                        incrementalSummary.gradientBlurMs;
                streamingState->gradientSobelMs +=
                        incrementalSummary.gradientSobelMs;
                streamingState->gradientMagnitudeMs +=
                        incrementalSummary.gradientMagnitudeMs;
                streamingState->gradientNormalizeMs +=
                        incrementalSummary.gradientNormalizeMs;
            }
        }
        lastPreparedCount = availableCount;
        if (availableCount >= FRAMES_PER_ROUND) {
            break;
        }
    }
    if (m_formalStreamingStop.load(std::memory_order_acquire)) {
        qDebug() << "PupilHybridStream: cancelled before finalization, round"
                 << roundIdx;
        return;
    }

#if ENABLE_ALGO_VERBOSE_LOG
    qDebug().noquote()
            << QString("PupilHybridStream: round=%1, main_photo_pair=%2+%3, "
                       "anchors=%4, dynamic_calls=%5, model=%6 ms, "
                       "gradients=%7/%8, gradient=%9 ms, reason=%10")
               .arg(roundIdx)
               .arg(formalCaptureNumber(
                        FORMAL_PRIMARY_MODEL_ANCHORS.front()))
               .arg(formalCaptureNumber(
                        FORMAL_PRIMARY_MODEL_ANCHORS.back()))
               .arg(streamingState->anchorsReady ? "ready" : "insufficient")
               .arg(streamingState->dynamicRecoveryCallCount)
               .arg(streamingState->modelTotalMs, 0, 'f', 1)
               .arg(streamingState->gradientFrameCount)
               .arg(FRAMES_PER_ROUND)
               .arg(streamingState->gradientTotalMs, 0, 'f', 1)
               .arg(QString::fromStdString(
                        streamingState->failureReason));
#endif
    processFormalHybridRound(roundIdx, streamingState);
}

void CAlgo::processFormalHybridRound(
        int roundIdx,
        const std::shared_ptr<FormalStreamingRoundState>& streamingState)
{
    const auto taskCancelled = [this]() {
        return m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)
                || resultState != calcResultState_Succ;
    };
    if (taskCancelled()) {
        qDebug() << "PupilHybrid: cancelled before processing round" << roundIdx;
        return;
    }
    const int64 finalizeStart = cv::getTickCount();
#if ENABLE_ALGO_TIMING_LOG
    AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
    // 混合路径以整轮为一个正式任务，结算前停止，确保轮次汇总能读到本轮完整耗时。
    AlgoTimingScope formalTaskTiming(AlgoTimingStage_FormalTaskTotal);
#endif
    PupilFailDetailRoundLogScope pupilFailDetailLogScope(
            &m_rounds[roundIdx].pupilFailDetailLogCount);
    std::vector<cv::Mat> images;
    const auto eyeFlags = get_eye_flags(m_eye);
    images.reserve(FRAMES_PER_ROUND);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        MeasurementRound& round = m_rounds[roundIdx];
        for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
            if (!round.frames[imgNo].hasFrame
                    || round.frames[imgNo].frame.empty()) {
                qWarning() << "PupilHybrid: incomplete cached round"
                           << roundIdx << "image" << imgNo;
                return;
            }
            images.push_back(round.frames[imgNo].frame);
            round.frames[imgNo].frame.release();
        }
    }

    if (taskCancelled()) {
        qDebug() << "PupilHybrid: cancelled after snapshot round" << roundIdx;
        return;
    }

    // 旧整轮兼容路径优先复用已提交跨轮轨迹；若跨轮校正失败，
    // 本函数继续执行原有的本轮模型锚点+轻量跟踪安全路径。
    const PupilCrossRoundTargetGradientCache* crossRoundGradientCache =
            streamingState
            && streamingState->crossRoundGradientFrameCount
                    == FRAMES_PER_ROUND
            ? &streamingState->crossRoundGradientCache : nullptr;
    if (roundIdx > 0 && streamingState) {
#if ENABLE_ALGO_VERBOSE_LOG
        qDebug().noquote()
                << QString("PupilCrossRoundStream: finalize round=%1, "
                           "cache=%2, streamed_gradient=%3 ms")
                   .arg(roundIdx)
                   .arg(crossRoundGradientCache ? "ready" : "fallback")
                   .arg(streamingState->crossRoundGradientTotalMs,
                        0, 'f', 1);
#endif
    }
    if (roundIdx > 0 && roundIdx <= 2
            && processFormalCrossRound(roundIdx, images,
                                       crossRoundGradientCache)) {
#if ENABLE_ALGO_TIMING_LOG
        formalTaskTiming.stop();
#endif
        tryFinalizeRoundLocked(roundIdx);
        return;
    }

    if (taskCancelled()) {
        qDebug() << "PupilHybrid: cancelled after cross-round attempt" << roundIdx;
        return;
    }

    const bool useStreamingAnchors =
            streamingState && streamingState->anchorsReady;
    bool hybridOk = true;
    std::vector<PupilLightFrame> trackedFrames =
            streamingState
            ? streamingState->trackedFrames
            : std::vector<PupilLightFrame>(FRAMES_PER_ROUND);
    std::bitset<FRAME_ARRAY_SIZE> attemptedFrames = streamingState
            ? streamingState->attemptedFrames
            : std::bitset<FRAME_ARRAY_SIZE>();
    bool modelRuntimeFailed = streamingState
            ? streamingState->modelRuntimeFailed : false;
    double modelTotalMs = streamingState
            ? streamingState->modelTotalMs : 0.0;
    double modelPreprocessMs = streamingState
            ? streamingState->modelPreprocessMs : 0.0;
    double modelForwardMs = streamingState
            ? streamingState->modelForwardMs : 0.0;
    double modelPostprocessMs = streamingState
            ? streamingState->modelPostprocessMs : 0.0;
    double trackerTotalMs = streamingState
            ? streamingState->gradientTotalMs : 0.0;
    double trackerGradientMs = streamingState
            ? streamingState->gradientTotalMs : 0.0;
    double trackerGradientCropMs = streamingState
            ? streamingState->gradientCropMs : 0.0;
    double trackerGradientResizeMs = streamingState
            ? streamingState->gradientResizeMs : 0.0;
    double trackerGradientBlurMs = streamingState
            ? streamingState->gradientBlurMs : 0.0;
    double trackerGradientSobelMs = streamingState
            ? streamingState->gradientSobelMs : 0.0;
    double trackerGradientMagnitudeMs = streamingState
            ? streamingState->gradientMagnitudeMs : 0.0;
    double trackerGradientNormalizeMs = streamingState
            ? streamingState->gradientNormalizeMs : 0.0;
    double trackerMatchMs = 0.0;
    int modelCallCount = streamingState
            ? streamingState->modelCallCount : 0;
    int recoveryPairCallCount = streamingState
            ? streamingState->dynamicRecoveryCallCount : 0;
    bool dynamicQualityRetryAdded = false;
    // 正式深度路径仅在C800锚点不足时启用限量传统安全网。它不是逐帧
    // 回退，而是最多检查已经参加过C800配对的四张候选图。
    bool haarSafetyFallbackUsed = false;
    int haarSafetyScannedImageCount = 0;
    int haarSafetyRecoveredEyeCount = 0;
    PupilLightTrackerSummary trackerSummary;
    PupilLightTrackerCache trackerGradientCache;
    if (useStreamingAnchors) {
        trackerGradientCache =
                std::move(streamingState->gradientCache);
    }
    std::string failureReason = streamingState
            ? streamingState->failureReason : std::string();
    const auto accumulateTrackerTiming =
            [&](const PupilLightTrackerSummary& value) {
        trackerTotalMs += value.totalMs;
        trackerGradientMs += value.gradientMs;
        trackerGradientCropMs += value.gradientCropMs;
        trackerGradientResizeMs += value.gradientResizeMs;
        trackerGradientBlurMs += value.gradientBlurMs;
        trackerGradientSobelMs += value.gradientSobelMs;
        trackerGradientMagnitudeMs += value.gradientMagnitudeMs;
        trackerGradientNormalizeMs += value.gradientNormalizeMs;
        trackerMatchMs += value.sequentialTrackMs + value.directTrackMs;
    };

    {
        ScopedOpenCvThreadCount threadScope(PUPIL_MODEL_CPU_THREADS);
        for (int index = 0; index < FRAMES_PER_ROUND; ++index) {
            trackedFrames[index].lampNumber = index + 1;
        }

        // 正式阶段所有深度学习定位统一走C800。一次推理的两张输出分别验收：
        // 某一半无效不会丢弃另一半已经可靠的锚点。
        const auto inferPairAnchors =
                [&](int firstAnchorImgNo, int secondAnchorImgNo) -> int {
            if (taskCancelled()) {
                return 0;
            }
            PupilPairResult pairResult;
            std::string pairError;
            attemptedFrames.set(firstAnchorImgNo);
            attemptedFrames.set(secondAnchorImgNo);

            // 模型运行库/模型文件异常时，不进入逐帧Haar回退；锚点恢复
            // 阶段结束后仅允许执行有上限的传统安全网。
            if (!pupilPairModelAvailable()) {
                modelRuntimeFailed = true;
                failureReason = "C800 detector is unavailable";
                return 0;
            }
            ++modelCallCount;
            const int64 pairAttemptStart = cv::getTickCount();
            const bool pairInferOk = inferPupilPairModel(
                    images[firstAnchorImgNo - 1],
                    images[secondAnchorImgNo - 1],
                    &pairResult, 0.0f, 8, &pairError);
            const double pairAttemptMs =
                    (cv::getTickCount() - pairAttemptStart) * 1000.0
                    / cv::getTickFrequency();
#if ENABLE_ALGO_TIMING_LOG
            // 混合正式路径的恢复C800同样统计完整模型调用耗时，失败调用不跳过记录。
            AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Total,
                                           pairAttemptMs);
            AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Preprocess,
                                           pairResult.preprocessMs);
            AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Forward,
                                           pairResult.forwardMs);
            AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Postprocess,
                                           pairResult.postprocessMs);
#endif
            // 无论结果是否通过几何门禁，都记录本次C800已经消耗的时间。
            modelPreprocessMs += pairResult.preprocessMs;
            modelForwardMs += pairResult.forwardMs;
            modelPostprocessMs += pairResult.postprocessMs;
            modelTotalMs += pairResult.totalMs > 0.0
                    ? pairResult.totalMs : pairAttemptMs;
            if (!pairInferOk) {
                modelRuntimeFailed = true;
                failureReason = pairError.empty()
                        ? "C800 pair inference failed" : pairError;
                return 0;
            }

            // 单次ONNX推理不可中断；推理返回后立即检查，防止旧会话继续
            // 使用结果进行跟踪、传统兜底或最终结算。
            if (taskCancelled()) {
                return 0;
            }

            const auto applyPairFrame =
                    [&](int anchorImgNo,
                        const PupilPairFrameResult& pairFrame) -> int {
                PupilLightFrame& anchor = trackedFrames[anchorImgNo - 1];
                int acceptedEyeCount = 0;
                if (eyeFlags.first && pairModelEyeResultIsUsable(
                            pairFrame.subjectRight, whichEye_Right)) {
                    anchor.subjectRight.detected = true;
                    anchor.subjectRight.reliable = true;
                    anchor.subjectRight.center =
                            pairFrame.subjectRight.center;
                    anchor.subjectRight.radius =
                            pairFrame.subjectRight.equivalentRadius;
                    anchor.subjectRight.score = 1.0f;
                    anchor.subjectRight.source = PupilSource_DeepModel;
                    anchor.isSubjectRightAnchor = true;
                    ++acceptedEyeCount;
                }
                if (eyeFlags.second && pairModelEyeResultIsUsable(
                            pairFrame.subjectLeft, whichEye_Left)) {
                    anchor.subjectLeft.detected = true;
                    anchor.subjectLeft.reliable = true;
                    anchor.subjectLeft.center =
                            pairFrame.subjectLeft.center;
                    anchor.subjectLeft.radius =
                            pairFrame.subjectLeft.equivalentRadius;
                    anchor.subjectLeft.score = 1.0f;
                    anchor.subjectLeft.source = PupilSource_DeepModel;
                    anchor.isSubjectLeftAnchor = true;
                    ++acceptedEyeCount;
                }
                // 公共双眼跟踪仅接收同一张图上完整的双眼锚点；不完整时
                // 稍后改走按眼别独立跟踪，避免错误地把另一眼当作可靠模板。
                anchor.isAnchor = (!eyeFlags.first
                                   || anchor.isSubjectRightAnchor)
                        && (!eyeFlags.second || anchor.isSubjectLeftAnchor);
                return acceptedEyeCount;
            };
            const int usableEyeCount =
                    applyPairFrame(firstAnchorImgNo, pairResult.frames[0])
                    + applyPairFrame(secondAnchorImgNo, pairResult.frames[1]);
            if (usableEyeCount == 0) {
                failureReason = "C800 pair output has no usable eye";
            }
#if ENABLE_ALGO_VERBOSE_LOG
            qDebug().noquote()
                    << QString("PupilHybridDynamic: C800 result "
                               "photo_pair=%1+%2, usable_eyes=%3")
                       .arg(formalCaptureNumber(firstAnchorImgNo))
                       .arg(formalCaptureNumber(secondAnchorImgNo))
                       .arg(usableEyeCount);
#endif
            return usableEyeCount;
        };

        const auto stereoAnchorCount = [&]() {
            int count = 0;
            for (const PupilLightFrame& frame : trackedFrames) {
                if ((!eyeFlags.first || frame.isSubjectRightAnchor)
                        && (!eyeFlags.second || frame.isSubjectLeftAnchor)) {
                    ++count;
                }
            }
            return count;
        };
        const auto hasIndependentEyeAnchors = [&]() {
            bool hasRight = !eyeFlags.first;
            bool hasLeft = !eyeFlags.second;
            for (const PupilLightFrame& frame : trackedFrames) {
                hasRight = hasRight || frame.isSubjectRightAnchor;
                hasLeft = hasLeft || frame.isSubjectLeftAnchor;
            }
            return hasRight && hasLeft;
        };

        const auto runBoundedHaarAnchorFallback = [&]() {
            bool missingRight = eyeFlags.first;
            bool missingLeft = eyeFlags.second;
            for (const PupilLightFrame& frame : trackedFrames) {
                missingRight = missingRight && !frame.isSubjectRightAnchor;
                missingLeft = missingLeft && !frame.isSubjectLeftAnchor;
            }
            if (!missingRight && !missingLeft) {
                return;
            }

            // 只扫描已经由C800选中过的图，避免退回原先22张、44眼的
            // 全量Haar路径。旧版双眼detectPupil有“右眼成功即短路左眼”
            // 的行为，因此这里按缺失眼分别调用，保证左眼也能精定位。
            haarSafetyFallbackUsed = true;
            for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
                if (taskCancelled() || haarSafetyScannedImageCount
                        >= FORMAL_HAAR_FALLBACK_MAX_IMAGES
                        || (!missingRight && !missingLeft)) {
                    break;
                }
                if (!attemptedFrames.test(imgNo)) {
                    continue;
                }

                PupilLightFrame& frame = trackedFrames[imgNo - 1];
                const auto recoverOneEye =
                        [&](bool subjectRight) {
                    if (haarSafetyScannedImageCount
                            >= FORMAL_HAAR_FALLBACK_MAX_IMAGES) {
                        return;
                    }
                    stPupilInfo haarRight;
                    stPupilInfo haarLeft;
                    const enSingleDualEyeMode targetMode = subjectRight
                            ? singleDualEyeMode_Right : singleDualEyeMode_Left;
                    const bool haarOk = detectPupil(images[imgNo - 1].data,
                                                     imgNo, m_age,
                                                     haarRight, haarLeft,
                                                     true, targetMode);
                    ++haarSafetyScannedImageCount;
                    if (!haarOk || taskCancelled()) {
                        return;
                    }
                    stPupilInfo& target = subjectRight ? haarRight : haarLeft;
                    const enWhichEye targetEye = subjectRight
                            ? whichEye_Right : whichEye_Left;
                    if (!traditionalPupilResultIsUsable(target, targetEye)) {
                        return;
                    }
                    PupilLightEye& targetFrame = subjectRight
                            ? frame.subjectRight : frame.subjectLeft;
                    targetFrame.detected = true;
                    targetFrame.reliable = true;
                    targetFrame.center = target.center;
                    targetFrame.radius = target.radius;
                    targetFrame.score = 1.0f;
                    targetFrame.source = PupilSource_TraditionalFallback;
                    if (subjectRight) {
                        frame.isSubjectRightAnchor = true;
                        frame.isSubjectRightHaarAnchor = true;
                        missingRight = false;
                    } else {
                        frame.isSubjectLeftAnchor = true;
                        frame.isSubjectLeftHaarAnchor = true;
                        missingLeft = false;
                    }
                    ++haarSafetyRecoveredEyeCount;
                };
                if (missingRight) {
                    recoverOneEye(true);
                }
                if (missingLeft) {
                    recoverOneEye(false);
                }
                frame.isAnchor = (!eyeFlags.first
                                  || frame.isSubjectRightAnchor)
                        && (!eyeFlags.second || frame.isSubjectLeftAnchor);
            }
            qWarning().noquote()
                    << QString("PupilHybridHaarSafety: eye_calls=%1/%2, "
                               "recovered_eyes=%3, missing_right=%4, "
                               "missing_left=%5")
                       .arg(haarSafetyScannedImageCount)
                       .arg(FORMAL_HAAR_FALLBACK_MAX_IMAGES)
                       .arg(haarSafetyRecoveredEyeCount)
                       .arg(missingRight ? "yes" : "no")
                       .arg(missingLeft ? "yes" : "no");
        };

        // 没有流式状态表示跨轮校正失败后才进入本轮安全路径，此时先运行
        // 固定主配对。首轮流式状态即使锚点不足也已尝试过主配对，不能重复。
        if (!streamingState) {
            inferPairAnchors(FORMAL_PRIMARY_MODEL_ANCHORS.front(),
                             FORMAL_PRIMARY_MODEL_ANCHORS.back());
        }

        // 主配对只有0～1个可靠锚点时，从尚未推理的22张图片中动态挑选
        // 质量最好且拍摄间隔较大的两张。不会重复使用已经失败的主图片。
        while (stereoAnchorCount() < 2
               && !taskCancelled()
               && !modelRuntimeFailed
               && modelCallCount < FORMAL_MAX_MODEL_CALLS_PER_ROUND) {
            std::array<int, 2> selected = {{-1, -1}};
            std::array<double, 2> qualities = {{0.0, 0.0}};
            if (!selectFormalDynamicFallbackPair(
                        images,
                        attemptedFrames,
                        eyeFlags.first,
                        eyeFlags.second,
                        &selected,
                        &qualities)) {
                break;
            }
            ++recoveryPairCallCount;
#if ENABLE_ALGO_VERBOSE_LOG
            qDebug().noquote()
                    << QString("PupilHybridDynamic: final_select "
                               "photo_pair=%1+%2, quality=%3/%4, "
                                "round_model_attempt=%5/%6")
                       .arg(formalCaptureNumber(selected[0]))
                       .arg(formalCaptureNumber(selected[1]))
                       .arg(qualities[0], 0, 'f', 1)
                       .arg(qualities[1], 0, 'f', 1)
                       .arg(modelCallCount + 1)
                       .arg(FORMAL_MAX_MODEL_CALLS_PER_ROUND);
#endif
            inferPairAnchors(selected[0], selected[1]);
        }
        if (taskCancelled()) {
            qDebug() << "PupilHybrid: cancelled during anchor recovery round"
                     << roundIdx;
            return;
        }
        if (!hasIndependentEyeAnchors()) {
            // 兼容整轮路径也不再追加传统安全网；正式逐照片路径失败时由
            // 129 ROI失败判定和每轮一次恢复C800负责收尾。
            qDebug() << "PupilHybrid: traditional safety fallback disabled"
                     << roundIdx;
        }
        if (taskCancelled()) {
            qDebug() << "PupilHybrid: cancelled during light recovery round"
                     << roundIdx;
            return;
        }
        const bool useIndependentEyeTracking =
                stereoAnchorCount() < 2 && hasIndependentEyeAnchors();
        if (stereoAnchorCount() < 2 && !useIndependentEyeTracking) {
            hybridOk = false;
            if (failureReason.empty()) {
                failureReason = modelRuntimeFailed
                        ? "C800 runtime failed; skip repeated model calls"
                        : "C800 could not provide a reliable anchor for each eye";
            }
        }

        if (hybridOk) {
            PupilLightTracker tracker;
            PupilLightTrackerOptions trackerOptions;
            trackerOptions.trackSubjectRight = eyeFlags.first;
            trackerOptions.trackSubjectLeft = eyeFlags.second;
            trackerOptions.processingScale = FORMAL_TRACK_SCALE;
            trackerOptions.useFullSmallFrame = true;
            trackerOptions.maximumTemplateHalf = FORMAL_LIGHT_TEMPLATE_HALF;
            trackerOptions.maximumSearchMargin = FORMAL_LIGHT_SEARCH_MARGIN;
            trackerOptions.maximumMovementPadding =
                    FORMAL_LIGHT_MOVEMENT_PADDING;
            std::string trackerError;
            if (useIndependentEyeTracking) {
                // C800可能在不同图中分别只看清左右眼。此时每只眼使用自己
                // 的锚点独立做前后跟踪，不把未识别的另一眼伪造为模型结果。
                PupilLightTrackerSummary mergedSummary;
                mergedSummary.selectedMode = "independent_eye";
                mergedSummary.processingScale = trackerOptions.processingScale;
                bool mergedAnyEye = false;
                const auto runIndependentEye =
                        [&](bool subjectRight) -> bool {
                    const bool shouldTrack = subjectRight
                            ? eyeFlags.first : eyeFlags.second;
                    if (!shouldTrack) {
                        return true;
                    }
                    std::vector<PupilLightFrame> eyeFrames = trackedFrames;
                    for (PupilLightFrame& frame : eyeFrames) {
                        frame.isAnchor = subjectRight
                                ? frame.isSubjectRightAnchor
                                : frame.isSubjectLeftAnchor;
                    }
                    PupilLightTrackerOptions eyeOptions = trackerOptions;
                    eyeOptions.trackSubjectRight = subjectRight;
                    eyeOptions.trackSubjectLeft = !subjectRight;
                    PupilLightTrackerSummary eyeSummary;
                    std::string eyeError;
                    if (!tracker.run(images, &eyeFrames, eyeOptions,
                                     &eyeSummary, &eyeError, nullptr)) {
                        failureReason = (subjectRight ? "right eye: "
                                                       : "left eye: ")
                                + eyeError;
                        return false;
                    }
                    for (int index = 0; index < FRAMES_PER_ROUND; ++index) {
                        if (subjectRight) {
                            trackedFrames[index].subjectRight =
                                    eyeFrames[index].subjectRight;
                        } else {
                            trackedFrames[index].subjectLeft =
                                    eyeFrames[index].subjectLeft;
                        }
                    }
                    // 两次独立运行都记录进同一轮耗时，便于和公共双眼路径对比。
                    mergedSummary.gradientMs += eyeSummary.gradientMs;
                    mergedSummary.gradientCropMs += eyeSummary.gradientCropMs;
                    mergedSummary.gradientResizeMs += eyeSummary.gradientResizeMs;
                    mergedSummary.gradientBlurMs += eyeSummary.gradientBlurMs;
                    mergedSummary.gradientSobelMs += eyeSummary.gradientSobelMs;
                    mergedSummary.gradientMagnitudeMs += eyeSummary.gradientMagnitudeMs;
                    mergedSummary.gradientNormalizeMs += eyeSummary.gradientNormalizeMs;
                    mergedSummary.sequentialTrackMs += eyeSummary.sequentialTrackMs;
                    mergedSummary.directTrackMs += eyeSummary.directTrackMs;
                    mergedSummary.totalMs += eyeSummary.totalMs;
                    mergedSummary.gradientPixelRatio += eyeSummary.gradientPixelRatio;
                    mergedSummary.unreliableEyeCount += eyeSummary.unreliableEyeCount;
                    mergedSummary.directFallbackTriggered =
                            mergedSummary.directFallbackTriggered
                            || eyeSummary.directFallbackTriggered;
                    if (!mergedAnyEye) {
                        mergedSummary.scoreP05 = eyeSummary.scoreP05;
                    } else {
                        mergedSummary.scoreP05 = std::min(
                                mergedSummary.scoreP05, eyeSummary.scoreP05);
                    }
                    mergedAnyEye = true;
                    return true;
                };
                if (!runIndependentEye(true) || !runIndependentEye(false)) {
                    hybridOk = false;
                } else {
                    if (mergedAnyEye) {
                        const int trackedEyeCount = (eyeFlags.first ? 1 : 0)
                                + (eyeFlags.second ? 1 : 0);
                        if (trackedEyeCount > 0) {
                            mergedSummary.gradientPixelRatio /= trackedEyeCount;
                        }
                    }
                    trackerSummary = mergedSummary;
                    accumulateTrackerTiming(trackerSummary);
                    qWarning().noquote()
                            << QString("PupilHybrid: independent-eye tracking "
                                       "used, stereo_anchors=%1")
                               .arg(stereoAnchorCount());
                }
            } else {
                if (!tracker.run(images, &trackedFrames, trackerOptions,
                                 &trackerSummary, &trackerError,
                                 &trackerGradientCache)) {
                    hybridOk = false;
                    failureReason = trackerError;
                } else {
                    accumulateTrackerTiming(trackerSummary);
                }
            }

            if (taskCancelled()) {
                qDebug() << "PupilHybrid: cancelled after light tracking round"
                         << roundIdx;
                return;
            }

            // 两锚点结果只有在每只眼均通过几何、半径和匹配分数检查时才直接采用。
            bool needsMiddleAnchor = false;
            if (hybridOk) {
                needsMiddleAnchor =
                        trackerSummary.scoreP05 < FORMAL_TRACK_RELIABLE_SCORE
                        || trackerSummary.unreliableEyeCount > 0;
                for (const PupilLightFrame& tracked : trackedFrames) {
                    if (tracked.isAnchor) {
                        continue;
                    }
                    const bool rightUsable =
                            tracked.subjectRight.detected
                            && tracked.subjectRight.reliable
                            && tracked.subjectRight.score
                               >= FORMAL_TRACK_RELIABLE_SCORE
                            && tracked.subjectRight.radius >= 6.0f
                            && tracked.subjectRight.radius <= 64.0f
                            && isNormalPupil(tracked.subjectRight.center,
                                             whichEye_Right);
                    const bool leftUsable =
                            tracked.subjectLeft.detected
                            && tracked.subjectLeft.reliable
                            && tracked.subjectLeft.score
                               >= FORMAL_TRACK_RELIABLE_SCORE
                            && tracked.subjectLeft.radius >= 6.0f
                            && tracked.subjectLeft.radius <= 64.0f
                            && isNormalPupil(tracked.subjectLeft.center,
                                             whichEye_Left);
                    if ((eyeFlags.first && !rightUsable)
                            || (eyeFlags.second && !leftUsable)) {
                        needsMiddleAnchor = true;
                        break;
                    }
                }
            }

            if (hybridOk && !useIndependentEyeTracking
                    && !haarSafetyFallbackUsed && needsMiddleAnchor
                    && !taskCancelled()
                    && !modelRuntimeFailed
                    && modelCallCount
                       < FORMAL_MAX_MODEL_CALLS_PER_ROUND) {
                // 限量Haar已介入时不再追加第三次C800，避免困难样本把
                // 单轮耗时继续拉长。否则质量不足时动态挑选一对补强。
                // 不固定补某一张，更不复用旧锚点浪费
                // C800的一半输入；仍从未推理图片中动态选两张，一次补两个机会。
                std::array<int, 2> selected = {{-1, -1}};
                std::array<double, 2> qualities = {{0.0, 0.0}};
                if (selectFormalDynamicFallbackPair(
                            images,
                            attemptedFrames,
                            eyeFlags.first,
                            eyeFlags.second,
                            &selected,
                            &qualities)) {
                    ++recoveryPairCallCount;
#if ENABLE_ALGO_VERBOSE_LOG
                    qDebug().noquote()
                            << QString("PupilHybridDynamic: quality_retry "
                                       "photo_pair=%1+%2, quality=%3/%4, "
                                       "round_model_attempt=%5/%6")
                               .arg(formalCaptureNumber(selected[0]))
                               .arg(formalCaptureNumber(selected[1]))
                               .arg(qualities[0], 0, 'f', 1)
                               .arg(qualities[1], 0, 'f', 1)
                               .arg(modelCallCount + 1)
                               .arg(FORMAL_MAX_MODEL_CALLS_PER_ROUND);
#endif
                    const int addedAnchorCount =
                            inferPairAnchors(selected[0], selected[1]);
                    if (addedAnchorCount <= 0) {
                        hybridOk = false;
                        if (failureReason.empty()) {
                            failureReason =
                                    "C800 dynamic quality retry incomplete";
                        }
                    } else {
                        dynamicQualityRetryAdded = true;

                        // 梯度缓存已经生成，本次只根据新增锚点重做轻量匹配，
                        // 不会再次计算22张的梯度预处理。
                        PupilLightTrackerSummary fallbackTrackerSummary;
                        trackerError.clear();
                        if (!tracker.run(images, &trackedFrames,
                                         trackerOptions,
                                         &fallbackTrackerSummary,
                                         &trackerError,
                                         &trackerGradientCache)) {
                            hybridOk = false;
                            failureReason = trackerError;
                        } else {
                            accumulateTrackerTiming(fallbackTrackerSummary);
                            trackerSummary = fallbackTrackerSummary;
                        }
                    }
                }
            }

            if (taskCancelled()) {
                qDebug() << "PupilHybrid: cancelled after quality recovery round"
                         << roundIdx;
                return;
            }
        }

        if (hybridOk) {
            for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
                if (taskCancelled()) {
                    qDebug() << "PupilHybrid: cancelled while storing round"
                             << roundIdx;
                    return;
                }
                PupilLightFrame& tracked = trackedFrames[imgNo - 1];
                stPupilInfo pupilRight;
                stPupilInfo pupilLeft;
                if (eyeFlags.first) {
                    pupilRight = makeTrackedPupilInfo(tracked.subjectRight);
                }
                if (eyeFlags.second) {
                    pupilLeft = makeTrackedPupilInfo(tracked.subjectLeft);
                }
                if (eyeFlags.first && tracked.isSubjectRightAnchor
                        && !tracked.isSubjectRightHaarAnchor) {
                    pupilRight.fallbackType = PupilFallback_DeepModel;
                }
                if (eyeFlags.second && tracked.isSubjectLeftAnchor
                        && !tracked.isSubjectLeftHaarAnchor) {
                    pupilLeft.fallbackType = PupilFallback_DeepModel;
                }

                bool rightLocated = eyeFlags.first
                        && tracked.subjectRight.detected
                        && tracked.subjectRight.radius >= 6.0f
                        && tracked.subjectRight.radius <= 64.0f
                        && isNormalPupil(tracked.subjectRight.center,
                                         whichEye_Right);
                bool leftLocated = eyeFlags.second
                        && tracked.subjectLeft.detected
                        && tracked.subjectLeft.radius >= 6.0f
                        && tracked.subjectLeft.radius <= 64.0f
                        && isNormalPupil(tracked.subjectLeft.center,
                                         whichEye_Left);

                // 非锚点低分只作为当前图 ROI 精修的搜索起点；只有精修或
                // 当前图传统兜底成功后，processAndStore 才会写入 DS。
                const bool rightLowConfidence = rightLocated
                        && !tracked.isSubjectRightAnchor
                        && tracked.subjectRight.score
                           < FORMAL_TRACK_RELIABLE_SCORE;
                const bool leftLowConfidence = leftLocated
                        && !tracked.isSubjectLeftAnchor
                        && tracked.subjectLeft.score
                           < FORMAL_TRACK_RELIABLE_SCORE;
                if (rightLowConfidence || leftLowConfidence) {
                    qDebug().noquote()
                            << QString("PupilHybridDsCandidate: round=%1, "
                                       "photo=%2, right=%3, left=%4")
                               .arg(roundIdx)
                               .arg(formalCaptureNumber(imgNo))
                               .arg(rightLowConfidence ? "low_confidence" : "no")
                               .arg(leftLowConfidence ? "low_confidence" : "no");
                }

                processAndStoreLocatedFrame(roundIdx, imgNo,
                                            images[imgNo - 1],
                                            rightLocated, pupilRight,
                                            leftLocated, pupilLeft);

                if (rightLowConfidence || leftLowConfidence) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    MeasurementRound& round = m_rounds[roundIdx];
                    if (rightLowConfidence && round.validRight.test(imgNo)) {
                        round.lowConfidenceRight.set(imgNo);
                    }
                    if (leftLowConfidence && round.validLeft.test(imgNo)) {
                        round.lowConfidenceLeft.set(imgNo);
                    }
                }
            }
        }
    }

    if (taskCancelled()) {
        qDebug() << "PupilHybrid: cancelled before round finalization"
                 << roundIdx;
        return;
    }

    if (hybridOk && m_formalCrossRoundState) {
        // 不能把模型/跟踪轨迹直接作为source；先暂存，等待本轮结算确认。
        stageCrossRoundSourceCandidate(roundIdx, images);
    }

    if (!hybridOk) {
        qWarning().noquote()
                << QString("PupilHybrid: round %1 failed (%2); "
                           "marking the round invalid after bounded Haar safety")
                   .arg(roundIdx)
                   .arg(QString::fromStdString(failureReason));
        // C800与限量Haar安全网仍未形成锚点、运行时出错或轻量跟踪失败时，
        // 必须尽快让本轮走原有DS拒绝路径；不允许44次Haar堵塞后续物理轮。
        for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
            processAndStoreLocatedFrame(roundIdx, imgNo,
                                        images[imgNo - 1],
                                        false, stPupilInfo(),
                                        false, stPupilInfo());
        }
#if ENABLE_ALGO_TIMING_LOG
        formalTaskTiming.stop();
#endif
        tryFinalizeRoundLocked(roundIdx);
        return;
    }

#if ENABLE_ALGO_VERBOSE_LOG
    qDebug().noquote()
            << QString("PupilHybrid: round=%1, model_calls=%2, model=%3 ms "
                       "(pre=%4, forward=%5, post=%6), track=%7 ms "
                       "(gradient=%8, match=%9), gradient_roi=%10%, "
                       "retry_cache=%11, recovery_pair_calls=%12, "
                       "dynamic_quality_retry=%13, haar_safety=%14, "
                       "haar_scan=%15, haar_recovered=%16, score_p05=%17, "
                       "unreliable=%18, mode=%19")
               .arg(roundIdx)
               .arg(modelCallCount)
               .arg(modelTotalMs, 0, 'f', 1)
               .arg(modelPreprocessMs, 0, 'f', 1)
               .arg(modelForwardMs, 0, 'f', 1)
               .arg(modelPostprocessMs, 0, 'f', 1)
               .arg(trackerTotalMs, 0, 'f', 1)
               .arg(trackerGradientMs, 0, 'f', 1)
               .arg(trackerMatchMs, 0, 'f', 1)
               .arg(trackerSummary.gradientPixelRatio * 100.0, 0, 'f', 1)
               .arg(trackerSummary.gradientCacheReused ? "yes" : "no")
               .arg(recoveryPairCallCount)
               .arg(dynamicQualityRetryAdded ? "yes" : "no")
               .arg(haarSafetyFallbackUsed ? "yes" : "no")
               .arg(haarSafetyScannedImageCount)
               .arg(haarSafetyRecoveredEyeCount)
               .arg(trackerSummary.scoreP05, 0, 'f', 3)
               .arg(trackerSummary.unreliableEyeCount)
               .arg(QString::fromStdString(trackerSummary.selectedMode));
    qDebug().noquote()
            << QString("PupilHybridStreamFinal: round=%1, stream=%2, "
                       "model_calls=%3, recovery_calls=%4, "
                       "precomputed_gradients=%5/%6, post_capture=%7 ms")
               .arg(roundIdx)
               .arg(useStreamingAnchors ? "used" : "final_recovery")
               .arg(modelCallCount)
               .arg(recoveryPairCallCount)
               .arg(streamingState
                    ? streamingState->gradientFrameCount : 0)
               .arg(FRAMES_PER_ROUND)
               .arg((cv::getTickCount() - finalizeStart) * 1000.0
                    / cv::getTickFrequency(), 0, 'f', 1);
    qDebug().noquote()
            << QString("PupilHybridGradient: round=%1, crop=%2 ms, "
                       "resize=%3 ms, blur=%4 ms, sobel=%5 ms, "
                       "magnitude=%6 ms, normalize=%7 ms")
               .arg(roundIdx)
               .arg(trackerGradientCropMs, 0, 'f', 1)
               .arg(trackerGradientResizeMs, 0, 'f', 1)
               .arg(trackerGradientBlurMs, 0, 'f', 1)
               .arg(trackerGradientSobelMs, 0, 'f', 1)
               .arg(trackerGradientMagnitudeMs, 0, 'f', 1)
               .arg(trackerGradientNormalizeMs, 0, 'f', 1);
#endif
#if ENABLE_ALGO_TIMING_LOG
    formalTaskTiming.stop();
#endif
    tryFinalizeRoundLocked(roundIdx);
}

bool CAlgo::runFormalC800Pair(const cv::Mat& firstImage,
                              const cv::Mat& secondImage,
                              PupilLightFrame* firstFrame,
                              PupilLightFrame* secondFrame,
                              std::string* errorMessage)
{
    if (firstFrame == nullptr || firstImage.empty()
            || secondImage.empty()) {
        if (errorMessage) {
            *errorMessage = "formal C800 pair input is empty";
        }
        return false;
    }

    *firstFrame = PupilLightFrame{};
    firstFrame->lampNumber = 1;
    firstFrame->isAnchor = true;
    if (secondFrame) {
        *secondFrame = PupilLightFrame{};
        secondFrame->lampNumber = 1;
        secondFrame->isAnchor = true;
    }
    const auto eyeFlags = get_eye_flags(m_eye);

    if (isSimulatedEye || !ensurePupilModelLoaded()) {
        if (errorMessage) {
            *errorMessage = "formal C800 model is unavailable";
        }
        return false;
    }

    PupilPairResult result;
    std::string inferError;
    bool inferSucceeded = false;
    {
        std::lock_guard<std::mutex> lock(m_pupilModelMutex);
        if (!pupilPairModelAvailable()) {
            inferError = "formal C800 detector is null";
        } else {
            // 正式模型调用与预览使用同一CPU线程上限，离开作用域后恢复
            // 原有OpenCV线程配置。
            ScopedOpenCvThreadCount threadScope(PUPIL_MODEL_CPU_THREADS);
            // 正式入口传入当前真实小图和同尺寸黑图；旧兼容调用仍复用
            // 该底层接口，但不属于正式逐照片异步流程。
            inferSucceeded = inferPupilPairModel(
                    firstImage, secondImage, &result, 0.0f, 8, &inferError);
        }
    }

#if ENABLE_ALGO_TIMING_LOG
    // 检测器已经提供三段模型耗时；即使几何门禁或推理失败，也保留本次调用的已测结果。
    if (AlgoTiming::currentPhase() == AlgoTimingPhase_Formal) {
        AlgoTiming::recordMilliseconds(
                AlgoTimingStage_C800Preprocess, result.preprocessMs);
        AlgoTiming::recordMilliseconds(
                AlgoTimingStage_C800Forward, result.forwardMs);
        AlgoTiming::recordMilliseconds(
                AlgoTimingStage_C800Postprocess, result.postprocessMs);
    }
#endif

    if (!inferSucceeded) {
        if (errorMessage) {
            *errorMessage = inferError.empty()
                    ? "formal C800 inference failed" : inferError;
        }
        return false;
    }

    const auto fillModelFrame = [&](const PupilPairFrameResult& source,
                                    PupilLightFrame* target,
                                    int lampNumber) {
        if (!target) {
            return true;
        }
        *target = PupilLightFrame{};
        target->lampNumber = lampNumber;
        target->isAnchor = true;
        bool rightOk = !eyeFlags.first
                || pairModelEyeResultIsUsable(
                        source.subjectRight, whichEye_Right);
        bool leftOk = !eyeFlags.second
                || pairModelEyeResultIsUsable(
                        source.subjectLeft, whichEye_Left);
        if (rightOk && leftOk && eyeFlags.first && eyeFlags.second) {
            const float xDiff = source.subjectLeft.center.x
                    - source.subjectRight.center.x;
            const float yDiff = std::abs(source.subjectRight.center.y
                                         - source.subjectLeft.center.y);
            // 正式C800接收原图；兼容小图调用仍按实际输入尺寸缩放双眼
            // 几何门槛，不能把原图像素门槛直接用于小图。
            const float inputScaleX = static_cast<float>(firstImage.cols)
                    / 1280.0F;
            const float inputScaleY = static_cast<float>(firstImage.rows)
                    / 512.0F;
            if (xDiff < MIN_PREVIEW_EYE_X_DIFF * inputScaleX
                    || xDiff > MAX_PREVIEW_EYE_X_DIFF * inputScaleX
                    || yDiff > MAX_PREVIEW_EYE_Y_DIFF * inputScaleY) {
                rightOk = false;
                leftOk = false;
            }
        }

        if (eyeFlags.first && rightOk) {
            target->subjectRight.detected = true;
            target->subjectRight.reliable = true;
            target->subjectRight.center = source.subjectRight.center;
            target->subjectRight.radius = source.subjectRight.equivalentRadius;
            target->subjectRight.score = 1.0F;
            target->subjectRight.source = PupilSource_DeepModel;
            target->isSubjectRightAnchor = true;
        }
        if (eyeFlags.second && leftOk) {
            target->subjectLeft.detected = true;
            target->subjectLeft.reliable = true;
            target->subjectLeft.center = source.subjectLeft.center;
            target->subjectLeft.radius = source.subjectLeft.equivalentRadius;
            target->subjectLeft.score = 1.0F;
            target->subjectLeft.source = PupilSource_DeepModel;
            target->isSubjectLeftAnchor = true;
        }
        target->isAnchor = (!eyeFlags.first || rightOk)
                && (!eyeFlags.second || leftOk);
        return rightOk && leftOk;
    };

    const bool firstOk = fillModelFrame(result.frames[0], firstFrame, 1);
    const bool secondOk = secondFrame
            ? fillModelFrame(result.frames[1], secondFrame, 1) : true;
    if ((!firstOk || !secondOk) && errorMessage) {
        *errorMessage = "formal C800 returned an invalid required eye";
    }
    return firstOk && secondOk;
}

bool CAlgo::runFormalC800Once(const cv::Mat& image,
                              PupilLightFrame* modelFrame,
                              std::string* errorMessage)
{
    if (modelFrame == nullptr || image.empty()) {
        if (errorMessage) {
            *errorMessage = "formal C800 input is empty";
        }
        return false;
    }
    const cv::Mat blackCompanion = cv::Mat::zeros(image.size(), image.type());
    return runFormalC800Pair(image, blackCompanion, modelFrame, nullptr,
                             errorMessage);
}

void CAlgo::publishFormalAnchorFromRound(
        int roundIdx,
        int imgNo,
        const cv::Mat& image,
        std::uint64_t measurementGeneration,
        std::uint64_t roundGeneration)
{
    const auto eyeFlags = get_eye_flags(m_eye);
    std::shared_ptr<FormalPupilAnchor> anchor(new FormalPupilAnchor());
    anchor->image = image;
    anchor->smallImage = makeFormalSmallGray(image);
    anchor->lampNumber = imgNo;

    bool candidateReady = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (roundIdx >= 0 && roundIdx < MAX_ROUNDS
                && imgNo >= 1 && imgNo <= FRAMES_PER_ROUND) {
            MeasurementRound& round = m_rounds[roundIdx];
            const FormalAsyncRoundState& asyncState = round.asyncState;
            const bool generationOk =
                    asyncState.measurementGeneration == measurementGeneration
                    && asyncState.roundGeneration == roundGeneration;
            if (generationOk && !asyncState.anchorReady) {
                const bool rightConfirmed = eyeFlags.first
                        && round.frames[imgNo].processed
                        && round.validRight.test(imgNo);
                const bool leftConfirmed = eyeFlags.second
                        && round.frames[imgNo].processed
                        && round.validLeft.test(imgNo);
                if (rightConfirmed) {
                    anchor->right = round.pupilInfoRight[imgNo];
                    anchor->rightValid = true;
                    anchor->sourceFrame.subjectRight.detected = true;
                    anchor->sourceFrame.subjectRight.reliable = true;
                    anchor->sourceFrame.subjectRight.center =
                            anchor->right.center;
                    anchor->sourceFrame.subjectRight.radius =
                            static_cast<float>(anchor->right.radius);
                    anchor->sourceFrame.subjectRight.score = 1.0F;
                    anchor->sourceFrame.subjectRight.source =
                            PupilSource_RoiRefined;
                    anchor->sourceFrame.isSubjectRightAnchor = true;
                    setFormalAnchorConfirmedLocked(
                            roundIdx, true, anchor->right, "anchor_photo_roi_refined");
                }
                if (leftConfirmed) {
                    anchor->left = round.pupilInfoLeft[imgNo];
                    anchor->leftValid = true;
                    anchor->sourceFrame.subjectLeft.detected = true;
                    anchor->sourceFrame.subjectLeft.reliable = true;
                    anchor->sourceFrame.subjectLeft.center =
                            anchor->left.center;
                    anchor->sourceFrame.subjectLeft.radius =
                            static_cast<float>(anchor->left.radius);
                    anchor->sourceFrame.subjectLeft.score = 1.0F;
                    anchor->sourceFrame.subjectLeft.source =
                            PupilSource_RoiRefined;
                    anchor->sourceFrame.isSubjectLeftAnchor = true;
                    setFormalAnchorConfirmedLocked(
                            roundIdx, false, anchor->left, "anchor_photo_roi_refined");
                }
                anchor->sourceFrame.lampNumber = imgNo;
                anchor->sourceFrame.isAnchor =
                        (!eyeFlags.first
                         || anchor->sourceFrame.subjectRight.detected)
                        && (!eyeFlags.second
                            || anchor->sourceFrame.subjectLeft.detected);
                candidateReady = anchor->sourceFrame.isAnchor;
            }
        }
    }

    if (!candidateReady) {
        ALGO_KEY_LOG(
            qInfo().noquote()
                << QString("[DL_ANCHOR] result=failed,round=%1,img_no=%2")
                       .arg(roundIdx)
                       .arg(imgNo));
        return;
    }

    // 锚点发布前只计算一次其局部梯度；缓存失败时仍保留锚点，
    // 后续匹配会使用既有的非缓存实现，不会再次调用C800。
    PupilLightTrackerOptions cacheOptions;
    cacheOptions.trackSubjectRight =
            anchor->sourceFrame.subjectRight.detected;
    cacheOptions.trackSubjectLeft =
            anchor->sourceFrame.subjectLeft.detected;
    const bool cacheUsesSmallFrame = !anchor->smallImage.empty();
    const PupilLightFrame cacheAnchorFrame = cacheUsesSmallFrame
            ? makeFormalSmallPupilFrame(anchor->sourceFrame,
                                        anchor->image.size())
            : anchor->sourceFrame;
    cacheOptions.processingScale = cacheUsesSmallFrame
            ? 1.0F : FORMAL_TRACK_SCALE;
    cacheOptions.useFullSmallFrame = true;
    cacheOptions.maximumTemplateHalf = FORMAL_LIGHT_TEMPLATE_HALF;
    cacheOptions.maximumSearchMargin = FORMAL_LIGHT_SEARCH_MARGIN;
    cacheOptions.maximumMovementPadding = FORMAL_LIGHT_MOVEMENT_PADDING;
    cacheOptions.minimumMatchScore = FORMAL_TRACK_RELIABLE_SCORE;
    PupilLightTracker tracker;
    std::string cacheError;
    anchor->trackerCacheReady = tracker.prepareAnchorCache(
            cacheUsesSmallFrame ? anchor->smallImage : anchor->image,
            cacheAnchorFrame, cacheOptions,
            &anchor->trackerCache, &cacheError);
    if (!anchor->trackerCacheReady) {
        qWarning().noquote()
                << QString("FormalAsyncAnchorCache: round=%1,photo=%2,"
                           "prepared=no,error=%3")
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(imgNo))
                   .arg(QString::fromStdString(cacheError));
    }

    bool published = false;
    bool masterPublished = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (roundIdx >= 0 && roundIdx < MAX_ROUNDS) {
            MeasurementRound& round = m_rounds[roundIdx];
            FormalAsyncRoundState& asyncState = round.asyncState;
            const bool generationOk =
                    asyncState.measurementGeneration == measurementGeneration
                    && asyncState.roundGeneration == roundGeneration;
            if (generationOk && !asyncState.anchorReady) {
                asyncState.anchor = anchor;
                asyncState.anchorReady = true;
                published = true;
                // 第一份完成C800+129 ROI确认的不可变锚点成为整次测量
                // 的固定主模板；后续照片不得再发布或替换模板。
                if (!m_formalMasterAnchorReady
                        && measurementGeneration != 0) {
                    m_formalMasterAnchor = anchor;
                    m_formalMasterAnchorReady = true;
                    ++m_formalMasterAnchorGeneration;
                    m_formalMasterAnchorMeasurementGeneration =
                            measurementGeneration;
                    m_formalMasterAnchorRound = roundIdx;
                    m_formalMasterAnchorImgNo = imgNo;
                    masterPublished = true;
                }
            }
        }
    }

#if ENABLE_ALGO_VERBOSE_LOG
    if (published) {
        qDebug().noquote()
                << QString("[DL_ANCHOR] result=published,round=%1,"
                           "photo=%2,immutable=yes")
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(imgNo));
    }
    if (masterPublished) {
        qInfo().noquote()
                << QString("[DL_MASTER_ANCHOR] action=published round=%1 img=%2")
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(imgNo));
    }
#endif
}

void CAlgo::processOneFormalAsyncFrame(
        int roundIdx,
        int imgNo,
        const cv::Mat& image,
        const cv::Mat& smallImage,
        const std::shared_ptr<const FormalPupilAnchor>& anchor,
        std::uint64_t measurementGeneration,
        std::uint64_t roundGeneration)
{
#if ENABLE_ALGO_TIMING_LOG
    // 异步线程必须显式绑定所属正式轮次，否则事件会因上下文无效而被丢弃。
    AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
#endif
    // 异步正式任务绑定本轮失败详情预算，避免逐照片失败日志刷屏。
    PupilFailDetailRoundLogScope pupilFailDetailLogScope(
            &m_rounds[roundIdx].pupilFailDetailLogCount);

    const auto generationIsCurrent = [this, roundIdx, measurementGeneration,
                                      roundGeneration]() {
        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)
                || resultState != calcResultState_Succ
                || m_formalMeasurementGeneration.load(
                        std::memory_order_acquire) != measurementGeneration) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
            return false;
        }
        const FormalAsyncRoundState& asyncState =
                m_rounds[roundIdx].asyncState;
        return asyncState.measurementGeneration == measurementGeneration
                && asyncState.roundGeneration == roundGeneration;
    };

    if (!anchor || image.empty() || !generationIsCurrent()) {
        return;
    }

    const auto eyeFlags = get_eye_flags(m_eye);
    PupilLightTracker tracker;
    const bool useCachedSmallFrame = !smallImage.empty()
            && !anchor->smallImage.empty();
    const cv::Mat& trackerAnchorImage = useCachedSmallFrame
            ? anchor->smallImage : anchor->image;
    const cv::Mat& trackerTargetImage = useCachedSmallFrame
            ? smallImage : image;
    const PupilLightFrame trackerAnchorFrame = useCachedSmallFrame
            ? makeFormalSmallPupilFrame(anchor->sourceFrame,
                                        anchor->image.size())
            : anchor->sourceFrame;
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
    // 正式异步路径每张照片都以固定锚点为输入，不把上一张正式照片
    // 的Match结果传给下一张；该日志用于板端确认实际锚点来源。
    qInfo().noquote()
            << QString("[DL_FIXED_ANCHOR_MATCH] round=%1 target_img=%2 "
                       "anchor_type=%3 anchor_img=%4 independent=yes "
                       "cached=%5")
               .arg(roundIdx)
               .arg(formalCaptureNumber(imgNo))
               .arg("master")
               .arg(formalCaptureNumber(anchor->lampNumber))
               .arg(anchor->trackerCacheReady ? "yes" : "no");
#endif
    PupilLightTrackerOptions options;
    options.trackSubjectRight = eyeFlags.first
            && anchor->sourceFrame.subjectRight.detected;
    options.trackSubjectLeft = eyeFlags.second
            && anchor->sourceFrame.subjectLeft.detected;
    options.processingScale = useCachedSmallFrame
            ? 1.0F : FORMAL_TRACK_SCALE;
    // 跨轮来源和本轮锚点统一使用400×160整图梯度；眼别限制只在匹配窗口生效。
    options.useFullSmallFrame = true;
    options.maximumTemplateHalf = FORMAL_LIGHT_TEMPLATE_HALF;
    options.maximumSearchMargin = FORMAL_LIGHT_SEARCH_MARGIN;
    options.maximumMovementPadding = FORMAL_LIGHT_MOVEMENT_PADDING;
    options.minimumMatchScore = FORMAL_TRACK_RELIABLE_SCORE;

    PupilLightFrame tracked;
    PupilLightTrackerSummary summary;
    std::string error;
    const auto begin = std::chrono::steady_clock::now();
    const bool trackedOk = anchor->trackerCacheReady
            ? tracker.trackOneFrameFromAnchorCached(
                    trackerAnchorImage, trackerAnchorFrame,
                    trackerTargetImage, imgNo,
                    options, anchor->trackerCache, &tracked, &summary,
                    &error)
            : tracker.trackOneFrameFromAnchor(
                    trackerAnchorImage, trackerAnchorFrame,
                    trackerTargetImage, imgNo,
                    options, &tracked, &summary, &error);
    if (useCachedSmallFrame) {
        mapFormalSmallFrameToOriginal(&tracked, image.size());
    }
    const double elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
#if ENABLE_ALGO_TIMING_LOG
    // 当前函数每进入一次就代表一次正式小图Match，成功和失败都要统计。
    AlgoTiming::recordMilliseconds(AlgoTimingStage_FormalSmallMatch, elapsedMs);
#endif
    if (!generationIsCurrent()) {
        return;
    }

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    // score=-1且match未执行时归为not_attempted；执行后低于阈值才归为
    // score_below_threshold，避免把两类问题混在一起。
    auto effectiveMatchReason = [](const PupilLightMatchDiagnostic& diagnostic) {
        return !diagnostic.matchExecuted
                && diagnostic.reason == PupilLightMatch_None
                ? PupilLightMatch_NotAttempted : diagnostic.reason;
    };
    PupilLightMatchReason rightReason = PupilLightMatch_None;
    PupilLightMatchReason leftReason = PupilLightMatch_None;
    int rightReasonOccurrence = 0;
    int leftReasonOccurrence = 0;
    auto logSmallMatchDiagnostic = [&](const char* eyeName,
                                        const PupilLightEye& eye,
                                        PupilLightMatchReason reason,
                                        int reasonOccurrence) {
        Q_UNUSED(reasonOccurrence);
        // 成功不输出逐照片日志；失败详情统一服从整轮两条预算。
        if (reason == PupilLightMatch_None || !shouldLogPupilFailDetail()) {
            return;
        }
        const PupilLightMatchDiagnostic& diagnostic = eye.matchDiagnostic;
        QString message = QString("[DL_SMALL_MATCH_FAIL] round=%1 img=%2 eye=%3 "
                                  "reason=%4 score=%5 threshold=%6 match=%7")
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(imgNo))
                   .arg(eyeName)
                   .arg(pupilLightMatchReasonName(reason))
                   .arg(eye.score, 0, 'f', 3)
                   .arg(diagnostic.threshold, 0, 'f', 3)
                   .arg(diagnostic.matchExecuted ? "executed" : "not_executed");
        if (reason != PupilLightMatch_ScoreBelowThreshold) {
            message += QString(" original_center=%1 padded_center=%2 radius=%3 "
                               "template=%4 search=%5 eye_bounds=%6 "
                               "padded_search_bounds=%7 gradient_roi=%8 "
                               "template_size=%9 search_size=%10 "
                               "vertical_padding=%11 edge_padding_used=%12 "
                               "matched_center_padded=%13 matched_center_original=%14")
                    .arg(formatSmallMatchPoint(diagnostic.previousCenter))
                    .arg(formatSmallMatchPoint(diagnostic.paddedCenter))
                    .arg(diagnostic.previousRadius, 0, 'f', 1)
                    .arg(formatSmallMatchRect(diagnostic.templateRect))
                    .arg(formatSmallMatchRect(diagnostic.searchRect))
                    .arg(formatSmallMatchRect(diagnostic.searchBounds))
                    .arg(formatSmallMatchRect(diagnostic.paddedSearchBounds))
                    .arg(formatSmallMatchRect(diagnostic.gradientRoi))
                    .arg(formatSmallMatchSize(diagnostic.templateSize))
                    .arg(formatSmallMatchSize(diagnostic.searchSize))
                    .arg(diagnostic.verticalPadding)
                    .arg(diagnostic.edgePaddingUsed ? "yes" : "no")
                    .arg(formatSmallMatchPoint(diagnostic.matchedCenterPadded))
                    .arg(formatSmallMatchPoint(diagnostic.matchedCenterOriginal));
        }
        qWarning().noquote() << message;
    };

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        FormalAsyncRoundState& asyncState = m_rounds[roundIdx].asyncState;
        auto countDiagnostic = [](std::array<int, PupilLightMatchReason_Count>& counts,
                                  PupilLightMatchReason reason) {
            const int index = static_cast<int>(reason);
            if (index >= 0 && index < PupilLightMatchReason_Count) {
                ++counts[static_cast<size_t>(index)];
                return counts[static_cast<size_t>(index)];
            }
            return 0;
        };
        if (eyeFlags.first && options.trackSubjectRight) {
            rightReason = effectiveMatchReason(
                    tracked.subjectRight.matchDiagnostic);
            rightReasonOccurrence = countDiagnostic(
                    asyncState.smallMatchRightCounts, rightReason);
        }
        if (eyeFlags.second && options.trackSubjectLeft) {
            leftReason = effectiveMatchReason(
                    tracked.subjectLeft.matchDiagnostic);
            leftReasonOccurrence = countDiagnostic(
                    asyncState.smallMatchLeftCounts, leftReason);
        }
    }
    if (eyeFlags.first && options.trackSubjectRight) {
        logSmallMatchDiagnostic("right", tracked.subjectRight,
                                rightReason, rightReasonOccurrence);
    }
    if (eyeFlags.second && options.trackSubjectLeft) {
        logSmallMatchDiagnostic("left", tracked.subjectLeft,
                                leftReason, leftReasonOccurrence);
    }
#endif

    // 正式小图模式下必须同时满足detected和reliable；低可信坐标不能
    // 映射回原图，更不能进入129 ROI和DS输入。
    const bool rightLocated = eyeFlags.first
            && tracked.subjectRight.detected
            && tracked.subjectRight.reliable;
    const bool leftLocated = eyeFlags.second
            && tracked.subjectLeft.detected
            && tracked.subjectLeft.reliable;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        FormalAsyncRoundState& asyncState = m_rounds[roundIdx].asyncState;
        const bool allMasterEyesLocated =
                (!eyeFlags.first || rightLocated)
                && (!eyeFlags.second || leftLocated);
        if (allMasterEyesLocated) {
            ++asyncState.masterAnchorMatchSuccess;
        } else {
            ++asyncState.masterAnchorMatchFailed;
        }
    }
#if ENABLE_ALGO_TIMING_LOG
    // 进入本函数即表示当前照片执行了小图Match。
    // C800输入照片不会进入该函数，不能通过照片编号判断是否属于Match路径。
    if (eyeFlags.first) {
        AlgoTiming::event(rightLocated
                              ? AlgoTimingEvent_FormalSmallMatchSuccess
                              : AlgoTimingEvent_FormalSmallMatchFailure);
    }
    if (eyeFlags.second) {
        AlgoTiming::event(leftLocated
                             ? AlgoTimingEvent_FormalSmallMatchSuccess
                             : AlgoTimingEvent_FormalSmallMatchFailure);
    }
#endif
    const stPupilInfo pupilRight = rightLocated
            ? makeTrackedPupilInfo(tracked.subjectRight) : stPupilInfo{};
    const stPupilInfo pupilLeft = leftLocated
            ? makeTrackedPupilInfo(tracked.subjectLeft) : stPupilInfo{};

#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
    qDebug().noquote()
            << QString("FormalAsyncFrame: round=%1,photo=%2,scheduled=yes,"
                       "tracked=%3,right=%4,left=%5,reliable_right=%6,"
                       "reliable_left=%7,elapsed_ms=%8,error=%9,"
                       "eye_vector_retry=%10")
               .arg(roundIdx)
               .arg(formalCaptureNumber(imgNo))
               .arg(trackedOk ? "yes" : "no")
               .arg(rightLocated ? "yes" : "no")
               .arg(leftLocated ? "yes" : "no")
               .arg(tracked.subjectRight.reliable ? "yes" : "no")
               .arg(tracked.subjectLeft.reliable ? "yes" : "no")
               .arg(elapsedMs, 0, 'f', 2)
               .arg(QString::fromStdString(error))
               .arg(summary.eyeVectorRetryCount);
#else
    const bool allTrackedEyesReliable =
            (!eyeFlags.first || rightLocated)
            && (!eyeFlags.second || leftLocated);
    if (!trackedOk || !allTrackedEyesReliable) {
        // 正式帧失败摘要与Small Match、129 ROI共用整轮日志预算。
        ALGO_ERROR_LOG(
            if (shouldLogPupilFailDetail()) {
                qWarning().noquote()
                        << QString("FormalAsyncFrame: round=%1,photo=%2,scheduled=yes,"
                                   "tracked=%3,right=%4,left=%5,reliable_right=%6,"
                                   "reliable_left=%7,elapsed_ms=%8,error=%9,"
                                   "eye_vector_retry=%10")
                           .arg(roundIdx)
                           .arg(formalCaptureNumber(imgNo))
                           .arg(trackedOk ? "yes" : "no")
                           .arg(rightLocated ? "yes" : "no")
                           .arg(leftLocated ? "yes" : "no")
                           .arg(tracked.subjectRight.reliable ? "yes" : "no")
                           .arg(tracked.subjectLeft.reliable ? "yes" : "no")
                           .arg(elapsedMs, 0, 'f', 2)
                           .arg(QString::fromStdString(error))
                           .arg(summary.eyeVectorRetryCount);
            }
        );
    }
#endif
    processAndStoreLocatedFrame(roundIdx, imgNo, image,
                                rightLocated, pupilRight,
                                leftLocated, pupilLeft,
                                measurementGeneration, roundGeneration,
                                false);
    if (!generationIsCurrent()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        MeasurementRound& round = m_rounds[roundIdx];
        const bool rightLowConfidence = eyeFlags.first
                && tracked.subjectRight.detected
                && !tracked.subjectRight.reliable
                && round.validRight.test(imgNo);
        const bool leftLowConfidence = eyeFlags.second
                && tracked.subjectLeft.detected
                && !tracked.subjectLeft.reliable
                && round.validLeft.test(imgNo);
        if (rightLowConfidence) {
            round.lowConfidenceRight.set(imgNo);
        }
        if (leftLowConfidence) {
            round.lowConfidenceLeft.set(imgNo);
        }
        if (eyeFlags.first) {
            if (round.validRight.test(imgNo)) {
                setFormalAnchorConfirmedLocked(
                        roundIdx, true, round.pupilInfoRight[imgNo],
                        "tracking_roi_refined");
            } else {
                ++round.asyncState.rightAnchor.consecutiveFailureCount;
                ALGO_ERROR_LOG(
                    if (shouldLogPupilFailDetail()) {
                        qWarning().noquote()
                                << QString("FormalPupilMissing: round=%1,photo=%2,eye=right,"
                                           "reason=%3,haar_fallback=disabled")
                                   .arg(roundIdx)
                                   .arg(formalCaptureNumber(imgNo))
                                   .arg(anchor->sourceFrame.subjectRight.detected
                                            ? "roi_refine_failed_no_fallback"
                                            : "missing_no_anchor");
                    }
                );
            }
        }
        if (eyeFlags.second) {
            if (round.validLeft.test(imgNo)) {
                setFormalAnchorConfirmedLocked(
                        roundIdx, false, round.pupilInfoLeft[imgNo],
                        "tracking_roi_refined");
            } else {
                ++round.asyncState.leftAnchor.consecutiveFailureCount;
                ALGO_ERROR_LOG(
                    if (shouldLogPupilFailDetail()) {
                        qWarning().noquote()
                                << QString("FormalPupilMissing: round=%1,photo=%2,eye=left,"
                                           "reason=%3,haar_fallback=disabled")
                                   .arg(roundIdx)
                                   .arg(formalCaptureNumber(imgNo))
                                   .arg(anchor->sourceFrame.subjectLeft.detected
                                            ? "roi_refine_failed_no_fallback"
                                            : "missing_no_anchor");
                    }
                );
            }
        }
        round.frames[imgNo].processed = true;
        round.asyncState.processedFrames.set(imgNo);
        round.asyncState.lastProcessedAt = std::chrono::steady_clock::now();
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
        const int processedCount = static_cast<int>(
                round.asyncState.processedFrames.count());
        qDebug().noquote()
                << QString("FormalAsyncQueue: round=%1,photo=%2,received=%3,"
                           "scheduled=%4,processed=%5,active=%6")
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(imgNo))
                   .arg(static_cast<int>(round.asyncState.receivedFrames.count()))
                   .arg(static_cast<int>(round.asyncState.scheduledFrames.count()))
                   .arg(processedCount)
                   .arg(m_formalAsyncTasksInFlight.load(
                           std::memory_order_acquire));
#endif
    }
}

void CAlgo::activateWaitingFormalRounds()
{
    std::vector<int> activatedRounds;
    int activeRoundCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(
                        std::memory_order_acquire)) {
            return;
        }

        for (int roundIdx = 0;
             roundIdx < MAX_ROUNDS && m_activeFormalRounds.size() < 2;
             ++roundIdx) {
            const FormalAsyncRoundState& state =
                    m_rounds[roundIdx].asyncState;
            if ((state.receivedFrames.none() && !state.inputFinished)
                    || state.earlyRetryRequested
                    || state.settlementCompletedOnce
                    || state.settlementCompleted) {
                continue;
            }
            bool alreadyActive = false;
            for (const int activeRound : m_activeFormalRounds) {
                if (activeRound == roundIdx) {
                    alreadyActive = true;
                    break;
                }
            }
            if (alreadyActive) {
                continue;
            }
            m_activeFormalRounds.push_back(roundIdx);
            activatedRounds.push_back(roundIdx);
        }
        m_activeFormalProcessingRound = m_activeFormalRounds.empty()
                ? -1 : m_activeFormalRounds.front();
        activeRoundCount = static_cast<int>(m_activeFormalRounds.size());
    }

#if ENABLE_ALGO_VERBOSE_LOG
    for (const int roundIdx : activatedRounds) {
        qInfo().noquote()
                   << QString("[DL_INTERLEAVE_ACTIVATE] round=%1 "
                              "active_rounds=%2")
                   .arg(roundIdx)
                   .arg(activeRoundCount);
    }
#endif
}

void CAlgo::pumpFormalInterleavedFrames()
{
    activateWaitingFormalRounds();

    std::vector<int> activeRounds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const int activeCount = static_cast<int>(m_activeFormalRounds.size());
        if (activeCount == 0) {
            return;
        }
        activeRounds.assign(m_activeFormalRounds.begin(),
                            m_activeFormalRounds.end());
    }

    // 按轮次升序分配全部可用槽位：最早未结算轮次优先获得三个槽位；
    // 只有该轮当前没有足够的待处理照片时，剩余槽位才自然交给下一轮。
    std::sort(activeRounds.begin(), activeRounds.end());
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
    struct ScheduleLog {
        int roundIdx = -1;
        int scheduledNow = 0;
        int inFlightRound = 0;
        int inFlightTotal = 0;
    };
    std::vector<ScheduleLog> scheduleLogs;
#endif
    int scheduledPhotoTasks = 0;
    for (const int activeRound : activeRounds) {
        if (scheduledPhotoTasks
                >= FORMAL_MAX_CONCURRENT_PHOTO_TASKS) {
            break;
        }
        int scheduledForRound = 0;
        schedulePendingFormalFrames(
                activeRound,
                FORMAL_MAX_CONCURRENT_PHOTO_TASKS,
                &scheduledForRound);
        scheduledPhotoTasks += scheduledForRound;
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
        if (scheduledForRound > 0) {
            ScheduleLog scheduleLog;
            scheduleLog.roundIdx = activeRound;
            scheduleLog.scheduledNow = scheduledForRound;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                scheduleLog.inFlightRound =
                        m_rounds[activeRound].asyncState.photoTasksInFlight;
                scheduleLog.inFlightTotal = m_formalPhotoTasksInFlight;
            }
            scheduleLogs.push_back(scheduleLog);
        }
#endif
    }
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
    for (const ScheduleLog& scheduleLog : scheduleLogs) {
        qInfo().noquote()
                << QString("[DL_INTERLEAVE_SCHEDULE] priority=oldest_first "
                           "round=%1 scheduled_now=%2 in_flight_round=%3 "
                           "in_flight_total=%4")
                   .arg(scheduleLog.roundIdx)
                   .arg(scheduleLog.scheduledNow)
                   .arg(scheduleLog.inFlightRound)
                   .arg(scheduleLog.inFlightTotal);
    }
#endif
}

void CAlgo::schedulePendingFormalFrames(int roundIdx,
                                         int maxPhotoTasksForRound,
                                         int* scheduledPhotoTasks)
{
    // 调度期间产生的无锚点详情同样受本轮日志预算控制。
    PupilFailDetailRoundLogScope pupilFailDetailLogScope(
            &m_rounds[roundIdx].pupilFailDetailLogCount);

    struct PendingFrame {
        int imgNo = -1;
        cv::Mat image;
        cv::Mat smallImage;
        std::shared_ptr<const FormalPupilAnchor> anchor;
        std::uint64_t measurementGeneration = 0;
        std::uint64_t roundGeneration = 0;
    };

    std::vector<PendingFrame> pending;
    struct ModelStart {
        int imgNo = 0;
        cv::Mat image;
        cv::Mat smallImage;
        std::uint64_t measurementGeneration = 0;
        std::uint64_t roundGeneration = 0;
        std::string reason;
    };
    ModelStart modelStart;
    bool hasModelStart = false;
    bool shouldSettleNoAnchor = false;
    bool logMasterAnchorUse = false;
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
    int logActiveRoundCount = 0;
    int logInFlightRound = 0;
    int logInFlightTotal = 0;
#endif
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (roundIdx < 0 || roundIdx >= MAX_ROUNDS
                || m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
            return;
        }

        MeasurementRound& round = m_rounds[roundIdx];
        FormalAsyncRoundState& asyncState = round.asyncState;
        // 当前轮已经请求提前换轮；迟到照片只允许由外部保存，不能再进入
        // C800、Match、129 ROI或DS结算。
        if (asyncState.earlyRetryRequested) {
            return;
        }
        if (asyncState.receivedFrames.none() && !asyncState.inputFinished) {
            // 上一轮结算可能早于当前轮首张图片到达；此时只保留空状态，
            // 不提前生成本轮代际号。
            return;
        }
        bool isActiveRound = false;
        for (const int activeRound : m_activeFormalRounds) {
            if (activeRound == roundIdx) {
                isActiveRound = true;
                break;
            }
        }
        if (!isActiveRound) {
            return;
        }
        if (asyncState.measurementGeneration == 0) {
            asyncState.measurementGeneration =
                    m_formalMeasurementGeneration.load(
                            std::memory_order_acquire);
        }
        if (asyncState.roundGeneration == 0) {
            asyncState.roundGeneration =
                    m_formalRoundGenerationCounter.fetch_add(
                            1, std::memory_order_acq_rel) + 1;
        }

        // 主模板建立失败时，下一物理轮仍按原有恢复规则重新执行C800；
        // 主模板一旦建立，后续所有轮次都固定复用同一个不可变快照。
        const bool retryC800FromPreviousRound = roundIdx > 0
                && m_rounds[roundIdx - 1].asyncState.earlyRetryRequested;
        const std::uint64_t currentMeasurementGeneration =
                m_formalMeasurementGeneration.load(
                        std::memory_order_acquire);
        const bool masterAnchorReady =
                m_formalMasterAnchorReady
                && static_cast<bool>(m_formalMasterAnchor)
                && m_formalMasterAnchorMeasurementGeneration
                       == currentMeasurementGeneration;
        const std::shared_ptr<const FormalPupilAnchor> masterAnchor =
                masterAnchorReady ? m_formalMasterAnchor
                                   : std::shared_ptr<const FormalPupilAnchor>();

        // 模型任务必须覆盖C800、ROI、锚点构造和发布全过程；即使锚点
        // 已经写入缓存，只要任务尚未最终提交，其他缓存照片仍不能开始处理。
        if (asyncState.anchorTaskRunning) {
            if (!masterAnchorReady && asyncState.modelFinished) {
                qWarning().noquote()
                        << QString("FormalAsyncStateInvariantViolation: round=%1,"
                                   "reason=model_finished_while_anchor_task_running,"
                                   "action=defer")
                           .arg(roundIdx);
            }
            return;
        }

        // 同时满足全局和单轮两个上限；pump按最早轮次优先传入3，
        // 这里扣除该轮已有在途任务，保证同一轮最多三个照片任务。
        const int roundSchedulingBudget = std::max(
                0, maxPhotoTasksForRound - asyncState.photoTasksInFlight);
        int schedulingBudget = std::min(
                std::max(
                        0,
                        FORMAL_MAX_CONCURRENT_PHOTO_TASKS
                                - m_formalPhotoTasksInFlight),
                roundSchedulingBudget);
        if (schedulingBudget <= 0) {
            return;
        }

        if (masterAnchorReady) {
            if (!asyncState.masterAnchorUseLogged) {
                asyncState.masterAnchorUseLogged = true;
                logMasterAnchorUse = true;
            }
            for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
                if (static_cast<int>(pending.size()) >= schedulingBudget) {
                    break;
                }
                if (!asyncState.receivedFrames.test(imgNo)
                        || asyncState.scheduledFrames.test(imgNo)
                        || asyncState.processedFrames.test(imgNo)
                        || round.frames[imgNo].frame.empty()) {
                    continue;
                }

                PendingFrame item;
                item.imgNo = imgNo;
                item.image = round.frames[imgNo].frame;
                item.smallImage = round.frames[imgNo].smallFrame;
                item.anchor = masterAnchor;
                item.measurementGeneration = asyncState.measurementGeneration;
                item.roundGeneration = asyncState.roundGeneration;
                asyncState.scheduledFrames.set(imgNo);
                ++asyncState.photoTasksInFlight;
                ++m_formalPhotoTasksInFlight;
#if ENABLE_ALGO_TIMING_LOG
                // 只记录照片任务并发峰值，避免为每个任务输出一条日志。
                AlgoTiming::recordFormalPhotoTasksInFlight(
                        m_formalPhotoTasksInFlight);
#endif
                pending.push_back(std::move(item));
                if (scheduledPhotoTasks != nullptr) {
                    ++*scheduledPhotoTasks;
                }
            }
        } else if (roundIdx == 0 || retryC800FromPreviousRound) {
            const int modelImgNo = asyncState.firstReceivedImgNo;
            const bool modelFrameReady = modelImgNo >= 1
                    && modelImgNo <= FRAMES_PER_ROUND
                    && asyncState.receivedFrames.test(modelImgNo)
                    && !round.frames[modelImgNo].frame.empty();
            if ((roundIdx == 0 || retryC800FromPreviousRound)
                    && modelFrameReady
                    && !asyncState.modelAttempted) {
                // 首轮或上一轮锚点提前失败后的重试轮，只在最早到达的
                // 有效照片上执行一次C800；其余照片等待锚点后再处理。
                asyncState.modelInputImgNo = modelImgNo;
                asyncState.scheduledFrames.set(modelImgNo);
                modelStart.imgNo = modelImgNo;
                modelStart.image = round.frames[modelImgNo].frame;
                modelStart.smallImage = round.frames[modelImgNo].smallFrame;
                modelStart.measurementGeneration = asyncState.measurementGeneration;
                modelStart.roundGeneration = asyncState.roundGeneration;
                modelStart.reason = retryC800FromPreviousRound
                        ? "anchor_retry" : "initial_anchor";
                asyncState.modelAttempted = true;
                asyncState.anchorTaskRunning = true;
                asyncState.modelFinished = false;
                m_formalModelAttempted.store(true, std::memory_order_release);
                m_formalModelFinished.store(false, std::memory_order_release);
                hasModelStart = true;
            }
        } else {
            // 主模板尚未建立且当前轮不是C800恢复轮；等待上一轮恢复结果，
            // 不再查询或绑定逐灯位历史模板。
            bool earlierRoundsFinished = true;
            for (int previousRound = 0; previousRound < roundIdx;
                 ++previousRound) {
                const FormalAsyncRoundState& previousState =
                        m_rounds[previousRound].asyncState;
                if (previousState.earlyRetryRequested) {
                    continue;
                }
                if (!previousState.inputFinished
                        || previousState.anchorTaskRunning
                        || previousState.photoTasksInFlight != 0
                        || previousState.processedFrames.count()
                               != FRAMES_PER_ROUND) {
                    earlierRoundsFinished = false;
                    break;
                }
            }

            if (asyncState.inputFinished && earlierRoundsFinished
                    && !asyncState.modelAttempted
                    && asyncState.photoTasksInFlight == 0
                    && !m_formalC800TaskRunning) {
                // 主模板缺失且没有可恢复的C800任务时，保留原有无锚点
                // 冲刷逻辑，保证失败测量能够正常收尾。
                asyncState.modelAttempted = true;
                asyncState.modelFinished = true;
                asyncState.modelInputImgNo = 0;
                for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
                    if (!asyncState.receivedFrames.test(imgNo)
                            || asyncState.processedFrames.test(imgNo)
                            || masterAnchorReady) {
                        continue;
                    }
                    asyncState.scheduledFrames.set(imgNo);
                    asyncState.processedFrames.set(imgNo);
                    round.frames[imgNo].processed = true;
                    round.frames[imgNo].pupilDetected = false;
                    round.frames[imgNo].failureReason = "missing_no_anchor";
                }
                if (asyncState.processedFrames.count() == FRAMES_PER_ROUND
                        && !asyncState.settlementReady) {
                    asyncState.settlementReady = true;
                    shouldSettleNoAnchor = true;
                }
            }
        }

        const bool anchorAttemptReallyFinished =
                asyncState.modelAttempted
                && asyncState.modelFinished
                && !asyncState.anchorTaskRunning;
        const bool previousRoundSettledOrRetry = roundIdx == 0
                || retryC800FromPreviousRound
                || m_rounds[roundIdx - 1].asyncState.settlementCompletedOnce
                || m_rounds[roundIdx - 1].asyncState.earlyRetryRequested;
        if ((roundIdx == 0 || retryC800FromPreviousRound)
                && !masterAnchorReady
                && anchorAttemptReallyFinished
                && previousRoundSettledOrRetry) {
            const auto eyeFlags = get_eye_flags(m_eye);
            const bool rightAvailable =
                    asyncState.rightAnchor.state != FormalAnchor_NoAnchor;
            const bool leftAvailable =
                    asyncState.leftAnchor.state != FormalAnchor_NoAnchor;
            const char* missingReason = (!rightAvailable && !leftAvailable)
                    ? "anchor_failed_both_eyes"
                    : "missing_no_anchor";

            // 无锚点冲刷只能发生在C800任务完整结束后；先统计将要冲刷的
            // 缓存数量，再打印诊断，便于确认没有提前结算缓存照片。
            const int receivedCount = asyncState.receivedFrames.count();
            const int alreadyProcessedCount =
                    asyncState.processedFrames.count();
            int flushableCount = 0;
            for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
                if (asyncState.receivedFrames.test(imgNo)
                        && !asyncState.processedFrames.test(imgNo)) {
                    ++flushableCount;
                }
            }
            qWarning().noquote()
                    << QString("FormalAsyncNoAnchorFlush: round=%1,"
                               "model_input_img_no=%2,received=%3,"
                               "already_processed=%4,flushed=%5,"
                               "model_finished=yes,anchor_task_running=no,"
                               "anchor_ready=no")
                       .arg(roundIdx)
                       .arg(asyncState.modelInputImgNo)
                       .arg(receivedCount)
                       .arg(alreadyProcessedCount)
                       .arg(flushableCount);
            for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
                if (!asyncState.receivedFrames.test(imgNo)
                        || asyncState.processedFrames.test(imgNo)) {
                    continue;
                }
                asyncState.scheduledFrames.set(imgNo);
                asyncState.processedFrames.set(imgNo);
                round.frames[imgNo].processed = true;
                round.frames[imgNo].pupilDetected = false;
                round.frames[imgNo].failureReason = missingReason;
                if (eyeFlags.first && !rightAvailable) {
                    ALGO_ERROR_LOG(
                        if (shouldLogPupilFailDetail()) {
                            qWarning().noquote()
                                    << QString("FormalPupilMissing: round=%1,photo=%2,"
                                               "eye=right,reason=%3,haar_fallback=disabled")
                                       .arg(roundIdx)
                                       .arg(formalCaptureNumber(imgNo))
                                       .arg(missingReason);
                        }
                    );
                }
                if (eyeFlags.second && !leftAvailable) {
                    ALGO_ERROR_LOG(
                        if (shouldLogPupilFailDetail()) {
                            qWarning().noquote()
                                    << QString("FormalPupilMissing: round=%1,photo=%2,"
                                               "eye=left,reason=%3,haar_fallback=disabled")
                                       .arg(roundIdx)
                                       .arg(formalCaptureNumber(imgNo))
                                       .arg(missingReason);
                        }
                    );
                }
            }
            if (asyncState.inputFinished
                    && asyncState.processedFrames.count() == FRAMES_PER_ROUND
                    && !asyncState.settlementReady) {
                asyncState.settlementReady = true;
                asyncState.lastProcessedAt = std::chrono::steady_clock::now();
                shouldSettleNoAnchor = true;
            }
        }
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
        logActiveRoundCount = static_cast<int>(m_activeFormalRounds.size());
        logInFlightRound = asyncState.photoTasksInFlight;
        logInFlightTotal = m_formalPhotoTasksInFlight;
#endif
    }

#if ENABLE_ALGO_VERBOSE_LOG
    if (logMasterAnchorUse) {
        // 日志在锁外输出，每个轮次最多一次。
        qInfo().noquote()
                << QString("[DL_MASTER_ANCHOR] action=used target_round=%1")
                   .arg(roundIdx);
    }
#endif

    if (hasModelStart) {
#if ENABLE_ALGO_VERBOSE_LOG
        qDebug().noquote()
                << QString("FormalAsyncModel: queued,round=%1,img_no=%2,"
                           "capture_no=%3,round_attempt=1,reason=%4")
                   .arg(roundIdx)
                   .arg(modelStart.imgNo)
                   .arg(formalCaptureNumber(modelStart.imgNo))
                   .arg(QString::fromStdString(modelStart.reason));
#endif
        requestFormalC800Task(roundIdx, modelStart.imgNo, modelStart.image,
                              modelStart.smallImage,
                              modelStart.measurementGeneration,
                              modelStart.roundGeneration,
                              modelStart.reason);
    }

    for (PendingFrame item : pending) {
        m_formalAsyncTasksInFlight.fetch_add(1, std::memory_order_acq_rel);
        const auto taskScope = std::make_shared<FormalAsyncTaskScope>(
                &m_formalAsyncTasksInFlight, &m_formalAsyncWaitCondition);
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
        int inFlightRound = 0;
        int inFlightTotal = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            inFlightRound = m_rounds[roundIdx].asyncState.photoTasksInFlight;
            inFlightTotal = m_formalPhotoTasksInFlight;
        }
        qInfo().noquote()
                << QString("[DL_INTERLEAVE_FRAME] round=%1 img=%2 "
                           "action=queued in_flight_round=%3 "
                           "in_flight_total=%4")
                   .arg(roundIdx)
                   .arg(formalCaptureNumber(item.imgNo))
                   .arg(inFlightRound)
                   .arg(inFlightTotal);
#endif
        pool->start(new QRunnableFunction(
                [this, roundIdx, item = std::move(item), taskScope]() mutable {
            Q_UNUSED(taskScope);
            FormalScopeExit taskGuard([this, roundIdx,
                                       measurementGeneration =
                                               item.measurementGeneration,
                                       roundGeneration = item.roundGeneration]() {
                completeFormalPhotoTask(roundIdx, measurementGeneration,
                                        roundGeneration);
            });
            processOneFormalAsyncFrame(roundIdx, item.imgNo, item.image,
                    item.smallImage, item.anchor,
                    item.measurementGeneration, item.roundGeneration);
        }));
    }

    if (shouldSettleNoAnchor) {
        scheduleNextReadyFormalSettlement();
    }

#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
    if (!pending.empty() || shouldSettleNoAnchor) {
        qDebug().noquote()
                << QString("FormalAsyncQueue: round=%1,scheduled_now=%2,"
                           "active=%3,in_flight_round=%4,in_flight_total=%5")
                   .arg(roundIdx)
                   .arg(static_cast<int>(pending.size()))
                   .arg(logActiveRoundCount)
                   .arg(logInFlightRound)
                   .arg(logInFlightTotal);
    }
#endif
}

void CAlgo::completeFormalPhotoTask(
        int roundIdx,
        std::uint64_t measurementGeneration,
        std::uint64_t roundGeneration)
{
    bool settlementReadyNow = false;
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
    int inFlightRound = 0;
    int inFlightTotal = 0;
#endif
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_formalPhotoTasksInFlight > 0) {
            --m_formalPhotoTasksInFlight;
        }
        if (roundIdx >= 0 && roundIdx < MAX_ROUNDS) {
            FormalAsyncRoundState& state = m_rounds[roundIdx].asyncState;
            if (state.measurementGeneration == measurementGeneration
                    && state.roundGeneration == roundGeneration
                    && state.photoTasksInFlight > 0) {
                --state.photoTasksInFlight;
                if (state.inputFinished
                        && !state.earlyRetryRequested
                        && state.processedFrames.count()
                               == FRAMES_PER_ROUND
                        && state.photoTasksInFlight == 0
                        && !state.settlementReady) {
                    state.settlementReady = true;
                    state.lastProcessedAt =
                            std::chrono::steady_clock::now();
                    settlementReadyNow = true;
                }
            }
#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
            inFlightRound = state.photoTasksInFlight;
            inFlightTotal = m_formalPhotoTasksInFlight;
#endif
        }
    }

#if ENABLE_DL_FORMAL_NORMAL_DETAIL_LOG
    qInfo().noquote()
            << QString("[DL_INTERLEAVE_FRAME] round=%1 action=finished "
                       "in_flight_round=%2 in_flight_total=%3")
               .arg(roundIdx)
               .arg(inFlightRound)
               .arg(inFlightTotal);
#endif
    if (settlementReadyNow) {
        scheduleNextReadyFormalSettlement();
    }
    // 释放一个照片槽位后统一激活和轮询调度，保证两轮交错。
    pumpFormalInterleavedFrames();
}

void CAlgo::scheduleNextReadyFormalSettlement()
{
    int settlementRound = -1;
    std::uint64_t measurementGeneration = 0;
    std::uint64_t roundGeneration = 0;
    bool settlementAlreadyRunning = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(
                        std::memory_order_acquire)) {
            return;
        }
        if (m_formalSettlementTaskRunning) {
            settlementAlreadyRunning = true;
        } else {
            // 只在已经ready的轮次中选择最小编号；更早轮未ready不阻塞后续轮。
            for (int candidate = 0; candidate < MAX_ROUNDS; ++candidate) {
                FormalAsyncRoundState& state =
                        m_rounds[candidate].asyncState;
                if (state.settlementCompletedOnce
                        || state.settlementCompleted
                        || state.earlyRetryRequested
                        || !state.settlementReady
                        || state.settlementQueued) {
                    continue;
                }
                state.settlementQueued = true;
                state.settlementStarted = true;
                m_formalSettlementTaskRunning = true;
                settlementRound = candidate;
                measurementGeneration = state.measurementGeneration;
                roundGeneration = state.roundGeneration;
                break;
            }
        }
    }

#if ENABLE_ALGO_VERBOSE_LOG
    if (settlementAlreadyRunning) {
        qInfo().noquote()
                << QString("[DL_DS_SETTLEMENT] action=wait_running");
        return;
    }
#else
    if (settlementAlreadyRunning) {
        return;
    }
#endif
    if (settlementRound < 0) {
        return;
    }
#if ENABLE_ALGO_VERBOSE_LOG
    qInfo().noquote()
            << QString("[DL_DS_SETTLEMENT] round=%1 action=ready")
               .arg(settlementRound);
#endif
    enqueueFormalAsyncSettlement(settlementRound, measurementGeneration,
                                 roundGeneration);
}

void CAlgo::finishFormalRoundInput(const int roundIdx)
{
    if (roundIdx < 0 || roundIdx >= MAX_ROUNDS) {
        return;
    }

    // 缺图可能同时涉及多个灯位和双眼，统一使用本轮两条详情预算。
    PupilFailDetailRoundLogScope pupilFailDetailLogScope(
            &m_rounds[roundIdx].pupilFailDetailLogCount);

    int receivedCount = 0;
    int missingCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        MeasurementRound& round = m_rounds[roundIdx];
        if (round.asyncState.measurementGeneration == 0) {
            round.asyncState.measurementGeneration =
                    m_formalMeasurementGeneration.load(
                            std::memory_order_acquire);
        }
        if (round.asyncState.roundGeneration == 0) {
            round.asyncState.roundGeneration =
                    m_formalRoundGenerationCounter.fetch_add(
                            1, std::memory_order_acq_rel) + 1;
        }
        if (round.asyncState.earlyRetryRequested) {
            // 提前换轮不进入本轮正常DS结算，也不提交跨轮源；仅记录
            // 输入已结束，供当前残轮的保存和下一物理轮继续使用。
            round.asyncState.inputFinished = true;
            round.asyncState.lastProcessedAt =
                    std::chrono::steady_clock::now();
            writeRoundAlgoStatusLocked(
                    roundIdx,
                    QStringLiteral("anchor_failed_early_retry"),
                    QStringLiteral("round_input_finished_without_settlement"));
            qWarning().noquote()
                    << QString("FormalAsyncInputFinished: round=%1,"
                               "early_retry=yes,settlement=skipped")
                       .arg(roundIdx);
            return;
        }
        round.asyncState.inputFinished = true;
        const auto eyeFlags = get_eye_flags(m_eye);
        for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
            if (round.asyncState.receivedFrames.test(imgNo)) {
                ++receivedCount;
                continue;
            }
            ++missingCount;
            round.frames[imgNo].processed = true;
            round.frames[imgNo].pupilDetected = false;
            round.frames[imgNo].failureReason = "formal_frame_missing";
            round.asyncState.processedFrames.set(imgNo);
            if (eyeFlags.first) {
                ALGO_ERROR_LOG(
                    if (shouldLogPupilFailDetail()) {
                        qWarning().noquote()
                                << QString("FormalPupilMissing: round=%1,photo=%2,"
                                           "eye=right,reason=formal_frame_missing,"
                                           "haar_fallback=disabled")
                                   .arg(roundIdx)
                                   .arg(formalCaptureNumber(imgNo));
                    }
                );
            }
            if (eyeFlags.second) {
                ALGO_ERROR_LOG(
                    if (shouldLogPupilFailDetail()) {
                        qWarning().noquote()
                                << QString("FormalPupilMissing: round=%1,photo=%2,"
                                           "eye=left,reason=formal_frame_missing,"
                                           "haar_fallback=disabled")
                                   .arg(roundIdx)
                                   .arg(formalCaptureNumber(imgNo));
                    }
                );
            }
        }
        if (missingCount > 0) {
            round.asyncState.lastProcessedAt =
                    std::chrono::steady_clock::now();
        }

        // 整轮没有任何有效照片时没有合法C800输入；只有此时才直接
        // 进入无锚点缺失结算。只要有任意照片到达，就交给调度器选择
        // 本轮真实最早到达的照片作为C800输入。
        if (!round.asyncState.modelAttempted
                && round.asyncState.receivedFrames.none()) {
            round.asyncState.modelAttempted = true;
            round.asyncState.modelFinished = true;
            round.asyncState.modelInputImgNo = 0;
            m_formalModelAttempted.store(true, std::memory_order_release);
            m_formalModelFinished.store(true, std::memory_order_release);
            qWarning().noquote()
                    << QString("FormalAsyncModel: round=%1,model_input=none,"
                               "reason=no_valid_frame_for_anchor")
                       .arg(roundIdx);
        }
        if (round.asyncState.processedFrames.count() == FRAMES_PER_ROUND
                && round.asyncState.photoTasksInFlight == 0
                && !round.asyncState.earlyRetryRequested) {
            round.asyncState.settlementReady = true;
        }
    }

#if ENABLE_ALGO_VERBOSE_LOG
    qDebug().noquote()
            << QString("FormalAsyncInputFinished: round=%1,received=%2,missing=%3")
               .arg(roundIdx).arg(receivedCount).arg(missingCount);
#endif
#if ENABLE_ALGO_TIMING_LOG
    // 输入结束以物理轮次结束为准，即使存在缺失槽位也要记录拍摄完成时刻。
    AlgoTiming::markFormalRoundCaptureComplete(roundIdx);
#endif
    pumpFormalInterleavedFrames();
    scheduleNextReadyFormalSettlement();
}

void CAlgo::enqueueFormalAsyncSettlement(
        int roundIdx,
        std::uint64_t measurementGeneration,
        std::uint64_t roundGeneration)
{
    m_formalAsyncTasksInFlight.fetch_add(1, std::memory_order_acq_rel);
    const auto taskScope = std::make_shared<FormalAsyncTaskScope>(
            &m_formalAsyncTasksInFlight, &m_formalAsyncWaitCondition);
    pool->start(new QRunnableFunction(
            [this, roundIdx, measurementGeneration, roundGeneration,
             taskScope]() {
        Q_UNUSED(taskScope);
        FormalScopeExit settlementGuard([this]() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_formalSettlementTaskRunning = false;
            }
            scheduleNextReadyFormalSettlement();
        });
        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_formalMeasurementGeneration.load(
                        std::memory_order_acquire) != measurementGeneration) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const FormalAsyncRoundState& state = m_rounds[roundIdx].asyncState;
            if (state.measurementGeneration != measurementGeneration
                    || state.roundGeneration != roundGeneration
                    || state.earlyRetryRequested
                    || !state.settlementStarted
                    || !state.settlementQueued
                    || !state.settlementReady) {
                return;
            }
        }
#if ENABLE_ALGO_VERBOSE_LOG
        qInfo().noquote()
                << QString("[DL_DS_SETTLEMENT] round=%1 action=running")
                   .arg(roundIdx);
#endif
        finalizeFormalAsyncRound(roundIdx);
#if ENABLE_ALGO_VERBOSE_LOG
        qInfo().noquote()
                << QString("[DL_DS_SETTLEMENT] round=%1 action=completed")
                   .arg(roundIdx);
#endif
    }));
}

void CAlgo::finalizeFormalAsyncRound(int roundIdx)
{
    std::vector<cv::Mat> images(FRAMES_PER_ROUND);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (roundIdx < 0 || roundIdx >= MAX_ROUNDS
                || m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)
                || m_rounds[roundIdx].asyncState.earlyRetryRequested
                || !m_rounds[roundIdx].asyncState.inputFinished
                || m_rounds[roundIdx].asyncState.processedFrames.count()
                   != FRAMES_PER_ROUND) {
            return;
        }
        for (int imgNo = 1; imgNo <= FRAMES_PER_ROUND; ++imgNo) {
            images[imgNo - 1] = m_rounds[roundIdx].frames[imgNo].frame;
        }
    }

    // 只把最终ROI结果暂存为候选，真正是否成为下一轮源仍由既有DS结算
    // 的accepted/rejected结果决定。
    stageCrossRoundSourceCandidate(roundIdx, images);
    tryFinalizeRoundLocked(roundIdx);
    if (roundIdx == 0) {
        bool initialAnchorComplete = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto eyeFlags = get_eye_flags(m_eye);
            const FormalAsyncRoundState& asyncState =
                    m_rounds[roundIdx].asyncState;
            initialAnchorComplete = asyncState.anchorReady
                    && static_cast<bool>(asyncState.anchor)
                    && (!eyeFlags.first
                        || asyncState.rightAnchor.state
                            == FormalAnchor_Confirmed)
                    && (!eyeFlags.second
                        || asyncState.leftAnchor.state
                            == FormalAnchor_Confirmed);
        }
        if (!initialAnchorComplete) {
            emitFormalPupilNotFoundOnce(
                    roundIdx,
                    QStringLiteral("initial_anchor_not_confirmed"));
            return;
        }
    }
    bool shouldLogRound = false;
    int modelInputImgNo = 0;
    int receivedFrameCount = 0;
    int rightValidCount = 0;
    int leftValidCount = 0;
    int rightDsCount = 0;
    int leftDsCount = 0;
    int rightNativeDsCount = 0;
    int leftNativeDsCount = 0;
    bool roundAccepted = false;
    bool roundPending = false;
    bool roundRejected = false;
    int masterAnchorMatchSuccess = 0;
    int masterAnchorMatchFailed = 0;
    int failureDetailAttempts = 0;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    std::array<int, PupilLightMatchReason_Count> smallMatchRightCounts{};
    std::array<int, PupilLightMatchReason_Count> smallMatchLeftCounts{};
#endif
    const char* anchorSource = "none";
    {
        // tryFinalizeRoundLocked内部已完成本轮DS处理以及跨轮候选的提交/丢弃；
        // 这里再在m_mutex下发布一次性门控，避免调度线程读取结算线程状态。
        std::lock_guard<std::mutex> lock(m_mutex);
        if (roundIdx >= 0 && roundIdx < MAX_ROUNDS) {
            auto& round = m_rounds[roundIdx];
            if (!round.asyncState.settlementCompletedOnce) {
                round.asyncState.settlementCompletedOnce = true;
                round.asyncState.settlementCompleted = true;
                round.asyncState.settlementReady = false;
                round.asyncState.settlementQueued = false;
                shouldLogRound = true;
                modelInputImgNo = round.asyncState.modelInputImgNo;
                receivedFrameCount = static_cast<int>(
                        round.asyncState.receivedFrames.count());
                rightValidCount = static_cast<int>(round.validRight.count());
                leftValidCount = static_cast<int>(round.validLeft.count());
                const RoundDs& ds = m_roundDs[roundIdx];
                const auto eyeFlags = get_eye_flags(m_eye);
                rightDsCount = eyeFlags.first && ds.generated
                        ? DS_ITEM_COUNT - missingDsCount(ds.right) : 0;
                leftDsCount = eyeFlags.second && ds.generated
                        ? DS_ITEM_COUNT - missingDsCount(ds.left) : 0;
                if (ds.generated) {
                    for (int itemIdx = 0; itemIdx < DS_ITEM_COUNT; ++itemIdx) {
                        if (ds.right[itemIdx].nativeValid
                                && ds.right[itemIdx].finalValid) {
                            ++rightNativeDsCount;
                        }
                        if (ds.left[itemIdx].nativeValid
                                && ds.left[itemIdx].finalValid) {
                            ++leftNativeDsCount;
                        }
                    }
                }
                roundAccepted = round.result.valid;
                roundPending = ds.pending;
                roundRejected = round.rejected || ds.rejected;
                masterAnchorMatchSuccess =
                        round.asyncState.masterAnchorMatchSuccess;
                masterAnchorMatchFailed =
                        round.asyncState.masterAnchorMatchFailed;
                failureDetailAttempts = round.pupilFailDetailLogCount.load(
                        std::memory_order_relaxed);
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
                smallMatchRightCounts =
                        round.asyncState.smallMatchRightCounts;
                smallMatchLeftCounts =
                        round.asyncState.smallMatchLeftCounts;
#endif
                const bool masterAnchorValid = m_formalMasterAnchorReady
                        && static_cast<bool>(m_formalMasterAnchor)
                        && m_formalMasterAnchorMeasurementGeneration
                            == m_formalMeasurementGeneration.load(
                                    std::memory_order_acquire);
                const bool currentAnchorComplete = round.asyncState.anchorReady
                        && (!eyeFlags.first
                            || round.asyncState.rightAnchor.state
                                == FormalAnchor_Confirmed)
                        && (!eyeFlags.second
                            || round.asyncState.leftAnchor.state
                                == FormalAnchor_Confirmed);
                const bool completeCrossRoundSource = roundIdx > 0
                        && hasCompleteFormalCrossRoundSourceLocked();
                if (masterAnchorValid) {
                    anchorSource = "master";
                } else if (currentAnchorComplete) {
                    anchorSource = "current_round";
                } else if (completeCrossRoundSource) {
                    anchorSource = "cross_round";
                }

            }
            for (auto activeIt = m_activeFormalRounds.begin();
                 activeIt != m_activeFormalRounds.end();) {
                if (*activeIt == roundIdx) {
                    activeIt = m_activeFormalRounds.erase(activeIt);
                } else {
                    ++activeIt;
                }
            }
            m_activeFormalProcessingRound = m_activeFormalRounds.empty()
                    ? -1 : m_activeFormalRounds.front();
        }
    }
    if (shouldLogRound) {
        const char* state = roundAccepted ? "accepted"
                : (roundPending ? "pending"
                   : (roundRejected ? "rejected" : "completed"));
        ALGO_KEY_LOG(
            qInfo().noquote()
                << QString("[DL_ROUND] round=%1,model_input=%2,frames=%3,"
                           "right_valid=%4,left_valid=%5,right_ds=%6,"
                           "left_ds=%7,state=%8,anchor_source=%9,"
                           "native_right_ds=%10,native_left_ds=%11,"
                           "master_anchor_match_success=%12,"
                           "master_anchor_match_failed=%13,"
                           "failure_detail_attempts=%14,"
                           "failure_detail_logged=%15,"
                           "failure_detail_suppressed=%16")
                   .arg(roundIdx)
                   .arg(modelInputImgNo)
                   .arg(receivedFrameCount)
                   .arg(rightValidCount)
                   .arg(leftValidCount)
                   .arg(rightDsCount)
                   .arg(leftDsCount)
                   .arg(state)
                   .arg(anchorSource)
                   .arg(rightNativeDsCount)
                   .arg(leftNativeDsCount)
                   .arg(masterAnchorMatchSuccess)
                   .arg(masterAnchorMatchFailed)
                   .arg(failureDetailAttempts)
                   .arg(std::min(failureDetailAttempts,
                                 MAX_PUPIL_FAIL_DETAIL_LOGS_PER_ROUND))
                   .arg(std::max(0, failureDetailAttempts
                                 - MAX_PUPIL_FAIL_DETAIL_LOGS_PER_ROUND))
        );
    }
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    if (shouldLogRound) {
        auto matchReasonCount = [&](const std::array<int,
                                                     PupilLightMatchReason_Count>& counts,
                                    PupilLightMatchReason reason) {
            const size_t index = static_cast<size_t>(reason);
            return counts[index];
        };
        auto totalReasonCount = [&](PupilLightMatchReason reason) {
            return matchReasonCount(smallMatchRightCounts, reason)
                    + matchReasonCount(smallMatchLeftCounts, reason);
        };
        int rightMatchCount = 0;
        int leftMatchCount = 0;
        for (int index = 0; index < PupilLightMatchReason_Count; ++index) {
            rightMatchCount += smallMatchRightCounts[
                    static_cast<size_t>(index)];
            leftMatchCount += smallMatchLeftCounts[
                    static_cast<size_t>(index)];
        }
        const int searchInvalid =
                totalReasonCount(PupilLightMatch_SearchRectInvalid)
                + totalReasonCount(PupilLightMatch_SearchOutsideGradient)
                + totalReasonCount(PupilLightMatch_SearchSmallerThanTemplate);
        qInfo().noquote()
                << QString("[DL_SMALL_MATCH_SUMMARY] round=%1 "
                           "success=%2 low_score=%3 "
                           "previous_eye_invalid=%4 previous_gradient_invalid=%5 "
                           "template_rect_invalid=%6 "
                           "template_outside_gradient=%7 "
                           "template_outside_eye_half=%8 "
                           "search_invalid=%9 empty_image=%10 exception=%11 "
                           "not_attempted=%12 right_success=%13 "
                           "right_failed=%14 left_success=%15 left_failed=%16 "
                           "retry_low_score=%17 retry_delta_inconsistent=%18")
                   .arg(roundIdx)
                   .arg(totalReasonCount(PupilLightMatch_None))
                   .arg(totalReasonCount(PupilLightMatch_ScoreBelowThreshold))
                   .arg(totalReasonCount(PupilLightMatch_PreviousEyeInvalid))
                   .arg(totalReasonCount(PupilLightMatch_PreviousGradientInvalid))
                   .arg(totalReasonCount(PupilLightMatch_TemplateRectInvalid))
                   .arg(totalReasonCount(PupilLightMatch_TemplateOutsideGradient))
                   .arg(totalReasonCount(PupilLightMatch_TemplateOutsideEyeHalf))
                   .arg(searchInvalid)
                   .arg(totalReasonCount(PupilLightMatch_TemplateOrSearchEmpty))
                   .arg(totalReasonCount(PupilLightMatch_MatchException))
                   .arg(totalReasonCount(PupilLightMatch_NotAttempted))
                   .arg(matchReasonCount(smallMatchRightCounts,
                                         PupilLightMatch_None))
                   .arg(rightMatchCount - matchReasonCount(
                           smallMatchRightCounts, PupilLightMatch_None))
                   .arg(matchReasonCount(smallMatchLeftCounts,
                                         PupilLightMatch_None))
                   .arg(leftMatchCount - matchReasonCount(
                           smallMatchLeftCounts, PupilLightMatch_None))
                   .arg(totalReasonCount(
                           PupilLightMatch_RetryScoreBelowThreshold))
                   .arg(totalReasonCount(
                           PupilLightMatch_RetryDeltaInconsistent));
    }
#endif
#if ENABLE_ALGO_VERBOSE_LOG
    qInfo().noquote()
            << QString("[DL_INTERLEAVE_SETTLEMENT] round=%1 "
                       "ready=yes action=completed")
               .arg(roundIdx);
#endif
    // 结算后释放活动槽位，统一寻找最早已缓存的等待轮次。
    pumpFormalInterleavedFrames();
    scheduleNextReadyFormalSettlement();
}

void CAlgo::requestFormalC800Task(
        int roundIdx,
        int imgNo,
        const cv::Mat& image,
        const cv::Mat& smallImage,
        std::uint64_t measurementGeneration,
        std::uint64_t roundGeneration,
        const std::string& reason)
{
    bool startNow = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
            return;
        }
        if (m_formalC800TaskRunning) {
            FormalC800Request request;
            request.roundIdx = roundIdx;
            request.imgNo = imgNo;
            request.image = image;
            request.smallImage = smallImage;
            request.measurementGeneration = measurementGeneration;
            request.roundGeneration = roundGeneration;
            request.reason = reason;
            m_pendingFormalC800Starts.push_back(std::move(request));
        } else {
            m_formalC800TaskRunning = true;
            m_formalC800RunningRound = roundIdx;
            startNow = true;
        }
    }

#if ENABLE_ALGO_VERBOSE_LOG
    if (!startNow) {
        qInfo().noquote()
                << QString("[DL_INTERLEAVE_C800] round=%1 action=deferred")
                   .arg(roundIdx);
        return;
    }
#else
    if (!startNow) {
        return;
    }
#endif
#if ENABLE_ALGO_VERBOSE_LOG
    qInfo().noquote()
            << QString("[DL_INTERLEAVE_C800] round=%1 action=running")
               .arg(roundIdx);
#endif
    startFormalC800Task(roundIdx, imgNo, image, smallImage,
                        measurementGeneration, roundGeneration, reason);
}

void CAlgo::finishFormalC800Task()
{
    FormalC800Request nextRequest;
    bool startNext = false;
    int finishedRound = -1;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        finishedRound = m_formalC800RunningRound;
        m_formalC800TaskRunning = false;
        m_formalC800RunningRound = -1;
        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
            m_pendingFormalC800Starts.clear();
        } else {
            while (!m_pendingFormalC800Starts.empty()) {
                FormalC800Request candidate =
                        std::move(m_pendingFormalC800Starts.front());
                m_pendingFormalC800Starts.pop_front();
                if (candidate.roundIdx < 0
                        || candidate.roundIdx >= MAX_ROUNDS) {
                    continue;
                }
                const FormalAsyncRoundState& state =
                        m_rounds[candidate.roundIdx].asyncState;
                if (state.measurementGeneration
                            != candidate.measurementGeneration
                        || state.roundGeneration
                            != candidate.roundGeneration
                        || state.modelInputImgNo != candidate.imgNo
                        || !state.modelAttempted
                        || !state.anchorTaskRunning
                        || state.earlyRetryRequested) {
                    qWarning().noquote()
                            << QString("[DL_INTERLEAVE_C800] round=%1 "
                                       "action=discarded reason=stale_request")
                               .arg(candidate.roundIdx);
                    continue;
                }
                nextRequest = std::move(candidate);
                m_formalC800TaskRunning = true;
                m_formalC800RunningRound = nextRequest.roundIdx;
                startNext = true;
                break;
            }
        }
    }

#if ENABLE_ALGO_VERBOSE_LOG
    if (finishedRound >= 0) {
        qInfo().noquote()
                << QString("[DL_INTERLEAVE_C800] round=%1 action=finished")
                   .arg(finishedRound);
    }
#endif
    if (startNext) {
#if ENABLE_ALGO_VERBOSE_LOG
        qInfo().noquote()
                << QString("[DL_INTERLEAVE_C800] round=%1 action=running")
                   .arg(nextRequest.roundIdx);
#endif
        startFormalC800Task(
                nextRequest.roundIdx, nextRequest.imgNo,
                nextRequest.image, nextRequest.smallImage,
                nextRequest.measurementGeneration,
                nextRequest.roundGeneration, nextRequest.reason);
    }
}

void CAlgo::startFormalC800Task(int roundIdx,
                                 int imgNo,
                                 const cv::Mat& image,
                                 const cv::Mat& smallImage,
                                 std::uint64_t measurementGeneration,
                                 std::uint64_t roundGeneration,
                                 const std::string& reason)
{
    m_formalAsyncTasksInFlight.fetch_add(1, std::memory_order_acq_rel);
    const auto taskScope = std::make_shared<FormalAsyncTaskScope>(
            &m_formalAsyncTasksInFlight, &m_formalAsyncWaitCondition);
    // 正式C800必须接收原图；检测器内部负责灰度化、缩放和黑图拼接。
    // smallImage仅保留在调度接口中，正式模型任务不使用它。
    const cv::Mat modelImage = image;
    Q_UNUSED(smallImage);
    const auto modelEyeFlags = get_eye_flags(m_eye);
    pool->start(new QRunnableFunction(
            [this, roundIdx, imgNo, modelImage,
             measurementGeneration, roundGeneration, reason,
             modelEyeFlags, taskScope]() {
        Q_UNUSED(taskScope);
        FormalScopeExit c800Guard([this]() {
            finishFormalC800Task();
        });
        // C800存在多处代际/状态提前返回，统一用守卫释放串行闸门，
        // 防止某个旧任务退出后后续轮次永久停在deferred状态。
        if (roundIdx < 0 || roundIdx >= MAX_ROUNDS
                || imgNo < 1 || imgNo > FRAMES_PER_ROUND
                || m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)
                 || m_formalMeasurementGeneration.load(
                            std::memory_order_acquire) != measurementGeneration) {
            return;
        }

#if ENABLE_ALGO_TIMING_LOG
        // 线程局部计时上下文不会自动从提交线程传入线程池，必须在线程入口绑定轮次。
        AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
#endif

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const FormalAsyncRoundState& state =
                    m_rounds[roundIdx].asyncState;
            if (state.measurementGeneration != measurementGeneration
                    || state.roundGeneration != roundGeneration
                    || state.modelInputImgNo != imgNo
                    || state.earlyRetryRequested
                    || !state.modelAttempted
                    || !state.anchorTaskRunning) {
                return;
            }
        }

        // 正式流程只允许首张有效照片执行一次C800；输入为原图真实照片，
        // 检测器内部生成400×160真实图和同尺寸黑图，后续不再调用C800。
        ALGO_KEY_LOG(
            qInfo().noquote()
                << QString("[DL_MODEL_INPUT] mode=real_plus_black,"
                           "source=original,source_size=%3x%4,"
                           "round=%1,img_no=%2,"
                           "layout=real_400x160_plus_black_400x160,"
                           "reason=%5")
                       .arg(roundIdx)
                       .arg(imgNo)
                       .arg(modelImage.cols)
                       .arg(modelImage.rows)
                       .arg(QString::fromStdString(reason)));

        const auto started = std::chrono::steady_clock::now();
        PupilLightFrame modelFrame;
        std::string error;
        const bool modelSuccess = runFormalC800Once(
                modelImage, &modelFrame, &error);
        const double elapsedMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
#if ENABLE_ALGO_TIMING_LOG
        // 外层耗时覆盖模型调用、结果转换和几何门禁，失败调用同样计入。
        AlgoTiming::recordMilliseconds(AlgoTimingStage_C800Total, elapsedMs);
#endif
        // runFormalC800Once返回的modelFrame已经经过模型结果几何门禁；
        // 这里记录的是门禁后的原图坐标，避免误称为ONNX原始输出。
        ALGO_KEY_LOG(
            qInfo().noquote()
                << QString("[DL_MODEL_FILTERED] right_detected=%1,right_x=%2,"
                           "right_y=%3,right_radius=%4,left_detected=%5,"
                           "left_x=%6,left_y=%7,left_radius=%8,"
                           "coordinate_space=original,stage=geometry_filtered")
                       .arg(modelFrame.subjectRight.detected ? "yes" : "no")
                       .arg(modelFrame.subjectRight.center.x, 0, 'f', 2)
                       .arg(modelFrame.subjectRight.center.y, 0, 'f', 2)
                       .arg(modelFrame.subjectRight.radius, 0, 'f', 2)
                       .arg(modelFrame.subjectLeft.detected ? "yes" : "no")
                       .arg(modelFrame.subjectLeft.center.x, 0, 'f', 2)
                       .arg(modelFrame.subjectLeft.center.y, 0, 'f', 2)
                       .arg(modelFrame.subjectLeft.radius, 0, 'f', 2));
        ALGO_KEY_LOG(
            qInfo().noquote()
                << QString("[DL_MODEL_RESULT] round=%1,img_no=%2,"
                           "success=%3,right=%4,left=%5,elapsed_ms=%6,error=%7")
                       .arg(roundIdx)
                       .arg(imgNo)
                       .arg(modelSuccess ? "yes" : "no")
                       .arg(modelFrame.subjectRight.detected ? "yes" : "no")
                       .arg(modelFrame.subjectLeft.detected ? "yes" : "no")
                       .arg(elapsedMs, 0, 'f', 2)
                       .arg(QString::fromStdString(error)));

#if ENABLE_ALGO_VERBOSE_LOG
        qDebug().noquote()
                << QString("FormalAsyncModel: generation=%1,round=%2,img_no=%3,"
                           "capture_no=%4,input=real_400x160_plus_black_400x160,reason=%5,"
                           "round_attempt=1,requested_right=%6,"
                           "requested_left=%7,detected_right=%8,"
                           "detected_left=%9,success=%10,elapsed_ms=%11,"
                           "error=%12,layout=real_400x160_plus_black_400x160")
                   .arg(measurementGeneration)
                   .arg(roundIdx)
                   .arg(imgNo)
                   .arg(formalCaptureNumber(imgNo))
                   .arg(QString::fromStdString(reason))
                   .arg(modelEyeFlags.first ? "yes" : "no")
                   .arg(modelEyeFlags.second ? "yes" : "no")
                   .arg(modelFrame.subjectRight.detected ? "yes" : "no")
                   .arg(modelFrame.subjectLeft.detected ? "yes" : "no")
                   .arg(modelSuccess ? "yes" : "no")
                   .arg(elapsedMs, 0, 'f', 2)
                   .arg(QString::fromStdString(error));
#endif

        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)
                || m_formalMeasurementGeneration.load(
                        std::memory_order_acquire) != measurementGeneration) {
            return;
        }

        // C800未检出任一目标眼时，本次测量已经无法建立完整首帧锚点。
        // 直接复用现有PupilNotFound收尾链路停止转灯并返回预览，不再换物理轮重试。
        const bool requiredEyesDetected =
                (!modelEyeFlags.first || modelFrame.subjectRight.detected)
                && (!modelEyeFlags.second || modelFrame.subjectLeft.detected);
        if (!requiredEyesDetected) {
            emitFormalPupilNotFoundOnce(
                    roundIdx,
                    QStringLiteral("c800_required_eye_not_detected"));
            return;
        }

        // C800坐标已经映射回原图；只对模型实际检出的眼执行一次同照片
        // 129 ROI确认，确认后的坐标才允许成为本轮正式锚点。
        const bool currentModelPupilDetected =
                (modelEyeFlags.first && modelFrame.subjectRight.detected)
                || (modelEyeFlags.second && modelFrame.subjectLeft.detected);
        if (currentModelPupilDetected) {
            // C800首张129 ROI失败也使用本轮统一预算。
            PupilFailDetailRoundLogScope pupilFailDetailLogScope(
                    &m_rounds[roundIdx].pupilFailDetailLogCount);
            processAndStoreLocatedFrame(
                    roundIdx, imgNo, modelImage,
                    modelEyeFlags.first && modelFrame.subjectRight.detected,
                    modelFrame.subjectRight.detected
                        ? makeTrackedPupilInfo(modelFrame.subjectRight)
                        : stPupilInfo{},
                    modelEyeFlags.second && modelFrame.subjectLeft.detected,
                    modelFrame.subjectLeft.detected
                     ? makeTrackedPupilInfo(modelFrame.subjectLeft)
                     : stPupilInfo{},
                    measurementGeneration, roundGeneration, false);
        } else {
            // C800完全漏检时也缓存第一张原图，结果页返回双眼无效状态。
            cacheFirstFramePupilCropSource(
                    roundIdx, imgNo, modelImage,
                    false, stPupilInfo{},
                    false, stPupilInfo{},
                    measurementGeneration, roundGeneration);
#if ENABLE_ALGO_TIMING_LOG
            // 这里只补充统计，不改变算法状态。
            AlgoTiming::event(AlgoTimingEvent_FormalPupilFailure);
            if (modelEyeFlags.first) {
                AlgoTiming::event(AlgoTimingEvent_FormalRoi129Failure);
                AlgoTiming::event(AlgoTimingEvent_ProcessFailure);
            }
            if (modelEyeFlags.second) {
                AlgoTiming::event(AlgoTimingEvent_FormalRoi129Failure);
                AlgoTiming::event(AlgoTimingEvent_ProcessFailure);
            }
#endif
        }

        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)
                || m_formalMeasurementGeneration.load(
                        std::memory_order_acquire) != measurementGeneration) {
            return;
        }

        bool taskStillCurrent = false;
        bool rightConfirmed = false;
        bool leftConfirmed = false;
        FormalAnchorState rightAnchorState = FormalAnchor_NoAnchor;
        FormalAnchorState leftAnchorState = FormalAnchor_NoAnchor;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            MeasurementRound& round = m_rounds[roundIdx];
            FormalAsyncRoundState& asyncState = round.asyncState;
            if (asyncState.measurementGeneration == measurementGeneration
                    && asyncState.roundGeneration == roundGeneration
                    && asyncState.modelInputImgNo == imgNo
                    && asyncState.modelAttempted
                    && asyncState.anchorTaskRunning
                    && !asyncState.modelFinished) {
                taskStillCurrent = true;
                rightConfirmed = modelEyeFlags.first
                        && round.validRight.test(imgNo);
                leftConfirmed = modelEyeFlags.second
                        && round.validLeft.test(imgNo);
                if (rightConfirmed) {
                    setFormalAnchorConfirmedLocked(
                            roundIdx, true, round.pupilInfoRight[imgNo],
                            "c800_photo_roi_refined");
                }
                if (leftConfirmed) {
                    setFormalAnchorConfirmedLocked(
                            roundIdx, false, round.pupilInfoLeft[imgNo],
                            "c800_photo_roi_refined");
                }
                rightAnchorState = asyncState.rightAnchor.state;
                leftAnchorState = asyncState.leftAnchor.state;
            }
        }
        if (!taskStillCurrent) {
            return;
        }

        // 首张C800及同照片129 ROI必须同时确认所有目标眼；任一目标眼失败
        // 就立即结束当前物理轮，禁止发布不完整锚点或继续处理后续照片。
        const bool requiredEyesConfirmed =
                (!modelEyeFlags.first || rightConfirmed)
                && (!modelEyeFlags.second || leftConfirmed);
        if (!requiredEyesConfirmed) {
            bool shouldEmitRetry = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                MeasurementRound& round = m_rounds[roundIdx];
                FormalAsyncRoundState& asyncState = round.asyncState;
                if (asyncState.measurementGeneration == measurementGeneration
                        && asyncState.roundGeneration == roundGeneration
                        && asyncState.modelInputImgNo == imgNo
                        && asyncState.modelAttempted
                        && asyncState.anchorTaskRunning
                        && !asyncState.earlyRetryRequested) {
                    asyncState.earlyRetryRequested = true;
                    asyncState.anchorReady = false;
                    asyncState.anchor.reset();
                    asyncState.modelFinished = true;
                    asyncState.anchorTaskRunning = false;
                    asyncState.processedFrames.set(imgNo);
                    round.frames[imgNo].processed = true;
                    round.frames[imgNo].failureReason =
                            "anchor_failed_early_retry";
                    asyncState.lastProcessedAt =
                            std::chrono::steady_clock::now();
                    writeRoundAlgoStatusLocked(
                            roundIdx,
                            QStringLiteral("anchor_failed_early_retry"),
                            QStringLiteral("c800_or_roi_confirmation_failed"));
                    for (auto activeIt = m_activeFormalRounds.begin();
                         activeIt != m_activeFormalRounds.end();) {
                        if (*activeIt == roundIdx) {
                            activeIt = m_activeFormalRounds.erase(activeIt);
                        } else {
                            ++activeIt;
                        }
                    }
                    m_activeFormalProcessingRound = m_activeFormalRounds.empty()
                            ? -1 : m_activeFormalRounds.front();
                    shouldEmitRetry = true;
                }
            }

            if (shouldEmitRetry) {
                resultState = calcResultState_RetryNextRound;
                m_formalModelFinished.store(true, std::memory_order_release);
                qWarning().noquote()
                        << QString("FormalAsyncAnchorRetry: round=%1,img_no=%2,"
                                   "right_confirmed=%3,left_confirmed=%4")
                           .arg(roundIdx)
                           .arg(formalCaptureNumber(imgNo))
                           .arg(rightConfirmed ? "yes" : "no")
                           .arg(leftConfirmed ? "yes" : "no");
                if (m_visionCb) {
                    // 回调参数包含失败轮次，界面侧可丢弃已经过期的迟到通知。
                    bool finished = false;
                    bool questionable = false;
                    m_visionCb(calcResultState_RetryNextRound,
                               roundIdx,
                               stVisionValue{}, stVisionAbnormal{},
                               std::vector<stVisionValue>{}, finished, questionable);
                }
                pumpFormalInterleavedFrames();
            }
            return;
        }

        ALGO_KEY_LOG(
            qInfo().noquote()
                << QString("[DL_ANCHOR_ROI] round=%1,img_no=%2,"
                           "right_confirmed=%3,left_confirmed=%4")
                       .arg(roundIdx)
                       .arg(imgNo)
                       .arg(rightConfirmed ? "yes" : "no")
                       .arg(leftConfirmed ? "yes" : "no"));

        publishFormalAnchorFromRound(
                roundIdx, imgNo, modelImage,
                measurementGeneration, roundGeneration);

        if (m_formalStreamingStop.load(std::memory_order_acquire)
                || m_hasEmittedFinalResult.load(std::memory_order_acquire)
                || m_formalMeasurementGeneration.load(
                        std::memory_order_acquire) != measurementGeneration) {
            return;
        }

        bool taskCompleted = false;
        bool anchorReady = false;
        int cachedFrameCount = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            MeasurementRound& round = m_rounds[roundIdx];
            FormalAsyncRoundState& asyncState = round.asyncState;
            if (!m_formalStreamingStop.load(std::memory_order_acquire)
                    && !m_hasEmittedFinalResult.load(
                            std::memory_order_acquire)
                    && m_formalMeasurementGeneration.load(
                            std::memory_order_acquire) == measurementGeneration
                    && asyncState.measurementGeneration
                        == measurementGeneration
                    && asyncState.roundGeneration == roundGeneration
                    && asyncState.modelInputImgNo == imgNo
                    && asyncState.modelAttempted
                    && asyncState.anchorTaskRunning
                    && !asyncState.modelFinished) {
                asyncState.processedFrames.set(imgNo);
                round.frames[imgNo].processed = true;
                asyncState.modelFinished = true;
                asyncState.anchorTaskRunning = false;
                asyncState.lastProcessedAt =
                        std::chrono::steady_clock::now();
                anchorReady = asyncState.anchorReady;
                cachedFrameCount = asyncState.receivedFrames.count();
                taskCompleted = true;
            }
        }

        if (!taskCompleted) {
            return;
        }

        m_formalModelFinished.store(true, std::memory_order_release);
#if ENABLE_ALGO_VERBOSE_LOG
        qDebug().noquote()
                << QString("FormalAsyncAnchorTaskComplete: round=%1,img_no=%2,"
                           "capture_no=%3,model_success=%4,anchor_ready=%5,"
                           "right_state=%6,left_state=%7,cached_frames=%8")
                   .arg(roundIdx)
                   .arg(imgNo)
                   .arg(formalCaptureNumber(imgNo))
                   .arg(modelSuccess ? "yes" : "no")
                   .arg(anchorReady ? "yes" : "no")
                   .arg(formalAnchorStateName(rightAnchorState))
                   .arg(formalAnchorStateName(leftAnchorState))
                   .arg(cachedFrameCount);
#endif

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            MeasurementRound& round = m_rounds[roundIdx];
            if (round.asyncState.inputFinished
                    && round.asyncState.processedFrames.count()
                       == FRAMES_PER_ROUND
                    && round.asyncState.photoTasksInFlight == 0
                    && !round.asyncState.earlyRetryRequested) {
                round.asyncState.settlementReady = true;
            }
        }
        pumpFormalInterleavedFrames();
        scheduleNextReadyFormalSettlement();
    }), 1);
}
enCalcResultState CAlgo::appendFormalAsyncImage(const cv::Mat& image,
                                                int roundIdx,
                                                int imgNo,
                                                bool isInRange)
{
    if (image.empty() || roundIdx < 0
            || roundIdx >= MAX_ROUNDS
            || imgNo < 1 || imgNo > FRAMES_PER_ROUND) {
        return calcResultState_ParamInvalid;
    }
    if (m_formalStreamingStop.load(std::memory_order_acquire)
            || resultState != calcResultState_Succ) {
        return calcResultState_Aborted;
    }
    if (m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
        return calcResultState_Aborted;
    }

    // 相机图只在入队时生成一次400×160灰度小图；原图仍保留给129 ROI
    // 和DS计算。正式流程的C800只由首张有效照片启动一次。
    const cv::Mat smallImage = makeFormalSmallGray(image);
    if (smallImage.empty()) {
        return calcResultState_ParamInvalid;
    }

    std::uint64_t measurementGeneration =
            m_formalMeasurementGeneration.load(std::memory_order_acquire);
    std::uint64_t roundGeneration = 0;
    bool selectedAsFirst = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        MeasurementRound& round = m_rounds[roundIdx];
        if (round.asyncState.earlyRetryRequested) {
            // 提前换轮后仍可能收到迟到照片；外部保存链路照常完成，
            // 算法只返回成功并丢弃该照片，不再修改本轮处理状态。
            return calcResultState_Succ;
        }
        if (round.asyncState.measurementGeneration == 0) {
            round.asyncState.measurementGeneration = measurementGeneration;
        }
        if (round.asyncState.roundGeneration == 0) {
            round.asyncState.roundGeneration =
                    m_formalRoundGenerationCounter.fetch_add(
                            1, std::memory_order_acq_rel) + 1;
        }
        measurementGeneration = round.asyncState.measurementGeneration;
        roundGeneration = round.asyncState.roundGeneration;

        // 距离超限只影响本轮质量风险；同一槽位如果后续收到合格距离，
        // 清除该槽位风险标记，但不丢弃图片。
        if (isInRange) {
            round.outOfRangeFrames.reset(imgNo);
        } else {
            round.outOfRangeFrames.set(imgNo);
        }
        round.outOfRangeFrameCount = static_cast<int>(
                round.outOfRangeFrames.count());

        if (round.asyncState.receivedFrames.test(imgNo)
                || round.asyncState.processedFrames.test(imgNo)) {
            if (round.asyncState.modelAttempted
                    && round.asyncState.modelInputImgNo == imgNo) {
                qWarning() << "FormalAsyncModel: duplicate call blocked, "
                              "call remains one per round,round=" << roundIdx;
            }
            return calcResultState_Succ;
        }
        round.frames[imgNo].hasFrame = true;
        round.frames[imgNo].frame = image;
        round.frames[imgNo].smallFrame = smallImage;
        round.asyncState.receivedFrames.set(imgNo);
        if (round.asyncState.firstReceivedImgNo == 0) {
            round.asyncState.firstReceivedImgNo = imgNo;
            selectedAsFirst = true;
        }

    }

    if (selectedAsFirst) {
#if ENABLE_ALGO_VERBOSE_LOG
        qDebug().noquote()
                << QString("FormalAsyncFirstFrame: round=%1,img_no=%2,"
                           "capture_no=%3,selected_as_first=yes")
                   .arg(roundIdx)
                   .arg(imgNo)
                   .arg(formalCaptureNumber(imgNo));
#endif
    }

#if ENABLE_ALGO_VERBOSE_LOG
    qDebug().noquote()
            << QString("PupilRange: round=%1,photo=%2,cached=yes,"
                       "range_affected=%3")
               .arg(roundIdx)
               .arg(formalCaptureNumber(imgNo))
               .arg(isInRange ? "no" : "yes");
#endif

    // 相机线程只缓存照片；模型是否启动由调度器在上一轮结算门控后决定。
    pumpFormalInterleavedFrames();
    return resultState;
}

enCalcResultState CAlgo::appendImage(const cv::Mat &img, const int roundIdx, const int imgNo, const bool isInRange)
{
//    QString homePath = QDir::homePath();  // 获取家目录路径
//    QString filePath = QString("%1/tmp/%2.jpg").arg(homePath).arg(m_imgIndex++);
//    cv::imwrite(filePath.toStdString(), img);
//    qDebug()<<"filePath:"<<filePath;
//    return calcResultState_Succ;

//    QString homePath = QDir::homePath();  // 获取家目录路径
//    QString tmpDir = QString("%1/tmp").arg(homePath);
//    cv::Mat img=_img;
//    // 先检查tmp目录是否存在
//    QDir directory(tmpDir);
//    if (directory.exists()) {
//        // 获取目录下所有的jpg文件
//        QStringList filters;
//        filters << "*.jpg" << "*.jpeg" << "*.JPG" << "*.JPEG";
//        directory.setNameFilters(filters);
//        QFileInfoList fileList = directory.entryInfoList(QDir::Files);

//        if (!fileList.isEmpty()) {
//            // 随机选择一个文件
//            int randomIndex = QRandomGenerator::global()->bounded(fileList.size());
//            QFileInfo fileInfo = fileList.at(randomIndex);
//            QString filePath = fileInfo.absoluteFilePath();

//            qDebug() << "Randomly selected image:" << filePath;

//            img = cv::imread(filePath.toStdString(), cv::IMREAD_GRAYSCALE);

//            if (!img.empty()) {
//                qDebug() << "Successfully loaded random image:" << filePath;
//            } else {
//                // 如果以上都失败，使用传入的_img
//                qWarning() << "Failed to load image:" << filePath;
//            }
//        } else {
//            qWarning() << "No jpg images found in directory:" << tmpDir;
//        }
//    } else {
//        qWarning() << "Directory does not exist:" << tmpDir;
//    }

    if (resultState != calcResultState_Succ) {
        return resultState;
    }

    // 测量是否已开始？
    if (roundIdx < 0) {
        qDebug() << "appendImage: measurement not started";
        return calcResultState_Aborted;
    }
    if (roundIdx >= MAX_ROUNDS) {
        qDebug() << QString("appendImage: roundIdx %1 out of range [0, %2)")
                    .arg(roundIdx)
                    .arg(MAX_ROUNDS);
        return calcResultState_ParamInvalid;
    }

    // 第0帧是相机真实输出的转灯起始/清缓存帧。
    // 采集层统一提交0～22帧，算法层在这里集中决定第0帧不参与计算。
    // 不克隆、不缓存、不入队、不计入轮次完整性，也不写入DS。
    if (imgNo == 0) {
        return calcResultState_Succ;
    }

    // 只有第1～22帧属于计算灯位。
    if (imgNo < 1 || imgNo > FRAMES_PER_ROUND) {
        qDebug() << QString("appendImage: imgNo %1 out of range [1, %2]")
                 .arg(imgNo).arg(FRAMES_PER_ROUND);
        return calcResultState_ParamInvalid;
    }

    PERF_POINT(__PRETTY_FUNCTION__ + QString("imgIdx = %1").arg(imgNo));

    // 1. 检查 cv::Mat 是否为空或无效
    if (img.empty())
    {
        qDebug() << "appendImage: image is null or empty";
        return calcResultState_ParamInvalid;
    }

    // 2. 检查图像数据指针是否有效（更严格的检查）
    if (!img.data || img.data == nullptr)
    {
        qDebug() << "appendImage: null image data";
        return calcResultState_ParamInvalid;
    }

    // 3. 检查图像尺寸是否合理
    if (img.rows <= 0 || img.cols <= 0)
    {
        qDebug() << QString("appendImage: invalid dimensions %1x%2").arg(img.rows).arg(img.cols);
        return calcResultState_ParamInvalid;
    }

    // 4. 检查通道数是否合理（根据业务需求）
    if (img.channels() != 1 )
    {
        qDebug() << QString("appendImage: invalid channels %1").arg(img.channels());
        return calcResultState_ParamInvalid;
    }

    // ========== 2. 状态检查（加锁）==========
    if (m_hasEmittedFinalResult.load(std::memory_order_acquire)) {
        qDebug() << "appendImage: measurement already completed";
        return calcResultState_Aborted;
    }

#if ENABLE_ALGO_TIMING_LOG
    AlgoTiming::markFormalFrame(roundIdx);
    m_timingFormalFrameCount.fetch_add(1, std::memory_order_relaxed);
    const int64_t appendCloneStartNs = AlgoTiming::nowNs();
#endif
    cv::Mat threadSafeImg;
    try {
        threadSafeImg = img.clone(); // 如果这里是野指针，会抛出异常而不是 Segfault
    } catch (const cv::Exception& e) {
        ALGO_ERROR_LOG(
            qCritical() << "appendImage: Failed to clone input image, memory may be corrupted:"
                        << e.what()
        );
        return calcResultState_ProgramException;
    }

    if (threadSafeImg.empty()) {
        return calcResultState_ParamInvalid;
    }
#if ENABLE_ALGO_TIMING_LOG
    AlgoTiming::record(AlgoTimingStage_AppendClone,
                       AlgoTiming::nowNs() - appendCloneStartNs,
                       AlgoTimingPhase_Formal, roundIdx);
    const int64_t enqueueNs = AlgoTiming::nowNs();
#endif

    // 正式真人测量统一走逐照片异步路径：相机线程只完成校验/克隆/入队，
    // 不再进入旧的整轮等待、4张配额或11张模型锚点阶段机。
    if (FORMAL_PER_FRAME_ASYNC_ENABLED && shouldUseFormalHybrid()) {
        return appendFormalAsyncImage(threadSafeImg, roundIdx, imgNo,
                                      isInRange);
    }

    bool useFormalHybrid = false;
    bool startFormalHybrid = false;
    bool startFormalStreaming = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        MeasurementRound& round = m_rounds[roundIdx];
        round.frames[imgNo].hasFrame = true;
        useFormalHybrid = shouldUseFormalHybrid();
        if (useFormalHybrid) {
            round.frames[imgNo].frame = threadSafeImg;

            int cachedCount = 0;
            for (int index = 1; index <= FRAMES_PER_ROUND; ++index) {
                if (round.frames[index].hasFrame
                        && !round.frames[index].frame.empty()) {
                    ++cachedCount;
                }
            }
            if (cachedCount == FRAMES_PER_ROUND) {
#if ENABLE_ALGO_TIMING_LOG
                // 22张缓存全部到齐，精确记录该轮转灯图进入算法的结束时刻。
                AlgoTiming::markFormalRoundCaptureComplete(roundIdx);
#endif
            }
            // 只在主识别的两张图片都已经缓存后启动。L型箱中，这两个
            // 内部槽位对应相机实际拍摄的第1张+第11张。
            const bool streamingAnchorsCached =
                    roundIdx == 0
                    && round.frames[
                            FORMAL_PRIMARY_MODEL_ANCHORS.front()].hasFrame
                    && !round.frames[
                            FORMAL_PRIMARY_MODEL_ANCHORS.front()].frame.empty()
                    && round.frames[
                            FORMAL_PRIMARY_MODEL_ANCHORS.back()].hasFrame
                    && !round.frames[
                            FORMAL_PRIMARY_MODEL_ANCHORS.back()].frame.empty();
            // 兼容整轮路径按已有照片数量启动后台处理；正式逐照片异步
            // 路径不等待固定照片配额。
            const bool readyToStart = roundIdx == 0
                    ? streamingAnchorsCached
                    : (roundIdx <= 2 ? cachedCount > 0
                                     : cachedCount == FRAMES_PER_ROUND);
            if (readyToStart && !round.hybridProcessingStarted) {
                round.hybridProcessingStarted = true;
                startFormalHybrid = true;
                startFormalStreaming =
                        roundIdx <= 2
                        && cachedCount < FRAMES_PER_ROUND;
#if ENABLE_ALGO_VERBOSE_LOG
                if (startFormalStreaming) {
                    qDebug() << "PupilHybridQueue: streaming images ready"
                             << "round=" << roundIdx
                             << "received=" << cachedCount
                             << "mode=" << (roundIdx == 0
                                             ? "first_round_C800"
                                             : "cross_round_gradient");
                }
#endif
            }
        }
    }

    if (useFormalHybrid) {
        // 唤醒正在等待下一批转灯图的首轮流式任务。
        m_formalFrameCondition.notify_all();
        if (startFormalHybrid) {
            // 任务计数必须覆盖“已入队但尚未执行”的窗口，避免预览模型抢占。
            // 通过shared_ptr交给QRunnable持有；即使线程池clear()删除该任务，
            // 其析构也会回收计数，不会留下跨测量的假忙状态。
            auto taskScope = std::make_shared<FormalHybridTaskScope>(
                    &m_formalHybridTasksInFlight,
                    &m_previewModelSuppressionLogged);
            auto hybridTask = [this, roundIdx, startFormalStreaming
                               , taskScope
#if ENABLE_ALGO_TIMING_LOG
                               , enqueueNs
#endif
                               ]() {
                Q_UNUSED(taskScope);
                // 取消、新测量开始、或前一轮已经产出最终结果时，均不允许
                // 排队中的旧任务继续读写m_rounds。
                if (m_formalStreamingStop.load(std::memory_order_acquire)
                        || m_hasEmittedFinalResult.load(std::memory_order_acquire)
                        || resultState != calcResultState_Succ) {
#if ENABLE_ALGO_VERBOSE_LOG
                    qDebug() << "PupilHybridQueue: skip round" << roundIdx
                             << "because measurement was cancelled or finished";
#endif
                    return;
                }
#if ENABLE_ALGO_TIMING_LOG
                AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
                AlgoTiming::record(AlgoTimingStage_QueueWait,
                                   AlgoTiming::nowNs() - enqueueNs);
#endif
#if ENABLE_ALGO_VERBOSE_LOG
                qDebug() << "PupilHybridQueue: start round" << roundIdx;
#endif
                if (startFormalStreaming) {
                    processFormalStreamingRound(roundIdx);
                } else {
                    processFormalHybridRound(roundIdx);
                }
            };
#if ENABLE_ALGO_VERBOSE_LOG
            qDebug() << "PupilHybridQueue: submit round" << roundIdx
                     << "active=" << m_formalHybridPool.activeThreadCount();
#endif
            // 在提交到线程池之前先增加计数，消除任务尚未开始时预览模型插入的竞态。
            m_formalHybridTasksInFlight.fetch_add(1, std::memory_order_acq_rel);
            m_formalHybridPool.start(new QRunnableFunction(hybridTask));
        }
        // 兼容整轮路径的后台任务仍由相机线程唤醒；正式逐照片异步路径
        // 在上面的分支中已经直接返回。
        return resultState;
    }

    // === 步骤 3：定义异步处理任务 ===
    auto task = [this, roundIdx, imgNo, rawImg = std::move(threadSafeImg)
#if ENABLE_ALGO_TIMING_LOG
                 , enqueueNs
#endif
                 ]() {
#if ENABLE_ALGO_TIMING_LOG
        AlgoTimingContextScope timingContext(AlgoTimingPhase_Formal, roundIdx);
        AlgoTiming::record(AlgoTimingStage_QueueWait,
                           AlgoTiming::nowNs() - enqueueNs);
        AlgoTimingScope formalTaskTiming(AlgoTimingStage_FormalTaskTotal);
#endif
        PERF_START(task_total);
        PupilFailDetailRoundLogScope pupilFailDetailLogScope(&m_rounds[roundIdx].pupilFailDetailLogCount);
        auto shouldExitTask = [this]() {
            return m_hasEmittedFinalResult.load(std::memory_order_acquire)
                    || resultState != calcResultState_Succ;
        };

        // 【关键修改】每次做实际工作前，检查一下是不是被清场了
        if (shouldExitTask()) {
            return; // 安全退出，不操作任何 m_rounds 数据
        }

        stPupilInfo pr, pl;
        // 检测瞳孔
        bool ok = detectPupil(rawImg.data,
                             imgNo, m_age,
                             pr, pl, true, m_eye);
#if ENABLE_ALGO_TIMING_LOG
        AlgoTiming::event(ok ? AlgoTimingEvent_FormalPupilSuccess
                             : AlgoTimingEvent_FormalPupilFailure);
#endif
        if (shouldExitTask()) {
            return;
        }
        bool needFinalize = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (shouldExitTask()) {
                return;
            }
            m_rounds[roundIdx].frames[imgNo].pupilDetected = ok;
            if (!ok) {
                m_rounds[roundIdx].frames[imgNo].processed = true;
                m_rounds[roundIdx].frames[imgNo].failureReason = "detectPupil_failed";
#if ENABLE_ALGO_ROUND_DIAG_LOG
                logRoundDiagnosisLocked(roundIdx, QString("fail_img_%1_detectPupil").arg(imgNo));
#endif
                needFinalize = true;
            }
        }
        if (!ok) {
#if ENABLE_ALGO_APPEND_VERBOSE_LOG
            qDebug() << QString("appendImage: round=%1,No.=%2 pupil not found").arg(roundIdx).arg(imgNo);
#endif
            if (needFinalize) {
#if ENABLE_ALGO_TIMING_LOG
                formalTaskTiming.stop();
#endif
                tryFinalizeRoundLocked(roundIdx);
            }
            return;
        }

        PERF_END(task_total, QString("task detectPupil,round=%1,No.=%2").arg(roundIdx).arg(imgNo).toStdString());
        if (shouldExitTask()) {
            return;
        }

        PERF_START(task_processPicOfOneEye);
        const int angle = getImageAngle(imgNo);
        needFinalize = false;
        if (m_eye == singleDualEyeMode_Both) {
            cv::Mat pupilR;
#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
            setPupilFailDetailForceLog(true);
#endif
            bool rightProcessed = false;
            {
#if ENABLE_ALGO_TIMING_LOG
                ALGO_TIMING_SCOPE(AlgoTimingStage_ProcessRight);
#endif
                rightProcessed = processPicOfOneEye(rawImg, imgNo, pr, angle,
                                                     whichEye_Right, pupilR);
            }
            setPupilFailDetailForceLog(false);
#if ENABLE_ALGO_TIMING_LOG
            AlgoTiming::event(rightProcessed ? AlgoTimingEvent_ProcessSuccess
                                              : AlgoTimingEvent_ProcessFailure);
#endif
            if (!rightProcessed) {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (shouldExitTask()) {
                        return;
                    }
                    m_rounds[roundIdx].frames[imgNo].processed = true;
                    m_rounds[roundIdx].frames[imgNo].failureReason = "right_processPic_failed";
#if ENABLE_ALGO_ROUND_DIAG_LOG
                    logRoundDiagnosisLocked(roundIdx, QString("fail_img_%1_right").arg(imgNo));
#endif
                    needFinalize = true;
                }
                if (needFinalize) {
#if ENABLE_ALGO_TIMING_LOG
                    formalTaskTiming.stop();
#endif
                    tryFinalizeRoundLocked(roundIdx);
                }
                return;
            }

            if (shouldExitTask()) {
                return;
            }
            cv::Mat pupilL;
#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
            setPupilFailDetailForceLog(true);
#endif
            bool leftProcessed = false;
            {
#if ENABLE_ALGO_TIMING_LOG
                ALGO_TIMING_SCOPE(AlgoTimingStage_ProcessLeft);
#endif
                leftProcessed = processPicOfOneEye(rawImg, imgNo, pl, angle,
                                                    whichEye_Left, pupilL);
            }
            setPupilFailDetailForceLog(false);
#if ENABLE_ALGO_TIMING_LOG
            AlgoTiming::event(leftProcessed ? AlgoTimingEvent_ProcessSuccess
                                             : AlgoTimingEvent_ProcessFailure);
#endif
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (shouldExitTask()) {
                    return;
                }
                if (rightProcessed) {
                    m_rounds[roundIdx].setPupil(imgNo, pupilR, pr, whichEye_Right);
                }
                if (leftProcessed) {
                    m_rounds[roundIdx].setPupil(imgNo, pupilL, pl, whichEye_Left);
                    m_rounds[roundIdx].frames[imgNo].failureReason.clear();
                } else {
                    m_rounds[roundIdx].frames[imgNo].failureReason = "left_processPic_failed";
#if ENABLE_ALGO_ROUND_DIAG_LOG
                    logRoundDiagnosisLocked(roundIdx, QString("fail_img_%1_left").arg(imgNo));
#endif
                }
                m_rounds[roundIdx].frames[imgNo].processed = true;
                needFinalize = true;
                PERF_POINT(QString("validRight=%1,validLeft=%2").arg(QString::fromStdString(m_rounds[roundIdx].validRight.to_string())).arg(QString::fromStdString(m_rounds[roundIdx].validLeft.to_string())));
            }
            if (needFinalize) {
#if ENABLE_ALGO_TIMING_LOG
                formalTaskTiming.stop();
#endif
                tryFinalizeRoundLocked(roundIdx);
            }
        } else {
            enWhichEye eyeType = (m_eye == singleDualEyeMode_Right) ? whichEye_Right : whichEye_Left;
            stPupilInfo p = (m_eye == singleDualEyeMode_Right) ? pr : pl;
            cv::Mat pupil;
            if (shouldExitTask()) {
                return;
            }
#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
            setPupilFailDetailForceLog(true);
#endif
            bool processed = false;
            {
#if ENABLE_ALGO_TIMING_LOG
                ALGO_TIMING_SCOPE(eyeType == whichEye_Right
                                  ? AlgoTimingStage_ProcessRight
                                  : AlgoTimingStage_ProcessLeft);
#endif
                processed = processPicOfOneEye(rawImg, imgNo, p, angle,
                                                eyeType, pupil);
            }
            setPupilFailDetailForceLog(false);
#if ENABLE_ALGO_TIMING_LOG
            AlgoTiming::event(processed ? AlgoTimingEvent_ProcessSuccess
                                        : AlgoTimingEvent_ProcessFailure);
#endif
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (shouldExitTask()) {
                    return;
                }
                if (processed) {
                    m_rounds[roundIdx].setPupil(imgNo, pupil, p, eyeType);
                    m_rounds[roundIdx].frames[imgNo].failureReason.clear();
                } else {
                    m_rounds[roundIdx].frames[imgNo].failureReason =
                            (eyeType == whichEye_Right) ? "right_processPic_failed" : "left_processPic_failed";
#if ENABLE_ALGO_ROUND_DIAG_LOG
                    logRoundDiagnosisLocked(roundIdx, QString("fail_img_%1_single_eye").arg(imgNo));
#endif
                }
                m_rounds[roundIdx].frames[imgNo].processed = true;
                needFinalize = true;
                PERF_POINT(QString("validRight=%1,validLeft=%2").arg(QString::fromStdString(m_rounds[roundIdx].validRight.to_string())).arg(QString::fromStdString(m_rounds[roundIdx].validLeft.to_string())));
            }
            if (needFinalize) {
#if ENABLE_ALGO_TIMING_LOG
                formalTaskTiming.stop();
#endif
                tryFinalizeRoundLocked(roundIdx);
            }
        }
        PERF_END(task_processPicOfOneEye, QString("task processPicOfOneEye,round=%1,No.=%2").arg(roundIdx).arg(imgNo).toStdString());
        if (shouldExitTask()) {
            return;
        }

        PERF_END(task_total, QString("task total,round=%1,No.=%2").arg(roundIdx).arg(imgNo).toStdString());
    };

#if ENABLE_ALGO_APPEND_VERBOSE_LOG
    qDebug()<<"appendImage:task start-------------";
#endif
    pool->start(new QRunnableFunction(task));
#if ENABLE_ALGO_APPEND_VERBOSE_LOG
    qDebug()<<"appendImage:task end-------------"<<resultState<<endl;
#endif
    return resultState;
}


void CAlgo::onRoundCompleted(int roundIdx)
{
    PERF_POINT(__PRETTY_FUNCTION__ + QString(" roundIdx=%1").arg(roundIdx));
    tryFinalizeRoundLocked(roundIdx);
}


bool CAlgo::detectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l,
                                    bool _is_calc_vision, enSingleDualEyeMode _single_dual_eye)
{
#if ENABLE_ALGO_TIMING_LOG
    const int timingRound = _is_calc_vision ? AlgoTiming::currentRound() : -1;
    AlgoTimingContextScope timingContext(_is_calc_vision
                                         ? AlgoTimingPhase_Formal
                                         : AlgoTimingPhase_Preview,
                                         timingRound);
    AlgoTimingScope detectPupilTiming(AlgoTimingStage_DetectPupilTotal);
#endif
    Q_UNUSED(_age_range)
    UEP[1] = false;
    UEP[0] = false;

    Mat img;
    try {
        // 尝试 clone，如果内存非法会抛出异常
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_START(input_clone);
#endif
        img = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1, _img_data).clone();
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_END(input_clone, AlgoTimingStage_ImageClone);
#endif
    } catch (const cv::Exception& e) {
        logCritical(QString("CAlgo::detectPupil(): clone failed with OpenCV exception: %1").arg(e.what()));
        return false;
    } catch (...) {
        logCritical("CAlgo::detectPupil(): clone failed with unknown memory access error (Segfault?)");
        return false;
    }

    //const int RATIO = (_is_calc_vision ? 1 : 4);

    memset(&_pupil_info_r, 0, sizeof(stPupilInfo));
    memset(&_pupil_info_l, 0, sizeof(stPupilInfo));

    auto eye_flags=get_eye_flags(_single_dual_eye);

    //粗略找眼
    bool is_succ = false;
    bool is_succ_rough_r = false;
    bool is_succ_rough_l = false;
    PupilInfo rpupil,lpupil;

    PERF_START(detectPupil_getEyes);
#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
    setPupilFailDetailForceLog(_is_calc_vision);
#else
    setPupilFailDetailForceLog(false);
#endif
    try {
        if (isSimulatedEye) {
            // 模拟眼只有瞳孔，没有正常人眼眼部结构；强制走固定模拟眼框和旧版瞳孔识别路径。
            cv::Rect right_eye;
            cv::Rect left_eye;
            std::tie(right_eye, left_eye) = getModelEyes();
            {
                ALGO_TIMING_SCOPE(AlgoTimingStage_PupilRight);
                rpupil = createPupilFromRectForSimulatedEye(
                            img, _img_idx, right_eye, modeleye_wh_ratio, whichEye_Right);
            }
            {
                ALGO_TIMING_SCOPE(AlgoTimingStage_PupilLeft);
                lpupil = createPupilFromRectForSimulatedEye(
                            img, _img_idx, left_eye, modeleye_wh_ratio, whichEye_Left);
            }
        } else {
            // 预览允许宽松找眼兜底；正式转灯只走常规找眼，避免每张转灯图重复跑宽松 Haar。
            std::tie(rpupil, lpupil)=getEyesLegacyForCalc(img,_img_idx,humaneye_wh_ratio,modeleye_wh_ratio,
                                                          _is_calc_vision);
        }

        const bool isLShapePreview = !_is_calc_vision && opticalPathType_LShape == g_opticalPathType;
        // L 型预览阶段不用旧版中心区域卡单眼，避免不同机器 ROI 偏移导致可用瞳孔被误杀。
        const bool right_pupil_ok = rpupil.radius() > 0.0f
                && (isLShapePreview
                    ? isPreviewPupilInLShapeEffectiveRoi(rpupil, whichEye_Right)
                    : isNormalPupil(rpupil.center(), rpupil.whichEye));
        if(eye_flags.first && right_pupil_ok){
            _pupil_info_r=rpupil.getPupilInfoStruct();
            is_succ_rough_r=true;
        }

        const bool left_pupil_ok = lpupil.radius() > 0.0f
                && (isLShapePreview
                    ? isPreviewPupilInLShapeEffectiveRoi(lpupil, whichEye_Left)
                    : isNormalPupil(lpupil.center(), lpupil.whichEye));
        if(eye_flags.second && left_pupil_ok){
            _pupil_info_l=lpupil.getPupilInfoStruct();
            is_succ_rough_l=true;
        }
    } catch (...) {
        ALGO_ERROR_LOG(
            qCritical() << "exception unnkown: errno = " << errno
                        << ", str = " << strerror(errno)
        );
    }
    setPupilFailDetailForceLog(false);
    if (!_is_calc_vision) {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_PreviewGate);
#endif
        // 手持常规光路恢复旧版预览放行：双眼模式下任意一眼识别成功即可放行。
        const bool isGeneralPreview = opticalPathType_General == g_opticalPathType;
        bool rightRegionOk = false;
        bool leftRegionOk = false;
        bool orderOk = true;
        float yDiff = 0.0f;
        float xDiff = 0.0f;
        bool xDiffOk = true;
        bool yDiffOk = true;
        if (isGeneralPreview) {
            rightRegionOk = is_succ_rough_r;
            leftRegionOk = is_succ_rough_l;
            is_succ = is_succ_rough_r || is_succ_rough_l;
        } else if (eye_flags.first && eye_flags.second) {
            // L 型等箱体光路维持当前门控：双眼都成功且左右眼位置关系合理才放行。
            rightRegionOk = isPreviewPupilInExpectedRegion(rpupil, whichEye_Right);
            leftRegionOk = isPreviewPupilInExpectedRegion(lpupil, whichEye_Left);
            orderOk = rpupil.center().x < lpupil.center().x;
            xDiff = lpupil.center().x - rpupil.center().x;
            yDiff = std::abs(rpupil.center().y - lpupil.center().y);
            xDiffOk = xDiff >= MIN_PREVIEW_EYE_X_DIFF && xDiff <= MAX_PREVIEW_EYE_X_DIFF;
            yDiffOk = yDiff <= MAX_PREVIEW_EYE_Y_DIFF;
            is_succ = is_succ_rough_r
                    && is_succ_rough_l
                    && rightRegionOk
                    && leftRegionOk
                    && orderOk
                    && xDiffOk
                    && yDiffOk;
        } else if (eye_flags.first) {
            rightRegionOk = isPreviewPupilInSingleEyeRegion(rpupil, whichEye_Right);
            is_succ = is_succ_rough_r
                    && rightRegionOk;
        } else if (eye_flags.second) {
            leftRegionOk = isPreviewPupilInSingleEyeRegion(lpupil, whichEye_Left);
            is_succ = is_succ_rough_l
                    && leftRegionOk;
        }

#if ENABLE_PREVIEW_DIAG_LOG
        if (shouldLogPreviewDiag(is_succ)) {
            qDebug().noquote() << QString("PreviewGateDiag: final=%1,mode=%2,"
                                          "rough_r=%3,rough_l=%4,"
                                          "right_region=%5,left_region=%6,"
                                          "order_ok=%7,x_diff=%8,x_diff_ok=%9,"
                                          "y_diff=%10,y_diff_ok=%11,"
                                          "right={%12},left={%13}")
                                  .arg(is_succ ? 1 : 0)
                                  .arg(static_cast<int>(_single_dual_eye))
                                  .arg(is_succ_rough_r ? 1 : 0)
                                  .arg(is_succ_rough_l ? 1 : 0)
                                  .arg(rightRegionOk ? 1 : 0)
                                  .arg(leftRegionOk ? 1 : 0)
                                  .arg(orderOk ? 1 : 0)
                                  .arg(xDiff, 0, 'f', 1)
                                  .arg(xDiffOk ? 1 : 0)
                                  .arg(yDiff, 0, 'f', 1)
                                  .arg(yDiffOk ? 1 : 0)
                                  .arg(pupilBrief(rpupil))
                                  .arg(pupilBrief(lpupil));
            if (!is_succ) {
                const bool rightRoiOk = isPreviewPupilInExpectedRegion(rpupil, whichEye_Right);
                const bool leftRoiOk = isPreviewPupilInExpectedRegion(lpupil, whichEye_Left);
                QStringList pairRejectReasons;
                if (eye_flags.first && !is_succ_rough_r) {
                    pairRejectReasons << "right_not_pass";
                }
                if (eye_flags.second && !is_succ_rough_l) {
                    pairRejectReasons << "left_not_pass";
                }
                if (eye_flags.first && !rightRegionOk) {
                    pairRejectReasons << "right_region";
                }
                if (eye_flags.second && !leftRegionOk) {
                    pairRejectReasons << "left_region";
                }
                if (eye_flags.first && eye_flags.second && !orderOk) {
                    pairRejectReasons << "order";
                }
                if (eye_flags.first && eye_flags.second && !xDiffOk) {
                    pairRejectReasons << "x_diff";
                }
                if (eye_flags.first && eye_flags.second && !yDiffOk) {
                    pairRejectReasons << "y_diff";
                }

                qDebug().noquote() << QString("PreviewRejectReason: mode=%1,"
                                              "right_reason=%2,left_reason=%3,"
                                              "right_roi=%4,left_roi=%5,"
                                              "order_ok=%6,x_diff=%7,x_diff_ok=%8,"
                                              "y_diff=%9,y_diff_ok=%10,"
                                              "pair_reasons=%11")
                                      .arg(static_cast<int>(_single_dual_eye))
                                      .arg(previewEyeRejectReason(rpupil, whichEye_Right, is_succ_rough_r, rightRegionOk))
                                      .arg(previewEyeRejectReason(lpupil, whichEye_Left, is_succ_rough_l, leftRegionOk))
                                      .arg(rightRoiOk ? 1 : 0)
                                      .arg(leftRoiOk ? 1 : 0)
                                      .arg(orderOk ? 1 : 0)
                                      .arg(xDiff, 0, 'f', 1)
                                      .arg(xDiffOk ? 1 : 0)
                                      .arg(yDiff, 0, 'f', 1)
                                      .arg(yDiffOk ? 1 : 0)
                                      .arg(pairRejectReasons.join("|"));
            }
        }
#endif
    } else if (is_succ_rough_r || is_succ_rough_l) {
        // 正式计算阶段保持原有召回逻辑，后续再由 accOnePupil 做精确定位。
        is_succ = true;
    }
    PERF_END(detectPupil_getEyes,QString("detectPupil getEyes,index=%1").arg(_img_idx).toStdString());

    //计算图像时，瞳孔精确定位
    if (_is_calc_vision) {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_AccuratePupil);
#endif
        bool rightAccAttempted = false;
        bool leftAccAttempted = false;
        bool rightAccOk = false;
        bool leftAccOk = false;
        bool leftAccSkippedByShortCircuit = false;

        // 保持旧版“右眼成功后 || 短路”的行为，只把是否跳过左眼精定位记录下来，便于定位左眼空值来源。
        if (is_succ_rough_r) {
            rightAccAttempted = true;
            rightAccOk = accOnePupil(img, _pupil_info_r);
        }

        if (rightAccOk) {
            is_succ = true;
            leftAccSkippedByShortCircuit = is_succ_rough_l;
        } else {
            if (is_succ_rough_l) {
                leftAccAttempted = true;
                leftAccOk = accOnePupil(img, _pupil_info_l);
            }
            is_succ = leftAccOk;
        }

#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
        const bool needCalcTrace = eye_flags.first && eye_flags.second
                && (leftAccSkippedByShortCircuit
                    || !is_succ_rough_l
                    || (leftAccAttempted && !leftAccOk)
                    || !isNormalPupil(_pupil_info_l.center, whichEye_Left));
        if (needCalcTrace && shouldLogPupilFailDetail()) {
            qDebug().noquote() << QString("PupilFormalTrace: img=%1,mode=both,"
                                          "rough_r=%2,rough_l=%3,"
                                          "acc_attempt_r=%4,acc_ok_r=%5,"
                                          "acc_attempt_l=%6,acc_ok_l=%7,acc_l_skipped=%8,"
                                          "final_ok=%9,"
                                          "right_center=(%10,%11),right_radius=%12,"
                                          "left_center=(%13,%14),left_radius=%15")
                        .arg(_img_idx)
                        .arg(is_succ_rough_r ? 1 : 0)
                        .arg(is_succ_rough_l ? 1 : 0)
                        .arg(rightAccAttempted ? 1 : 0)
                        .arg(rightAccOk ? 1 : 0)
                        .arg(leftAccAttempted ? 1 : 0)
                        .arg(leftAccOk ? 1 : 0)
                        .arg(leftAccSkippedByShortCircuit ? 1 : 0)
                        .arg(is_succ ? 1 : 0)
                        .arg(_pupil_info_r.center.x, 0, 'f', 1)
                        .arg(_pupil_info_r.center.y, 0, 'f', 1)
                        .arg(_pupil_info_r.radius, 0, 'f', 1)
                        .arg(_pupil_info_l.center.x, 0, 'f', 1)
                        .arg(_pupil_info_l.center.y, 0, 'f', 1)
                        .arg(_pupil_info_l.radius, 0, 'f', 1);
        }
#endif
    }

#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
    if (!is_succ && _is_calc_vision) {
        cv::Rect rightEye;
        cv::Rect leftEye;
        detectHumanEyes(img, rightEye, leftEye);
        cv::Rect modelRightEye;
        cv::Rect modelLeftEye;
        std::tie(modelRightEye, modelLeftEye) = getModelEyes();

        if (eye_flags.first) {
            if (cvRectEmpty(rightEye)) {
                rightEye = modelRightEye;
            }
            logPupilFailDetail(img, _img_idx, rightEye, whichEye_Right,
                               humaneye_wh_ratio, "detectPupil_final_failed",
                               ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG);
        }

        if (eye_flags.second) {
            if (cvRectEmpty(leftEye)) {
                leftEye = modelLeftEye;
            }
            logPupilFailDetail(img, _img_idx, leftEye, whichEye_Left,
                               humaneye_wh_ratio, "detectPupil_final_failed",
                               ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG);
        }
    }
#endif

    if (!_is_calc_vision && !is_succ) {
        // 预览阶段严格保持 code 版传统算法优先；只有传统算法失败时，才低频调用模型兜底。
        stPupilInfo modelPupilRight;
        stPupilInfo modelPupilLeft;
        if (detectPupilByModelForPreview(img, _single_dual_eye,
                                         modelPupilRight, modelPupilLeft)) {
            _pupil_info_r = modelPupilRight;
            _pupil_info_l = modelPupilLeft;
            is_succ = true;
        }
    }

#if ENABLE_ALGO_TIMING_LOG
    detectPupilTiming.stop();
    if (!_is_calc_vision) {
        AlgoTiming::completePreviewFrame(is_succ);
    }
#endif
    return is_succ;
}

// 计算图像的曝光量信息
bool CAlgo::calcExposure(unsigned char* img_data, int /*img_idx*/,
                         stPupilInfo pupil_r, stPupilInfo pupil_l,
                         int& avg, bool& over_expo,
                         enSingleDualEyeMode eye_mode)
{
    avg = -1;
    over_expo = false;

    auto eye_flags = get_eye_flags(eye_mode);

    // C++11: 使用pair直接存储眼别标志和瞳孔信息引用
    using EyeData = std::pair<bool, const stPupilInfo*>;

    EyeData eye_datas[] = {
        {eye_flags.first,  &pupil_r},
        {eye_flags.second, &pupil_l}
    };

    cv::Mat img(IMG_HEIGHT, IMG_WIDTH, CV_8UC1, img_data);
    const cv::Rect imageRect(0, 0, img.cols, img.rows);

    std::vector<std::pair<float, int>> results;

    for (const auto& ed : eye_datas) {
        if (!ed.first || ed.second->radius <= 0) continue;

        const cv::Point center(cvRound(ed.second->center.x), cvRound(ed.second->center.y));
        if (!imageRect.contains(center)) {
            continue;
        }

        int width_half = std::max(5, static_cast<int>(ed.second->radius * 3 / 4));     // ROI 宽度的一半
        int height_half = std::max(3, static_cast<int>(width_half * 0.6));             // ROI 高度的一半

        int width = width_half * 2 + 1;         // ROI 宽度
        int height = height_half * 2 + 1;       // ROI 高度

        // 新算法只保证中心和半径，曝光测光必须裁剪到图像内，避免 avg=-1 卡住曝光状态机。
        cv::Rect roi(
            static_cast<int>(ed.second->center.x - width_half),
            static_cast<int>(ed.second->center.y - height_half),
            width,
            height
        );
        roi = roi & imageRect;

        if (roi.area() <= 0) {
            continue;
        }

        const cv::Mat mat_roi = img(roi);

        float a; int c;
        calcPupilAvgAndOverExpoOptimized(mat_roi, a, c);
        results.emplace_back(a, c);
    }

    if (results.empty()) {
        for (const auto& ed : eye_datas) {
            if (!ed.first || ed.second->radius <= 0) continue;

            const cv::Point center(cvRound(ed.second->center.x), cvRound(ed.second->center.y));
            if (!imageRect.contains(center)) {
                continue;
            }

            // 极端曝光或半径异常时，仍用中心 11x11 小窗给曝光调节器一个有效灰度反馈。
            cv::Rect roi(center.x - 5, center.y - 5, 11, 11);
            roi = roi & imageRect;
            if (roi.area() <= 0) {
                continue;
            }

            const cv::Mat mat_roi = img(roi);
            float a; int c;
            calcPupilAvgAndOverExpoOptimized(mat_roi, a, c);
            results.emplace_back(a, c);
        }
    }

    if (results.empty()) return false;

    auto best = *std::max_element(results.begin(), results.end(),
                                  [](const std::pair<float, int>& a,
                                     const std::pair<float, int>& b) {
                                      return a.first < b.first;
                                  });

    constexpr int thr_normal = 28 * PIX_COEF;
    constexpr int thr_simulated = 10 * PIX_COEF;

    auto divisor = (eye_mode == singleDualEyeMode_Both) ? 1 : 2;
    auto threshold = (isSimulatedEye ? thr_simulated : thr_normal) / divisor;

    avg = static_cast<int>(best.first);
    over_expo = best.second >= threshold;

    return true;
}

// 计算指定区域的均值及映光点像素数
void CAlgo::calcPupilAvgAndOverExpo(IplImage *_img_pupil, float &_avg, int &_count_over)
{
    _count_over = 0;

    double MinValue = 0.0;
    double MaxValue = 0.0;
    CvPoint MinLocation;
    CvPoint MaxLocation;
    cvMinMaxLoc(_img_pupil, &MinValue, &MaxValue, &MinLocation, &MaxLocation);
    //std::cout<<"crudeAlgorithm+++++++++179++++++++" << std::endl;
    double avalVal = MaxValue * 0.98;
    //double avalVal = 243 * 0.98;

    int data_temp = 0;
    float sum = 0.0;

    for (int i = 0; i < _img_pupil->width; i++)
    {
        for (int j = 0; j < _img_pupil->height; j++)
        {
            CvScalar s = cvGet2D(_img_pupil, j, i);

            data_temp = s.val[0];

            if (data_temp >= avalVal)
            {
                _count_over++;
            }

            sum += data_temp;
        }
    }

    _avg = sum / (_img_pupil->width * _img_pupil->height);
}

/**
 * @brief 计算瞳孔区域的平均灰度和过曝像素数（OpenCV优化版本）
 * @param img_pupil 输入的瞳孔区域图像（cv::Mat）
 * @param avg 输出的平均灰度值
 * @param count_over 输出的过曝像素数
 * @note 使用OpenCV内置SIMD优化函数，C++11规范
 */
void CAlgo::calcPupilAvgAndOverExpoOptimized(const cv::Mat& img_pupil,
                                              float& avg, int& count_over)
{
    // C++11: 使用CV_Assert进行调试断言
    CV_Assert(img_pupil.channels() == 1);
    CV_Assert(!img_pupil.empty());

    // C++11: 使用auto推导，避免显式类型
    // cv::mean返回Scalar，使用auto推导
    auto mean_scalar = cv::mean(img_pupil);
    avg = static_cast<float>(mean_scalar[0]);

    // C++11: 使用auto和列表初始化
    auto min_val = 0.0;
    auto max_val = 0.0;

    // 计算最大值用于阈值
    cv::minMaxLoc(img_pupil, &min_val, &max_val);

    // C++11: 使用auto推导，避免类型转换警告
    auto threshold = static_cast<uint8_t>(max_val * 0.98);

    // C++11: 使用lambda封装条件判断（展示特性，实际可直接计算）
    auto isOverExposed = [threshold](uint8_t val) -> bool {
        return val >= threshold;
    };

    // C++11: 使用OpenCV优化函数替代手动循环
    // 创建二值掩码（过曝像素为255，其他为0）
    cv::Mat over_mask;
    cv::threshold(img_pupil, over_mask, threshold, 255, cv::THRESH_BINARY);

    // C++11: 使用auto接收返回值
    count_over = cv::countNonZero(over_mask);
}


//保存每对灯的K值
void CAlgo::saveDSArr(const std::string &subDirName,
                      const double *DSR,
                      const double *DSL,
                      bool needLeft,
                      bool needRight) const
{
    if (!needLeft && !needRight) return;

    // 1. 拼目录并保证存在
    const QString logDir = QDir::cleanPath(
                QString::fromStdString(CAlgoIntf::getRootDirPath()) +
                QString::fromLatin1("/algoLog"));
    QDir().mkpath(logDir);

    // 2. 拼文件路径
    const QString fileName = QString::fromStdString(subDirName);
    const QString filePath =
            logDir + QString::fromLatin1("/log_") + fileName + QString::fromLatin1(".txt");

    // 3. 打开文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    // 4. 写数据
    QTextStream out(&file);
    out.setRealNumberPrecision(6);
    out.setRealNumberNotation(QTextStream::FixedNotation);

    enum { Size = 10 };

    if (needLeft) {
        out << "leftK = ";
        for (int i = 0; i < Size; ++i) {
            out << DSL[i];
            if (i + 1 != Size) out << ';';
        }
        out << '\n';
    }

    if (needRight) {
        out << "rightK = ";
        for (int i = 0; i < Size; ++i) {
            out << DSR[i];
            if (i + 1 != Size) out << ';';
        }
        out << '\n';
    }

    // 5. 显式 flush 并关闭（C++11 没有 QScopeGuard）
    out.flush();
    file.close();
}




// 保存瞳孔ROI图像
template<typename ImageArrayType>
void CAlgo::savePupilROIImages(enSingleDualEyeMode _single_dual_eye,
                              const ImageArrayType& pupilImgRight,
                              const ImageArrayType& pupilImgLeft,
                              const std::map<int, cv::Mat>& pupilROIImgRight,
                              const std::map<int, cv::Mat>& pupilROIImgLeft)
{
    //QString file_dir = QString("/mnt/hgfs/vmware_work/testdata/pupil_imgs");
    QString file_dir = QString::fromStdString(CAlgoIntf::getImageDirPath()) + QDir::separator() + QString::fromStdString(m_subDir);
    if (!QFile::exists(file_dir)) {
        QDir().mkpath(file_dir);
    }

    auto eye_flags = get_eye_flags(_single_dual_eye);

    if (eye_flags.first) {
        // 保存右眼原始图像 (pupilImgRight) - 使用安全的查找方法
        for (int i = 1; i <= FRAMES_PER_ROUND; i++) {
            // 使用 find 方法替代 operator[] 来安全地检查元素是否存在
            auto it = pupilImgRight.find(i);
            if (it != pupilImgRight.end() && !it->second.empty()) {
                QString file_path = file_dir + "/" + QString::asprintf("R_%02d.bmp", i);
                cv::imwrite(file_path.toStdString(), it->second);
            }
        }

        // 保存右眼ROI图像
        for (const auto& pair : pupilROIImgRight) {
            int index = pair.first;
            const cv::Mat& image = pair.second;

            if (!image.empty()) {
                QString file_path = file_dir + QDir::separator() + QString::asprintf("ROI_R_%02d.bmp", index);
                cv::imwrite(file_path.toStdString(), image);
            }
        }
    }

    if (eye_flags.second) {
        // 保存左眼原始图像 (pupilImgLeft) - 使用安全的查找方法
        for (int i = 1; i <= FRAMES_PER_ROUND; i++) {
            // 使用 find 方法替代 operator[] 来安全地检查元素是否存在
            auto it = pupilImgLeft.find(i);
            if (it != pupilImgLeft.end() && !it->second.empty()) {
                QString file_path = file_dir + "/" + QString::asprintf("L_%02d.bmp", i);
                cv::imwrite(file_path.toStdString(), it->second);
            }
        }

        // 保存左眼ROI图像
        for (const auto& pair : pupilROIImgLeft) {
            int index = pair.first;
            const cv::Mat& image = pair.second;

            if (!image.empty()) {
                QString file_path = file_dir + QDir::separator() + QString::asprintf("ROI_L_%02d.bmp", index);
                cv::imwrite(file_path.toStdString(), image);
            }
        }
    }
}

/*================  统一屈光计算核心实现（包含数据保存）  ========*/
enCalcResultState CAlgo::calculateRefraction(
    enAgeRange age_range,
    enSingleDualEyeMode _single_dual_eye,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoRight,
    const std::bitset<FRAME_ARRAY_SIZE>& validRight,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoLeft,
    const std::bitset<FRAME_ARRAY_SIZE>& validLeft,
    const double DSR[10],
    const double DSL[10],
    stVisionValue& vision,
    stVisionAbnormal& abnormal,
    bool enable_debug_output)
{
    // 屈光计算核心逻辑
    double sphR = 0, cylR = 0, axiR = 0, DR0 = 0, DR60 = 0, DR120 = 0;
    double sphL = 0, cylL = 0, axiL = 0, DL0 = 0, DL60 = 0, DL120 = 0;

    auto eye_flags = get_eye_flags(_single_dual_eye);

    // 右眼计算
    if (eye_flags.first && DSR != nullptr) {
        {
#if ENABLE_ALGO_TIMING_LOG
            ALGO_TIMING_SCOPE(AlgoTimingStage_RefractionRight);
#endif
            std::tie(DR0, DR60, DR120) = calRefraction(age_range, isHmMode, DSR);
        }
        std::tie(sphR, cylR, axiR) = calABD(DR0, DR60, DR120);
    }
    // 左眼计算
    if (eye_flags.second && DSL != nullptr) {
        {
#if ENABLE_ALGO_TIMING_LOG
            ALGO_TIMING_SCOPE(AlgoTimingStage_RefractionLeft);
#endif
            std::tie(DL0, DL60, DL120) = calRefraction(age_range, isHmMode, DSL);
        }
        std::tie(sphL, cylL, axiL) = calABD(DL0, DL60, DL120);
    }

    // 中间数据保存和调试输出（根据标志位控制）
    if (enable_debug_output) {
        saveIntermediateData(DSR, DSL, eye_flags.second, eye_flags.first,
                            sphL, cylL, axiL, sphR, cylR, axiR);
    }

    {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_ResultPostProcess);
#endif
        // 统一的结果处理流程
        CPointF pupilSpotAvgRight = {0.0, 0.0};
        CPointF pupilSpotAvgLeft = {0.0, 0.0};

        // === 修改：传入 validMask ===
        enCalcResultState st = calculatePupilAverageSpot(
            _single_dual_eye, maxGazeDeviation,
            pupilInfoRight, validRight, pupilSpotAvgRight,      // 传入 validRight
            pupilInfoLeft, validLeft, pupilSpotAvgLeft);        // 传入 validLeft

        if (st != calcResultState_Succ) {
            return st;
        }

        double rightAvg, leftAvg;
        // === 修改：传入 validMask ===
        calculatePupilAverageRadius(_single_dual_eye,
                                    pupilInfoRight, validRight, rightAvg,     // 传入 validRight
                                    pupilInfoLeft, validLeft, leftAvg);       // 传入 validLeft

        // === 修改：传入 validMask ===
        double pupil_distance = calcPupilDistance(_single_dual_eye,
                                                  pupilInfoRight, validRight,   // 传入 validRight
                                                  pupilInfoLeft, validLeft);    // 传入 validLeft

        fillVisionResults(vision, abnormal, _single_dual_eye,
                          sphL, cylL, axiL, sphR, cylR, axiR,
                          rightAvg, leftAvg,
                          pupilSpotAvgRight, pupilSpotAvgLeft,
                          pupil_distance);

        checkVisionAbnormal(vision, abnormal, _single_dual_eye,
                            DL0, DL60, DL120, DR0, DR60, DR120,
                            DSL, DSR);

        polishVisionResults(vision, abnormal, _single_dual_eye);
    }

    return calcResultState_Succ;
}

/*================  数据保存辅助函数  ========*/
void CAlgo::saveIntermediateData(const double DSR[10], const double DSL[10],
                                bool need_left, bool need_right,
                                double sphL, double cylL, double axiL,
                                double sphR, double cylR, double axiR)
{
    // 图像数据保存
    if (isSaveImg) {
        saveDSArr("", DSR, DSL, need_left, need_right);
    }

    // 调试输出
    if (need_left && DSL != nullptr) {
        qDebug() << "left EYE is : sphL = " << sphL << "; cylL = " << cylL
                  << "; axiL = " << axiL << endl;
        qDebug() << DSL[0] << "," << DSL[3] << "," << DSL[6] << ","
                  << DSL[1] << "," << DSL[4] << "," << DSL[7] << ","
                  << DSL[2] << "," << DSL[5] << "," << DSL[8] << ","
                  << DSL[9] << endl;
    }

    if (need_right && DSR != nullptr) {
        qDebug() << "right EYE is : sphR = " << sphR << "; cylR = " << cylR
                  << "; axiR = " << axiR << endl;
        qDebug() << DSR[0] << "," << DSR[3] << "," << DSR[6] << ","
                  << DSR[1] << "," << DSR[4] << "," << DSR[7] << ","
                  << DSR[2] << "," << DSR[5] << "," << DSR[8] << ","
                  << DSR[9] << endl;
    }
}

// 判断上睑下垂
void CAlgo::checkPtosis(enSingleDualEyeMode _single_dual_eye)
{
    auto eye_flags=get_eye_flags(_single_dual_eye);
    if (eye_flags.first) {
        double aspect_ratio_r = pyrAR.getRightAveraVal();
        UEP[1] = (aspect_ratio_r < PTOSIS_THRESH);
    } else {
        UEP[1] = false;
    }

    if (eye_flags.second) {
        double aspect_ratio_l = pyrAR.getLeftAveraVal();
        UEP[0] = (aspect_ratio_l < PTOSIS_THRESH);
    } else {
        UEP[0] = false;
    }
}


/**
 * @brief 填充视觉结果数据
 * @param vision 视觉数值结果
 * @param abnormal 视觉异常结果
 * @param _single_dual_eye 单双眼模式
 * @param sphL 左眼球镜度数
 * @param cylL 左眼柱镜度数
 * @param axiL 左眼轴位
 * @param sphR 右眼球镜度数
 * @param cylR 右眼柱镜度数
 * @param axiR 右眼轴位
 * @param pupil_distance 瞳孔距离
 */
void CAlgo::fillVisionResults(stVisionValue& vision, stVisionAbnormal& abnormal,
                             enSingleDualEyeMode _single_dual_eye,
                             double sphL, double cylL, int axiL,
                             double sphR, double cylR, int axiR,
                             double pupilRadiusAvgRight,
                             double pupilRadiusAvgLeft,
                             CPointF pupilSpotAvgRight,
                             CPointF pupilSpotAvgLeft,
                             double pupil_distance)
{
    // 初始化结构体
    memset(&vision, 0, sizeof(stVisionValue));
    memset(&abnormal, 0, sizeof(stVisionAbnormal));

    // 获取双眼标志
    auto eye_flags = get_eye_flags(_single_dual_eye);

    // 填充右眼数据（如果需要）
    if (eye_flags.first) {
        vision.RSph = sphR;
        vision.RCyl = cylR;
        vision.RAx = axiR;
        vision.RHz = pupilSpotAvgRight.x;
        // 使用平均映光点的y坐标作为垂直方向值
        vision.RVz = pupilSpotAvgRight.y;
        vision.RPs = pupilRadiusAvgRight * 2 * PIXEL_TO_PHY;
        vision.RPtosis = UEP[1];
    }

    // 填充左眼数据（如果需要）
    if (eye_flags.second) {
        vision.LSph = sphL;
        vision.LCyl = cylL;
        vision.LAx = axiL;
        // 使用平均映光点的x坐标作为水平方向值
        vision.LHz = pupilSpotAvgLeft.x;
        // 使用平均映光点的y坐标作为垂直方向值
        vision.LVz = pupilSpotAvgLeft.y;
        vision.LPs = pupilRadiusAvgLeft * 2 * PIXEL_TO_PHY;
        vision.LPtosis = UEP[0];
    }

    // 填充双眼共同数据
    if (eye_flags.first && eye_flags.second) {
        vision.PD = pupil_distance * PIXEL_TO_PHY;
    }
}

// 检查视觉异常
void CAlgo::checkVisionAbnormal(stVisionValue& vision, stVisionAbnormal& abnormal,
                               enSingleDualEyeMode _single_dual_eye,
                               double DL0, double DL60, double DL120,
                               double DR0, double DR60, double DR120,
                               const double DSL[10],const double DSR[10])
{
    auto eye_flags=get_eye_flags(_single_dual_eye);
    // 检查轴位不可信
    if (eye_flags.first) {
        checkAxisUntrusted(abnormal.RAxisUntrusted, DR0, DR60, DR120);
    }
    if (eye_flags.second) {
        checkAxisUntrusted(abnormal.LAxisUntrusted, DL0, DL60, DL120);
    }

    // 检查球镜过大
    if (eye_flags.first) {
        checkSphTooLarge(abnormal.RSphTooLarge, vision.RSph, DR0, DR60, DSR);
    }
    if (eye_flags.second) {
        checkSphTooLarge(abnormal.LSphTooLarge, vision.LSph, DL0, DL60, DSL);
    }

    // 检查上睑下垂导致的柱镜不可信
    if (eye_flags.first && UEP[1] && (vision.RCyl > 3 || vision.RCyl < -3)) {
        vision.RSph = DR0;
        vision.RCyl = 0;
        vision.RAx = 0;
        abnormal.RCylUntrusted = true;
    }
    if (eye_flags.second && UEP[0] && (vision.LCyl > 3 || vision.LCyl < -3)) {
        vision.LSph = DL0;
        vision.LCyl = 0;
        vision.LAx = 0;
        abnormal.LCylUntrusted = true;
    }

    // 检查轴位边界值
    if (eye_flags.first && (vision.RAx == 0 || vision.RAx == 180)) {
        abnormal.RAxisUntrusted = true;
    }
    if (eye_flags.second && (vision.LAx == 0 || vision.LAx == 180)) {
        abnormal.LAxisUntrusted = true;
    }
}

// 检查轴位不可信
void CAlgo::checkAxisUntrusted(bool& axis_untrusted, double D0, double D60, double D120)
{
    if (D0 == D60 || D0 == D120 || D60 == D120) {
        if (D0 == D60 && (std::fabs(D0 - D120) - 0.1) < M_FLOAT_PRECISION) {
            axis_untrusted = true;
        } else if (D0 == D120 && (std::fabs(D0 - D60) - 0.1) < M_FLOAT_PRECISION) {
            axis_untrusted = true;
        } else if (D60 == D120 && (std::fabs(D60 - D0) - 0.1) < M_FLOAT_PRECISION) {
            axis_untrusted = true;
        }
    }
}

// 检查球镜过大
void CAlgo::checkSphTooLarge(bool& sph_too_large, double& sph, double D0, double D60, const double DS[10])
{
    if (D0 == -8 && D60 == -8) {
        sph_too_large = true;
        sph = -7.5;
    } else if (DS[9] <= -0.0283 && DS[0] < DS[1] && DS[3] < DS[4]) {
        if (sph < 0) {  // 原代码是检查cylR < 0，但根据上下文应该是检查球镜
            sph_too_large = true;
            sph = -7.5;
        }
    }
}

// 修饰视觉结果
void CAlgo::polishVisionResults(stVisionValue& vision, stVisionAbnormal& abnormal,
                               enSingleDualEyeMode _single_dual_eye)
{
    auto eye_flags=get_eye_flags(_single_dual_eye);
    // 若柱镜度为0，则轴位设为0
    if (eye_flags.first && Util::compDouble(0, vision.RCyl) == 0) {
        vision.RAx = 0;
    }
    if (eye_flags.second && Util::compDouble(0, vision.LCyl) == 0) {
        vision.LAx = 0;
    }
}
