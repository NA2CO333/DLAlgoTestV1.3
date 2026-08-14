#ifndef ALGO_UTILS_H
#define ALGO_UTILS_H

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <atomic>
#include <chrono>
#include <QString>
#include "ransac.h"
#include "algointf.h"
#include "curvedata.h"
#include "perftimer.h"
#include "haar_thread_safe.h"

// 每次转灯/帧数（业务数量）
static constexpr int FRAMES_PER_ROUND = 22;
// 底层数组容量（因为采用 1-based 索引，所以需要 +1 空出第 0 号位置）
static constexpr int FRAME_ARRAY_SIZE = FRAMES_PER_ROUND + 1;
// 图像宽度
#define IMG_WIDTH   1280
// 图像高度
#define IMG_HEIGHT  512
//
#if (CAMERA_TYPE == 1)
    // 像元系数，即每个像素所对应实物长度的比例，以迈德威视相机为 1
    #define PIX_COEF  1
    #define CCD_PIXEL_SIZE 0.006

    static const int WIDTH_HALF_ACCU = 32;

#else
    // 像元系数，同样的实物长度，度申相机的对应像素数是迈德威视相机的 1.5 倍
    #define PIX_COEF  1.5
    //CCD像素尺寸，单位mm
    #define CCD_PIXEL_SIZE 0.004
#endif

//像元尺寸，950mm物距，35mm像距(焦距），像距/物距为放大率，ccd像素尺寸/放大率即为像元所代表的物理尺寸，单位mm
#define OBJECT_DISTANCE 950.0f //物距
#define IMAGE_DISTANCE 35.0f   //像距
constexpr float BETA=IMAGE_DISTANCE/OBJECT_DISTANCE; //放大率
//计算可得一个像素尺寸代表的实际物理长度为0.1085714285714286mm
constexpr float PIXEL_TO_PHY = CCD_PIXEL_SIZE/BETA;

#define KROIH 5 * PIX_COEF
#define KROIW 11 * PIX_COEF

//129*129的瞳孔裁剪区，中心到上下边界均为129/2=64
#define ROI_WIDTH 129
#define ROI_HEIGHT 129
#define ROI_WIDTH_HALF ROI_WIDTH/2        // 精确识别时截图宽度的一半，64 * 2 + 1 == 129，与 Python 算法一致
#define ROI_HEIGHT_HALF ROI_HEIGHT/2

const double EPSILON = 1e-6; // 用于浮点数比较的小量
const double DIVISOR_MIN = 1e-3; // 除数最小值，避免除以0

static constexpr double PTOSIS_THRESH = 0.75;       // 上睑下垂的判断阈值

/*
 * 最小瞳孔直径2.5mm，最大瞳孔直径10mm，一个像元对应现实世界约0.1mm
 * 用瞳孔面积来约束找到的轮廓
*/
static constexpr double MIN_PD=2.0f/PIXEL_TO_PHY;
static constexpr double MAX_PD=10.0f/PIXEL_TO_PHY;
static constexpr double MIN_PUPIL_AREA=M_PI*(MIN_PD/2)*(MIN_PD/2);
static constexpr double MAX_PUPIL_AREA=M_PI*(MAX_PD/2)*(MAX_PD/2);

static const int MAX_SPOT_COUNTER=300;

// 日志标签
#define LOG_TAG "algo"

/**
 * @brief 斜率计算配置结构体
 *
 * 用于描述单个斜率计算的配置参数，包括使用的图像索引、符号和描述信息
 */
struct SlopeCalculationConfig {
    int imageIndex1;        ///< 第一个图像的索引（被除数图像）
    int imageIndex2;        ///< 第二个图像的索引（除数图像）
    double sign;           ///< 符号系数：1.0 表示正号，-1.0 表示负号
    const char* description; ///< 描述信息，用于结果输出显示

    // 构造函数，便于初始化
    SlopeCalculationConfig(int idx1, int idx2, double s, const char* desc)
        : imageIndex1(idx1), imageIndex2(idx2), sign(s), description(desc) {}
};

//辅助函数
//记录微秒
inline void logMicroSecond(const std::string& method,
                       std::chrono::high_resolution_clock::time_point start) {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << method << " cost:" << duration.count() << " us" << std::endl;
}

//记录毫秒
inline void logMillSecond(const std::string& method,
                       std::chrono::high_resolution_clock::time_point start) {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << method << " cost:" << duration.count() << " ms" << std::endl;
}


// 类型转换（声明）
CvRect CRect_to_CvRect(const CRect &_c_rect);
CRect CvRect_to_CRect(const CvRect &_cv_rect);
CvPoint2D32f CPointF_to_CvPoint2D32f(CPointF _c_point);
CPointF CvPoint2D32f_to_CPointF(CvPoint2D32f _cv_point);

// 图像处理函数
void SpotEnhance(IplImage *src, IplImage *dst);
void lhMorpFillHole(const IplImage *src, IplImage *dst);
void lhMorpRErode(const IplImage *src, const IplImage *msk, IplImage *dst, IplConvKernel *se, int iterations);
int lhImageCmp(const IplImage *_img1, const IplImage *_img2);
void FillInternalContours(IplImage *pBinary, double dAreaThre);
int ConnectEdge(IplImage *src);

// 图像转换和预处理
cv::Mat binaryImage(const cv::Mat& srcImg);
cv::Mat binaryImageOptimized(const cv::Mat& srcImg);

// 模型/跟踪坐标只作为搜索起点，最终坐标必须由当前图片的小 ROI 轮廓确认。
// fallbackLevel：0=失败，1=129 ROI，2=废弃（193 ROI已移除），3=当前图传统兜底。
struct PupilRoiRefineResult
{
    bool valid = false;
    cv::Point2f predictedCenter;
    cv::Point2f refinedCenter;
    float predictedRadius = 0.0f;
    float refinedRadius = 0.0f;
    float centerShift = 0.0f;
    double contourRatio = 0.0;
    double contourArea = 0.0;
    double contourCircularity = 0.0;
    int roiSize = 0;
    int fallbackLevel = 0;
    double elapsedMs = 0.0;
    // 同一张照片左右眼共享一次完整传统定位时，用以下字段避免耗时重复统计。
    bool sharedFallback = false;
    double sharedFallbackElapsedMs = 0.0;
    QString sharedFallbackCallId;
    QString rejectReason;
};

bool refinePupilInPredictedRoi(const cv::Mat& image,
                               int imageNumber,
                               enWhichEye whichEye,
                               const cv::Point2f& predictedCenter,
                               float predictedRadius,
                               float whRatioThreshold,
                               PupilRoiRefineResult& result);

inline bool cvRectEmpty(const cv::Rect& rect) {
    return rect.width <= 0 || rect.height <= 0;
}

//注意，这里取gamma的倒数，跟python一致
cv::Mat adjustGamma(cv::Mat _img, double gamma=1.0);


/**
 * @brief 从轮廓列表中筛选符合面积和长宽比要求的轮廓
 * @param contours 输入轮廓列表
 * @param minArea 最小面积阈值
 * @param maxArea 最大面积阈值
 * @param whRatio 长宽比阈值
 * @return std::vector<std::vector<cv::Point>> 符合条件的轮廓副本列表
 */
std::vector<std::vector<cv::Point>> filterContoursByAreaAndRatio(
    const std::vector<std::vector<cv::Point>>& contours,
    double minArea,
    double maxArea,
    double whRatio);

/**
 * @brief 从过滤后的轮廓中选择最佳候选瞳孔轮廓
 * @param candidateContours 候选轮廓列表
 * @param glint 映光点坐标（用于多轮廓情况下的最终选择）
 * @param _pupil_info 输出瞳孔信息
 * @return bool 选择是否成功
 */
bool selectBestPupilContour(
    const std::vector<std::vector<cv::Point>>& candidateContours,
    cv::Point2f glint,
    stPupilInfo& _pupil_info);

/**
 * @brief 在图像中查找瞳孔
 * @param img 输入图像 (通常为眼部 ROI)
 * @param num 图片序号 (仅用于日志打印)
 * @param _center 输出找到的瞳孔中心坐标
 * @param _radius 输出找到的瞳孔半径
 * @param wh_ratio_threshold 长宽比阈值
 * @return bool 是否成功找到有效瞳孔
 */
bool findPupilLegacyForCalc(cv::Mat img, int num, cv::Point2f& _center, float& _radius,
                            float wh_ratio_threshold,
                            double* outArea = nullptr,
                            double* outContourRatio = nullptr,
                            double* outCircularity = nullptr);



// 瞳孔和光斑检测
//根据技术指标，瞳孔尺寸3.2mm~9mm，瞳孔距离35mm~80mm,上下左右预留一定的位置
std::pair<cv::Rect, cv::Rect> getModelEyes();

std::pair<cv::Rect, cv::Rect> haarDetectEyes(const cv::Mat& _img,const double scaleFactor, int miniNeighbors,const float scale);

bool detectHumanEyes(const cv::Mat& image, cv::Rect& rightEye, cv::Rect& leftEye,
                     bool allowRelaxedFallback = true);

bool isNormalPupil(cv::Point2f _pc, enWhichEye _whichEye);
int getImageAngle(int img_idx);
void rotateImage(const cv::Mat& img, cv::Mat& img_rotate, int degree);
bool getGlintBlobPoint(const cv::Mat &_img,
                           cv::Point &_point,
                           enWhichEye whichEye,
                           double glintThreshold,
                           int borderMargin);
void reduceGlintBlob(const cv::Mat &src, const cv::Point &spot, cv::Mat &dst);
cv::Mat setPupilImgRoi(const cv::Mat& _img_pupil, double _radius);
bool selectContour(std::vector<std::vector<cv::Point>>& contours, stPupilInfo& _pupil_info, cv::Point2f glint, float wh_ratio_threshold);
bool pupilCenter(const cv::Mat& pupilImg, stPupilInfo& _pupil_info, enWhichEye _which_eye);
bool isValidPupilRadius(float radius);

void accLocPupil(cv::Mat img, int _img_idx, stPupilInfo &_pupil_info_r, bool is_need_right, stPupilInfo &_pupil_info_l, bool is_need_left);
bool accOnePupil(cv::Mat img, stPupilInfo &_pupil_info);

bool processPicOfOneEye(const cv::Mat& inputImg,
                             int _img_idx,
                             stPupilInfo& _pupil_info,
                             int _angle,
                             enWhichEye _which_eye,
                             cv::Mat& out_processedImg); // 新增：用于返回处理后的图像

void logPupilFailDetail(const cv::Mat& img,
                        int num,
                        const cv::Rect& eyeRect,
                        enWhichEye whichEye,
                        float whRatioThreshold,
                        const QString& stage,
                        bool forceLog = false);
void setPupilFailDetailForceLog(bool enabled);
void setPupilFailDetailRoundLogLimiter(std::atomic<int>* counter, int maxLogsPerRound);
bool shouldLogPupilFailDetail();

// 圆度计算 cv::InputArray 作为参数类型，增强通用性
inline double roundNess(cv::InputArray contour) {
    double area = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);
    return (4 * CV_PI * area) / (perimeter * perimeter); // OpenCV定义了CV_PI
}

//返回平均矩形
cv::Rect averageRect(const std::vector<cv::Rect>& rects);

cv::Rect maxRect(std::vector<cv::Rect>& rects);

// 内联函数，根据传入的眼睛模式返回是否需要右眼和左眼渲染
// 参数 _single_dual_eye: 眼睛模式枚举值
// 返回值: std::pair<bool, bool>，first表示是否需要右眼，second表示是否需要左眼
inline std::pair<bool, bool> get_eye_flags(enSingleDualEyeMode  _single_dual_eye) {
    bool is_need_right = (_single_dual_eye & singleDualEyeMode_Right); // 检查是否需要右眼渲染
    bool is_need_left = (_single_dual_eye & singleDualEyeMode_Left); // 检查是否需要左眼渲染

    return std::make_pair(is_need_right, is_need_left);
}

// 处理瞳孔ROI区域
template<typename ImageArrayType>
void processPupilROI(enSingleDualEyeMode _single_dual_eye,
                     const ImageArrayType& pupilImgRight, double pupilRadiusAvgRight,
                     std::array<cv::Mat, FRAME_ARRAY_SIZE>& pupilROIImgRight,
                     const ImageArrayType& pupilImgLeft, double pupilRadiusAvgLeft,
                     std::array<cv::Mat, FRAME_ARRAY_SIZE>& pupilROIImgLeft)
{
    auto eye_flags = get_eye_flags(_single_dual_eye);

    if (eye_flags.first) {
        for (int i = 1; i <= FRAMES_PER_ROUND; i++) {
            // 对于 array，直接用索引；对于 map，需要判断
            // 统一用 empty() 判断
            if (!pupilImgRight[i].empty()) {
                pupilROIImgRight[i] = setPupilImgRoi(pupilImgRight[i], pupilRadiusAvgRight);
            }
        }
    }

    if (eye_flags.second) {
        for (int i = 1; i <= FRAMES_PER_ROUND; i++) {
            if (!pupilImgLeft[i].empty()) {
                pupilROIImgLeft[i] = setPupilImgRoi(pupilImgLeft[i], pupilRadiusAvgLeft);
            }
        }
    }
}

//屈光计算
double compareEach(cv::Mat& img1, cv::Mat& img2);
double compareEach(cv::Mat& img1, cv::Mat& img2, const RANSACParams& params);

void printSlopeResults(const std::string& eyeSide,
                             double* results,
                             const std::vector<SlopeCalculationConfig>& configs,
                             const std::vector<int>& standardIndices);

template<typename ImageArrayType>
void calculateAllSlopes(double* result, const ImageArrayType& imageArray,
                       const std::vector<SlopeCalculationConfig>& configs,
                       const std::vector<int>& standardIndices, bool isRightEye);

/**
 * @brief 计算所有斜率的核心模板函数
 *
 * 这个函数通过配置驱动的方式计算多个斜率值，支持左右眼的对称计算
 * 使用模板参数以便支持不同类型的图像数组容器
 *
 * @tparam ImageArrayType 图像数组类型，需要支持operator[]和size()方法
 * @param result 计算结果输出数组，长度为10（前9个普通斜率+第10个标准斜率）
 * @param imageArray 图像数组容器，包含所有可用的图像
 * @param configs 斜率计算配置数组，描述前9个斜率的计算方式
 * @param standardIndices 标准斜率计算的4个图像索引
 * @param isRightEye 标识是否为右眼计算（影响标准斜率的计算公式）
 *
 * @note 标准斜率的4个索引含义：[idx1, idx2, idx3, idx4] 对应：
 *       - 右眼：(imageArray[idx1]/imageArray[idx2] - imageArray[idx3]/imageArray[idx4]) / 2
 *       - 左眼：(-imageArray[idx1]/imageArray[idx2] + imageArray[idx3]/imageArray[idx4]) / 2
 */
template<typename ImageArrayType>
void calculateAllSlopes(double* result,
                              const ImageArrayType& imageArray,
                              const std::vector<SlopeCalculationConfig>& configs,
                              const std::vector<int>& standardIndices,
                              bool isRightEye)
{
    PERF_SCOPE("calculateAllSlopes");

    // ==================== 第一步：计算前9个普通斜率 ====================
    // 遍历配置数组，每个配置对应一个斜率的计算
    for (size_t i = 0; i < configs.size() && i < 9; ++i) {
        const auto& config = configs[i];  // 获取当前配置

        // 安全检查：确保图像索引在有效范围内
        if (config.imageIndex1 < imageArray.size() &&
            config.imageIndex2 < imageArray.size()) {

            // 核心计算：调用compareEach计算两个图像的斜率比值
            // 注意：参数顺序是 (被除数图像, 除数图像)
            // 修正：从QMap中获取图像，并处理const问题
            cv::Mat img1 = imageArray.at(config.imageIndex1);
            cv::Mat img2 = imageArray.at(config.imageIndex2);
            double slope = compareEach(img1,  // 被除数图像
                                     img2,  // 除数图像
                                     DEFAULT_PARAMS);                 // 使用默认RANSAC参数

            // 应用符号系数：根据左右眼的对称性调整正负号
            result[i] = config.sign * slope;

        } else {
            // 索引越界处理：设置为0并记录警告
            result[i] = 0.0;
            std::cerr << "Warning: Invalid image index in config " << i
                      << " (indices: " << config.imageIndex1 << ", "
                      << config.imageIndex2 << ")" << std::endl;
        }
    }

    // ==================== 第二步：计算第10个标准斜率 ====================
    // 标准斜率需要4个图像索引，进行组合计算
    if (standardIndices.size() == 4) {
        // 验证所有索引的有效性
        bool indicesValid = true;
        for (int idx : standardIndices) {
            if (idx > imageArray.size()) {
                indicesValid = false;
                std::cerr << "Error: Invalid standard slope index " << idx << std::endl;
                break;
            }
        }

        if (indicesValid) {
            cv::Mat img1 = imageArray.at(standardIndices[0]);
            cv::Mat img2 = imageArray.at(standardIndices[1]);
            cv::Mat img3 = imageArray.at(standardIndices[2]);
            cv::Mat img4 = imageArray.at(standardIndices[3]);
            // 计算第一个斜率比值：standardIndices[0] / standardIndices[1]
            double slope1 = compareEach(img1, img2,DEFAULT_PARAMS);  // 两参数调用
            // 计算第二个斜率比值：standardIndices[2] / standardIndices[3]
            double slope2 = compareEach(img3, img4,DEFAULT_PARAMS);  // 两参数调用
            // 根据左右眼选择不同的计算公式
            if (isRightEye) {
                // 右眼公式：(slope1 - slope2) / 2
                result[9] = (slope1 - slope2) / 2.0;
            } else {
                // 左眼公式：(-slope1 + slope2) / 2
                // 注意：左眼的符号与右眼相反，体现对称性
                result[9] = (-slope1 + slope2) / 2.0;
            }
        } else {
            // 索引无效时设置为0
            result[9] = 0.0;
        }
    } else {
        // 参数错误处理
        result[9] = 0.0;
        std::cerr << "Error: Standard slope calculation requires exactly 4 indices, got "
                  << standardIndices.size() << std::endl;
    }
}

template<typename ImageArrayType>
void getDSR(const ImageArrayType& r_pupilimgs,double *DSR);

/**
 * @brief 右眼斜率计算函数
 *
 * 计算右眼(DSR)的10个斜率值，包括9个普通斜率和1个标准斜率
 * 右眼的计算特点是图像索引顺序和符号与左眼呈镜像对称
 *
 * @param DSR 输出数组，长度为10，存储右眼斜率计算结果
 */
template<typename ImageArrayType>
void getDSR(const ImageArrayType& r_pupilimgs,double *DSR)
{
    // ==================== 右眼斜率计算配置 ====================
    // 配置说明：每个配置项格式为 {索引1, 索引2, 符号, 描述}
    // 右眼特点：索引顺序较大/较小，符号多为负号（与左眼相反）
    static const std::vector<SlopeCalculationConfig> configs = {
        // 前6个：负号斜率组（对应左眼的正号组）
        {6, 5, -1.0, "[06/05]"},  // 图像6除以图像5，结果取负
        {4, 3, -1.0, "[04/03]"},  // 图像4除以图像3，结果取负
        {2, 1, -1.0, "[02/01]"},  // 图像2除以图像1，结果取负
        {12, 11, -1.0, "[12/11]"}, // 图像12除以图像11，结果取负
        {10, 9, -1.0, "[10/09]"}, // 图像10除以图像9，结果取负
        {8, 7, -1.0, "[08/07]"},  // 图像8除以图像7，结果取负

        // 后3个：正号斜率组（对应左眼的负号组）
        {17, 18, 1.0, "[17/18]"},  // 图像17除以图像18，结果取正
        {15, 16, 1.0, "[15/16]"},  // 图像15除以图像16，结果取正
        {13, 14, 1.0, "[13/14]"}   // 图像13除以图像14，结果取正
    };

    // ==================== 标准斜率计算索引 ====================
    // 右眼标准斜率：([19/20] - [22/21]) / 2
    // 索引顺序对应：19→idx0, 20→idx1, 22→idx2, 21→idx3
    std::vector<int> standardIndices = {19, 20, 22, 21};

    // ==================== 执行计算 ====================
    calculateAllSlopes(DSR,              // 结果输出数组
                       r_pupilimgs,     // 右眼图像数组
                       configs,          // 右眼计算配置
                       standardIndices,  // 标准斜率索引
                       true);            // 标识为右眼计算
    // ==================== 输出结果 ====================
//    printSlopeResults("right",    // 标识为右眼结果
//                     DSR,         // 斜率结果数组
//                     configs,      // 配置信息（用于输出描述）
//                     standardIndices); // 标准斜率索引（用于输出描述）
}

template<typename ImageArrayType>
void getDSL(const ImageArrayType& l_pupilimgs,double* DSL);
/**
 * @brief 左眼斜率计算函数
 *
 * 计算左眼(DSL)的10个斜率值，与右眼呈镜像对称关系
 * 左眼的图像索引顺序和符号与右眼相反，体现双眼的对称性
 *
 * @param DSL 输出数组，长度为10，存储左眼斜率计算结果
 */
template<typename ImageArrayType>
void getDSL(const ImageArrayType& l_pupilimgs,double* DSL)
{
    // ==================== 左眼斜率计算配置 ====================
    // 左眼特点：索引顺序与右眼相反，符号与右眼相反
    static const std::vector<SlopeCalculationConfig> configs = {
        // 前6个：正号斜率组（对应右眼的负号组）
        {5, 6, 1.0, "[05/06]"},   // 图像5除以图像6，结果取正（右眼是6/5取负）
        {3, 4, 1.0, "[03/04]"},   // 图像3除以图像4，结果取正（右眼是4/3取负）
        {1, 2, 1.0, "[01/02]"},   // 图像1除以图像2，结果取正（右眼是2/1取负）
        {11, 12, 1.0, "[11/12]"}, // 图像11除以图像12，结果取正（右眼是12/11取负）
        {9, 10, 1.0, "[09/10]"},  // 图像9除以图像10，结果取正（右眼是10/9取负）
        {7, 8, 1.0, "[07/08]"},   // 图像7除以图像8，结果取正（右眼是8/7取负）

        // 后3个：负号斜率组（对应右眼的正号组）
        {18, 17, -1.0, "[18/17]"}, // 图像18除以图像17，结果取负（右眼是17/18取正）
        {16, 15, -1.0, "[16/15]"}, // 图像16除以图像15，结果取负（右眼是15/16取正）
        {14, 13, -1.0, "[14/13]"}  // 图像14除以图像13，结果取负（右眼是13/14取正）
    };

    // ==================== 标准斜率计算索引 ====================
    // 左眼标准斜率：([21/22] - [20/19]) / 2
    // 注意：索引顺序与右眼不同，体现对称性
    std::vector<int> standardIndices = {20, 19, 21, 22};

    // ==================== 执行计算 ====================
    calculateAllSlopes(DSL,             // 结果输出数组
                       l_pupilimgs,     // 左眼图像数组
                       configs,          // 左眼计算配置
                       standardIndices,  // 标准斜率索引
                       false);           // 标识为左眼计算（非右眼）

    // ==================== 输出结果 ====================
//    printSlopeResults("left",     // 标识为左眼结果
//                     DSL,         // 斜率结果数组
//                     configs,      // 配置信息
//                     standardIndices); // 标准斜率索引
}

std::tuple<double,double,double> calRef(enAgeRange _age_range,bool isHmMode, double *DS);
void getRefCurve(enAgeRange _age_range, bool isHmMode, double **_p1, double **_p2, double **_p3, double **_p4, int curve = 2);

// 单轮单眼的标定曲线匹配质量，仅供诊断日志使用，不参与当前结果判定。
struct RefractionFitDiagnostics {
    double diopter[3] = {0.0, 0.0, 0.0};
    double minError[3] = {0.0, 0.0, 0.0};
    double rmsError[3] = {0.0, 0.0, 0.0};
    double localSharpness[3] = {0.0, 0.0, 0.0};
    double alternativeGap[3] = {0.0, 0.0, 0.0};
    double signedResidual[3][4] = {};
    double totalMinError = 0.0;
    double worstMinError = 0.0;
    int worstDirection = -1;
    int bestIndex[3] = {0, 0, 0};
};

std::tuple<double,double,double> calRefraction(enAgeRange _age_range,
                                               bool isHmMode,
                                               const double *DS,
                                               RefractionFitDiagnostics *diagnostics = nullptr);
std::tuple<double, double, int> calABD(double D0, double D60, double D120);

enCalcResultState calculatePupilAverageSpot(
    enSingleDualEyeMode _single_dual_eye,
    int maxGazeDeviation,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoRight,
    const std::bitset<FRAME_ARRAY_SIZE>& validRight,
    CPointF& pupilSpotAvgRight,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoLeft,
    const std::bitset<FRAME_ARRAY_SIZE>& validLeft,
    CPointF& pupilSpotAvgLeft);

void calculatePupilAverageRadius(
    enSingleDualEyeMode _single_dual_eye,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoRight,
    const std::bitset<FRAME_ARRAY_SIZE>& validRight,
    double& avgR,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoLeft,
    const std::bitset<FRAME_ARRAY_SIZE>& validLeft,
    double& avgL);

double calcPupilDistance(
    enSingleDualEyeMode _single_dual_eye,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoRight,
    const std::bitset<FRAME_ARRAY_SIZE>& validRight,
    const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoLeft,
    const std::bitset<FRAME_ARRAY_SIZE>& validLeft);


// 工具函数
LineModel fitLineRANSAC(const std::vector<double>& xData, const std::vector<double>& yData,
                       const RANSACParams& params, std::vector<bool>& inlierMask);

void drawEnhancedLineChart(const double* data, int dataSize, const QString& windowName ,
                          int width, int height);

void concatenate_arrays_to_array(const double* arr1, size_t arr1_size, size_t start1, size_t end1,
                                const double* arr2, size_t arr2_size, size_t start2, size_t end2,
                                const double* arr3, size_t arr3_size, size_t start3, size_t end3,
                                double* result, size_t result_size);

int findClosestIndex(const double* array1, const double* array2, int size, double target1, double target2);
// 新增辅助函数：统计数组中非空图像的数量
int getValidImageCount(const std::map<int, cv::Mat>& imageMap, int expected_count);

//瞳孔类
class PupilInfo
{

public:
    PupilInfo(){
        pc={0,0};
        spot={0,0};
        dx=0;
        dy=0;
        r=0;
        num=0;
        whichEye=whichEye_Right;
        fallbackTypeValue=PupilFallback_None;
        eyeRectSourceValue=PupilEyeRect_Base;
    };

    PupilInfo(int _num, enWhichEye _whichEye, cv::Point2f _pc, float _radius, cv::Point2f _spot,
              PupilFallbackType _fallbackType = PupilFallback_None,
              PupilEyeRectSource _eyeRectSource = PupilEyeRect_Base,
              bool _hasReliableGlint = false,
              cv::Point2f _reliableGlint = cv::Point2f()) {
        num = _num;
        whichEye = _whichEye;
        pc = _pc;
        r = _radius;
        spot = _spot;
        dx = spot.x - pc.x;
        dy = spot.y - pc.y;
        fallbackTypeValue = _fallbackType;
        eyeRectSourceValue = _eyeRectSource;
        reliableGlintAvailable = _hasReliableGlint;
        reliableGlintPoint = _reliableGlint;

        if (!isNormalPupil(pc, whichEye)) {
            qDebug() << QString("图片%1 瞳孔超出拍摄边界,瞳孔中心坐标(%2,%3))").arg(num).arg(pc.x).arg(pc.y);
        }
    };

    float radius(){
        return r;
    };

    cv::Point2f center(){
        return pc;
    };

    int pd(){
        return r*2;
    };

    //偏心灯的角度
    int angle(int i){
        return getImageAngle(i);
    };

    //根据论文，映光点每偏离1个毫米，对应的固视角度偏离是7°
    void degree(float& dg_x,float& dg_y){
        dg_x=7*dx*PIXEL_TO_PHY;
        dg_y=7*dy*PIXEL_TO_PHY;
    };

    float dist(){
        return sqrt(dx*dx+dy*dy);
    };

    PupilFallbackType fallbackType() const {
        return fallbackTypeValue;
    };

    PupilEyeRectSource eyeRectSource() const {
        return eyeRectSourceValue;
    };

    void setEyeRectSource(PupilEyeRectSource source) {
        eyeRectSourceValue = source;
    };

    bool hasReliableGlint() {
        return reliableGlintAvailable;
    };

    cv::Point2f reliableGlint() {
        return reliableGlintPoint;
    };

    stPupilInfo getPupilInfoStruct(){
        stPupilInfo pi;
        pi.center.x=pc.x;
        pi.center.y=pc.y;
        pi.spotPt.x=spot.x;
        pi.spotPt.y=spot.y;
        pi.radius=r;
        pi.fallbackType = static_cast<int>(fallbackTypeValue);
        pi.eyeRectSource = static_cast<int>(eyeRectSourceValue);
        return pi;
    };

    std::string toString() const {
        std::ostringstream oss;

        oss << "PupilInfo Object:" << std::endl;
        oss << "  whichEye: " << (whichEye == whichEye_Left ? "LEFT_EYE" : "RIGHT_EYE") << std::endl;
        oss << "  num: " << num << std::endl;
        oss << "  pupilCenter: (" << pc.x << ", " << pc.y << ")" << std::endl;
        oss << "  radius: " << r << std::endl;
        oss << "  spot: (" << spot.x << ", " << spot.y << ")" << std::endl;

        if (r > 0) {
            // 计算偏心距离
            float dist = sqrt(pow(spot.x - pc.x, 2) + pow(spot.y - pc.y, 2));
            oss << "  eccentricity: " << dist << " pixels" << std::endl;

            // 计算对应的角度
            float dg_x = 0, dg_y = 0;
            const_cast<PupilInfo*>(this)->degree(dg_x, dg_y);
            oss << "  deviation_angle: (" << dg_x << "°, " << dg_y << "°)" << std::endl;
        }

        oss << "  ROI尺寸: " << ROI_WIDTH << "x" << ROI_HEIGHT;

        return oss.str();
    }

    enWhichEye whichEye;
private:
    int num;
    cv::Point2f pc;
    float r;
    cv::Point2f spot;
    float dx,dy;
    PupilFallbackType fallbackTypeValue;
    PupilEyeRectSource eyeRectSourceValue;
    bool reliableGlintAvailable {false};
    cv::Point2f reliableGlintPoint {0.0f, 0.0f};
};


PupilInfo createPupilFromRectLegacyForCalc(cv::Mat img, int num, cv::Rect eyeRect,
                                           float wh_ratio_threshold, enWhichEye whichEye);
PupilInfo createPupilFromRectForSimulatedEye(cv::Mat img, int num, cv::Rect eyeRect,
                                             float wh_ratio_threshold, enWhichEye whichEye);

std::pair<PupilInfo, PupilInfo> getEyes(cv::Mat img,int num,float humaneye_wh_ratio,float modeleye_wh_ratio);
std::pair<PupilInfo, PupilInfo> getEyesLegacyForCalc(cv::Mat img,int num,float humaneye_wh_ratio,float modeleye_wh_ratio,
                                                     bool isCalcVision = false);

// 同一张照片只执行一次完整传统双眼路径，供正式流程和测试工具共享兜底结果。
struct TraditionalPupilPairResult
{
    bool rightValid = false;
    bool leftValid = false;
    stPupilInfo right;
    stPupilInfo left;
    double elapsedMs = 0.0;
};

bool locatePupilPairByTraditionalEyePath(const cv::Mat& image,
                                         int imageNumber,
                                         float humaneyeWhRatio,
                                         float modeleyeWhRatio,
                                         TraditionalPupilPairResult& result);

// 兼容旧调用方的单眼包装；正式批量流程应使用上面的双眼接口。
bool locatePupilByTraditionalEyePath(const cv::Mat& image,
                                     int imageNumber,
                                     enWhichEye whichEye,
                                     float humaneyeWhRatio,
                                     float modeleyeWhRatio,
                                     stPupilInfo& output);

#endif // ALGO_UTILS_H
