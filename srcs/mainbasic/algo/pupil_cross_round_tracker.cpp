#include "pupil_cross_round_tracker.h"

#include <algorithm>
#include <cmath>

#include <opencv2/imgproc.hpp>

namespace
{
struct MatchResult
{
    cv::Point2f center;
    float score = -1.0F;
};

void setError(std::string *errorMessage, const std::string &text)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = text;
    }
}

double elapsedMilliseconds(int64 start)
{
    return (cv::getTickCount() - start) * 1000.0 / cv::getTickFrequency();
}

int applyMaximum(int value, int maximum)
{
    return maximum > 0 ? std::min(value, maximum) : value;
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

cv::Mat makeGradient(const cv::Mat &gray, bool fastL1Gradient)
{
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0.0);

    // 快速路径保留边缘相对强弱即可：模板匹配不需要L2物理梯度值。
    // CV_16S + |dx|+|dy| 避免CV_32F平方、开方和逐ROI归一化。
    if (fastL1Gradient)
    {
        cv::Mat dx16;
        cv::Mat dy16;
        cv::Mat absDx;
        cv::Mat absDy;
        cv::Mat l1Gradient;
        cv::Sobel(blurred, dx16, CV_16S, 1, 0, 3);
        cv::Sobel(blurred, dy16, CV_16S, 0, 1, 3);
        cv::convertScaleAbs(dx16, absDx);
        cv::convertScaleAbs(dy16, absDy);
        cv::add(absDx, absDy, l1Gradient);
        return l1Gradient;
    }

    cv::Mat dx;
    cv::Mat dy;
    cv::Mat magnitude;
    cv::Mat normalized;
    cv::Sobel(blurred, dx, CV_32F, 1, 0, 3);
    cv::Sobel(blurred, dy, CV_32F, 0, 1, 3);
    cv::magnitude(dx, dy, magnitude);
    cv::normalize(magnitude,
                  normalized,
                  0.0,
                  255.0,
                  cv::NORM_MINMAX,
                  CV_8U);
    return normalized;
}

bool centeredRect(const cv::Point2f &center,
                  int halfWidth,
                  const cv::Size &imageSize,
                  cv::Rect *rect)
{
    const cv::Rect candidate(cvRound(center.x) - halfWidth,
                             cvRound(center.y) - halfWidth,
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

cv::Rect boundedRect(const cv::Point2f &center,
                     int halfWidth,
                     const cv::Size &imageSize)
{
    const int centerX = cvRound(center.x);
    const int centerY = cvRound(center.y);
    const int left = std::max(0, centerX - halfWidth);
    const int top = std::max(0, centerY - halfWidth);
    const int right = std::min(imageSize.width, centerX + halfWidth + 1);
    const int bottom = std::min(imageSize.height, centerY + halfWidth + 1);
    return cv::Rect(left,
                    top,
                    std::max(0, right - left),
                    std::max(0, bottom - top));
}

cv::Mat resizeForTracking(const cv::Mat &gray, float scale)
{
    if (scale >= 0.999F)
    {
        return gray;
    }
    cv::Mat resized;
    cv::resize(gray,
               resized,
               cv::Size(),
               scale,
               scale,
               cv::INTER_AREA);
    return resized;
}

bool buildTemplate(const cv::Mat &sourceImage,
                   const PupilLightEye &sourceEye,
                   const PupilCrossRoundOptions &options,
                   PupilCrossRoundTemplate *result)
{
    const int templateHalf = applyMaximum(
        std::max(18,
                 cvRound(sourceEye.radius * options.templateRadiusScale)),
        options.maximumTemplateHalf);
    cv::Rect templateRect;
    if (!centeredRect(sourceEye.center,
                      templateHalf,
                      sourceImage.size(),
                      &templateRect))
    {
        return false;
    }
    result->gradient =
        makeGradient(resizeForTracking(sourceImage(templateRect),
                                       options.processingScale),
                     options.fastL1Gradient);
    result->center = sourceEye.center;
    result->radius = sourceEye.radius;
    return !result->gradient.empty();
}

bool buildTargetGradient(const cv::Mat &targetImage,
                         const PupilLightEye &sourceEye,
                         const PupilCrossRoundOptions &options,
                         PupilCrossRoundTargetGradient *result,
                         double *gradientMs)
{
    const int templateHalf = applyMaximum(
        std::max(18,
                 cvRound(sourceEye.radius * options.templateRadiusScale)),
        options.maximumTemplateHalf);
    const int searchMargin = applyMaximum(
        std::max(32,
                 cvRound(sourceEye.radius * options.searchMarginScale)),
        options.maximumSearchMargin);
    result->searchRect = boundedRect(sourceEye.center,
                                     templateHalf + searchMargin,
                                     targetImage.size());
    if (result->searchRect.empty())
    {
        return false;
    }

    const int64 start = cv::getTickCount();
    result->gradient = makeGradient(
        resizeForTracking(targetImage(result->searchRect),
                          options.processingScale),
        options.fastL1Gradient);
    *gradientMs += elapsedMilliseconds(start);
    return !result->gradient.empty();
}

MatchResult matchTarget(const cv::Mat &targetImage,
                        const PupilCrossRoundTemplate &source,
                        const PupilCrossRoundOptions &options,
                        const PupilCrossRoundTargetGradient *cachedGradient,
                        const cv::Point2f *refinedSearchCenter,
                        double *gradientMs,
                        double *matchMs)
{
    MatchResult result;
    result.center = source.center;
    PupilCrossRoundTargetGradient generatedGradient;
    const PupilCrossRoundTargetGradient *targetGradientData = cachedGradient;
    if (!targetGradientData || targetGradientData->gradient.empty())
    {
        PupilLightEye sourceEye;
        sourceEye.detected = true;
        // 粗定位成功后，精匹配窗口应以粗定位结果为中心，而不是仍围绕
        // 上一轮旧坐标；否则大位移虽然被发现，仍会落在±40px窗口之外。
        sourceEye.center = refinedSearchCenter != nullptr
                ? *refinedSearchCenter : source.center;
        sourceEye.radius = source.radius;
        if (!buildTargetGradient(targetImage, sourceEye, options,
                                 &generatedGradient, gradientMs))
        {
            return result;
        }
        targetGradientData = &generatedGradient;
    }
    const cv::Mat &targetGradient = targetGradientData->gradient;
    // 遮挡、边缘坐标或异常半径会让模板与搜索ROI尺寸不再满足匹配条件。
    // 这不是进程级错误：返回低置信空结果，交由稀疏/完整/本轮模型兜底。
    if (targetGradient.empty() || source.gradient.empty()
            || targetGradient.type() != source.gradient.type()
            || targetGradient.rows < source.gradient.rows
            || targetGradient.cols < source.gradient.cols)
    {
        return result;
    }

    const int64 start = cv::getTickCount();
    cv::Mat response;
    cv::matchTemplate(targetGradient,
                      source.gradient,
                      response,
                      cv::TM_CCOEFF_NORMED);
    double maximum = -1.0;
    cv::Point location;
    cv::minMaxLoc(response, nullptr, &maximum, nullptr, &location);
    *matchMs += elapsedMilliseconds(start);
    result.center = cv::Point2f(
            static_cast<float>(
            targetGradientData->searchRect.x +
            (location.x + source.gradient.cols * 0.5) /
                options.processingScale),
        static_cast<float>(
            targetGradientData->searchRect.y +
            (location.y + source.gradient.rows * 0.5) /
                options.processingScale));
    result.score = static_cast<float>(maximum);
    return result;
}

// 在低分辨率整图梯度中，以较大的原图范围粗略寻找瞳孔。它只用于稀疏
// 照片的“把精匹配窗口搬到正确位置”，不直接作为最终坐标或DS输入。
MatchResult coarseMatchTarget(
    const cv::Mat &targetImage,
    const cv::Mat &cachedCoarseGradient,
    const PupilCrossRoundTemplate &source,
    const PupilCrossRoundOptions &options,
    double *gradientMs,
    double *matchMs)
{
    MatchResult result;
    result.center = source.center;
    if (!options.enableCoarsePrealignment || source.gradient.empty())
    {
        return result;
    }

    cv::Mat generatedGradient;
    const cv::Mat *fullGradient = &cachedCoarseGradient;
    if (fullGradient->empty())
    {
        const int64 gradientStart = cv::getTickCount();
        generatedGradient = makeGradient(
            resizeForTracking(targetImage, options.coarseProcessingScale),
            options.fastL1Gradient);
        *gradientMs += elapsedMilliseconds(gradientStart);
        fullGradient = &generatedGradient;
    }
    if (fullGradient->empty() || fullGradient->type() != source.gradient.type())
    {
        return result;
    }

    const int templateHalf = applyMaximum(
        std::max(18, cvRound(source.radius * options.templateRadiusScale)),
        options.maximumTemplateHalf);
    const cv::Rect rawSearchRect = boundedRect(
        source.center, templateHalf + options.coarseSearchMargin,
        targetImage.size());
    if (rawSearchRect.empty())
    {
        return result;
    }
    const int left = std::max(0, cvFloor(rawSearchRect.x * options.coarseProcessingScale));
    const int top = std::max(0, cvFloor(rawSearchRect.y * options.coarseProcessingScale));
    const int right = std::min(fullGradient->cols,
                               cvCeil((rawSearchRect.x + rawSearchRect.width) *
                                      options.coarseProcessingScale));
    const int bottom = std::min(fullGradient->rows,
                                cvCeil((rawSearchRect.y + rawSearchRect.height) *
                                       options.coarseProcessingScale));
    const cv::Rect scaledSearchRect(left, top,
                                    std::max(0, right - left),
                                    std::max(0, bottom - top));
    if (scaledSearchRect.empty() ||
        scaledSearchRect.width < source.gradient.cols ||
        scaledSearchRect.height < source.gradient.rows)
    {
        return result;
    }

    const int64 matchStart = cv::getTickCount();
    cv::Mat response;
    cv::matchTemplate((*fullGradient)(scaledSearchRect), source.gradient,
                      response, cv::TM_CCOEFF_NORMED);
    double maximum = -1.0;
    cv::Point location;
    cv::minMaxLoc(response, nullptr, &maximum, nullptr, &location);
    *matchMs += elapsedMilliseconds(matchStart);
    result.center = cv::Point2f(
        static_cast<float>((scaledSearchRect.x + location.x +
                            source.gradient.cols * 0.5F) /
                           options.coarseProcessingScale),
        static_cast<float>((scaledSearchRect.y + location.y +
                            source.gradient.rows * 0.5F) /
                           options.coarseProcessingScale));
    result.score = static_cast<float>(maximum);
    return result;
}

std::vector<int> fullFrameIndices(size_t frameCount)
{
    std::vector<int> indices;
    indices.reserve(frameCount);
    for (size_t index = 0; index < frameCount; ++index)
    {
        indices.push_back(static_cast<int>(index));
    }
    return indices;
}

std::vector<int> validSparseIndices(const PupilCrossRoundOptions &options,
                                    size_t frameCount)
{
    std::vector<int> indices;
    for (int index : options.sparseAnchorIndices)
    {
        if (index >= 0 && index < static_cast<int>(frameCount))
        {
            indices.push_back(index);
        }
    }
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices.size() >= 2 ? indices : std::vector<int>();
}

bool cacheIsUsable(const PupilCrossRoundCache &cache,
                   const std::vector<cv::Mat> &sourceImages,
                   const PupilCrossRoundOptions &options,
                   const std::vector<int> &templateIndices)
{
    return !sourceImages.empty() &&
           std::fabs(cache.processingScale - options.processingScale) < 1e-6F &&
           cache.tracksSubjectRight == options.trackSubjectRight &&
           cache.tracksSubjectLeft == options.trackSubjectLeft &&
           cache.maximumTemplateHalf == options.maximumTemplateHalf &&
           cache.maximumSearchMargin == options.maximumSearchMargin &&
           cache.fastL1Gradient == options.fastL1Gradient &&
           cache.sourceImageSize == sourceImages.front().size() &&
           cache.templateIndices == templateIndices &&
           (!options.trackSubjectRight ||
            cache.subjectRight.size() == sourceImages.size()) &&
           (!options.trackSubjectLeft ||
            cache.subjectLeft.size() == sourceImages.size());
}

bool prepareTemplateCache(const std::vector<cv::Mat> &sourceImages,
                          const std::vector<PupilLightFrame> &sourceFrames,
                          const PupilCrossRoundOptions &options,
                          const std::vector<int> &templateIndices,
                          PupilCrossRoundCache *cache,
                          bool *cacheReused,
                          double *templateMs,
                          std::string *failureDetail)
{
    *cacheReused =
        cacheIsUsable(*cache, sourceImages, options, templateIndices);
    if (*cacheReused)
    {
        return true;
    }

    const int64 templateStart = cv::getTickCount();
    cache->clear();
    cache->tracksSubjectRight = options.trackSubjectRight;
    cache->tracksSubjectLeft = options.trackSubjectLeft;
    cache->processingScale = options.processingScale;
    cache->maximumTemplateHalf = options.maximumTemplateHalf;
    cache->maximumSearchMargin = options.maximumSearchMargin;
    cache->fastL1Gradient = options.fastL1Gradient;
    cache->sourceImageSize = sourceImages.front().size();
    cache->templateIndices = templateIndices;
    if (options.trackSubjectRight)
    {
        cache->subjectRight.resize(sourceImages.size());
    }
    if (options.trackSubjectLeft)
    {
        cache->subjectLeft.resize(sourceImages.size());
    }

    for (int index : templateIndices)
    {
        const bool rightReady = !options.trackSubjectRight ||
            buildTemplate(sourceImages[index],
                          sourceFrames[index].subjectRight,
                          options,
                          &cache->subjectRight[index]);
        const bool leftReady = !options.trackSubjectLeft ||
            buildTemplate(sourceImages[index],
                          sourceFrames[index].subjectLeft,
                          options,
                          &cache->subjectLeft[index]);
        if (!rightReady || !leftReady)
        {
            // 明确指出哪张源图、哪只眼无法裁出模板，避免上层只能得到笼统的
            // “build source template failed”。坐标仍为原始图片坐标。
            const PupilLightEye &rightEye = sourceFrames[index].subjectRight;
            const PupilLightEye &leftEye = sourceFrames[index].subjectLeft;
            std::string text = "Source template invalid at photo "
                    + std::to_string(index + 1) + ": ";
            if (!rightReady)
            {
                text += "R(x=" + std::to_string(rightEye.center.x)
                        + ",y=" + std::to_string(rightEye.center.y)
                        + ",r=" + std::to_string(rightEye.radius) + ")";
            }
            if (!rightReady && !leftReady)
            {
                text += "; ";
            }
            if (!leftReady)
            {
                text += "L(x=" + std::to_string(leftEye.center.x)
                        + ",y=" + std::to_string(leftEye.center.y)
                        + ",r=" + std::to_string(leftEye.radius) + ")";
            }
            setError(failureDetail, text);
            // 缓存只允许以“完整构建成功”的状态参与后续复用。否则下一轮会
            // 误把半成品模板当作有效缓存，产生全-1分数或直接沿用旧坐标。
            cache->clear();
            return false;
        }
    }
    *templateMs += elapsedMilliseconds(templateStart);
    return true;
}

float pointDistance(const cv::Point2f &first, const cv::Point2f &second)
{
    const cv::Point2f difference = first - second;
    return std::sqrt(difference.x * difference.x +
                     difference.y * difference.y);
}

cv::Point2f interpolateDelta(int frameIndex,
                             const std::vector<int> &anchors,
                             const std::vector<cv::Point2f> &deltas)
{
    if (frameIndex <= anchors.front())
    {
        return deltas.front();
    }
    if (frameIndex >= anchors.back())
    {
        return deltas.back();
    }
    for (size_t index = 1; index < anchors.size(); ++index)
    {
        if (frameIndex <= anchors[index])
        {
            const int leftIndex = anchors[index - 1];
            const int rightIndex = anchors[index];
            const float ratio = static_cast<float>(frameIndex - leftIndex) /
                                static_cast<float>(rightIndex - leftIndex);
            return deltas[index - 1] * (1.0F - ratio) +
                   deltas[index] * ratio;
        }
    }
    return deltas.back();
}

struct SparseEyeMatches
{
    std::vector<int> indices;
    std::vector<cv::Point2f> deltas;
    std::vector<float> scores;
};

const PupilCrossRoundTargetGradient *cachedTargetGradient(
    const PupilCrossRoundTargetGradientCache *cache,
    bool subjectRight,
    int imageIndex,
    const cv::Size &imageSize,
    const PupilCrossRoundOptions &options)
{
    if (!cache || cache->imageSize != imageSize ||
        std::fabs(cache->processingScale - options.processingScale) > 1e-6F ||
        cache->maximumTemplateHalf != options.maximumTemplateHalf ||
        cache->maximumSearchMargin != options.maximumSearchMargin ||
        cache->fastL1Gradient != options.fastL1Gradient || imageIndex < 0)
    {
        return nullptr;
    }
    const std::vector<PupilCrossRoundTargetGradient> &values = subjectRight
        ? cache->subjectRight : cache->subjectLeft;
    if (imageIndex >= static_cast<int>(values.size()) ||
        values[imageIndex].gradient.empty())
    {
        return nullptr;
    }
    return &values[imageIndex];
}

const cv::Mat &cachedCoarseGradient(
    const PupilCrossRoundTargetGradientCache *cache,
    int imageIndex,
    const cv::Size &imageSize,
    const PupilCrossRoundOptions &options)
{
    static const cv::Mat empty;
    if (!cache || !options.enableCoarsePrealignment ||
        cache->imageSize != imageSize || !cache->coarsePrealignment ||
        std::fabs(cache->coarseProcessingScale -
                  options.coarseProcessingScale) > 1e-6F || imageIndex < 0 ||
        imageIndex >= static_cast<int>(cache->coarseGradients.size()))
    {
        return empty;
    }
    return cache->coarseGradients[imageIndex];
}

void collectSparseEyeMatches(
    bool subjectRight,
    const std::vector<int> &anchors,
    const std::vector<cv::Mat> &sourceImages,
    const std::vector<cv::Mat> &targetImages,
    const std::vector<PupilLightFrame> &sourceFrames,
    const PupilCrossRoundCache &cache,
    const PupilCrossRoundOptions &options,
    const PupilCrossRoundTargetGradientCache *targetGradientCache,
    SparseEyeMatches *matches,
    PupilCrossRoundSummary *summary)
{
    if ((subjectRight && !options.trackSubjectRight) ||
        (!subjectRight && !options.trackSubjectLeft))
    {
        return;
    }

    matches->indices.reserve(anchors.size());
    matches->deltas.reserve(anchors.size());
    matches->scores.reserve(anchors.size());
    for (int anchor : anchors)
    {
        const PupilCrossRoundTemplate &source = subjectRight
            ? cache.subjectRight[anchor] : cache.subjectLeft[anchor];
        const PupilLightEye &sourceEye = subjectRight
            ? sourceFrames[anchor].subjectRight
            : sourceFrames[anchor].subjectLeft;
        MatchResult coarse;
        const cv::Point2f *refinedSearchCenter = nullptr;
        if (options.enableCoarsePrealignment)
        {
            PupilCrossRoundOptions coarseOptions = options;
            coarseOptions.processingScale = options.coarseProcessingScale;
            PupilCrossRoundTemplate coarseTemplate;
            if (buildTemplate(sourceImages[anchor], sourceEye, coarseOptions,
                              &coarseTemplate))
            {
                coarse = coarseMatchTarget(
                    targetImages[anchor],
                    cachedCoarseGradient(targetGradientCache, anchor,
                                         targetImages[anchor].size(), options),
                    coarseTemplate, options, &summary->coarseGradientMs,
                    &summary->coarseMatchMs);
                if (coarse.score >= options.coarseMinimumMatchScore)
                {
                    refinedSearchCenter = &coarse.center;
                    ++summary->coarsePrealignedEyeCount;
                }
            }
        }
        const MatchResult result = matchTarget(
            targetImages[anchor], source, options,
            // 粗定位后原有流式小ROI围绕旧坐标，不能继续复用。
            refinedSearchCenter == nullptr
                ? cachedTargetGradient(targetGradientCache, subjectRight,
                                       anchor, targetImages[anchor].size(),
                                       options)
                : nullptr,
            refinedSearchCenter, &summary->targetGradientMs,
            &summary->matchMs);
        matches->indices.push_back(anchor);
        matches->scores.push_back(result.score);
        matches->deltas.push_back(result.center - sourceEye.center);
    }
}

// 稀疏路径不再要求三张照片全部成功：只要至少两张高可信、且位移彼此
// 连续，就可作为本眼跨轮位移的安全依据。第三张低分只记录诊断信息，
// 不应把整轮直接推入44次完整匹配。
bool selectConsistentSparseMatches(const SparseEyeMatches &allMatches,
                                   const PupilCrossRoundOptions &options,
                                   float minimumScore,
                                   SparseEyeMatches *selectedMatches)
{
    selectedMatches->indices.clear();
    selectedMatches->deltas.clear();
    selectedMatches->scores.clear();

    int bestFirst = -1;
    int bestSecond = -1;
    float bestQuality = -1.0F;
    for (size_t first = 0; first < allMatches.indices.size(); ++first)
    {
        if (allMatches.scores[first] < minimumScore)
        {
            continue;
        }
        for (size_t second = first + 1;
             second < allMatches.indices.size(); ++second)
        {
            if (allMatches.scores[second] < minimumScore ||
                // 大位移粗到细模式中，不同照片可处于受检者移动轨迹的
                // 不同阶段，位移并不要求相差16px以内；仍由双眼同照片
                // 一致性与两次高分精匹配共同把关。
                (!options.enableCoarsePrealignment &&
                 pointDistance(allMatches.deltas[first],
                               allMatches.deltas[second]) >
                    options.sparseMaximumDeltaSpread))
            {
                continue;
            }
            const float quality = allMatches.scores[first]
                    + allMatches.scores[second];
            if (quality > bestQuality)
            {
                bestQuality = quality;
                bestFirst = static_cast<int>(first);
                bestSecond = static_cast<int>(second);
            }
        }
    }
    if (bestFirst < 0 || bestSecond < 0)
    {
        return false;
    }

    selectedMatches->indices.push_back(allMatches.indices[bestFirst]);
    selectedMatches->deltas.push_back(allMatches.deltas[bestFirst]);
    selectedMatches->scores.push_back(allMatches.scores[bestFirst]);
    selectedMatches->indices.push_back(allMatches.indices[bestSecond]);
    selectedMatches->deltas.push_back(allMatches.deltas[bestSecond]);
    selectedMatches->scores.push_back(allMatches.scores[bestSecond]);
    return true;
}

bool sparseMatchContainsIndex(const SparseEyeMatches &matches, int imageIndex)
{
    return std::find(matches.indices.begin(), matches.indices.end(), imageIndex)
            != matches.indices.end();
}

bool findSparseDelta(const SparseEyeMatches &matches,
                     int imageIndex,
                     cv::Point2f *delta)
{
    for (size_t index = 0; index < matches.indices.size(); ++index)
    {
        if (matches.indices[index] == imageIndex)
        {
            *delta = matches.deltas[index];
            return true;
        }
    }
    return false;
}

// 将已通过“两张一致”校验的一只眼轨迹写入目标轮。该眼不再进入完整匹配，
// 但只有这两张锚点可作为有效瞳孔观测；其他照片只保留插值坐标供定位，
// 不能把闭眼、遮挡照片错误送入DS等后续计算。
void applySparseEyeTrajectory(bool subjectRight,
                               const SparseEyeMatches &matches,
                               const std::vector<PupilLightFrame> &sourceFrames,
                               std::vector<PupilLightFrame> *targetFrames,
                               PupilCrossRoundSummary *summary)
{
    if (matches.indices.empty() || matches.scores.empty())
    {
        return;
    }

    const float score = *std::min_element(matches.scores.begin(),
                                          matches.scores.end());
    for (size_t index = 0; index < targetFrames->size(); ++index)
    {
        PupilLightEye &targetEye = subjectRight
            ? (*targetFrames)[index].subjectRight
            : (*targetFrames)[index].subjectLeft;
        const PupilLightEye &sourceEye = subjectRight
            ? sourceFrames[index].subjectRight
            : sourceFrames[index].subjectLeft;
        const bool isLocalSparseMatch =
            sparseMatchContainsIndex(matches, static_cast<int>(index));
        targetEye.detected = true;
        targetEye.reliable = isLocalSparseMatch;
        targetEye.score = score;
        targetEye.radius = sourceEye.radius;
        targetEye.center = sourceEye.center +
            interpolateDelta(static_cast<int>(index),
                             matches.indices,
                             matches.deltas);
        targetEye.source = isLocalSparseMatch
                ? PupilSource_CrossRoundLocal
                : PupilSource_CrossRoundInterpolated;
        if (isLocalSparseMatch)
        {
            ++summary->localMatchEyeCount;
        }
        else
        {
            ++summary->interpolatedEyeCount;
            ++summary->interpolatedPositionOnlyEyeCount;
        }
    }
    ++summary->sparseTrajectoryEyeCount;
}

// 判断失败眼是否至少有一张“弱但合理”的局部证据：它本身无需构成两点
// 稀疏轨迹，但位移必须和另一只高可信眼的跨轮位移一致，防止直接借眼猜测。
bool selectCarrierCompatibleWeakEvidence(
    const SparseEyeMatches &allMatches,
    const SparseEyeMatches &carrierMatches,
    const PupilCrossRoundOptions &options,
    SparseEyeMatches *weakEvidence)
{
    weakEvidence->indices.clear();
    weakEvidence->deltas.clear();
    weakEvidence->scores.clear();
    if (carrierMatches.indices.empty() ||
        carrierMatches.deltas.empty() ||
        allMatches.indices.empty())
    {
        return false;
    }

    int bestIndex = -1;
    float bestScore = -1.0F;
    for (size_t index = 0; index < allMatches.indices.size(); ++index)
    {
        if (allMatches.scores[index] < options.singleEyeCarryMinimumScore)
        {
            continue;
        }
        const cv::Point2f carrierDelta = interpolateDelta(
            allMatches.indices[index],
            carrierMatches.indices,
            carrierMatches.deltas);
        // 位移过大通常是错模板或受检者尚未稳定，不能把它传播到另一眼。
        if (pointDistance(carrierDelta, cv::Point2f()) >
                options.singleEyeCarryMaximumDisplacement ||
            pointDistance(allMatches.deltas[index], carrierDelta) >
                options.singleEyeCarryMaximumDeltaDifference)
        {
            continue;
        }
        if (allMatches.scores[index] > bestScore)
        {
            bestScore = allMatches.scores[index];
            bestIndex = static_cast<int>(index);
        }
    }
    if (bestIndex < 0)
    {
        return false;
    }

    weakEvidence->indices.push_back(allMatches.indices[bestIndex]);
    weakEvidence->deltas.push_back(allMatches.deltas[bestIndex]);
    weakEvidence->scores.push_back(allMatches.scores[bestIndex]);
    return true;
}

// 另一眼只在弱局部证据的那一张标记为可靠；其余位置借用高可信眼位移，
// 仅服务于 ROI 连续性。上层 DS 质量门可据此决定是否使用该轮，而这些
// 插值位置永远不会被当成实测瞳孔写回下一轮源轨迹。
void applySingleEyeSparseCarryTrajectory(
    bool subjectRight,
    const SparseEyeMatches &carrierMatches,
    const SparseEyeMatches &weakEvidence,
    const std::vector<PupilLightFrame> &sourceFrames,
    std::vector<PupilLightFrame> *targetFrames,
    PupilCrossRoundSummary *summary)
{
    if (carrierMatches.indices.empty() || weakEvidence.indices.empty())
    {
        return;
    }

    const float carryScore = *std::min_element(
        carrierMatches.scores.begin(), carrierMatches.scores.end());
    const int evidenceIndex = weakEvidence.indices.front();
    const cv::Point2f evidenceDelta = weakEvidence.deltas.front();
    const float evidenceScore = weakEvidence.scores.front();
    for (size_t index = 0; index < targetFrames->size(); ++index)
    {
        PupilLightEye &targetEye = subjectRight
            ? (*targetFrames)[index].subjectRight
            : (*targetFrames)[index].subjectLeft;
        const PupilLightEye &sourceEye = subjectRight
            ? sourceFrames[index].subjectRight
            : sourceFrames[index].subjectLeft;
        const bool hasLocalEvidence = static_cast<int>(index) == evidenceIndex;
        const cv::Point2f carriedDelta = hasLocalEvidence
            ? evidenceDelta
            : interpolateDelta(static_cast<int>(index),
                               carrierMatches.indices,
                               carrierMatches.deltas);
        targetEye.detected = true;
        targetEye.reliable = hasLocalEvidence;
        targetEye.score = hasLocalEvidence ? evidenceScore : carryScore;
        targetEye.radius = sourceEye.radius;
        targetEye.center = sourceEye.center + carriedDelta;
        targetEye.source = hasLocalEvidence
                ? PupilSource_CrossRoundLocal
                : PupilSource_CrossRoundOtherEye;
        if (hasLocalEvidence)
        {
            ++summary->localMatchEyeCount;
            ++summary->singleEyeCarryLocalEvidenceCount;
        }
        else
        {
            ++summary->interpolatedEyeCount;
            ++summary->interpolatedPositionOnlyEyeCount;
            ++summary->singleEyeCarryPositionOnlyEyeCount;
            ++summary->otherEyeFallbackCount;
        }
    }
}
}

bool PupilCrossRoundTracker::prepareTargetGradientFrame(
    const cv::Mat &targetImage,
    const PupilLightFrame &sourceFrame,
    int imageIndex,
    int frameCount,
    const PupilCrossRoundOptions &options,
    PupilCrossRoundTargetGradientCache *cache,
    double *gradientMs,
    std::string *errorMessage) const
{
    if (cache == nullptr || gradientMs == nullptr || targetImage.empty() ||
        targetImage.type() != CV_8UC1 || imageIndex < 0 ||
        imageIndex >= frameCount || frameCount <= 0)
    {
        setError(errorMessage, "Invalid streaming target gradient input.");
        return false;
    }

    const bool configChanged =
        cache->imageSize != targetImage.size() ||
        cache->tracksSubjectRight != options.trackSubjectRight ||
        cache->tracksSubjectLeft != options.trackSubjectLeft ||
        std::fabs(cache->processingScale - options.processingScale) > 1e-6F ||
        cache->maximumTemplateHalf != options.maximumTemplateHalf ||
        cache->maximumSearchMargin != options.maximumSearchMargin ||
        cache->fastL1Gradient != options.fastL1Gradient ||
        cache->coarsePrealignment != options.enableCoarsePrealignment ||
        (options.enableCoarsePrealignment &&
         std::fabs(cache->coarseProcessingScale -
                   options.coarseProcessingScale) > 1e-6F) ||
        (options.enableCoarsePrealignment &&
         cache->coarseGradients.size() != static_cast<size_t>(frameCount)) ||
        (options.trackSubjectRight &&
         cache->subjectRight.size() != static_cast<size_t>(frameCount)) ||
        (options.trackSubjectLeft &&
         cache->subjectLeft.size() != static_cast<size_t>(frameCount));
    if (configChanged)
    {
        cache->clear();
        cache->tracksSubjectRight = options.trackSubjectRight;
        cache->tracksSubjectLeft = options.trackSubjectLeft;
        cache->processingScale = options.processingScale;
        cache->maximumTemplateHalf = options.maximumTemplateHalf;
        cache->maximumSearchMargin = options.maximumSearchMargin;
        cache->fastL1Gradient = options.fastL1Gradient;
        cache->coarsePrealignment = options.enableCoarsePrealignment;
        cache->coarseProcessingScale = options.coarseProcessingScale;
        cache->imageSize = targetImage.size();
        if (options.enableCoarsePrealignment)
        {
            cache->coarseGradients.resize(static_cast<size_t>(frameCount));
        }
        if (options.trackSubjectRight)
        {
            cache->subjectRight.resize(static_cast<size_t>(frameCount));
        }
        if (options.trackSubjectLeft)
        {
            cache->subjectLeft.resize(static_cast<size_t>(frameCount));
        }
    }

    if (options.trackSubjectRight)
    {
        if (!sourceFrame.subjectRight.detected ||
            !buildTargetGradient(targetImage, sourceFrame.subjectRight, options,
                                 &cache->subjectRight[imageIndex], gradientMs))
        {
            setError(errorMessage, "Unable to build right streaming gradient.");
            return false;
        }
    }
    if (options.trackSubjectLeft)
    {
        if (!sourceFrame.subjectLeft.detected ||
            !buildTargetGradient(targetImage, sourceFrame.subjectLeft, options,
                                 &cache->subjectLeft[imageIndex], gradientMs))
        {
            setError(errorMessage, "Unable to build left streaming gradient.");
            return false;
        }
    }
    if (options.enableCoarsePrealignment)
    {
        const int64 coarseStart = cv::getTickCount();
        cache->coarseGradients[imageIndex] = makeGradient(
            resizeForTracking(targetImage, options.coarseProcessingScale),
            options.fastL1Gradient);
        *gradientMs += elapsedMilliseconds(coarseStart);
        if (cache->coarseGradients[imageIndex].empty())
        {
            setError(errorMessage, "Unable to build coarse streaming gradient.");
            return false;
        }
    }
    return true;
}

bool PupilCrossRoundTracker::run(
    const std::vector<cv::Mat> &sourceImages,
    const std::vector<PupilLightFrame> &sourceFrames,
    const std::vector<cv::Mat> &targetImages,
    std::vector<PupilLightFrame> *targetFrames,
    const PupilCrossRoundOptions &options,
    PupilCrossRoundSummary *summary,
    std::string *errorMessage,
    PupilCrossRoundCache *sparseTemplateCache,
    PupilCrossRoundCache *fullTemplateCache,
    const PupilCrossRoundTargetGradientCache *targetGradientCache) const
{
    if (targetFrames == nullptr || summary == nullptr ||
        sourceImages.empty() ||
        sourceImages.size() != sourceFrames.size() ||
        targetImages.size() != sourceImages.size())
    {
        setError(errorMessage, "Invalid cross-round tracker input.");
        return false;
    }
    if (options.processingScale < 0.5F ||
        options.processingScale > 1.0F)
    {
        setError(errorMessage,
                 "Cross-round processing scale must be within [0.5, 1.0].");
        return false;
    }
    if (!options.trackSubjectRight && !options.trackSubjectLeft)
    {
        setError(errorMessage, "Cross-round tracker has no selected eye.");
        return false;
    }
    for (size_t index = 0; index < sourceImages.size(); ++index)
    {
        if (sourceImages[index].empty() || targetImages[index].empty() ||
            sourceImages[index].type() != CV_8UC1 ||
            targetImages[index].type() != CV_8UC1 ||
            sourceImages[index].size() != targetImages[index].size() ||
            (options.trackSubjectRight &&
             !sourceFrames[index].subjectRight.detected) ||
            (options.trackSubjectLeft &&
             !sourceFrames[index].subjectLeft.detected))
        {
            setError(errorMessage,
                     "Cross-round images or source trajectory are incomplete.");
            return false;
        }
    }

    *summary = PupilCrossRoundSummary();
    const int64 totalStart = cv::getTickCount();
    PupilCrossRoundCache localSparseCache;
    PupilCrossRoundCache localFullCache;
    PupilCrossRoundCache *sparseCache =
        sparseTemplateCache != nullptr ? sparseTemplateCache : &localSparseCache;
    // 未显式提供完整缓存时，本次调用仍使用独立的本地完整缓存，避免
    // 稀疏模板与完整模板在同一次运行中相互覆盖。
    PupilCrossRoundCache *fullCache =
        fullTemplateCache != nullptr ? fullTemplateCache : &localFullCache;

    const std::vector<int> sparseAnchors =
        validSparseIndices(options, sourceImages.size());
    // 稀疏阶段仅一眼失败时，保存成功眼的两点一致轨迹；后续只为失败眼
    // 运行完整匹配。该行为必须由选项显式开启，正式程序不会自动改变。
    bool reuseSparseRightTrajectory = false;
    bool reuseSparseLeftTrajectory = false;
    SparseEyeMatches sparseRightTrajectory;
    SparseEyeMatches sparseLeftTrajectory;
    if (options.enableSparseAnchorFastPath && !sparseAnchors.empty())
    {
        summary->sparseFastPathAttempted = true;
        bool sparseCacheReused = false;
        const bool sparseCacheReady =
            prepareTemplateCache(sourceImages, sourceFrames, options,
                                 sparseAnchors, sparseCache, &sparseCacheReused,
                                 &summary->sourceTemplateMs, nullptr);
        summary->sourceTemplateCacheReused = sparseCacheReused;
        summary->sparseTemplateCacheReused = sparseCacheReused;
        if (sparseCacheReady)
        {
            SparseEyeMatches rightAllMatches;
            SparseEyeMatches leftAllMatches;
            SparseEyeMatches rightMatches;
            SparseEyeMatches leftMatches;
            collectSparseEyeMatches(true, sparseAnchors, sourceImages,
                                    targetImages,
                                    sourceFrames, *sparseCache, options,
                                    targetGradientCache,
                                    &rightAllMatches, summary);
            collectSparseEyeMatches(false, sparseAnchors, sourceImages,
                                    targetImages,
                                    sourceFrames, *sparseCache, options,
                                    targetGradientCache,
                                    &leftAllMatches, summary);
            const bool rightReady = !options.trackSubjectRight
                    || selectConsistentSparseMatches(rightAllMatches,
                                                     options,
                                                     options.minimumMatchScore,
                                                     &rightMatches);
            const bool leftReady = !options.trackSubjectLeft
                    || selectConsistentSparseMatches(leftAllMatches,
                                                     options,
                                                     options.minimumMatchScore,
                                                     &leftMatches);

            // 记录三个实际候选的逐眼结果。这里保留内部索引，上层统一映射为
            // 实际照片编号后输出，避免日志再次混入灯位概念。
            for (int anchor : sparseAnchors)
            {
                PupilCrossRoundSparseDiagnostic diagnostic;
                diagnostic.imageIndex = anchor;
                for (size_t index = 0;
                     index < rightAllMatches.indices.size(); ++index)
                {
                    if (rightAllMatches.indices[index] == anchor)
                    {
                        diagnostic.subjectRightScore =
                                rightAllMatches.scores[index];
                        diagnostic.subjectRightDelta =
                                rightAllMatches.deltas[index];
                        diagnostic.subjectRightAccepted =
                                sparseMatchContainsIndex(rightMatches, anchor);
                        break;
                    }
                }
                for (size_t index = 0;
                     index < leftAllMatches.indices.size(); ++index)
                {
                    if (leftAllMatches.indices[index] == anchor)
                    {
                        diagnostic.subjectLeftScore =
                                leftAllMatches.scores[index];
                        diagnostic.subjectLeftDelta =
                                leftAllMatches.deltas[index];
                        diagnostic.subjectLeftAccepted =
                                sparseMatchContainsIndex(leftMatches, anchor);
                        break;
                    }
                }
                summary->sparseDiagnostics.push_back(diagnostic);
            }

            bool stereoReady = true;
            if (rightReady && leftReady &&
                options.trackSubjectRight && options.trackSubjectLeft)
            {
                int commonAnchorCount = 0;
                for (int anchor : sparseAnchors)
                {
                    cv::Point2f rightDelta;
                    cv::Point2f leftDelta;
                    if (!findSparseDelta(rightMatches, anchor, &rightDelta)
                            || !findSparseDelta(leftMatches, anchor, &leftDelta))
                    {
                        continue;
                    }
                    ++commonAnchorCount;
                    if (pointDistance(rightDelta, leftDelta) >
                        options.sparseMaximumStereoDeltaDifference)
                    {
                        stereoReady = false;
                        break;
                    }
                }
                // 双眼各自至少有两张有效还不够；必须至少共享一张有效照片，
                // 才能验证两眼的跨轮整体运动没有相互矛盾。
                if (commonAnchorCount == 0)
                {
                    stereoReady = false;
                }
            }

            if (rightReady && leftReady && stereoReady)
            {
                targetFrames->assign(sourceFrames.begin(),
                                     sourceFrames.end());
                std::vector<double> sparseScores;
                sparseScores.insert(sparseScores.end(),
                                    rightMatches.scores.begin(),
                                    rightMatches.scores.end());
                sparseScores.insert(sparseScores.end(),
                                    leftMatches.scores.begin(),
                                    leftMatches.scores.end());

                const float rightScore = rightMatches.scores.empty()
                    ? 1.0F
                    : *std::min_element(rightMatches.scores.begin(),
                                        rightMatches.scores.end());
                const float leftScore = leftMatches.scores.empty()
                    ? 1.0F
                    : *std::min_element(leftMatches.scores.begin(),
                                        leftMatches.scores.end());
                for (size_t index = 0; index < targetFrames->size(); ++index)
                {
                    PupilLightFrame &target = (*targetFrames)[index];
                    target.isAnchor = false;
                    if (options.trackSubjectRight)
                    {
                        target.subjectRight.detected = true;
                        target.subjectRight.reliable = true;
                        target.subjectRight.score = rightScore;
                        target.subjectRight.radius =
                            sourceFrames[index].subjectRight.radius;
                        target.subjectRight.center =
                            sourceFrames[index].subjectRight.center +
                            interpolateDelta(static_cast<int>(index),
                                             rightMatches.indices,
                                             rightMatches.deltas);
                        target.subjectRight.source =
                            sparseMatchContainsIndex(rightMatches,
                                                     static_cast<int>(index))
                                ? PupilSource_CrossRoundLocal
                                : PupilSource_CrossRoundInterpolated;
                        if (sparseMatchContainsIndex(rightMatches,
                                                     static_cast<int>(index)))
                        {
                            ++summary->localMatchEyeCount;
                        }
                        else
                        {
                            ++summary->interpolatedEyeCount;
                        }
                    }
                    if (options.trackSubjectLeft)
                    {
                        target.subjectLeft.detected = true;
                        target.subjectLeft.reliable = true;
                        target.subjectLeft.score = leftScore;
                        target.subjectLeft.radius =
                            sourceFrames[index].subjectLeft.radius;
                        target.subjectLeft.center =
                            sourceFrames[index].subjectLeft.center +
                            interpolateDelta(static_cast<int>(index),
                                             leftMatches.indices,
                                             leftMatches.deltas);
                        target.subjectLeft.source =
                            sparseMatchContainsIndex(leftMatches,
                                                     static_cast<int>(index))
                                ? PupilSource_CrossRoundLocal
                                : PupilSource_CrossRoundInterpolated;
                        if (sparseMatchContainsIndex(leftMatches,
                                                     static_cast<int>(index)))
                        {
                            ++summary->localMatchEyeCount;
                        }
                        else
                        {
                            ++summary->interpolatedEyeCount;
                        }
                    }
                }
                summary->sparseFastPathUsed = true;
                summary->scoreP05 = percentile(sparseScores, 0.05);
                summary->totalMs = elapsedMilliseconds(totalStart);
                return true;
            }

            if (!rightReady)
            {
                summary->sparseRejectReason =
                        "right_eye_has_fewer_than_two_consistent_matches";
            }
            else if (!leftReady)
            {
                summary->sparseRejectReason =
                        "left_eye_has_fewer_than_two_consistent_matches";
            }
            else
            {
                summary->sparseRejectReason =
                        "stereo_motion_is_inconsistent";
            }

            // 只有“恰好一只眼”通过两张一致校验时才能拆分完整兜底；若两眼
            // 都通过但双眼位移相互矛盾，仍必须双眼完整匹配，不能单眼猜测。
            if (options.enableSingleEyeFullMatchFallback &&
                options.trackSubjectRight && options.trackSubjectLeft)
            {
                // 正常快速路径仍严格使用minimumMatchScore；这里只为避免
                // 0.59这类“接近阈值但两张位移高度一致”的成功眼也被迫
                // 进入44次完整匹配。该低阈值不能单独决定整轮快速通过。
                SparseEyeMatches rightFallbackMatches = rightMatches;
                SparseEyeMatches leftFallbackMatches = leftMatches;
                const bool rightFallbackReady = rightReady ||
                    selectConsistentSparseMatches(
                        rightAllMatches, options,
                        options.singleEyeFallbackMinimumScore,
                        &rightFallbackMatches);
                const bool leftFallbackReady = leftReady ||
                    selectConsistentSparseMatches(
                        leftAllMatches, options,
                        options.singleEyeFallbackMinimumScore,
                        &leftFallbackMatches);
                if (rightFallbackReady != leftFallbackReady)
                {
                    // 儿童手持场景的快速补偿：一眼已有两张一致轨迹时，不必
                    // 立刻为另一眼执行22次完整匹配。另一眼至少要有一张弱
                    // 局部证据，且该证据的位移要与高可信眼相容；否则仍走
                    // 原有完整匹配，绝不只凭另一眼的位置制造瞳孔观测。
                    if (options.enableSingleEyeSparseCarry)
                    {
                        const bool carrierIsRight = rightFallbackReady;
                        const SparseEyeMatches &carrierMatches = carrierIsRight
                            ? rightFallbackMatches : leftFallbackMatches;
                        const SparseEyeMatches &weakAllMatches = carrierIsRight
                            ? leftAllMatches : rightAllMatches;
                        SparseEyeMatches weakEvidence;
                        if (selectCarrierCompatibleWeakEvidence(
                                weakAllMatches, carrierMatches, options,
                                &weakEvidence))
                        {
                            targetFrames->assign(sourceFrames.begin(),
                                                 sourceFrames.end());
                            if (carrierIsRight)
                            {
                                applySparseEyeTrajectory(
                                    true, carrierMatches, sourceFrames,
                                    targetFrames, summary);
                                applySingleEyeSparseCarryTrajectory(
                                    false, carrierMatches, weakEvidence,
                                    sourceFrames, targetFrames, summary);
                            }
                            else
                            {
                                applySparseEyeTrajectory(
                                    false, carrierMatches, sourceFrames,
                                    targetFrames, summary);
                                applySingleEyeSparseCarryTrajectory(
                                    true, carrierMatches, weakEvidence,
                                    sourceFrames, targetFrames, summary);
                            }
                            std::vector<double> carryScores;
                            carryScores.insert(carryScores.end(),
                                               carrierMatches.scores.begin(),
                                               carrierMatches.scores.end());
                            carryScores.insert(carryScores.end(),
                                               weakEvidence.scores.begin(),
                                               weakEvidence.scores.end());
                            summary->singleEyeSparseCarryUsed = true;
                            summary->sparseFastPathUsed = true;
                            summary->sparseRejectReason.clear();
                            summary->scoreP05 = percentile(carryScores, 0.05);
                            summary->totalMs = elapsedMilliseconds(totalStart);
                            return true;
                        }
                        summary->singleEyeCarryRejectReason =
                            "no_weak_match_compatible_with_carrier_eye";
                    }
                    reuseSparseRightTrajectory = rightFallbackReady;
                    reuseSparseLeftTrajectory = leftFallbackReady;
                    sparseRightTrajectory = rightFallbackMatches;
                    sparseLeftTrajectory = leftFallbackMatches;
                }
            }
        }
        else
        {
            summary->sparseRejectReason = "sparse_template_build_failed";
        }

        // 稀疏锚点低分、位移不连续或双眼位移不一致时，恢复完整匹配；若
        // 仅一眼失败，则成功眼保留两点一致轨迹，只匹配失败眼。
        summary->fullMatchFallbackTriggered = true;
        summary->localMatchEyeCount = 0;
        summary->interpolatedEyeCount = 0;
        summary->otherEyeFallbackCount = 0;
        summary->directReuseFallbackCount = 0;
    }

    PupilCrossRoundOptions fullOptions = options;
    fullOptions.trackSubjectRight = options.trackSubjectRight &&
        !reuseSparseRightTrajectory;
    fullOptions.trackSubjectLeft = options.trackSubjectLeft &&
        !reuseSparseLeftTrajectory;
    summary->singleEyeFullMatchFallbackUsed =
        reuseSparseRightTrajectory || reuseSparseLeftTrajectory;
    summary->fullMatchRequestedEyeCount =
        (fullOptions.trackSubjectRight ? 1 : 0) +
        (fullOptions.trackSubjectLeft ? 1 : 0);

    // 完整匹配前先写入可靠眼的跨轮稀疏轨迹，完整匹配循环仅更新失败眼。
    targetFrames->assign(sourceFrames.begin(), sourceFrames.end());
    if (reuseSparseRightTrajectory)
    {
        applySparseEyeTrajectory(true, sparseRightTrajectory, sourceFrames,
                                 targetFrames, summary);
    }
    if (reuseSparseLeftTrajectory)
    {
        applySparseEyeTrajectory(false, sparseLeftTrajectory, sourceFrames,
                                 targetFrames, summary);
    }

    const std::vector<int> allIndices = fullFrameIndices(sourceImages.size());
    bool fullCacheReused = false;
    if (!prepareTemplateCache(sourceImages, sourceFrames, fullOptions,
                              allIndices, fullCache, &fullCacheReused,
                              &summary->sourceTemplateMs, errorMessage))
    {
        if (errorMessage == nullptr || errorMessage->empty())
        {
            setError(errorMessage,
                     "Failed to build cross-round source template.");
        }
        return false;
    }
    summary->sourceTemplateCacheReused =
        summary->sourceTemplateCacheReused || fullCacheReused;
    summary->fullTemplateCacheReused = fullCacheReused;

    std::vector<double> scores;
    scores.insert(scores.end(), sparseRightTrajectory.scores.begin(),
                  sparseRightTrajectory.scores.end());
    scores.insert(scores.end(), sparseLeftTrajectory.scores.begin(),
                  sparseLeftTrajectory.scores.end());
    const bool trackingBoth = options.trackSubjectRight &&
                              options.trackSubjectLeft;
    for (size_t index = 0; index < targetImages.size(); ++index)
    {
        MatchResult right;
        MatchResult left;
        if (fullOptions.trackSubjectRight)
        {
            right = matchTarget(targetImages[index],
                                fullCache->subjectRight[index],
                                fullOptions,
                                cachedTargetGradient(targetGradientCache, true,
                                                     static_cast<int>(index),
                                                     targetImages[index].size(),
                                                     fullOptions),
                                nullptr, &summary->targetGradientMs,
                                &summary->matchMs);
            scores.push_back(right.score);
        }
        if (fullOptions.trackSubjectLeft)
        {
            left = matchTarget(targetImages[index],
                               fullCache->subjectLeft[index],
                               fullOptions,
                               cachedTargetGradient(targetGradientCache, false,
                                                    static_cast<int>(index),
                                                    targetImages[index].size(),
                                                    fullOptions),
                               nullptr, &summary->targetGradientMs,
                               &summary->matchMs);
            scores.push_back(left.score);
        }

        const cv::Point2f rightDelta = fullOptions.trackSubjectRight
            ? right.center - sourceFrames[index].subjectRight.center
            : cv::Point2f();
        const cv::Point2f leftDelta = fullOptions.trackSubjectLeft
            ? left.center - sourceFrames[index].subjectLeft.center
            : cv::Point2f();
        const bool rightReliable = fullOptions.trackSubjectRight &&
            right.score >= fullOptions.minimumMatchScore;
        const bool leftReliable = fullOptions.trackSubjectLeft &&
            left.score >= fullOptions.minimumMatchScore;

        PupilLightFrame &target = (*targetFrames)[index];
        target.isAnchor = false;
        if (fullOptions.trackSubjectRight)
        {
            target.subjectRight.score = right.score;
            target.subjectRight.reliable = rightReliable;
            if (rightReliable)
            {
                target.subjectRight.center =
                    sourceFrames[index].subjectRight.center + rightDelta;
                target.subjectRight.source = PupilSource_CrossRoundLocal;
                ++summary->localMatchEyeCount;
            }
            else if (trackingBoth && (leftReliable || reuseSparseLeftTrajectory))
            {
                const cv::Point2f fallbackDelta = leftReliable
                    ? leftDelta
                    : interpolateDelta(static_cast<int>(index),
                                       sparseLeftTrajectory.indices,
                                       sparseLeftTrajectory.deltas);
                target.subjectRight.center =
                    sourceFrames[index].subjectRight.center + fallbackDelta;
                target.subjectRight.source = PupilSource_CrossRoundOtherEye;
                ++summary->otherEyeFallbackCount;
            }
            else
            {
                // 单眼不存在另一眼位移，低可信时原样复用首轮坐标。
                target.subjectRight.center =
                    sourceFrames[index].subjectRight.center;
                target.subjectRight.source = PupilSource_CrossRoundDirectReuse;
                ++summary->directReuseFallbackCount;
            }
            target.subjectRight.detected = true;
            target.subjectRight.radius = sourceFrames[index].subjectRight.radius;
        }
        if (fullOptions.trackSubjectLeft)
        {
            target.subjectLeft.score = left.score;
            target.subjectLeft.reliable = leftReliable;
            if (leftReliable)
            {
                target.subjectLeft.center =
                    sourceFrames[index].subjectLeft.center + leftDelta;
                target.subjectLeft.source = PupilSource_CrossRoundLocal;
                ++summary->localMatchEyeCount;
            }
            else if (trackingBoth && (rightReliable || reuseSparseRightTrajectory))
            {
                const cv::Point2f fallbackDelta = rightReliable
                    ? rightDelta
                    : interpolateDelta(static_cast<int>(index),
                                       sparseRightTrajectory.indices,
                                       sparseRightTrajectory.deltas);
                target.subjectLeft.center =
                    sourceFrames[index].subjectLeft.center + fallbackDelta;
                target.subjectLeft.source = PupilSource_CrossRoundOtherEye;
                ++summary->otherEyeFallbackCount;
            }
            else
            {
                // 单眼不存在另一眼位移，低可信时原样复用首轮坐标。
                target.subjectLeft.center =
                    sourceFrames[index].subjectLeft.center;
                target.subjectLeft.source = PupilSource_CrossRoundDirectReuse;
                ++summary->directReuseFallbackCount;
            }
            target.subjectLeft.detected = true;
            target.subjectLeft.radius = sourceFrames[index].subjectLeft.radius;
        }
    }
    summary->scoreP05 = percentile(scores, 0.05);
    summary->totalMs = elapsedMilliseconds(totalStart);
    return true;
}
