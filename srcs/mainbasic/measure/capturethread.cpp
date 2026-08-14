//获取相机图片并刷到显示屏，图片在resultByte给runtask计算，改动需谨慎

#include "capturethread.h"

#include <sys/prctl.h>

#include <QDebug>
#include <QTime>
#include <QMetaType>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"

#include "winmeasure.h"
#include "mainwindow.h"
#include "global.h"
#include "settings/settings.h"
#include "tool.h"
#include "engineermode/engineermode.h"
#include "windowsmanager.h"

// 抓图线程逐灯位日志默认关闭；需要现场逐帧排查时再单独打开。
#ifndef ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#define ENABLE_DL_MERGE_TEST_FULL_VERBOSE 0
#endif
#ifndef ENABLE_CAPTURE_FRAME_VERBOSE_LOG
#define ENABLE_CAPTURE_FRAME_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

#if (OS_TYPE == 2)
# include "testdesktop.h"
#endif

//using namespace cv;

extern int g_read_fps;          //统计帧率
extern int g_tmpcount;

//
//CirclShowThread *circlShow = new CirclShowThread;       // TODO: QApplication未创建就在这里创建依赖于消息循环的对象，可能有问题
//detectBarcode *detBarcode = new detectBarcode;          // TODO: QApplication未创建就在这里创建依赖于消息循环的对象，可能有问题

//
bool pupil_Vec_State = true;
bool testState = true;

QString Old_Id;

//
QString enumToText_CaptureStep(enCaptureStep _step)
{
    switch (_step) {
    case captureStep_PupilDetect        : return "PupilDetect";
    case captureStep_TurnLamp           : return "TurnLamp";
    case captureStep_TurnLampFinished   : return "TurnLampFinished";
    }
    return "??";
}

//
Util::CCircularQueue<bool> *CCaptureThread::syncFlagList = new Util::CCircularQueue<bool>(36, 1);
const char * const CCaptureThread::S_CLASS_NAME = CCaptureThread::staticMetaObject.className();

CCaptureThread::CCaptureThread(QObject *parent) :
    QThread(parent)
{
    //
    reset();

}

CCaptureThread::~CCaptureThread()
{
    delete syncFlagList;
    syncFlagList = Q_NULLPTR;

}

void CCaptureThread::reset()
{

}

void CCaptureThread::outerFrameSync(bool _is_wait)
{
    countOuterFrameSync++;

    if (countOuterFrameSync % 3 == 0)      // 测距帧率倍减
    {
        // 调用外部帧同步过程
        bool *is_done_ptr = (_is_wait ? syncFlagList->nextOne() : Q_NULLPTR);
        emit sigOuterFrameSync(is_done_ptr);

        // 等待应答
        if (_is_wait) {
            long timeout_usec = frameIntervalMs - elapsedCapture.elapsed() - 5;     // TODO: 这里该减多少？增加等待时间，好像可以使应答超时率降低？
            bool is_in_time = Util::sleepUntil(is_done_ptr, true, timeout_usec);
            if (!is_in_time) {
                countOuterFrameSyncTimeout++;
            }
        }

        //
        if (is_done_ptr) {
            *is_done_ptr = true;
        }
    }
}

stCameraStatInfo CCaptureThread::getOneFrame(int _timeout, bool _is_turn_lamp)
{
    //qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered"
    //         << ", _is_turn_lamp = " << Util::bool2str(_is_turn_lamp) << ", timeout = " << _timeout;

    //
    //static const int TIMEOUT_RETRY  = 3;
    static const int TIMEOUT_RETRY  = 1;

    unsigned char *raw_buff = Q_NULLPTR;
    stCameraStatInfo stat_info(cameraStat_Succ);

    for (int i = 0; i < TIMEOUT_RETRY; i++)
    {
        // 耗时优化的测试：获得第一帧后，拷贝到替代帧，后续帧不再读取，减少 USB 操作，检查耗时的减少量
        //static uchar *img_instead = nullptr;
        //if (!img_instead)
        {
            // 从相机读取一帧
            stat_info = g_CameraIntf->getImageBuffer(&raw_buff, _timeout);
        }
        //else {
        //    raw_buff = img_instead;
        //    stat_info.cameraStat = cameraStat_Succ;
        //}

        //
        if (cameraStat_Succ == stat_info.cameraStat) {
            countAllFrame++;

            // 调用相机 SDK 处理图像        // TODO: 如果不经过这一部处理，图像是上下翻转的？
            bool succ_got_data = false;
            uchar *img_data = Q_NULLPTR;
            int img_idx = -1;
            if (m_callbackGetFrameMem) {
                succ_got_data = m_callbackGetFrameMem(img_data, img_idx);
                //qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ... img_idx = " << img_idx << ", &img_data: " << reinterpret_cast<uintptr_t>(img_data);
            } else {
                // TODO:

            }
            if (!succ_got_data) {
                // 得不到帧图像的存储空间，没法继续执行
                //stat_info.cameraStat = cameraStat_Fail;    // TODO:
                logWarning("CCaptureThread::getOneFrame(): camera error: got mem failed!", CGlobal::LOG_CAPTURE);
                break;
            }

            //if (!m_isUseRawImg)
            {
                stat_info = g_CameraIntf->imageProcess(raw_buff, img_data);       /* 迈德威视 API 的注释里说这个处理之后得到的是3通道图像，但是经实测，得到的还是单通道图像 */
                //TODO: 这个 RAW 图像 其实是 Bayer 或 YUV 格式，并不能直接使用，须先用该函数转为位图格式？而且据开发手册，它的处理结果并非不是线性的？不过实测是否调用几乎看不出差别？是因为图像是黑白的？
                if (cameraStat_Succ != stat_info.cameraStat) {
                    logWarning("CCaptureThread::getOneFrame(): camera error: process failed!", CGlobal::LOG_CAPTURE);
                    break;
                }
            }
            //else      // TODO: 这个图像处理（CameraImageProcess()）后的图其实并非不是“原图”？
            //{
            //    static const int n = (gCameraIntf->getMediaType() == CAMERA_MEDIA_TYPE_MONO8 ? 1 : 3);
            //    memcpy(frame_data, raw_buff, gCameraIntf->getImgWidth() * gCameraIntf->getImgHeight() * n);
            //    cv::Mat mat_raw = cv::Mat(gCameraIntf->getImgHeight(), gCameraIntf->getImgWidth(), CV_8UC1, raw_buff);
            //    cv::Mat mat     = cv::Mat(gCameraIntf->getImgHeight(), gCameraIntf->getImgWidth(), CV_8UC1, frame_data);
            //    cv::flip(mat_raw, mat, 0);      // 垂直翻转
            //}

            // 释放相机 SDK 图像内存
            stat_info = g_CameraIntf->releaseImageBuffer(raw_buff);
            if (cameraStat_Succ != stat_info.cameraStat) {
                logWarning("CCaptureThread::getOneFrame(): camera error: release failed!", CGlobal::LOG_CAPTURE);
                break;
            }

            //
            if (_is_turn_lamp) {
                m_imgNum++;
#if ENABLE_CAPTURE_FRAME_VERBOSE_LOG
                qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): turnLamp image " << m_imgNum << " got!";
#endif
            }

            // 若使用模拟抓图的数据，用模拟数据替换相机真实数据
            if (Util::getUseSimulateImg()) {
                int idx = (_is_turn_lamp ? m_imgNum + 1 : 0);
                memcpy(img_data, Util::getSimulateCaptureInst()->getImage(idx), Util::getSimulateCaptureInst()->getImgDataLen());
            }

            // 相机自身图像翻转的处理              // TODO: 若和视筛箱的图像翻转合到一起处理可提高效率，但也增加了逻辑复杂性和维护出错概率
            int flip_code = -2;     // -2:不处理，-1:同时垂直翻转和水平翻转，0:垂直翻转，1:水平翻转
            bool is_flip_vert = false;
            bool is_flip_hori = false;

            if (CGlobal::isInvertImg) {                             // 若是设置了倒转图像，则图像旋转 180 度        // TODO: 最好从硬件上确保这里不需切换，否则影响效率？
                is_flip_vert = !is_flip_vert;
                is_flip_hori = !is_flip_hori;
            }

            if (is_flip_vert && is_flip_hori) {
                flip_code = -1;
            } else if (is_flip_vert && !is_flip_hori) {
                flip_code = 0;
            } else if (!is_flip_vert && is_flip_hori) {
                flip_code = 1;
            }
            if (flip_code > -2) {
                cv::Mat tmp_mat(cv::Size(g_CameraIntf->getImgWidth(), g_CameraIntf->getImgHeight()), CV_8UC1, img_data);
                cv::flip(tmp_mat, tmp_mat, flip_code);
            }

            // “帧获得”信号发射
            m_countFrameSent++;
            emit sigFrameCaptured(img_idx, img_data, (_is_turn_lamp ? m_imgNum : -1));       // TODO: 图像数据通过线程安全的缓冲区传递，防止线程不同步而导致数据错乱？
            // TODO: 通过时间戳确定帧号？

            // 耗时优化的测试：获得第一帧后，拷贝到替代帧，后续帧不再读取，减少 USB 操作，检查耗时的减少量
            //if (!img_instead && captureStep_TurnLamp == getCaptureStep()) {
            //    int data_len = g_CameraIntf->getImgWidth() * g_CameraIntf->getImgHeight();
            //    img_instead = new uchar[data_len];
            //    memcpy(img_instead, img_data, data_len);
            //}

            // 成功后退出重试循环
            break;

        } else if (cameraStat_Timeout != stat_info.cameraStat) {        // 非超时错误的处理
            if (cameraStat_Unrecoverable == stat_info.cameraStat) {
                logWarning("CCaptureThread::getOneFrame(): camera unable work!", CGlobal::LOG_CAPTURE);
                break;
            } else {
                logWarning("CCaptureThread::getOneFrame(): camera error: unknown error!", CGlobal::LOG_CAPTURE);
                // TODO: ？？
            }
        }
    }

    //
    //qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): exited, stat = " << enumToText_CameraStat(stat_info.cameraStat);
    return stat_info;
}

stCameraStatInfo CCaptureThread::triggerTurnLamp(const int _timeout_ms_clear_buff)
{
    //logDebug("CCaptureThread::triggerTurnLamp() into ...", CGlobal::LOG_CAPTURE);
    qDebug() << QString("%1::%2(): entered, timeout_ms_clear_buff = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(_timeout_ms_clear_buff);

    stCameraStatInfo stat_info(cameraStat_Succ);

    do
    {
        // 转为硬触发
        stat_info = g_CameraIntf->setTriggerMode(cameraTriggerMode_Hard);
        logDebug(QString::asprintf("setTriggerMode([hard]) -> %d", (int)stat_info.cameraStat), CGlobal::LOG_CAPTURE);
        if (cameraStat_Succ != stat_info.cameraStat) {
            logWarning("CCaptureThread::triggerTurnLamp(): camera error: set trigger mode to hard failed!", CGlobal::LOG_CAPTURE);
            break;
        }

        // 清空缓存帧
        int count_cleared = 0;
        stat_info = g_CameraIntf->clearFrameBuffer(_timeout_ms_clear_buff, count_cleared);
        if (cameraStat_Succ == stat_info.cameraStat) {
            if (count_cleared > 0) {
                qWarning() << QString("%1::%2(): count_cleared = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(count_cleared);
            }
        } else {
            logWarning("CCaptureThread::triggerTurnLamp(): camera error: clear buffer failed!", CGlobal::LOG_CAPTURE);
            break;
        }

        // 发送转灯指令
        g_WinMeasure->sendTurnLampCmd();

    } while (false);

    //
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): exited";
    return stat_info;
}

void CCaptureThread::run()
{
    logDebug(QString::asprintf("CCaptureThread::run() into ----- %lu", (unsigned long)QThread::currentThreadId()), CGlobal::LOG_CAPTURE);

    // 设置线程名称
    prctl(PR_SET_NAME, "CCaptureThread", nullptr, nullptr, nullptr);

    // 初始化相机
    QString err_msg;
    stCameraStatInfo stat_info(cameraStat_Succ);
    do {
        // 清空帧缓存
        int count_cleared = 0;
        stat_info = g_CameraIntf->clearFrameBuffer(0, count_cleared);
        if (count_cleared > 0) {
            logDebug(QString::asprintf("CCaptureThread::run(): clear %d frames, cameraStat = %d", count_cleared, stat_info.cameraStat), CGlobal::LOG_CAPTURE);
        }
        if (cameraStat_Succ != stat_info.cameraStat) {
            err_msg = "clear buffer failed!";
            logWarning(QString("CCaptureThread::run(): ") + err_msg, CGlobal::LOG_CAPTURE);
            break;
        }

        // 相机转软触发
        stat_info = g_CameraIntf->setTriggerMode(cameraTriggerMode_Soft);
        if (cameraStat_Succ != stat_info.cameraStat) {
            err_msg = "set soft trigger failed!";
            logWarning(QString("CCaptureThread::run(): ") + err_msg, CGlobal::LOG_CAPTURE);
            break;
        }

        // 使相机 SDK 开始接收图像
        stat_info = g_CameraIntf->play();
        if (cameraStat_Succ != stat_info.cameraStat) {
            err_msg = "restart camera failed!";
            logWarning(QString("CCaptureThread::run(): ") + err_msg, CGlobal::LOG_CAPTURE);
            break;
        }
    } while (false);

    // 相机不可恢复错误，发送错误，并退出抓图循环
    if (cameraStat_Unrecoverable == stat_info.cameraStat) {
        logCritical(QString("CCaptureThread::run(): ") + err_msg + QString::asprintf(" err[%d] on capture preparing:", stat_info.errCode) + ", intfMsg=" + stat_info.errMsg, CGlobal::LOG_CAPTURE);

        emit sigCaptureErr(captureError_CameraUnusable, err_msg + QString::asprintf("err[%d] on capture preparing", stat_info.errCode));
        setIsNeedRun(false);
    }

    // 相关变量初始化
    syncFlagList->zeroAll();

    // 计算每次循环的时间间隔
    frameIntervalMs = round((float)1000 / CGlobal::frameRate);

    turnLampInit();                 // 转灯变量初始化

    countSoftTrigger = 0;           // 软触发超时次数
    countAllFrame = 0;              // 所有帧计数
    countSoftFrame = 0;             // 软触发帧计数
    countOuterFrameSync = 0;        // 外部帧同步计数
    countOuterFrameSyncTimeout = 0; // 外部帧同步超时计数

    elapsedCapture.start();         // 循环耗时计时初始化

    // 循环抓图
    isUnexpectlyQuit = true;
    countLoop = 0;
    unsigned int timeout_count = 0;     // 连续超时次数
    while (true)
    {
        if ( ! isNeedRun ) {        /* 注意避免此处死循环。统一使用 setIsNeedRun(false) 退出本循环，不用 break。 */
            logWarning("CCaptureThread::run(): isNeedRun is false, quitting loop ...", CGlobal::LOG_CAPTURE);
            break;
        }

        //
        countLoop++;

        logDebug(QString::asprintf("CCaptureThread::run(): countLoop = %d, CaptureStep = %d", countLoop, getCaptureStep()), CGlobal::LOG_CAPTURE);

        //
        //timeval tv1, tv2;
        //int gettime_succ1 = gettimeofday(&tv1, NULL);       // TODO: 这个的计时精度更高？实测好像和 QElapsedTimer.elapsed() 精度差不多？   // TODO: 改用 clock_gettime() ？
        //QThread::sleep(1);
        //int gettime_succ2 = gettimeofday(&tv2, NULL);       // TODO: 改用 clock_gettime() ？
        //if (0 == gettime_succ1 && 0 == gettime_succ2) {
        //    qDebug() << Util::getTvDiffUsec(tv2, tv1) / 1000;
        //}

        //logDebug(QString::asprintf("elapsedCapture.elapsed() = %lld", elapsedCapture.elapsed()), CGlobal::LOG_CAPTURE);
        elapsedCapture.restart();

        // TODO: 取消非转灯状态？
        // 转灯和非转灯两种状态的抓图流程分开处理
        do {
            if (captureStep_PupilDetect == getCaptureStep())            // 瞳孔检测状态        /* 注意：这个抓图步骤状态值，可能随时被外部改变，所以始于此处的调用堆栈不能直接访问该变量，而应从此处向后传递 */
            {
                static const int GET_FRAME_TIMEOUT_MS_DETECT   = 100;   // 瞳孔检测状态的相机超时    // TODO: 不同相机，超时不同？

                //
                if (isSyncFrame()) {      // 如果需要同步，则先收取完图像，再同步并等待外部过程
                    // 软触发
                    stat_info = g_CameraIntf->softTrigger();
                    if (cameraStat_Succ != stat_info.cameraStat) {
                        logWarning("soft trigger failed! breaking", CGlobal::LOG_CAPTURE);
                        break;
                    }

                    countSoftTrigger++;

                    // 取一帧图
                    stat_info = getOneFrame(GET_FRAME_TIMEOUT_MS_DETECT, false);           /* 软触发的帧，触发一次，获取一次，一一对应 */      // TODO: 相机出错时，无法一一对应，怎么处理？
                    if (cameraStat_Succ == stat_info.cameraStat) {
                        countSoftFrame++;
                    } else {
                        logWarning("get frame failed! breaking", CGlobal::LOG_CAPTURE);
                        break;
                    }

                    // 外部帧同步
                    outerFrameSync(true);
                } else {                // 如果不需要同步，则同时触发图像帧和外部帧同步过程（须确保立即返回），再收取图像      // TODO： 这两种情况有必要分开吗？
                    // 软触发
                    stat_info = g_CameraIntf->softTrigger();
                    if (cameraStat_Succ != stat_info.cameraStat) {
                        logWarning("soft trigger failed! breaking", CGlobal::LOG_CAPTURE);
                        break;
                    }

                    countSoftTrigger++;

                    // 外部帧同步
                    outerFrameSync(false);       // TODO: 这里不应调用外部帧同步？或根据测距模块类型连接不同的槽函数？

                    // 取一帧图
                    stat_info = getOneFrame(GET_FRAME_TIMEOUT_MS_DETECT, false);
                    if (cameraStat_Succ == stat_info.cameraStat) {
                        countSoftFrame++;
                    } else {
                        logWarning("get frame failed! breaking", CGlobal::LOG_CAPTURE);
                        break;
                    }
                }

                // 瞳孔检测状态，根据帧率延时     // TODO: 是否时必要的？不需帧同步时不用延时？
                while ((captureStep_PupilDetect == getCaptureStep()) && (elapsedCapture.elapsed() < frameIntervalMs)) {
                    static const int SLEEP_PER_TIME = 5;
                    QThread::msleep(SLEEP_PER_TIME);
                }
            }
            else if (captureStep_TurnLamp == getCaptureStep())          // 转灯状态
            {
                //
                if (!m_isTurnlampTriggered) {
                    //
                    //int timeout_ms_clear_buff = qRound(((double)g_CameraIntf->getExposureTime() / 1000) * 2.0);      // TODO: 这里不应该是触发间隔的倍数，而是触发间隔 + 固定值？
                    int timeout_ms_clear_buff = 30;

                    // 触发转灯
                    stat_info = triggerTurnLamp(timeout_ms_clear_buff);
                    if (cameraStat_Succ != stat_info.cameraStat) {
                        logWarning("go into turn lamp failed! breaking", CGlobal::LOG_CAPTURE);
                        break;
                    }

                    //
                    m_isTurnlampTriggered = true;
                }

                // 取一帧图
                int timeout_ms_get_frame = 40;
                stat_info = getOneFrame(timeout_ms_get_frame, true);

                // 判断是否转灯结束
                static constexpr int TIME_OUT_COUNT_WAIT_HARD = 2;     // 等待硬触发帧的取图超时次数

                if (cameraStat_Timeout == stat_info.cameraStat) {
                    m_countTurnlampTimeout++;                 // TODO: 这个取图超时计数，不能只在发生超时错误时才增1？而掉线等错误时也应该增1然后退出流程，否则可能死循环？
                }

                int img_count = m_imgNum + 1;

                bool is_turn_lamp_finished = (m_countTurnlampTimeout >= TIME_OUT_COUNT_WAIT_HARD);      // 转灯时，超时设定次数后，判断为转灯抓图结束
                is_turn_lamp_finished = is_turn_lamp_finished || (G_TURN_LAMP_FRAME_COUNT == img_count);    // (2026-02-05)改为通过帧数来判断转灯结束

                // 转灯完成后的处理
                if (is_turn_lamp_finished) {
                    //logDebug(QString::asprintf("CCaptureThread::run(): Turn lamp ended, image count = %d", m_imgNum + 1), CGlobal::LOG_CAPTURE);
                    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): Turn lamp ended, image count = " << img_count;

                    // 设置当前状态为转灯结束
                    if (captureStep_TurnLamp == getCaptureStep()) {
                        setCaptureStep(captureStep_TurnLampFinished);
                    }

                    // 信号：转灯完成了一次
                    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): emitting sigTurnLampOnce";
                    emit sigTurnLampOnce(img_count);
                }
            }
            else                                                        // 其它状态，退出抓图线程
            {
                // NOTE: 需支持连续转灯，所以非瞳孔检测和非转灯状态，这里还需空循环
                static const int SLEEP_EMPTY_LOOP = 5;      // 空循环时 sleep 时间
                QThread::msleep(SLEEP_EMPTY_LOOP);
                break;
            }
        } while (false);

        // 运行状态检查处理
        if (cameraStat_Succ != stat_info.cameraStat) {
            if (captureStep_PupilDetect == getCaptureStep()) {
                logWarning(QString("CCaptureThread::run(): camera error! errCode = %1").arg(stat_info.errCode), CGlobal::LOG_CAPTURE);
                if (cameraStat_Timeout == stat_info.cameraStat) {
                    // 超时计数
                    timeout_count++;
                } else if (cameraStat_Unrecoverable == stat_info.cameraStat) {
                    logCritical(QString::asprintf("CCaptureThread::run(): Unrecoverable err[%d] on capturing!", stat_info.errCode) + ", intfMsg=" + stat_info.errMsg, CGlobal::LOG_CAPTURE);

                    // 发送错误，并退出抓图循环
                    emit sigCaptureErr(captureError_CameraUnusable, QString::asprintf(" err[%d] on capturing", stat_info.errCode));
                    setIsNeedRun(false);
                } else {
                    m_countCameraErr++;

                    //  其它错误达到设定次数后，判断为相机不可使用，发送错误，并退出线程
                    static const int MAX_CAMERA_ERR = 30;

                    if (m_countCameraErr > MAX_CAMERA_ERR) {
                        logCritical(QString::asprintf("CCaptureThread::run(): camera error too much! last err = %d", stat_info.errCode) + ", intfMsg=" + stat_info.errMsg, CGlobal::LOG_CAPTURE);

                        emit sigCaptureErr(captureError_CameraUnusable, QString::asprintf(" camera error too much! last err = %d", stat_info.errCode));
                        setIsNeedRun(false);
                   }
                }
            } else {
                // 非瞳孔检测阶段，不需检查相机错误，因为处于非触发状态？
                logDebug("get frame faield.");
                // TODO:

            }
        } else {
            timeout_count = 0;
        }

        // 超时错误过多时，判断为相机不可使用，发送错误，并退出线程
        static const int MAX_TIMEOUT = 30;

        if (timeout_count > MAX_TIMEOUT) {
            logCritical("CCaptureThread::run(): timeout too much! exiting ...", CGlobal::LOG_CAPTURE);

            emit sigCaptureErr(captureError_CameraUnusable, QString(" camera timeout too much!"));
            setIsNeedRun(false);
        }

        //
        if(!WinMeasure::isOpened()) {
            logCritical("measure win not visble but capture thread running! exiting ...", CGlobal::LOG_CAPTURE);
            setIsNeedRun(false);
        }
    }
    logDebug("CCaptureThread::run(): loop ended ......", CGlobal::LOG_CAPTURE);

    // 使相机 SDK 停止接收图像
    stat_info = g_CameraIntf->stop();
    if (cameraStat_Succ != stat_info.cameraStat) {
        err_msg = "pause camera failed!";
        logCritical(QString("CCaptureThread::run(): ") + err_msg + QString::asprintf(" err[%d] on capture end", stat_info.errCode) + ", intfMsg=" + stat_info.errMsg, CGlobal::LOG_CAPTURE);
        if (cameraStat_Unrecoverable == stat_info.cameraStat) {
            emit sigCaptureErr(captureError_CameraUnusable, err_msg + QString::asprintf("err[%d] on capture end", stat_info.errCode));
        }
    }

    // 若运行状态值未置否，可循环已结束，逻辑错误
    if (getIsNeedRun()) {
        logCritical("CCaptureThread::run(): Logic Error: isNeedRun is true but thread loop quitted!");
        setIsNeedRun(false);
    }

    //
    isUnexpectlyQuit = false;       // NOTE: 最后要将此变量置否，所以中间不可有 return
    logDebug("CCaptureThread::run() ended", CGlobal::LOG_CAPTURE);
}

void CCaptureThread::setIsNeedRun(bool _is_need_run)
{
    logDebug(QString::asprintf("CCaptureThread::setIsNeedRun(%s)", Util::bool2str(_is_need_run)), CGlobal::LOG_CAPTURE);
    isNeedRun = _is_need_run;
}

bool CCaptureThread::getIsNeedRun()
{
    return isNeedRun;
}

bool CCaptureThread::getIsUnexpectlyQuit()
{
    return isUnexpectlyQuit;
}

void CCaptureThread::setCaptureStep(const enCaptureStep _capture_step_new)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, CaptureStep = " << enumToText_CaptureStep(_capture_step_new);

    //
    const enCaptureStep capture_step_old = getCaptureStep();

    //
    if (_capture_step_new != capture_step_old) {
        switch (_capture_step_new) {
        case captureStep_PupilDetect: {
            // 软触发      // TODO: 逻辑梳理优化？run() 函数里的软触发设置，应由这里执行
            //if (captureStep_PupilDetect != capture_step_old) {
            //    g_CameraIntf->setTriggerMode(cameraTriggerMode_Soft);
            //}

            // 取消设置触发延时     // NOTE: 度申相机的触发延时设置对软硬触发都生效
            if (m_isTriggerDelaySet && enTriggerInputType::RisingEdge == CGlobal::triggerInputType) {         // NOTE: 只有“上升沿触发”时，才需要设置硬触发延时
                stCameraStatInfo stat_info = g_CameraIntf->setTriggerDelayUs(0);
                if (cameraStat_Succ != stat_info.cameraStat) {
                    QString msg = "set trigger delay failed!";
                    logWarning(QString("%1::%2(): %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(msg), CGlobal::LOG_CAPTURE);
                }
                // NOTE: 2025-12-24：由下降沿触发改为上升沿触发后，根据下位机程序流程，需要补上触发延时

                //
                m_isTriggerDelaySet = false;
            }
        }
            //
            break;
        case captureStep_TurnLamp: {
            // 设置触发延时       // NOTE: 度申相机的触发延时设置对软硬触发都生效
            if (!m_isTriggerDelaySet && enTriggerInputType::RisingEdge == CGlobal::triggerInputType) {        // NOTE: 只有“上升沿触发”时，才需要设置硬触发延时
                int trigger_delay_us = qRound(CGlobal::hardTriggerDelayMs * 1000);
                stCameraStatInfo stat_info = g_CameraIntf->setTriggerDelayUs(trigger_delay_us);
                if (cameraStat_Succ != stat_info.cameraStat) {
                    QString msg = "set trigger delay failed!";
                    logWarning(QString("%1::%2(): %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(msg), CGlobal::LOG_CAPTURE);
                }
                // NOTE: 2025-12-24：由下降沿触发改为上升沿触发后，根据下位机程序流程，需要补上触发延时

                //
                m_isTriggerDelaySet = true;
            }

            //
            turnLampInit();
        }
            //
            break;
        case captureStep_TurnLampFinished:

            //
            break;
        default:
            break;
        }

        //
        m_captureStep = _capture_step_new;
    }

    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): exited";
}

void CCaptureThread::abordTurnLamp()
{
    // 若由转灯还未完成的状态转到瞳孔检测状态，则发送转灯完成信号
    if (captureStep_TurnLamp == getCaptureStep()) {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): currently under TurnLamp, emiting sigTurnLampOnce() ...";

        // 设置当前状态为转灯结束
        setCaptureStep(captureStep_TurnLampFinished);

        // 信号：转灯完成了一次
        emit sigTurnLampOnce(m_imgNum + 1, true);
    } else {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): currently not under TurnLamp, action canceled.";
    }
}

void CCaptureThread::setCountLoop(unsigned int _count)
{
    countLoop = _count;
}

unsigned int CCaptureThread::getCountLoop()
{
    return countLoop;
}

void CCaptureThread::turnLampInit()
{
    m_isTurnlampTriggered = false;
    m_countTurnlampTimeout = 0;
    m_imgNum = -1;

}

void CCaptureThread::releaseVectorImage(QVector<unsigned char *> *vector)
{
    QVector<uchar *>::iterator iter;

    for (iter = vector->begin(); iter != vector->end(); iter++) {
        if (iter == NULL) {
            continue;
        }
        uchar *image = *iter;
        if (image != NULL) {
            free(image);
            image = nullptr;
        }
    }
    vector->clear();
    QVector<uchar *> (*vector).swap(*vector);
}


///=================================================================================================
/// class CCapture

CCapture::CCapture(QObject *parent) : QThread(parent)
{

}

CCapture::~CCapture()
{

}

int CCapture::getImgWidth()
{
    return imgWidth;
}

int CCapture::getImgHeight()
{
    return imgHeight;
}

int CCapture::getImgSize()
{
    return imgSize;
}

void CCapture::setCameraHandle(int _camera_handle)
{
    cameraHandle = _camera_handle;
}

void CCapture::run()
{
    const int MAX_CAPTURE_TIME = 2000;      // 最大抓图时间（ms）

    QString err_msg;


//    // TODO: 改为使用通用接口
//    //
//    enCameraStat ret = cameraStat_Unknow;

//    uchar *img_raw = Q_NULLPTR;
//    uchar *img_data = Q_NULLPTR;
//    int capture_count = 0;

//    QTime time;
//    time.start();
//    dvpStatus  stat;
//    do {
//        const int timeout = 500;
//        stat = CameraGetImageBuffer(cameraHandle, &frameHead, &img_raw, timeout);
//        if (DVP_STATUS_OK == stat) {
//            if (!imgWidth)
//                imgWidth    = frameHead.iWidth;
//            if (!imgHeight)
//                imgHeight   = frameHead.iHeight;
//            if (!imgSize)
//                imgSize     = imgWidth * imgHeight * (frameHead.uiMediaType == CAMERA_MEDIA_TYPE_MONO8 ? 1 : 3);

//            // 图像数据拷贝
//            img_data = (uchar *)malloc(imgSize);
//            if (isUseCameraApiProcess) {
//                CameraImageProcess(cameraHandle, img_raw, img_data, &frameHead);
//            } else {
//                memcpy(img_data, img_raw, imgSize);
//            }

//            // 释放图像内存
//            CameraReleaseImageBuffer(cameraHandle, img_raw);

//            // 帧计数
//            capture_count++;

//            // 触发帧图像信号
//            emit sigGetImg(img_data, capture_count);
//            img_data = Q_NULLPTR;       // 信号的槽函数必须释放此内存，否则将有内存泄漏

//        } else {
//            qDebug() << "camera err = " << stat << ", time elapsed = " << time.elapsed();
//            if (0 == capture_count) {   // 如果还没抓到第一张图，继续，因为可能抓图过程开始得比快门触发早了
//                continue;
//            } else {
//                err_msg = QString("camera err = %1").arg(stat);
//                break;
//            }
//        }
//    } while (time.elapsed() < MAX_CAPTURE_TIME);



    //
    emit sigRunEnd(err_msg);
}
