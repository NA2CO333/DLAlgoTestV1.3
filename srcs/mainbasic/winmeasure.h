#ifndef CAMERA_H
#define CAMERA_H

#include <stdio.h>
#include <stdlib.h>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QThread>
#include <QVector>
#include <QStringList>
#include <QLabel>
#include <QSettings>
#include <QMetaType>
#include <QPainter>
#include <QTimer>
#include <QElapsedTimer>

#include "baseform.h"
#include "capturethread.h"
#include "algo-invoker.h"
#include "myserialport.h"
#include "detectbarcode.h"
#include "enhancementimag.h"
#include "distancedetect.h"
#include "myeditline.h"
#include "distcalibration.h"
#include "camerainit.h"
#include "measurestatview.h"
#include "statusbarform.h"
#include "eyelimitmark.h"
#include "measurectrl.h"
#include "waitingmovie.h"
#include "soundintf.h"

//
namespace Ui {
class WinMeasure;
}

// 标准距离
#define STD_DISTANCE            970

// 距离补偿（内部的固定的，来自旧代码）
#define DIST_OFFSET_INTERNAL    30

// 前或后景深（前后景深当作相等，这是与镜头关联的固定值，与“距离允差”配置项不同）
#define FIELD_DEPTH             30

//
extern int g_distanceVal;           // 显示给用户的距离值（毫米）
extern bool g_isTurnLampTestMode;   // 是否转灯测试模式

// 运行模式         // TODO: 这个模式的定义有点乱，需对业务逻辑进行重新梳理和优化     // 去掉这个模式的全局属性，每个窗体自建 enModeFlag modeFlags（模式标识，支持 or 合并）
enum enOperationMode
{
    // TODO: 废弃这个属性，用 enPatientSource + 被测者的 isBatch 等属性来代替？

    operationMode_Unknown           = -1,   // 未知
    operationMode_NormalMeasure     = 0,    // 常规测量                         // TODO: 新增“是否批量”属性来区分？
    operationMode_HistoryRecord,            // 门诊记录列表                      // TODO: 这个根据“上一个窗口”信息来区分？
    operationMode_BatchRecord,              // 批量模式记录列表
    operationMode_BatchScreen,              // 批量筛查模式                      // TODO: 新增“是否批量”属性来区分？
    operationMode_InputMeasure,             // 输入型测量，如扫码、蓝牙控制测试     // TODO: 这个好像没什么用，替换为“批量模式”即可？

    //normalReTest,           // 常规重新测量                       // TODO: 根据是否有数据库主键来判断是否新增的测量？
    //batchReTest,            // 批量模式重新测量
};

//
class CFrameDrawer;
class CMeasureCtrl;
class CExposureAdjuster;

//
class WinMeasure : public CBaseWidget
{
    Q_OBJECT
public:
    explicit WinMeasure(QWidget *parent = 0);
    ~WinMeasure();

    void testAlgo();

    CMeasureCtrl *measureCtrl() { return m_measureCtrl; }
    CExposureAdjuster *exposureAdjuster() { return m_exposureAdjuster; }
    CDistanceDetect *distanceDetect() { return m_distanceDetect; }

    enhancementImag *enhanImg_thread = Q_NULLPTR;
    CAlgoInvoker *m_algoInvoker = Q_NULLPTR;
    QThread *mThreadAlgo = Q_NULLPTR;
    CameraInitThread *camerainit = Q_NULLPTR;
    QTimer *timerBaseBoardQuery = Q_NULLPTR;     // 底板信息查询定时器
    QTimer *screenTimer = Q_NULLPTR;

    static bool isOpened() { return s_isOpened; }   // 是否已打开

    void setPatient(const CPatient &_pat);
    const CPatient currPatient() { return m_patient; }

    inline enDistanceState getDistanceState() {
        return m_distanceState;
    }
    inline bool getIsDistanceNearFit() {            // 距离是否接近合适（未达到可转灯范围，但是可以进行瞳孔识别）
        bool is_dist_near_fit = ((getDistanceState() >= distStat_FitNear) && (m_distanceState <= distStat_FitFar));
        return (getIsIgnoreDist() || is_dist_near_fit);
    }
    inline bool getIsDistanceFit() {                // 距离是否合适
        bool is_dist_fit = (distStat_Fit == m_distanceState);
        return (getIsIgnoreDist() || is_dist_fit);
    }

    void setPatientSource(enPatientSource _patient_source) { m_patientSource = _patient_source; }

    enCameraStat init_SDK();
    void musicControl(bool _is_opened);             // 音乐开关
    void playVoicePrompt(enVoicePrompt _voice);     // 播放语音提示
    void coloredLampControl(bool _is_opened);       // 彩灯开关

    static void getDevStat(std::string &stat);
    static void getRunStat(std::string &stat);
    static void setDevStat(const std::string);
    static void setRunStat(const std::string);

    static enOperationMode getOperationMode();
    static void setOperationMode(enOperationMode _mode);

    static enAgeRange getCurrentAgeRange();
    static enAgeRange getAgeRangeLimited(const enAgeRange &_age_range);

    bool getIsIgnoreDist() { return isIgnoreDist; }     // 是否忽略距离
    void setIsIgnoreDist(bool _is_ignore);

    void setIsDistCalibration(bool _is_dist_calibration);

    void setLampPwrOpened(bool _is_opened);         // 设置光源电源开关

    /**
     * @brief 发送转灯指令
     * @param _interval 硬触发间隔，小于等于 0 为无效值，表示取配置值
     */
    void sendTurnLampCmd(int _interval = -1);

    qint64 writeCaliSerial(const char *_data, qint64 _size);
    QByteArray readCaliSerialAll();
    void clearCaliSerial(QSerialPort::Direction _direction);

    void queryInfosFromBaseBoard();

    void continueMeasuring();

//    QMutex dLocker,rLocker;

    qint64 elapsedMeasure() const;              // 测量计时

    bool isPressKeySave = false;            // 按下物理按键时存图

    int countPupilDetectSent = 0;           // 已发送的在等待处理的瞳孔计算信号计数

    /**
     * @brief 设置灯珠电流等级（通过定时器延后调用）
     * @param _delay_ms 延时毫秒数
     * @param _is_reset 是否强制（若是，则不检查是否重复设置）
     */
    void setLedLevel(int _delay_ms, bool _is_force = false);

    void goBack();

    /**
     * @brief 使测量过程进入指定步骤
     * @param _into_step
     * @return 是否成功
     */
    bool goIntoMeasureStep(const enMeasureStep _into_step, bool _is_force = false);     // NOTE: 在本函数内部不要递归调用自身，而是要通过信号槽跳转步骤，否则可能导致本函数内部状态错乱

    inline enMeasureStep getMeasureStep() { return m_measureStep; }

    void setIsFocusMode(bool _is_focus_mode);

    static bool getMusicStateCfg(bool _is_reload = false);      // 获取：本界面的“音乐”按钮是否是打开状态
    static void setMusicStateCfg(bool _stat);                   // 设置：本界面的“音乐”按钮是否是打开状态

    inline int countTurnLamp() { return m_countTurnLamp; }          // 转灯次数
    inline int countAlgoCallback() { return m_countAlgoCallback; }  // 算法回调计数

signals:
    void sendSIGNAL(enSysSignal _sys_signal);

    void sigDistanceNotice(int _current_dist, enDistanceState _state);
    void sigCaptureRestarted();
    void sendBlueToothData(QString);    //2020.10.12  tao

    void sigPupilDetect(uchar *_img_data, int _img_idx, enAgeRange _age_range, bool _is_need_calc_expo);
    //void sigCalcVision(std::vector<uchar *> *_img_list, const CPatient &_patient);
    void sigGotBarcodeImg(uchar *_img_data);
    void sigCalcImgInfo(unsigned char *_img_data);

public slots:
    void slot_upLoadWork_MeasureCtrl(std::string cmd);
    void slot_captureThread_FrameCaptured(int _img_idx, uchar *_img_data, int _img_num);
    void slot_captureThread_finished();
    void slotPhysicButtonPressed();         // 槽函数：物理按键被点击

protected Q_SLOTS:
    void slot_serialBaseBoard_CmdReceived(int _cmd_id, QByteArray _pkg_data);
    void slot_timerBaseBoardQuery_timeout();
    void slot_timerSonarCloseDelay_timeout();
    void slot_screenTimer_timeout();
    void slot_this_DistanceNotice(int _dist_val, enDistanceState _dist_state);
    void setDistanceDetectState(bool _is_opened, bool _is_init = false);
    void showWaiting(bool _is_show);
    void slot_ResetCamera();
    void slot_barcodeDetect_DetectionResult(bool _is_succ, QString _decode_data);
    void slot_distanceDetect_DistanceChanged(int _new_dist);
    void slot_distanceDetect_CheckSensorTypeFinished();
    void slotCameraInitFinished(enCameraStat _status, QString _msg);
    void slot_timerUpdateDebugInfo_timeout();
    void slot_captureThread_TurnLampOnce(int _img_count, bool _is_aborted);
    void slot_algoInvoker_PupilDetectionResult(uchar *_img_data, int _img_idx, bool _succ, stPupilInfo _pupil_info_r,
                                         stPupilInfo _pupil_info_l, int _avg, bool _over_expo);
    //void slot_algoInvoker_CalcVisionFinished(const enCalcResultState _calc_result_state, const stVisionValue _vision, const stVisionAbnormal _vision_abnormal);
    void slot_algoInvoker_CalcVisionCallbackReceived(const enCalcResultState _calc_result_state, const int _round_idx,
                                             const stVisionValue _vision, const stVisionAbnormal _vision_abnormal,
                                             const std::vector<stVisionValue> &_result_set, bool _is_finished, bool _is_questionable);
    void slot_algoInvoker_AlgoErr(enAlgoErrType _algo_err_type, QString _msg);
    void slot_algoInvoker_MsgNotify(QString _msg);
    void slot_captureThread_CaptureErr(enCaptureError _err_code, QString _err_str);
    void slot_measureCtrl_GoIntoMeasureStep(enMeasureStep _step);
    void slot_measureCtrl_ErrMsg(QString _err_str);
    void slot_measureCtrl_StartMeasure();
    void slot_exposureAdjuster_MsgNotify(QString _msg);

protected:
    //void keyPressEvent(QKeyEvent *event);
    void showEvent(QShowEvent*);
    void hideEvent(QHideEvent*);
    //void paintEvent(QPaintEvent *event);

    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    static bool s_isOpened;             // 是否已打开    // TODO: 改用系统管理模块添加的 systemBusy（系统忙）状态来替代？

    MySerialPort *serialBaseBoard = Q_NULLPTR;

    bool isIgnoreDist = false;          // 是否忽略距离

    CPatient m_patient;

    enPatientSource m_patientSource = patientSource_Unknown;

    CCaptureThread *m_captureThread = Q_NULLPTR;
    CMeasureCtrl *m_measureCtrl = Q_NULLPTR;
    CExposureAdjuster *m_exposureAdjuster = Q_NULLPTR;

    bool isBarcodeStat = false;             // 是否“正在扫码”状态
    bool isDistCalibration = false;         // 是否“距离校准”状态

    CWaitingMovie *loadingMovie = Q_NULLPTR;    // 等待动画

    CvPoint pyr1, pyr2;
    int radius1, radius2;
    static bool musicStateCfg;     // （配置的）音乐是否已打开
    int timeoutError;

    static std::string dev_stat;    //2020.10.12  tao
    static std::string run_stat;    //2020.10.12  tao

    CFrameDrawer *frameDrawer = Q_NULLPTR;
    QThread *mThreadDrawFrame = Q_NULLPTR;

    CEyeLimitMark *eyeLimitMark;    // 取景框

    int stateOfCmd;          // 由旧代码整理：0-发送【超声指令】后，1-发送【转灯指令】后，2-发送【打开超声指令】后，3-发送【关闭超声指令】后，4-发送其它指令后
    // TODO: 好像没什么用？把这破东西扔了！

    QElapsedTimer m_elapsedMeasure;         // 测量计时
    QElapsedTimer m_elapsedFrameRate;       // 帧率计时

    CDistanceDetect *m_distanceDetect {nullptr};    // 距离检测模块
    QThread *threadDistDetect;
    int distanceFitCount = 0;

    int m_countMeasure = 0;                 // 测量次数（每次进入测量界面时初始化，每次 Ready 步骤前的初始化时 +1）
    int m_countTurnLamp = 0;                // 转灯次数（每次进入测量界面时初始化，每次转灯 +1）   // NOTE: 并未能确保每次从 Ready 步骤到 MeasureFinished 步骤的过程中只转一次灯
    int m_countAlgoCallback = 0;            // 算法回调计数
    int m_countFrameLoss = 0;               // 转灯图缺帧计数

    bool m_isWaitingForAlgoFinish = false;  // 是否正在等待算法结束

    bool isStatisticalShown = false;        // 推值是否已显示（避免在抓图循环中多次触发显示推值）
    QElapsedTimer mTimeFirstPupilDetected;  // 第一次距离合适时间

    int countPupilDetection = 0;            // 识别瞳孔次数（推值所用，旧算法里只是描圆的识别）
    int countPupilDetectionSucc = 0;        // 识别瞳孔成功次数（失败后清零）

    QTimer *timerUpdateDebugInfo = Q_NULLPTR;   // 更新调试界面信息定时器      // TODO: 重构，把全局变量改为信号，去掉本定时器

    CCalcImgInfo *calcImgInfo = Q_NULLPTR;
    QThread *threadCalcImgInfo = Q_NULLPTR;
    float maxClarity = 0;

    CDistCalibration *distCalibration = Q_NULLPTR;

    int distInterval = 100;

    static enOperationMode m_operationMode;
    static enAgeRange currentAgeRange;      // 当前年龄段（最后一次测量的年龄段）

    bool isPupilDetectUserTriggerMode = false;          // 是否“瞳孔识别用户触发”测试模式   /* 此模式下，用户每按一次物理按键，则识别瞳孔一帧，并记录识别统计信息，并存图 */
    bool isPupilDetectUserTriggered = false;            // “瞳孔识别用户触发”模式下用户已触发
    int pupilDetectUserTriggerCount = 0;                // “瞳孔识别用户触发”模式下的总触发数
    int pupilDetectUserTriggerSuccCount = 0;            // “瞳孔识别用户触发”模式下的识别成功数

    bool isShowRawImg = false;                          // 是否显示原始图像（不增强）
    bool isQtDraw = false;                              // 是否使用 Qt 部件绘帧
    QElapsedTimer elapsedFrameGot;

    detectBarcode *barcodeDetect = Q_NULLPTR;
    QThread *threadBarcode = Q_NULLPTR;
    bool isBarcodeImgSent = false;
    int barcodeSendCount = 0;
    bool isForceNotFrameSync = false;       // 是否强制不要帧同步

    int pwmDutyPercent = 100;
    int lastDistTipState = -1;              // 上一次距离提示状态（见赋值代码的注释）      // 改为 enDistanceState 类型？和 m_distanceState 重复？

    enMeasureStep m_measureStep {measureStep_Unknow};
    enDistanceState m_distanceState = distStat_Unknown;

    QLabel *lblMeasureStep = Q_NULLPTR;
    CMeasureStatView *measureStatView = Q_NULLPTR;

    enCalcResultState m_calcResultState = calcResultState_Fail;     // 结果计算是否成功（当前转灯的）
    QString m_resultErrMsg;                                         // 结果错误信息（当前转灯的）
    std::vector<stVisionValue> m_resultList;                        // 结果列表（本次测量的所有转灯的结果）
    stVisionValue m_visionValue;                                    // （最终的）屈光数值
    stVisionAbnormal m_visionAbnormal;                              // （最终的）屈光异常
    bool m_isFinished {false};                                      // 算法模块是否已得出结果
    bool m_isResultQuestionable {false};                            // （最终的）结果是否存疑

    bool isExposureSetted = false;

    int countDistSent = 0;

    bool m_isAutoTurnLamp = true;               // 是否自动转灯

    bool isFocusMode = false;                   // 是否调焦模式（用于生产调校）

    QTimer *timerSonarCloseDelay;               // 声纳关闭延时定时器

    int countRePowerOn = 0;                     // 相机重上电计数

    int countGazeOver = 0;                      // 固视不准的次数          // TODO: 这些计数变量好像太多了，待梳理优化？

    //QThread *threadSerialBaseBoard = Q_NULLPTR;

    bool m_isTurnLampTest {false};
    // 转灯中再次点击时，先保存中止轮次，再启动新的手动重启轮次。
    bool m_isForceRestartTurnLamp {false};
    bool m_isStartingManualRestartTurnLamp {false};

    QTimer *m_ledSetTimer {nullptr};        // 延后设置 LED 电流的定时器
    bool isLedSetted = false;               // 是否 LED 电流已设置（防止重复调用）
    bool isLedSettingDelaying = false;      // 是否在调用 LED 电流设置函数的延时期间

    int m_fixedExposure {-1};               // 固定的曝光时间（单位 us，小于 0 表示非固定曝光）（调试用，仅调试模式下有效）

    bool m_isOpticalTypeConfirmed {false};  // （L形视筛箱）光路类型是否已确认

    bool m_isLastTurnLampeFramePupilFound {false};   // 最后一个转灯帧是否能发现瞳孔

    // 最终返回“找不到瞳孔”后，是否正在等待底层转灯停止回调。
    // 等待期间禁止普通流程再次启动新的转灯轮次。
    bool m_isWaitingPupilNotFoundAbort {false};

    // 首张C800/129 ROI失败后，是否正在等待当前物理轮停止以重试下一轮。
    // 该标志只覆盖一次提前换轮请求，避免迟到回调重复启动转灯。
    bool m_isWaitingAnchorRetryAbort {false};

    //
    void setFixedExposure(int _expo);       // 设置固定的曝光时间（单位 us，小于 0 表示非固定曝光）（调试用，仅调试模式下有效）

    //
    void doOnDistanceReceived(int _new_dist, int _surce = 0);

    //
    void updateUi();            // 更新UI，包括：布局、可见性/有效性、样式、语言

    void startMeasure();        // 启动测量过程（本窗体显示期间可能被执行多次）   // TODO: 把这些逻辑控制代码全部移到 CMeasureCtrl 类，本类只保留 UI 直接相关的代码
    void stopMeasure();         // 结束测量过程

    void initMeasure();         // 初始化测量相关变量（在“启动测量过程”的前期调用）
    // 正式测量被中断后清理旧算法会话，避免下一次测量继承旧轮次和结果。
    void resetInterruptedMeasurementSession();
    void startTurnLamp();       // 开始转灯

    void processBaseBoardCmd(int _cmd_id, QByteArray _pkg_data);
    void processDistanceCmd(QString _cmd_hex);

    void saveImgToUdisk(QString _relative_path, uchar *_img_data, QString _udisk_path = "");

    void startCaptureThread();                  // 启动抓图线程
    void stopCaptureThread();                   // 结束抓图线程

    void distCalibrationCheckAndApply();
    void setIsQtDraw(bool _is_qt_draw);
    void setIsUseFrameBuff(bool _use_frame_buff);   // 设置是否启用 Linux 的帧缓存    // TODO: 移到全局公用模块

    void setIsBarcodeStat(bool _is_barcode_stat);

    void showResult(const std::vector<stVisionValue> &_results);    // 测量完成后显示测量结果
    void simulateMultiResults(CPatient &_pat, const stVisionValue &_vision, const double _vision_prec,
                              const bool _is_has_right, const bool _is_has_left);                                   // 虚构多次测量结果
    //void showMonthAgeVision(std::vector<unsigned char *> &_img_list, enAgeRange _age_range, QDate _birth_date);     // 显示按月龄视力估值

    void createObjects();

    /**
     * @brief 判断是否能进入指定的测量步骤，若可，则进入
     * @param _into_step
     * @param _is_err   是否错误    // TODO: 不能进入不一定需要判定为错误，如有些重复进入的问题，流程逻辑可能没错，只是防止重复进入比较麻烦，在这里处理兜底？
     * @param _err_str
     * @return 是否能进入
     */
    bool judgeIntoMeasureStep(const enMeasureStep _into_step, bool &_is_err, QString &_err_str);

    /**
     * @brief 执行进入指定步骤的工作（真正进入该步骤）
     * @param _into_step
     * @return
     */
    bool doIntoMeasureStep(const enMeasureStep _into_step);

    void emitPupilDetect(uchar *_img_data, int _img_idx, bool _is_need_calc_expo);

    void doSetLedLevel(bool _is_force = false);
    void updateView_btnMusic();

    void resetSpeedCounter();

    void updateView_isFocusMode();                      // 根据是否“调焦模式”调整界面
    void updateView_DebugWidgets(bool _is_visible);     // 更新调试所用 UI 部件的显示状态

    void updateView_btnOpticalType();                   // 刷新视图 - btnOpticalType
    void updateView_btnSingleDualEye();                 // 刷新视图 - btnSingleDualEye
    void updateView_btnHighDiopter();                   // 刷新视图 - btnHighDiopter

    static void setCurrentAgeRange(enAgeRange _age_range);

    //void showStatisticalValues(int _age_range);     // 显示统计值

    /**
     * @brief 检查结果的可信度，如果超出范围弹出提示是否重测
     * @param list：结果信息
     * @return 若可信，返回 true，否则返回 false
     */
    bool checkResultReliability(const stVisionValue list);

    void paintSingleEyeCover(QLabel *_label);           // 绘制单眼遮盖的图案

    void setDebugPanelExpanded(bool _is_expanded);      // 设置调试面板的展开状态
    bool isDebugPanelVisible();                         // 获取“调试面板是否已显示”

    void doOnOpticalPathTypeChanged(enOpticalPathType _optical_type);       // 光路类型改变事件处理
    void doOnSingleDualEyeChanged(enSingleDualEyeMode _single_dual_eye);    // 单双眼属性改变事件处理

    void autoCheckOpticalType(const int _curr_dist);    // 自动检测光路类型

    void cameraRePowerOn();                             // 相机重新上电

    void doOnAlgoResultError();                         // 算法结果错误时的处理

private slots:
    void on_btnGoBack_clicked();
    void on_btnScanBarcode_clicked();
    void on_btnMusic_clicked();
    void on_ckbIsTurnLampTest_clicked(bool _checked);
    void on_btnGoIntoDetect_clicked();
    void on_btnTurnLamp_clicked();
    void on_ckbLightOn_clicked(bool checked);
    void on_ckbFixExposure_clicked(bool _checked);
    void on_edtExposure_textEdited(const QString &_arg1);
    void on_ckbIgnoreDistance_clicked(bool checked);
    void on_btnSaveDistCalitrData_clicked();
    void on_btnStart_clicked();
    void on_btnSetGain_clicked();
    void on_ckbIsShowRawImg_clicked(bool checked);
    void on_ckbIsPressKeySave_clicked(bool checked);
    void on_ckbIsQtDraw_clicked(bool checked);
    void on_cbbPupilAlgoVer_currentIndexChanged(int index);
    void on_ckbIsUserTriggerPupilDetectMode_clicked(bool checked);
    void on_ckbDistDetectLight_clicked(bool checked);
    void on_btnCameraRestartCnt_clicked();
    void on_btnSetPwmDuty_clicked();
    void on_btnDebugPanelExpand_clicked();
    void on_btnOpticalType_clicked();
    void on_btnSingleDualEye_clicked();
    void on_btnHighDiopter_clicked();
private:
    // 完成最终未找到瞳孔后的测量收尾，统一处理取消、重置和Ready切换。
    void finishPupilNotFoundSession();
    Ui::WinMeasure *ui;
};

// 帧绘制
class CFrameDrawer : public QObject
{
    Q_OBJECT

public:
    explicit CFrameDrawer(QWidget *_parent, int _left, int _top, int _width, int _height);
    ~CFrameDrawer();

    void setIsQtDraw(bool _is_qt_draw);

    /**
     * @brief 刷新帧图像
     * @param _is_draw_circle
     * @param _is_qt_draw
     * @param _flip_mode        翻转模式，-2：不处理，[-1~1]：与 OpenCV 的 cvFlip() 函数的 flip_mode 参数含义一致：-1:同时垂直和水平翻转，0:垂直翻转，1:水平翻转
     * @note core_c.h -> cvFlip() 里的备注“around horizontal (flip=0)”是指绕着水平线即x轴翻转，就是垂直翻转（上下翻转），而不是指水平翻转（左右翻转）！
     */
    void updateFrame(bool _is_draw_circle, bool _is_qt_draw, int _flip_mode);

    // 拷贝图像数据到屏幕缓存
#if (1 == OS_TYPE || 2 == OS_TYPE)
# ifdef SURPORT_FRAME_BUFFER
    void copyToFrameBuffer(char * const fbp, fb_var_screeninfo &scrinfo);
# else
    void copyToFrameBuffer(char * const fbp);
# endif
#endif

    void pushImage(uchar *_img_data, bool _is_need_enhance);    // 推送图像给本对象

    bool detectPupilAndDrawCircle(uchar *_img_data);            // 识别瞳孔并描圆到【待绘制的图像】中

    void reset();

    void setCountFrame(unsigned int _count);
    int getCountFrame();

    void setPupilInfo(bool _succ, const stPupilInfo &_pupil_info_r, const stPupilInfo &_pupil_info_l);

    QGraphicsView *getViewer();

    QRect getImgViewRect();

protected:
    IplImage *imgDrawingRaw = Q_NULLPTR;    // 用于绘制到界面上的图像（3通道的）的原图（未缩放的）（【待绘制的图像】）
    IplImage *imgDrawingView = Q_NULLPTR;   // 最终绘制到界面上的图像（可能需要缩放适配屏幕尺寸等处理）

    uchar *computeImgBuf = Q_NULLPTR;           // 加强处理后的图像

    uchar *imgDataMagnifiedR = Q_NULLPTR;       // 放大后的瞳孔图像的数据
    uchar *imgDataMagnifiedL = Q_NULLPTR;       // 放大后的瞳孔图像的数据

    QGraphicsView *gvFrame = Q_NULLPTR;
    QGraphicsScene *m_pGraphScene = Q_NULLPTR;
    QGraphicsPixmapItem *m_pGraphPixmapItem = Q_NULLPTR;
    //QVector<QRgb> grayColourTable;

    int countFrame = 0;    // 刷帧计数

    stPupilInfo pupilInfoR;     // 瞳孔信息 - 右眼    // NOTE: 此处的瞳孔信息的眼别，与实际眼别无关。应避免测量模块的眼别逻辑扩散到这里。
    stPupilInfo pupilInfoL;
    bool isPupilInfoValid = false;

    void drawFrame();

    void drawPupilCircle();
    void drawPupilCircle(stPupilInfo _pupil_info_r, stPupilInfo _pupil_info_l);

};

/// =============
///

// 功能码 "dev_stat" 对应 data 字段如下：
const std::string BUSY              = "busy";                       //拍摄中
const std::string AVAILABLE         = "available";                  //空闲中

// 功能码 "run_stat" 对应 data 字段如下：
const std::string TOOFAR            = "too far";                    //太远
const std::string TOOCLOSE          = "too close";                  //太近
const std::string SUITABLE          = "suitable distance";          //请保持不动
const std::string OUT_OF_RANGE      = "pupil out of range";         //瞳孔尺寸超过范围
const std::string UNDETECTED        = "failed to detect pupil";     //无法检测瞳孔
const std::string OUT_OF_TIME       = "measure timeout";            //测量超时
const std::string PUPILTOOSMALL     = "pupil too small";            //瞳孔过小
const std::string NOTRUNNING        = "camera not running";         //不在运行状态
const std::string DETECT_PUPIL      = "dectect pupil";              //检测瞳孔
const std::string GRAB_FRAME        = "grab frame";                 //抓取图像      // TODO: 指当前满足转灯条件？
const std::string CALCULATING       = "calculating";                //计算结果
const std::string MEASURE_SUCC      = "measure succ";               //测量成功
const std::string MEASURE_FAIL      = "measure fail";               //测量失败

//“stat”字段，指定操作的状态
//const string STAT_SUCC  = "succ";   // 成功
//const string STAT_FAIL  = "fail";   // 失败

#endif // CAMERA_H
