#ifndef CAPTURETHREAD_H
#define CAPTURETHREAD_H

#include <QThread>
#include <QImage>

#include <QVector>
#include <QElapsedTimer>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
//#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <functional>
#include <atomic>

#include "circlshowthread.h"
#include "util-common.h"
#include "CameraIntf.h"

// 抓图错误
enum enCaptureError {
    captureError_Unknown        = -1,
    captureError_NoError        = 0,    // 无错误
    captureError_FrameSetEmpty,         // 转灯图集为空
    captureError_FrameSingle,           // 转灯图集只有单帧
    captureError_FrameLoss,             // 转灯图集丢帧
    captureError_FrameExcess,           // 转灯图集超帧
    captureError_CameraUnusable,        // 相机不可继续使用
};

// 抓图线程步骤状态
enum enCaptureStep {
    captureStep_PupilDetect         ,       // 瞳孔识别
    captureStep_TurnLamp            ,       // 转灯
    captureStep_TurnLampFinished    ,       // 转灯结束
};
QString enumToText_CaptureStep(enCaptureStep _step);

// 回调：获取帧存储空间
using funcGetFrameMem = std::function< bool (uchar *&_img_data, int &_img_idx) >;

//
class CCaptureThread : public QThread
{
    Q_OBJECT
public:
    explicit CCaptureThread(QObject *parent = 0);
    ~CCaptureThread();

    void reset();

    void abordTurnLamp();       // 中止转灯

public:
    void run();

    inline bool isSyncFrame() { return m_isSyncFrame; }                 // 是否需要帧同步（每抓一帧图，就和外部同步一次）
    void setIsSyncFrame(bool _is_sync) { m_isSyncFrame = _is_sync; }

    void setIsUseRawImg(bool _is_use_raw) { m_isUseRawImg = _is_use_raw; }

    void setCallbackGetFrameMem(funcGetFrameMem _callback_get_frame_mem) { m_callbackGetFrameMem = _callback_get_frame_mem; }

    int countFrameSent() { return m_countFrameSent; }                   // 已发送的在等待处理的帧计数
    void setCountFrameSent(int _count) { m_countFrameSent = _count; }

    void setIsNeedRun(bool _is_need_run);
    bool getIsNeedRun();

    void releaseVectorImage(QVector<unsigned char*> *vector);

    void setCaptureStep(const enCaptureStep _capture_step_new);
    enCaptureStep getCaptureStep() { return m_captureStep; }

    unsigned int getCountLoop();

    bool getIsUnexpectlyQuit();

signals:
    /**
     * @brief 帧获得信号
     * @param _img_idx
     * @param _img_data
     * @param _img_num  图号（和灯号一致）
     */
    void sigFrameCaptured(int _img_idx, uchar *_img_data, int _img_num);

    void sigTurnLampOnce(int _img_count, bool _is_aborted = false);
    void sigCaptureErr(enCaptureError _err_code, QString _err_str);
    void sigOuterFrameSync(bool *_is_done_ptr = Q_NULLPTR);

protected:
    static const char * const S_CLASS_NAME;     // 本类的类名

    std::atomic<bool> isNeedRun {false};        // 是否需要运行。当其为 false 时，run() 函数应退出
    bool isUnexpectlyQuit;      // 是否意外退出循环

    bool m_isSyncFrame = false;             // 是否需要帧同步（每抓一帧图，就和外部同步一次）
    bool m_isUseRawImg = false;

    funcGetFrameMem m_callbackGetFrameMem = nullptr;
    int m_countFrameSent = 0;               // 已发送的在等待处理的帧计数

    static Util::CCircularQueue<bool> *syncFlagList;    // 帧同步标志队列

    enCaptureStep m_captureStep = captureStep_PupilDetect;
    QElapsedTimer elapsedCapture;

    unsigned int countLoop = 0;             // 循环次数

    unsigned int countSoftTrigger = 0;              // 软触发超时次数
    unsigned int countAllFrame = 0;                 // 所有帧计数
    unsigned int countSoftFrame = 0;                // 软触发帧计数
    unsigned int countOuterFrameSync = 0;           // 外部帧同步计数
    unsigned int countOuterFrameSyncTimeout = 0;    // 外部帧同步超时计数

    bool m_isTurnlampTriggered = false;     // 转灯是否已触发（指令是否已发送）
    int m_countTurnlampTimeout = 0;         // 转灯抓图期间的相机超时次数
    int m_imgNum = -1;                      // 转灯图计数（从0开始）    // NOTE: 下位机是按灯号顺序转灯的，所以这个计数就等于灯号

    int m_countCameraErr = 0;               // 相机错误计数

    int frameIntervalMs = 100;              // 每帧间隔（ms）

    bool m_isTriggerDelaySet {false};       // 触发延时是否已设置

    void outerFrameSync(bool _is_wait);     // 触发外部帧同步      // @_is_wait：是否等待应答

    stCameraStatInfo getOneFrame(int _timeout, bool _is_turn_lamp);         // 从相机获取一帧图像
    stCameraStatInfo triggerTurnLamp(const int _timeout_ms_clear_buff);     // 触发转灯
    void turnLampInit();
    void setCountLoop(unsigned int _count);

};

/// ============================================================

// 抓图模块
class CCapture : public QThread         // TODO: 目前只有 lampcalibrate 模块使用，与 CaptureThread 统一 ？
{
    Q_OBJECT
public:
    explicit CCapture(QObject *parent = 0);
    ~CCapture();

    bool isUseCameraApiProcess;

    int getImgWidth();
    int getImgHeight();
    int getImgSize();

    void setCameraHandle(int _camera_handle);

protected:
    void run();

    int cameraHandle = 0;

    int imgWidth = 0;
    int imgHeight = 0;
    int imgSize = 0;

signals:
    void sigGetImg(uchar *_img_raw, int _capture_count);
    void sigRunEnd(QString _err_msg);
};


#endif // CAPTURETHREAD_H
