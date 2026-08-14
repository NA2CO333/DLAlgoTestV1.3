#ifndef CALGOINTF_H
#define CALGOINTF_H

/* =============================================================================
 * 接口编码说明：
 *     外部模块（非基础的必要的模块）和本模块之间的耦合应只有本接口头文件定义的类的公开成员，禁止再以其它方式发生关联，如 extern 等。
 *     外部代码需要创建算法对象实例时只要调用本接口的 CAlgoIntf::createInstance() 函数即可。
 *
 */

#include <string.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <math.h>
#include <opencv2/opencv.hpp>
#include <QString>

#include "globaltypes.h"

// ------------- 全局类型 begin

// 相机类型
enum enCameraType;

// 单双眼模式
enum enSingleDualEyeMode;

// 年龄段
enum enAgeRange;


// 哪一只眼
enum enWhichEye {
    whichEye_Right  = 1,
    whichEye_Left,

    whichEye_Min    = whichEye_Right,
    whichEye_Max    = whichEye_Left,
};
// -------------- 全局类型 end

// 算法模式
enum enAlgoMode {
    algoMode_General        = 1,        // 普通模式
    algoMode_Professional,              // 专业模式

    algoMode_Min        = algoMode_General,             // 最小值（用于值的合法性检查）
    algoMode_Max        = algoMode_Professional,
    algoMode_Default    = algoMode_Professional,
};

// 算法版本
enum enAlgoVer {
    algoVer_2022_12     = 100,

    algoVer_Min         = algoVer_2022_12,                  // 最小值（用于值的合法性检查）
    algoVer_Max         = algoVer_2022_12,
    algoVer_Default     = algoVer_2022_12,
};
const char *enumToCaption_AlgoVer(enAlgoVer _ver);      // 枚举值转标题 - 算法版本

// 算法运行控制命令；显式固定枚举值，避免后续调整顺序造成协议变化。
enum class enAlgoCommandType {
    SetTurnLampSaveContext = 0,
    CancelMeasurementRuntime = 1,
    FinishFormalRoundInput = 2,
    QueryFormalAsyncPupilMode = 3
};

// 统一封装原有运行控制接口的参数，保持各命令原有线程语义不变。
struct stAlgoCommand {
    enAlgoCommandType type {
        enAlgoCommandType::FinishFormalRoundInput
    };
    int roundIdx {-1};
    std::string patientImageDir;
    std::string batchDir;
    std::string sourceDir;
    std::string usbRoot;

    static stAlgoCommand makeSetTurnLampSaveContext(
            const std::string &patientImageDir,
            const std::string &batchDir,
            const std::string &sourceDir,
            const std::string &usbRoot)
    {
        stAlgoCommand command;
        command.type = enAlgoCommandType::SetTurnLampSaveContext;
        command.patientImageDir = patientImageDir;
        command.batchDir = batchDir;
        command.sourceDir = sourceDir;
        command.usbRoot = usbRoot;
        return command;
    }

    static stAlgoCommand makeCancelMeasurementRuntime()
    {
        stAlgoCommand command;
        command.type = enAlgoCommandType::CancelMeasurementRuntime;
        return command;
    }

    static stAlgoCommand makeFinishFormalRoundInput(int roundIdx)
    {
        stAlgoCommand command;
        command.type = enAlgoCommandType::FinishFormalRoundInput;
        command.roundIdx = roundIdx;
        return command;
    }

    static stAlgoCommand makeQueryFormalAsyncPupilMode()
    {
        stAlgoCommand command;
        command.type = enAlgoCommandType::QueryFormalAsyncPupilMode;
        return command;
    }
};

// 统一命令的返回值；查询命令通过boolValue返回正式异步模式状态。
struct stAlgoCommandResult {
    bool success {false};
    bool boolValue {false};
    std::string error;

    static stAlgoCommandResult makeSuccess(bool boolValue = false)
    {
        stAlgoCommandResult result;
        result.success = true;
        result.boolValue = boolValue;
        return result;
    }

    static stAlgoCommandResult makeFailure(const std::string &error)
    {
        stAlgoCommandResult result;
        result.success = false;
        result.error = error;
        return result;
    }
};

// 区域
struct CRect
{
    int x;
    int y;
    int width;
    int height;
};

// 点（浮点类型）
struct CPointF
{
    float x;
    float y;
};

enum PupilFallbackType {
    PupilFallback_None = 0,
    PupilFallback_DarkGlintHough = 1,
    PupilFallback_GlintDarkRegion = 2,
    PupilFallback_RadialBoundary = 3,
    PupilFallback_NonRoundCandidate = 4,
    PupilFallback_DeepModel = 5,
    PupilFallback_DeepTrack = 6,
    // 模型/跟踪坐标经过当前图片小 ROI 确认后的来源标记。
    PupilFallback_RoiRefined = 7,
    PupilFallback_TraditionalRoi = 8
};

enum PupilEyeRectSource {
    PupilEyeRect_Base = 0,
    PupilEyeRect_Fallback1 = 1,
    PupilEyeRect_Fallback2 = 2,
    PupilEyeRect_Fallback3 = 3,
    PupilEyeRect_Fallback4 = 4
};

// 瞳孔信息（单位：像素）
struct stPupilInfo {
    cv::Rect        rect;               // 瞳孔的边界矩形
    double          area = -1;          // 瞳孔面积
    double          perimeter = -1;     // 周长
    cv::Point2f     center;             // 瞳孔中心坐标
    cv::Point2f     spotPt;             // 映光点坐标
    double          radius = -1;        // 瞳孔半径
    double          circularity = -1;   // 圆度（ = (4 * pi * area) / (perimeter ^ 2)）

    float dx,dy;         //眼位（映光点相对于瞳孔中心偏离）
    int fallbackType = PupilFallback_None;       // 单帧瞳孔识别兜底类型
    int eyeRectSource = PupilEyeRect_Base;       // 眼框来源，0 表示原眼框
    int coordinateSource = 0;                    // 模型/跟踪/跨轮来源，仅用于诊断
    //
    stPupilInfo() {
        memset(this, 0, sizeof (stPupilInfo));
    }
};

// 第一轮第一张正式照片的左右眼瞳孔裁图结果。
// 裁图拥有独立的cv::Mat内存；结果结构由调用方持有，图片可按需clone后异步使用。
struct stPupilCropResult {
    // 用于区分不同测量任务，防止旧结果被误当成新结果。
    std::uint64_t measurementGeneration = 0;

    // 当前接口严格对应第一物理轮第1张正式照片，即roundIdx=0、imgNo=1。
    // 第1张缺失时不使用其他照片替代。
    int roundIdx = -1;
    int imgNo = 0;

    // 当前固定输出尺寸为140×225；内容由瞳孔中心附近70×112原图区域放大得到。
    int cropWidth = 0;
    int cropHeight = 0;

    // 对应眼是否得到129 ROI精修后的有效中心。
    bool rightValid = false;
    bool leftValid = false;

    // 右眼、左眼瞳孔中心的原图坐标。
    cv::Point2f rightCenter;
    cv::Point2f leftCenter;

    // 裁图拥有独立的cv::Mat内存；如需跨线程长期保存，请对cv::Mat执行clone()。
    cv::Mat rightImage;
    cv::Mat leftImage;
};

// 视力数据
struct stVisionValue {
    double  RSph;        // 右眼 球镜度
    double  RCyl;        // 右眼 柱镜度
    int     RAx;        // 右眼 轴位
    double  RPs;        // 右眼 瞳孔直径(PupilSize)，单位：mm
    int     RHz;        // 右眼 水平凝视
    int     RVz;        // 右眼 垂直凝视
    bool    RPtosis;    // 右眼 是否上睑下垂

    int     PD;        // 瞳距，单位：mm

    double  LSph;        // 左眼 球镜度
    double  LCyl;        // 左眼 柱镜度
    int     LAx;        // 左眼 轴位
    double  LPs;        // 左眼 瞳孔直径(PupilSize)，单位：mm
    int     LHz;        // 左眼 水平凝视
    int     LVz;        // 左眼 垂直凝视
    bool    LPtosis;    // 左眼 是否上睑下垂

    // 辅助函数：根据左右眼设置屈光值
    void setRef(double s, double c, int a, enWhichEye eye) {
        if (eye == whichEye_Right) { RSph = s; RCyl = c; RAx = a; }
        else { LSph = s; LCyl = c; LAx = a; }
    }
    // 辅助函数：从另一结构体复制非屈光参数
    void copyOtherParamsFrom(const stVisionValue& other, enWhichEye eye) {
        if (eye == whichEye_Right) {
            RPs = other.RPs; RHz = other.RHz; RVz = other.RVz; RPtosis = other.RPtosis;
        } else {
            LPs = other.LPs; LHz = other.LHz; LVz = other.LVz; LPtosis = other.LPtosis;
        }
    }
    // 新增toString方法
    QString toString(bool detailed = true) const {
        if (detailed) {
            return QString(
                "右眼: Sph=%1 Cyl=%2 Ax=%3 Ps=%4mm Hz=%5 Vz=%6 Ptosis=%7 | "
                "左眼: Sph=%8 Cyl=%9 Ax=%10 Ps=%11mm Hz=%12 Vz=%13 Ptosis=%14 | "
                "PD=%15mm"
            )
            .arg(RSph, 0, 'f', 2).arg(RCyl, 0, 'f', 2).arg(RAx)
            .arg(RPs, 0, 'f', 1).arg(RHz).arg(RVz).arg(RPtosis ? "是" : "否")
            .arg(LSph, 0, 'f', 2).arg(LCyl, 0, 'f', 2).arg(LAx)
            .arg(LPs, 0, 'f', 1).arg(LHz).arg(LVz).arg(LPtosis ? "是" : "否")
            .arg(PD);
        } else {
            // 简要模式
            return QString("R:Sph%1/Cyl%2/Ax%3 L:Sph%4/Cyl%5/Ax%6 PD%7mm")
                .arg(RSph, 0, 'f', 2).arg(RCyl, 0, 'f', 2).arg(RAx)
                .arg(LSph, 0, 'f', 2).arg(LCyl, 0, 'f', 2).arg(LAx)
                .arg(PD);
        }
    }
};



// stVisionValue 类型相加
stVisionValue addVisionValue(const stVisionValue &_a, const stVisionValue &_b);

// 视力值异常
/** 异常数值显示规则（据旧代码 AlgorithmThread::resultAlgorithm() 整理，2022-01-07 Henry）：
 * 1、球镜度值太大，球镜度值改为 -7.5（已在 AlgorithmThread::resultAlgorithm() 处理），且在显示时在度数值前加"<"(原代码好像只检查负数值)；
 * 2、柱镜度值太大，根据“是否高度数模式”改为 MAX_CYL_HMMODE 或 MAX_CYL_NORMAL（已在 AlgorithmThread::resultAlgorithm() 处理），且在显示时在度数值前加"<"(柱镜度只有负数)；
 * 3、柱镜度值不可信，则柱镜度和轴位值都显示为4个空格；
 * 4、轴位值不可信，则显示为4个空格；
 */
struct stVisionAbnormal {
    bool LAxisUntrusted = false;    // 轴位不可信
    bool RAxisUntrusted = false;
    bool LSphTooLarge   = false;    // 球镜度太大
    bool RSphTooLarge   = false;
    bool LCylTooLarge   = false;    // 柱镜度太大
    bool RCylTooLarge   = false;
    bool LCylUntrusted  = false;    // 柱镜度不可信
    bool RCylUntrusted  = false;
};

// stVisionAbnormal 类型相加
stVisionAbnormal addVisionAbnormal(const stVisionAbnormal &_a, const stVisionAbnormal &_b);

// 屈光计算的结果
struct stVisionCalcResult {
    stVisionValue visionValue;          // 屈光数值
    stVisionAbnormal visionAbnormal;    // 屈光异常
};

// 屈光计算结果状态
enum enCalcResultState {
    calcResultState_Unknown     = -1,       // 未知
    calcResultState_Succ        = 0,        // 成功
    calcResultState_Fail        = 1,        // 失败（原因未明确）
    calcResultState_GazeOver,               // 眼位超过最大值
    calcResultState_Blinked,                // 眨眼了
    calcResultState_PupilNotFound,          // 识别瞳孔失败
    calcResultState_VisionAbnormal,         // 视力值异常
    calcResultState_ParamInvalid,           // 参数非法
    calcResultState_Timeout,                // 计算超时
    calcResultState_ImageError,             // 图像错误
    calcResultState_ProgramException,       // 发生程序异常
    calcResultState_OutOfRange,             // 瞳孔坐标超出坐标范围
    calcResultState_ResultCallbackTimeout,  // 结果回调超时
    calcResultState_Aborted,                // 被中止
    // 首张C800或同照片129 ROI未确认，请求当前物理轮提前结束并重试下一轮。
    calcResultState_RetryNextRound = 13,

    calcResultState_Min                 = calcResultState_Succ,
    calcResultState_Max                 = calcResultState_OutOfRange,
};

/**
 * @brief 算法接口类（虚类，只有声明，没有实现）
 */
class CAlgoIntf
{
public:

    /**
     * @brief 根据传入的参数创建实例（工厂模式，不是单例模式）
     * @param _camera_type  : 相机类型
     * @param _algo_ver     : 算法版本
     * @return : 算法实现的对象实例指针，调用者使用完后须手动释放
     */
    static CAlgoIntf *createInstance(enCameraType _camera_type, enAlgoVer _algo_ver = algoVer_Default);

    // 设置【普通/专业】模式
    virtual void setAlgoMode(enAlgoMode _algo_mode) = 0;

    // 设置【是否高度数模式】
    virtual void setIsHmMode(bool _is_hm_mode) = 0;

    // 设置【最大固视偏差（°）】
    virtual void setMaxGazeDeviation(int _max_gaze_dev) = 0;

    // 设置【是否存图】
    virtual void setIsSaveImg(bool _is_save_img) = 0;

    // 设置【是否单线程计算】
    virtual void setIsSingleThread(bool _is_single_thread) = 0;

    // 设置【是否模拟眼】
    virtual void setIsSimulatedEye(bool _is_simulated) = 0;

    //设置模拟眼和人眼瞳孔的长宽比阈值，作为瞳孔的过滤条件
    virtual void setWHRatio(float modeleye_wh_ratio,float humaneye_wh_ratio) = 0;

    /**
     * @brief 计算单张图像的清晰度
     * @param _image 输入图像，支持CV_8UC1灰度图和CV_8UC3 BGR图
     * @return 拉普拉斯方差，数值越大越清晰；返回-1.0表示当前实现不支持或计算失败
     */
    virtual double calcImageClarity(const cv::Mat &_image) const
    {
        // 清晰度属于可选能力，未提供实现的传统算法返回失败值。
        (void)_image;
        return -1.0;
    }


    /**
     * @brief 检测瞳孔
     * @param _img_data             : 图像数据
     * @param _img_idx              : 图像索引号，若是转灯图，则与灯号一致
     * @param _pupil_info_r         : 【输出参数】右眼瞳孔信息
     * @param _pupil_info_l         : 【输出参数】左眼瞳孔信息
     * @param _is_calc_vision       : 是否屈光计算
     * @param _single_dual_eye      : 单双眼模式
     * @return : 是否成功
     */
    virtual bool detectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l,
                              bool _is_calc_vision, enSingleDualEyeMode _single_dual_eye) = 0;

    /**
     * @brief 计算图像的曝光量信息，返回是否计算成功
     * @param _img_data
     * @param _img_idx
     * @param _pupil_info_r
     * @param _pupil_info_l
     * @param _avg                  : 【输出参数】瞳孔区域的灰度均值
     * @param _over_expo            : 【输出参数】是否过曝
     * @param _single_dual_eye
     * @return : 是否成功
     */
    virtual bool calcExposure(unsigned char *_img_data, int _img_idx, stPupilInfo _pupil_info_r, stPupilInfo _pupil_info_l,
                              int &_avg, bool &_over_expo, enSingleDualEyeMode _single_dual_eye) = 0;

    // 统一算法运行控制入口；具体命令仍由调用方维持原有同步/异步语义。
    virtual stAlgoCommandResult handleAlgoCommand(
            const stAlgoCommand &command) = 0;
    /**
     * @brief 类型定义：屈光结果回调函数
     * @param _vision   【输出参数】计算得到的屈光数值
     * @param _abnormal 【输出参数】屈光数值的异常状态             // TODO: 这个参数是否需要保留？
     */
    using VisionCallback = std::function<void(enCalcResultState,
                                              int roundIdx,
                                              const stVisionValue &,
                                              const stVisionAbnormal &,
                                              const std::vector<stVisionValue> &,
                                              bool& finished, bool& questionable)>;

    /**
     * @brief 设置屈光结果回调函数
     * @param cb
     */
    virtual void setVisionResultCallback(VisionCallback cb)=0;

    // 结果页主动读取第一物理轮第一张正式照片的左右眼瞳孔裁图。
    // 只有最终结果已经输出后才返回true；裁图在本接口调用时生成。
    virtual bool getPupilCropResult(stPupilCropResult& result) = 0;

    /**
     * @brief 计算屈光度开始
     * @param _img_list             : 转等图数据列表
     * @param _age_range            : 年龄段
     * @param _sub_dir_name         : 本次测量所对应的子目录名（调用方应确保每次测量所传入的“子目录名”具有不重复性；存档文件的路径 = 根目录路径 + 路径分隔符 + 子目录名 + 路径分隔符 + 自定义文件名）
     * @param _single_dual_eye      : 单双眼模式
     * @return
     */
    virtual bool calcVisionBegin(const enAgeRange _age_range, const std::string _sub_dir_name, const enSingleDualEyeMode _single_dual_eye,int roundNo)
    {
        m_currAgeRange = _age_range;
        m_currSubDirName = _sub_dir_name;
        m_currSingleDualEye = _single_dual_eye;

        return true;
    }

    /**
     * @brief 添加正式转灯图像
     * @param _img          单通道图像，step等于图像宽度
     * @param _round_idx    物理转灯轮次，从0开始
     * @param _img_no       相机真实帧编号，合法范围为0～22；0为转灯起始/清缓存帧，
     *                      由算法接收后忽略；1～22为参与瞳孔定位和DS计算的有效灯位
     * @param isInRange     当前图片对应的距离状态
     * @return              算法接收状态
     */
    virtual enCalcResultState appendImage(const cv::Mat &_img, const int _round_idx, const int _img_no, const bool isInRange) = 0;

    // 根目录路径（本模块所保存的文件必须在此目录内）
    static const std::string &getRootDirPath();
    // 存图目录路径
    static const std::string &getImageDirPath();

protected:
    enAgeRange          m_currAgeRange      {enAgeRange::ageRange_Invalid};                 // 当前年龄段
    std::string         m_currSubDirName    {};                                             // 当前 subDirName
    enSingleDualEyeMode m_currSingleDualEye {enSingleDualEyeMode::singleDualEyeMode_Both};  // 当前 单双眼 模式

    // 根目录路径（本模块所保存的文件必须在此目录内）
    static const std::string rootDirPath;
    // 存图目录路径
    static const std::string imageDirPath;

};

#endif // CALGOINTF_H
