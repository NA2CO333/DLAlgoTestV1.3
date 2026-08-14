#ifndef CALGOINVOKER_H
#define CALGOINVOKER_H

#include <atomic>

#include <QObject>
#include <QMetaType>
#include <QElapsedTimer>
#include <QTimer>
#include <QDebug>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "algointf.h"
#include "globaltypes.h"
#include "data.h"
#include "logger.h"

Q_DECLARE_METATYPE(stAlgoCommand)

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

//
// 像素长度单位转换为物理长度单位（毫米）的系数，即每个像素对应的毫米数，迈德威视相机的 = 120(mm) / 752(pixel)
#define PIX_TO_PHY      0.15957446 / PIX_COEF

//
const double MAX_CYL_NORMAL = -4.0F;    // 最大柱镜度（非高度数模式）
const double MAX_CYL_HMMODE = -7.5F;    // 最大柱镜度（高度数模式）（high myopia）

// 算法错误类别（瞳孔识别或屈光计算）
enum enAlgoErrType {
    algoErrType_DetectPupil,        // 瞳孔识别
    algoErrType_CalcVision,         // 屈光计算
};
const char *enumToText_AlgoErrType(enAlgoErrType _err_type);     // 枚举转文本：算法错误类别

QString enumToText_CalcResultState(enCalcResultState _result_stat);      // 枚举转文本：屈光计算结果状态
const char *enumToName_CalcResultState(enCalcResultState _result_stat);

// 参考视力的显示类型
enum enVisionNotation;

// 算法版本（全部，含旧算法）        // TODO: 清理掉？
enum enAlgoVerAll {
    algoVerAll_2019         = 0,
    algoVerAll_2021_07,
    algoVerAll_2022_04_1,
    algoVerAll_2022_04_2,
    algoVerAll_2022_12,

    algoVerAll_Min     = algoVerAll_2019,
    algoVerAll_Max     = algoVerAll_2022_12,
    algoVerAll_Default = algoVerAll_2022_12,
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

// 屈光度计算调用流程的状态
enum class enDiopterCalcStat {
    NotBegin        = 0,    // 未调用 calcVisionBegin()
    HasBegin        ,       // 已调用 calcVisionBegin()
    HasCallback     ,       // 已回调
    HasEnd          ,       // 已结束
};
const char *enumToText_DiopterCalcStat(enDiopterCalcStat _stat);     // 枚举转文本：屈光度计算流程的状态

//
class CAlgoInvoker : public QObject        // TODO: 清理？
{
    Q_OBJECT
public:
    explicit CAlgoInvoker(QObject *_parent=nullptr);

    static int visionValueSource;           // 屈光数据来源：0-正常计算，1-月龄屈光值，2-随机统计值
    static int bSpotCoarseLocationCnt;

    static enPupilDetectionResult pupilDetectionResult;

    void init();        // 初始化
    void reset();       // 重置
    // 统一算法运行控制入口；取消/查询保持同步，设置上下文/结束轮次排队。
    stAlgoCommandResult executeAlgoCommand(
            const stAlgoCommand &command);

    // 检测瞳孔（算法4：二值化采用了自适应阈值，优化了结构）
    bool detectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l,
                              bool _is_calc_vision, enSingleDualEyeMode _single_dual_eye);
    // 计算图像的曝光量信息，返回是否计算成功
    bool calcExposure(unsigned char *_img_data, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, int &_avg, bool &_over_expo,
                             enSingleDualEyeMode _single_dual_eye);
    // 计算屈光度
    //enCalcResultState calcVision(std::vector<unsigned char *> &_img_list, enSingleDualEyeMode _single_dual_eye,
    //                             QString _img_dir_name, QDate _birth_date);

    // 测试过程中保存22张图片         // TODO: 从算法模块拷贝，待优化
    static void testSaveByteImageinFolder_(unsigned char *pFrameBuffer, int index, QString _img_dir_name, int mode);

    // 对调转灯图集中指定的两张图像的位置
    static void swapPosiOfTurnLampImgs(std::vector<unsigned char *> &_img_set, const QPair<int, int> &_pair);

    // 将指定光路类型的图集转换为与常规光路类型一致
    static void convertImageSetToGeneralOpticalType(const enOpticalPathType _optical_type, std::vector<unsigned char *> *_img_set);
    // 将指定光路类型的图像和图号转换为与常规光路类型一致
    static bool convertImageAndNumberToGeneralOpticalType(const enOpticalPathType _optical_type, unsigned char * const _img_data,
                                                          const int _img_num_old, int &_img_num_new, QString &_err_msg);

    // 屈光度 转 参考视力
    static QString diopterToVision(QString _sph_str, QString _cyl_str, enVisionNotation _vision_type = visionNotation_FivePoint);
    // 屈光度 转 参考视力
    static QString diopterToVision(double _sph, double _cyl, enVisionNotation _vision_type = visionNotation_FivePoint);

    static float getSE(float _sph, float _cyl);
    static void getStatisticalValues(int _age_range, QDate _birth_date, stVisionValue &_vision, stVisionAbnormal &_vision_abnormal);

    static bool getIsCalculatingVision();   // 是否正在计算结果（函数 calcVision() 正在执行）
    static void setIsCalculatingVision(bool _val);   // TODO: 临时函数。将计算结果的函数非公开化后去掉该函数，该值仅内部可修改

    static int getCurve();
    static void setCurve(int _curve);

    //static void scope_limitation(CPatient &_patient);           // 异常大的数值，加上 ">"或"<" 表示   // TODO：这个处理，应当仅用于输出的显示，而不应当存储到数据库？这个处理过后，数值将无法再用于比较等运算？
    //static QString resultReceieve(const QString &temp);         // 将含">"或"<"的屈光度字符串，去掉">"或"<"后转浮点数，再加上">"或"<"（即对异常值字符串检查修正）
    //static QString truncAbnormalChar(QString _val_str);         // 清除屈光度字符串里的">"或"<"    // TODO: 这个清掉
    //static bool isVisionStrNormal(const QString &_vision);      // 判断屈光度字符串是否正常（不包含">"和"<"）

    static void calcMeanStdDev(const cv::Mat &_img, double &_mean, double &_std_dev);           // 计算图像的均值和标准差
    static void calcMeanStdDev(unsigned char *_img_data, double &_mean, double &_std_dev);      // 计算图像的均值和标准差

    void setCurrentPupilAlgoVer(enAlgoVerAll _pupil_algo_ver);      // NOTE: 已废弃

    static float calcClarity(const cv::Mat &grayImage);
    static float calcClarity(unsigned char *_img_data);

    void setCountDetect(unsigned int _count);
    unsigned int getCountDetect();

    void savePdfPreviewImg(unsigned char *pFrameBuffer);
    //void saturationSaveImage(unsigned char *pFrameBuffer, double value);
    //static void SaveByteFaultImageinFolder(unsigned char *pFrameBuffer, int index ,int mode);

    // 设置按月龄估算的屈光度数
    /* 参数：*_birth_date：生日。若没有，则传入 nullptr，函数将通过瞳距估算月龄。
     *      如果没有生日，须传入 PD 值。
     */
    static bool setMonthAgeVisionByBirthday(stVisionValue &_vision, int _age_range, QDate *_birth_date = Q_NULLPTR);

    /**
     * @brief 修正轴位角
     * @param _ax   原值（可能经过运算后，值小于0，或大于180）
     * @return 取值范围 0~179 度
     */
    static int correctAxis(const int _ax);

    /**
     * @brief 随机构造 3 个均值为 avg 的，偏差范围在 0~_deviation 的数
     * @param _avg
     * @param _deviation    偏差范围
     * @param _prec         精度
     * @param _num1
     * @param _num2
     * @param _num3
     */
    static void generateDoubles(const double _avg, const double _deviation, const double _prec,
                                double &_num1, double &_num2, double &_num3);

    /**
     * @brief 浮点数格式的屈光度数（SPH 或 CYL）转为文本格式
     * @param _diopter      屈光度数（SPH 或 CYL）
     * @param _resolution   屈光度的分辨率（0.01或0.25）
     * @return  屈光度数的文本表示
     */
    static QString doubleToDiopterStr(double _diopter, double _resolution);

    /**
     * @brief 切换正负散光（字符串 类型 AX）
     * @param _sph_old          旧的 SPH 值        // NOTE: 须是负散光时的值
     * @param _cyl_old
     * @param _ax_old
     * @param _is_cyl_negative  是否负散光
     * @param _sph_new          新的 SPH 值        // NOTE: 当“是否负散光”为否时才转换散光符号，否则原样返回
     * @param _cyl_new
     * @param _ax_new
     */
    static void switchCylSign_StrAx(const QString &_sph_old, const QString &_cyl_old, const QString &_ax_old,
                                    bool _is_cyl_negative, QString &_sph_new, QString &_cyl_new, QString &_ax_new);
    // 切换正负散光（int 类型 AX）
    static void switchCylSign_IntAx(const double &_sph_old, const double &_cyl_old, const int &_ax_old,
                                    bool _is_cyl_negative, double &_sph_new, double &_cyl_new, int &_ax_new);
    // 切换正负散光（double 类型 AX）
    static void switchCylSign_DblAx(const double &_sph_old, const double &_cyl_old, const double &_ax_old,
                                    bool _is_cyl_negative, double &_sph_new, double &_cyl_new, double &_ax_new);

    static cv::Mat pixmapToMat(const QPixmap &_pixmap);         // QPixmap -> cv::Mat   // TODO: 未测试，仅支持3通道？
    static QPixmap matToPixmap(const cv::Mat &_mat);            // cv:Mat -> QPixMap    // TODO: QPixmap 应改为 QImage ？

    static cv::Mat readAndEqualizeHistFromFileToCvMat(const QString &_path);        // 从文件读取并直方图均衡化处理指定路径的灰度图，返回 cv::Mat 类型
    static QPixmap readAndEqualizeHistFromFileToPixmap(const QString &_path);       // 从文件读取并直方图均衡化处理指定路径的灰度图，返回 QPixmap 类型
    static IplImage *readAndEqualizeHistFromFileToIplImage(const QString &_path);   // 从文件读取并直方图均衡化处理指定路径的灰度图，返回 IplImage* 类型（由调用方释放数据）

    /**
     * @brief 屈光计算开始
     * @param _age_range
     * @param _img_dir_name
     * @param _single_dual_eye
     * @param _round_idx
     */
    void calcVisionBegin(const enAgeRange _age_range, const QString _img_dir_name, const enSingleDualEyeMode _single_dual_eye,
                         int _round_idx);

    /**
     * @brief 添加图像（1通道）
     * @param _img_data     图像数据
     * @param _round_idx    转灯轮次索引号（从0开始）
     * @param _img_num      图像编号
     */
    void appendImage(unsigned char *_img_data, const int _w, const int _h, const int _round_idx, const int _img_num, const bool _is_dist_fit);

    void calcVisionEnd();                           // （主动）结束 CalcVision 过程（标识过程结束，使后续回调事件拒绝处理）
    void startCalcVisionTimeoutChecking();          // 开始 CalcVision 超时检查
    void stopCalcVisionTimeoutChecking();           // 结束 CalcVision 超时检查

    enDiopterCalcStat diopterCalcStat() { return m_diopterCalcStat; }       // 屈光度计算流程状态

signals:
    //void sigCalcVisionFinished(const enCalcResultState _calc_result_state, const stVisionValue _vision, const stVisionAbnormal _vision_abnormal);
    void sigCalcVisionCallbackReceived(const enCalcResultState _calc_result_state, int _round_idx,
                               const stVisionValue _vision, const stVisionAbnormal _vision_abnormal,
                               const std::vector<stVisionValue> &_result_set, bool _is_finished, bool _is_questionable);
    void sigAlgoErr(enAlgoErrType _algo_err, QString _msg);
    void sigMsgNotify(QString _msg);
    void sigPupilDetectionResult(uchar *_img_data, int _img_idx, bool _succ, stPupilInfo _pupil_info_r, stPupilInfo _pupil_info_l, int _avg, bool _over_expo);

    /* 私有信号 */
    void sigCalcVisionBegin(const enAgeRange _age_range, const QString _img_dir_name, const enSingleDualEyeMode _single_dual_eye,
                            int _round_idx, QPrivateSignal);
    void sigExecuteAlgoCommand(stAlgoCommand command, QPrivateSignal);
    void sigAppendImage(unsigned char *_img_data, const int _w, const int _h, const int _round_idx, const int _img_num, const bool _is_dist_fit, QPrivateSignal);

public slots:
    void slotDetectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, bool _is_need_calc_expo);
    //void slotCalcVision(std::vector<unsigned char *> *_img_list, const CPatient &_patient);

protected Q_SLOTS:
    void slot_this_CalcVisionBegin(const enAgeRange _age_range, const QString _img_dir_name, const enSingleDualEyeMode _single_dual_eye,
                                   int _round_idx, QPrivateSignal);
    void slot_this_ExecuteAlgoCommand(stAlgoCommand command, QPrivateSignal);
    void slot_this_AppendImage(unsigned char *_img_data, const int _w, const int _h, const int _round_idx, const int _img_num, const bool _is_dist_fit);
    void slot_timerCalcVisionCallback_timeout();

protected:
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    static int curve;
    static bool isCalculatingVision;

    static enAlgoVerAll currentPupilAlgoVer;   // 瞳孔识别算法版本

    unsigned int countDetect = 0;

    QString oldID;          // TODO: 这是？
    static int timesCnt;    // TODO: 这是？

    static stVisionValue getRandomStatisticalVision(int _age_idx);

    static bool setMonthAgeVision(stVisionValue &_vision, int _month_age);
    static void setMonthAgeAxis(stVisionValue &_vision, int _month_age);
    static void getMonthBoundaryOfAgeRange(int _age_range, int &_min_month_age, int &_max_month_age);

    void doOnGetCalcVisionResult(const enCalcResultState _result_stat, int _round_idx,
                                 const stVisionValue &_vision, const stVisionAbnormal &_abnormal,
                                 const std::vector<stVisionValue> &_result_set, bool _finished, bool _questionable);

    void doOnCalcVisionTimeout();

    CAlgoIntf *m_algoIntf {nullptr};

    QElapsedTimer g_elapsedDetectOnce;      // 瞳孔检测耗时
    //QElapsedTimer g_calcVision;             // 结果计算耗时

    std::atomic<enDiopterCalcStat> m_diopterCalcStat {enDiopterCalcStat::NotBegin};     // 屈光度计算流程状态

    QTimer *m_timerCalcVisionCallback {nullptr};        // CalcVisionCallback 超时检查

};

///============================================================================
/// class CCalcImgInfo

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
struct strabismus{          // 斜视数据
    double leftEyeLR;       // 左眼水平斜视
    double leftEyeUD;       // 左眼垂直斜视
    double rightEyeLR;
    double rightEyeUD;

    void clear(){
        leftEyeLR = 0;
        leftEyeUD = 0;
        rightEyeLR = 0;
        rightEyeUD = 0;
    }
};

// =============================================================================================================
// class CNumericMovingAvgFilter
// =============================================================================================================

// 移动平均(Moving Average)数值滤波器（模板参数类型只支持数值类型）
/* 功能：将持续得到的数值输入给此类，等到信号稳定时（由此类判定），即可通过此类获得稳定的当前值。
 */
template <typename T>
class CNumericMovingAvgFilter
{
public:
    /**
     * @brief 构造函数
     * @param _window_size          移动平均的窗口大小（单位：元素个数）
     * @param _abnormal_diff_ratio  异常值（跳变过快的值）的偏差（绝对值）的最大比例，0或负数表示不检查异常值
     */
    CNumericMovingAvgFilter(size_t _window_size, double _abnormal_diff_ratio)
        : m_avgWindowSize(_window_size), m_abnormalDiffRatio(_abnormal_diff_ratio)
    {}
    ~CNumericMovingAvgFilter() {}

    // 重置
    void reset()
    {
        m_values.clear();
        m_lastTotal = 0;
        m_valueFiltered = 0;
    }

    // 输入数值
    T inputValue(T _val)
    {
        logDebug(QString("%1->%2(): entered... InputValue = %3, FilteredValue = %4").arg(__BASE_FILE__).arg(__FUNCTION__).arg(_val).arg(m_valueFiltered));

        //if(m_valueFiltered<=0)
        {
            // 元素插入
            m_values.push_back(_val);
            m_lastTotal += _val;

            // 移动平均
            m_valueFiltered = movingAvg();
        }

        //
        return m_valueFiltered;
    }

    // 数值是否“已稳定”
    bool isSteady() { return (m_values.size() >= m_avgWindowSize); }

    // 滤波后的数值
    T valueFiltered() { return m_valueFiltered; }

protected:
    // 以下成员变量需要重置 =============================================

    std::deque<T> m_values;     // 数值队列     // TODO: 改为【固定长度数组+当前索引号】数据结构效率更高？
    T m_lastTotal {0};          // 上次的总和
    T m_valueFiltered {0};      // 已滤波的当前值

    // 以下成员变量不需重置 =============================================
    size_t m_avgWindowSize;         // 移动平均的窗口大小（单位：元素个数）
    double m_abnormalDiffRatio;     // 异常值（跳变过快的值）的偏差（绝对值）的最大比例，负数表示不检查异常值

    // 移动平均
    double movingAvg()
    {
        // TODO: 支持空值？


        //
        if (m_values.empty()) {
            return 0;
        }

        // 移除掉超出移动平均窗口范围的值
        if (m_values.size() > m_avgWindowSize) {
            m_lastTotal -= m_values.front();
            m_values.pop_front();
        }

        // 求均值
        double avg = (double)m_lastTotal / m_values.size();

        //
        if (m_abnormalDiffRatio > 0.000001 && m_values.size() >= m_avgWindowSize) {
            // 剔除异常值（过滤掉跳变过快的值）
            int count_outlier = 0;      // 离群值计数
            for (int i = (int)m_values.size() - 1; i >= 0; i--) {
                T &v = m_values.at(i);
                if (std::abs(v - avg) / avg > m_abnormalDiffRatio) {
                    m_lastTotal -= v;
                    m_values.erase(m_values.begin() + i);
                    count_outlier++;
                }
            }
        }

        //
        return avg;
    }
};

//
extern enSingleDualEyeMode g_SingleDualEye;     // 单双眼模式       // TODO: 移到 global

extern CvPoint3D32f g_SaturationCenterL;
extern CvPoint3D32f g_SaturationCenterR;

extern int g_lastDetectTime;

extern int g_PupilState;

#endif // CALGOINVOKER_H
