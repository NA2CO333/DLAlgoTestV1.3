#ifndef PUPIL_LIGHT_TRACKER_H
#define PUPIL_LIGHT_TRACKER_H

#include "perftimer.h"

#include <string>
#include <vector>

#include <opencv2/core.hpp>

// 坐标来源用于诊断和 DS 门控；它不改变轻量跟踪的数值计算。
enum PupilCoordinateSource
{
    PupilSource_Unknown = 0,
    PupilSource_DeepModel = 1,
    PupilSource_LightTrack = 2,
    PupilSource_CrossRoundLocal = 3,
    PupilSource_CrossRoundInterpolated = 4,
    PupilSource_CrossRoundOtherEye = 5,
    PupilSource_CrossRoundDirectReuse = 6,
    PupilSource_TraditionalFallback = 7,
    // 作为跨轮源时表示坐标已经经过当前图片最终精修确认。
    PupilSource_RoiRefined = 8
};

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
// 轻量小图匹配的诊断原因。该枚举只用于日志和统计，不参与匹配决策。
enum PupilLightMatchReason
{
    // None表示没有失败原因；当matchExecuted为true时即为匹配成功。
    PupilLightMatch_None = 0,
    // 没有进入matchOneEye的情况由上层根据matchExecuted单独归类。
    PupilLightMatch_NotAttempted,
    PupilLightMatch_PreviousEyeInvalid,
    PupilLightMatch_PreviousGradientInvalid,
    PupilLightMatch_TemplateRectInvalid,
    PupilLightMatch_TemplateOutsideGradient,
    PupilLightMatch_TemplateOutsideEyeHalf,
    PupilLightMatch_SearchRectInvalid,
    PupilLightMatch_SearchOutsideGradient,
    PupilLightMatch_SearchSmallerThanTemplate,
    PupilLightMatch_TemplateOrSearchEmpty,
    PupilLightMatch_MatchException,
    PupilLightMatch_ScoreBelowThreshold,
    PupilLightMatch_RetryScoreBelowThreshold,
    PupilLightMatch_RetryDeltaInconsistent,
    PupilLightMatchReason_Count
};

const char *pupilLightMatchReasonName(PupilLightMatchReason reason);

struct PupilLightMatchDiagnostic
{
    PupilLightMatchReason reason = PupilLightMatch_None;
    bool matchExecuted = false;
    cv::Point2f previousCenter;
    float previousRadius = 0.0F;
    cv::Rect templateRect;
    cv::Rect searchRect;
    cv::Rect searchBounds;
    cv::Rect paddedSearchBounds;
    cv::Rect gradientRoi;
    cv::Size templateSize;
    cv::Size searchSize;
    cv::Point2f paddedCenter;
    cv::Point2f matchedCenterPadded;
    cv::Point2f matchedCenterOriginal;
    int verticalPadding = 0;
    bool edgePaddingUsed = false;
    float threshold = -1.0F;
};
#endif

struct PupilLightEye
{
    bool detected = false;
    bool reliable = false;
    cv::Point2f center;
    float radius = 0.0F;
    float score = -1.0F;
    int source = PupilSource_Unknown;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    PupilLightMatchDiagnostic matchDiagnostic;
#endif
};

struct PupilLightFrame
{
    int lampNumber = -1;
    // 双眼锚点可来自不同拍摄图。isAnchor保留给旧的双眼公共路径；
    // 两个按眼别标志用于正式阶段的独立锚点恢复。
    bool isAnchor = false;
    bool isSubjectRightAnchor = false;
    bool isSubjectLeftAnchor = false;
    // 仅用于记录极限兜底来源：true 表示该眼锚点来自限量传统定位，
    // 不是 C800 模型输出。跟踪器本身只关心 isSubject*Anchor。
    bool isSubjectRightHaarAnchor = false;
    bool isSubjectLeftHaarAnchor = false;
    PupilLightEye subjectRight;
    PupilLightEye subjectLeft;
};

struct PupilLightTrackerOptions
{
    // 指定本次需要跟踪的受检者眼别。双眼模式保持两个开关均为true；
    // 单眼模式只启用目标眼，避免为不存在/不参与计算的对侧眼生成梯度和模板。
    bool trackSubjectRight = true;
    bool trackSubjectLeft = true;
    // 只缩放轻量跟踪使用的梯度图；模型输入和最终输出坐标保持原尺寸。
    float processingScale = 1.0F;
    // 正式路径固定使用400×160小图；关闭时保留兼容路径的局部ROI模式。
    bool useFullSmallFrame = false;
    // 模板半径约为瞳孔半径的1.4倍，保留瞳孔边缘和少量周边结构。
    float templateRadiusScale = 1.4F;
    // 搜索边距约为瞳孔半径的1.1倍。
    float searchMarginScale = 1.1F;
    // 以下上限均使用缩放后的处理图坐标；0表示不限制。
    int maximumTemplateHalf = 0;
    int maximumSearchMargin = 0;
    int maximumMovementPadding = 0;
    // 正常匹配要求较高可信度；单眼借用瞳距向量重试可适度放宽，
    // 但重试结果仍必须通过小图几何约束和后续129 ROI。
    float minimumMatchScore = 0.70F;
    float retryMatchScore = 0.60F;
    // 默认重跟踪全部非锚点；补充中间锚点时可只重算受影响区段（0基图像索引，含首尾）。
    int trackingBeginIndex = -1;
    int trackingEndIndex = -1;
};

struct PupilLightTrackerSummary
{
    std::string selectedMode;
    // 顺序跟踪低可信时，才会额外计算直接锚点候选。
    bool directFallbackTriggered = false;
    // 补充锚点后复用上一轮已计算的ROI梯度，避免再次处理22张图。
    bool gradientCacheReused = false;
    float processingScale = 1.0F;
    // 实际计算梯度的像素数相对于整张缩放图的比例。
    double gradientPixelRatio = 1.0;
    double gradientMs = 0.0;
    // 梯度预处理分阶段耗时，用于定位板端性能瓶颈。
    double gradientCropMs = 0.0;
    double gradientResizeMs = 0.0;
    double gradientBlurMs = 0.0;
    double gradientSobelMs = 0.0;
    double gradientMagnitudeMs = 0.0;
    double gradientNormalizeMs = 0.0;
    double directTrackMs = 0.0;
    double sequentialTrackMs = 0.0;
    double totalMs = 0.0;
    double scoreP05 = 0.0;
    int unreliableEyeCount = 0;
    int eyeVectorRetryCount = 0;
};

struct PupilLightTrackerCache
{
    // 缓存建立时的激活眼别。缓存只能在相同眼别配置下复用。
    bool tracksSubjectRight = true;
    bool tracksSubjectLeft = true;
    float processingScale = 0.0F;
    bool useFullSmallFrame = false;
    float templateRadiusScale = 0.0F;
    float searchMarginScale = 0.0F;
    int maximumTemplateHalf = 0;
    int maximumSearchMargin = 0;
    int maximumMovementPadding = 0;
    cv::Size sourceImageSize;
    // 原图坐标系ROI：先裁原图，再仅缩放眼部小块。
    cv::Rect subjectRightSourceRoi;
    cv::Rect subjectLeftSourceRoi;
    // 处理图坐标系ROI：用于跟踪坐标和局部梯度坐标之间的转换。
    cv::Rect subjectRightRoi;
    cv::Rect subjectLeftRoi;
    // 400×160小图中的左右眼搜索区域，允许中间区域有少量重叠。
    cv::Rect subjectRightSearchRoi;
    cv::Rect subjectLeftSearchRoi;
    // 正式路径左右眼共用同一张梯度图，避免每张照片重复计算两次梯度。
    std::vector<cv::Mat> sharedGradients;
    std::vector<cv::Mat> subjectRightGradients;
    std::vector<cv::Mat> subjectLeftGradients;

    void clear()
    {
        tracksSubjectRight = true;
        tracksSubjectLeft = true;
        processingScale = 0.0F;
        useFullSmallFrame = false;
        templateRadiusScale = 0.0F;
        searchMarginScale = 0.0F;
        maximumTemplateHalf = 0;
        maximumSearchMargin = 0;
        maximumMovementPadding = 0;
        sourceImageSize = cv::Size();
        subjectRightSourceRoi = cv::Rect();
        subjectLeftSourceRoi = cv::Rect();
        subjectRightRoi = cv::Rect();
        subjectLeftRoi = cv::Rect();
        subjectRightSearchRoi = cv::Rect();
        subjectLeftSearchRoi = cv::Rect();
        sharedGradients.clear();
        subjectRightGradients.clear();
        subjectLeftGradients.clear();
    }
};

class PupilLightTracker
{
public:
    // 流式正式拍摄使用：模型锚点就绪后，按当前已经到达的连续帧追加梯度。
    // grayImages 可以从 8 张逐步增长到 22 张；已生成的梯度不会重复计算。
    // frames 使用完整轮次长度，模型锚点位置必须提前填写并设置 isAnchor=true。
    bool extendGradientCache(
             const std::vector<cv::Mat> &grayImages,
             const std::vector<PupilLightFrame> &frames,
             const PupilLightTrackerOptions &options,
             PupilLightTrackerSummary *summary,
             std::string *errorMessage,
             PupilLightTrackerCache *gradientCache) const;

    // frames 中参考灯位必须提前填入模型输出，并设置 isAnchor=true。
    bool run(const std::vector<cv::Mat> &grayImages,
             std::vector<PupilLightFrame> *frames,
             const PupilLightTrackerOptions &options,
             PupilLightTrackerSummary *summary,
             std::string *errorMessage,
             PupilLightTrackerCache *gradientCache = nullptr) const;

    // 基于不可变锚点独立处理一张目标照片，不读取前一张照片的跟踪结果。
    bool trackOneFrameFromAnchor(
            const cv::Mat &anchorImage,
            const PupilLightFrame &anchorFrame,
            const cv::Mat &targetImage,
            int lampNumber,
            const PupilLightTrackerOptions &options,
            PupilLightFrame *result,
            PupilLightTrackerSummary *summary,
            std::string *errorMessage) const;

    // 只为一张不可变锚点预计算梯度；后续目标照片任务各自复制该缓存头，
    // 在自己的局部缓存中追加目标梯度，避免重复计算锚点和并发写共享缓存。
    bool prepareAnchorCache(
            const cv::Mat &anchorImage,
            const PupilLightFrame &anchorFrame,
            const PupilLightTrackerOptions &options,
            PupilLightTrackerCache *cache,
            std::string *errorMessage) const;

    // 使用已经准备好的单锚点缓存处理一张目标照片。
    bool trackOneFrameFromAnchorCached(
            const cv::Mat &anchorImage,
            const PupilLightFrame &anchorFrame,
            const cv::Mat &targetImage,
            int lampNumber,
            const PupilLightTrackerOptions &options,
            const PupilLightTrackerCache &anchorCache,
            PupilLightFrame *result,
            PupilLightTrackerSummary *summary,
            std::string *errorMessage,
            // 可选输出：返回目标图已经生成的单张梯度缓存。
            // 返回缓存只读，内部cv::Mat与本次任务共享数据，不复制像素。
            PupilLightTrackerCache *targetGradientCache = nullptr) const;

    // 跨轮单照片接口与普通锚点接口共用只读模板匹配实现。
    bool trackOneCrossRoundFrame(
            const cv::Mat &sourceImage,
            const PupilLightFrame &sourceFrame,
            const cv::Mat &targetImage,
            int lampNumber,
            PupilLightFrame *result,
            PupilLightTrackerSummary *summary,
            std::string *errorMessage) const;
};

#endif
