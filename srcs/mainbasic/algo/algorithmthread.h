#ifndef ALGORITHMTHREAD_H
#define ALGORITHMTHREAD_H


#include <QObject>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <QVector>
#include <QStringList>
#include <QString>
#include <QList>
#include <QMap>
#include <QThreadPool>
#include <QDebug>
#include <QElapsedTimer>

#ifndef UNIT_TEST
#  include "mysqlitepatients.h"
#  include "serialdatatrans.h"
#else
#  include "virtualinterface.h"
#endif

//
extern int g_lastDetectTime;
//
extern int g_PupilState;

//
//20190724V01
#define CURVE_VERSION   "20200520V1.0"

//
#if (CAMERA_TYPE == 1)
// 像元系数，即每个像素所对应实物长度的比例，以迈德威视相机为 1
#  define PIX_COEF  1
#else
// 像元系数，同样的实物长度，度申相机的对应像素数是迈德威视相机的 1.5 倍
#  define PIX_COEF  1.5
#endif

// 像素长度单位转换为物理长度单位（毫米）的系数，即每个像素对应的毫米数，迈德威视相机的 = 120(mm) / 752(pixel)
#define PIX_TO_PHY      0.15957446 / PIX_COEF

// 哪一只眼
enum enWhichEye {
    whichEye_Right  = 1,
    whichEye_Left,

    //whichEye_Min    = whichEye_Right,
    //whichEye_Max    = whichEye_Left,
};

// 单双眼模式
enum enSingleDoubleEyeMode {
    singleDoubleEyeMode_Right   = 1,    // 右眼模式
    singleDoubleEyeMode_Left    = 2,    // 左眼模式
    singleDoubleEyeMode_Both    = 3,    // 双眼模式

    singleDoubleEyeMode_Min = singleDoubleEyeMode_Right,
    singleDoubleEyeMode_Max = singleDoubleEyeMode_Both,
};

//
const double MAX_CYL_NORMAL = -4.0F;    // 最大柱镜度（非高度数模式）
const double MAX_CYL_HMMODE = -7.5F;    // 最大柱镜度（高度数模式）（high myopia）

// 算法模式
enum enAlgoMode {
    GeneralAlgo,        // 普通模式
    ProfessionalAlgo,   // 专业模式
};

// 算法错误类别（瞳孔识别或屈光计算）
enum enAlgoErrType {
    algoErrType_DetectPupil,        // 瞳孔识别
    algoErrType_CalcVision,         // 屈光计算
};

// 屈光计算结果状态
enum enCalcResultState {
    calcResultState_Unknown     = -1,
    calcResultState_Succ        = 0,
    calcResultState_GazeOver,               // 眼位超过最大值
    calcResultState_Blinked,                // 眨眼了
    calcResultState_ValueUnnormal,          // 值异常
    calcResultState_ProgramException,       // 发生程序异常
};

//
class ResultAnalysist{
public:
    ResultAnalysist();
    void GetStableResult(double &_sphL, double &_cylL, double &_sphR, double &_cylR);
    void loadResult(double _sphL, double _cylL, double _sphR, double _cylR,int _Pd, double _leftPd, double _rightPd);
    bool judgePatient(int _Pd,double _leftPd,double _rightPd);
    void clear();

private:
    double lastSphL,lastCylL,lastSphR,lastCylR,lastLeftPd,lastRightPd;
    int lastPd;
    bool judgeState;
};

// 瞳孔信息（单位：像素）
struct stPupilInfo {
    CvRect          rect;               // 瞳孔的边界矩形
    double          area = -1;          // 瞳孔面积
    double          perimeter = -1;     // 周长
    CvPoint2D32f    center;             // 瞳孔中心坐标
    double          radius = -1;        // 瞳孔半径
    double          circularity = -1;   // 圆度（ = (4 * pi * area) / (perimeter ^ 2)）
    CvPoint2D32f    spotPt;             // 映光点坐标（相对于瞳孔中心）

    //
    stPupilInfo() {
        memset(this, 0, sizeof (stPupilInfo));
    }
};
// 双眼瞳孔信息
struct stPupilInfoPair {
    stPupilInfo R;      // 右眼
    stPupilInfo L;      // 左眼
};

// 视力数据
struct stVisionValue {
    float RSph;     // 右眼 球镜度
    float RCyl;     // 右眼 柱镜度
    int RAx;        // 右眼 轴位
    float RPs;      // 右眼 瞳孔直径(PupilSize)，单位：mm
    int RHz;        // 右眼 水平凝视
    int RVz;        // 右眼 垂直凝视
    int PD;         // 瞳距，单位：mm
    float LSph;     // 左眼 球镜度
    float LCyl;     // 左眼 柱镜度
    int LAx;        // 左眼 轴位
    float LPs;      // 左眼 瞳孔直径(PupilSize)，单位：mm
    int LHz;        // 左眼 水平凝视
    int LVz;        // 左眼 垂直凝视
};

// 视力值异常
/** 异常数值显示规则（据旧代码 AlgorithmThread::resultAlgorithm() 整理，2022-01-07 Henry）：
 * 1、球镜度值太大，球镜度值改为 -7.5（已在 AlgorithmThread::resultAlgorithm() 处理），且在显示时在度数值前加"<"(原代码好像只检查负数值)；
 * 2、柱镜度值太大，根据“是否高度数模式”改为 MAX_CYL_HMMODE 或 MAX_CYL_NORMAL（已在 AlgorithmThread::resultAlgorithm() 处理），且在显示时在度数值前加"<"(柱镜度只有负数)；
 * 3、柱镜度值不可信，则柱镜度和轴位值都显示为4个空格；
 * 4、轴位值不可信，则显示为4个空格；
 */
struct stVisionValueUnnormal {
    bool LAxisUntrusted;    // 轴位不可信
    bool RAxisUntrusted;
    bool LSphTooLarge;      // 球镜度太大
    bool RSphTooLarge;
    bool LCylTooLarge;      // 柱镜度太大
    bool RCylTooLarge;
    bool LCylUntrusted;     // 柱镜度不可信
    bool RCylUntrusted;
};

// 算法版本
enum enAlgoVerAll {
    algoVerAll_2019         = 100,
    algoVerAll_2021_07,
    algoVerAll_2022_04_1,
    algoVerAll_2022_04_2,
    algoVerAll_2022_12,

    algoVerAll_min     = algoVerAll_2019,
    algoVerAll_max     = algoVerAll_2022_12,
    algoVerAll_default = algoVerAll_2022_12,
};

// 参考视力的显示类型
enum enVisionNotation {
    visionNotation_None = 0,       // 不显示
    visionNotation_FivePoint,      // 五分制
    visionNotation_Decimal,        // 小数制

    visionNotation_Min = visionNotation_None,
    visionNotation_Max = visionNotation_Decimal,
};

// 瞳孔识别结果   // TODO: 清掉？
enum enPupilDetectionResult {
    PupilDetectionResult_Unknow = 0,            // Unknow
    PupilDetectionResult_Success,               // Success
    PupilDetectionResult_NotFindContours,       // NotFindContours
    PupilDetectionResult_NotFindContours_2,     // NotFindContours_2
    PupilDetectionResult_NotFindContours_3,     // NotFindContours_3
    PupilDetectionResult_OutOfRange,            // OutOfRange
    PupilDetectionResult_OutOfRange_2,          // OutOfRange_2
    PupilDetectionResult_OutOfRange_3,          // OutOfRange_3
    PupilDetectionResult_OutOfRange_4,          // OutOfRange_4
    PupilDetectionResult_AreaWrong,             // AreaWrong
    PupilDetectionResult_AreaWrong_2,           // AreaWrong_2
    PupilDetectionResult_PDTooSmall,            // PDTooSmall
    PupilDetectionResult_PDTooLarge,            // PDTooLarge
    PupilDetectionResult_RadiusTooSmall,        // RadiusTooSmall
    PupilDetectionResult_RadiusTooLarge,        // RadiusTooLarge
    PupilDetectionResult_DetectionFailed,       // DetectionFailed
};

const QString pupilDetectionResultDesc[] = {
    "Unknow",
    "Success",
    "NotFindContours",
    "NotFindContours_2",
    "NotFindContours_3",
    "OutOfRange",
    "OutOfRange_2",
    "OutOfRange_3",
    "OutOfRange_4",
    "AreaWrong",
    "AreaWrong_2",
    "PDTooSmall",
    "PDTooLarge",
    "RadiusTooSmall",
    "RadiusTooLarge",
    "DetectionFailed",
};

//
class AlgorithmThread : public QObject
{
    Q_OBJECT
public:
    explicit AlgorithmThread(QObject *parent = 0);
    ~AlgorithmThread();

    static int ExposureVal;
    static int timesCnt;
    static int bSpotCoarseLocationCnt;
    QString oldID;
    static int TurnLampNumber;              // 转灯失败次数
    static bool visionValueSource;          // 屈光数据来源：0-正常计算，1-月龄屈光值，2-随机统计值

    static enPupilDetectionResult pupilDetectionResult;

    static int getCurve();
    static void setCurve(int _curve);
    static bool getIsCalculatingVision();   // 正在计算结果（函数 AlgorithmThread::calcVision() 正在执行）
    static void setIsCalculatingVision(bool _val);   // TODO: 临时函数。将计算结果的函数非公开化后去掉该函数，该值仅内部可修改

    void setCurrentPupilAlgoVer(enAlgoVerAll _pupil_algo_ver);

    bool crudeAlgorithm(unsigned char *pFrameBuffer, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l);
    bool crudeAlgorithm0(unsigned char *pFrameBuffer, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l);
    bool singleEye_crudeAlgorithm(unsigned char *pFrameBuffer);
    void accurateAlgorithm(unsigned char *pFrameBuffer);
    //void testSaveImage(unsigned char *pFrameBuffer);
    void testSaveImageinFolder(IplImage*,int);
    void testSaveByteImageinFolder(unsigned char *pFrameBuffer, int index, int mode=0);
    void SaveByteFaultImageinFolder(unsigned char *pFrameBuffer, int index ,int mode ,QString strPath);

    void savePdfPreviewImg(unsigned char *pFrameBuffer);
    void saturationSaveImage(unsigned char *pFrameBuffer, double value);

    //QStringList CalculatePartData();      // TODO: 这是干嘛的？（只有算推值时用到它的返回值里的轴位，2021-09-02）

    static bool detectPupilRoughly(IplImage *Img, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, int &_thresh_val, int _tag = 0);
    static bool detectPupilAccurately(IplImage *_img, stPupilInfo &_pupil_info, int _thresh_val, int _tag1 = 0, int _tag2 = 0);

    // 检测瞳孔光斑
    static std::vector<cv::KeyPoint> detectGlintBlob(cv::Mat &_mat);
    // 测试点是否在圆内部
    static bool pointInCircleTest(cv::Vec3f _circle, cv::Point2f _pt);
    // 过滤亮斑
    static std::vector<cv::Point2f> filtGlintPoints(std::vector<cv::KeyPoint> &_keypoints, int _width);
    // HoughCircles 找瞳孔
    static std::vector<cv::Vec3f> findPupilCircle(cv::Mat &_mat, int &_thresh_val);
    //
    static bool detectPupil_2(IplImage *_img, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, int &_thresh_val, int _tag);
    static void circlesToPupilInfo(cv::Vec3f &_c_l, cv::Vec3f &_c_r, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l);

    static bool findSinglePupilCircle(cv::Mat &_img, cv::Vec3f &_circle, int &_thresh_val);
    static bool detectPupil_3(IplImage *_img, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, int &_thresh_val, int _tag);

    // 检测瞳孔（算法4：二值化采用了自适应阈值，优化了结构）
    static bool detectPupil_4(unsigned char *_img_data, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l,
                              bool _is_calc_vision, enSingleDoubleEyeMode _single_double_eye);
    // 识别图像里的单个瞳
    static bool detectPupilOfImgRoi(IplImage *_img_roi, int _img_idx, stPupilInfo &_pupil_info, bool _is_calc_vision,
                                  enWhichEye _which_eye, bool _is_roughly, int _redu_ratio);
    // 获得单个眼睛的瞳孔轮廓（传入的图像须只含一个瞳孔）
    static int getSinglePupilContours(IplImage *_img_roi, int _img_idx, CvMemStorage *_mem_storage, CvSeq **_contours, bool _is_calc_vision,
                                      enWhichEye _which_eye, bool _is_roughly, int _redu_ratio);
    // 过滤单个眼睛的瞳孔轮廓
    static bool filterSinglePupilContours(IplImage *_img_roi, int _img_idx, CvSeq **_contours, stPupilInfo &_pupil_info, bool _is_calc_vision,
                                          enWhichEye _which_eye, bool _is_roughly, int _redu_ratio);
    // 查找映光点坐标（传入图像须是单只眼睛的，且须设置 ROI）
    static bool detectReflectionSpotSingle(IplImage *_img_roi, int _img_idx, enWhichEye _which_eye, CvPoint &_point);

    // 计算图像的曝光量信息，返回是否计算成功
    static bool calcExposure(unsigned char *_img_data, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, int &_avg, bool &_over_expo,
                             enSingleDoubleEyeMode _single_double_eye);
    static void calcPupilAvgAndOverExpo(IplImage *_img_pupil, float &_avg, int &_count_over);

    //
    static float getSE(float _sph, float _cyl);
    static stVisionValue getRandomStatisticalVision(int _age_idx);
    static bool setMonthAgeVisionByBirthday(stVisionValue &_vision, int _age_range, QDate *_birth_date = Q_NULLPTR);
    static bool setMonthAgeVision(stVisionValue &_vision, int _month_age);
    static void setMonthAgeAxis(stVisionValue &_vision, int _month_age);
    static void getMonthBoundaryOfAgeRange(int _age_range, int &_min_month_age, int &_max_month_age);

    static bool getIsAvgGreyTooLow(int _ave_grey);
    static bool getPupilGreyMin();

    static double calcAvgGrey(const cv::Mat& _img);                                             // 计算图像的平均梯度
    static float calcClarity(const cv::Mat &grayImage);
    static float calcClarity(unsigned char *_img_data);
    static void calcMeanStdDev(const cv::Mat &_img, double &_mean, double &_std_dev);           // 计算图像的均值和标准差
    static void calcMeanStdDev(unsigned char *_img_data, double &_mean, double &_std_dev);      // 计算图像的均值和标准差

    static QString truncUnnormalChar(QString _val_str);

    static void getStatisticalValues(int _age_range, stVisionValue &_vision, stVisionValueUnnormal &_value_unnormal);

    bool detectPupil(unsigned char *_img_data, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l);
    void calcVision(const QVector<unsigned char *> &_img_list, enSingleDoubleEyeMode _single_double_eye);
    static QString seToReferVision(QString _se, int _type = 1);

    void setCountDetect(unsigned int _count);
    unsigned int getCountDetect();

public slots:
    //void save(unsigned char *pFrameBuffer);
    void slotDetectPupil(unsigned char *_img_data, int _img_idx, bool _is_need_calc_expo);
    void slotCalcVision(const QVector<unsigned char *> *_img_list);
    void singleEye_ResultAlgorithm(const QVector<unsigned char *> &_img_list);
    void errorHandle();

signals:
    void sigCalcVisionFinished(enCalcResultState _calc_result_state, stVisionValue _vision, stVisionValueUnnormal _value_unnormal);
    void sigAlgoErr(enAlgoErrType _algo_err, QString _msg);
    //void sigReleaseTurnLampImgList();
    void start_catch_wave();
    void sendPyrLocation(CvPoint3D32f[]);
    void setLoading(bool);
    //void writeBlueToothData(QByteArray);    //2020.10.12  tao
    void sigPupilDetectionResult(uchar *_img_data, int _img_idx, bool _succ, stPupilInfo _pupil_info_r, stPupilInfo _pupil_info_l, int _avg, bool _over_expo);

protected:
    void getMaxLoc(IplImage *_img, CvSeq *_contour, CvPoint &_max_loc);

private:
    static int curve;
    static bool isCalculatingVision;

    int blurpyrDetect(IplImage *pyrImg, CvPoint3D32f *avalPoint3D, int *pupilDis, double *maxValue, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l);
    int blurpyrDetect0(IplImage *pyrImg, CvPoint3D32f *avalPoint3D, int *pupilDis, double *maxValue, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, int _img_num);
    int singleEye_BlurpyrDetect(IplImage *pyrImg, CvPoint3D32f *avalPoint3D, int *pupilDis, double *maxValue);
    static void SpotEnhance(IplImage *src, IplImage *dst);
    static void FillInternalContours(IplImage *pBinary, double dAreaThre);
    static int ConnectEdge(IplImage * src);
    static void lhMorpFillHole(const IplImage* src, IplImage* dst);
    static void lhMorpRErode(const IplImage* src, const IplImage* msk, IplImage* dst, IplConvKernel* se, int iterations);
    static int lhImageCmp(const IplImage *_img1, const IplImage *_img2);
    void rotateImage(IplImage* img, IplImage *img_rotate, int degree);
    double compareEach(IplImage *_img1, IplImage *_img2);
    void setPupilImgRoi(IplImage *_img_pupil, double _radius, enWhichEye _which_eye);     // 设置瞳孔区域 ROI
    void delayCapture(long msec);
    void doCaptureRun();
    void releaseMapImage(QMap<int,IplImage*> *m_map);
    void showStatisticalValues(int _age_range);

    static bool pyrDetectMode;
    //ResultAnalysist StableAnalysit;
    static enAlgoVerAll currentPupilAlgoVer;   // 瞳孔识别算法版本

    unsigned int countDetect = 0;

};

// 图像信息计算
class CCalcImgInfo : public QObject
{
    Q_OBJECT
public:
    explicit CCalcImgInfo(QObject *parent = 0);
    ~CCalcImgInfo();

    double mean = 0;        // 均值
    double stdDev = 0;      // 标准差
    float clarity = 0;      // 清晰度

    void reset();

public slots:
    void slotCalcImgInfo(unsigned char *_img_data);

};

//
extern bool g_SimulatedEyeMode;

extern QMap<int,IplImage*> g_pupilImgLeft;
extern QMap<int,IplImage*> g_pupilImgRight;

extern QMap<int, stPupilInfo> g_pupilInfoRight;
extern QMap<int, stPupilInfo> g_pupilInfoLeft;

extern QMap<int, stPupilInfo> g_pupilInfoRightRough;
extern QMap<int, stPupilInfo> g_pupilInfoLeftRough;

extern CvPoint3D32f g_SaturationCenterL;
extern CvPoint3D32f g_SaturationCenterR;

extern enCalcResultState g_CalcResultState;
extern bool g_UEP[2];
extern int g_saturationPd;                    // 瞳孔识别时最后得到的降维之后的瞳距（像素）

extern enSingleDoubleEyeMode g_SingleDoubleEye;      // 单双眼模式

#endif // ALGORITHMTHREAD_H
