#include "algo_utils.h"
#include "cascadepool.h"
#include "../settings/settings.h"
#include <opencv2/core/core_c.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <QDebug>
#include "perftimer.h"

using namespace cv;
//======================================辅助函数====================================

namespace {
thread_local bool g_forcePupilFailDetailLog = false;
thread_local std::atomic<int>* g_pupilFailDetailRoundLogCounter = nullptr;
thread_local int g_pupilFailDetailMaxLogsPerRound = 0;

bool acquirePupilFailDetailLogBudget()
{
    // PupilFailDetail 是逐帧逐眼的重日志；正式转灯时按 round 限流，只保留少量失败样本。
    if (g_pupilFailDetailRoundLogCounter == nullptr || g_pupilFailDetailMaxLogsPerRound <= 0) {
        return true;
    }

    const int logIndex = g_pupilFailDetailRoundLogCounter->fetch_add(1, std::memory_order_relaxed);
    return logIndex < g_pupilFailDetailMaxLogsPerRound;
}

QString rectToDiagString(const cv::Rect& rect)
{
    return QString("(%1,%2,%3,%4)")
            .arg(rect.x)
            .arg(rect.y)
            .arg(rect.width)
            .arg(rect.height);
}

#if ENABLE_PREVIEW_DIAG_LOG
bool shouldLogPreviewHaarDiag(bool foundAnyEye)
{
    static std::atomic<int> previewHaarDiagCounter(0);
    static std::atomic<int> lastPreviewHaarFound(-1);
    const int count = previewHaarDiagCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    const int currentFound = foundAnyEye ? 1 : 0;
    const int prevFound = lastPreviewHaarFound.exchange(currentFound, std::memory_order_relaxed);

    // Haar 眼框诊断是预览定位用的摘要日志：首批完整输出，后续按状态变化和间隔限流。
    return count <= 20 || prevFound != currentFound || (count % 15 == 0);
}

void logPreviewHaarDiag(const QString& stage,
                        bool foundAnyEye,
                        const cv::Rect& rightEye,
                        const cv::Rect& leftEye,
                        const cv::Rect& effectiveRoiOnResized)
{
    if (!shouldLogPreviewHaarDiag(foundAnyEye)) {
        return;
    }

    qDebug().noquote() << QString("PreviewHaarDiag: stage=%1,found=%2,"
                                  "right_empty=%3,left_empty=%4,"
                                  "right_rect=%5,left_rect=%6,"
                                  "effective_roi_scaled=%7")
                          .arg(stage)
                          .arg(foundAnyEye ? 1 : 0)
                          .arg(cvRectEmpty(rightEye) ? 1 : 0)
                          .arg(cvRectEmpty(leftEye) ? 1 : 0)
                          .arg(rectToDiagString(rightEye))
                          .arg(rectToDiagString(leftEye))
                          .arg(rectToDiagString(effectiveRoiOnResized));
}

void logPreviewHaarFinalDiag(bool foundAnyEye,
                             const cv::Rect& rawRightEye,
                             const cv::Rect& rawLeftEye,
                             const cv::Rect& rightLimit,
                             const cv::Rect& leftLimit,
                             const cv::Rect& finalRightEye,
                             const cv::Rect& finalLeftEye)
{
    if (!shouldLogPreviewHaarDiag(foundAnyEye)) {
        return;
    }

    qDebug().noquote() << QString("PreviewHaarFinalDiag: found=%1,"
                                  "raw_right=%2,raw_left=%3,"
                                  "right_limit=%4,left_limit=%5,"
                                  "final_right=%6,final_left=%7,"
                                  "right_clipped_empty=%8,left_clipped_empty=%9")
                          .arg(foundAnyEye ? 1 : 0)
                          .arg(rectToDiagString(rawRightEye))
                          .arg(rectToDiagString(rawLeftEye))
                          .arg(rectToDiagString(rightLimit))
                          .arg(rectToDiagString(leftLimit))
                          .arg(rectToDiagString(finalRightEye))
                          .arg(rectToDiagString(finalLeftEye))
                          .arg(cvRectEmpty(finalRightEye) ? 1 : 0)
                          .arg(cvRectEmpty(finalLeftEye) ? 1 : 0);
}
#endif
}

void setPupilFailDetailForceLog(bool enabled)
{
    g_forcePupilFailDetailLog = enabled;
}

void setPupilFailDetailRoundLogLimiter(std::atomic<int>* counter, int maxLogsPerRound)
{
    g_pupilFailDetailRoundLogCounter = counter;
    g_pupilFailDetailMaxLogsPerRound = maxLogsPerRound;
}

bool shouldLogPupilFailDetail()
{
    const bool wantsLog =
#if ENABLE_PREVIEW_PUPIL_FAIL_DETAIL_LOG
            true;
#else
            g_forcePupilFailDetailLog;
#endif
    if (!wantsLog) {
        return false;
    }

    // 正式转灯任务由每轮共享计数器限制，避免 22 帧双眼失败时刷屏。
    if (g_pupilFailDetailRoundLogCounter != nullptr) {
        return acquirePupilFailDetailLogBudget();
    }

    // 预览阶段只采样：前 6 条完整输出，之后每 30 次失败输出一条。
    static std::atomic<int> previewPupilFailDetailCounter(0);
    const int count = previewPupilFailDetailCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    return count <= 6 || (count % 30 == 0);
}

void SpotEnhance(IplImage *src, IplImage *dst)
{
    qDebug() << "CAlgo::SpotEnhance() into ...";

    int w = src->width;
    int h = src->height;

    IplImage *temp1 = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);
    IplImage *temp2 = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);
    // 去除高光
    cvNot(src, temp1);
    lhMorpFillHole(temp1, temp2);
    cvNot(temp2, temp1);

    // 相减得到高光位置
    cvSub(src, temp1, dst);

    cvReleaseImage(&temp1);
    cvReleaseImage(&temp2);
    qDebug() << "CAlgo::SpotEnhance() ended ...";
}

void lhMorpFillHole(const IplImage *src, IplImage *dst)
{
    IplImage *temp = cvCloneImage(src);
    double min, max;
    cvMinMaxLoc(src, &min, &max);

    //标记图像
    cvRectangle(temp, cvPoint(3, 3), cvPoint(temp->width - 7, temp->height - 7), cvScalar(max, max, max), -1);

    //将原图像作为掩模图像
    lhMorpRErode(temp, src, dst, NULL, -1);
    cvReleaseImage(&temp);
}

//形态学测地腐蚀和腐蚀重建运算
void lhMorpRErode(const IplImage *src, const IplImage *msk, IplImage *dst, IplConvKernel *se, int iterations)
{
    assert(src != NULL  && msk != NULL && dst != NULL && src != dst);

    if (iterations < 0)
    {
        //腐蚀重建
        cvMax(src, msk, dst);
        cvErode(dst, dst, se);
        cvMax(dst, msk, dst);

        int width = src->width;
        int height = src->height;
        IplImage  *temp1 = cvCreateImage(cvSize(width, height), 8, 1);
        do
        {
            //record last result
            cvCopy(dst, temp1);
            cvErode(dst, dst, se);
            cvMax(dst, msk, dst);
        }
        while (lhImageCmp(temp1, dst) != 0);

        cvReleaseImage(&temp1);

        return;

    }
    else if (iterations == 0)
    {
        cvCopy(src, dst);
    }
    else
    {
        //普通测地腐蚀 p137(6.2)
        cvMax(src, msk, dst);
        cvErode(dst, dst, se);
        cvMax(dst, msk, dst);

        for (int i = 1; i < iterations; i++)
        {
            cvErode(dst, dst, se);
            cvMax(dst, msk, dst);
        }
    }
}

int lhImageCmp(const IplImage *_img1, const IplImage *_img2)
{
    if (_img1->width != _img2->width || _img1->height != _img2->height || _img1->nChannels != _img2->nChannels) {
        return false;
    }

    if (_img1->width == _img1->width / 4 * 4) {
        return memcmp(_img1->imageData, _img2->imageData, _img1->imageSize);        // opencv 的图像矩阵的行是对齐到 4 的
    } else {
        IplImage *img_temp = cvCreateImage(cvSize(_img1->width, _img1->height), _img1->depth, _img1->nChannels);

        cvCmp(_img1, _img2, img_temp, CV_CMP_NE);
        bool is_same = (cvSum(img_temp).val[0] == 0);

        cvReleaseImage(&img_temp);

        return is_same;
    }
}

void FillInternalContours(IplImage *pBinary, double dAreaThre)
{
    qDebug() << "CAlgo::FillInternalContours() into ...";

    double dConArea;
    CvSeq *pContour = NULL;
    CvSeq *pConInner = NULL;
    CvMemStorage *pStorage = NULL;
    // 执行条件
    if (pBinary)
    {
        // 查找所有轮廓
        pStorage = cvCreateMemStorage(0);
        cvFindContours(pBinary, pStorage, &pContour, sizeof(CvContour), CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);
        // 填充所有轮廓
        cvDrawContours(pBinary, pContour, cvScalar(255, 255, 255), cvScalar(255, 255, 255), 2, CV_FILLED, 8, cvPoint(0, 0));
        // 外轮廓循环
        int wai = 0;
        int nei = 0;
        for (; pContour != NULL; pContour = pContour->h_next)
        {
            wai++;
            // 内轮廓循环
            for (pConInner = pContour->v_next; pConInner != NULL; pConInner = pConInner->h_next)
            {
                nei++;
                // 内轮廓面积
                dConArea = fabs(cvContourArea(pConInner, CV_WHOLE_SEQ));
                //				printf("%f\n", dConArea);
                if (dConArea <= dAreaThre)
                {
                    cvDrawContours(pBinary, pConInner, cvScalar(255, 255, 255), cvScalar(255, 255, 255), 0, CV_FILLED, 8, cvPoint(0, 0));
                }
            }
        }
        //		printf("wai = %d, nei = %d\n", wai, nei);
        cvReleaseMemStorage(&pStorage);
        pStorage = NULL;
    }

    qDebug() << "CAlgo::FillInternalContours() ended ...";
}

int ConnectEdge(IplImage *src)
{
    qDebug() << "CAlgo::ConnectEdge() into ...";

    if (NULL == src) {
        qDebug() << "CAlgo::ConnectEdge() ended ...";
        return 1;
    }

    int width = src->width;
    int height = src->height;

    uchar *data = (uchar *)src->imageData;
    for (int i = 2; i < height - 2; i++)
    {
        for (int j = 2; j < width - 2; j++)
        {
            //如果该中心点为255,则考虑它的八邻域
            if (data[i * src->widthStep + j] == 255)
            {
                int num = 0;
                for (int k = -1; k < 2; k++)
                {
                    for (int l = -1; l < 2; l++)
                    {
                        //如果八邻域中有灰度值为0的点，则去找该点的十六邻域
                        if (k != 0 && l != 0 && data[(i + k) * src->widthStep + j + l] == 255)
                            num++;
                    }
                }
                //如果八邻域中只有一个点是255，说明该中心点为端点，则考虑他的十六邻域
                if (num == 1)
                {
                    for (int k = -2; k < 3; k++)
                    {
                        for (int l = -2; l < 3; l++)
                        {
                            //如果该点的十六邻域中有255的点，则该点与中心点之间的点置为255
                            if (!(k < 2 && k > -2 && l < 2 && l > -2) && data[(i + k) * src->widthStep + j + l] == 255)
                            {
                                data[(i + k / 2) * src->widthStep + j + l / 2] = 255;
                            }
                        }
                    }
                }
            }
        }
    }

    qDebug() << "CAlgo::ConnectEdge() ended ...";
    return 0;
}

Mat binaryImage(const cv::Mat& srcImg)
{
    Mat _img;
    adaptiveThreshold(srcImg, _img, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 95,-6);

    //9*9圆形结构元素，使得瞳孔形态毛刺少，圆度更好
    Mat kernel = getStructuringElement(MORPH_ELLIPSE,Size(9,9));
    Mat mat;
    morphologyEx(_img, mat, MORPH_OPEN, kernel);

    return mat;
}

namespace {

// 在一个固定大小的预测 ROI 中寻找最接近圆形的瞳孔轮廓。
// ROI 越界时直接失败，不能裁剪后继续计算，否则会把边界伪轮廓当成瞳孔。
bool refinePupilInOneRoi(const cv::Mat& image,
                         int imageNumber,
                         enWhichEye whichEye,
                         const cv::Point2f& predictedCenter,
                         float whRatioThreshold,
                         int roiSize,
                         PupilRoiRefineResult& result,
                         QString* rejectReason)
{
    const int half = roiSize / 2;
    const cv::Rect imageRect(0, 0, image.cols, image.rows);
    const cv::Rect roi(cvRound(predictedCenter.x) - half,
                       cvRound(predictedCenter.y) - half,
                       roiSize, roiSize);
    result.roiSize = roiSize;
    if ((roi & imageRect) != roi) {
        if (rejectReason != nullptr) {
            *rejectReason = QStringLiteral("roi_out_of_bounds");
        }
        return false;
    }

    const cv::Mat binary = binaryImage(image(roi));
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    int bestIndex = -1;
    double bestScore = -std::numeric_limits<double>::infinity();
    double bestArea = 0.0;
    double bestRatio = 0.0;
    double bestCircularity = 0.0;
    cv::Point2f bestCenter;
    float bestRadius = 0.0f;
    for (size_t index = 0; index < contours.size(); ++index) {
        const std::vector<cv::Point>& contour = contours[index];
        const double area = cv::contourArea(contour);
        if (area <= MIN_PUPIL_AREA || area >= MAX_PUPIL_AREA) {
            continue;
        }

        const cv::RotatedRect rotated = cv::minAreaRect(contour);
        const double width = rotated.size.width;
        const double height = rotated.size.height;
        if (width <= 0.0 || height <= 0.0) {
            continue;
        }
        const double ratio = std::min(width, height) / std::max(width, height);
        if (ratio < whRatioThreshold) {
            continue;
        }

        const cv::Rect bounds = cv::boundingRect(contour);
        if (bounds.x <= 0 || bounds.y <= 0
                || bounds.x + bounds.width >= roi.width - 1
                || bounds.y + bounds.height >= roi.height - 1) {
            continue;
        }

        cv::Point2f localCenter;
        float radius = 0.0f;
        cv::minEnclosingCircle(contour, localCenter, radius);
        const cv::Point2f globalCenter(localCenter.x + roi.x,
                                       localCenter.y + roi.y);
        const double shift = std::hypot(
                static_cast<double>(globalCenter.x - predictedCenter.x),
                static_cast<double>(globalCenter.y - predictedCenter.y));
        if (!std::isfinite(globalCenter.x) || !std::isfinite(globalCenter.y)
                || !std::isfinite(radius) || radius < 6.0f || radius > 64.0f
                || !isNormalPupil(globalCenter, whichEye)) {
            continue;
        }

        const double perimeter = cv::arcLength(contour, true);
        const double circularity = perimeter > 0.0
                ? 4.0 * CV_PI * area / (perimeter * perimeter) : 0.0;
        // 长宽比优先，圆度用于同面积候选之间的稳定排序。
        const double candidateScore = ratio * 0.7
                + std::min(circularity, 1.0) * 0.3;
        if (candidateScore > bestScore) {
            bestIndex = static_cast<int>(index);
            bestScore = candidateScore;
            bestArea = area;
            bestRatio = ratio;
            bestCircularity = circularity;
            bestCenter = globalCenter;
            bestRadius = std::floor(radius);
        }
    }

    if (bestIndex < 0) {
        if (rejectReason != nullptr) {
            *rejectReason = contours.empty()
                    ? QStringLiteral("no_contour")
                    : QStringLiteral("no_valid_contour");
        }
        return false;
    }

    result.valid = true;
    result.refinedCenter = bestCenter;
    result.refinedRadius = bestRadius;
    result.centerShift = static_cast<float>(std::hypot(
            static_cast<double>(bestCenter.x - predictedCenter.x),
            static_cast<double>(bestCenter.y - predictedCenter.y)));
    result.contourArea = bestArea;
    result.contourRatio = bestRatio;
    result.contourCircularity = bestCircularity;
    Q_UNUSED(imageNumber);
    Q_UNUSED(bestIndex);
    return true;
}

} // namespace

bool refinePupilInPredictedRoi(const cv::Mat& image,
                               int imageNumber,
                               enWhichEye whichEye,
                               const cv::Point2f& predictedCenter,
                               float predictedRadius,
                               float whRatioThreshold,
                               PupilRoiRefineResult& result)
{
    const auto start = std::chrono::steady_clock::now();
    result = PupilRoiRefineResult();
    result.predictedCenter = predictedCenter;
    result.predictedRadius = predictedRadius;

    auto finish = [&result, &start]() {
        result.elapsedMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
    };
    if (image.empty() || image.channels() != 1) {
        result.rejectReason = QStringLiteral("invalid_gray_image");
        finish();
        return false;
    }
    if (!std::isfinite(predictedCenter.x) || !std::isfinite(predictedCenter.y)
            || !std::isfinite(predictedRadius)) {
        result.rejectReason = QStringLiteral("non_finite_prediction");
        finish();
        return false;
    }

    QString reason;
    if (refinePupilInOneRoi(image, imageNumber, whichEye, predictedCenter,
                            whRatioThreshold, ROI_WIDTH, result, &reason)) {
        result.fallbackLevel = 1;
        result.rejectReason.clear();
        finish();
        return true;
    }
    // 本方案明确移除193×193扩大ROI；失败后由调用方按照片共享一次传统双眼兜底。
    result.rejectReason = QStringLiteral("roi129_%1").arg(reason);
    finish();
    return false;
}

/**
 * @brief 优化后的二值化函数
 *
 * 优化策略：
 * 1. 下采样：将图像缩小约4倍，大幅减少自适应阈值的计算量。
 * 2. 在小图上执行自适应阈值。
 * 3. 放大回原尺寸（使用最近邻插值保持二值特性）。
 * 4. 形态学操作在原图上执行（保持边缘精度）。
 */
cv::Mat binaryImageOptimized(const cv::Mat& srcImg)
{
    // 参数配置
    const int ADAPTIVE_BLOCK_SIZE = 95;
    const int ADAPTIVE_C = 6;
    const double SCALE_FACTOR = 0.25; // 缩小到原来的 1/4 (宽高各减半)

    // 1. 下采样
    cv::Mat smallImg;
    // 使用 INTER_AREA 进行缩小，抗锯齿效果好
    cv::resize(srcImg, smallImg, cv::Size(), SCALE_FACTOR, SCALE_FACTOR, cv::INTER_AREA);

    // 2. 在小图上执行自适应阈值
    // 注意：Block Size 也需要相应缩小，以保持相同的物理覆盖范围
    // 原始 95 -> 缩小后约 95 * 0.25 ≈ 23.77 (取奇数 23 或 25)
    int smallBlockSize = static_cast<int>(ADAPTIVE_BLOCK_SIZE * SCALE_FACTOR);
    if (smallBlockSize % 2 == 0) smallBlockSize++; // 必须是奇数
    if (smallBlockSize < 3) smallBlockSize = 3;    // 最小限制

    cv::Mat binarySmall;
    cv::adaptiveThreshold(smallImg, binarySmall, 255,
                          cv::ADAPTIVE_THRESH_MEAN_C,
                          cv::THRESH_BINARY,
                          smallBlockSize,
                          ADAPTIVE_C); // C 值通常保持不变或微调

    // 3. 放大回原尺寸
    cv::Mat binaryImg;
    // 使用 INTER_NEAREST 插值，因为二值图像放大不需要平滑，且速度最快
    cv::resize(binarySmall, binaryImg, srcImg.size(), 0, 0, cv::INTER_NEAREST);

    // 4. 形态学操作（在原图尺寸上进行，保证瞳孔边缘精度）
    // 9*9圆形结构元素
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9));
    cv::Mat result;
    cv::morphologyEx(binaryImg, result, cv::MORPH_OPEN, kernel);

    return result;
}


//根据瞳孔中心坐标是否在左右眼安全区域内，检测瞳孔是否正常
bool isNormalPupil(Point2f _pc,enWhichEye _whichEye) {
    if(_pc.y<=ROI_HEIGHT_HALF || _pc.y>=IMG_HEIGHT-ROI_HEIGHT_HALF) {
        return false;
    }
    if(_whichEye==whichEye_Right) {
        if(_pc.x <= ROI_WIDTH_HALF || _pc.x >= IMG_WIDTH/2-ROI_WIDTH_HALF ) {
            return false;
        } else {
            return true;
        }
    }
    if(_whichEye==whichEye_Left) {
        if(_pc.x <= IMG_WIDTH/2 + ROI_WIDTH_HALF || _pc.x >= IMG_WIDTH - ROI_WIDTH_HALF) {
            return false;
        } else {
            return true;
        }
    }
    return false;
}

/* 筛查仪上有23颗灯，分为4组，每组一个角度，除了中间那颗，其他22颗对应22张图，1~6张为0°，7～12 为60°，13~18 为120°，21～22 为41° */
// 确定灯珠角度   img_idx图片序号
int getImageAngle(int img_idx)
{
    int angle=0;
    if(img_idx<=6)
    {
        angle=0;
    } else if(img_idx<=12)
    {
        angle=60;
    } else if(img_idx<=18)
    {
        angle=120;
    } else if(img_idx<=20)
    {
        angle=139;
    } else if(img_idx<=22)
    {
        angle=41;
    }

    return angle;
}

void rotateImage(const cv::Mat& img, cv::Mat& img_rotate, int degree)
{
    // 旋转中心为图像中心
    cv::Point2f center;
    center.x = img.cols / 2.0f;
    center.y = img.rows / 2.0f;

    // 计算二维旋转的仿射变换矩阵
    cv::Mat M = cv::getRotationMatrix2D(center, degree, 1.0);

    // 变换图像，并用黑色填充其余值
    cv::warpAffine(img, img_rotate, M, img.size(),
                   cv::INTER_LINEAR | cv::WARP_FILL_OUTLIERS,
                   cv::BORDER_CONSTANT, cv::Scalar(0));
}

/**
 * @brief 在局部区域内精确地定位单个瞳孔。
 * @param img 输入图像 (cv::Mat)。
 * @param _pupil_info 包含预估中心点，返回精确的瞳孔信息。
 * @return 定位是否成功。
 */
bool accOnePupil(cv::Mat img, stPupilInfo &_pupil_info) {
    bool is_succ = false;

    // 1. 定义 ROI 矩形区域 (cv::Rect)
    // 注意：center.x 和 center.y 必须是整型，这里假设它们已经是整型或可安全转换为整型
    int roi_x = (int)_pupil_info.center.x - ROI_WIDTH_HALF;
    int roi_y = (int)_pupil_info.center.y - ROI_HEIGHT_HALF;

    cv::Rect rect(roi_x, roi_y, ROI_WIDTH, ROI_HEIGHT);

    // 2. 边界检查: 使用 cv::Rect::operator& 确保 ROI 在图像范围内
    cv::Rect img_rect(0, 0, img.cols, img.rows);
    cv::Rect safe_rect = rect & img_rect; // 取交集

    // 如果交集结果的面积不等于原始 ROI 的面积，则说明 ROI 超出边界
    if (safe_rect != rect)
    {
        qDebug() << "CAlgo::accOnePupil(): logic exception! pupil rect out of img boundaries.";
        return is_succ;
    }

    // 3. 提取 ROI
    // 注意：rois_img 是原始 img 数据的深拷贝，浅拷贝会出问题！。
    cv::Mat roi_img = img(rect).clone();

    // 4. 精确定位: 在 ROI 图像中查找瞳孔中心
    is_succ = pupilCenter(roi_img, _pupil_info, whichEye_Right);

    if (is_succ) {
        // 5. 坐标校正
        // pupilCenter 返回的 _pupil_info.center 是相对于 roi_img (起始于 rect.x, rect.y) 的相对坐标。
        // 将相对坐标转换为绝对坐标。
        _pupil_info.center.x += rect.x;
        _pupil_info.center.y += rect.y;
    }

    // cv::Mat 具有自动内存管理，无需 cvReleaseImage 或 Q_NULLPTR 操作。
    return is_succ;
}


bool pupilCenter(const cv::Mat& pupilImg, stPupilInfo& _pupil_info, enWhichEye _which_eye)
{
    // 1. 自适应阈值
    cv::Mat bImg2;
    // 对应 cvAdaptiveThreshold(pupilImg, bImg2, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 51, 0);
    cv::adaptiveThreshold(pupilImg, bImg2, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 51, 0);

    // 2. 寻找轮廓
    std::vector<std::vector<cv::Point>> contours;
    // 对应 cvFindContours(..., CV_RETR_LIST, CV_CHAIN_APPROX_NONE)
    cv::findContours(bImg2, contours, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    bool succ_filter=false;
    // 3. 获取光点
    cv::Point glintPoint;
    cv::minMaxLoc(pupilImg, nullptr, nullptr, nullptr, &glintPoint);
    float wh_ratio=0.80;
    // 4. 筛选轮廓
    succ_filter= selectContour(contours, _pupil_info, glintPoint, wh_ratio);

    // 5. 可视化
//    cv::Mat img_rgb;
//    cv::cvtColor(pupilImg, img_rgb, cv::COLOR_GRAY2RGB);
//    cv::circle(img_rgb, cv::Point((int)_pupil_info.center.x,(int)_pupil_info.center.y), (int)_pupil_info.radius, cv::Scalar(0, 255, 0));

//    cv::imshow("pupilImg", pupilImg);
//    cv::imshow("img_rgb", img_rgb);
//    cv::waitKey(0);

    return succ_filter;
}


/**
 * @brief 从轮廓列表中筛选符合面积和长宽比要求的轮廓
 *
 * 优化说明：
 * 1. 移除了返回指针容器(std::vector<*>)的反模式设计，改为返回轮廓对象的深拷贝容器。
 * 2. 彻底消除了 const_cast，保证输入数据的不可变性。
 * 3. 增加了宽高为 0 的防御性检查，防止后续计算长宽比时出现除以 0 的浮点异常。
 */
std::vector<std::vector<cv::Point>> filterContoursByAreaAndRatio(
    const std::vector<std::vector<cv::Point>>& contours,
    double minArea, double maxArea, double whRatio)
{
    std::vector<std::vector<cv::Point>> validContours; // 存储符合条件的轮廓副本

    for (size_t i = 0; i < contours.size(); i++) {
        // 1. 面积过滤
        double area = cv::contourArea(contours[i]);
        if (area <= minArea || area >= maxArea) {
            continue;
        }

        // 2. 长宽比过滤。使用旋转外接矩形，避免斜向长条被轴对齐外接框伪装成高圆度。
        cv::RotatedRect rect = cv::minAreaRect(contours[i]);
        double h = static_cast<double>(rect.size.height);
        double w = static_cast<double>(rect.size.width);

        // 防御性编程：防止极端退化轮廓导致除以 0
        if (w == 0 || h == 0) {
            continue;
        }

        double current_ratio = std::min(h, w) / std::max(h, w);
        if (current_ratio > whRatio) {
            // 将符合条件的轮廓深拷贝入结果集，切断与原容器的生命周期耦合
            validContours.push_back(contours[i]);
        }
    }

    return validContours; // 编译器会进行 RVO/NRVO 优化，无多余拷贝开销
}

/**
 * @brief 从过滤后的轮廓中选择最佳候选瞳孔轮廓
 *
 * 逻辑说明（保持原有业务逻辑不变）：
 * 1. 候选数为 0：失败。
 * 2. 候选数为 1：直接提取最小外接圆。
 * 3. 候选数 2~3：通过检查映光点是否在轮廓内部来确定最终瞳孔。
 * 4. 候选数 > 3：判定为干扰过多，直接失败。
 */
bool selectBestPupilContour(
    const std::vector<std::vector<cv::Point>>& candidateContours,
    cv::Point2f glint,
    stPupilInfo& _pupil_info)
{
    int len = static_cast<int>(candidateContours.size());

    // 策略 1：无候选轮廓
    if (len == 0) {
        return false;
    }
    // 策略 2：唯一候选，直接拟合
    else if (len == 1) {
        cv::Point2f center;
        float radius;
        cv::minEnclosingCircle(candidateContours[0], center, radius);
        _pupil_info.center = center;
        _pupil_info.radius = radius;
        _pupil_info.rect = cv::boundingRect(candidateContours[0]);
        return true;
    }
    // 策略 3：少量候选，利用映光点 进行精准判定
    else if (len <= 3) {
        for (int i = 0; i < len; i++) {
            // pointPolygonTest 返回值为正表示点在多边形内部
            double dist = cv::pointPolygonTest(candidateContours[i], glint, true);
            if (dist > 0) {
                cv::Point2f center;
                float radius;
                cv::minEnclosingCircle(candidateContours[i], center, radius);
                _pupil_info.center = center;
                _pupil_info.radius = radius;
                _pupil_info.rect = cv::boundingRect(candidateContours[i]);
                return true;
            }
        }
        return false; // 所有候选轮廓内均未包裹映光点，判定失败
    }
    // 策略 4：候选过多，认为存在严重干扰，拒绝猜测
    else {
        return false;
    }
}

float validPupilRadiusMin()
{
    return 10.0f;
}

float validPupilRadiusMax()
{
    return 31.0f;
}

bool isValidPupilRadius(float radius)
{
    return radius >= validPupilRadiusMin() && radius <= validPupilRadiusMax();
}

bool findReliableGlintPoint(const cv::Mat& img,
                            cv::Point2f& glintPoint,
                            double& maxValue,
                            double& localMean,
                            double& localStd)
{
    if (img.empty() || img.channels() != 1) {
        return false;
    }

    cv::Point maxPoint;
    cv::minMaxLoc(img, nullptr, &maxValue, nullptr, &maxPoint);

    static const int GLINT_STATS_HALF_SIZE = 45;
    cv::Rect statsRect(maxPoint.x - GLINT_STATS_HALF_SIZE,
                       maxPoint.y - GLINT_STATS_HALF_SIZE,
                       GLINT_STATS_HALF_SIZE * 2 + 1,
                       GLINT_STATS_HALF_SIZE * 2 + 1);
    statsRect = statsRect & cv::Rect(0, 0, img.cols, img.rows);
    if (statsRect.width < 20 || statsRect.height < 20) {
        statsRect = cv::Rect(0, 0, img.cols, img.rows);
    }

    cv::Scalar meanValue;
    cv::Scalar stdValue;
    cv::meanStdDev(img(statsRect), meanValue, stdValue);
    localMean = meanValue[0];
    localStd = stdValue[0];
    glintPoint = cv::Point2f(static_cast<float>(maxPoint.x),
                             static_cast<float>(maxPoint.y));

    return maxValue >= 70.0
            && maxValue >= localMean + std::max(8.0, localStd * 1.2);
}

bool findPupilForSimulatedEye(cv::Mat img, int num, cv::Point2f& _center, float& _radius,
                              float wh_ratio_threshold)
{
    // 模拟眼只有瞳孔，缺少正常人眼的眼睑/虹膜/稳定映光点特征；这里保留旧算法的宽松路径。
    PERF_SCOPE("findPupilForSimulatedEye");
#if ENABLE_ALGO_TIMING_LOG
    ALGO_TIMING_SCOPE(AlgoTimingStage_PupilContour);
#endif

    Mat bImg = binaryImage(img);

    std::vector<std::vector<Point>> contours;
    std::vector<Vec4i> hierarchy;
    cv::findContours(bImg, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    std::vector<std::vector<cv::Point>> candidateContours =
        filterContoursByAreaAndRatio(contours, MIN_PUPIL_AREA, MAX_PUPIL_AREA,
                                     wh_ratio_threshold);

    if (candidateContours.empty()) {
#if ENABLE_PREVIEW_VERBOSE_LOG
        qDebug() << QString("图片%1:模拟眼旧算法路径未找到候选瞳孔").arg(num);
#endif
        return false;
    }

    int bestIndex = -1;
    float max_wh_ratio = 0.0f;
    for (size_t i = 0; i < candidateContours.size(); ++i) {
        const auto& contour = candidateContours[i];
        cv::RotatedRect contourRect = cv::minAreaRect(contour);
        float w = static_cast<float>(contourRect.size.width);
        float h = static_cast<float>(contourRect.size.height);
        if (w == 0 || h == 0) {
            continue;
        }

        float wh_ratio = std::min(w, h) / std::max(w, h);
        if (wh_ratio > max_wh_ratio) {
            max_wh_ratio = wh_ratio;
            bestIndex = static_cast<int>(i);
        }
    }

    if (bestIndex < 0 || max_wh_ratio < wh_ratio_threshold) {
#if ENABLE_PREVIEW_VERBOSE_LOG
        qDebug() << QString("图片%1:模拟眼旧算法路径瞳孔长宽比不合理(ratio=%2)")
                    .arg(num).arg(max_wh_ratio);
#endif
        return false;
    }

    cv::minEnclosingCircle(candidateContours[bestIndex], _center, _radius);
    _radius = std::min(std::floor(_radius), std::floor((float)ROI_WIDTH_HALF));

    Q_ASSERT(_radius <= ROI_WIDTH_HALF);
    return true;
}

bool findPupilLegacyForCalc(cv::Mat img, int num, cv::Point2f& _center, float& _radius,
                            float wh_ratio_threshold,
                            double* outArea,
                            double* outContourRatio,
                            double* outCircularity)
{
    // 正式转灯图和预览全图找瞳孔恢复旧版宽松策略：只做轮廓面积/长宽比筛选和最小外接圆拟合。
    PERF_SCOPE("findPupilLegacyForCalc");
#if ENABLE_ALGO_TIMING_LOG
    ALGO_TIMING_SCOPE(AlgoTimingStage_PupilContour);
#endif

    if (outArea != nullptr) {
        *outArea = 0.0;
    }
    if (outContourRatio != nullptr) {
        *outContourRatio = 0.0;
    }
    if (outCircularity != nullptr) {
        *outCircularity = 0.0;
    }

    Mat bImg = binaryImage(img);

    std::vector<std::vector<Point>> contours;
    std::vector<Vec4i> hierarchy;
    cv::findContours(bImg, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    std::vector<std::vector<cv::Point>> candidateContours =
        filterContoursByAreaAndRatio(contours, MIN_PUPIL_AREA, MAX_PUPIL_AREA,
                                     wh_ratio_threshold);

    if (candidateContours.empty()) {
#if ENABLE_PREVIEW_VERBOSE_LOG
        qDebug() << QString("图片%1:旧版全图路径未找到候选瞳孔").arg(num);
#endif
        return false;
    }

    int bestIndex = -1;
    float max_wh_ratio = 0.0f;
    for (size_t i = 0; i < candidateContours.size(); ++i) {
        const auto& contour = candidateContours[i];
        cv::RotatedRect contourRect = cv::minAreaRect(contour);
        float w = static_cast<float>(contourRect.size.width);
        float h = static_cast<float>(contourRect.size.height);
        if (w == 0 || h == 0) {
            continue;
        }

        float wh_ratio = std::min(w, h) / std::max(w, h);
        if (wh_ratio > max_wh_ratio) {
            max_wh_ratio = wh_ratio;
            bestIndex = static_cast<int>(i);
        }
    }

    if (bestIndex < 0 || max_wh_ratio < wh_ratio_threshold) {
#if ENABLE_PREVIEW_VERBOSE_LOG
        qDebug() << QString("图片%1:旧版全图路径瞳孔长宽比不合理(ratio=%2)")
                    .arg(num).arg(max_wh_ratio);
#endif
        return false;
    }

    cv::minEnclosingCircle(candidateContours[bestIndex], _center, _radius);
    _radius = std::min(std::floor(_radius), std::floor((float)ROI_WIDTH_HALF));

    const double area = cv::contourArea(candidateContours[bestIndex]);
    const cv::RotatedRect bestRect =
            cv::minAreaRect(candidateContours[bestIndex]);
    const double bestWidth = bestRect.size.width;
    const double bestHeight = bestRect.size.height;
    const double ratio = (bestWidth > 0.0 && bestHeight > 0.0)
            ? std::min(bestWidth, bestHeight)
                    / std::max(bestWidth, bestHeight)
            : 0.0;
    const double perimeter = cv::arcLength(candidateContours[bestIndex], true);
    const double circularity = perimeter > 0.0
            ? 4.0 * CV_PI * area / (perimeter * perimeter) : 0.0;
    if (outArea != nullptr) {
        *outArea = area;
    }
    if (outContourRatio != nullptr) {
        *outContourRatio = ratio;
    }
    if (outCircularity != nullptr) {
        *outCircularity = circularity;
    }

    Q_ASSERT(_radius <= ROI_WIDTH_HALF);
    return true;
}


bool selectContour(std::vector<std::vector<cv::Point>>& contours,
                   stPupilInfo& _pupil_info,
                   cv::Point2f glint, float wh_ratio_threshold)
{
    // 使用新的辅助函数筛选轮廓
    auto candidateContours = filterContoursByAreaAndRatio(
        contours, MIN_PUPIL_AREA, MAX_PUPIL_AREA, wh_ratio_threshold);

    // 使用新的辅助函数选择最佳轮廓
    return selectBestPupilContour(candidateContours, glint, _pupil_info);
}


bool getGlintBlobPoint(const cv::Mat &_img,
                           cv::Point &_point,
                           enWhichEye whichEye,
                           double glintThreshold = 100.0,
                           int borderMargin = 10)
{
    // 输入验证
    if (_img.empty() || _img.channels() != 1) {
        return false;
    }

    // 检测最大值
    double max_value;
    cv::minMaxLoc(_img, nullptr, &max_value);

    // 检查阈值
//    if (max_value < glintThreshold) {
//        return false;
//    }

    // 计算有效区域
    const int validLeft = borderMargin;
    const int validTop = borderMargin;
    const int validRight = _img.cols - borderMargin;
    const int validBottom = _img.rows - borderMargin;

    if (validLeft >= validRight || validTop >= validBottom) {
        return false;
    }

    // 直接遍历有效区域，避免创建中间Mat
    int count = 0;
    cv::Point best_point;
    bool found = false;

    // 根据眼睛类型初始化最佳点
    if (whichEye == whichEye_Right) {
        best_point = cv::Point(INT_MAX, INT_MAX);
    } else {
        best_point = cv::Point(0, 0);
    }

    // 只在有效区域内遍历
    for (int r = validTop; r < validBottom; ++r) {
        const uchar* ptr = _img.ptr<uchar>(r);
        for (int c = validLeft; c < validRight; ++c) {
            if (ptr[c] >= max_value) {
                if (++count > MAX_SPOT_COUNTER) {
                    return false;  // 超过最大亮点数
                }

                if (whichEye == whichEye_Right) {
                    if (c < best_point.x) {
                        best_point = cv::Point(c, r);
                    }
                } else {
                    if (c > best_point.x) {
                        best_point = cv::Point(c, r);
                    }
                }
                found = true;
            }
        }
    }

    if (found) {
        _point = best_point;
        return true;
    }

    return false;
}


void reduceGlintBlob(const Mat &src, const Point &spot, Mat &dst)
{
    const int GLINTREGION = 4;
    // 以中心点上下左右扩展4个点，形成9 * 9的映光点重写区域，区域左上角坐标(x1,y1),右下角坐标(x2,y2)
    int x1 = spot.x - GLINTREGION;
    int x2 = spot.x + GLINTREGION;
    int y1 = spot.y - GLINTREGION;
    int y2 = spot.y + GLINTREGION;

    double glintValue[40];
    // 理想情况下，9 * 9区域的每个像素值通过外围一圈的像素在不同位置下的不同权重叠加计算出来
    // 9 * 9区域外围一圈（11 * 11方框）的像素（共40个像素），对9 * 9区域每个像素处理：
    // 外围40个像素对应当前像素坐标下的权重值与该点像素值乘积，再累加起来，作为该点的新的像素值
    for (int i = x1; i <= x2; i++)
    {
        for (int j = y1; j <= y2; j++)
        {
            int x = i - x1;
            int y = j - y1;
            // 外圈上边框
            for (int n = 0; n < 11; n++)
            {
                glintValue[n] = (double)(cof[n][x][y] * src.at<uchar>(x1 - 1,y1 - 1 + n)); // 注意行列顺序
            }
            // 外圈左边框
            for (int n = 0; n < 9; n++)
            {
                glintValue[n + 11] = (double)(cof[n + 11][x][y] * src.at<uchar>(x1 + n,y1 - 1));
            }
            // 外圈右边框
            for (int n = 0; n < 9; n++)
            {
                glintValue[n + 20] = (double)(cof[n + 20][x][y] * src.at<uchar>(x1 + n,y2 + 1));
            }
            // 外圈下边框
            for (int n = 0; n < 11; n++)
            {
                glintValue[n + 29] = (double)(cof[n + 29][x][y] * src.at<uchar>(x2 + 1,y1 - 1 + n));
            }

            double sumValue = 0;
            for (int k = 0; k < 40; k++) // 循环变量名改为k，避免与外层i冲突
            {
                sumValue += glintValue[k];
            }
            dst.at<uchar>(j, i) = sumValue; // 注意行列顺序
        }
    }
}


std::tuple<double,double,int> calABD(double D0,double D60,double D120) {
    double sph, cyl;
    int ax;
    //计算A, B, D, S, C, A
    double LA, LB, LD;
    LA = (D0 + D60 + D120) / 3;
    LB = (D120 + D60 - 2 * D0) / 3;
    LD = (D60 - D120) / sqrt(3);

    if (LA < -6.0)
        sph = LA - sqrt(LB * LB + LD * LD);
    else
        sph = LA + sqrt(LB * LB + LD * LD);

    cyl = 2 * sqrt(LB * LB + LD * LD);

    if(cyl < 0)
        cyl = 0;
    if(cyl > 0.5)
        cyl -= 0.25;
    cyl = -cyl;

    if(LB < 0)
    {
        ax = 0.5 * atan(LD / LB) / M_PI * 180;
        if(ax < 0)
            ax += 180;
    } else if(LB == 0) {
        ax = 45;
    } else {
        ax = (0.5 * atan(LD / LB) / M_PI * 180) + 90;
    }
    ax = ax > 0 ? ax : ax + 180;

    return std::make_tuple(sph,cyl,ax);
}


/**
 * @brief 设置瞳孔图像的ROI区域
 * @param _img_pupil 原始瞳孔图像
 * @param _radius 瞳孔半径
 * @return 提取的ROI区域图像
 */
cv::Mat setPupilImgRoi(const cv::Mat& _img_pupil, double _radius)
{
    assert(_radius > 0);

    int width_half = static_cast<int>(_radius * 3 / 4);     // ROI 宽度的一半
    int height_half = static_cast<int>(width_half * 0.6);   // ROI 高度的一半

    int width = width_half * 2 + 1;         // ROI 宽度
    int height = height_half * 2 + 1;       // ROI 高度

    assert(_img_pupil.cols > width && _img_pupil.rows > height);

    /* 前面processPicOfOneEye的处理过程已确保瞳孔图像的中心就是瞳孔中心 */
    int X0 = _img_pupil.cols / 2, Y0 = _img_pupil.rows / 2;
    int x = X0 - width_half;
    int y = Y0 - height_half;

    cv::Rect rect_roi(x, y, width, height);

    // 返回ROI图像，不修改原图像
    return _img_pupil(rect_roi);
}


void getRefCurve(enAgeRange _age_range, bool isHmMode, double **_p1, double **_p2, double **_p3, double **_p4, int curve)
{
#if ENABLE_REFRACTION_QUALITY_LOG
    std::cout <<"isHmMode " << isHmMode << " age_range = "<< _age_range << " curve:" << curve <<std::endl;
#endif
    if (isHmMode)//高度数模式 用高度数曲线
    {
        *_p1 = hDC1;
        *_p2 = hDC2;
        *_p3 = hDC3;
        *_p4 = hDC4;
    }
    else
    {
        //按不同年龄段用不同曲线计算
        switch (_age_range)
        {
            case ageRange_0_06_12_MONTH:
                *_p1 = DC1_06_12_Mon;
                *_p2 = DC2_06_12_Mon;
                *_p3 = DC3_06_12_Mon;
                *_p4 = DC4_06_12_Mon;
                break;
            case ageRange_1_01_03_YEAR:
                *_p1 = DC1_12_36_Mon;
                *_p2 = DC2_12_36_Mon;
                *_p3 = DC3_12_36_Mon;
                *_p4 = DC4_12_36_Mon;
                break;
            case ageRange_2_03_06_YEAR:
                *_p1 = DC1_03_06_Year;
                *_p2 = DC2_03_06_Year;
                *_p3 = DC3_03_06_Year;
                *_p4 = DC4_03_06_Year;
                break;
            case ageRange_3_06_20_YEAR:
                *_p1 = DC1_06_20_Year;
                *_p2 = DC2_06_20_Year;
                *_p3 = DC3_06_20_Year;
                *_p4 = DC4_06_20_Year;
                break;
            case ageRange_4_20_100_YEAE:
                if(curve == 1)
                {
                    *_p1 = Low_DC1;
                    *_p2 = Low_DC2;
                    *_p3 = Low_DC3;
                    *_p4 = Low_DC4;
                }
                if(curve == 2)
                {
                    *_p1 = Mid_DC1;
                    *_p2 = Mid_DC2;
                    *_p3 = Mid_DC3;
                    *_p4 = Mid_DC4;
                }
                if(curve == 3)
                {
                    *_p1 = High_DC1;
                    *_p2 = High_DC2;
                    *_p3 = High_DC3;
                    *_p4 = High_DC4;
                }
                break;
            default:
                break;
        }
    }

//    drawEnhancedLineChart(*_p1, 441);
//    drawEnhancedLineChart(*_p2, 441);
//    drawEnhancedLineChart(*_p3, 441);
//    drawEnhancedLineChart(*_p4, 441);
}

// 根据年龄、是否高度数模式返回屈光-斜率4条曲线
//void CAlgo::getRefCurve(enAgeRange _age_range, double **_p1, double **_p2, double **_p3, double **_p4)
//{
//    // 检查输出参数是否有效
//    if (_p1 == nullptr || _p2 == nullptr || _p3 == nullptr || _p4 == nullptr) {
//        std::cerr << "Error: Output pointers are null" << std::endl;
//        return;
//    }

//    std::cout << "isHmMode " << isHmMode << " age_range = " << _age_range << " curve:" << getCurve() << std::endl;

//    // 按不同年龄段用不同曲线计算
//    const double *dc1, *dc2, *dc3, *dc4;

//    switch (_age_range) {
//        case ageRange_0_06_12_MONTH:
//            dc1 = DC1_06_12_Mon;
//            dc2 = DC2_06_12_Mon;
//            dc3 = DC3_06_12_Mon;
//            dc4 = DC4_06_12_Mon;
//            break;
//        case ageRange_1_01_03_YEAR:
//            dc1 = DC1_12_36_Mon;
//            dc2 = DC2_12_36_Mon;
//            dc3 = DC3_12_36_Mon;
//            dc4 = DC4_12_36_Mon;
//            break;
//        case ageRange_2_03_06_YEAR:
//            dc1 = DC1_03_06_Year;
//            dc2 = DC2_03_06_Year;
//            dc3 = DC3_03_06_Year;
//            dc4 = DC4_03_06_Year;
//            break;
//        case ageRange_3_06_20_YEAR:
//            dc1 = DC1_06_20_Year;
//            dc2 = DC2_06_20_Year;
//            dc3 = DC3_06_20_Year;
//            dc4 = DC4_06_20_Year;
//            break;
//        case ageRange_4_20_100_YEAE:

//        default:
//            dc1 = Mid_DC1;
//            dc2 = Mid_DC2;
//            dc3 = Mid_DC3;
//            dc4 = Mid_DC4;
//            break;
//    }

//    // 为输出参数分配内存（假设调用者会负责释放）
//    const size_t result_size = 400;

//    *_p1 = new double[result_size];
//    *_p2 = new double[result_size];
//    *_p3 = new double[result_size];
//    *_p4 = new double[result_size];

//    // 执行拼接操作
//    concatenate_arrays_to_array(hDC1, 441, 20, 120,
//                               dc1, 321, 60, 260,
//                               hDC1, 441, 320, 420,
//                               *_p1, result_size);

//    concatenate_arrays_to_array(hDC2, 441, 20, 120,
//                               dc2, 321, 60, 260,
//                               hDC2, 441, 320, 420,
//                               *_p2, result_size);

//    concatenate_arrays_to_array(hDC3, 441, 20, 120,
//                               dc3, 321, 60, 260,
//                               hDC3, 441, 320, 420,
//                               *_p3, result_size);

//    concatenate_arrays_to_array(hDC4, 441, 20, 120,
//                               dc4, 321, 60, 260,
//                               hDC4, 441, 320, 420,
//                               *_p4, result_size);

////    drawEnhancedLineChart(*_p1, result_size);
////    drawEnhancedLineChart(*_p2, result_size);
////    drawEnhancedLineChart(*_p3, result_size);
////    drawEnhancedLineChart(*_p4, result_size);
//}

// 主函数 - 基于RANSAC的compareEach
double compareEach(cv::Mat& img1, cv::Mat& img2, const RANSACParams& params) {
#if ENABLE_ALGO_TIMING_LOG
    ALGO_TIMING_SCOPE(AlgoTimingStage_CompareEach);
#endif
    // 输入检查
    if (img1.empty() || img2.empty()) {
        qDebug() << "CAlgo::compareEach(): input images are empty";
        return 0.0;
    }

    if (img1.size() != img2.size()) {
        qDebug() << "CAlgo::compareEach(): image sizes don't match";
        return 0.0;
    }

    // 转换为灰度图
    cv::Mat gray1, gray2;
    if (img1.channels() > 1) {
        cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
    } else {
        gray1 = img1;
    }

    if (img2.channels() > 1) {
        cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);
    } else {
        gray2 = img2;
    }

    // 计算每列的平均比值
    std::vector<double> pointvector;
    std::vector<int> locatevector;

    int width = gray1.cols;
    int height = gray1.rows;

    int zeroDivisorCount = 0;
    int maxValueCount = 0;

    for (int col = 0; col < width; ++col) {
        double sum = 0.0;
        int validPixelCount = 0;

        for (int row = 0; row < height; ++row) {
            double val1 = static_cast<double>(gray1.at<uchar>(row, col));
            double val2 = static_cast<double>(gray2.at<uchar>(row, col));

            // 处理除数为0或过小的情况
            if (std::abs(val2) < DIVISOR_MIN) {
                val2 = DIVISOR_MIN;
                ++zeroDivisorCount;
            }

            double ratio = val1 / val2;

            // 检查是否是最大值（255）
            if (std::abs(ratio - 255.0) < EPSILON) {
                ++maxValueCount;
                continue;
            }

            sum += ratio;
            ++validPixelCount;
        }

        if (validPixelCount > 0) {
            pointvector.push_back(sum / validPixelCount);
            locatevector.push_back(col - KROIW);
        }
    }

    // 记录特殊情况
    if (zeroDivisorCount > 0) {
        qDebug() << (QString("CAlgo::compareEach(): zero divisors: %1").arg(zeroDivisorCount));
    }
    if (maxValueCount > 0) {
        qDebug() << (QString("CAlgo::compareEach(): max values: %1").arg(maxValueCount));
    }

    // 检查有效数据点
    if (pointvector.size() < 2) {
        qDebug() << "CAlgo::compareEach(): insufficient valid points for fitting";
        return 0.0;
    }

    // 准备RANSAC输入数据
    std::vector<double> xData, yData;
    for (size_t i = 0; i < pointvector.size(); ++i) {
        xData.push_back(static_cast<double>(locatevector[i]));
        yData.push_back(pointvector[i]);
    }

    // 使用RANSAC拟合直线
    std::vector<bool> inlierMask;
    LineModel bestModel = fitLineRANSAC(xData, yData, params, inlierMask);

    // 统计内点信息（用于调试）
    int inlierCount = std::count(inlierMask.begin(), inlierMask.end(), true);
    double inlierRatio = static_cast<double>(inlierCount) / inlierMask.size();

    PERF_POINT(QString("RANSAC fitting result: slope = %1 , inlier ratio = %2 ( %3 / %4 )").arg(bestModel.slope).arg(inlierRatio).arg(inlierCount).arg(inlierMask.size()));

    // 返回斜率（应用比例系数）
    return bestModel.slope * PIX_COEF;
}

double compareEach(cv::Mat& img1, cv::Mat& img2)
{
    // 输入检查
    if (img1.empty() || img2.empty()) {
        qDebug() << "CAlgo::compareEach(): input images are empty";
        return 0.0;
    }

    // 尺寸检查
    if (img1.size() != img2.size()) {
        qDebug() << "CAlgo::compareEach(): image sizes don't match";
        return 0.0;
    }

    // 转换为灰度图（如果还不是）
    cv::Mat gray1, gray2;
    if (img1.channels() > 1) {
        cv::cvtColor(img1, gray1, cv::COLOR_BGR2GRAY);
    } else {
        gray1 = img1.clone();
    }

    if (img2.channels() > 1) {
        cv::cvtColor(img2, gray2, cv::COLOR_BGR2GRAY);
    } else {
        gray2 = img2.clone();
    }

    // 计算每列的平均比值
    std::vector<double> pointvector;
    std::vector<int> locatevector;

    int width = gray1.cols;
    int height = gray1.rows;

    int zeroDivisorCount = 0;
    int maxValueCount = 0;

    for (int col = 0; col < width; ++col) {
        double sum = 0.0;
        int validPixelCount = 0;

        for (int row = 0; row < height; ++row) {
            double val1 = gray1.at<uchar>(row, col);
            double val2 = gray2.at<uchar>(row, col);

            // 处理除数为0或过小的情况
            if (std::abs(val2) < DIVISOR_MIN) {
                val2 = DIVISOR_MIN;
                ++zeroDivisorCount;
            }

            double ratio = val1 / val2;

            // 检查是否是最大值（255）
            if (std::abs(ratio - 255.0) < EPSILON) {
                ++maxValueCount;
                continue;
            }

            sum += ratio;
            ++validPixelCount;
        }

        if (validPixelCount > 0) {
            pointvector.push_back(sum / validPixelCount);
            locatevector.push_back(col - KROIW); // 假设KROIW已定义
        }
    }

    // 记录特殊情况
    if (zeroDivisorCount > 0) {
        qDebug() << (QString(__PRETTY_FUNCTION__) + " zero divisors: %1").arg(zeroDivisorCount);
    }
    if (maxValueCount > 0) {
        qDebug() << (QString(__PRETTY_FUNCTION__) + " max values: %1").arg(maxValueCount);
    }

    // 最小二乘法计算斜率
    if (pointvector.empty()) {
        qDebug() << "CAlgo::compareEach(): no valid points for fitting";
        return 0.0;
    }

    double xSum = 0.0, ySum = 0.0, xySum = 0.0, xxSum = 0.0;
    size_t pointCount = pointvector.size();

    for (size_t i = 0; i < pointCount; ++i) {
        double x = locatevector[i];
        double y = pointvector[i];

        xSum += x;
        ySum += y;
        xySum += x * y;
        xxSum += x * x;
    }

    double denominator = pointCount * xxSum - xSum * xSum;
    if (std::abs(denominator) < EPSILON) {
        qDebug() << "CAlgo::compareEach(): denominator is zero";
        return 0.0;
    }

    double k = (pointCount * xySum - xSum * ySum) / denominator;

    // 应用比例系数
    return k * PIX_COEF; // 假设PIX_COEF已定义
}


/**
 * @brief 斜率结果输出函数
 *
 * 统一格式化输出左右眼的斜率计算结果，确保输出格式一致
 *
 * @param eyeSide 眼睛标识："right"或"left"，用于生成正确的标题
 * @param results 斜率结果数组，长度为10
 * @param configs 斜率配置数组，用于获取每个斜率的描述信息
 * @param standardIndices 标准斜率索引，用于生成标准斜率的描述
 */
void printSlopeResults(const std::string& eyeSide,
                             double* results,
                             const std::vector<SlopeCalculationConfig>& configs,
                             const std::vector<int>& standardIndices)
{
    // 输出标题行
    std::cout << "\n=== Slope List(" << eyeSide << " eye) ===" << std::endl;

    // 输出前9个普通斜率结果
    for (size_t i = 0; i < configs.size() && i < 9; ++i) {
        std::cout << "DS" << (eyeSide == "right" ? "R" : "L")  // 根据眼睛选择前缀：DSR或DSL
                  << "[" << i << "]" << configs[i].description  // 输出索引和描述
                  << " = " << std::fixed << std::setprecision(6) << results[i] << std::endl;
    }

    // 输出第10个标准斜率结果
    std::cout << "DS" << (eyeSide == "right" ? "R" : "L") << "[9]";

    // 根据眼睛类型生成不同的标准斜率描述
    if (eyeSide == "right") {
        std::cout << "([19/20] - [22/21]) / 2";
    } else {
        std::cout << "([21/22] - [20/19]) / 2";
    }

    std::cout << " = " << std::fixed << std::setprecision(6) << results[9] << std::endl;
    std::cout << "=================================" << std::endl;
}

std::tuple<double,double,double> calRef(enAgeRange _age_range,bool isHmMode, double *DS) {
    double *dc1,*dc2,*dc3,*dc4;
    getRefCurve(_age_range, isHmMode, &dc1, &dc2, &dc3, &dc4);
    int mLength;
    double mFix;
    double step;
    //mLength从441、321改成440、320，使得step为0.05，两者最后计算结果最多有0.25的降低
    if (isHmMode)
    {
        mLength = 440;
        mFix = 11.0;

    } else {
        mLength = 320;
        mFix = 8.0;
    }
    //屈光斜率曲线里每个屈光点的步长
    step=2*mFix/mLength;

    int n1_0,n1_60,n1_120,n2_0,n2_60,n2_120,n3_0,n3_60,n3_120;
    double D1_0,D1_60,D1_120,D2_0,D2_60,D2_120,D3_0,D3_60,D3_120;
    double sph_1,cyl_1,ax_1,sph_2,cyl_2,ax_2,sph_3,cyl_3,ax_3;
    n1_0=findClosestIndex(dc1,dc4,mLength,DS[0],DS[9]);
    n1_60=findClosestIndex(dc1,dc4,mLength,DS[3],DS[9]);
    n1_120=findClosestIndex(dc1,dc4,mLength,DS[6],DS[9]);
    D1_0 = n1_0 * step - mFix ;
    D1_60 = n1_60 * step - mFix;
    D1_120 = n1_120 * step - mFix;

    std::tie(sph_1,cyl_1,ax_1)=calABD(D1_0,D1_60,D1_120);
    std::cout << "D1_0 = " << D1_0 << "; D1_60 = " << D1_60 << "; D1_120 = " << D1_120 << std::endl;
    std::cout << "sph_1 = " << sph_1 << "; cyl_1 = " << cyl_1 << "; ax_1 = " << ax_1 << std::endl;

    n2_0=findClosestIndex(dc2,dc4,mLength,DS[1],DS[9]);
    n2_60=findClosestIndex(dc2,dc4,mLength,DS[4],DS[9]);
    n2_120=findClosestIndex(dc2,dc4,mLength,DS[7],DS[9]);
    D2_0 = n2_0 * step - mFix ;
    D2_60 = n2_60 * step - mFix;
    D2_120 = n2_120 * step - mFix;

    std::tie(sph_2,cyl_2,ax_2)=calABD(D2_0,D2_60,D2_120);
    std::cout << "D2_0 = " << D2_0 << "; D2_60 = " << D2_60 << "; D2_120 = " << D2_120 << std::endl;
    std::cout << "sph_2 = " << sph_2 << "; cyl_2 = " << cyl_2 << "; ax_2 = " << ax_2 << std::endl;

    n3_0=findClosestIndex(dc3,dc4,mLength,DS[2],DS[9]);
    n3_60=findClosestIndex(dc3,dc4,mLength,DS[5],DS[9]);
    n3_120=findClosestIndex(dc3,dc4,mLength,DS[8],DS[9]);
    D3_0 = n3_0 * step - mFix ;
    D3_60 = n3_60 * step - mFix;
    D3_120 = n3_120 * step - mFix;

    std::tie(sph_3,cyl_3,ax_3)=calABD(D3_0,D3_60,D3_120);
    std::cout << "D3_0 = " << D3_0 << "; D3_60 = " << D3_60 << "; D3_120 = " << D3_120 << std::endl;
    std::cout << "sph_3 = " << sph_3 << "; cyl_3 = " << cyl_3 << "; ax_3 = " << ax_3 << std::endl;

}

std::tuple<double,double,double> calRefraction(enAgeRange _age_range,
                                               bool isHmMode,
                                               const double *DS,
                                               RefractionFitDiagnostics *diagnostics) {
    int mLength;
    double mFix;
    double step;
    //mLength从441、321改成440、320，使得step为0.05，两者最后计算结果最多有0.25的降低
    if (isHmMode)
    {
        mLength = 440;
        mFix = 11.0;

    } else {
        mLength = 320;
        mFix = 8.0;
    }
    //屈光斜率曲线里每个屈光点的步长
    step=2*mFix/mLength;

    double *p1,*p2,*p3,*p4;
    getRefCurve(_age_range, isHmMode, &p1, &p2, &p3, &p4);

    // 用来计算 aggregrate sum, 找到 r0 r60 r120，从来计算 A, B ,D
    double Y0[441] = { 0 };
    double Y60[441] = { 0 };
    double Y120[441] = { 0 };

    double dc1, dc2, dc3, dc4;
    for (int m = 0; m < mLength; m++)
    {
        //需要知道cal-curve 上， m对应的Slope值是多少
        dc1 = p1[m];
        dc2 = p2[m];
        dc3 = p3[m];
        dc4 = p4[m];

        Y0[m] = (DS[0] - dc1) * (DS[0] - dc1) + (DS[1] - dc2) * (DS[1] - dc2) + (DS[2] - dc3) * (DS[2] - dc3) + (DS[9] - dc4) * (DS[9] - dc4);
        Y60[m] = (DS[3] - dc1) * (DS[3] - dc1) + (DS[4] - dc2) * (DS[4] - dc2) + (DS[5] - dc3) * (DS[5] - dc3) + (DS[9] - dc4) * (DS[9] - dc4);
        Y120[m] = (DS[6] - dc1) * (DS[6] - dc1) + (DS[7] - dc2) * (DS[7] - dc2) + (DS[8] - dc3) * (DS[8] - dc3) + (DS[9] - dc4) * (DS[9] - dc4);
    }

    double min0 = Y0[0], min60 = Y60[0], min120 = Y120[0];
    //求最小的aggregrate sum对应的m值,用nL/R表示
    int n0 = 0, n60 = 0, n120 = 0;

    double *min_ptr;
    min_ptr = std::min_element(Y0, Y0 + mLength);
    min0 = *min_ptr;
    n0 = min_ptr - Y0;  // 计算索引（指针偏移）

    min_ptr = std::min_element(Y60, Y60 + mLength);
    min60 = *min_ptr;
    n60 = min_ptr - Y60;  // 计算索引（指针偏移）

    min_ptr = std::min_element(Y120, Y120 + mLength);
    min120 = *min_ptr;
    n120 = min_ptr - Y120;  // 计算索引（指针偏移）

    double D0, D60, D120;
    D0 = n0 * step - mFix ;
    D60 = n60 * step - mFix;
    D120 = n120 * step - mFix;

    if (diagnostics != nullptr) {
        const double *curves[3] = {Y0, Y60, Y120};
        const int bestIndices[3] = {n0, n60, n120};
        const double minErrors[3] = {min0, min60, min120};
        const double diopters[3] = {D0, D60, D120};
        const double measuredDs[3][4] = {
            {DS[0], DS[1], DS[2], DS[9]},
            {DS[3], DS[4], DS[5], DS[9]},
            {DS[6], DS[7], DS[8], DS[9]}
        };

        diagnostics->totalMinError = 0.0;
        diagnostics->worstMinError = 0.0;
        diagnostics->worstDirection = -1;

        for (int direction = 0; direction < 3; ++direction) {
            const int best = bestIndices[direction];
            const double bestError = minErrors[direction];
            const int leftNeighbor = std::max(0, best - 1);
            const int rightNeighbor = std::min(mLength - 1, best + 1);

            // 局部尖锐度：最低点左右相邻误差均值相对最低误差的增量。
            const double neighborMean =
                    (curves[direction][leftNeighbor] + curves[direction][rightNeighbor]) / 2.0;

            // 次优间隔：排除最低点前后 0.25D，寻找其余区间的最小误差。
            const int exclusionRadius = std::max(1, static_cast<int>(std::round(0.25 / step)));
            double alternativeError = std::numeric_limits<double>::max();
            for (int m = 0; m < mLength; ++m) {
                if (std::abs(m - best) <= exclusionRadius) {
                    continue;
                }
                alternativeError = std::min(alternativeError, curves[direction][m]);
            }

            diagnostics->diopter[direction] = diopters[direction];
            diagnostics->minError[direction] = bestError;
            diagnostics->rmsError[direction] = std::sqrt(bestError / 4.0);
            diagnostics->localSharpness[direction] = std::max(0.0, neighborMean - bestError);
            diagnostics->alternativeGap[direction] =
                    alternativeError == std::numeric_limits<double>::max()
                    ? 0.0 : std::max(0.0, alternativeError - bestError);
            diagnostics->bestIndex[direction] = best;

            // 保留有符号残差，便于判断具体是哪一个 DS 通道偏高或偏低。
            const double fittedValues[4] = {p1[best], p2[best], p3[best], p4[best]};
            for (int channel = 0; channel < 4; ++channel) {
                diagnostics->signedResidual[direction][channel] =
                        measuredDs[direction][channel] - fittedValues[channel];
            }

            diagnostics->totalMinError += bestError;
            if (diagnostics->worstDirection < 0 || bestError > diagnostics->worstMinError) {
                diagnostics->worstMinError = bestError;
                diagnostics->worstDirection = direction == 0 ? 0 : (direction == 1 ? 60 : 120);
            }
        }
    }

    return std::make_tuple(D0,D60,D120);
}

/**
 * @brief 计算瞳孔平均映光点并检查眼位偏差
 * @param _single_dual_eye 单双眼模式
 * @param pupilInfoRight 右眼瞳孔信息映射
 * @param pupilInfoLeft 左眼瞳孔信息映射
 * @return 计算状态：calcResultState_GazeOver（眼位偏差超限）或 calcResultState_Succ（成功）
 */
enCalcResultState calculatePupilAverageSpot(
    enSingleDualEyeMode _single_dual_eye,
    int maxGazeDeviation,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoRight,
    const std::bitset<FRAME_ARRAY_SIZE>& validRight,
    CPointF& pupilSpotAvgRight,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoLeft,
    const std::bitset<FRAME_ARRAY_SIZE>& validLeft,
    CPointF& pupilSpotAvgLeft)
{
    auto eye_flags = get_eye_flags(_single_dual_eye);

    if (eye_flags.first && validRight.any()) {
        CPointF sum_spot = {0.0, 0.0};
        int valid_count = 0;
        for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
            if (!validRight.test(i)) continue;
            sum_spot.x += pupilInfoRight[i].dx;
            sum_spot.y += pupilInfoRight[i].dy;
            valid_count++;
        }
        if (valid_count == 0) return calcResultState_Fail;

        pupilSpotAvgRight.x = sum_spot.x / valid_count;
        pupilSpotAvgRight.y = sum_spot.y / valid_count;

        // 检查眼位偏差...
        double rx_deg = pupilSpotAvgRight.x * PIXEL_TO_PHY * 7;
        double ry_deg = pupilSpotAvgRight.y * PIXEL_TO_PHY * 7;
        int gaze_deviation_r = std::ceil(std::sqrt(std::pow(rx_deg, 2) + std::pow(ry_deg, 2)));

        if (gaze_deviation_r > maxGazeDeviation) {
            return calcResultState_GazeOver;
        }
    } else {
        pupilSpotAvgRight = {0.0, 0.0};
    }

    // 左眼同理...
    if (eye_flags.second && validLeft.any()) {
        CPointF sum_spot = {0.0, 0.0};
        int valid_count = 0;
        for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
            if (!validLeft.test(i)) continue;
            sum_spot.x += pupilInfoLeft[i].dx;
            sum_spot.y += pupilInfoLeft[i].dy;
            valid_count++;
        }
        if (valid_count == 0) return calcResultState_Fail;

        pupilSpotAvgLeft.x = sum_spot.x / valid_count;
        pupilSpotAvgLeft.y = sum_spot.y / valid_count;

        double lx_deg = pupilSpotAvgLeft.x * PIXEL_TO_PHY * 7;
        double ly_deg = pupilSpotAvgLeft.y * PIXEL_TO_PHY * 7;
        int gaze_deviation_l = std::ceil(std::sqrt(std::pow(lx_deg, 2) + std::pow(ly_deg, 2)));

        if (gaze_deviation_l > maxGazeDeviation) {
            return calcResultState_GazeOver;
        }
    } else {
        pupilSpotAvgLeft = {0.0, 0.0};
    }

    return calcResultState_Succ;
}

// 计算瞳孔平均半径
void calculatePupilAverageRadius(
    enSingleDualEyeMode _single_dual_eye,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoRight,
    const std::bitset<FRAME_ARRAY_SIZE>& validRight,
    double& avgR,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoLeft,
    const std::bitset<FRAME_ARRAY_SIZE>& validLeft,
    double& avgL)
{
    auto eye_flags = get_eye_flags(_single_dual_eye);

    // 计算右眼平均半径
    if (eye_flags.first) {
        double sum_diameter = 0.0;
        int valid_count = 0;
        for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
            if (!validRight.test(i)) continue;
            sum_diameter += pupilInfoRight[i].radius;
            valid_count++;
        }
        avgR = valid_count > 0 ? sum_diameter / valid_count : 0.0;
    } else {
        avgR = 0.0;
    }

    // 计算左眼平均半径
    if (eye_flags.second) {
        double sum_diameter = 0.0;
        int valid_count = 0;
        for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
            if (!validLeft.test(i)) continue;
            sum_diameter += pupilInfoLeft[i].radius;
            valid_count++;
        }
        avgL = valid_count > 0 ? sum_diameter / valid_count : 0.0;
    } else {
        avgL = 0.0;
    }
}

/* 计算本次测量左右眼平均瞳距（单位：像素） */
double calcPupilDistance(
    enSingleDualEyeMode _single_dual_eye,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoRight,
    const std::bitset<FRAME_ARRAY_SIZE>& validRight,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoLeft,
    const std::bitset<FRAME_ARRAY_SIZE>& validLeft)
{
    auto eye_flags = get_eye_flags(_single_dual_eye);
    if (!eye_flags.first || !eye_flags.second) return 0.0;

    double sum = 0.0;
    int valid = 0;

    for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
        if (!validRight.test(i) || !validLeft.test(i)) continue;

        const stPupilInfo& r = pupilInfoRight[i];
        const stPupilInfo& l = pupilInfoLeft[i];

        sum += std::hypot(r.center.x - l.center.x, r.center.y - l.center.y);
        ++valid;
    }

    return valid > 0 ? sum / valid : 0.0;
}


bool processPicOfOneEye(const cv::Mat& img, int _img_idx, stPupilInfo &_pupil_info, int _angle, enWhichEye _which_eye,cv::Mat& out_processedImg)
{
    PERF_SCOPE("processPicOfOneEye");

    //中心+0.5像素再取整
    cv::Point center_int(std::round(_pupil_info.center.x + 0.5f), std::round(_pupil_info.center.y + 0.5f));
#if ENABLE_PREVIEW_VERBOSE_LOG
    qDebug()<<QString("center_int.x=%1,center_int.y=%2").arg(center_int.x).arg(center_int.y);
#endif
    if (!isNormalPupil(center_int, _which_eye))
    {
        if (shouldLogPupilFailDetail()) {
            qDebug().noquote() << QString("PupilFailDetail: img=%1,eye=%2,stage=processPicOfOneEye,reason=pupil_roi_out_of_range,center=(%3,%4),raw_center=(%5,%6),radius=%7")
                        .arg(_img_idx)
                        .arg(_which_eye == whichEye_Right ? "right" : "left")
                        .arg(center_int.x)
                        .arg(center_int.y)
                        .arg(_pupil_info.center.x, 0, 'f', 1)
                        .arg(_pupil_info.center.y, 0, 'f', 1)
                        .arg(_pupil_info.radius, 0, 'f', 1);
        }
        return false;
    }
    cv::Mat img_pupil_origin,img_pupil_rotated;
    cv::Rect rect;
    try {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_RoiCropRotate);
#endif
        //isNormalPupil已经把瞳孔中心限制在合法边界
        rect=cv::Rect(center_int.x - ROI_WIDTH_HALF, center_int.y - ROI_HEIGHT_HALF, ROI_WIDTH, ROI_HEIGHT);
        img_pupil_origin = img(rect);

        // ----------- 旋转（保持 ROI 尺寸不变）----------
        rotateImage(img_pupil_origin,img_pupil_rotated,_angle);
    } catch (const cv::Exception& e) {
        // 异常与普通定位失败共用同一个限流入口；一项异常只输出一条完整日志，
        // 避免尺寸、矩形和异常文本拆成多条后绕过整轮预算。
        if (shouldLogPupilFailDetail()) {
            qWarning().noquote() << QString("PupilFailDetail: img=%1,eye=%2,stage=processPicOfOneEye,reason=roi_crop_or_rotate_exception,image=(%3,%4),center=(%5,%6),radius=%7,roi=(%8,%9,%10,%11),exception=%12")
                        .arg(_img_idx)
                        .arg(_which_eye == whichEye_Right ? "right" : "left")
                        .arg(img.cols)
                        .arg(img.rows)
                        .arg(center_int.x)
                        .arg(center_int.y)
                        .arg(_pupil_info.radius, 0, 'f', 1)
                        .arg(rect.x)
                        .arg(rect.y)
                        .arg(rect.width)
                        .arg(rect.height)
                        .arg(e.what());
        }
        return false;
    } catch (...) {
        if (shouldLogPupilFailDetail()) {
            qWarning().noquote() << QString("PupilFailDetail: img=%1,eye=%2,stage=processPicOfOneEye,reason=roi_crop_or_rotate_unknown_exception,image=(%3,%4),center=(%5,%6),radius=%7,roi=(%8,%9,%10,%11)")
                        .arg(_img_idx)
                        .arg(_which_eye == whichEye_Right ? "right" : "left")
                        .arg(img.cols)
                        .arg(img.rows)
                        .arg(center_int.x)
                        .arg(center_int.y)
                        .arg(_pupil_info.radius, 0, 'f', 1)
                        .arg(rect.x)
                        .arg(rect.y)
                        .arg(rect.width)
                        .arg(rect.height);
        }
        return false;
    }
    // ----------- Mask ----------
    cv::Mat img_pupil;
    {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_MaskCopy);
#endif
        int PADDING=20;
        cv::Mat mask = cv::Mat::zeros(img_pupil_rotated.size(), CV_8UC1);
        cv::circle(mask, cv::Point(ROI_WIDTH_HALF, ROI_HEIGHT_HALF), (int)_pupil_info.radius + PADDING, 255, -1);
        img_pupil_rotated.copyTo(img_pupil, mask);
    }

    // ----------- 亮点检测 ----------
    Point glint;
#if ENABLE_ALGO_TIMING_LOG
    ALGO_TIMING_START(glint_process);
#endif
    cv::Mat img_pupil_glint_cleaned = img_pupil.clone();
    double glintMaxValue = 0.0;
    cv::minMaxLoc(img_pupil, nullptr, &glintMaxValue, nullptr, &glint);
    //限制映光点在范围内
    if (glint.x>=10 && glint.x<=ROI_WIDTH-10 && glint.y>=10 && glint.y<=ROI_HEIGHT-10)
    {
        // ----------- 去除高亮点 ----------
        reduceGlintBlob(img_pupil,glint,img_pupil_glint_cleaned);
        // 将结果通过输出参数返回
        out_processedImg = img_pupil_glint_cleaned; // 使用clone确保数据独立

        _pupil_info.dx=(float)(glint.x-ROI_WIDTH_HALF);
        _pupil_info.dy=(float)(glint.y-ROI_HEIGHT_HALF);

    } else {
        cv::Mat brightMask;
        cv::threshold(img_pupil, brightMask, 180, 255, cv::THRESH_BINARY);
        const int brightCount = cv::countNonZero(brightMask);
        cv::Mat saturatedMask;
        cv::threshold(img_pupil, saturatedMask, 245, 255, cv::THRESH_BINARY);
        const int saturatedCount = cv::countNonZero(saturatedMask);
        const int roiArea = std::max(1, img_pupil.rows * img_pupil.cols);
        if (shouldLogPupilFailDetail()) {
            qDebug().noquote() << QString("PupilFailDetail: img=%1,eye=%2,stage=processPicOfOneEye,reason=glint_out_of_roi,center=(%3,%4),raw_center=(%5,%6),radius=%7,roi=(%8,%9,%10,%11),angle=%12,glint=(%13,%14),glint_max=%15,bright_count=%16,bright_ratio=%17,saturated_count=%18,saturated_ratio=%19")
                        .arg(_img_idx)
                        .arg(_which_eye == whichEye_Right ? "right" : "left")
                        .arg(center_int.x)
                        .arg(center_int.y)
                        .arg(_pupil_info.center.x, 0, 'f', 1)
                        .arg(_pupil_info.center.y, 0, 'f', 1)
                        .arg(_pupil_info.radius, 0, 'f', 1)
                        .arg(rect.x)
                        .arg(rect.y)
                        .arg(rect.width)
                        .arg(rect.height)
                        .arg(_angle)
                        .arg(glint.x)
                        .arg(glint.y)
                        .arg(glintMaxValue, 0, 'f', 1)
                        .arg(brightCount)
                        .arg(static_cast<double>(brightCount) / roiArea, 0, 'f', 4)
                        .arg(saturatedCount)
                        .arg(static_cast<double>(saturatedCount) / roiArea, 0, 'f', 4);
        }
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_END(glint_process, AlgoTimingStage_GlintProcess);
#endif
        return false;
    }
#if ENABLE_PREVIEW_VERBOSE_LOG
    qDebug()<<QString("processPicOfOneEye imgNo %1,glint=(%2,%3)").arg(_img_idx).arg(glint.x).arg(glint.y)<<endl;
#endif

    Q_ASSERT(!out_processedImg.empty()||out_processedImg.cols!=ROI_HEIGHT||out_processedImg.rows!=ROI_WIDTH);
#if ENABLE_ALGO_TIMING_LOG
    ALGO_TIMING_END(glint_process, AlgoTimingStage_GlintProcess);
#endif
    return true;
}

//画折线图
void drawEnhancedLineChart(const double* data, int dataSize,
                          const QString& windowName = "Line Chart",
                          int width = 1000,
                          int height = 600) {
    // 创建图像
    Mat chart = Mat::zeros(height, width, CV_8UC3);
    chart = Scalar(245, 245, 245); // 浅灰色背景

    // 设置边距
    int margin = 80;
    int chartWidth = width - 2 * margin;
    int chartHeight = height - 2 * margin;

    // 数据范围
    double minVal = data[0];
    double maxVal = data[0];
    for (int i = 1; i < dataSize; i++) {
        if (data[i] < minVal) minVal = data[i];
        if (data[i] > maxVal) maxVal = data[i];
    }

    double range = maxVal - minVal;

    // 防止除零
    if (range == 0) {
        range = 1;
        maxVal = minVal + 1;
    }

    // 绘制网格
    for (int i = 0; i <= 10; i++) {
        int y = height - margin - (i * chartHeight / 10);
        line(chart, Point(margin, y), Point(width - margin, y),
             Scalar(200, 200, 200), 1);

        // Y轴标签（使用QString进行数值格式化）
        double value = minVal + (i * range / 10);
        QString label = QString::number(value, 'f', 2);
        putText(chart, label.toStdString(), Point(margin - 60, y + 5),
                FONT_HERSHEY_SIMPLEX, 0.4, Scalar(100, 100, 100), 1);
    }

    // 绘制坐标轴
    line(chart, Point(margin, margin), Point(margin, height - margin), Scalar(0, 0, 0), 2);
    line(chart, Point(margin, height - margin), Point(width - margin, height - margin), Scalar(0, 0, 0), 2);

    // 绘制数据点和折线
    std::vector<Point> points;
    for (int i = 0; i < dataSize; i++) {
        int x = margin + (i * chartWidth) / max(1, dataSize - 1);
        int y = height - margin - ((data[i] - minVal) * chartHeight / range);
        points.push_back(Point(x, y));

        // 绘制数据点
        circle(chart, Point(x, y), 4, Scalar(0, 100, 255), -1);

        // X轴标签（使用QString）
        if (i % (max(1, dataSize / 10)) == 0) {
            QString xLabel = QString::number(i);
            putText(chart, xLabel.toStdString(), Point(x, height - margin + 20),
                    FONT_HERSHEY_SIMPLEX, 0.4, Scalar(100, 100, 100), 1);
        }
    }

    // 绘制折线
    for (int i = 0; i < points.size() - 1; i++) {
        line(chart, points[i], points[i + 1], Scalar(0, 0, 255), 2);
    }

    // 添加标题
    putText(chart, windowName.toStdString(), Point(width/2 - 100, 30),
            FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 0, 0), 2);

    // 显示图像
    imshow(windowName.toStdString(), chart);
    waitKey(0);
}

// 拼接函数 - 返回静态数组（需要调用者知道大小）
void concatenate_arrays_to_array(
    const double* arr1, size_t arr1_size, size_t start1, size_t end1,
    const double* arr2, size_t arr2_size, size_t start2, size_t end2,
    const double* arr3, size_t arr3_size, size_t start3, size_t end3,
    double* result, size_t result_size) {

    size_t current_index = 0;

    // 第一部分：arr1[start1:end1]
    if (start1 < arr1_size && end1 <= arr1_size && start1 < end1) {
        size_t copy_size = end1 - start1;
        if (current_index + copy_size <= result_size) {
            std::copy(arr1 + start1, arr1 + end1, result + current_index);
            current_index += copy_size;
        } else {
            std::cerr << "Warning: Result array too small for part1" << std::endl;
        }
    }

    // 第二部分：arr2[start2:end2]
    if (start2 < arr2_size && end2 <= arr2_size && start2 < end2) {
        size_t copy_size = end2 - start2;
        if (current_index + copy_size <= result_size) {
            std::copy(arr2 + start2, arr2 + end2, result + current_index);
            current_index += copy_size;
        } else {
            std::cerr << "Warning: Result array too small for part2" << std::endl;
        }
    }

    // 第三部分：arr3[start3:end3]
    if (start3 < arr3_size && end3 <= arr3_size && start3 < end3) {
        size_t copy_size = end3 - start3;
        if (current_index + copy_size <= result_size) {
            std::copy(arr3 + start3, arr3 + end3, result + current_index);
        } else {
            std::cerr << "Warning: Result array too small for part3" << std::endl;
        }
    }

}

// 计算数组的最小值索引（argmin）
template <typename T, size_t N>
size_t argmin(const T (&arr)[N]) {
    size_t min_index = 0;
    for (size_t i = 1; i < N; ++i) {
        if (arr[i] < arr[min_index]) {
            min_index = i;
        }
    }
    return min_index;
}

// 新增辅助函数：统计数组中非空图像的数量
int getValidImageCount(const std::map<int, cv::Mat>& imageMap, int expected_count)
{
    int count = 0;
    for (const auto& pair : imageMap) {
        if (!pair.second.empty()) {
            count++;
        }
        // 如果已经达到或超过期望数量，可以提前退出
        if (count >= expected_count) {
            break;
        }
    }
    return count;
}


// 查找数组中与目标值平方差最小的元素索引
int findClosestIndex(const double* array1, const double* array2, int size, double target1, double target2) {
    if (array1 == nullptr || array2 == nullptr || size <= 0) {
        return -1; // 无效输入
    }

    int closestIndex = 0;
    double minSqDiff = std::numeric_limits<double>::max();

    for (int i = 0; i < size; ++i) {
        double sqDiff = (array1[i] - target1) * (array1[i] - target1) + (array2[i] - target2) * (array2[i] - target2);

        if (sqDiff < minSqDiff) {
            minSqDiff = sqDiff;
            closestIndex = i;
        }
    }

    return closestIndex;
}

PupilInfo createPupilFromRectLegacyForCalc(cv::Mat img, int num, cv::Rect eyeRect,
                                           float wh_ratio_threshold, enWhichEye whichEye)
{
    // 旧版路径只负责提高召回率，不要求可靠映光点，也不走新增的 ROI fallback/边界严格门槛。
    cv::Rect safeEyeRect = eyeRect & cv::Rect(0, 0, img.cols, img.rows);
    if (safeEyeRect.area() <= 0) {
#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
        if (shouldLogPupilFailDetail()) {
            qDebug().noquote() << QString("PupilFormalTrace: img=%1,eye=%2,stage=createPupilFromRectLegacyForCalc,reason=empty_eye_rect,eye_rect=%3,safe_eye_rect=%4")
                        .arg(num)
                        .arg(whichEye == whichEye_Right ? "right" : "left")
                        .arg(rectToDiagString(eyeRect))
                        .arg(rectToDiagString(safeEyeRect));
        }
#endif
        return PupilInfo();
    }

    cv::Mat eyeImg = img(safeEyeRect).clone();

    cv::Point2f localCenter;
    cv::Point localSpot;
    float radius = 0.0f;
    if (!findPupilLegacyForCalc(eyeImg, num, localCenter, radius, wh_ratio_threshold)) {
#if ENABLE_PREVIEW_PUPIL_FAIL_DETAIL_LOG || ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
        // 预览/正式阶段瞳孔失败时，按限流规则记录眼框内的候选统计。
        logPupilFailDetail(img, num, safeEyeRect, whichEye,
                           wh_ratio_threshold, "createPupilFromRectLegacy_failed",
                           g_forcePupilFailDetailLog);
#endif
        return PupilInfo();
    }

    getGlintBlobPoint(eyeImg, localSpot, whichEye);

    const cv::Point2f globalCenter(localCenter.x + safeEyeRect.x,
                                   localCenter.y + safeEyeRect.y);
    const cv::Point2f globalSpot(static_cast<float>(localSpot.x + safeEyeRect.x),
                                 static_cast<float>(localSpot.y + safeEyeRect.y));

    return PupilInfo(num, whichEye, globalCenter, radius, globalSpot);
}

static PupilInfo createPupilFromLShapeHalfRoiFallback(cv::Mat img, int num, const cv::Rect& halfRoi,
                                                      float whRatioThreshold, enWhichEye whichEye)
{
    // 正式转灯图兜底：Haar 小眼框失败时，直接在 L 型固定左右半区内找瞳孔，避免眼框缺失导致整帧失败。
    cv::Rect safeHalfRoi = halfRoi & cv::Rect(0, 0, img.cols, img.rows);
    if (safeHalfRoi.area() <= 0) {
        return PupilInfo();
    }

    PupilInfo pupil = createPupilFromRectLegacyForCalc(img, num, safeHalfRoi,
                                                       whRatioThreshold, whichEye);
#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
    if (pupil.radius() > 0.0f && shouldLogPupilFailDetail()) {
        qDebug().noquote() << QString("PupilFormalTrace: img=%1,eye=%2,stage=lshape_half_roi_fallback_success,"
                                      "half_roi=%3,pupil=(%4,%5,r=%6)")
                    .arg(num)
                    .arg(whichEye == whichEye_Right ? "right" : "left")
                    .arg(rectToDiagString(safeHalfRoi))
                    .arg(pupil.center().x, 0, 'f', 1)
                    .arg(pupil.center().y, 0, 'f', 1)
                    .arg(pupil.radius(), 0, 'f', 1);
    }
#endif
    return pupil;
}

struct LowPupilCandidate
{
    cv::Rect rect;
    cv::Point2f center;
    float radius = 0.0f;
    double area = 0.0;
    double circularity = 0.0;
    double score = 0.0;
};

#if ENABLE_PREVIEW_DIAG_LOG
static bool shouldLogPreviewLowPupilFallback(bool success)
{
    static std::atomic<int> lowFallbackCounter(0);
    static std::atomic<int> lastSuccess(-1);
    const int count = lowFallbackCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    const int currentSuccess = success ? 1 : 0;
    const int prevSuccess = lastSuccess.exchange(currentSuccess, std::memory_order_relaxed);

    // 低位救援只在原路径失败时触发；首批完整输出，后续按状态变化和间隔限流。
    return count <= 20 || prevSuccess != currentSuccess || (count % 15 == 0);
}

static void logPreviewLowPupilFallback(const QString& stage,
                                       int num,
                                       enWhichEye whichEye,
                                       const cv::Rect& lowRoi,
                                       int candidateCount,
                                       PupilInfo pupil)
{
    const bool success = pupil.radius() > 0.0f;
    if (!shouldLogPreviewLowPupilFallback(success)) {
        return;
    }

    const cv::Point2f center = pupil.center();
    qDebug().noquote() << QString("PreviewLowPupilFallback: img=%1,stage=%2,eye=%3,"
                                  "roi=%4,candidates=%5,success=%6,pupil=(%7,%8,r=%9)")
                          .arg(num)
                          .arg(stage)
                          .arg(whichEye == whichEye_Right ? "right" : "left")
                          .arg(rectToDiagString(lowRoi))
                          .arg(candidateCount)
                          .arg(success ? 1 : 0)
                          .arg(center.x, 0, 'f', 1)
                          .arg(center.y, 0, 'f', 1)
                          .arg(pupil.radius(), 0, 'f', 1);
}
#endif

static cv::Rect lShapeLowPupilRescueRoi(const cv::Rect& halfRoi, const cv::Size& imageSize)
{
    if (halfRoi.area() <= 0 || imageSize.width <= 0 || imageSize.height <= 0) {
        return cv::Rect();
    }

    // 低位救援只覆盖 L 型左右半区的下 65%，专门处理瞳孔靠近有效红框下沿时的预览漏检。
    static const float LOW_RESCUE_START_RATIO = 0.35f;
    const cv::Rect imageRect(0, 0, imageSize.width, imageSize.height);
    const cv::Rect safeHalfRoi = halfRoi & imageRect;
    const int lowY = safeHalfRoi.y + static_cast<int>(std::round(safeHalfRoi.height * LOW_RESCUE_START_RATIO));
    return cv::Rect(safeHalfRoi.x,
                    lowY,
                    safeHalfRoi.width,
                    safeHalfRoi.y + safeHalfRoi.height - lowY) & imageRect;
}

static cv::Rect expandRectAroundCenter(const cv::Point2f& center, int sideLength,
                                       const cv::Rect& limitRect, const cv::Size& imageSize)
{
    const cv::Rect imageRect(0, 0, imageSize.width, imageSize.height);
    const cv::Rect safeLimit = limitRect.area() > 0 ? (limitRect & imageRect) : imageRect;
    if (safeLimit.area() <= 0 || sideLength <= 0) {
        return cv::Rect();
    }

    cv::Rect rect(static_cast<int>(std::round(center.x)) - sideLength / 2,
                  static_cast<int>(std::round(center.y)) - sideLength / 2,
                  sideLength,
                  sideLength);
    return rect & safeLimit;
}

static std::vector<LowPupilCandidate> findBrightPupilCandidatesInLowRoi(const cv::Mat& img,
                                                                        const cv::Rect& lowRoi,
                                                                        const cv::Rect& halfRoi)
{
    std::vector<LowPupilCandidate> candidates;
    if (img.empty() || img.channels() != 1 || lowRoi.area() <= 0) {
        return candidates;
    }

    const cv::Rect imageRect(0, 0, img.cols, img.rows);
    const cv::Rect safeLowRoi = lowRoi & imageRect;
    const cv::Rect safeHalfRoi = halfRoi & imageRect;
    if (safeLowRoi.area() <= 0 || safeHalfRoi.area() <= 0) {
        return candidates;
    }

    const cv::Mat lowImg = img(safeLowRoi);
    cv::Scalar meanValue;
    cv::Scalar stdValue;
    cv::meanStdDev(lowImg, meanValue, stdValue);

    double maxValue = 0.0;
    cv::minMaxLoc(lowImg, nullptr, &maxValue);
    if (maxValue < 70.0) {
        return candidates;
    }

    // 亮瞳孔候选阈值：随局部亮度自适应，同时避免只剩一个映光点小斑。
    double thresholdValue = meanValue[0] + std::max(18.0, stdValue[0] * 1.15);
    thresholdValue = std::max(70.0, std::min(230.0, thresholdValue));
    if (maxValue > thresholdValue + 8.0) {
        thresholdValue = std::min(thresholdValue, maxValue - 8.0);
    }

    cv::Mat mask;
    cv::threshold(lowImg, mask, thresholdValue, 255, cv::THRESH_BINARY);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < MIN_PUPIL_AREA * 0.25 || area > MAX_PUPIL_AREA * 1.8) {
            continue;
        }

        const double perimeter = cv::arcLength(contour, true);
        if (perimeter <= 0.0) {
            continue;
        }

        const double circularity = 4.0 * M_PI * area / (perimeter * perimeter);
        if (circularity < 0.38) {
            continue;
        }

        cv::Point2f localCenter;
        float radius = 0.0f;
        cv::minEnclosingCircle(contour, localCenter, radius);
        if (radius < validPupilRadiusMin() * 0.7f || radius > validPupilRadiusMax() * 1.35f) {
            continue;
        }

        const cv::Point2f globalCenter(localCenter.x + safeLowRoi.x,
                                       localCenter.y + safeLowRoi.y);
        if (!safeHalfRoi.contains(cv::Point(static_cast<int>(std::round(globalCenter.x)),
                                           static_cast<int>(std::round(globalCenter.y))))) {
            continue;
        }

        const int sideLength = std::max(ROI_WIDTH,
                                        static_cast<int>(std::ceil(radius * 4.2f)));
        cv::Rect candidateRect = expandRectAroundCenter(globalCenter,
                                                        sideLength,
                                                        safeHalfRoi,
                                                        img.size());
        if (candidateRect.area() <= 0) {
            continue;
        }

        LowPupilCandidate candidate;
        candidate.rect = candidateRect;
        candidate.center = globalCenter;
        candidate.radius = radius;
        candidate.area = area;
        candidate.circularity = circularity;
        candidate.score = circularity * 1000.0 + area;
        candidates.push_back(candidate);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const LowPupilCandidate& a, const LowPupilCandidate& b) {
                  return a.score > b.score;
              });

    if (candidates.size() > 3) {
        candidates.resize(3);
    }
    return candidates;
}

static PupilInfo createPupilFromLShapeLowPupilFallback(cv::Mat img, int num,
                                                       const cv::Rect& halfRoi,
                                                       float whRatioThreshold,
                                                       enWhichEye whichEye)
{
    // 仅预览阶段使用：Haar 小框和平移框都没有找出瞳孔时，在低位半区找亮瞳孔候选再交给旧算法确认。
    const cv::Rect lowRoi = lShapeLowPupilRescueRoi(halfRoi, img.size());
    std::vector<LowPupilCandidate> candidates =
            findBrightPupilCandidatesInLowRoi(img, lowRoi, halfRoi);

    for (const LowPupilCandidate& candidate : candidates) {
        PupilInfo pupil = createPupilFromRectLegacyForCalc(img, num, candidate.rect,
                                                           whRatioThreshold, whichEye);
        if (pupil.radius() > 0.0f) {
#if ENABLE_PREVIEW_DIAG_LOG
            logPreviewLowPupilFallback("success", num, whichEye, lowRoi,
                                       static_cast<int>(candidates.size()), pupil);
#endif
            return pupil;
        }
    }

#if ENABLE_PREVIEW_DIAG_LOG
    logPreviewLowPupilFallback("failed", num, whichEye, lowRoi,
                               static_cast<int>(candidates.size()), PupilInfo());
#endif
    return PupilInfo();
}

PupilInfo createPupilFromRectForSimulatedEye(cv::Mat img, int num, cv::Rect eyeRect,
                                             float wh_ratio_threshold, enWhichEye whichEye)
{
    // 模拟眼走旧版瞳孔路径：不依赖 Haar 人眼框，也不要求正常人眼的映光点/半径特征。
    cv::Rect safeEyeRect = eyeRect & cv::Rect(0, 0, img.cols, img.rows);
    if (safeEyeRect.area() <= 0) {
        return PupilInfo();
    }

    cv::Mat eyeImg = img(safeEyeRect).clone();

    cv::Point2f localCenter;
    cv::Point localSpot;
    float radius = 0.0f;
    if (!findPupilForSimulatedEye(eyeImg, num, localCenter, radius, wh_ratio_threshold)) {
        return PupilInfo();
    }

    getGlintBlobPoint(eyeImg, localSpot, whichEye);

    const cv::Point2f globalCenter(localCenter.x + safeEyeRect.x,
                                   localCenter.y + safeEyeRect.y);
    const cv::Point2f globalSpot(static_cast<float>(localSpot.x + safeEyeRect.x),
                                 static_cast<float>(localSpot.y + safeEyeRect.y));

    return PupilInfo(num, whichEye, globalCenter, radius, globalSpot,
                     PupilFallback_None, PupilEyeRect_Base);
}

bool isValidPupilInfo(PupilInfo pupil)
{
    return pupil.radius() > 0;
}

QString previewPupilFailSuspect(int candidateCount,
                                float bestRatio,
                                float radius,
                                bool hasReliableGlint,
                                double brightRatio,
                                double saturatedRatio)
{
    if (candidateCount <= 0) {
        return "no_dark_candidate_or_closed_eye";
    }

    if (bestRatio > 0.0f && bestRatio < 0.88f) {
        return "eyelid_or_lash_occlusion_suspected";
    }

    if (radius > 0.0f && !isValidPupilRadius(std::floor(radius))) {
        return "radius_out_of_range";
    }

    if (!hasReliableGlint) {
        return "glint_unreliable";
    }

    if (saturatedRatio > 0.03 || brightRatio > 0.25) {
        return "over_exposure_or_large_glint_suspected";
    }

    return "candidate_rejected_by_later_gate";
}

void logPupilFailDetail(const cv::Mat& img,
                        int num,
                        const cv::Rect& eyeRect,
                        enWhichEye whichEye,
                        float whRatioThreshold,
                        const QString& stage,
                        bool forceLog)
{
    if (forceLog) {
        // 强制日志同样走限流，避免正式转灯连续失败时绕过 shouldLogPupilFailDetail() 刷屏。
        if (!acquirePupilFailDetailLogBudget()) {
            return;
        }
    } else if (!shouldLogPupilFailDetail()) {
        return;
    }

    const cv::Rect safeEyeRect = eyeRect & cv::Rect(0, 0, img.cols, img.rows);
    if (safeEyeRect.area() <= 0) {
        qDebug().noquote() << QString("PupilFailDetail: img=%1,eye=%2,stage=%3,reason=invalid_eye_rect,eye_rect=(%4,%5,%6,%7)")
                    .arg(num)
                    .arg(whichEye == whichEye_Right ? "right" : "left")
                    .arg(stage)
                    .arg(eyeRect.x)
                    .arg(eyeRect.y)
                    .arg(eyeRect.width)
                    .arg(eyeRect.height);
        return;
    }

    const cv::Mat eyeImg = img(safeEyeRect).clone();
    const cv::Mat bImg = binaryImage(eyeImg);
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(bImg, contours, hierarchy, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);

    int areaBelowMinCount = 0;
    int areaAboveMaxCount = 0;
    int areaValidCount = 0;
    int ratioRejectedCount = 0;
    int ratioValidCount = 0;
    double largestAreaBelowMin = -1.0;
    double smallestAreaAboveMax = std::numeric_limits<double>::max();
    float bestRatioAfterArea = 0.0f;

    for (const std::vector<cv::Point>& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area <= MIN_PUPIL_AREA) {
            areaBelowMinCount++;
            largestAreaBelowMin = std::max(largestAreaBelowMin, area);
            continue;
        }
        if (area >= MAX_PUPIL_AREA) {
            areaAboveMaxCount++;
            smallestAreaAboveMax = std::min(smallestAreaAboveMax, area);
            continue;
        }

        areaValidCount++;
        const cv::RotatedRect contourRect = cv::minAreaRect(contour);
        const float w = static_cast<float>(contourRect.size.width);
        const float h = static_cast<float>(contourRect.size.height);
        if (w <= 0.0f || h <= 0.0f) {
            ratioRejectedCount++;
            continue;
        }

        const float ratio = std::min(w, h) / std::max(w, h);
        bestRatioAfterArea = std::max(bestRatioAfterArea, ratio);
        if (ratio > whRatioThreshold) {
            ratioValidCount++;
        } else {
            ratioRejectedCount++;
        }
    }

    // 清理旧优化残留：失败诊断只按当前路径的长宽比阈值统计候选，不再引入非圆候选兜底阈值。
    const float coarseRatioThreshold = whRatioThreshold;
    std::vector<std::vector<cv::Point>> candidateContours =
            filterContoursByAreaAndRatio(contours, MIN_PUPIL_AREA, MAX_PUPIL_AREA,
                                         coarseRatioThreshold);

    int bestIndex = -1;
    float bestRatio = 0.0f;
    for (size_t i = 0; i < candidateContours.size(); ++i) {
        const cv::RotatedRect contourRect = cv::minAreaRect(candidateContours.at(i));
        const float w = static_cast<float>(contourRect.size.width);
        const float h = static_cast<float>(contourRect.size.height);
        if (w <= 0.0f || h <= 0.0f) {
            continue;
        }

        const float ratio = std::min(w, h) / std::max(w, h);
        if (ratio > bestRatio) {
            bestRatio = ratio;
            bestIndex = static_cast<int>(i);
        }
    }

    cv::Point2f candidateCenter(-1.0f, -1.0f);
    float candidateRadius = -1.0f;
    double candidateArea = -1.0;
    if (bestIndex >= 0) {
        cv::minEnclosingCircle(candidateContours.at(bestIndex),
                               candidateCenter,
                               candidateRadius);
        candidateArea = cv::contourArea(candidateContours.at(bestIndex));
    }

    cv::Point2f reliableGlintPoint(-1.0f, -1.0f);
    double glintMaxValue = 0.0;
    double glintLocalMean = 0.0;
    double glintLocalStd = 0.0;
    const bool hasReliableGlint = findReliableGlintPoint(eyeImg,
                                                         reliableGlintPoint,
                                                         glintMaxValue,
                                                         glintLocalMean,
                                                         glintLocalStd);

    cv::Mat brightMask;
    cv::threshold(eyeImg, brightMask, 180, 255, cv::THRESH_BINARY);
    const int brightCount = cv::countNonZero(brightMask);
    cv::Mat saturatedMask;
    cv::threshold(eyeImg, saturatedMask, 245, 255, cv::THRESH_BINARY);
    const int saturatedCount = cv::countNonZero(saturatedMask);
    const double roiArea = std::max(1, safeEyeRect.area());
    const double brightRatio = brightCount / roiArea;
    const double saturatedRatio = saturatedCount / roiArea;

    const QString suspect = previewPupilFailSuspect(static_cast<int>(candidateContours.size()),
                                                    bestRatio,
                                                    candidateRadius,
                                                    hasReliableGlint,
                                                    brightRatio,
                                                    saturatedRatio);

    QString rejectReason;
    if (contours.empty()) {
        rejectReason = "no_contour_after_binary";
    } else if (areaValidCount == 0) {
        if (areaBelowMinCount > 0 && areaAboveMaxCount == 0) {
            rejectReason = "all_contours_below_min_area";
        } else if (areaAboveMaxCount > 0 && areaBelowMinCount == 0) {
            rejectReason = "all_contours_above_max_area";
        } else {
            rejectReason = "all_contours_outside_area_range";
        }
    } else if (ratioValidCount == 0) {
        rejectReason = "all_area_valid_contours_below_ratio";
    } else {
        rejectReason = "candidate_exists_but_later_stage_failed";
    }

    const double loggedSmallestAreaAboveMax =
            smallestAreaAboveMax == std::numeric_limits<double>::max()
            ? -1.0 : smallestAreaAboveMax;

    qDebug().noquote() << QString("PupilFailDetail: img=%1,eye=%2,stage=%3,reason=%4,"
                                  "eye_rect=(%5,%6,%7,%8),contours=%9,"
                                  "min_area=%10,max_area=%11,area_below=%12,area_valid=%13,area_above=%14,"
                                  "largest_below=%15,smallest_above=%16,ratio_threshold=%17,"
                                  "ratio_rejected=%18,ratio_valid=%19,best_ratio_after_area=%20,"
                                  "candidates=%21,candidate_center=(%22,%23),candidate_radius=%24,candidate_area=%25,"
                                  "glint_ok=%26,glint=(%27,%28),glint_max=%29,glint_mean=%30,glint_std=%31,"
                                  "bright_count=%32,bright_ratio=%33,saturated_count=%34,saturated_ratio=%35,suspect=%36")
                .arg(num)
                .arg(whichEye == whichEye_Right ? "right" : "left")
                .arg(stage)
                .arg(rejectReason)
                .arg(safeEyeRect.x)
                .arg(safeEyeRect.y)
                .arg(safeEyeRect.width)
                .arg(safeEyeRect.height)
                .arg(static_cast<int>(contours.size()))
                .arg(MIN_PUPIL_AREA, 0, 'f', 1)
                .arg(MAX_PUPIL_AREA, 0, 'f', 1)
                .arg(areaBelowMinCount)
                .arg(areaValidCount)
                .arg(areaAboveMaxCount)
                .arg(largestAreaBelowMin, 0, 'f', 1)
                .arg(loggedSmallestAreaAboveMax, 0, 'f', 1)
                .arg(whRatioThreshold, 0, 'f', 3)
                .arg(ratioRejectedCount)
                .arg(ratioValidCount)
                .arg(bestRatioAfterArea, 0, 'f', 3)
                .arg(static_cast<int>(candidateContours.size()))
                .arg(candidateCenter.x + safeEyeRect.x, 0, 'f', 1)
                .arg(candidateCenter.y + safeEyeRect.y, 0, 'f', 1)
                .arg(candidateRadius, 0, 'f', 1)
                .arg(candidateArea, 0, 'f', 1)
                .arg(hasReliableGlint ? 1 : 0)
                .arg(reliableGlintPoint.x + safeEyeRect.x, 0, 'f', 1)
                .arg(reliableGlintPoint.y + safeEyeRect.y, 0, 'f', 1)
                .arg(glintMaxValue, 0, 'f', 1)
                .arg(glintLocalMean, 0, 'f', 1)
                .arg(glintLocalStd, 0, 'f', 1)
                .arg(brightCount)
                .arg(brightRatio, 0, 'f', 4)
                .arg(saturatedCount)
                .arg(saturatedRatio, 0, 'f', 4)
                .arg(suspect);
}

//根据技术指标，瞳孔尺寸3.2mm~9mm，瞳孔距离35mm~80mm,上下左右预留一定的位置
std::pair<Rect, Rect> getModelEyes() {
    Rect rightEye,leftEye;
//    rightEye=Rect(190,131,400,250);
//    leftEye=Rect(740,131,400,250);
    rightEye=Rect(190,64,400,384);
    leftEye=Rect(740,64,400,384);
    return std::make_pair(rightEye,leftEye);
}

//注意，这里取gamma的倒数，跟python一致
Mat adjustGamma(Mat _img, double gamma) {
    float invGamma = 1.0/gamma;
    Mat lut(1, 256, CV_8UC1);
    for (int i = 0; i < 256; i++) {
        lut.at<uchar>(i) = saturate_cast<uchar>(pow((double)i / 255.0, invGamma) * 255.0);
    }

    Mat result;
    LUT(_img, lut, result);
    return result;
}


std::pair<Rect, Rect> haarDetectEyes(const Mat& _img, const double scaleFactor,
                                         int miniNeighbors, const float scale) {
    Rect rightEye, leftEye;
    std::vector<Rect> eyes;

    try {
        PERF_SCOPE("detectMultiScale");
        // 每个线程调用自己的getEyeCascade()，获得独立的实例
        CascadeClassifier& eyeCascade = getEyeCascade();

        eyeCascade.detectMultiScale(_img, eyes, scaleFactor, miniNeighbors,
                                   CASCADE_SCALE_IMAGE, Size(25,25), Size(45,45));
    } catch (const std::exception& e) {
        ALGO_ERROR_LOG(qCritical() << "Exception in haarDetectEyes:" << e.what());
    } catch (...) {
        ALGO_ERROR_LOG(qCritical() << "Unknown exception in haarDetectEyes");
    }

    // 如果检测到人眼，则返回真
    if (eyes.size() > 0) {
        std::vector<Rect> rRects;
        std::vector<Rect> lRects;

        for(size_t i=0;i<eyes.size();i++) {
            Rect e=eyes.at(i);
            Rect eye(scale*e.x, scale*e.y, scale*e.width, scale*e.height);
            //判断眼睛矩形框中心在中线左侧还是右侧
            if(e.x+e.width/2<=_img.cols/2) {
                rRects.push_back(eye);
            } else {
                lRects.push_back(eye);
            }
        }

        rightEye=maxRect(rRects);
        leftEye=maxRect(lRects);
    }

    return std::make_pair(rightEye, leftEye);
}

static bool detectHumanEyesWithParams(const cv::Mat& image,
                                      cv::Rect& rightEye,
                                      cv::Rect& leftEye,
                                      int gammaThreshold,
                                      double scaleFactor,
                                      const cv::Size& minEyeSize,
                                      const cv::Size& maxEyeSize,
                                      const cv::Rect& priorRoi,
                                      const char* perfName)
{
#if ENABLE_ALGO_TIMING_LOG
    ALGO_TIMING_SCOPE(AlgoTimingStage_EyeDetectTotal);
    const bool isRelaxedPass = std::strstr(perfName, "Relaxed") != nullptr;
#endif
    // 初始化输出参数
    rightEye = cv::Rect();
    leftEye = cv::Rect();

    // 验证输入图像
    if (image.empty()) {
        return false;
    }

    // 预处理图像 - 缩小尺寸提高检测速度
    static const double DEFAULT_GAMMA = 2.9;
    static const float IMAGE_SCALE = 6.0f;
    static const int MIN_NEIGHBORS = 1;

    cv::Mat resizedImg;
    {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_EyeResize);
#endif
        cv::resize(image, resizedImg, cv::Size(), 1.0f / IMAGE_SCALE, 1.0f / IMAGE_SCALE,
                   cv::INTER_AREA);  // 使用 INTER_AREA 更适合缩小
    }

    const cv::Rect fullRoi(0, 0, resizedImg.cols, resizedImg.rows);
    const cv::Rect safePriorRoi = priorRoi.area() > 0 ? (priorRoi & fullRoi) : fullRoi;
    const bool usePriorRoi = (safePriorRoi.area() > 0
                              && (safePriorRoi.x != fullRoi.x
                                  || safePriorRoi.y != fullRoi.y
                                  || safePriorRoi.width != fullRoi.width
                                  || safePriorRoi.height != fullRoi.height));

    // 计算平均亮度
    double avgGray = 0.0;
    {
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_SCOPE(AlgoTimingStage_EyePreprocess);
#endif
        avgGray = cv::mean(resizedImg)[0];
    }

    PERF_POINT(QString("detectHumanEyes avgGray=%1").arg(avgGray));

    // 检测眼睛的主逻辑函数
    auto detectEyesWithImage = [&](const cv::Mat& inputImg) -> bool {
        cv::Rect tempRightEye, tempLeftEye;
        const cv::Mat detectImg = usePriorRoi ? inputImg(safePriorRoi) : inputImg;
        {
#if ENABLE_ALGO_TIMING_LOG
            ALGO_TIMING_SCOPE(isRelaxedPass
                              ? AlgoTimingStage_HaarRelaxed
                              : AlgoTimingStage_HaarNormal);
#endif
            std::tie(tempRightEye, tempLeftEye) = haarDetectEyesSafe(detectImg, scaleFactor, MIN_NEIGHBORS,
                                                                    IMAGE_SCALE, minEyeSize, maxEyeSize);
        }
        if (usePriorRoi) {
            // Haar 在裁剪后的小图中返回的是局部坐标，这里补回缩小图 ROI 偏移，再换算回原图坐标。
            tempRightEye.x += static_cast<int>(safePriorRoi.x * IMAGE_SCALE);
            tempRightEye.y += static_cast<int>(safePriorRoi.y * IMAGE_SCALE);
            tempLeftEye.x += static_cast<int>(safePriorRoi.x * IMAGE_SCALE);
            tempLeftEye.y += static_cast<int>(safePriorRoi.y * IMAGE_SCALE);
        }

        PERF_POINT(QString("%1 tempRightEye=%2, tempLeftEye=%3")
                   .arg(perfName)
                   .arg(cvRectEmpty(tempRightEye))
                   .arg(cvRectEmpty(tempLeftEye)));

        // 如果检测不完整，尝试直方图均衡化
        if (cvRectEmpty(tempRightEye) || cvRectEmpty(tempLeftEye)) {
#if ENABLE_ALGO_TIMING_LOG
            ALGO_TIMING_SCOPE(AlgoTimingStage_HaarEqualized);
#endif
            cv::Mat eqImg;
            cv::equalizeHist(inputImg, eqImg);

            cv::Rect fallbackRight, fallbackLeft;
            const cv::Mat fallbackDetectImg = usePriorRoi ? eqImg(safePriorRoi) : eqImg;
            std::tie(fallbackRight, fallbackLeft) = haarDetectEyesSafe(fallbackDetectImg, scaleFactor, MIN_NEIGHBORS,
                                                                       IMAGE_SCALE, minEyeSize, maxEyeSize);
            if (usePriorRoi) {
                // 均衡化兜底路径同样需要把局部 ROI 坐标补回原图坐标。
                fallbackRight.x += static_cast<int>(safePriorRoi.x * IMAGE_SCALE);
                fallbackRight.y += static_cast<int>(safePriorRoi.y * IMAGE_SCALE);
                fallbackLeft.x += static_cast<int>(safePriorRoi.x * IMAGE_SCALE);
                fallbackLeft.y += static_cast<int>(safePriorRoi.y * IMAGE_SCALE);
            }

            // 只替换未检测到的眼睛
            if (cvRectEmpty(tempRightEye) && !cvRectEmpty(fallbackRight)) {
                tempRightEye = fallbackRight;
            }
            if (cvRectEmpty(tempLeftEye) && !cvRectEmpty(fallbackLeft)) {
                tempLeftEye = fallbackLeft;
            }
        }

        // 更新引用参数
        rightEye = tempRightEye;
        leftEye = tempLeftEye;

        // 返回检测成功状态：至少检测到一只眼睛
        return !cvRectEmpty(rightEye) || !cvRectEmpty(leftEye);
    };

    // 根据亮度选择检测策略
    bool detectionSuccess = false;
    const bool usedGamma = (avgGray < gammaThreshold);
    if (usedGamma) {
        // 暗图像：先进行gamma校正
        cv::Mat gammaImg;
        {
#if ENABLE_ALGO_TIMING_LOG
            ALGO_TIMING_SCOPE(AlgoTimingStage_EyePreprocess);
#endif
            gammaImg = adjustGamma(resizedImg, DEFAULT_GAMMA);
        }
        detectionSuccess = detectEyesWithImage(gammaImg);
    } else {
        // 亮图像：直接检测
        detectionSuccess = detectEyesWithImage(resizedImg);
    }

    return detectionSuccess;
}

static cv::Rect lShapeEffectiveRoiForSize(const cv::Size& size)
{
    if (size.width <= 0 || size.height <= 0) {
        return cv::Rect();
    }

    // L 型视筛箱固定视野先验：与预览阶段保持同一块有效红框；新机器眼区更靠左、靠下。
    return cv::Rect(static_cast<int>(size.width * 0.09f),
                    static_cast<int>(size.height * 0.25f),
                    static_cast<int>(size.width * 0.77f),
                    static_cast<int>(size.height * 0.75f))
            & cv::Rect(0, 0, size.width, size.height);
}

static cv::Rect lShapeEyeSearchRoiForImage(const cv::Size& imageSize, enWhichEye whichEye)
{
    cv::Rect effectiveRoi = lShapeEffectiveRoiForSize(imageSize);
    if (cvRectEmpty(effectiveRoi)) {
        return cv::Rect();
    }

    // 图像左半区是受检者右眼，图像右半区是受检者左眼。
    const int halfWidth = effectiveRoi.width / 2;
    if (whichEye == whichEye_Right) {
        return cv::Rect(effectiveRoi.x, effectiveRoi.y, halfWidth, effectiveRoi.height);
    }

    return cv::Rect(effectiveRoi.x + halfWidth,
                    effectiveRoi.y,
                    effectiveRoi.width - halfWidth,
                    effectiveRoi.height);
}

static void extendEyeRectDownForLShapeCalc(const cv::Mat& image, cv::Rect& eyeRect, const cv::Rect& limitRect = cv::Rect())
{
    if (image.empty() || cvRectEmpty(eyeRect)) {
        return;
    }

    // L 型正式转灯图专用：只向下扩展眼眶 ROI，解决低位瞳孔落在 Haar 眼框下方的问题。
    const float DOWN_EXTEND_RATIO = 1.6f;
    const int newHeight = static_cast<int>(std::ceil(eyeRect.height * DOWN_EXTEND_RATIO));
    const cv::Rect imageRect = cvRectEmpty(limitRect)
            ? cv::Rect(0, 0, image.cols, image.rows)
            : (limitRect & cv::Rect(0, 0, image.cols, image.rows));
    eyeRect.height = std::max(eyeRect.height, newHeight);
    eyeRect = eyeRect & imageRect;
}

bool detectHumanEyes(const cv::Mat& image, cv::Rect& rightEye, cv::Rect& leftEye,
                     bool allowRelaxedFallback) {
    if (opticalPathType_LShape == g_opticalPathType && !image.empty()) {
        static const float IMAGE_SCALE = 6.0f;
        const cv::Size resizedSize(static_cast<int>(image.cols / IMAGE_SCALE),
                                   static_cast<int>(image.rows / IMAGE_SCALE));
        const cv::Rect effectiveRoiOnResized = lShapeEffectiveRoiForSize(resizedSize);

        // L 型先在有效红框内跑一次常规 Haar；宽松兜底只给预览/非正式阶段使用。
        bool foundAnyEye = detectHumanEyesWithParams(image, rightEye, leftEye,
                                                     60,
                                                     1.075,
                                                     cv::Size(25,25),
                                                     cv::Size(45,45),
                                                     effectiveRoiOnResized,
                                                     "detectHumanEyesLShape");
#if ENABLE_PREVIEW_DIAG_LOG
        if (allowRelaxedFallback) {
            logPreviewHaarDiag("normal", foundAnyEye, rightEye, leftEye, effectiveRoiOnResized);
        }
#endif

        if (allowRelaxedFallback && (!foundAnyEye || (cvRectEmpty(rightEye) && cvRectEmpty(leftEye)))) {
            cv::Rect relaxedRightEye;
            cv::Rect relaxedLeftEye;
            const bool relaxedFoundAnyEye = detectHumanEyesWithParams(image, relaxedRightEye, relaxedLeftEye,
                                                                      75,
                                                                      1.05,
                                                                      cv::Size(20,20),
                                                                      cv::Size(55,55),
                                                                      effectiveRoiOnResized,
                                                                      "detectHumanEyesLShapeRelaxedFallback");
#if ENABLE_PREVIEW_DIAG_LOG
            logPreviewHaarDiag("relaxed", relaxedFoundAnyEye, relaxedRightEye, relaxedLeftEye, effectiveRoiOnResized);
#endif
            if (relaxedFoundAnyEye) {
                // 预览阶段复用宽松找眼参数提高放行召回，最终瞳孔仍由 detectPupil 证伪。
                rightEye = relaxedRightEye;
                leftEye = relaxedLeftEye;
                foundAnyEye = true;
            }
        }

        const cv::Rect rightLimit = lShapeEyeSearchRoiForImage(image.size(), whichEye_Right);
        const cv::Rect leftLimit = lShapeEyeSearchRoiForImage(image.size(), whichEye_Left);
        const cv::Rect rawRightEye = rightEye;
        const cv::Rect rawLeftEye = leftEye;
        rightEye = rightEye & rightLimit;
        leftEye = leftEye & leftLimit;

        if (!cvRectEmpty(rightEye)) {
            extendEyeRectDownForLShapeCalc(image, rightEye, rightLimit);
        }
        if (!cvRectEmpty(leftEye)) {
            extendEyeRectDownForLShapeCalc(image, leftEye, leftLimit);
        }
#if ENABLE_PREVIEW_DIAG_LOG
        if (allowRelaxedFallback) {
            logPreviewHaarFinalDiag(foundAnyEye, rawRightEye, rawLeftEye,
                                    rightLimit, leftLimit, rightEye, leftEye);
        }
#endif
        return foundAnyEye && (!cvRectEmpty(rightEye) || !cvRectEmpty(leftEye));
    }

    // 完整 detectPupil 仍使用旧版找眼参数，避免预览门控调参影响正式瞳孔检测。
    const bool success = detectHumanEyesWithParams(image, rightEye, leftEye,
                                                   60,
                                                   1.075,
                                                   cv::Size(25,25),
                                                   cv::Size(45,45),
                                                   cv::Rect(),
                                                   "detectHumanEyes");
    return success;
}

std::pair<PupilInfo, PupilInfo> getEyesLegacyForCalc(Mat img,int num,float humaneye_wh_ratio,float modeleye_wh_ratio,
                                                     bool isCalcVision) {
    PupilInfo rpupil,lpupil;
    Rect rightEye,leftEye;
    PERF_START(detectHumanEyes_legacy);
    bool isHumanEye=detectHumanEyes(img,rightEye,leftEye, !isCalcVision);
    PERF_END(detectHumanEyes_legacy,"getEyesLegacyForCalc detectHumanEyes");

    if (opticalPathType_LShape == g_opticalPathType) {
        PERF_START(getEyes_legacy_lshape_region);
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_START(eye_rect_build);
#endif
        const cv::Rect rightLimit = lShapeEyeSearchRoiForImage(img.size(), whichEye_Right);
        const cv::Rect leftLimit = lShapeEyeSearchRoiForImage(img.size(), whichEye_Left);
        const cv::Rect effectiveRoi = lShapeEffectiveRoiForSize(img.size());
        const float effectiveCenterX = effectiveRoi.x + effectiveRoi.width / 2.0f;
        const cv::Rect rightEyeHaar = rightEye;
        const cv::Rect leftEyeHaar = leftEye;

        // L 型模式下左右眼必定分别落在红框左右半区；先把 Haar 眼框裁到对应半区。
        rightEye = rightEye & rightLimit;
        leftEye = leftEye & leftLimit;
        const bool rightHaarMissing = cvRectEmpty(rightEye);
        const bool leftHaarMissing = cvRectEmpty(leftEye);

        bool rightUsedShiftFallback = false;
        bool leftUsedShiftFallback = false;
        bool rightUsedHalfRoiFallback = false;
        bool leftUsedHalfRoiFallback = false;
        bool rightUsedLowPupilFallback = false;
        bool leftUsedLowPupilFallback = false;
        if (cvRectEmpty(rightEye) && !cvRectEmpty(leftEye)) {
            // L 型路径不扫整个右半区；按有效红框中心镜像出同尺寸小框，适配新机器 ROI 偏左的情况。
            const int mirroredX = static_cast<int>(std::round(2.0f * effectiveCenterX - (leftEye.x + leftEye.width)));
            cv::Rect mirroredRight(mirroredX, leftEye.y, leftEye.width, leftEye.height);
            rightEye = mirroredRight & rightLimit;
            rightUsedShiftFallback = !cvRectEmpty(rightEye);
        }
        if (cvRectEmpty(leftEye) && !cvRectEmpty(rightEye)) {
            // 右眼存在、左眼缺失时同样按有效红框中心镜像补框，不增加 Haar 扫描负担。
            const int mirroredX = static_cast<int>(std::round(2.0f * effectiveCenterX - (rightEye.x + rightEye.width)));
            cv::Rect mirroredLeft(mirroredX, rightEye.y, rightEye.width, rightEye.height);
            leftEye = mirroredLeft & leftLimit;
            leftUsedShiftFallback = !cvRectEmpty(leftEye);
        }
#if ENABLE_ALGO_TIMING_LOG
        ALGO_TIMING_END(eye_rect_build, AlgoTimingStage_EyeRectBuild);
#endif

        if (!cvRectEmpty(rightEye)) {
            ALGO_TIMING_SCOPE(AlgoTimingStage_PupilRight);
            rpupil = createPupilFromRectLegacyForCalc(img, num, rightEye,
                                                      humaneye_wh_ratio, whichEye_Right);
        }
        if (!cvRectEmpty(leftEye)) {
            ALGO_TIMING_SCOPE(AlgoTimingStage_PupilLeft);
            lpupil = createPupilFromRectLegacyForCalc(img, num, leftEye,
                                                      humaneye_wh_ratio, whichEye_Left);
        }

        if (!isCalcVision && rpupil.radius() <= 0.0f) {
            // 预览阶段低位救援：不再额外跑 Haar，只在右眼半区下部找亮瞳孔候选。
            PupilInfo fallbackPupil;
            {
                ALGO_TIMING_SCOPE(AlgoTimingStage_LowFallbackRight);
                fallbackPupil = createPupilFromLShapeLowPupilFallback(img, num, rightLimit,
                                                                      humaneye_wh_ratio, whichEye_Right);
            }
            if (fallbackPupil.radius() > 0.0f) {
                rpupil = fallbackPupil;
                rightUsedLowPupilFallback = true;
#if ENABLE_ALGO_TIMING_LOG
                AlgoTiming::event(AlgoTimingEvent_LowFallbackSuccess);
#endif
            }
        }
        if (!isCalcVision && lpupil.radius() <= 0.0f) {
            // 预览阶段低位救援：左眼同样限制在 L 型左半区下部，避免扩大到全图。
            PupilInfo fallbackPupil;
            {
                ALGO_TIMING_SCOPE(AlgoTimingStage_LowFallbackLeft);
                fallbackPupil = createPupilFromLShapeLowPupilFallback(img, num, leftLimit,
                                                                      humaneye_wh_ratio, whichEye_Left);
            }
            if (fallbackPupil.radius() > 0.0f) {
                lpupil = fallbackPupil;
                leftUsedLowPupilFallback = true;
#if ENABLE_ALGO_TIMING_LOG
                AlgoTiming::event(AlgoTimingEvent_LowFallbackSuccess);
#endif
            }
        }

        if (isCalcVision && rightHaarMissing && rpupil.radius() <= 0.0f) {
            // 正式转灯兜底：右眼 Haar 缺失时，在右眼固定半区内再找一次。
            PupilInfo fallbackPupil;
            {
                ALGO_TIMING_SCOPE(AlgoTimingStage_HalfFallbackRight);
                fallbackPupil = createPupilFromLShapeHalfRoiFallback(img, num, rightLimit,
                                                                     humaneye_wh_ratio, whichEye_Right);
            }
            if (fallbackPupil.radius() > 0.0f) {
                rpupil = fallbackPupil;
                rightUsedHalfRoiFallback = true;
#if ENABLE_ALGO_TIMING_LOG
                AlgoTiming::event(AlgoTimingEvent_HalfFallbackSuccess);
#endif
            }
        }
        if (isCalcVision && leftHaarMissing && lpupil.radius() <= 0.0f) {
            // 正式转灯兜底：左眼 Haar 缺失时，只在 L 型左眼固定半区内搜索，避免回到全图扫描。
            PupilInfo fallbackPupil;
            {
                ALGO_TIMING_SCOPE(AlgoTimingStage_HalfFallbackLeft);
                fallbackPupil = createPupilFromLShapeHalfRoiFallback(img, num, leftLimit,
                                                                     humaneye_wh_ratio, whichEye_Left);
            }
            if (fallbackPupil.radius() > 0.0f) {
                lpupil = fallbackPupil;
                leftUsedHalfRoiFallback = true;
#if ENABLE_ALGO_TIMING_LOG
                AlgoTiming::event(AlgoTimingEvent_HalfFallbackSuccess);
#endif
            }
        }

#if ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
        const bool needLShapeTrace = rightUsedShiftFallback
                || leftUsedShiftFallback
                || rightUsedHalfRoiFallback
                || leftUsedHalfRoiFallback
                || rightUsedLowPupilFallback
                || leftUsedLowPupilFallback
                || rpupil.radius() <= 0.0f
                || lpupil.radius() <= 0.0f;
        if (needLShapeTrace && shouldLogPupilFailDetail()) {
            qDebug().noquote() << QString("PupilFormalTrace: img=%1,stage=getEyesLegacyForCalcLShape,"
                                          "isHumanEye=%2,"
                                          "right_haar=%3,left_haar=%4,"
                                          "right_limit=%5,left_limit=%6,"
                                           "right_final_rect=%7,left_final_rect=%8,"
                                           "right_used_shift=%9,left_used_shift=%10,"
                                          "right_used_half_roi=%11,left_used_half_roi=%12,"
                                          "right_used_low_pupil=%13,left_used_low_pupil=%14,"
                                          "right_pupil=(%15,%16,r=%17),left_pupil=(%18,%19,r=%20)")
                        .arg(num)
                        .arg(isHumanEye ? 1 : 0)
                        .arg(rectToDiagString(rightEyeHaar))
                        .arg(rectToDiagString(leftEyeHaar))
                        .arg(rectToDiagString(rightLimit))
                        .arg(rectToDiagString(leftLimit))
                        .arg(rectToDiagString(rightEye))
                        .arg(rectToDiagString(leftEye))
                        .arg(rightUsedShiftFallback ? 1 : 0)
                        .arg(leftUsedShiftFallback ? 1 : 0)
                        .arg(rightUsedHalfRoiFallback ? 1 : 0)
                        .arg(leftUsedHalfRoiFallback ? 1 : 0)
                        .arg(rightUsedLowPupilFallback ? 1 : 0)
                        .arg(leftUsedLowPupilFallback ? 1 : 0)
                        .arg(rpupil.center().x, 0, 'f', 1)
                        .arg(rpupil.center().y, 0, 'f', 1)
                        .arg(rpupil.radius(), 0, 'f', 1)
                        .arg(lpupil.center().x, 0, 'f', 1)
                        .arg(lpupil.center().y, 0, 'f', 1)
                        .arg(lpupil.radius(), 0, 'f', 1);
        }
#endif

        PERF_END(getEyes_legacy_lshape_region, QString("getEyesLegacyForCalc lshape region,index=%1,isHumanEye=%2")
                 .arg(num)
                 .arg(isHumanEye ? "true" : "false")
                 .toStdString());
        return std::make_pair(rpupil, lpupil);
    }

    if(!isHumanEye){
        PERF_START(getEyes_legacy_modeleye);
        // 找不到人眼时沿用旧版策略：回退到固定模拟眼框，优先保住瞳孔召回率。
        std::tie(rightEye, leftEye)=getModelEyes();
        {
            ALGO_TIMING_SCOPE(AlgoTimingStage_PupilRight);
            rpupil = createPupilFromRectLegacyForCalc(img, num, rightEye,
                                                      modeleye_wh_ratio, whichEye_Right);
        }
        {
            ALGO_TIMING_SCOPE(AlgoTimingStage_PupilLeft);
            lpupil = createPupilFromRectLegacyForCalc(img, num, leftEye,
                                                      modeleye_wh_ratio, whichEye_Left);
        }

        PERF_END(getEyes_legacy_modeleye,QString("getEyesLegacyForCalc modeleye ,index=%1").arg(num).toStdString());
    } else {
        PERF_START(getEyes_legacy_humaneye);
        // 单侧眼框缺失时保持旧版平移补另一侧眼框，不使用新增 ROI fallback。
        if (cvRectEmpty(rightEye)) {
            int start_x = leftEye.x - IMG_WIDTH / 2;
            rightEye=Rect(start_x, leftEye.y, leftEye.width, leftEye.height);
        }
        {
            ALGO_TIMING_SCOPE(AlgoTimingStage_PupilRight);
            rpupil = createPupilFromRectLegacyForCalc(img, num, rightEye,
                                                      humaneye_wh_ratio, whichEye_Right);
        }

        if (cvRectEmpty(leftEye)) {
            int start_x = rightEye.x + IMG_WIDTH / 2;
            leftEye=Rect(start_x, rightEye.y, rightEye.width, rightEye.height);
        }
        {
            ALGO_TIMING_SCOPE(AlgoTimingStage_PupilLeft);
            lpupil = createPupilFromRectLegacyForCalc(img, num, leftEye,
                                                      humaneye_wh_ratio, whichEye_Left);
        }

        PERF_END(getEyes_legacy_humaneye,QString("getEyesLegacyForCalc humaneye ,index=%1").arg(num).toStdString());
    }
    return std::make_pair(rpupil, lpupil);
}

std::pair<PupilInfo, PupilInfo> getEyes(Mat img,int num,float humaneye_wh_ratio,float modeleye_wh_ratio) {
    PupilInfo rpupil,lpupil;
    Rect rightEye,leftEye;
    PERF_START(detectHumanEyes);
    bool isHumanEye=detectHumanEyes(img,rightEye,leftEye);
    PERF_END(detectHumanEyes,"getEyes detectHumanEyes");

//    std::cout<<"findEyes"<<num<<rightEye<<leftEye<<std::endl;
//    Mat rgbImg;
//    cvtColor(img,rgbImg,COLOR_GRAY2RGB);
//    rectangle(rgbImg,rightEye,Scalar(0,255,255),1);
//    rectangle(rgbImg,leftEye,Scalar(0,255,255),1);
//    imshow("findEyes"+std::to_string(num),rgbImg);

    if (opticalPathType_LShape == g_opticalPathType) {
        PERF_START(getEyes_lshape_region);
        const cv::Rect rightLimit = lShapeEyeSearchRoiForImage(img.size(), whichEye_Right);
        const cv::Rect leftLimit = lShapeEyeSearchRoiForImage(img.size(), whichEye_Left);

        // L 型模式下用固定左右半区作为硬边界，避免单眼平移继承另一只眼的偏移误差。
        rightEye = cvRectEmpty(rightEye) ? rightLimit : (rightEye & rightLimit);
        leftEye = cvRectEmpty(leftEye) ? leftLimit : (leftEye & leftLimit);

        if (cvRectEmpty(rightEye)) {
            rightEye = rightLimit;
        }
        if (cvRectEmpty(leftEye)) {
            leftEye = leftLimit;
        }

        if (!cvRectEmpty(rightEye)) {
            rpupil = createPupilFromRectLegacyForCalc(img, num, rightEye, humaneye_wh_ratio, whichEye_Right);
        }
        if (!cvRectEmpty(leftEye)) {
            lpupil = createPupilFromRectLegacyForCalc(img, num, leftEye, humaneye_wh_ratio, whichEye_Left);
        }

        PERF_END(getEyes_lshape_region, QString("getEyes lshape region,index=%1,isHumanEye=%2")
                 .arg(num)
                 .arg(isHumanEye ? "true" : "false")
                 .toStdString());
        return std::make_pair(rpupil, lpupil);
    }

    if(!isHumanEye){
        PERF_START(getEyes_modeleye);
        //模拟眼一般是校准、实验场合，场景是可以高度定制，因此，我们可以给个确定的小框减小整张图的检测范围
        std::tie(rightEye, leftEye)=getModelEyes();
        rpupil = createPupilFromRectLegacyForCalc(img, num, rightEye, modeleye_wh_ratio, whichEye_Right);
        lpupil = createPupilFromRectLegacyForCalc(img, num, leftEye, modeleye_wh_ratio, whichEye_Left);

        PERF_END(getEyes_modeleye,QString("getEyes modeleye ,index=%1").arg(num).toStdString());
    } else {
        PERF_START(getEyes_humaneye);
        //如果左边或右边缺失，就将右眼或左眼Rect平移half_width个单位
        if (cvRectEmpty(rightEye)) {
            int start_x = leftEye.x - IMG_WIDTH / 2;

            rightEye=Rect(start_x, leftEye.y, leftEye.width, leftEye.height);
        }
        {
            ALGO_TIMING_SCOPE(AlgoTimingStage_PupilRight);
            rpupil = createPupilFromRectLegacyForCalc(img, num, rightEye,
                                                      humaneye_wh_ratio, whichEye_Right);
        }

        if (cvRectEmpty(leftEye)) {
            int start_x = rightEye.x + IMG_WIDTH / 2;

            leftEye=Rect(start_x, rightEye.y, rightEye.width, rightEye.height);
        }
        {
            ALGO_TIMING_SCOPE(AlgoTimingStage_PupilLeft);
            lpupil = createPupilFromRectLegacyForCalc(img, num, leftEye,
                                                      humaneye_wh_ratio, whichEye_Left);
        }

        PERF_END(getEyes_humaneye,QString("getEyes humaneye ,index=%1").arg(num).toStdString());
    }
//    qDebug()<<"rpupil="<<QString::fromStdString(rpupil.toString())<<endl;
//    qDebug()<<"lpupil="<<QString::fromStdString(lpupil.toString())<<endl;
    return std::make_pair(rpupil, lpupil);
}

namespace {

bool convertTraditionalPupil(PupilInfo pupil,
                             enWhichEye whichEye,
                             stPupilInfo& output)
{
    output = stPupilInfo();
    const cv::Point2f center = pupil.center();
    const float radius = pupil.radius();
    if (!std::isfinite(center.x) || !std::isfinite(center.y)
            || !std::isfinite(radius) || radius < 6.0f || radius > 64.0f
            || !isNormalPupil(center, whichEye)) {
        return false;
    }

    output = pupil.getPupilInfoStruct();
    output.rect = cv::Rect(cvRound(center.x - radius),
                           cvRound(center.y - radius),
                           cvRound(radius * 2.0f),
                           cvRound(radius * 2.0f));
    output.area = CV_PI * radius * radius;
    output.perimeter = 2.0 * CV_PI * radius;
    output.circularity = 1.0;
    output.fallbackType = PupilFallback_TraditionalRoi;
    output.eyeRectSource = static_cast<int>(pupil.eyeRectSource());
    return true;
}

} // namespace

bool locatePupilPairByTraditionalEyePath(const cv::Mat& image,
                                         int imageNumber,
                                         float humaneyeWhRatio,
                                         float modeleyeWhRatio,
                                         TraditionalPupilPairResult& result)
{
    const auto start = std::chrono::steady_clock::now();
    result = TraditionalPupilPairResult();
    if (image.empty() || image.channels() != 1) {
        return false;
    }

    // 传统接口本身会同时处理双眼；本函数保证同一照片只调用一次。
    const std::pair<PupilInfo, PupilInfo> pupils = getEyesLegacyForCalc(
            image, imageNumber, humaneyeWhRatio, modeleyeWhRatio, true);
    result.rightValid = convertTraditionalPupil(
            pupils.first, whichEye_Right, result.right);
    result.leftValid = convertTraditionalPupil(
            pupils.second, whichEye_Left, result.left);
    result.elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
    return result.rightValid || result.leftValid;
}

bool locatePupilByTraditionalEyePath(const cv::Mat& image,
                                     int imageNumber,
                                     enWhichEye whichEye,
                                     float humaneyeWhRatio,
                                     float modeleyeWhRatio,
                                     stPupilInfo& output)
{
    TraditionalPupilPairResult result;
    locatePupilPairByTraditionalEyePath(image, imageNumber,
                                        humaneyeWhRatio, modeleyeWhRatio,
                                        result);
    if (whichEye == whichEye_Right) {
        if (!result.rightValid) {
            output = stPupilInfo();
            return false;
        }
        output = result.right;
        return true;
    }
    if (!result.leftValid) {
        output = stPupilInfo();
        return false;
    }
    output = result.left;
    return true;
}
