#include "perftimer.h"
#include "pupil_light_tracker.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include <opencv2/imgproc.hpp>

// 当前工程随代码只分发了OpenCV 3.4.12的部分头文件，但板端完整
// libopencv_video.so.3.4.12已经存在。这里声明本模块唯一使用的稳定API，
// 避免错误混入3.2或4.5.5头文件；所有参数均显式传入，不依赖默认值。
namespace cv
{
void calcOpticalFlowPyrLK(InputArray prevImg,
                          InputArray nextImg,
                          InputArray prevPts,
                          InputOutputArray nextPts,
                          OutputArray status,
                          OutputArray err,
                          Size winSize,
                          int maxLevel,
                          TermCriteria criteria,
                          int flags,
                          double minEigThreshold);
}

namespace
{
// 正式400×160小图的最大模板半径为13，垂直补边覆盖完整模板。
static constexpr int FORMAL_SMALL_VERTICAL_PADDING = 13;

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#define SET_MATCH_FAILURE(result, reasonValue) \
    (result).setFailureReason(reasonValue)
#else
#define SET_MATCH_FAILURE(result, reasonValue) \
    do { } while (0)
#endif

struct MatchResult
{
    cv::Point2f center;
    float score = -1.0F;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    PupilLightMatchReason failureReason = PupilLightMatch_None;
    bool matchExecuted = false;
    PupilLightMatchDiagnostic diagnostic;

    // 统一同步底层结果和诊断结果，避免日志看到过期的失败原因。
    void setFailureReason(PupilLightMatchReason reason)
    {
        failureReason = reason;
        diagnostic.reason = reason;
    }
#endif
};

struct CandidateResult
{
    std::vector<PupilLightFrame> frames;
    double elapsedMs = 0.0;
    double scoreP05 = 0.0;
    int eyeVectorRetryCount = 0;
};

void setError(std::string *errorMessage, const std::string &text)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = text;
    }
}

double percentile(const std::vector<double> &values, double ratio)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const double position = ratio * static_cast<double>(sorted.size() - 1);
    const size_t lower = static_cast<size_t>(std::floor(position));
    const size_t upper = static_cast<size_t>(std::ceil(position));
    if (lower == upper)
    {
        return sorted[lower];
    }
    const double fraction = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - fraction) + sorted[upper] * fraction;
}

double elapsedMilliseconds(int64 start)
{
    return (cv::getTickCount() - start) * 1000.0 / cv::getTickFrequency();
}

int applyMaximum(int value, int maximum)
{
    return maximum > 0 ? std::min(value, maximum) : value;
}

struct GradientPreparationTiming
{
    double cropMs = 0.0;
    double resizeMs = 0.0;
    double blurMs = 0.0;
    double sobelMs = 0.0;
    double magnitudeMs = 0.0;
    double normalizeMs = 0.0;
};

struct GradientWorkspace
{
    // 这些中间Mat会在22张图之间反复复用，避免循环内频繁申请和释放内存。
    cv::Mat resized;
    cv::Mat blurred;
    cv::Mat dx;
    cv::Mat dy;
    cv::Mat magnitude;
};

void makeGradient(const cv::Mat &gray,
                  GradientWorkspace *workspace,
                  cv::Mat *normalized,
                  GradientPreparationTiming *timing)
{
    int64 start = cv::getTickCount();
    cv::GaussianBlur(gray, workspace->blurred, cv::Size(3, 3), 0.0);
    timing->blurMs += elapsedMilliseconds(start);

    start = cv::getTickCount();
    cv::Sobel(workspace->blurred, workspace->dx, CV_32F, 1, 0, 3);
    cv::Sobel(workspace->blurred, workspace->dy, CV_32F, 0, 1, 3);
    timing->sobelMs += elapsedMilliseconds(start);

    start = cv::getTickCount();
    cv::magnitude(workspace->dx, workspace->dy, workspace->magnitude);
    timing->magnitudeMs += elapsedMilliseconds(start);

    start = cv::getTickCount();
    cv::normalize(workspace->magnitude,
                  *normalized,
                  0.0,
                  255.0,
                  cv::NORM_MINMAX,
                  CV_8U);
    timing->normalizeMs += elapsedMilliseconds(start);
}

bool centeredRect(const cv::Point2f &center,
                  int halfWidth,
                  const cv::Size &imageSize,
                  cv::Rect *rect)
{
    const int centerX = cvRound(center.x);
    const int centerY = cvRound(center.y);
    const cv::Rect candidate(centerX - halfWidth,
                             centerY - halfWidth,
                             halfWidth * 2 + 1,
                             halfWidth * 2 + 1);
    const cv::Rect imageRect(0, 0, imageSize.width, imageSize.height);
    if ((candidate & imageRect) != candidate)
    {
        return false;
    }
    *rect = candidate;
    return true;
}

MatchResult matchOneEye(const cv::Mat &previousGradient,
                        const cv::Mat &currentGradient,
                        const cv::Rect &gradientRoi,
                        const cv::Rect &searchBounds,
                        const cv::Size &processingImageSize,
                        const PupilLightEye &previousEye,
                        const cv::Point2f &predictedTargetCenter,
                        bool usePredictedTargetWindow,
                        const PupilLightTrackerOptions &options)
{
    MatchResult result;
    result.center = previousEye.center;
    const int verticalPadding = options.useFullSmallFrame
            ? FORMAL_SMALL_VERTICAL_PADDING : 0;
    const cv::Size matchingImageSize(
            processingImageSize.width,
            processingImageSize.height + verticalPadding * 2);
    const cv::Rect matchingGradientRoi(
            gradientRoi.x,
            options.useFullSmallFrame ? 0 : gradientRoi.y,
            gradientRoi.width,
            options.useFullSmallFrame
                ? processingImageSize.height + verticalPadding * 2
                : gradientRoi.height);
    const cv::Point2f matchingPreviousCenter(
            previousEye.center.x,
            previousEye.center.y + verticalPadding);
    const cv::Point2f matchingPredictedCenter(
            predictedTargetCenter.x,
            predictedTargetCenter.y + verticalPadding);
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    result.diagnostic.previousCenter = previousEye.center;
    result.diagnostic.previousRadius = previousEye.radius;
    result.diagnostic.paddedCenter = matchingPreviousCenter;
    result.diagnostic.searchBounds = searchBounds;
    result.diagnostic.verticalPadding = verticalPadding;
    result.diagnostic.gradientRoi = matchingGradientRoi;
    result.diagnostic.threshold = options.minimumMatchScore;
#endif
    // 仅记录异常输入，不提前改变旧算法的处理路径。
    if (!previousEye.detected || previousEye.radius <= 0.0F)
    {
        SET_MATCH_FAILURE(result, PupilLightMatch_PreviousEyeInvalid);
    }
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    // 测试副本把梯度无效单独归类；正式工程不增加新的判断路径。
    if (previousGradient.empty() || currentGradient.empty())
    {
        SET_MATCH_FAILURE(result, PupilLightMatch_PreviousGradientInvalid);
        return result;
    }
#endif
    // 正式400×160小图使用动态模板，避免所有眼睛都固定为27×27；
    // 旧兼容路径继续沿用原来的缩放规则。
    const int minimumTemplateHalf = options.useFullSmallFrame
            ? 7 : std::max(4, cvRound(18.0F * options.processingScale));
    const int minimumSearchMargin =
        std::max(4, cvRound(12.0F * options.processingScale));
    const int templateRadius = cvRound(
            previousEye.radius * options.templateRadiusScale);
    const int templateHalf = options.useFullSmallFrame
            ? std::min(13, std::max(7, templateRadius))
            : applyMaximum(std::max(minimumTemplateHalf, templateRadius),
                           options.maximumTemplateHalf);
    const int searchMargin = applyMaximum(
        std::max(minimumSearchMargin,
                 cvRound(previousEye.radius * options.searchMarginScale)),
        options.maximumSearchMargin);
    // 搜索矩形表示模板左上角的候选区域。补边后将其限制为原图
    // y=0～height-1对应的中心范围，防止匹配到补边本身。
    const cv::Rect matchingSearchBounds = options.useFullSmallFrame
            ? cv::Rect(searchBounds.x,
                       verticalPadding - templateHalf,
                       searchBounds.width,
                       processingImageSize.height + templateHalf * 2)
            : searchBounds;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    result.diagnostic.paddedSearchBounds = matchingSearchBounds;
#endif

    cv::Rect templateRect;
    cv::Rect searchRect;
    if (!centeredRect(matchingPreviousCenter,
                      templateHalf,
                      matchingImageSize,
                      &templateRect))
    {
        SET_MATCH_FAILURE(result, PupilLightMatch_TemplateRectInvalid);
        return result;
    }
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    result.diagnostic.templateRect = templateRect;
    result.diagnostic.edgePaddingUsed = verticalPadding > 0
            && (templateRect.y < verticalPadding
                || templateRect.br().y
                    > verticalPadding + processingImageSize.height);
#endif
    if ((templateRect & matchingGradientRoi) != templateRect)
    {
        SET_MATCH_FAILURE(result, PupilLightMatch_TemplateOutsideGradient);
        return result;
    }
    if ((templateRect & matchingSearchBounds) != templateRect)
    {
        SET_MATCH_FAILURE(result, PupilLightMatch_TemplateOutsideEyeHalf);
        return result;
    }

    // 正式小图的首次匹配直接搜索对应半区；单眼重试才使用预测中心
    // 附近的窗口。旧兼容路径仍使用预测中心附近窗口。
    if (options.useFullSmallFrame && !usePredictedTargetWindow) {
        searchRect = matchingSearchBounds;
    } else if (options.useFullSmallFrame) {
        // 补边后允许重试窗口在上下边缘裁剪，而不是因完整窗口越界
        // 直接返回search_rect_invalid；最终候选仍由matchingSearchBounds限制。
        const int searchHalf = templateHalf + searchMargin;
        searchRect = cv::Rect(
                cvRound(matchingPredictedCenter.x) - searchHalf,
                cvRound(matchingPredictedCenter.y) - searchHalf,
                searchHalf * 2 + 1,
                searchHalf * 2 + 1);
        searchRect &= cv::Rect(0, 0,
                               matchingImageSize.width,
                               matchingImageSize.height);
    } else if (!centeredRect(matchingPredictedCenter,
                             templateHalf + searchMargin,
                             matchingImageSize,
                             &searchRect)) {
        SET_MATCH_FAILURE(result, PupilLightMatch_SearchRectInvalid);
        return result;
    }

    // 左右眼在400×160小图中使用独立搜索区；搜索窗口越界时裁剪，
    // 但模板本身必须完整落在对应眼区内，避免跨眼误匹配。
    searchRect &= matchingSearchBounds;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    result.diagnostic.searchRect = searchRect;
#endif
    if (searchRect.empty()) {
        SET_MATCH_FAILURE(result, PupilLightMatch_SearchRectInvalid);
        return result;
    }
    if ((searchRect & matchingGradientRoi) != searchRect) {
        SET_MATCH_FAILURE(result, PupilLightMatch_SearchOutsideGradient);
        return result;
    }
    if (searchRect.width < templateRect.width
            || searchRect.height < templateRect.height) {
        SET_MATCH_FAILURE(result, PupilLightMatch_SearchSmallerThanTemplate);
        return result;
    }

    const cv::Rect localTemplateRect(
        templateRect.x - matchingGradientRoi.x,
        templateRect.y - matchingGradientRoi.y,
        templateRect.width,
        templateRect.height);
    const cv::Rect localSearchRect(
        searchRect.x - matchingGradientRoi.x,
        searchRect.y - matchingGradientRoi.y,
        searchRect.width,
        searchRect.height);
    const cv::Mat templateImage = previousGradient(localTemplateRect);
    const cv::Mat searchImage = currentGradient(localSearchRect);
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    result.diagnostic.templateSize = templateImage.size();
    result.diagnostic.searchSize = searchImage.size();
#endif
    // 边界瞳孔或异常半径可能令局部模板比搜索ROI更大。OpenCV 3.4在
    // 此情况下会直接抛异常并终止进程，必须把它作为本次匹配失败处理。
    if (templateImage.empty() || searchImage.empty())
    {
        SET_MATCH_FAILURE(result, PupilLightMatch_TemplateOrSearchEmpty);
        return result;
    }
    if (templateImage.rows > searchImage.rows
            || templateImage.cols > searchImage.cols)
    {
        SET_MATCH_FAILURE(result, PupilLightMatch_SearchSmallerThanTemplate);
        return result;
    }
    cv::Mat response;
    double maximum = -1.0;
    cv::Point maximumLocation;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    // 进入OpenCV模板匹配前即标记为已执行，异常时也能区分“未调用”和“调用失败”。
    result.matchExecuted = true;
    result.diagnostic.matchExecuted = true;
#endif
    // OpenCV异常只在异常输入时触发；正常匹配路径和结果保持不变。
    try
    {
        cv::matchTemplate(searchImage,
                          templateImage,
                          response,
                          cv::TM_CCOEFF_NORMED);
        cv::minMaxLoc(response,
                      nullptr,
                      &maximum,
                      nullptr,
                      &maximumLocation);
    }
    catch (const cv::Exception &)
    {
        SET_MATCH_FAILURE(result, PupilLightMatch_MatchException);
        return result;
    }
    const cv::Point2f matchedCenterPadded(
        static_cast<float>(searchRect.x + maximumLocation.x + templateHalf),
        static_cast<float>(searchRect.y + maximumLocation.y + templateHalf));
    result.center = matchedCenterPadded;
    result.center.y -= static_cast<float>(verticalPadding);
    if (options.useFullSmallFrame) {
        result.center.y = std::max(
                0.0F,
                std::min(result.center.y,
                         static_cast<float>(processingImageSize.height - 1)));
    }
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    result.diagnostic.matchedCenterPadded = matchedCenterPadded;
    result.diagnostic.matchedCenterOriginal = result.center;
    result.diagnostic.edgePaddingUsed = verticalPadding > 0
            && (templateRect.y < verticalPadding
                || templateRect.br().y
                    > verticalPadding + processingImageSize.height);
#endif
    result.score = static_cast<float>(maximum);
    SET_MATCH_FAILURE(result, result.score >= options.minimumMatchScore
            ? PupilLightMatch_None
            : PupilLightMatch_ScoreBelowThreshold);
    return result;
}

#undef SET_MATCH_FAILURE

std::vector<int> anchorIndices(const std::vector<PupilLightFrame> &frames)
{
    std::vector<int> indices;
    for (size_t index = 0; index < frames.size(); ++index)
    {
        if (frames[index].isAnchor)
        {
            indices.push_back(static_cast<int>(index));
        }
    }
    return indices;
}

float interpolateScalar(int frameIndex,
                        const std::vector<int> &anchors,
                        const std::vector<float> &values)
{
    if (frameIndex <= anchors.front())
    {
        return values.front();
    }
    if (frameIndex >= anchors.back())
    {
        return values.back();
    }
    for (size_t segment = 0; segment + 1 < anchors.size(); ++segment)
    {
        if (frameIndex >= anchors[segment] &&
            frameIndex <= anchors[segment + 1])
        {
            const float ratio =
                static_cast<float>(frameIndex - anchors[segment]) /
                static_cast<float>(anchors[segment + 1] - anchors[segment]);
            return values[segment] * (1.0F - ratio) +
                   values[segment + 1] * ratio;
        }
    }
    return values.back();
}

cv::Point2f interpolatePoint(int frameIndex,
                             const std::vector<int> &anchors,
                             const std::vector<cv::Point2f> &values)
{
    if (frameIndex <= anchors.front())
    {
        return values.front();
    }
    if (frameIndex >= anchors.back())
    {
        return values.back();
    }
    for (size_t segment = 0; segment + 1 < anchors.size(); ++segment)
    {
        if (frameIndex >= anchors[segment] &&
            frameIndex <= anchors[segment + 1])
        {
            const float ratio =
                static_cast<float>(frameIndex - anchors[segment]) /
                static_cast<float>(anchors[segment + 1] - anchors[segment]);
            return values[segment] * (1.0F - ratio) +
                   values[segment + 1] * ratio;
        }
    }
    return values.back();
}

const PupilLightEye &frameEye(const PupilLightFrame &frame, bool subjectRight)
{
    return subjectRight ? frame.subjectRight : frame.subjectLeft;
}

bool isEyeTracked(const PupilLightTrackerOptions &options, bool subjectRight)
{
    return subjectRight ? options.trackSubjectRight : options.trackSubjectLeft;
}

bool isTrackedEyeValid(const PupilLightFrame &frame,
                       const PupilLightTrackerOptions &options,
                       bool subjectRight)
{
    if (!isEyeTracked(options, subjectRight))
    {
        return true;
    }
    const PupilLightEye &eye = frameEye(frame, subjectRight);
    return eye.detected && eye.radius > 0.0F;
}

int eyeSupportHalf(const PupilLightEye &eye,
                   const PupilLightTrackerOptions &options)
{
    const int minimumTemplateHalf = options.useFullSmallFrame
            ? 7 : std::max(4, cvRound(18.0F * options.processingScale));
    const int minimumSearchMargin =
        std::max(4, cvRound(12.0F * options.processingScale));
    const int templateRadius = cvRound(
            eye.radius * options.templateRadiusScale);
    const int templateHalf = options.useFullSmallFrame
            ? std::min(13, std::max(7, templateRadius))
            : applyMaximum(std::max(minimumTemplateHalf, templateRadius),
                           options.maximumTemplateHalf);
    const int searchMargin = applyMaximum(
        std::max(minimumSearchMargin,
                 cvRound(eye.radius * options.searchMarginScale)),
        options.maximumSearchMargin);
    return templateHalf + searchMargin;
}

cv::Rect buildEyeGradientRoi(const std::vector<PupilLightFrame> &frames,
                             const std::vector<int> &anchors,
                             bool subjectRight,
                             const cv::Size &processingImageSize,
                             const PupilLightTrackerOptions &options)
{
    float minimumX = std::numeric_limits<float>::max();
    float minimumY = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    float maximumY = std::numeric_limits<float>::lowest();
    float maximumRadius = 0.0F;
    int maximumSupportHalf = 0;
    for (int anchor : anchors)
    {
        const PupilLightEye &eye = frameEye(frames[anchor], subjectRight);
        minimumX = std::min(minimumX, eye.center.x);
        minimumY = std::min(minimumY, eye.center.y);
        maximumX = std::max(maximumX, eye.center.x);
        maximumY = std::max(maximumY, eye.center.y);
        maximumRadius = std::max(maximumRadius, eye.radius);
        maximumSupportHalf =
            std::max(maximumSupportHalf, eyeSupportHalf(eye, options));
    }

    // 除模板和搜索窗口外，再保留约三个瞳孔半径，覆盖首尾锚点外推和手持轻微移动。
    const int movementPadding = applyMaximum(
        std::max(cvRound(32.0F * options.processingScale),
                 cvRound(maximumRadius * 3.0F)),
        options.maximumMovementPadding);
    const int padding = maximumSupportHalf + movementPadding + 4;
    const int left = static_cast<int>(std::floor(minimumX)) - padding;
    const int top = static_cast<int>(std::floor(minimumY)) - padding;
    const int right = static_cast<int>(std::ceil(maximumX)) + padding + 1;
    const int bottom = static_cast<int>(std::ceil(maximumY)) + padding + 1;
    return cv::Rect(left, top, right - left, bottom - top) &
           cv::Rect(0, 0, processingImageSize.width, processingImageSize.height);
}

int sourceRoiAlignment(float processingScale)
{
    // 0.625等于5/8。原图边界按8像素对齐后，局部resize的采样网格与整图resize一致。
    if (std::fabs(processingScale - 0.625F) < 1e-6F)
    {
        return 8;
    }
    if (std::fabs(processingScale - 0.5F) < 1e-6F)
    {
        return 2;
    }
    return 1;
}

int alignDown(int value, int alignment)
{
    return (value / alignment) * alignment;
}

int alignUp(int value, int alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

cv::Rect sourceRoiFromProcessingRoi(const cv::Rect &processingRoi,
                                    const cv::Size &sourceImageSize,
                                    float processingScale)
{
    if (processingScale >= 0.999F)
    {
        return processingRoi &
               cv::Rect(0, 0, sourceImageSize.width, sourceImageSize.height);
    }

    const int alignment = sourceRoiAlignment(processingScale);
    int left = static_cast<int>(
        std::floor(processingRoi.x / processingScale));
    int top = static_cast<int>(
        std::floor(processingRoi.y / processingScale));
    int right = static_cast<int>(
        std::ceil((processingRoi.x + processingRoi.width) / processingScale));
    int bottom = static_cast<int>(
        std::ceil((processingRoi.y + processingRoi.height) / processingScale));
    left = std::max(0, alignDown(left, alignment));
    top = std::max(0, alignDown(top, alignment));
    right = std::min(sourceImageSize.width, alignUp(right, alignment));
    bottom = std::min(sourceImageSize.height, alignUp(bottom, alignment));
    return cv::Rect(left,
                    top,
                    std::max(0, right - left),
                    std::max(0, bottom - top));
}

cv::Rect processingRoiFromSourceRoi(const cv::Rect &sourceRoi,
                                    const cv::Size &processingImageSize,
                                    float processingScale)
{
    const cv::Rect result(
        cvRound(sourceRoi.x * processingScale),
        cvRound(sourceRoi.y * processingScale),
        cvRound(sourceRoi.width * processingScale),
        cvRound(sourceRoi.height * processingScale));
    return result &
           cv::Rect(0,
                    0,
                    processingImageSize.width,
                    processingImageSize.height);
}

bool cacheSupportsAnchors(const PupilLightTrackerCache &cache,
                          const std::vector<PupilLightFrame> &frames,
                          const std::vector<int> &anchors,
                          const PupilLightTrackerOptions &options)
{
    for (int anchor : anchors)
    {
        const cv::Size processingSize(
            cvRound(cache.sourceImageSize.width * options.processingScale),
            cvRound(cache.sourceImageSize.height * options.processingScale));
        const auto supportInsideCache = [&](bool subjectRight) {
            if (!isEyeTracked(options, subjectRight))
            {
                return true;
            }
            const PupilLightEye &eye = frameEye(frames[anchor], subjectRight);
            cv::Rect support;
            const cv::Rect &cachedRoi = subjectRight
                ? cache.subjectRightRoi : cache.subjectLeftRoi;
            return centeredRect(eye.center, eyeSupportHalf(eye, options),
                                processingSize, &support)
                    && (support & cachedRoi) == support;
        };
        if (!supportInsideCache(true) || !supportInsideCache(false))
        {
            return false;
        }
    }
    return true;
}

bool gradientCacheIsUsable(const PupilLightTrackerCache &cache,
                           const std::vector<cv::Mat> &grayImages,
                           const std::vector<PupilLightFrame> &workingFrames,
                           const std::vector<int> &anchors,
                           const PupilLightTrackerOptions &options)
{
    if (std::fabs(cache.processingScale - options.processingScale) > 1e-6F ||
        std::fabs(cache.templateRadiusScale
                  - options.templateRadiusScale) > 1e-6F ||
        std::fabs(cache.searchMarginScale
                  - options.searchMarginScale) > 1e-6F ||
        cache.tracksSubjectRight != options.trackSubjectRight ||
        cache.tracksSubjectLeft != options.trackSubjectLeft ||
        cache.maximumTemplateHalf != options.maximumTemplateHalf ||
        cache.maximumSearchMargin != options.maximumSearchMargin ||
         cache.maximumMovementPadding != options.maximumMovementPadding ||
         cache.useFullSmallFrame != options.useFullSmallFrame ||
         cache.sourceImageSize != grayImages.front().size() ||
         (options.useFullSmallFrame &&
          cache.sharedGradients.size() != grayImages.size()) ||
        (options.trackSubjectRight &&
          (cache.subjectRightGradients.size() != grayImages.size() ||
           cache.subjectRightSourceRoi.empty() ||
           cache.subjectRightRoi.empty() ||
           cache.subjectRightSearchRoi.empty())) ||
        (options.trackSubjectLeft &&
          (cache.subjectLeftGradients.size() != grayImages.size() ||
           cache.subjectLeftSourceRoi.empty() ||
           cache.subjectLeftRoi.empty() ||
           cache.subjectLeftSearchRoi.empty())))
    {
        return false;
    }
    for (const cv::Mat &image : grayImages)
    {
        if (image.size() != cache.sourceImageSize)
        {
            return false;
        }
    }
    return cacheSupportsAnchors(cache, workingFrames, anchors, options);
}

bool preparedAnchorCacheIsUsable(
        const PupilLightTrackerCache &cache,
        const std::vector<cv::Mat> &grayImages,
        const std::vector<PupilLightFrame> &workingFrames,
        const std::vector<int> &anchors,
        const PupilLightTrackerOptions &options)
{
    // 单照片任务固定使用[锚点,目标图]两张图，缓存中只能已经存在锚点梯度。
    if (grayImages.size() != 2 || workingFrames.size() != 2
            || anchors.size() != 1 || anchors.front() != 0) {
        return false;
    }
    if (std::fabs(cache.processingScale - options.processingScale) > 1e-6F
            || std::fabs(cache.templateRadiusScale
                         - options.templateRadiusScale) > 1e-6F
            || std::fabs(cache.searchMarginScale
                         - options.searchMarginScale) > 1e-6F
            || cache.tracksSubjectRight != options.trackSubjectRight
            || cache.tracksSubjectLeft != options.trackSubjectLeft
            || cache.maximumTemplateHalf != options.maximumTemplateHalf
            || cache.maximumSearchMargin != options.maximumSearchMargin
         || cache.maximumMovementPadding != options.maximumMovementPadding
         || cache.useFullSmallFrame != options.useFullSmallFrame
         || cache.sourceImageSize != grayImages.front().size()
         || (options.useFullSmallFrame
             && cache.sharedGradients.size() != 1)
         || grayImages[1].size() != cache.sourceImageSize) {
        return false;
    }
    if (options.trackSubjectRight
            && (cache.subjectRightGradients.size() != 1
                || cache.subjectRightSourceRoi.empty()
                || cache.subjectRightRoi.empty()
                || cache.subjectRightSearchRoi.empty())) {
        return false;
    }
    if (options.trackSubjectLeft
            && (cache.subjectLeftGradients.size() != 1
                || cache.subjectLeftSourceRoi.empty()
                || cache.subjectLeftRoi.empty()
                || cache.subjectLeftSearchRoi.empty())) {
        return false;
    }
    return cacheSupportsAnchors(cache, workingFrames, anchors, options);
}

void initializeGradientCache(const cv::Size &sourceImageSize,
                             const std::vector<PupilLightFrame> &workingFrames,
                             const std::vector<int> &anchors,
                             const PupilLightTrackerOptions &options,
                             PupilLightTrackerCache *cache)
{
    cache->clear();
    cache->tracksSubjectRight = options.trackSubjectRight;
    cache->tracksSubjectLeft = options.trackSubjectLeft;
    cache->processingScale = options.processingScale;
    cache->useFullSmallFrame = options.useFullSmallFrame;
    cache->templateRadiusScale = options.templateRadiusScale;
    cache->searchMarginScale = options.searchMarginScale;
    cache->maximumTemplateHalf = options.maximumTemplateHalf;
    cache->maximumSearchMargin = options.maximumSearchMargin;
    cache->maximumMovementPadding = options.maximumMovementPadding;
    cache->sourceImageSize = sourceImageSize;
    const cv::Size processingImageSize(
        cvRound(cache->sourceImageSize.width * options.processingScale),
        cvRound(cache->sourceImageSize.height * options.processingScale));
    const auto prepareEyeRoi = [&](bool subjectRight,
                                   cv::Rect *sourceRoi,
                                   cv::Rect *processingRoi) {
        if (!isEyeTracked(options, subjectRight))
        {
            *sourceRoi = cv::Rect();
            *processingRoi = cv::Rect();
            return;
        }
        const cv::Rect desiredRoi = options.useFullSmallFrame
            ? cv::Rect(0, 0, processingImageSize.width,
                       processingImageSize.height)
            : buildEyeGradientRoi(workingFrames, anchors, subjectRight,
                                  processingImageSize, options);
        *sourceRoi = sourceRoiFromProcessingRoi(
            desiredRoi, cache->sourceImageSize, options.processingScale);
        *processingRoi = processingRoiFromSourceRoi(
            *sourceRoi, processingImageSize, options.processingScale);
    };
    prepareEyeRoi(true, &cache->subjectRightSourceRoi,
                  &cache->subjectRightRoi);
    prepareEyeRoi(false, &cache->subjectLeftSourceRoi,
                  &cache->subjectLeftRoi);

    if (options.useFullSmallFrame) {
        // 受检者右眼位于小图左侧，左眼位于小图右侧；中间保留重叠。
        // 正式400×160小图只保留原有水平分区，垂直方向覆盖完整图像，
        // 避免上下边缘附近的合法模板被人为20像素边距拒绝。
        cache->subjectRightSearchRoi = cv::Rect(
                0, 0, std::min(220, processingImageSize.width),
                std::max(0, processingImageSize.height));
        cache->subjectLeftSearchRoi = cv::Rect(
                std::min(180, processingImageSize.width), 0,
                std::min(220, std::max(0, processingImageSize.width - 180)),
                std::max(0, processingImageSize.height));
        const cv::Rect imageRect(0, 0, processingImageSize.width,
                                 processingImageSize.height);
        cache->subjectRightSearchRoi &= imageRect;
        cache->subjectLeftSearchRoi &= imageRect;
    } else {
        cache->subjectRightSearchRoi = cache->subjectRightRoi;
        cache->subjectLeftSearchRoi = cache->subjectLeftRoi;
    }

}

void appendGradientCache(const std::vector<cv::Mat> &grayImages,
                         size_t beginIndex,
                         const PupilLightTrackerOptions &options,
                         PupilLightTrackerCache *cache,
                         GradientPreparationTiming *timing)
{
    if (options.trackSubjectRight)
    {
        cache->subjectRightGradients.reserve(grayImages.size());
    }
    if (options.trackSubjectLeft)
    {
        cache->subjectLeftGradients.reserve(grayImages.size());
    }
    GradientWorkspace rightWorkspace;
    GradientWorkspace leftWorkspace;
    for (size_t imageIndex = beginIndex;
         imageIndex < grayImages.size();
         ++imageIndex)
    {
        const cv::Mat &image = grayImages[imageIndex];
        int64 start = cv::getTickCount();
        // cv::Mat ROI只是创建共享原图数据的头部，不复制像素。
        const cv::Mat rightSource = options.trackSubjectRight
            ? image(cache->subjectRightSourceRoi) : cv::Mat();
        const cv::Mat leftSource = options.trackSubjectLeft
            ? image(cache->subjectLeftSourceRoi) : cv::Mat();
        timing->cropMs += elapsedMilliseconds(start);

        cv::Mat rightInput;
        cv::Mat leftInput;
        if (options.processingScale < 0.999F)
        {
            start = cv::getTickCount();
            if (options.trackSubjectRight)
            {
                cv::resize(rightSource, rightWorkspace.resized,
                           cache->subjectRightRoi.size(), 0.0, 0.0,
                           cv::INTER_AREA);
                rightInput = rightWorkspace.resized;
            }
            if (options.trackSubjectLeft)
            {
                cv::resize(leftSource, leftWorkspace.resized,
                           cache->subjectLeftRoi.size(), 0.0, 0.0,
                           cv::INTER_AREA);
                leftInput = leftWorkspace.resized;
            }
            timing->resizeMs += elapsedMilliseconds(start);
        }
        else
        {
            if (options.trackSubjectRight)
            {
                rightInput = rightSource;
            }
            if (options.trackSubjectLeft)
            {
                leftInput = leftSource;
            }
        }

        if (options.useFullSmallFrame) {
            // 正式路径左右眼共享一张400×160梯度图，避免同一照片重复
            // 缩放、模糊、Sobel和归一化。
            cv::Mat fullInput;
            if (options.processingScale < 0.999F) {
                const cv::Size fullSmallSize = options.trackSubjectRight
                        ? cache->subjectRightRoi.size()
                        : cache->subjectLeftRoi.size();
                cv::resize(image, fullInput,
                           fullSmallSize, 0.0, 0.0,
                           cv::INTER_AREA);
            } else {
                fullInput = image;
            }
            cv::Mat paddedFullInput;
            // 正式小图上下补边，使y=0/159附近也能提取完整模板；
            // 左右不补边，水平分区仍由searchBounds严格控制。
            cv::copyMakeBorder(
                    fullInput,
                    paddedFullInput,
                    FORMAL_SMALL_VERTICAL_PADDING,
                    FORMAL_SMALL_VERTICAL_PADDING,
                    0,
                    0,
                    cv::BORDER_REFLECT_101);
            cache->sharedGradients.emplace_back();
            makeGradient(paddedFullInput, &rightWorkspace,
                         &cache->sharedGradients.back(), timing);
            if (options.trackSubjectRight) {
                cache->subjectRightGradients.push_back(
                        cache->sharedGradients.back());
            }
            if (options.trackSubjectLeft) {
                cache->subjectLeftGradients.push_back(
                        cache->sharedGradients.back());
            }
            continue;
        }

        if (options.trackSubjectRight)
        {
            cache->subjectRightGradients.emplace_back();
            makeGradient(rightInput, &rightWorkspace,
                         &cache->subjectRightGradients.back(), timing);
        }
        if (options.trackSubjectLeft)
        {
            cache->subjectLeftGradients.emplace_back();
            makeGradient(leftInput, &leftWorkspace,
                         &cache->subjectLeftGradients.back(), timing);
        }
    }
}

void prepareGradientCache(const std::vector<cv::Mat> &grayImages,
                          const std::vector<PupilLightFrame> &workingFrames,
                          const std::vector<int> &anchors,
                          const PupilLightTrackerOptions &options,
                          PupilLightTrackerCache *cache,
                          GradientPreparationTiming *timing)
{
    initializeGradientCache(grayImages.front().size(),
                            workingFrames,
                            anchors,
                            options,
                            cache);
    appendGradientCache(grayImages, 0, options, cache, timing);
}

void trackStep(int previousIndex,
               int currentIndex,
               const PupilLightTrackerCache &gradientCache,
               const std::vector<int> &anchors,
               const std::vector<float> &rightRadii,
               const std::vector<float> &leftRadii,
               const PupilLightTrackerOptions &options,
               int *eyeVectorRetryCount,
               std::vector<PupilLightFrame> *frames)
{
    const PupilLightFrame &previous = (*frames)[previousIndex];
    const cv::Size processingImageSize(
        cvRound(gradientCache.sourceImageSize.width *
                options.processingScale),
        cvRound(gradientCache.sourceImageSize.height *
                options.processingScale));
    MatchResult right;
    MatchResult left;
    if (options.trackSubjectRight)
    {
        right = matchOneEye(
            gradientCache.subjectRightGradients[previousIndex],
            gradientCache.subjectRightGradients[currentIndex],
            gradientCache.subjectRightRoi,
            gradientCache.subjectRightSearchRoi,
            processingImageSize,
            previous.subjectRight, previous.subjectRight.center,
            false, options);
    }
    if (options.trackSubjectLeft)
    {
        left = matchOneEye(
            gradientCache.subjectLeftGradients[previousIndex],
            gradientCache.subjectLeftGradients[currentIndex],
            gradientCache.subjectLeftRoi,
            gradientCache.subjectLeftSearchRoi,
            processingImageSize,
            previous.subjectLeft, previous.subjectLeft.center,
            false, options);
    }

    bool rightReliable = options.trackSubjectRight
        && right.score >= options.minimumMatchScore;
    bool leftReliable = options.trackSubjectLeft
        && left.score >= options.minimumMatchScore;
    cv::Point2f rightDelta = options.trackSubjectRight
        ? right.center - previous.subjectRight.center : cv::Point2f();
    cv::Point2f leftDelta = options.trackSubjectLeft
        ? left.center - previous.subjectLeft.center : cv::Point2f();

    const float maximumDeltaDifference = std::max(
            8.0F, static_cast<float>(options.maximumMovementPadding));
    const float maximumEyeVectorShift = std::max(
            12.0F,
            static_cast<float>(options.maximumMovementPadding) * 1.5F);
    const auto acceptEyeVectorRetry = [&](bool subjectRight,
                                          const MatchResult &retry,
                                          const cv::Point2f &referenceDelta) {
        if (retry.score < options.retryMatchScore) {
            // 重试使用独立的0.60门槛；把失败重试结果复制回最终眼结果，
            // 避免上层继续读取第一次Match的旧失败原因。
            if (subjectRight) {
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
                right.diagnostic = retry.diagnostic;
                if (retry.matchExecuted) {
                    // 已真正执行Match但低于0.60，才归类为重试低分。
                    right.setFailureReason(
                            PupilLightMatch_RetryScoreBelowThreshold);
                    right.diagnostic.threshold = options.retryMatchScore;
                }
#endif
            } else {
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
                left.diagnostic = retry.diagnostic;
                if (retry.matchExecuted) {
                    // 已真正执行Match但低于0.60，才归类为重试低分。
                    left.setFailureReason(
                            PupilLightMatch_RetryScoreBelowThreshold);
                    left.diagnostic.threshold = options.retryMatchScore;
                }
#endif
            }
            return false;
        }
        const cv::Point2f retryDelta = retry.center -
                (subjectRight ? previous.subjectRight.center
                               : previous.subjectLeft.center);
        if (std::hypot(retryDelta.x - referenceDelta.x,
                       retryDelta.y - referenceDelta.y)
                > maximumDeltaDifference) {
            // 分数已达到0.60，但位移与参考眼不一致，不能采纳本次重试。
            if (subjectRight) {
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
                right.diagnostic = retry.diagnostic;
                right.setFailureReason(
                        PupilLightMatch_RetryDeltaInconsistent);
                right.diagnostic.threshold = options.retryMatchScore;
#endif
            } else {
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
                left.diagnostic = retry.diagnostic;
                left.setFailureReason(
                        PupilLightMatch_RetryDeltaInconsistent);
                left.diagnostic.threshold = options.retryMatchScore;
#endif
            }
            return false;
        }
        if (subjectRight) {
            right = retry;
            rightDelta = retryDelta;
            rightReliable = true;
            // 重试结果已通过当前重试门槛，日志应反映最终采用结果。
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
            right.setFailureReason(PupilLightMatch_None);
            right.diagnostic.threshold = options.retryMatchScore;
#endif
        } else {
            left = retry;
            leftDelta = retryDelta;
            leftReliable = true;
            // 重试结果已通过当前重试门槛，日志应反映最终采用结果。
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
            left.setFailureReason(PupilLightMatch_None);
            left.diagnostic.threshold = options.retryMatchScore;
#endif
        }
        if (eyeVectorRetryCount != nullptr) {
            ++(*eyeVectorRetryCount);
        }
        return true;
    };

    // 一眼可靠时只对失败眼追加一次匹配。预测中心来自当前可靠眼的位移
    // 和锚点瞳距向量，避免把失败眼直接复制成上一帧位置。
    if (rightReliable && !leftReliable && options.trackSubjectLeft) {
        PupilLightEye predictedLeft = previous.subjectLeft;
        predictedLeft.center += rightDelta;
        const MatchResult retryLeft = matchOneEye(
                gradientCache.subjectLeftGradients[previousIndex],
                gradientCache.subjectLeftGradients[currentIndex],
                gradientCache.subjectLeftRoi,
                gradientCache.subjectLeftSearchRoi,
                processingImageSize,
                previous.subjectLeft, predictedLeft.center,
                true,
                options);
        acceptEyeVectorRetry(false, retryLeft, rightDelta);
    } else if (leftReliable && !rightReliable && options.trackSubjectRight) {
        PupilLightEye predictedRight = previous.subjectRight;
        predictedRight.center += leftDelta;
        const MatchResult retryRight = matchOneEye(
                gradientCache.subjectRightGradients[previousIndex],
                gradientCache.subjectRightGradients[currentIndex],
                gradientCache.subjectRightRoi,
                gradientCache.subjectRightSearchRoi,
                processingImageSize,
                previous.subjectRight, predictedRight.center,
                true,
                options);
        acceptEyeVectorRetry(true, retryRight, leftDelta);
    }

    // 双眼初次结果若瞳距变化异常，只保留分数更高的一眼，再按瞳距向量
    // 对另一眼追加一次局部匹配；单眼模式不会进入此几何分支。
    if (rightReliable && leftReliable && options.trackSubjectRight
            && options.trackSubjectLeft) {
        const cv::Point2f previousVector =
                previous.subjectLeft.center - previous.subjectRight.center;
        const cv::Point2f currentVector = left.center - right.center;
        const bool vectorBad =
                std::hypot(currentVector.x - previousVector.x,
                           currentVector.y - previousVector.y)
                    > maximumEyeVectorShift
                || std::hypot(leftDelta.x - rightDelta.x,
                              leftDelta.y - rightDelta.y)
                    > maximumDeltaDifference;
        if (vectorBad) {
            if (right.score >= left.score) {
                leftReliable = false;
                PupilLightEye predictedLeft = previous.subjectLeft;
                predictedLeft.center += rightDelta;
                const MatchResult retryLeft = matchOneEye(
                        gradientCache.subjectLeftGradients[previousIndex],
                        gradientCache.subjectLeftGradients[currentIndex],
                        gradientCache.subjectLeftRoi,
                        gradientCache.subjectLeftSearchRoi,
                        processingImageSize,
                        previous.subjectLeft, predictedLeft.center,
                        true,
                        options);
                acceptEyeVectorRetry(false, retryLeft, rightDelta);
            } else {
                rightReliable = false;
                PupilLightEye predictedRight = previous.subjectRight;
                predictedRight.center += leftDelta;
                const MatchResult retryRight = matchOneEye(
                        gradientCache.subjectRightGradients[previousIndex],
                        gradientCache.subjectRightGradients[currentIndex],
                        gradientCache.subjectRightRoi,
                        gradientCache.subjectRightSearchRoi,
                        processingImageSize,
                        previous.subjectRight, predictedRight.center,
                        true,
                        options);
                acceptEyeVectorRetry(true, retryRight, leftDelta);
            }
        }
    }

    // 旧兼容路径保留另一眼位移借用；正式小图模式的失败眼必须保持
    // detected=false，不能伪造坐标进入原图129 ROI。
    if (!options.useFullSmallFrame
            && options.trackSubjectRight && options.trackSubjectLeft)
    {
        if (rightReliable && !leftReliable)
        {
            leftDelta = rightDelta;
        }
        else if (!rightReliable && leftReliable)
        {
            rightDelta = leftDelta;
        }
        else if (!rightReliable && !leftReliable)
        {
            rightDelta = cv::Point2f();
            leftDelta = cv::Point2f();
        }
    }
    else
    {
        if (options.trackSubjectRight && !rightReliable)
        {
            rightDelta = cv::Point2f();
        }
        if (options.trackSubjectLeft && !leftReliable)
        {
            leftDelta = cv::Point2f();
        }
    }

    PupilLightFrame &current = (*frames)[currentIndex];
    if (options.trackSubjectRight)
    {
        current.subjectRight.detected = options.useFullSmallFrame
                ? rightReliable : true;
        current.subjectRight.reliable = rightReliable;
        current.subjectRight.center = previous.subjectRight.center + rightDelta;
        current.subjectRight.radius =
            interpolateScalar(currentIndex, anchors, rightRadii);
        current.subjectRight.score = right.score;
        current.subjectRight.source = PupilSource_LightTrack;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        current.subjectRight.matchDiagnostic = right.diagnostic;
#endif
    }
    if (options.trackSubjectLeft)
    {
        current.subjectLeft.detected = options.useFullSmallFrame
                ? leftReliable : true;
        current.subjectLeft.reliable = leftReliable;
        current.subjectLeft.center = previous.subjectLeft.center + leftDelta;
        current.subjectLeft.radius =
            interpolateScalar(currentIndex, anchors, leftRadii);
        current.subjectLeft.score = left.score;
        current.subjectLeft.source = PupilSource_LightTrack;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        current.subjectLeft.matchDiagnostic = left.diagnostic;
#endif
    }
}

void applyStereoGeometry(const std::vector<int> &anchors,
                         int trackingBeginIndex,
                         int trackingEndIndex,
                         std::vector<PupilLightFrame> *frames)
{
    std::vector<cv::Point2f> eyeVectors;
    for (int anchor : anchors)
    {
        eyeVectors.push_back((*frames)[anchor].subjectLeft.center -
                             (*frames)[anchor].subjectRight.center);
    }

    for (size_t index = 0; index < frames->size(); ++index)
    {
        if (static_cast<int>(index) < trackingBeginIndex ||
            static_cast<int>(index) > trackingEndIndex)
        {
            continue;
        }
        if (std::find(anchors.begin(),
                      anchors.end(),
                      static_cast<int>(index)) != anchors.end())
        {
            continue;
        }
        PupilLightFrame &frame = (*frames)[index];
        const cv::Point2f expectedVector =
            interpolatePoint(static_cast<int>(index), anchors, eyeVectors);
        const cv::Point2f midpoint =
            (frame.subjectRight.center + frame.subjectLeft.center) * 0.5F;
        frame.subjectRight.center = midpoint - expectedVector * 0.5F;
        frame.subjectLeft.center = midpoint + expectedVector * 0.5F;
    }
}

CandidateResult runCandidate(const PupilLightTrackerCache &gradientCache,
                             const std::vector<PupilLightFrame> &initialFrames,
                             const std::vector<int> &anchors,
                             const PupilLightTrackerOptions &options,
                             bool direct)
{
    CandidateResult candidate;
    candidate.frames = initialFrames;
    int eyeVectorRetryCount = 0;
    const int trackingBeginIndex = options.trackingBeginIndex >= 0
        ? std::max(0, options.trackingBeginIndex)
        : 0;
    const int trackingEndIndex = options.trackingEndIndex >= 0
        ? std::min(static_cast<int>(candidate.frames.size()) - 1,
                   options.trackingEndIndex)
        : static_cast<int>(candidate.frames.size()) - 1;
    std::vector<float> rightRadii;
    std::vector<float> leftRadii;
    for (int anchor : anchors)
    {
        if (options.trackSubjectRight)
        {
            rightRadii.push_back(candidate.frames[anchor].subjectRight.radius);
        }
        if (options.trackSubjectLeft)
        {
            leftRadii.push_back(candidate.frames[anchor].subjectLeft.radius);
        }
    }

    const int64 start = cv::getTickCount();
    if (direct)
    {
        for (size_t index = 0; index < candidate.frames.size(); ++index)
        {
            if (static_cast<int>(index) < trackingBeginIndex ||
                static_cast<int>(index) > trackingEndIndex ||
                candidate.frames[index].isAnchor)
            {
                continue;
            }
            const int frameIndex = static_cast<int>(index);
            const int reference = *std::min_element(
                anchors.begin(),
                anchors.end(),
                [frameIndex](int left, int right) {
                    return std::abs(frameIndex - left) <
                           std::abs(frameIndex - right);
                });
            trackStep(reference,
                      frameIndex,
                      gradientCache,
                      anchors,
                      rightRadii,
                      leftRadii,
                      options,
                      &eyeVectorRetryCount,
                      &candidate.frames);
        }
    }
    else
    {
        for (int index = anchors.front() - 1; index >= 0; --index)
        {
            if (index < trackingBeginIndex || index > trackingEndIndex)
            {
                continue;
            }
            trackStep(index + 1,
                      index,
                      gradientCache,
                      anchors,
                      rightRadii,
                      leftRadii,
                      options,
                      &eyeVectorRetryCount,
                      &candidate.frames);
        }
        for (size_t segment = 0; segment + 1 < anchors.size(); ++segment)
        {
            const int leftAnchor = anchors[segment];
            const int rightAnchor = anchors[segment + 1];
            const int middle = (leftAnchor + rightAnchor) / 2;
            for (int index = leftAnchor + 1; index <= middle; ++index)
            {
                if (index < trackingBeginIndex || index > trackingEndIndex)
                {
                    continue;
                }
                trackStep(index - 1,
                          index,
                          gradientCache,
                          anchors,
                          rightRadii,
                          leftRadii,
                          options,
                          &eyeVectorRetryCount,
                          &candidate.frames);
            }
            for (int index = rightAnchor - 1; index > middle; --index)
            {
                if (index < trackingBeginIndex || index > trackingEndIndex)
                {
                    continue;
                }
                trackStep(index + 1,
                          index,
                          gradientCache,
                          anchors,
                          rightRadii,
                           leftRadii,
                           options,
                           &eyeVectorRetryCount,
                           &candidate.frames);
            }
        }
        for (size_t index = static_cast<size_t>(anchors.back() + 1);
             index < candidate.frames.size();
             ++index)
        {
            if (static_cast<int>(index) < trackingBeginIndex ||
                static_cast<int>(index) > trackingEndIndex)
            {
                continue;
            }
            trackStep(static_cast<int>(index - 1),
                      static_cast<int>(index),
                      gradientCache,
                      anchors,
                      rightRadii,
                       leftRadii,
                       options,
                       &eyeVectorRetryCount,
                       &candidate.frames);
        }
    }
    // 正式小图模式只用双眼向量做门控和重试，不能用双眼中点重写
    // 已匹配坐标；旧兼容路径继续保留原有几何校正。
    if (!options.useFullSmallFrame
            && options.trackSubjectRight && options.trackSubjectLeft)
    {
        applyStereoGeometry(anchors, trackingBeginIndex, trackingEndIndex,
                            &candidate.frames);
    }
    candidate.elapsedMs =
        (cv::getTickCount() - start) * 1000.0 / cv::getTickFrequency();

    std::vector<double> scores;
    for (const PupilLightFrame &frame : candidate.frames)
    {
        if (!frame.isAnchor)
        {
            if (options.trackSubjectRight)
            {
                scores.push_back(frame.subjectRight.score);
            }
            if (options.trackSubjectLeft)
            {
                scores.push_back(frame.subjectLeft.score);
            }
        }
    }
    candidate.scoreP05 = percentile(scores, 0.05);
    candidate.eyeVectorRetryCount = eyeVectorRetryCount;
    return candidate;
}

int unreliableEyeCount(const CandidateResult &candidate,
                       const PupilLightTrackerOptions &options)
{
    int count = 0;
    for (const PupilLightFrame &frame : candidate.frames)
    {
        if (!frame.isAnchor)
        {
            if (options.trackSubjectRight)
            {
                count += frame.subjectRight.reliable ? 0 : 1;
            }
            if (options.trackSubjectLeft)
            {
                count += frame.subjectLeft.reliable ? 0 : 1;
            }
        }
    }
    return count;
}
}

bool PupilLightTracker::buildFlowReference(
        const cv::Mat &gray,
        const PupilLightEye &eye,
        const PupilLightTrackerOptions &options,
        PupilLightFlowReference *reference,
        std::string *errorMessage) const
{
    if (reference == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "flow_reference_null";
        }
        return false;
    }
    reference->clear();
    if (gray.empty() || gray.type() != CV_8UC1 || !eye.detected
            || !std::isfinite(eye.center.x)
            || !std::isfinite(eye.center.y)
            || !std::isfinite(eye.radius)
            || eye.radius <= 0.0F) {
        if (errorMessage != nullptr) {
            *errorMessage = "flow_reference_invalid_input";
        }
        return false;
    }

    try {
        const int featureHalf = std::max(
                12, std::max(options.flowFeatureHalfWindow,
                             cvRound(eye.radius * 2.0F)));
        const cv::Rect imageBounds(0, 0, gray.cols, gray.rows);
        const cv::Rect desiredFeatureRoi(
                cvRound(eye.center.x) - featureHalf,
                cvRound(eye.center.y) - featureHalf,
                featureHalf * 2 + 1,
                featureHalf * 2 + 1);
        // 手持模式允许瞳孔靠近上下边缘；参考区域裁到图内即可，不能整块拒绝。
        const cv::Rect featureRoi = desiredFeatureRoi & imageBounds;
        if (featureRoi.width < 5 || featureRoi.height < 5) {
            if (errorMessage != nullptr) {
                *errorMessage = "flow_reference_roi_out_of_bounds";
            }
            return false;
        }

        std::vector<cv::Point2f> localPoints;
        cv::goodFeaturesToTrack(
                gray(featureRoi), localPoints,
                std::max(options.flowMaximumFeatures, 1),
                options.flowFeatureQualityLevel,
                options.flowFeatureMinimumDistance,
                cv::Mat(), 3, false, 0.04);
        if (static_cast<int>(localPoints.size())
                < options.flowMinimumValidPoints) {
            if (errorMessage != nullptr) {
                *errorMessage = "flow_reference_not_enough_features";
            }
            return false;
        }

        reference->gray = gray.clone();
        reference->points.reserve(localPoints.size());
        for (const cv::Point2f &point : localPoints) {
            reference->points.emplace_back(
                    point.x + static_cast<float>(featureRoi.x),
                    point.y + static_cast<float>(featureRoi.y));
        }
        reference->center = eye.center;
        reference->radius = eye.radius;
        reference->valid = true;
        return true;
    } catch (const cv::Exception &exception) {
        reference->clear();
        if (errorMessage != nullptr) {
            *errorMessage = exception.what();
        }
        return false;
    }
}

bool PupilLightTracker::trackOneEyeByFlow(
        const PupilLightFlowReference &reference,
        const cv::Mat &targetGray,
        const PupilLightTrackerOptions &options,
        PupilLightFlowResult *result,
        std::string *errorMessage) const
{
    if (result == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "flow_result_null";
        }
        return false;
    }
    *result = PupilLightFlowResult{};
    const int64 start = cv::getTickCount();
    auto finishTiming = [result, start]() {
        result->elapsedMs = (cv::getTickCount() - start) * 1000.0
                / cv::getTickFrequency();
    };
    if (!reference.valid || reference.gray.empty()
            || targetGray.empty() || reference.gray.type() != CV_8UC1
            || targetGray.type() != CV_8UC1
            || reference.gray.size() != targetGray.size()
            || static_cast<int>(reference.points.size())
                   < options.flowMinimumValidPoints) {
        result->failureReason = PupilLightFlow_InvalidInput;
        finishTiming();
        if (errorMessage != nullptr) {
            *errorMessage = "flow_invalid_input";
        }
        return false;
    }

    try {
        std::vector<cv::Point2f> currentPoints;
        std::vector<uchar> forwardStatus;
        std::vector<float> forwardError;
        cv::calcOpticalFlowPyrLK(
                reference.gray, targetGray, reference.points, currentPoints,
                forwardStatus, forwardError, options.flowWindowSize,
                options.flowMaximumLevel,
                cv::TermCriteria(cv::TermCriteria::COUNT
                                 | cv::TermCriteria::EPS, 30, 0.01),
                0, 0.001);

        std::vector<cv::Point2f> backwardPoints;
        std::vector<uchar> backwardStatus;
        std::vector<float> backwardError;
        cv::calcOpticalFlowPyrLK(
                targetGray, reference.gray, currentPoints, backwardPoints,
                backwardStatus, backwardError, options.flowWindowSize,
                options.flowMaximumLevel,
                cv::TermCriteria(cv::TermCriteria::COUNT
                                 | cv::TermCriteria::EPS, 30, 0.01),
                0, 0.001);

        std::vector<cv::Point2f> displacements;
        const size_t pointCount = reference.points.size();
        for (size_t index = 0; index < pointCount; ++index) {
            if (index >= currentPoints.size()
                    || index >= backwardPoints.size()
                    || index >= forwardStatus.size()
                    || index >= backwardStatus.size()
                    || !forwardStatus[index] || !backwardStatus[index]
                    || !std::isfinite(currentPoints[index].x)
                    || !std::isfinite(currentPoints[index].y)
                    || !std::isfinite(backwardPoints[index].x)
                    || !std::isfinite(backwardPoints[index].y)
                    || (index < forwardError.size()
                        && forwardError[index] > 30.0F)
                    || (index < backwardError.size()
                        && backwardError[index] > 30.0F)) {
                continue;
            }
            const double backwardDistance = cv::norm(
                    backwardPoints[index] - reference.points[index]);
            if (!std::isfinite(backwardDistance)
                    || backwardDistance > options.flowMaximumBackwardError) {
                continue;
            }
            displacements.emplace_back(
                    currentPoints[index].x - reference.points[index].x,
                    currentPoints[index].y - reference.points[index].y);
        }

        result->validPointRatio = static_cast<double>(displacements.size())
                / static_cast<double>(reference.points.size());
        if (static_cast<int>(displacements.size())
                < options.flowMinimumValidPoints) {
            result->failureReason = PupilLightFlow_TooFewValidPoints;
            finishTiming();
            return false;
        }
        if (result->validPointRatio < options.flowMinimumValidRatio) {
            result->failureReason = PupilLightFlow_LowValidRatio;
            finishTiming();
            return false;
        }

        std::vector<float> dx;
        std::vector<float> dy;
        dx.reserve(displacements.size());
        dy.reserve(displacements.size());
        for (const cv::Point2f &displacement : displacements) {
            dx.push_back(displacement.x);
            dy.push_back(displacement.y);
        }
        std::sort(dx.begin(), dx.end());
        std::sort(dy.begin(), dy.end());
        const size_t middle = dx.size() / 2;
        const float medianDx = dx[middle];
        const float medianDy = dy[middle];
        const double displacementMagnitude = std::hypot(
                static_cast<double>(medianDx),
                static_cast<double>(medianDy));
        if (!std::isfinite(displacementMagnitude)
                || displacementMagnitude > options.flowMaximumDisplacement) {
            result->failureReason = PupilLightFlow_ResidualTooLarge;
            finishTiming();
            return false;
        }

        std::vector<cv::Point2f> inliers;
        for (const cv::Point2f &displacement : displacements) {
            const double residual = std::hypot(
                    static_cast<double>(displacement.x - medianDx),
                    static_cast<double>(displacement.y - medianDy));
            if (residual <= options.flowMaximumResidual) {
                inliers.push_back(displacement);
            }
        }
        result->validPointCount = static_cast<int>(inliers.size());
        if (result->validPointCount < options.flowMinimumValidPoints) {
            result->failureReason = PupilLightFlow_ResidualTooLarge;
            finishTiming();
            return false;
        }
        // 残差剔除后必须重新计算比例，防止少量一致点绕过质量门禁。
        result->validPointRatio = static_cast<double>(inliers.size())
                / static_cast<double>(reference.points.size());
        if (result->validPointRatio < options.flowMinimumValidRatio) {
            result->failureReason = PupilLightFlow_LowValidRatio;
            finishTiming();
            return false;
        }

        const cv::Point2f predicted(
                reference.center.x + medianDx,
                reference.center.y + medianDy);
        const float edge = std::max(2.0F, reference.radius);
        if (predicted.x < edge || predicted.y < edge
                || predicted.x >= targetGray.cols - edge
                || predicted.y >= targetGray.rows - edge) {
            result->failureReason = PupilLightFlow_PredictionOutOfBounds;
            finishTiming();
            return false;
        }

        result->center = predicted;
        result->radius = reference.radius;
        result->success = true;
        result->failureReason = PupilLightFlow_None;
        finishTiming();
        return true;
    } catch (const cv::Exception &exception) {
        result->failureReason = PupilLightFlow_Exception;
        finishTiming();
        if (errorMessage != nullptr) {
            *errorMessage = exception.what();
        }
        return false;
    }
}

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
const char *pupilLightMatchReasonName(PupilLightMatchReason reason)
{
    switch (reason)
    {
    case PupilLightMatch_None:                 return "none";
    case PupilLightMatch_NotAttempted:         return "not_attempted";
    case PupilLightMatch_PreviousEyeInvalid:    return "previous_eye_invalid";
    case PupilLightMatch_PreviousGradientInvalid:
        return "previous_gradient_invalid";
    case PupilLightMatch_TemplateRectInvalid:   return "template_rect_invalid";
    case PupilLightMatch_TemplateOutsideGradient:
        return "template_outside_gradient";
    case PupilLightMatch_TemplateOutsideEyeHalf:
        return "template_outside_eye_half";
    case PupilLightMatch_SearchRectInvalid:     return "search_rect_invalid";
    case PupilLightMatch_SearchOutsideGradient:
        return "search_outside_gradient";
    case PupilLightMatch_SearchSmallerThanTemplate:
        return "search_smaller_than_template";
    case PupilLightMatch_TemplateOrSearchEmpty:
        return "template_or_search_empty";
    case PupilLightMatch_MatchException:
        return "match_exception";
    case PupilLightMatch_ScoreBelowThreshold:
        return "score_below_threshold";
    case PupilLightMatch_RetryScoreBelowThreshold:
        return "retry_score_below_threshold";
    case PupilLightMatch_RetryDeltaInconsistent:
        return "retry_delta_inconsistent";
    case PupilLightMatchReason_Count:           break;
    }
    return "unknown";
}
#endif

bool PupilLightTracker::extendGradientCache(
        const std::vector<cv::Mat> &grayImages,
        const std::vector<PupilLightFrame> &frames,
        const PupilLightTrackerOptions &options,
        PupilLightTrackerSummary *summary,
        std::string *errorMessage,
        PupilLightTrackerCache *gradientCache) const
{
    if (summary == nullptr || gradientCache == nullptr
            || grayImages.empty() || frames.empty()) {
        setError(errorMessage, "Invalid incremental gradient input.");
        return false;
    }
    if (options.processingScale < 0.25F
            || options.processingScale > 1.0F) {
        setError(errorMessage,
                 "Light tracker processing scale must be within [0.25, 1.0].");
        return false;
    }
    if (!options.trackSubjectRight && !options.trackSubjectLeft) {
        setError(errorMessage, "Light tracker has no selected eye.");
        return false;
    }
    for (const cv::Mat &image : grayImages) {
        if (image.empty() || image.type() != CV_8UC1
                || image.size() != grayImages.front().size()) {
            setError(errorMessage,
                     "Incremental gradient requires equal-size CV_8UC1 images.");
            return false;
        }
    }

    const std::vector<int> anchors = anchorIndices(frames);
    if (anchors.size() < 2) {
        setError(errorMessage,
                 "Incremental gradient requires at least two anchors.");
        return false;
    }
    for (int anchor : anchors) {
        if (anchor < 0 || anchor >= static_cast<int>(frames.size())) {
            setError(errorMessage, "Invalid incremental anchor index.");
            return false;
        }
        const PupilLightFrame &frame = frames[anchor];
        if (!isTrackedEyeValid(frame, options, true)
                || !isTrackedEyeValid(frame, options, false)) {
            setError(errorMessage, "Incremental anchor result is incomplete.");
            return false;
        }
    }

    std::vector<PupilLightFrame> workingFrames = frames;
    for (PupilLightFrame &frame : workingFrames) {
        if (options.trackSubjectRight && frame.subjectRight.detected) {
            frame.subjectRight.center *= options.processingScale;
            frame.subjectRight.radius *= options.processingScale;
        }
        if (options.trackSubjectLeft && frame.subjectLeft.detected) {
            frame.subjectLeft.center *= options.processingScale;
            frame.subjectLeft.radius *= options.processingScale;
        }
    }

    *summary = PupilLightTrackerSummary();
    summary->processingScale = options.processingScale;
    const int64 totalStart = cv::getTickCount();
    const size_t cachedCount = options.trackSubjectRight
            ? gradientCache->subjectRightGradients.size()
            : gradientCache->subjectLeftGradients.size();
    const bool emptyCache = cachedCount == 0
            && (gradientCache->sourceImageSize.width <= 0
                || gradientCache->sourceImageSize.height <= 0);

    if (emptyCache) {
        initializeGradientCache(grayImages.front().size(),
                                workingFrames,
                                anchors,
                                options,
                                gradientCache);
    } else {
        const bool metadataMatches =
                std::fabs(gradientCache->processingScale
                          - options.processingScale) <= 1e-6F
                && std::fabs(gradientCache->templateRadiusScale
                             - options.templateRadiusScale) <= 1e-6F
                && std::fabs(gradientCache->searchMarginScale
                             - options.searchMarginScale) <= 1e-6F
                && gradientCache->tracksSubjectRight
                   == options.trackSubjectRight
                && gradientCache->tracksSubjectLeft
                   == options.trackSubjectLeft
                && gradientCache->maximumTemplateHalf
                   == options.maximumTemplateHalf
                && gradientCache->maximumSearchMargin
                   == options.maximumSearchMargin
                 && gradientCache->maximumMovementPadding
                    == options.maximumMovementPadding
                 && gradientCache->useFullSmallFrame
                    == options.useFullSmallFrame
                 && gradientCache->sourceImageSize
                   == grayImages.front().size()
                && cacheSupportsAnchors(*gradientCache,
                                        workingFrames,
                                        anchors,
                                        options);
        const size_t rightCount = options.trackSubjectRight
                ? gradientCache->subjectRightGradients.size() : cachedCount;
        const size_t leftCount = options.trackSubjectLeft
                ? gradientCache->subjectLeftGradients.size() : cachedCount;
        if (!metadataMatches || rightCount != cachedCount
                || leftCount != cachedCount
                || cachedCount > grayImages.size()) {
            setError(errorMessage,
                     "Incremental gradient cache configuration changed.");
            return false;
        }
        summary->gradientCacheReused = true;
    }

    const int64 gradientStart = cv::getTickCount();
    GradientPreparationTiming gradientTiming;
    appendGradientCache(grayImages,
                        cachedCount,
                        options,
                        gradientCache,
                        &gradientTiming);
    summary->gradientMs =
            (cv::getTickCount() - gradientStart) * 1000.0
            / cv::getTickFrequency();
    summary->gradientCropMs = gradientTiming.cropMs;
    summary->gradientResizeMs = gradientTiming.resizeMs;
    summary->gradientBlurMs = gradientTiming.blurMs;
    summary->gradientSobelMs = gradientTiming.sobelMs;
    summary->gradientMagnitudeMs = gradientTiming.magnitudeMs;
    summary->gradientNormalizeMs = gradientTiming.normalizeMs;
    summary->totalMs =
            (cv::getTickCount() - totalStart) * 1000.0
            / cv::getTickFrequency();
    return true;
}

bool PupilLightTracker::run(const std::vector<cv::Mat> &grayImages,
                            std::vector<PupilLightFrame> *frames,
                            const PupilLightTrackerOptions &options,
                            PupilLightTrackerSummary *summary,
                            std::string *errorMessage,
                            PupilLightTrackerCache *gradientCache) const
{
    if (frames == nullptr || summary == nullptr ||
        frames->size() != grayImages.size() || grayImages.empty())
    {
        setError(errorMessage, "Invalid light tracker input.");
        return false;
    }
    if (options.processingScale < 0.25F ||
        options.processingScale > 1.0F)
    {
        setError(errorMessage,
                 "Light tracker processing scale must be within [0.25, 1.0].");
        return false;
    }
    if (!options.trackSubjectRight && !options.trackSubjectLeft)
    {
        setError(errorMessage, "Light tracker has no selected eye.");
        return false;
    }
    const std::vector<int> anchors = anchorIndices(*frames);
    // 单眼独立锚点恢复允许每只眼各有一个可靠模型锚点。顺序跟踪可以
    // 从该锚点分别向前、向后扩展；双锚点仍是默认且精度更高的路径。
    if (anchors.empty())
    {
        setError(errorMessage, "Light tracker requires at least one anchor.");
        return false;
    }
    for (size_t index = 0; index < grayImages.size(); ++index)
    {
        if (grayImages[index].empty() || grayImages[index].type() != CV_8UC1)
        {
            setError(errorMessage, "Light tracker requires CV_8UC1 images.");
            return false;
        }
    }
    for (int anchor : anchors)
    {
        const PupilLightFrame &frame = (*frames)[anchor];
        if (!isTrackedEyeValid(frame, options, true) ||
            !isTrackedEyeValid(frame, options, false))
        {
            setError(errorMessage, "Anchor model result is incomplete.");
            return false;
        }
    }
    const int effectiveTrackingBegin = options.trackingBeginIndex >= 0
        ? std::max(0, options.trackingBeginIndex)
        : 0;
    const int effectiveTrackingEnd = options.trackingEndIndex >= 0
        ? std::min(static_cast<int>(frames->size()) - 1,
                   options.trackingEndIndex)
        : static_cast<int>(frames->size()) - 1;
    if (effectiveTrackingBegin > effectiveTrackingEnd)
    {
        setError(errorMessage, "Invalid partial tracking range.");
        return false;
    }
    if (options.trackingBeginIndex >= 0 || options.trackingEndIndex >= 0)
    {
        // 区段外结果不会重算，调用方必须提供上一轮已完成的目标眼位置。
        for (size_t index = 0; index < frames->size(); ++index)
        {
            if (static_cast<int>(index) >= effectiveTrackingBegin &&
                static_cast<int>(index) <= effectiveTrackingEnd)
            {
                continue;
            }
            const PupilLightFrame &frame = (*frames)[index];
            if (!isTrackedEyeValid(frame, options, true) ||
                !isTrackedEyeValid(frame, options, false))
            {
                setError(errorMessage,
                         "Partial tracking requires preserved outer frames.");
                return false;
            }
        }
    }

    // 防止同一个汇总对象被复用时残留上一次运行的数据。
    *summary = PupilLightTrackerSummary();
    summary->processingScale = options.processingScale;
    const int64 totalStart = cv::getTickCount();

    // 模型结果在原图坐标系；候选跟踪前先变换到梯度图坐标系。
    const std::vector<PupilLightFrame> originalFrames = *frames;
    std::vector<PupilLightFrame> workingFrames = *frames;
    for (PupilLightFrame &frame : workingFrames)
    {
        if (options.trackSubjectRight && frame.subjectRight.detected)
        {
            frame.subjectRight.center *= options.processingScale;
            frame.subjectRight.radius *= options.processingScale;
        }
        if (options.trackSubjectLeft && frame.subjectLeft.detected)
        {
            frame.subjectLeft.center *= options.processingScale;
            frame.subjectLeft.radius *= options.processingScale;
        }
    }

    // 梯度只计算左右眼附近的两个局部ROI；补充中间锚点时优先复用本缓存。
    PupilLightTrackerCache localGradientCache;
    PupilLightTrackerCache *activeGradientCache =
        gradientCache != nullptr ? gradientCache : &localGradientCache;
    const bool preparedAnchorCache = gradientCache != nullptr
            && preparedAnchorCacheIsUsable(*activeGradientCache,
                                           grayImages,
                                           workingFrames,
                                           anchors,
                                           options);
    summary->gradientCacheReused = preparedAnchorCache
            || gradientCacheIsUsable(*activeGradientCache,
                                     grayImages,
                                     workingFrames,
                                     anchors,
                                     options);
    if (preparedAnchorCache)
    {
        // 单锚点缓存本身是只读的；调用方传入的应是每个任务的浅拷贝，
        // 因此这里只向任务私有vector追加目标图梯度。
        const int64 gradientStart = cv::getTickCount();
        GradientPreparationTiming gradientTiming;
        appendGradientCache(grayImages,
                            1,
                            options,
                            activeGradientCache,
                            &gradientTiming);
        summary->gradientMs =
                (cv::getTickCount() - gradientStart) * 1000.0 /
                cv::getTickFrequency();
        summary->gradientCropMs = gradientTiming.cropMs;
        summary->gradientResizeMs = gradientTiming.resizeMs;
        summary->gradientBlurMs = gradientTiming.blurMs;
        summary->gradientSobelMs = gradientTiming.sobelMs;
        summary->gradientMagnitudeMs = gradientTiming.magnitudeMs;
        summary->gradientNormalizeMs = gradientTiming.normalizeMs;
    }
    else if (!summary->gradientCacheReused)
    {
        const int64 gradientStart = cv::getTickCount();
        GradientPreparationTiming gradientTiming;
        prepareGradientCache(grayImages,
                             workingFrames,
                             anchors,
                             options,
                             activeGradientCache,
                             &gradientTiming);
        summary->gradientMs =
            (cv::getTickCount() - gradientStart) * 1000.0 /
            cv::getTickFrequency();
        summary->gradientCropMs = gradientTiming.cropMs;
        summary->gradientResizeMs = gradientTiming.resizeMs;
        summary->gradientBlurMs = gradientTiming.blurMs;
        summary->gradientSobelMs = gradientTiming.sobelMs;
        summary->gradientMagnitudeMs = gradientTiming.magnitudeMs;
        summary->gradientNormalizeMs = gradientTiming.normalizeMs;
    }
    const cv::Size processingImageSize(
        cvRound(grayImages.front().cols * options.processingScale),
        cvRound(grayImages.front().rows * options.processingScale));
    const double fullPixelCount =
        static_cast<double>(processingImageSize.area());
    double gradientRoiPixels = 0.0;
    if (options.trackSubjectRight)
    {
        gradientRoiPixels +=
            static_cast<double>(activeGradientCache->subjectRightRoi.area());
    }
    if (options.trackSubjectLeft)
    {
        gradientRoiPixels +=
            static_cast<double>(activeGradientCache->subjectLeftRoi.area());
    }
    summary->gradientPixelRatio = fullPixelCount > 0.0
        ? gradientRoiPixels / fullPixelCount : 1.0;

    // 正常样本只计算顺序候选，避免把两套跟踪的耗时全部付出。
    CandidateResult sequential =
        runCandidate(*activeGradientCache,
                     workingFrames,
                     anchors,
                     options,
                     false);
    summary->sequentialTrackMs = sequential.elapsedMs;
    const int sequentialUnreliable = unreliableEyeCount(sequential, options);
    // 400×160正式路径每只眼只做一次正常匹配，失败只允许在同一
    // trackStep内按瞳距向量追加一次；旧兼容路径才保留整段direct候选。
    summary->directFallbackTriggered = !options.useFullSmallFrame
        && (sequential.scoreP05 < options.minimumMatchScore
            || sequentialUnreliable > 0);

    const CandidateResult *selected = &sequential;
    summary->selectedMode = "sequential";
    CandidateResult direct;
    if (summary->directFallbackTriggered)
    {
        // 只有顺序候选低可信时，才付出直接锚点匹配的额外计算。
        direct = runCandidate(*activeGradientCache,
                              workingFrames,
                              anchors,
                              options,
                              true);
        summary->directTrackMs = direct.elapsedMs;
        const int directUnreliable = unreliableEyeCount(direct, options);
        const bool directBetter =
            directUnreliable < sequentialUnreliable ||
            (directUnreliable == sequentialUnreliable &&
             direct.scoreP05 > sequential.scoreP05);
        if (directBetter)
        {
            selected = &direct;
            summary->selectedMode = "direct_fallback";
        }
        else
        {
            summary->selectedMode = "sequential_fallback_kept";
        }
    }

    *frames = selected->frames;
    const float inverseScale = 1.0F / options.processingScale;
    for (size_t index = 0; index < frames->size(); ++index)
    {
        if ((*frames)[index].isAnchor)
        {
            // 锚点直接保留模型的原始精度，避免往返缩放产生误差。
            (*frames)[index] = originalFrames[index];
            continue;
        }
        if (options.trackSubjectRight)
        {
            (*frames)[index].subjectRight.center *= inverseScale;
            (*frames)[index].subjectRight.radius *= inverseScale;
        }
        if (options.trackSubjectLeft)
        {
            (*frames)[index].subjectLeft.center *= inverseScale;
            (*frames)[index].subjectLeft.radius *= inverseScale;
        }
    }
    summary->scoreP05 = selected->scoreP05;
    summary->unreliableEyeCount = unreliableEyeCount(*selected, options);
    summary->eyeVectorRetryCount = selected->eyeVectorRetryCount;
    summary->totalMs =
        (cv::getTickCount() - totalStart) * 1000.0 /
        cv::getTickFrequency();
    return true;
}

bool PupilLightTracker::prepareAnchorCache(
        const cv::Mat &anchorImage,
        const PupilLightFrame &anchorFrame,
        const PupilLightTrackerOptions &options,
        PupilLightTrackerCache *cache,
        std::string *errorMessage) const
{
    if (cache == nullptr || anchorImage.empty()
            || anchorImage.type() != CV_8UC1) {
        setError(errorMessage, "Invalid anchor cache input.");
        return false;
    }
    if (options.processingScale < 0.25F
            || options.processingScale > 1.0F
            || (!options.trackSubjectRight && !options.trackSubjectLeft)) {
        setError(errorMessage, "Invalid anchor cache options.");
        return false;
    }

    PupilLightFrame preparedFrame = anchorFrame;
    preparedFrame.isAnchor = true;
    if (!isTrackedEyeValid(preparedFrame, options, true)
            || !isTrackedEyeValid(preparedFrame, options, false)) {
        setError(errorMessage, "Anchor cache requires a valid selected eye.");
        return false;
    }

    PupilLightFrame workingFrame = preparedFrame;
    if (options.trackSubjectRight) {
        workingFrame.subjectRight.center *= options.processingScale;
        workingFrame.subjectRight.radius *= options.processingScale;
    }
    if (options.trackSubjectLeft) {
        workingFrame.subjectLeft.center *= options.processingScale;
        workingFrame.subjectLeft.radius *= options.processingScale;
    }
    std::vector<PupilLightFrame> frames(1, workingFrame);
    const std::vector<int> anchors(1, 0);
    initializeGradientCache(anchorImage.size(), frames, anchors, options,
                            cache);
    if ((options.trackSubjectRight
         && (cache->subjectRightSourceRoi.empty()
             || cache->subjectRightRoi.empty()))
            || (options.trackSubjectLeft
                && (cache->subjectLeftSourceRoi.empty()
                    || cache->subjectLeftRoi.empty()))) {
        cache->clear();
        setError(errorMessage, "Anchor cache ROI is empty.");
        return false;
    }

    // 只对锚点图计算一次梯度；目标图由每个独立任务追加。
    GradientPreparationTiming timing;
    appendGradientCache(std::vector<cv::Mat>(1, anchorImage),
                        0,
                        options,
                        cache,
                        &timing);
    if ((options.trackSubjectRight
         && cache->subjectRightGradients.size() != 1)
            || (options.trackSubjectLeft
                && cache->subjectLeftGradients.size() != 1)) {
        cache->clear();
        setError(errorMessage, "Anchor cache gradient preparation failed.");
        return false;
    }
    return true;
}

bool PupilLightTracker::trackOneFrameFromAnchorCached(
        const cv::Mat &anchorImage,
        const PupilLightFrame &anchorFrame,
        const cv::Mat &targetImage,
        int lampNumber,
        const PupilLightTrackerOptions &options,
        const PupilLightTrackerCache &anchorCache,
        PupilLightFrame *result,
        PupilLightTrackerSummary *summary,
        std::string *errorMessage,
        PupilLightTrackerCache *targetGradientCache) const
{
    if (targetGradientCache != nullptr) {
        // 输出缓存只在本次调用中写入；调用方拿到后不再修改其梯度Mat。
        targetGradientCache->clear();
    }
    if (result == nullptr || summary == nullptr
            || anchorImage.empty() || targetImage.empty()) {
        setError(errorMessage, "Invalid cached single-frame anchor input.");
        return false;
    }

    std::vector<cv::Mat> images;
    images.push_back(anchorImage);
    images.push_back(targetImage);
    std::vector<PupilLightFrame> frames(2);
    frames[0] = anchorFrame;
    frames[0].isAnchor = true;
    frames[1].lampNumber = lampNumber;

    // cv::Mat复制只复制头部；vector复制后目标梯度追加到本地缓存，
    // 原始anchorCache保持只读，多个照片任务可以安全并行执行。
    PupilLightTrackerCache localCache = anchorCache;
    if (!run(images, &frames, options, summary, errorMessage, &localCache)) {
        return false;
    }

    if (targetGradientCache != nullptr) {
        const bool rightReady = !options.trackSubjectRight
                || localCache.subjectRightGradients.size() == 2;
        const bool leftReady = !options.trackSubjectLeft
                || localCache.subjectLeftGradients.size() == 2;
        const bool sharedReady = !options.useFullSmallFrame
                || localCache.sharedGradients.size() == 2;
        if (rightReady && leftReady && sharedReady) {
            // 先复制缓存元数据和ROI，再仅保留第二张目标图的梯度。
            // cv::Mat和vector复制均为浅拷贝，不会重复分配梯度像素。
            *targetGradientCache = localCache;
            const auto keepTargetGradient =
                    [](const std::vector<cv::Mat>& gradients) {
                return std::vector<cv::Mat>{gradients[1]};
            };
            if (options.useFullSmallFrame) {
                targetGradientCache->sharedGradients =
                        keepTargetGradient(localCache.sharedGradients);
            } else {
                targetGradientCache->sharedGradients.clear();
            }
            if (options.trackSubjectRight) {
                targetGradientCache->subjectRightGradients =
                        keepTargetGradient(localCache.subjectRightGradients);
            } else {
                targetGradientCache->subjectRightGradients.clear();
            }
            if (options.trackSubjectLeft) {
                targetGradientCache->subjectLeftGradients =
                        keepTargetGradient(localCache.subjectLeftGradients);
            } else {
                targetGradientCache->subjectLeftGradients.clear();
            }
        }
    }
    *result = frames[1];
    result->lampNumber = lampNumber;
    result->isAnchor = false;
    return true;
}

bool PupilLightTracker::trackOneFrameFromAnchor(
        const cv::Mat &anchorImage,
        const PupilLightFrame &anchorFrame,
        const cv::Mat &targetImage,
        int lampNumber,
        const PupilLightTrackerOptions &options,
        PupilLightFrame *result,
        PupilLightTrackerSummary *summary,
        std::string *errorMessage) const
{
    if (result == nullptr || summary == nullptr
            || anchorImage.empty() || targetImage.empty()) {
        setError(errorMessage, "Invalid single-frame anchor input.");
        return false;
    }

    // 复用现有run()的局部梯度和直接匹配实现；目标照片只依赖锚点，
    // 不读取任何前一张正式照片的可变结果。
    std::vector<cv::Mat> images;
    images.push_back(anchorImage);
    images.push_back(targetImage);
    std::vector<PupilLightFrame> frames(2);
    frames[0] = anchorFrame;
    frames[0].isAnchor = true;
    frames[1].lampNumber = lampNumber;
    if (!run(images, &frames, options, summary, errorMessage, nullptr)) {
        return false;
    }
    *result = frames[1];
    result->lampNumber = lampNumber;
    result->isAnchor = false;
    return true;
}

bool PupilLightTracker::trackOneCrossRoundFrame(
        const cv::Mat &sourceImage,
        const PupilLightFrame &sourceFrame,
        const cv::Mat &targetImage,
        int lampNumber,
        PupilLightFrame *result,
        PupilLightTrackerSummary *summary,
        std::string *errorMessage) const
{
    PupilLightTrackerOptions options;
    // 跨轮接口没有单独的眼别参数，沿用源帧中已确认的目标眼，
    // 避免单眼测量因对侧眼未参与而被误判为锚点不完整。
    options.trackSubjectRight = sourceFrame.subjectRight.detected;
    options.trackSubjectLeft = sourceFrame.subjectLeft.detected;
    // 跨轮正式匹配同样使用400×160整图梯度，避免恢复路径重新走旧尺度。
    options.processingScale = 0.3125F;
    options.useFullSmallFrame = true;
    // 跨轮兼容接口也按400×160小图模板尺寸执行。
    options.maximumTemplateHalf = 13;
    options.maximumSearchMargin = 10;
    options.maximumMovementPadding = 16;
    options.minimumMatchScore = 0.70F;
    return trackOneFrameFromAnchor(sourceImage, sourceFrame, targetImage,
                                   lampNumber, options, result, summary,
                                   errorMessage);
}
