#ifndef CMEASURECTRL_H
#define CMEASURECTRL_H

#include <QObject>
#include <QPoint>
#include <QString>

#include <stdlib.h>
#include <vector>

#include "algointf.h"
#include "capturethread.h"
#include "exposure-adjuster.h"

// 全局常量
static constexpr int G_TURN_LAMP_FRAME_COUNT = 23;      // 转灯图帧数

// 前置声明
class CAlgoInvoker;

// 距离状态
enum enDistanceState {
    distStat_Unknown = 0,
    distStat_TooNear,       // 太近
    distStat_FitNear,       // 近于合适
    distStat_Fit,           // 合适
    distStat_FitFar,        // 远于合适
    distStat_TooFar,        // 太远
};

// 测量步骤
enum enMeasureStep {
    measureStep_Unknow          = -1,       // 未知
    measureStep_Ready           ,           // 【准备】步骤：包括距离调整到合适，然后识别瞳孔，然后调整曝光时间
    measureStep_Collect         ,           // 【采集】步骤：指转灯，然后抓图，构建用于屈光计算的图集
    measureStep_Calc            ,           // 【计算】步骤：此时正在计算屈光值
    measureStep_CalcFinished    ,           // 【结果】步骤：处理算法结果（成功或失败），及是否重测、推值等
    measureStep_MeasureFinished ,           // 测量结束：结束抓图线程，等

    measureStep_Min             = measureStep_Unknow,
    measureStep_Max             = measureStep_MeasureFinished,
};
const char *enumToText_MeasureStep(enMeasureStep _step);

// 距离信息
struct stDistInfo {
    int distVal;
    enDistanceState distStat;
};

// 帧信息
struct stFrameInfo {
    int idxBuff                 = -1;       // 帧数据的缓存列表索引号
    int num                     = -1;       // 灯号
    int idxDist                 = -1;       // 对应的距离信息的索引号
    int detecStat               = 0;        // 瞳孔识别状态。0:未知，1:成功，2:失败
    QPoint ptPupilR             ;           // 右眼瞳孔中心坐标
    QPoint ptPupilL             ;
    QPoint ptBlinkR             ;           // 右眼映光点坐标
    QPoint ptBlinkL             ;
    double circularityR         = 0;        // 右瞳孔圆度
    double circularityL         = 0;
};

// 图集信息
struct stImgSetInfo {
    int idxFirst                = -1;           // 第一张图在帧信息列表中的索引号
    int imgCount                = 0;            // 图像数量
    bool isDistStable           = false;        // 距离是否稳定
    bool isPupilStable          = false;        // 瞳孔是否稳定
    bool isBlinkStable          = false;        // 映光点是否稳定
    bool isCircularityStable    = false;        // 圆度是否稳定
    int checkStat               = 0;            // 本图集的检查状态。0:未检查，1:检查通过，-1:检查不通过

    //
    bool isValid()
    {
        return (imgCount == G_TURN_LAMP_FRAME_COUNT);
    }

    //
    void reset ()
    {
        idxFirst = -1;
        imgCount = 0;
    }
};

// 转灯图信息
struct stTurnLampImageInfo {
    int imageNum {-1};
    uchar *imageData {nullptr};
};

// 结果错误类别
enum class enResultErrorType {                  // NOTE: 错误码缺省值定义为 0，所以各个错误类别的 0 应定义为无错误
    NoError         = 0,    // 无错误
    CaptureError    ,       // 抓图错误（错误码对应 enCaptureError）
    AlgoError       ,       // 算法错误（错误码对应 enCalcResultState）
};

// 测量控制         // TODO: 将 WinMeasure 里面的所有不依赖UI的测量控制代码全部移到本模块下
class CMeasureCtrl : public QObject
{
    Q_OBJECT
public:
    explicit CMeasureCtrl(CCaptureThread *_capture_thread, QObject *parent = 0);
    ~CMeasureCtrl();

    void setAlgoInvoker(CAlgoInvoker *_algo_invoker);

    bool isIgnoreDist() { return m_isIgnoreDist; }              // 是否忽略距离
    void setIsIgnoreDist(bool _is_ignore_dist) { m_isIgnoreDist = _is_ignore_dist; }

    bool isMultiMeasure() { return m_isMultiMeasure; }          // 是否测量多次
    void setIsMultiMeasure(bool _is_multi) { m_isMultiMeasure = _is_multi; }

    void doBeforeMeasure();                         // 测量开始前的重置
    void doBeforeTurnLamp();                        //
    void doAfterTurnLamp(int _img_count = G_TURN_LAMP_FRAME_COUNT,
                         bool _is_aborted = false,
                         enCaptureError _capture_error = captureError_NoError,
                         const QString &_reason = QString());        // 转灯结束后保存本轮已有图
    void doAfterMeasure();                          // 测量完成后的清理

    void setExposureTime(int *_exposure_time);      // 设置曝光时间（us）
    int getExposureTime();                          // 获取曝光时间（us）

    bool getIsExposureOk();

    void inputDist(int _dist_val, enDistanceState _dist_stat);      // 数据输入：距离数值
    void inputTurnLampImg(int _img_idx, int _img_num);              // 数据输入：一张转灯图
    bool inputTurnLampOnce(int _img_count);                         // 数据输入：【转灯一次】消息
    void setTurnLampSaveSource(const QString &_source_name);        // 设置当前转灯存图来源
    QString turnLampSaveSource() const { return m_turnLampSaveSourceName; }
    void saveLatestTurnLampImages(int _img_count = G_TURN_LAMP_FRAME_COUNT,
                                  bool _is_aborted = false,
                                  enCaptureError _capture_error = captureError_NoError,
                                  const QString &_reason = QString()); // 保存最后一轮已有转灯图
    void saveAllCompleteTurnLampImages(int _img_count);             // 保存所有完整转灯图
    // 工程师模式“开启存图”的配套控制：关闭时停止接收新任务，并在后台排空队列后统一刷盘。
    void setTurnLampImageSaveEnabled(bool _enabled);
    void flushTurnLampImageSaveQueue();
    //void inputPupilDetectInfo(bool _is_succ, int _img_idx, stPupilInfo _pupil_info_r, stPupilInfo _pupil_info_l);   // 数据输入：瞳孔识别信息

    bool getOneFrameMem(uchar *&_img_data, int &_img_idx);      // 获取一帧图像的内存空间

    void startMeasure();        // 启动测量
    void judgeTurnLamp();       // 判断是否能开始转灯，若是，则发送转灯信号

    void jumpIntoMeasureStepLatter(enMeasureStep _step);            // 稍后进入指定测量步骤（通过信号跳转）   // NOTE: 需要实时检测当前步骤的跳转，不应调用此方法，有可能检测是否要跳转时为是但实际跳转时为否
    bool jumpIntoMeasureStepImmediately(enMeasureStep _step, bool _is_force = false);

    void savePreviewImages();       // 保存预览图

    std::vector<uchar *> *getImgSet();        // 获取用于计算的图集

    /**
     * @brief 执行算法政策
     * @param _result_set       当前结果集，可能会被删减
     * @param _vision           传入最后一个测量结果，可能会输出融合后的最终结果
     * @param _vision_abnormal
     * @return 流程的下一步：true: 结束并显示结果；false: 重新转灯
     */
    static bool executeAlgoPolicy(std::vector<stVisionValue> &_result_set, stVisionValue &_vision, stVisionAbnormal &_vision_abnormal, bool &_questionable);

signals:
    void sigGoIntoMeasureStep(enMeasureStep _into_step);
    void sigErrMsg(QString _err_str);

    void sigStartMeasure(); /* TODO: 私有 */

public slots:
    void slotIsExposureOk();

protected:
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    bool judgeCanIntoCalc();                // 检查图集，判断是否可以结束图像采集，进入计算阶段

    //bool checkImgSet(stImgSetInfo &_img_set_info, QString &_err_str);           // 检查图集，判断是否可以结束图像采集
    //bool checkImgSetDist(stImgSetInfo &_img_set_info, QString &_err_str);       // 检查图集的距离  // TODO: 0.5s 转一次灯，测距只要检查一次即可？或者加大测距频度？
    //bool checkImgSetDist_2(stImgSetInfo &_img_set_info, QString &_err_str);     // 检查图集的距离（各图的距离在景深内）

    void releaseTurnLampImageSets();                    // 释放转灯图集列表

    //
    const float MAX_STD_DEV_DIST        = 30;

    //
    CCaptureThread *m_captureThread {nullptr};

    CAlgoInvoker *m_algoInvoker {nullptr};

    std::vector<uchar *> *m_frameBuff = nullptr;                // 帧缓冲队列（注意要释放内存）   // NOTE: 抓图模块从这里获取内存，抓帧成功后图像数据就在这里

    //
    std::vector<stDistInfo> *m_listDistInfo = Q_NULLPTR;        // 距离信息队列

    std::vector<stFrameInfo> *m_listFrameInfo = Q_NULLPTR;      // 转灯图缓冲队列（可包含多次转灯的数据）
    std::vector<stImgSetInfo> *m_listImgSetInfo = Q_NULLPTR;    // 转灯图集队列（可包含多次转灯的数据）

    std::vector<uchar *> *m_imgSet = Q_NULLPTR;         // 最后一次转灯图集

    bool m_isIgnoreDist {false};
    bool m_isMultiMeasure {true};

    bool m_isExposureOk = false;    // 曝光时间是否已设置好

    unsigned char *m_img12 {nullptr};
    unsigned char *m_img18 {nullptr};

    QVector<QVector<stTurnLampImageInfo>> m_turnLampImageSets;      // 转灯图集列表（深度拷贝的数据，测量完成后须释放）
    QString m_turnLampSaveBatchName;                                // 当前测量批次目录名
    QString m_turnLampSaveSourceName;                               // 当前转灯存图来源目录名
    // 本次测量开始时解析一次 U 盘根目录；后续图片和算法状态保存任务仅使用该快照。
    // 不在后台保存线程里重复执行 mount -l，避免存图期间反复访问挂载信息。
    QString m_turnLampSaveUsbRoot;

};

#endif // CMEASURECTRL_H
