#ifndef PUPIL_CROSS_ROUND_TRACKER_H
#define PUPIL_CROSS_ROUND_TRACKER_H

#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "pupil_light_tracker.h"

struct PupilCrossRoundOptions
{
    // 双眼模式同时启用；单眼模式仅跟踪被测眼。
    // 单眼没有另一眼位移可借用，低可信时由上层触发模型/传统算法兜底。
    bool trackSubjectRight = true;
    bool trackSubjectLeft = true;
    // 离线30会话验证使用0.625；模型和最终结果仍保持原图坐标。
    float processingScale = 0.625F;
    float templateRadiusScale = 1.45F;
    // 跨轮头位变化大于同轮转灯，搜索边距放宽到约3个瞳孔半径。
    float searchMarginScale = 3.0F;
    // 以下上限使用原图坐标；0表示不限制。
    int maximumTemplateHalf = 0;
    int maximumSearchMargin = 0;
    // 大幅头位移动时先在低分辨率上做粗定位，再回到maximumSearchMargin
    // 范围内精匹配。默认关闭，避免影响当前正式流程与正常样本耗时。
    bool enableCoarsePrealignment = false;
    float coarseProcessingScale = 0.25F;
    int coarseSearchMargin = 224;
    float coarseMinimumMatchScore = 0.42F;
    // 低于此分数时借用另一只高可信眼的位移。
    float minimumMatchScore = 0.60F;
    // 默认保持原浮点L2梯度；fastL1Gradient 使用16位Sobel绝对值之和，
    // 避免浮点开方和全图归一化，仅用于经离线准确性对照后再启用。
    bool fastL1Gradient = false;
    // 正常样本优先仅匹配少量跨轮锚点，再对22张位移做分段线性插值。
    // 锚点索引由上层根据“实际拍摄照片编号”转换而来；跟踪器内部只使用
    // 图像数组索引，不应把它误解为硬件灯位。
    bool enableSparseAnchorFastPath = false;
    // 稀疏校正只失败一只眼时，复用另一只眼已经确认的稀疏轨迹，
    // 仅为失败眼运行22次完整匹配。默认关闭，先由独立测试工具验证。
    bool enableSingleEyeFullMatchFallback = false;
    // 单眼完整匹配优化的复用阈值可略低于正常稀疏快速路径；仍强制要求
    // 两张照片位移一致，避免把单张低分误识别当成跨轮运动。
    float singleEyeFallbackMinimumScore = 0.55F;
    // 儿童手持场景可启用：一只眼已经有两张一致稀疏轨迹，另一只眼只要
    // 存在一张与该位移相容的弱局部匹配，即可先采用“带动定位”快速路径。
    // 该路径不会把插值位置标记为可靠观测，是否采纳仍交给上层 DS 质量门。
    bool enableSingleEyeSparseCarry = false;
    // 被带动眼的一张弱局部证据最低分数；低于它不允许仅凭另一眼猜测。
    float singleEyeCarryMinimumScore = 0.45F;
    // 弱局部证据相对高可信眼位移的最大差异，使用原图像素坐标。
    float singleEyeCarryMaximumDeltaDifference = 12.0F;
    // 高可信眼整体跨轮位移的最大允许长度，防止错误模板造成大范围拖拽。
    float singleEyeCarryMaximumDisplacement = 24.0F;
    // 使用0基图像数组索引。正式流程由上层把实际照片编号转换后写入。
    std::vector<int> sparseAnchorIndices;
    // 同一只眼三个锚点的跨轮位移差异超过此值时，认为头位轨迹不连续。
    float sparseMaximumDeltaSpread = 16.0F;
    // 同一锚点左右眼位移差异超过此值时，认为双眼快速校正不一致。
    float sparseMaximumStereoDeltaDifference = 16.0F;
};

// 每个稀疏照片的逐眼匹配诊断。imageIndex始终是内部0基数组索引，
// 调用者负责映射并在日志中显示实际拍摄照片编号。
struct PupilCrossRoundSparseDiagnostic
{
    int imageIndex = -1;
    float subjectRightScore = -1.0F;
    float subjectLeftScore = -1.0F;
    cv::Point2f subjectRightDelta;
    cv::Point2f subjectLeftDelta;
    bool subjectRightAccepted = false;
    bool subjectLeftAccepted = false;
};

struct PupilCrossRoundSummary
{
    bool sourceTemplateCacheReused = false;
    bool sparseFastPathAttempted = false;
    bool sparseFastPathUsed = false;
    bool fullMatchFallbackTriggered = false;
    // 完整兜底是否只匹配一只失败眼，而非双眼44次都重新匹配。
    bool singleEyeFullMatchFallbackUsed = false;
    // 一眼两张一致、另一眼一张弱局部验证时，是否采用单眼带动快速路径。
    bool singleEyeSparseCarryUsed = false;
    // 本轮结果是否足够完整，可安全作为下一轮跨轮校正的源轨迹。
    bool sourceTrajectoryUpdated = false;
    bool sparseTemplateCacheReused = false;
    bool fullTemplateCacheReused = false;
    double sourceTemplateMs = 0.0;
    double targetGradientMs = 0.0;
    double coarseGradientMs = 0.0;
    double coarseMatchMs = 0.0;
    double matchMs = 0.0;
    double totalMs = 0.0;
    double scoreP05 = 0.0;
    int localMatchEyeCount = 0;
    int interpolatedEyeCount = 0;
    int otherEyeFallbackCount = 0;
    int directReuseFallbackCount = 0;
    // 仅统计“粗定位成功且随后进入精匹配”的眼数，便于离线判断大位移
    // 补偿是否真正生效。
    int coarsePrealignedEyeCount = 0;
    // 用稀疏插值直接沿用的眼数，以及实际执行完整匹配的眼数。
    int sparseTrajectoryEyeCount = 0;
    int fullMatchRequestedEyeCount = 0;
    // 被带动眼中实际有弱局部验证的位置数；其余只用于连续定位。
    int singleEyeCarryLocalEvidenceCount = 0;
    int singleEyeCarryPositionOnlyEyeCount = 0;
    // 单眼完整匹配时，只有两张一致锚点可作为观测；其余插值坐标仅供
    // 连续定位，必须排除在DS等有效瞳孔计算之外。
    int interpolatedPositionOnlyEyeCount = 0;
    // 仅记录稀疏路径被拒绝的原因，便于判断是低分、位移离散还是双眼不一致。
    std::string sparseRejectReason;
    std::string singleEyeCarryRejectReason;
    std::vector<PupilCrossRoundSparseDiagnostic> sparseDiagnostics;
};

struct PupilCrossRoundTemplate
{
    cv::Mat gradient;
    cv::Point2f center;
    float radius = 0.0F;
};

struct PupilCrossRoundCache
{
    bool tracksSubjectRight = true;
    bool tracksSubjectLeft = true;
    float processingScale = 0.0F;
    int maximumTemplateHalf = 0;
    int maximumSearchMargin = 0;
    bool fastL1Gradient = false;
    bool coarsePrealignment = false;
    float coarseProcessingScale = 0.0F;
    cv::Size sourceImageSize;
    // 当前缓存实际生成模板的图像索引；稀疏与完整缓存不能混用。
    std::vector<int> templateIndices;
    std::vector<PupilCrossRoundTemplate> subjectRight;
    std::vector<PupilCrossRoundTemplate> subjectLeft;

    void clear()
    {
        tracksSubjectRight = true;
        tracksSubjectLeft = true;
        processingScale = 0.0F;
        maximumTemplateHalf = 0;
        maximumSearchMargin = 0;
        fastL1Gradient = false;
        coarsePrealignment = false;
        coarseProcessingScale = 0.0F;
        sourceImageSize = cv::Size();
        templateIndices.clear();
        subjectRight.clear();
        subjectLeft.clear();
    }
};

// 后续轮流式预处理缓存：每张照片分别按首轮左右眼的预测搜索区域生成梯度。
// 该缓存只保存定位用的梯度，不保存原始图片；第22张到齐后的跨轮结算可直接复用。
struct PupilCrossRoundTargetGradient
{
    cv::Mat gradient;
    cv::Rect searchRect;
};

struct PupilCrossRoundTargetGradientCache
{
    bool tracksSubjectRight = true;
    bool tracksSubjectLeft = true;
    float processingScale = 0.0F;
    int maximumTemplateHalf = 0;
    int maximumSearchMargin = 0;
    bool fastL1Gradient = false;
    // 每张目标图一份低分辨率整图梯度，供少量稀疏锚点做大范围粗定位。
    // 不保存原始图像，避免正式流式流程增加图片内存副本。
    bool coarsePrealignment = false;
    float coarseProcessingScale = 0.0F;
    std::vector<cv::Mat> coarseGradients;
    cv::Size imageSize;
    std::vector<PupilCrossRoundTargetGradient> subjectRight;
    std::vector<PupilCrossRoundTargetGradient> subjectLeft;

    void clear()
    {
        tracksSubjectRight = true;
        tracksSubjectLeft = true;
        processingScale = 0.0F;
        maximumTemplateHalf = 0;
        maximumSearchMargin = 0;
        fastL1Gradient = false;
        coarsePrealignment = false;
        coarseProcessingScale = 0.0F;
        coarseGradients.clear();
        imageSize = cv::Size();
        subjectRight.clear();
        subjectLeft.clear();
    }
};

class PupilCrossRoundTracker
{
public:
    // 供第2、3轮采集线程调用：每张照片到达时立即生成对应眼的目标梯度。
    // sourceFrame必须来自当前跨轮源轨迹；imageIndex使用0基数组索引。
    bool prepareTargetGradientFrame(
            const cv::Mat &targetImage,
            const PupilLightFrame &sourceFrame,
            int imageIndex,
            int frameCount,
            const PupilCrossRoundOptions &options,
            PupilCrossRoundTargetGradientCache *cache,
            double *gradientMs,
            std::string *errorMessage) const;

    // sourceFrames是最近一轮可信的“模型锚点+轻量跟踪/跨轮校正”轨迹。
    // targetFrames输出后续一轮的22张校正结果，不再调用模型。
    bool run(const std::vector<cv::Mat> &sourceImages,
             const std::vector<PupilLightFrame> &sourceFrames,
             const std::vector<cv::Mat> &targetImages,
             std::vector<PupilLightFrame> *targetFrames,
             const PupilCrossRoundOptions &options,
             PupilCrossRoundSummary *summary,
             std::string *errorMessage,
             // 稀疏与完整模板分开缓存，避免稀疏失败后被44次完整模板覆盖。
             PupilCrossRoundCache *sparseTemplateCache = nullptr,
             PupilCrossRoundCache *fullTemplateCache = nullptr,
             // 可选的后续轮流式目标梯度缓存；缓存不完整时自动逐项即时计算。
             const PupilCrossRoundTargetGradientCache *targetGradientCache = nullptr) const;
};

#endif
