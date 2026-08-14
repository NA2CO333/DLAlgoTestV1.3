#include "winmeasure.h"
#include "ui_winmeasure.h"

#include <functional>
#include <chrono>
//#include <pthread.h>

#include <QThread>
#include <QDebug>
#include <QVector>
#include <QLabel>
#include <QMovie>
#include <QThread>
#include <QTextCodec>

#include "capturethread.h"
#include "mysqlitepatients.h"
#include "windowsmanager.h"
#include "bluetoothintf.h"
#include "hardware.h"
#include "DataTransmit.h"
#include "global.h"
#include "globalclass.h"
#include "utilui.h"

#include "result.h"
#include "mainwindow.h"
#include "mainwindow.h"
#include "windatatrans.h"
#include "widget-optical-type-options.h"

// 预览逐帧日志默认关闭，测试耗时仍由汇总日志记录。
#ifndef ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#define ENABLE_DL_MERGE_TEST_FULL_VERBOSE 0
#endif
#ifndef ENABLE_PREVIEW_FRAME_VERBOSE_LOG
#define ENABLE_PREVIEW_FRAME_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"

#ifdef SURPORT_FRAME_BUFFER
    #include "lcd.h"
#endif

using namespace DataTrans;

//extern Result *resultWin;

//const int MAX_DIST_WAVE_COUNT = 3;  // 最大测距波动次数

int g_tmpcount = 100;//tmp TEST
int g_distanceVal = 0;              // 显示给用户的距离值（毫米）
bool wave_status = false;           // 调试用的？
bool isrun = false;     // 据旧代码，isrun 是指当前正在发送转灯指令？

int gCameraRestartCnt = 0;              // 测量过程重启计数
void incCameraRestartCnt() {
    if(gCameraRestartCnt >= 999)
        gCameraRestartCnt = 0;
    gCameraRestartCnt++;
}

bool g_isTurnLampTestMode = false;      // 是否转灯测试模式
int g_countTurnLamp {0};                // 转灯计数
int g_countFrameLoss {0};               // 帧丢失情况的计数
int g_countFrameExcess {0};             // 帧超出情况的计数
int gTurnLampFrameListErrorCnt = 0;     // 转灯图集数量异常计数
void setTurnLampFrameListErrorCnt() {
    if(gTurnLampFrameListErrorCnt >= 999) {
        gTurnLampFrameListErrorCnt = 0;
    }
}

QMap<float, CvPoint3D32f> pupil_Map;

//extern uchar                   *g_readBuf;          //画板显示数据区

extern vector<CvRect> pupil_Vec;
extern bool pupil_Vec_State;

uchar *currentFrameData = Q_NULLPTR;        // 当前帧数据    /* 注意：刷新很快，所以应避免在处理图像过程中直接使用该指针，而是应该先复制下来再使用。 */

bool WinMeasure::musicStateCfg = false;     // 本界面的“音乐”按钮是否是打开状态    // NOTE: 此为本模块的配置，区别于全局的产品特性 CGlobal::getIsMusicEnabled(), getIsColoredLampEnabled()
bool WinMeasure::s_isOpened = false;

string WinMeasure::dev_stat = "available";
string WinMeasure::run_stat = "";

bool m_bTemperatureInfoShown = false;

/* 常量 ================== */

//
int DISTANCE_FIT_BEGIN    = STD_DISTANCE - 30;
int DISTANCE_FIT_END      = STD_DISTANCE + 30;

int DISTANCE_FIT_NEAR     = DISTANCE_FIT_BEGIN - 50;
int DISTANCE_FIT_FAR      = DISTANCE_FIT_END + 50;

const QString TEXT_START = "开始";
const QString TEXT_STOP = "结束";

// 静态变量
enOperationMode WinMeasure::m_operationMode = operationMode_NormalMeasure;
enAgeRange WinMeasure::currentAgeRange = ageRange_Invalid;

// 构造函数
WinMeasure::WinMeasure(QWidget *parent) :
    CBaseWidget(parent)
    , ui(new Ui::WinMeasure)
{
    ui->setupUi(this);

    //
    isShowStatusBar = true;

    // 恢复在窗体设计时为了方便查看控件结构而设置的属性
    Util::Ui::clearStyleSheet(this);

    ui->lblOperationTips->setFrameShape(QFrame::NoFrame);
    ui->lblArrow->setFrameShape(QFrame::NoFrame);
    ui->lblDistDesc->setFrameShape(QFrame::NoFrame);
    ui->lblDistVal->setFrameShape(QFrame::NoFrame);

    // 窗体尺寸
#if (SCREEN_SIZE_TYPE == 2)
    QRect rect_self = this->geometry();
    this->setGeometry(rect_self.left(), rect_self.top(), SCREEN_WIDTH, SCREEN_HEIGHT);
#endif

    //ui->pushButtonback->setStyleSheet("QPushButton::focus{outline: none;};"); //取消焦点框，具体负多少可以调节
    ui->btnGoBack->setStyleSheet("QPushButton{border-radius:5px;}");

    ui->lblBarcodeScanMask->setVisible(false);

    ui->btnMusic->setStyleSheet("QPushButton{border-radius:5px;}");
    ui->btnTurnLamp->setStyleSheet("QPushButton{border-radius:5px;}");

    //
    //QTextCodec *codec = QTextCodec::codecForName("GBK");
    //QTextCodec::setCodecForLocale(codec);

    //
    updateView_btnMusic();

    // 底板串口
    serialBaseBoard = MySerialPort::instance();
    //threadSerialBaseBoard = new QThread;
    //serialBaseBoard->moveToThread(threadSerialBaseBoard);
    //threadSerialBaseBoard->start();
    /* 2024-09-14 MySerialPort() 已将自身移到另一个线程，这里不需也不能再转移线程。 */

    QObject::connect(serialBaseBoard, &MySerialPort::sigCmdReceived, this, &WinMeasure::slot_serialBaseBoard_CmdReceived, Qt::QueuedConnection);//超声值返回处理

    //
    QObject::connect(this, &WinMeasure::sigDistanceNotice, this, &WinMeasure::slot_this_DistanceNotice, Qt::QueuedConnection);

    // 底板信息查询（包括超声、电量、充电状态、等）
    timerBaseBoardQuery = new QTimer(this);
    QObject::connect(timerBaseBoardQuery, &QTimer::timeout, this, &WinMeasure::slot_timerBaseBoardQuery_timeout);

    //
    timerSonarCloseDelay = new QTimer(this);
    timerSonarCloseDelay->setSingleShot(true);
    QObject::connect(timerSonarCloseDelay, &QTimer::timeout, this, &WinMeasure::slot_timerSonarCloseDelay_timeout);

    //
    screenTimer = new QTimer(this);
    QObject::connect(screenTimer, &QTimer::timeout, this, &WinMeasure::slot_screenTimer_timeout);

    //
    barcodeDetect = new detectBarcode();
    QObject::connect(this, &WinMeasure::sigGotBarcodeImg, barcodeDetect, &detectBarcode::slotDetectBarcode, Qt::QueuedConnection);
    QObject::connect(barcodeDetect, &detectBarcode::sigDetectionResult, this, &WinMeasure::slot_barcodeDetect_DetectionResult, Qt::QueuedConnection);

    threadBarcode = new QThread;
    barcodeDetect->moveToThread(threadBarcode);
    threadBarcode->start();

    //
    for (int i = algoVerAll_Min; i <= algoVerAll_Max; i++) {
        ui->cbbPupilAlgoVer->addItem(CGlobal::pupilAlgoVerDesc[i], i);
    }

    // 得到图像在窗体中的位置
    int img_view_left   = (SCREEN_WIDTH > IMG_WIDTH ? ((SCREEN_WIDTH - IMG_WIDTH) / 2) : 0)   ;
    int img_view_top    = STATUSBAR_HEIGHT;
    int img_view_width  = (SCREEN_WIDTH > IMG_WIDTH ? IMG_WIDTH : SCREEN_WIDTH);
    int img_view_height = img_view_width * ((float)IMG_HEIGHT / IMG_WIDTH);

    //
    ui->lblBarcodeScanMask->setGeometry(img_view_left, img_view_top, img_view_width, img_view_height);

    ui->frmBottom->setGeometry(0, img_view_top + img_view_height, SCREEN_WIDTH, SCREEN_HEIGHT - img_view_top - img_view_height);

    // 帧绘制部件
    frameDrawer = new CFrameDrawer(this, img_view_left, img_view_top, img_view_width, img_view_height);

    //mThreadDrawFrame = new QThread;
    //frameDrawer->moveToThread(mThreadDrawFrame);
    //mThreadDrawFrame->start();
    // TODO: 若传入 parent 参数，则后面 moveToThread() 时警告“Cannot move objects with a parent”，但是若不传，部件会显示在窗口外
    /* 2024-09-14 “帧绘制部件” 没有槽函数，且需访问 UI，为什么要移到独立线程？ */

    // 单眼遮罩
    ui->lblSingleEyeCover->resize(SCREEN_WIDTH / 2, SCREEN_HEIGHT - STATUSBAR_HEIGHT - ui->frmBottom->height());
    ui->lblSingleEyeCover->setVisible(false);
    ui->lblSingleEyeCover->setStyleSheet("background-color: rgba(211, 215, 207, 100);");
    paintSingleEyeCover(ui->lblSingleEyeCover);

    // 取景框（眼部限位标志）
    eyeLimitMark = new CEyeLimitMark(this, img_view_left, img_view_top, img_view_width, img_view_height);

    // 距离提示框
    ui->frmDistBg->setGeometry((SCREEN_WIDTH - ui->frmDistBg->width()) / 2, img_view_top + (img_view_height - ui->frmDistBg->height()) / 2,
                               ui->frmDistBg->width(), ui->frmDistBg->height());
    ui->frmDistBg->setStyleSheet("QFrame{background-color:rgba(0,0,0,51); border-radius:28px;} QLabel{background-color:rgba(0,0,0,0);}");

    ui->frmDistBg->setVisible(false);

    ui->lblDistVal->setStyleSheet("QLabel{color:rgb(255,255,255);}");
    ui->lblDistDesc->setStyleSheet("QLabel{color:rgb(130,255,60);}");

    // 等待动画
    constexpr int WAITING_MOVIE_WIDTH = 140;
    //loadingMovie = new CWaitingMovie(this, ":/resource/loading.gif", WAITING_MOVIE_WIDTH);      // 转圈动画
    //oadingMovie = new CWaitingMovie(this, ":/resource/measuring.gif", WAITING_MOVIE_WIDTH);    // 眨眼动画
    loadingMovie = new CWaitingMovie(this, ":/resource/measuring2.gif", WAITING_MOVIE_WIDTH);    // 眨眼动画
    loadingMovie->move((SCREEN_WIDTH - loadingMovie->width()) / 2, img_view_top + (img_view_height - loadingMovie->width()) / 2);
    loadingMovie->setVisible(false);

    //
    m_distanceDetect = new CDistanceDetect();

    threadDistDetect = new QThread();
    m_distanceDetect->moveToThread(threadDistDetect);
    threadDistDetect->start();

    QObject::connect(m_distanceDetect, &CDistanceDetect::sigDistanceChanged, this, &WinMeasure::slot_distanceDetect_DistanceChanged, Qt::QueuedConnection);
    QObject::connect(m_distanceDetect, &CDistanceDetect::sigCheckSensorTypeFinished, this, &WinMeasure::slot_distanceDetect_CheckSensorTypeFinished, Qt::QueuedConnection);
    QObject::connect(m_distanceDetect, &CDistanceDetect::sigMessage, getWinManage(), &CWinManage::slotMessage, Qt::QueuedConnection);

    //m_distanceDetect->setSensorType(CGlobal::distSensorType);     // NOTE: (2026-08-07) 由测距模块自动检测测距模组类型
    m_distanceDetect->init();

    //
    isDistCalibration = false;

    //
    elapsedFrameGot.start();

    // 设置控件的显示或有效状态
    setDevStat(AVAILABLE);  //相机空闲中 2020.10.12  tao

    setDistanceDetectState(false, true);

    // 创建其它对象
    createObjects();

    // 测量状态显示部件（当前曝光时间、调光状态等）
    QWidget *measure_stat_parent = ui->frmBottom;
    measureStatView = new CMeasureStatView(measure_stat_parent, m_measureCtrl);
    measureStatView->setGeometry((measure_stat_parent->width() - measureStatView->width()) / 2, measure_stat_parent->height() - measureStatView->height(),
                                 measureStatView->width(), measureStatView->height());

    // 距离校准
    ui->frmDistCalibration->move((SCREEN_WIDTH - ui->frmDistCalibration->width()) / 2, ui->frmDistCalibration->y());

    // 清晰度（调焦模式的）
    ui->lblClarity->move((SCREEN_WIDTH - ui->lblClarity->width()) / 2,
                         img_view_top + 20
                         //(img_view_top + img_view_height - ui->lblClarity->height()) / 2
                         );

    // 调试面板
    ui->wgtDebug->move(ui->btnDebugPanelExpand->x() + ui->btnDebugPanelExpand->width() + 5, ui->btnDebugPanelExpand->y());
    ui->wgtDebug->resize(ui->twgtDebug->size());

    ui->twgtDebug->move(0, 0);
    ui->twgtDebug->setCurrentIndex(0);

    ui->btnDebugPanelSize->move(ui->wgtDebug->x() + ui->wgtDebug->width() - ui->btnDebugPanelSize->width(),
                                ui->wgtDebug->y() + ui->wgtDebug->height() - ui->btnDebugPanelSize->height());

    // 控件的Z轴顺序调整
    frameDrawer->getViewer()->lower();      // 帧绘制控件

    eyeLimitMark->raise();                  // 取景框（眼部限位标志）
    ui->frmDistBg->raise();                 // 距离提示框（窗口中部的）
    ui->lblClarity->raise();                // 清晰度（调焦模式的）

    //ui->btnTurnLamp->raise();               // “转灯”按钮
    //ui->btnGoBack->raise();                 // “返回”按钮
    //measureStatView->raise();               // 测量状态显示部件（当前曝光时间、调光状态等）

    ui->wgtDebug->raise();                  // 调试控件
    ui->btnDebugPanelExpand->raise();
    ui->btnDebugPanelSize->raise();

    ui->btnScanBarcode->raise();            // 扫码（本机相机）按钮
    ui->lblBarcodeScanMask->raise();        // 二维码扫码遮盖

    loadingMovie->raise();                  // 等待动画

    //
    currentAgeRange = CGlobal::defaultAgeRange;

    // 清空设计期间为了容易查看无边框控件而设置的文本
    ui->lblBarcodeScanMask->setText("");
    ui->lblArrow->setText("");
    ui->lblSingleEyeCover->setText("");

    //
    qDebug() << "WinMeasure() ended";
}

WinMeasure::~WinMeasure()
{
    delete ui;
}

void WinMeasure::testAlgo()
{
    CMeasureCtrl::executeAlgoPolicy(m_resultList, m_visionValue, m_visionAbnormal, m_isResultQuestionable);
}

void WinMeasure::setPatient(const CPatient &_pat)
{
    m_patient.cloneFrom(_pat);
}

void WinMeasure::slot_barcodeDetect_DetectionResult(bool _is_succ, QString _decode_data)
{
    //
    isBarcodeImgSent = false;

    //
    const int MAX_BARCODE_SCAN_COUNT = 20;

    //
    if (_is_succ) {
        //
        setIsBarcodeStat(false);

        //
        globalService()->doOn_QrCode_ReceivedCode(_decode_data.toUtf8());
    } else if (barcodeSendCount >= MAX_BARCODE_SCAN_COUNT) {
        setIsBarcodeStat(false);

        //
        getWinManage()->showSuspensionPrompt(tr("识别失败"));   // "Identification failed"
    }
}

void WinMeasure::slotCameraInitFinished(enCameraStat _status, QString _msg)
{
    if (cameraStat_Succ != _status) {
        // 提示相机故障
        QString msg = tr("相机故障：%1\n请长按关机键关机！").arg(_msg);   // "Camera initialization failed: %1\nPlease press and hold the shutdown button to shut down!"
        getWinManage()->showSuspensionPrompt(msg, -1);

        // 重新上电         // TODO: 如果开机时都打开相机失败，说明相机通信问题比较严重，再自动重上电改善用户体验没多大意义？
        //bool is_succ = CameraInitThread::cameraRePowerOn();
        //if (!is_succ) {
        //    getWinManage()->showSuspensionPrompt(language ? "重新初始化中..." : "Re-initializing...");
        //}
        //if (!g_CameraIntf->getIsOn()) {
        //    getWinManage()->showSuspensionPrompt(language ? "打开相机失败！" : "Failed to open camera!");
        //}

    } else {
         logDebug(QString::asprintf("WinMeasure::slotCameraInitFinished(), camerainit->isRunning() = %s", Util::bool2str(camerainit->isRunning())), CGlobal::LOG_CAPTURE);

    }

    // 程序启动完成事件
    globalService()->checkStartupEvent(1);

}

void WinMeasure::createObjects()
{
    //
    if (!camerainit) {
        camerainit = new CameraInitThread(this);
        QObject::connect(camerainit, &CameraInitThread::sigCameraInitFinished, this, &WinMeasure::slotCameraInitFinished, Qt::QueuedConnection);
    }

    // 算法模块
    if (m_algoInvoker == Q_NULLPTR)
    {
        m_algoInvoker = new CAlgoInvoker();

        //QObject::connect(m_algoInvoker, &CAlgoInvoker::writeBlueToothData, btWin, &WinBluetooth::dataWrite);         //2020.10.12 tao
        //m_algoInvoker->btConnection = g_Bluetooth->getBtDatatrans();
        //m_algoInvoker->serialDatatrans = g_SerialDatatrans;

        QObject::connect(this, &WinMeasure::sigPupilDetect, m_algoInvoker, &CAlgoInvoker::slotDetectPupil, Qt::QueuedConnection);
        //QObject::connect(this, &WinMeasure::sigCalcVision, m_algoInvoker, &CAlgoInvoker::slotCalcVision, Qt::QueuedConnection);                          //计算结果
        //QObject::connect(m_algoInvoker, &CAlgoInvoker::sigPupilDetectionResult, this, &WinMeasure::slot_algoInvoker_PupilDetectionResult, Qt::QueuedConnection);
        //QObject::connect(m_algoInvoker, &CAlgoInvoker::sigCalcVisionFinished, this, &WinMeasure::slot_algoInvoker_CalcVisionFinished, Qt::QueuedConnection);
        QObject::connect(m_algoInvoker, &CAlgoInvoker::sigCalcVisionCallbackReceived, this, &WinMeasure::slot_algoInvoker_CalcVisionCallbackReceived, Qt::QueuedConnection);
        QObject::connect(m_algoInvoker, &CAlgoInvoker::sigAlgoErr, this, &WinMeasure::slot_algoInvoker_AlgoErr, Qt::QueuedConnection);
        QObject::connect(m_algoInvoker, &CAlgoInvoker::sigMsgNotify, this, &WinMeasure::slot_algoInvoker_MsgNotify, Qt::QueuedConnection);
    }

    if (mThreadAlgo == Q_NULLPTR) {
        mThreadAlgo = new QThread;
        m_algoInvoker->moveToThread(mThreadAlgo);
        mThreadAlgo->start();
    }

    //
    if (Q_NULLPTR == m_measureCtrl) {       // TODO: 构造本窗体时立即构造，后面不必检查是否为空。包括本函数内的其它对象，m_captureThread 不必重新创建。
        m_measureCtrl = new CMeasureCtrl(m_captureThread);

        QObject::connect(m_measureCtrl, &CMeasureCtrl::sigStartMeasure, this, &WinMeasure::slot_measureCtrl_StartMeasure, Qt::QueuedConnection);
        QObject::connect(m_measureCtrl, &CMeasureCtrl::sigGoIntoMeasureStep, this, &WinMeasure::slot_measureCtrl_GoIntoMeasureStep, Qt::QueuedConnection);
        QObject::connect(m_measureCtrl, &CMeasureCtrl::sigErrMsg, this, &WinMeasure::slot_measureCtrl_ErrMsg, Qt::QueuedConnection);

        m_measureCtrl->setAlgoInvoker(m_algoInvoker);
    }

    if (Q_NULLPTR == m_exposureAdjuster) {
        m_exposureAdjuster = new CExposureAdjuster();

        QObject::connect(m_exposureAdjuster, &CExposureAdjuster::sigIsExposureOk, m_measureCtrl, &CMeasureCtrl::slotIsExposureOk, Qt::QueuedConnection);
        QObject::connect(m_exposureAdjuster, &CExposureAdjuster::sigMsgNotify, this, &WinMeasure::slot_exposureAdjuster_MsgNotify, Qt::QueuedConnection);
    }

    // 图像信息计算模块（调试用的）
    if (!calcImgInfo) {
        calcImgInfo = new CCalcImgInfo;

        QObject::connect(this, &WinMeasure::sigCalcImgInfo, calcImgInfo, &CCalcImgInfo::slotCalcImgInfo, Qt::QueuedConnection);
    }
    if (!threadCalcImgInfo) {
        threadCalcImgInfo = new QThread;

        calcImgInfo->moveToThread(threadCalcImgInfo);
        threadCalcImgInfo->start();
    }

    // 抓图线程
    if (!m_captureThread)             // TODO: 旧代码里每次开始测量，都重新创建一次抓图模块对象，是否有助于避免某些未能预料的故障？
    {
        m_captureThread = new CCaptureThread(this);
        funcGetFrameMem callback_get_frame_mem = std::bind(&CMeasureCtrl::getOneFrameMem, m_measureCtrl, std::placeholders::_1, std::placeholders::_2);
        m_captureThread->setCallbackGetFrameMem(callback_get_frame_mem);

        // 信号槽连接
        QObject::connect(m_captureThread, &CCaptureThread::finished, this, &WinMeasure::slot_captureThread_finished, Qt::QueuedConnection);
        //QObject::connect(m_algoInvoker, &CAlgoInvoker::sigReleaseTurnLampImgList, m_captureThread, &CCaptureThread::slotReleaseTurnLampImgList, Qt::QueuedConnection);    //清空resultByte图像数据
        QObject::connect(m_captureThread, &CCaptureThread::sigFrameCaptured, this, &WinMeasure::slot_captureThread_FrameCaptured, Qt::QueuedConnection);
        QObject::connect(m_captureThread, &CCaptureThread::sigTurnLampOnce, this, &WinMeasure::slot_captureThread_TurnLampOnce, Qt::QueuedConnection);
        QObject::connect(m_captureThread, &CCaptureThread::sigCaptureErr, this, &WinMeasure::slot_captureThread_CaptureErr, Qt::QueuedConnection);
    }

    // 调试信息更新定时器
    if (!timerUpdateDebugInfo) {
        timerUpdateDebugInfo = new QTimer;
        QObject::connect(timerUpdateDebugInfo, &QTimer::timeout, this, &WinMeasure::slot_timerUpdateDebugInfo_timeout, Qt::QueuedConnection);
    }

}

void WinMeasure::updateUi()
{
    // 根据型号设置音乐/彩灯按钮是否可见
    bool is_music_btn_enabled = (CGlobal::getIsMusicEnabled() || CGlobal::getIsColoredLampEnabled());
    ui->btnMusic->setVisible(is_music_btn_enabled);
    ui->lblMusic->setVisible(is_music_btn_enabled);

    // 显示单眼模式的标签和遮盖
    if (singleDualEyeMode_Both != g_SingleDualEye) {
        ui->lblSingleEyeTip->setVisible(true);
        ui->lblSingleEyeCover->setVisible(true);

        ui->lblSingleEyeTip->setText(singleDualEyeMode_Left == g_SingleDualEye ?
                                         tr("左眼模式") : tr("右眼模式"));  // "Left Eye Mode", "Right Eye Mode"

        int tip_x_left_side = ((float)SCREEN_WIDTH / 2 - ui->lblSingleEyeTip->width()) / 2;                             // 左侧（不是左眼）单眼遮盖的提示的 x 坐标（提示与遮盖位于不同侧）
        int tip_x_right_side = ((float)SCREEN_WIDTH / 2 - ui->lblSingleEyeTip->width()) / 2 + (float)SCREEN_WIDTH / 2;  // 右侧（不是右眼）单眼遮盖的提示的 x 坐标
        int cover_x_left_side = 0;                      // 左侧（不是左眼）单眼遮盖的 x 坐标
        int cover_x_right_side = SCREEN_WIDTH / 2;      // 右侧（不是右眼）单眼遮盖的 x 坐标

        bool is_left_mode_cover_left = true;                        // 是否“左眼模式时遮盖左侧”（这是直线光路类型时的情况，其它光路类型不同）
        if (opticalPathType_Square == g_opticalPathType) {          // 方形视筛箱，左右眼位置与常规模式相反
            is_left_mode_cover_left = !is_left_mode_cover_left;
        } else if (opticalPathType_LShape == g_opticalPathType) {   // L形视筛箱，左右眼位置与常规模式相反
            is_left_mode_cover_left = !is_left_mode_cover_left;
        }

        bool is_cover_left_side;        // 是否遮盖左侧（不是左眼）
        if (singleDualEyeMode_Left == g_SingleDualEye) {
            is_cover_left_side = (is_left_mode_cover_left ? true : false);
        } else {
            is_cover_left_side = (is_left_mode_cover_left ? false : true);
        }

        ui->lblSingleEyeTip->move((is_cover_left_side ? tip_x_right_side : tip_x_left_side), ui->lblSingleEyeTip->y()); // NOTE: 遮盖左侧时，提示在右侧
        ui->lblSingleEyeCover->move((is_cover_left_side ? cover_x_left_side : cover_x_right_side), STATUSBAR_HEIGHT);

        // TODO: 单眼模式时使取景框只框住一边

    } else {
        ui->lblSingleEyeTip->setVisible(false);
        ui->lblSingleEyeCover->setVisible(false);
    }

    // 主题样式
    //this->setStyleSheet("QWidget{background-color:rgb(20,23,31); color:rgb(204,204,204);}");     // TODO: i.MX6Q 系统设置了这个会导致看不到图像
    //this->setAutoFillBackground(true);

    //QPalette palette = this->palette();
    if (themeType_Black == getSysThemeType()) {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_b.png"));   //默认黑色背景图

        ui->btnGoBack->setIcon(QIcon(":/resource/black_theme/back_b.png"));
        ui->frmBottom->setStyleSheet("QFrame{background-color:rgb(20,23,31);}");

        ui->lblBack->setStyleSheet("QLabel{color:rgb(204,204,204);}");
        ui->lblMusic->setStyleSheet("QLabel{color:rgb(204,204,204);}");

        ui->btnTurnLamp->setStyleSheet("QPushButton {background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px;}");

        ui->lblSingleEyeTip->setStyleSheet("color: #00D924");

        ui->lblOpticalType->setStyleSheet("QLabel{color:rgb(204,204,204);}");
        ui->lblSingleDualEye->setStyleSheet("QLabel{color:rgb(204,204,204);}");
        ui->lblHighDiopter->setStyleSheet("QLabel{color:rgb(204,204,204);}");

        ui->btnOpticalType->setStyleSheet("border-radius:5px;");        // NOTE: 设置 border-radius 可使鼠标按下时背景色不变
        ui->btnSingleDualEye->setStyleSheet("border-radius:5px;");
        ui->btnHighDiopter->setStyleSheet("border-radius:5px;");
    } else {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_w.png"));  //白色背景图

        ui->btnGoBack->setIcon(QIcon(":/resource/white_theme/back_w.png"));
        //ui->frmBottom->setStyleSheet("QFrame{background-color:rgb(20,23,31);}");
    }
    //this->setPalette(palette);

    // 文字语言
    ui->lblBack->setText(tr("返回")); // "Back"

    /* 音乐/彩灯按钮图标逻辑：只要有音乐功能，都显示音乐图标，否则显示彩灯图标。 */
    ui->lblMusic->setText(CGlobal::getIsMusicEnabled() ? tr("音乐") : tr("彩灯"));   // "Music" "ColoredLamp"
    updateView_btnMusic();

    // UI 部件状态设置
    ui->btnScanBarcode->setVisible(CGlobal::isDebugMode);       // 扫码功能未完全实现，正常状态下先隐藏

    ui->btnTurnLamp->setVisible(!this->m_isAutoTurnLamp || CGlobal::isDebugMode);

    ui->lblPupilDetectCount->setVisible(isPupilDetectUserTriggerMode);

    ui->lblSysInfos->setVisible(voltageState);      // 是否显示电压、温度、CPU占用、内存占用等系统信息

    //
    ui->frmDistCalibration->setVisible(isDistCalibration);
    ui->btnSaveDistCalitrData->setVisible(CGlobal::isDebugMode);
    ui->btnStart->setText(TEXT_START);

    //
    ui->ckbIsQtDraw->setChecked(isQtDraw);
    ui->cbbPupilAlgoVer->setCurrentIndex(CGlobal::getPupilAlgoVerCfg());

    ui->lblDistVal->clear();
    ui->lblDistDesc->clear();
    ui->lblArrow->setStyleSheet("");

    ui->lblDistLog->clear();

    // 状态栏设置
    QString title = (!isFocusMode ? tr("编号: %1 %2") : "调焦模式");    // "No. : %1 %2"
    //title = title.arg((m_patient.isBatch ? m_patient.screennum : m_patient.patientid)).arg(m_patient.patientname);
    title = title.arg(m_patient.patientid).arg(m_patient.patientname);
    getWinManage()->updateWindowTitle(this, title);

    // 调试部件的状态初始化
    updateView_DebugWidgets(CGlobal::isDebugMode);

}

//
void WinMeasure::showEvent(QShowEvent *)
{
    //
    logDebug(QString("WinMeasure::showEvent(): into ..."), CGlobal::LOG_CAPTURE);

    //
    s_isOpened = true;      // NOTE: 应最先执行，确保状态准确

    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 使系统不进入省电状态
    PowerControl::getlInstance()->reset();

    // 设置“是否扫码”状态
    setIsBarcodeStat(false);        /* 从 1.3 之后的版本，取消了扫码功能 */

    // 若是“距离标定”模式，初始化相关变量
    if (isDistCalibration) {
        // 构造并初始化距离校准对象
        if (!distCalibration) {
            distCalibration = new CDistCalibration;
        }
        distCalibration->reset();

        // 初始化距离值和清晰度值
        g_distanceVal = -1;
        calcImgInfo->reset();
    }

    // 重设固定曝光为否
    if (CGlobal::isDebugMode && m_fixedExposure > 0) {
        setFixedExposure(m_fixedExposure);
    } else {
        m_exposureAdjuster->setIsFixed(false);
    }

    // 调焦模式的设置
    updateView_isFocusMode();

    // 设置“是否使用 Qt 部件绘帧”
    setIsQtDraw(isQtDraw);

    // 调试面板
    setDebugPanelExpanded(false);
    ui->btnDebugPanelExpand->setVisible(CGlobal::isDebugMode);

    // 调试信息刷新定时器
    if (CGlobal::isDebugMode) {
        timerUpdateDebugInfo->start(300);
    }

    //
    ui->btnGoIntoDetect->setVisible(CGlobal::isDebugMode);
    ui->ckbIsTurnLampTest->setVisible(CGlobal::isDebugMode);

    ui->ckbIsTurnLampTest->setChecked(m_isTurnLampTest);

    /* =============== 初始化（本模块打开后只需初始化一次的，测量相关的）： beging --- */

    // 筛查超时开启计时
    screenTimer->start(1000);

    m_elapsedMeasure.start();

    // 年龄段设置
    enAgeRange age_range = m_patient.getAgeRange();
    if (!(age_range >= ageRange_Min && age_range <= ageRange_Max)) {
        getWinManage()->showSuspensionPrompt(tr("年龄段不合法！\n已改为默认年龄段。")); // "AgeRange not valid!\nChanged to default AgeRange."
        age_range = CGlobal::defaultAgeRange;
    }
    setCurrentAgeRange(age_range);

    // 根据年龄段改变算法状态
    CGlobal::judgeAndSetPupilAlgoVer(getCurrentAgeRange());

    // 初始化算法模块
    m_algoInvoker->init();

    // 设置“是否忽略距离”
    bool is_ignore_dist = (!settings::getCfg_IsEnableDistance() || opticalPathType_General != g_opticalPathType);
    setIsIgnoreDist(is_ignore_dist);

    // 设置“是否手动转灯”
    this->m_isAutoTurnLamp = CGlobal::getIsAutoTurnLamp();

    // 启动测距（包括红外）               // TODO: 把硬件的初始化提取为单独函数
    timerSonarCloseDelay->stop();
    setDistanceDetectState(true);

    // 确保灯珠电流等级已设置
    doSetLedLevel();

    //
    m_countMeasure = 0;
    m_countTurnLamp = 0;
    m_countAlgoCallback = 0;
    m_countFrameLoss = 0;

    m_isWaitingForAlgoFinish = false;
    m_isWaitingAnchorRetryAbort = false;

    //
    g_countTurnLamp = 0;
    g_countFrameLoss = 0;
    g_countFrameExcess = 0;
    if (g_isTurnLampTestMode) {
        gTurnLampFrameListErrorCnt = 0;
    }

    //
    m_calcResultState = calcResultState_Unknown;
    m_resultErrMsg.clear();
    m_resultList.clear();
    qDebug() << __PRETTY_FUNCTION__ << ": result list changed: all item cleared!";
    m_visionValue = stVisionValue {};
    m_visionAbnormal = stVisionAbnormal {};
    m_isFinished = false;
    m_isResultQuestionable = false;

    //
    m_isOpticalTypeConfirmed = false;

    // 重设距离标准常量
    DISTANCE_FIT_BEGIN    = STD_DISTANCE - CGlobal::distTolerance;
    DISTANCE_FIT_END      = STD_DISTANCE + CGlobal::distTolerance;
    DISTANCE_FIT_NEAR     = DISTANCE_FIT_BEGIN - 50;
    DISTANCE_FIT_FAR      = DISTANCE_FIT_END + 50;

    //
    gCameraRestartCnt = appSetting::value("camera/cameraRestartCnt").toInt();
    if (!g_isTurnLampTestMode) {
        gTurnLampFrameListErrorCnt = appSetting::value("camera/turnLampFrameListErrorCnt").toInt();
    }

    //
    if (!isBarcodeStat) {
        ui->lblOperationTips->setText("");
    } else {
        ui->lblOperationTips->setText(tr("请对准二维码"));    // "Please aim at the QR code"
    }

    // 算法模块设置
    m_algoInvoker->pupilDetectionResult = PupilDetectionResult_Unknow;
    m_algoInvoker->setCurrentPupilAlgoVer(CGlobal::getCurrentPupilAlgoVer());

    //
    //if (CGlobal::isDebugMode || isDistCalibration)
    {
        maxClarity = 0;
    }

    // 抓图线程
    m_captureThread->setIsSyncFrame(m_distanceDetect->getIsOuterTrigger() && !isForceNotFrameSync);
    m_captureThread->setIsUseRawImg(this->isShowRawImg);

    // 根据是否需要帧同步来重新连接帧同步信号槽
    QObject::disconnect(m_captureThread, &CCaptureThread::sigOuterFrameSync, m_distanceDetect, &CDistanceDetect::queryDistOnce);
    if (m_captureThread->isSyncFrame()) {
        QObject::connect(m_captureThread, &CCaptureThread::sigOuterFrameSync, m_distanceDetect, &CDistanceDetect::queryDistOnce, Qt::DirectConnection);
    } else {
        QObject::connect(m_captureThread, &CCaptureThread::sigOuterFrameSync, m_distanceDetect, &CDistanceDetect::queryDistOnce, Qt::QueuedConnection);
    }

    //
    RunningStatus *win_runningstatus = RunningStatus::getInstance();
    if (Q_NULLPTR != win_runningstatus) {
        if (voltageState) {
            win_runningstatus->startTimerRefreshCpuRate(1000);
            win_runningstatus->startTimerRefreshMemRate(3000);
        } else {
            win_runningstatus->stopAllTimerRefresh();
        }
    }

    // 初始化出推值的参数
    isStatisticalShown = false;

    //
    //m_measureCtrl->doBeforeMeasure();

    /* =============== 初始化（本模块打开后只需初始化一次的，测量相关的）： end --- */

    // 手动转灯的警告
    if (!this->m_isAutoTurnLamp) {
        getWinManage()->showSuspensionPrompt(tr("提醒：当前为手动转灯模式！"), -1); // "Reminder: Currently under manual turn lamp mode!"
    }

    // 音乐和彩灯开关
    musicControl(getMusicStateCfg());
    coloredLampControl(getMusicStateCfg());

    // 更新 UI，包括：布局、可见性/有效性、样式、语言
    updateUi();

    updateView_btnOpticalType();
    updateView_btnSingleDualEye();
    updateView_btnHighDiopter();

    // 启动测量过程（通过信号槽启动，因为这个过程耗时较长）
    m_measureCtrl->startMeasure();

    //
    if (opticalPathType_General != g_opticalPathType) {
        // 语音提示：开始测量
        playVoicePrompt(enVoicePrompt::MeasurementStarting);
    } else {
        // 语音提示：注视固视灯
        playVoicePrompt(enVoicePrompt::FocusOnLight);
    }

    //
    logDebug("WinMeasure::showEvent(): ended", CGlobal::LOG_CAPTURE);
}

void WinMeasure::hideEvent(QHideEvent *)
{
    logDebug(QString("WinMeasure::hideEvent(): into ..."), CGlobal::LOG_CAPTURE);

    //
    s_isOpened = false;     // NOTE: 应最先执行，确保状态准确

    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    //
    setDevStat(AVAILABLE);  //相机空闲中 2020.10.12  tao
    stopMeasure();

    // 关闭音乐
    musicControl(false);

    // 关闭彩灯
    coloredLampControl(false);

#if (WIFI_TYPE == 1 && OS_TYPE==1)
    emit sendSIGNAL(sysSignal_WifiScanOn);     //wifi启动刷新
#endif

    screenTimer->stop();

    //
    if (voltageState) {
        RunningStatus *win_runningstatus = RunningStatus::getInstance();
        if (Q_NULLPTR != win_runningstatus) {
            win_runningstatus->stopAllTimerRefresh();
        }
    }

    // 停止测距
    if (enDistSensorType::Mb1010 == m_distanceDetect->sensorType()) {
        timerSonarCloseDelay->setSingleShot(true);
        timerSonarCloseDelay->start(30000);
    } else {
        setDistanceDetectState(false);
    }

    // 窗体隐藏时重置 “距离校准” 状态为否
    isDistCalibration = false;

    // 退出本界面后，重置 “使用帧缓存” 为否
    setIsUseFrameBuff(false);

    // 退出本界面后，重置 “调焦模式” 为否
    isFocusMode = false;

    //
    m_patientSource = patientSource_Unknown;

    //
    m_measureCtrl->doAfterMeasure();

}

//void WinMeasure::paintEvent(QPaintEvent *event)
//{
//    Q_UNUSED(event)

//    if (singleDualEyeMode_Both != g_SingleDualEye) {
//        QPainter pt(this);

//        // 绘制左右眼模式的遮盖
//        if(singleDualEyeMode_Right == g_SingleDualEye) // 右眼模式（遮挡左眼）
//        {
//            pt.fillRect(QRect(SCREEN_WIDTH / 2, 0, IMG_WIDTH, IMG_HEIGHT), QColor(20, 20, 20, 250));
//        }
//        else if(singleDualEyeMode_Left == g_SingleDualEye) // 左眼模式（遮挡右眼）
//        {
//            pt.fillRect(QRect(24, 0, IMG_WIDTH, IMG_HEIGHT), QColor(20, 20, 20, 250));
//        }
//        pt.setPen(QPen(QColor(59, 225, 35), 10, Qt::SolidLine));
//        if(singleDualEyeMode_Right == g_SingleDualEye) //right eye
//        {
//            pt.drawRect(QRect(29, 5, 371, 470));
//        }
//        else if(singleDualEyeMode_Left == g_SingleDualEye) //left eye
//        {
//            pt.drawRect(QRect(400, 5, 371, 470));
//        }
//    }
//}

void WinMeasure::updateView_DebugWidgets(bool _is_visible)
{
    ui->frmRunStat->setVisible(_is_visible);

    ui->ckbDistDetectLight->setVisible(_is_visible && m_distanceDetect->getIsOuterTrigger());
    //ui->ckbDistDetectLight->setChecked(false);

#if (3 != OS_TYPE)
    ui->ckbIsQtDraw->setVisible(_is_visible);
#else
    ui->ckbIsQtDraw->setVisible(false);
#endif

    ui->ckbIsPressKeySave->setVisible(_is_visible);
    ui->ckbIsPressKeySave->setChecked(isPressKeySave);

    ui->ckbIsShowRawImg->setVisible(_is_visible);

    ui->ckbIgnoreDistance->setVisible(_is_visible);
    ui->ckbIgnoreDistance->setChecked(getIsIgnoreDist());

    ui->ckbLightOn->setVisible(_is_visible);

    ui->edtPwmDuty->setVisible(_is_visible);
    if (!getWinManage()->getIsShowingKeyboard() && ui->edtPwmDuty->text().length() == 0) {
        ui->edtPwmDuty->setText(QString::number(pwmDutyPercent));
    }
    ui->btnSetPwmDuty->setVisible(_is_visible);

    ui->lblDistLog->setVisible(_is_visible);
    ui->lblDistLog->setText("");

    ui->cbbPupilAlgoVer->setVisible(_is_visible);

    ui->ckbIsUserTriggerPupilDetectMode->setVisible(_is_visible);
    ui->ckbIsUserTriggerPupilDetectMode->setChecked(isPupilDetectUserTriggerMode);

    ui->edtGain->setVisible(_is_visible);
    if (!getWinManage()->getIsShowingKeyboard() && ui->edtGain->text().length() == 0) {
        ui->edtGain->setText(QString::number(g_CameraIntf->getAnalogGain(), 'f', 2));
    }
    ui->btnSetGain->setVisible(_is_visible);

    ui->edtExposure->setVisible(_is_visible);
    ui->ckbFixExposure->setVisible(_is_visible);

}

void WinMeasure::updateView_btnOpticalType()
{
    //
    if (opticalPathType_General == g_opticalPathType) {
        ui->btnOpticalType->setIcon(QIcon(":/resource/black_theme/icon_optical-type_general.png"));
    } else if (opticalPathType_LShape == g_opticalPathType) {
        ui->btnOpticalType->setIcon(QIcon(":/resource/black_theme/icon_optical-type_l-shape.png"));
    } else if (opticalPathType_Square == g_opticalPathType) {
        ui->btnOpticalType->setIcon(QIcon(":/resource/black_theme/icon_optical-type_square.png"));
    } else {
        ui->btnOpticalType->setIcon(QIcon());
    }

    //
    ui->lblOpticalType->setText(COpticalPathType::getDiscrip(g_opticalPathType));
}

void WinMeasure::updateView_btnSingleDualEye()
{
    //
    if (singleDualEyeMode_Both == g_SingleDualEye) {
        ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/icon_single-dual-eye_both_b.png"));
    } else if (singleDualEyeMode_Right == g_SingleDualEye) {
        ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/icon_single-dual-eye_right_b.png"));
    } else if (singleDualEyeMode_Left == g_SingleDualEye) {
        ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/icon_single-dual-eye_left_b.png"));
    } else {
        ui->btnSingleDualEye->setIcon(QIcon());
    }

    //
    ui->lblSingleDualEye->setText(enumToText_SingleDualEyeMode(g_SingleDualEye));
}

void WinMeasure::updateView_btnHighDiopter()
{
    //
    if (G_LANGUAGE_CHINESE == CGlobal::language) {
        if (g_isHmMode) {
            ui->btnHighDiopter->setIcon(QIcon(":/resource/black_theme/icon_diopter-high_chn.png"));
            ui->lblHighDiopter->setText(tr("高度数"));     // "High Degree"
        } else {
            ui->btnHighDiopter->setIcon(QIcon(":/resource/black_theme/icon_diopter-low_chn.png"));
            ui->lblHighDiopter->setText(tr("低度数"));     // "Low Degree"
        }
    } else {
        if (g_isHmMode) {
            ui->btnHighDiopter->setIcon(QIcon(":/resource/black_theme/icon_diopter-high_eng.png"));
            ui->lblHighDiopter->setText(tr("高度数"));     // "High Degree"
        } else {
            ui->btnHighDiopter->setIcon(QIcon(":/resource/black_theme/icon_diopter-low_eng.png"));
            ui->lblHighDiopter->setText(tr("低度数"));     // "Low Degree"
        }
    }
}

void WinMeasure::initMeasure()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    // 重置固视不准的次数
    countGazeOver = 0;

    //
    m_measureCtrl->doBeforeMeasure();

    //
    m_measureStep = measureStep_Unknow;
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_measureStep is set to " << enumToText_MeasureStep(m_measureStep);

    //
    distanceFitCount = 0;

    //
    stateOfCmd = -1;

    //
    frameDrawer->reset();

    // 初始化调试用的数值
    if (!isBarcodeStat) {
        m_countMeasure++;
        countPupilDetectionSucc = 0;
    }

    isPupilDetectUserTriggered = false;
    pupilDetectUserTriggerCount = 0;
    pupilDetectUserTriggerSuccCount = 0;

    // 初始化状态变量
    timeoutError = 0;

#if (WIFI_TYPE == 1 && OS_TYPE==1)
    emit sendSIGNAL(sysSignal_WifiScanOff);     //wifi停止刷新(wifi和相机有冲突,不停止刷新相机有卡顿)
#endif

    m_bTemperatureInfoShown = false;

    countDistSent = 0;

    // 初始化状态变量
    m_distanceState = distStat_Unknown;
    lastDistTipState = -1;

    measureStatView->reset();

    isShowRawImg = false;

    // 初始化全局测量结果数据
    g_SaturationCenterR = {};
    g_SaturationCenterL = {};

    CAlgoInvoker::visionValueSource = 0;

    //
    if (CGlobal::getIsExternalControl()) {
        WinMeasure::setDevStat(BUSY);           // 拍摄中
        WinMeasure::setRunStat(DETECT_PUPIL);   // 检测瞳孔
        emit sendBlueToothData(QString::fromStdString(FUNC_RUN_STAT));  // 拍摄中状态
    }

    //
    countPupilDetection = 0;
    mTimeFirstPupilDetected.start();

    // 对象重置
    m_exposureAdjuster->reset();

    // 信号槽相关计数重置
    countPupilDetectSent = 0;
    m_captureThread->setCountFrameSent(0);

    //
    m_isLastTurnLampeFramePupilFound = true;

}

void WinMeasure::startMeasure()
{
    //logDebug(QString::asprintf("WinMeasure::startMeasure() into ----- %lu", (unsigned long)QThread::currentThreadId()), CGlobal::LOG_MEASURE);
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    if (!this->isVisible()) {
        logWarning("WinMeasure::startMeasure(): Measure window is not visible!", CGlobal::LOG_MEASURE);
        return;
    }

    // 开始抓图线程
    startCaptureThread();

    // MeasureStep-01. 进入【准备】步骤
    m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_Ready, true);         // NOTE: 每次测量的初始化在这一步执行，须确保不被跳过

    //
    logDebug("WinMeasure::startMeasure() ended", CGlobal::LOG_MEASURE);
}

void WinMeasure::resetInterruptedMeasurementSession()
{
    // 普通中断只清理短生命周期的硬件等待状态。
    m_isWaitingPupilNotFoundAbort = false;
    m_isWaitingAnchorRetryAbort = false;

    // 被打断的正式会话回到 Ready 后，下一次 Collect 必须从第0轮重新开始。
    m_countTurnLamp = 0;
    m_countAlgoCallback = 0;
    m_countFrameLoss = 0;
    m_isWaitingForAlgoFinish = false;

    // 清理旧结果，避免取消任务的迟到回调污染下一次测量。
    m_calcResultState = calcResultState_Unknown;
    m_resultErrMsg.clear();
    m_resultList.clear();
    m_visionValue = stVisionValue {};
    m_visionAbnormal = stVisionAbnormal {};
    m_isFinished = false;
    m_isResultQuestionable = false;
}

void WinMeasure::finishPupilNotFoundSession()
{
    // 先解除等待标志，避免统一收尾过程中再次被识别为待停止状态。
    m_isWaitingPupilNotFoundAbort = false;

    // 通过现有统一命令接口取消本次测量残留的算法异步任务和轮次状态。
    if (m_algoInvoker) {
        m_algoInvoker->executeAlgoCommand(
                stAlgoCommand::makeCancelMeasurementRuntime());
    }

    // 清理本次测量状态。
    resetInterruptedMeasurementSession();

    // 回到预览阶段。
    if (isOpened()) {
        m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_Ready);
    }
}

void WinMeasure::stopMeasure()
{
    //logDebug("WinMeasure::stopMeasure() into ...", CGlobal::LOG_MEASURE);
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    if (m_exposureAdjuster) {
        // 用户退出、取消测量或停止预览时结束尚未完成的曝光计时会话。
        m_exposureAdjuster->finishExposureTiming(
                "interrupted", g_CameraIntf->getExposureTime());
    }

    // 页面退出或其他普通停止流程不能遗留最终失败的等待标志。
    m_isWaitingPupilNotFoundAbort = false;
    m_isWaitingAnchorRetryAbort = false;

    // 先同步取消正式算法任务，再停止抓图线程，避免旧任务继续回调结果。
    if (m_algoInvoker) {
        m_algoInvoker->executeAlgoCommand(
                stAlgoCommand::makeCancelMeasurementRuntime());
    }

    // 停止抓图线程
    stopCaptureThread();

    //
    logDebug("WinMeasure::stopMeasure() ended", CGlobal::LOG_MEASURE);
}

void WinMeasure::startCaptureThread()
{
    //logDebug("WinMeasure::startCaptureThread() into ...", CGlobal::LOG_MEASURE);
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    // TODO: camera 掉线重连？



    //
    //CAlgoInvoker::lastFourPicState = false;
    CAlgoInvoker::setIsCalculatingVision(false);

    if (m_captureThread) {
        // 如果线程还在运行，说明上次没能结束
        if (m_captureThread->isRunning()) {
            logCritical(QString(__PRETTY_FUNCTION__) + ": thread is still running! something error?", CGlobal::LOG_CAPTURE);

            // TODO: 上次的线程没法结束，就算重建线程对象，也可能有问题？

        }

        // 启动线程
        m_captureThread->setCaptureStep(captureStep_PupilDetect);
        m_captureThread->setIsNeedRun(true);
        m_captureThread->start(QThread::TimeCriticalPriority);      // 使线程的调度优先级尽量高
    }

    resetSpeedCounter();

    logDebug("WinMeasure::startCaptureThread() ended", CGlobal::LOG_MEASURE);
}

void WinMeasure::stopCaptureThread()
{
    //logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    try {
        if (m_captureThread) {
            //
            m_captureThread->setIsNeedRun(false);
            logDebug(QString(__PRETTY_FUNCTION__) + ": waiting m_captureThread exit ...", CGlobal::LOG_CAPTURE);
            m_captureThread->wait(5000);
            m_captureThread->exit();

            //
            if (m_captureThread->isRunning()) {
                qCritical() << "WinMeasure::stopCaptureThread(): finish m_captureThread failed! terminating ...";
                m_captureThread->terminate();         // TODO: 是否应该执行这个操作？该操作是否可以确保杀死因硬件故障等意外因素而堵塞且结束不了的线程？可能导致什么损害？之后的抓图操作是否应该阻止？
                m_captureThread->wait(5000);
            }
        }
        logDebug(QString(__PRETTY_FUNCTION__) + ": exiting m_captureThread is done.", CGlobal::LOG_CAPTURE);
    } catch (...) {
        qCritical() << "exception unnkown: errno = " << errno << ", str = " << strerror(errno);
    }

    //
    //delete m_captureThread;
    //m_captureThread = Q_NULLPTR;       // TODO: 释放后容易意外被访问导致异常？

    //CameraPause(gCameraHandle);

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
}

void WinMeasure::resetSpeedCounter()
{
    m_captureThread->reset();
    frameDrawer->setCountFrame(0);
    m_algoInvoker->setCountDetect(0);

    m_elapsedFrameRate.start();
}

void WinMeasure::setIsFocusMode(bool _is_focus_mode)
{
    isFocusMode = _is_focus_mode;
}

bool WinMeasure::getMusicStateCfg(bool _is_reload)
{
    static bool is_loaded = false;
    if (!is_loaded || _is_reload) {
        musicStateCfg = appSetting::value("/camera/musicstate").toBool();
    }
    return musicStateCfg;
}

void WinMeasure::setMusicStateCfg(bool _stat)
{
    musicStateCfg = _stat;
    appSetting::setValue("/camera/musicstate", musicStateCfg);
    appSetting::sync();
}

void WinMeasure::updateView_isFocusMode()
{
    static bool s_is_focus_mode = !isFocusMode;                 // NOTE: 局部静态变量初始化为与当前属性相反，确保第一次进入本函数时可进入改变事件处理过程
    static QRect rect_frm_dist = ui->frmDistBg->geometry();

    //
    if (s_is_focus_mode != isFocusMode) {
        bool is_focus_mode = isFocusMode;

        // 调焦模式下的设置
        if (is_focus_mode) {
            // 距离信息置零
            g_distanceVal = 0;
            m_distanceState = distStat_Unknown;
            lastDistTipState = -1;

            // 取消测量超时计时
            screenTimer->stop();

        } else {
            // 距离信息移回原位，箭头显示
            ui->frmDistBg->setGeometry(rect_frm_dist);
            ui->lblArrow->setVisible(true);
        }

        // 固定曝光时间
        if (is_focus_mode) {
            m_exposureAdjuster->setIsFixed(true);
            int expo = 10000;
            m_measureCtrl->setExposureTime(&expo);
        }

        // 距离信息显示样式的调整
        if (is_focus_mode) {
            // 距离信息移到底部栏位，箭头隐藏
            ui->frmDistBg->setGeometry((SCREEN_WIDTH - rect_frm_dist.width()) / 2, ui->frmBottom->y() + (ui->frmBottom->height() - rect_frm_dist.height()) / 2,
                                       rect_frm_dist.width(), rect_frm_dist.height());
            ui->lblArrow->setVisible(false);
        } else {
            // TODO: 移到默认位置
            // ui->frmDistBg->setGeometry();
            ui->lblArrow->setVisible(true);
        }

        // 隐藏测量步骤视图
        measureStatView->setVisible(!is_focus_mode);

        // 隐藏音乐按钮
        bool is_music_btn_enabled = (CGlobal::getIsMusicEnabled() || CGlobal::getIsColoredLampEnabled());
        if (is_music_btn_enabled) {
            ui->btnMusic->setVisible(!is_focus_mode);
            ui->lblMusic->setVisible(ui->btnMusic->isVisible());
        }

        // 设置取景框是否显示中心十字图案
        eyeLimitMark->setIsFocusMode(is_focus_mode);

        // 显示清晰度
        ui->lblClarity->setVisible(is_focus_mode);

    }

    //
    s_is_focus_mode = isFocusMode;
}

//
void WinMeasure::slot_ResetCamera()
{
    qDebug() << "------------------------ restarting camera ! -----------------------------------\n\n\n\n\n\n\n\n";

    //
    incCameraRestartCnt();

    //
    if (!this->isVisible()) {
        qDebug() << "--camera is not visible,return";
        return;
    }

    //getWinManage()->showSuspensionPrompt("restarting measuring!");

    qDebug() << "CameraRestartCnt: " << gCameraRestartCnt << ", frameErrorCnt: " << gTurnLampFrameListErrorCnt << __FUNCTION__ << "() -------------------";

    //
    emit sigCaptureRestarted();

    // 重新开始测量
    stopMeasure();
    startMeasure();
}

// 设置测距状态
void WinMeasure::setDistanceDetectState(bool _is_opened, bool _is_init)
{
    logDebug(QString::asprintf("WinMeasure::%s(): _is_opened = %s, CGlobal::distSensorType=%d",
                               __FUNCTION__, Util::bool2str(_is_opened), (int)m_distanceDetect->sensorType()), CGlobal::LOG_DISTANCE);

    // 不管什么测距模块，如果是开启测距时红外都须打开，关闭测距时红外和测距都须关闭。      // TODO: 打开红外的动作不应该和测距的动作混到一起
    if (_is_opened) {      //
        QThread::msleep(10);        // TODO: 这里的 sleep 来自旧代码，有必要吗？
        setLampPwrOpened(true);             // NOTE: 超声电源和灯板电源是一起的
        ui->ckbLightOn->setChecked(true);
    } else {
        QThread::msleep(10);
        setLampPwrOpened(false);
        ui->ckbLightOn->setChecked(false);
    }

    // 如果是接底板的测距模块，则设置超声指令定时器延时的间隔，否则设置测距模块的开关状态
    const int BASEBOARD_QUERY_INTERVAL = 1500;
    distInterval = !isDistCalibration ? CGlobal::distanceInterval2 : CDistCalibration::interval;
    if (distInterval < 30) {
        distInterval = 30;
        //logWarning();
    }

    if (enDistSensorType::Mb1010 == m_distanceDetect->sensorType()) {
        if (_is_opened) {
            QThread::msleep(10);
            serialBaseBoard->write(wave_open_command);

            timerBaseBoardQuery->start(distInterval);
        } else {
            timerBaseBoardQuery->start(BASEBOARD_QUERY_INTERVAL);

            QThread::msleep(10);
            serialBaseBoard->write(wave_close_command);
        }
    } else {
        if (_is_opened) {
            m_distanceDetect->setRequestInterval(distInterval);
            m_distanceDetect->setIsOpened(true);
        } else {
            if (!_is_init) {        // NOTE: CDistanceDetect 内部管理初始化逻辑
                m_distanceDetect->setIsOpened(false);
            }
        }

        timerBaseBoardQuery->start(BASEBOARD_QUERY_INTERVAL);    // 不管是不是接底板的测距模块，都需要定时发送超声指令，因为协议中距离信息与其它信息混到一起了     // TODO: 优化
    }

    //
    stateOfCmd = (_is_opened ? 2 : 3);
}

void WinMeasure::slot_timerUpdateDebugInfo_timeout()
{
    if (!CGlobal::isDebugMode)
        return;

    // frmRunStat
    ui->lblExposure->setText(QString("exposure: %1").arg(g_CameraIntf->getExposureTime()));

    ui->btnCameraRestartCnt->setText(QString("重启|错帧:%1|%2").arg(gCameraRestartCnt).arg(gTurnLampFrameListErrorCnt));

    //ui->lblFrameRate->setText(QString("%1 fps").arg((float)frameDrawer->getCountFrame() * 1000 / m_elapsedFrameRate.elapsed(), 0, 'f', 2));
    ui->lblFrameRate->setText(QString("%1 fps").arg((float)m_captureThread->getCountLoop() * 1000 / m_elapsedFrameRate.elapsed(), 0, 'f', 2));
    ui->lblDetectRate->setText(QString("%1 fps").arg((float)m_algoInvoker->getCountDetect() * 1000 / m_elapsedFrameRate.elapsed(), 0, 'f', 2));
    ui->lblDetectTime->setText(QString("%1 ms").arg(g_lastDetectTime));

    ui->frmRunStat->update();

    // 每隔一段时间重置帧率计数器
    if (m_elapsedFrameRate.elapsed() > 5000) {
        resetSpeedCounter();
    }

    //
    if (isPupilDetectUserTriggerMode) {
        ui->lblPupilDetectCount->setText(QString::asprintf("succ %d / total %d", pupilDetectUserTriggerSuccCount, pupilDetectUserTriggerCount));
    }

}

void WinMeasure::slot_captureThread_TurnLampOnce(int _img_count, bool _is_aborted)
{
    qDebug() << QString("%1::%2(): entered, img_count = %3, _is_aborted = %4, m_countTurnLamp = %5")
                .arg(S_CLASS_NAME).arg(__FUNCTION__).arg(_img_count).arg(Util::bool2str(_is_aborted)).arg(countTurnLamp());

    // 当前countTurnLamp()表示刚完成的物理轮次，算法轮次从0开始。
    const int completedRoundIdx = countTurnLamp() - 1;

    // TODO: 将除了界面显示之外的过程移到 CMeasureCtrl 等非 UI 功能模块

    // 若当前不是转灯状态，则逻辑错误
    //if (measureStep_Collect != getMeasureStep()) {
    //    qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): current measureStep(" << enumToText_MeasureStep(getMeasureStep())
    //                << ") is not measureStep_Collect!?";
    //    return;
    //}

    // 隐藏转圈动画
    //showWaiting(false);

    //
    //m_distanceDetect->setIsPaused(false);

    //
    bool is_imgs_ok = false;
    enCaptureError err_code = captureError_Unknown;
    QString err_msg;

    if (!_is_aborted) {
        //
        if (_img_count == 0) {
            err_code = captureError_FrameSetEmpty;
            g_countFrameLoss++;
            err_msg = tr("转灯图数量为0！触发线未接？"); // "Number of turn lamp images is zero! Is hard trigger line not connected?"
        } else if (_img_count == 1) {       // TODO: 是否因为程序问题导致多了一帧？        /* 经迈德威视的“演示程序”实测，曝光时间最大只能设置到 270.72ms，此时获得帧数为 3 帧 */
            err_code = captureError_FrameSingle;
            g_countFrameLoss++;
            err_msg = tr("转灯图数量为1！触发线未接 & 逻辑错误？");  // "Number of turn lamp images is zero! Is hard trigger line not connected & logic error?"
        } else if (_img_count < G_TURN_LAMP_FRAME_COUNT) {
            err_code = captureError_FrameLoss;
            g_countFrameLoss++;
            err_msg = tr("转灯图数量(=%1) < %2 ！曝光时间过长？").arg(_img_count).arg(G_TURN_LAMP_FRAME_COUNT);    // "Number of turn lamp images < %2 ! Is exposure time too long?"
        } else if (_img_count > G_TURN_LAMP_FRAME_COUNT) {
            err_code = captureError_FrameExcess;
            g_countFrameExcess++;
            err_msg = tr("转灯图数量(=%1) > %2 ！逻辑错误？").arg(_img_count).arg(G_TURN_LAMP_FRAME_COUNT);  // "Number of turn lamp images > %2 ! Logic Error?"
        } else if (_img_count == G_TURN_LAMP_FRAME_COUNT) {
            is_imgs_ok = true;
            err_code = captureError_NoError;
        } else {
            err_msg = QString("LogicError: Size of ImageSet(=%1) is unexpected!").arg(_img_count);
            logCritical(err_msg);
        }
    } else {
        err_msg = "aborted";
    }

    const bool anchorRetryRequested = m_isWaitingAnchorRetryAbort;
    const QString save_reason = anchorRetryRequested
            ? QStringLiteral("anchor_retry")
            : _is_aborted
            ? (m_isForceRestartTurnLamp
                   ? QStringLiteral("manual_restart")
                   : QStringLiteral("aborted"))
            : (err_code == captureError_NoError
                   ? QStringLiteral("normal") : err_msg);
    if (_is_aborted && m_isForceRestartTurnLamp) {
        m_measureCtrl->setTurnLampSaveSource("manual_restart");
    }

    // 最终结果触发的主动中止不是采集故障，不保存结果后的无效残轮。
    if (_is_aborted && m_isFinished && !m_isForceRestartTurnLamp
            && !m_isWaitingPupilNotFoundAbort) {
        return;
    }

    // 正常物理轮结束后通知算法关闭本轮输入，必须先于下一轮启动。
    if ((_is_aborted && anchorRetryRequested || !_is_aborted)
            && m_algoInvoker != nullptr && completedRoundIdx >= 0) {
        m_algoInvoker->executeAlgoCommand(
                stAlgoCommand::makeFinishFormalRoundInput(
                        completedRoundIdx));
    }

    // 保存本轮已缓存图片，支持完整轮、缺图轮和手动中断残轮。
    m_measureCtrl->doAfterTurnLamp(_img_count, _is_aborted, err_code, save_reason);

    // 最终PupilNotFound触发的停止回调必须在保存收尾后立即完成会话收尾，
    // 不得继续进入手动重启、错误恢复或下一轮转灯逻辑。
    if (m_isWaitingPupilNotFoundAbort) {
        finishPupilNotFoundSession();
        return;
    }

    // 首张C800/129 ROI失败触发的停灯只结束当前物理轮，不取消整个测量。
    // 先保存残轮并关闭当前轮输入，再等待硬件缓冲后启动下一轮。
    if (anchorRetryRequested) {
        m_isWaitingAnchorRetryAbort = false;
        if (!m_isFinished && isOpened() && countTurnLamp() < 20) {
            Util::waitMs(80);
            startTurnLamp();
            return;
        }
        // 已达到原有20轮安全上限时继续走下面的超时收尾逻辑，不能再
        // 直接启动第21轮。
        if (m_isFinished || !isOpened()) {
            return;
        }
    }

    if (_is_aborted && m_isForceRestartTurnLamp) {
        m_isForceRestartTurnLamp = false;

        if (!m_isFinished && isOpened() && !m_isWaitingForAlgoFinish) {
            Util::waitMs(80);       // 给底板/抓图状态一个短暂缓冲，避免新旧转灯命令挤在一起。
            m_isStartingManualRestartTurnLamp = true;
            startTurnLamp();
            m_isStartingManualRestartTurnLamp = false;
        }

        return;
    }

    if (captureError_NoError != err_code) {
        //logWarning()
    }

    // 转灯测试模式的处理
    if (g_isTurnLampTestMode) {
        if (g_countTurnLamp >= 100) {
            QString msg = QString("TurnLamp Test result: \n"
                                  "TriggerDelay = %1ms \n"
                                  "Frame Loss: %2 / %3 \n"
                                  "Frame Excess: %4 / %5 \n"
                                  "Total: %6 / %7"
                                  )
                    .arg(CGlobal::hardTriggerIntervalDelayMs)
                    .arg(g_countFrameLoss).arg(g_countTurnLamp)
                    .arg(g_countFrameExcess).arg(g_countTurnLamp)
                    .arg(gTurnLampFrameListErrorCnt).arg(g_countTurnLamp)
                    ;
            getWinManage()->showMsgWin(msg, -1);
            goBack();
        } else {
            if (!err_msg.isEmpty()) {
                getWinManage()->showSuspensionPrompt(err_msg, -1);
            }
        }

        //
        m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_Ready);

        //
        return;
    }

    // 根据转灯图集是否异常作不同处理
    if (is_imgs_ok) {
        //
        m_countFrameLoss = 0;

        //
        //bool is_calc = m_measureCtrl->inputTurnLampOnce(_img_count);        // NOTE: 这里会使当前步骤进入 measureStep_Calc
        //if (is_calc) {
        //    // 若结果计算已完成，则调到结果处理过程
        //    if (m_diopterResultCondition.isMet()) {
        //        qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_diopterResultCondition is met, going into measureStep_CalcFinished ...";
        //        // MeasureStep-04. 进入【结果处理】步骤
        //        bool succ_into = goIntoMeasureStep(measureStep_CalcFinished);
        //        if (!succ_into) {
        //            //
        //
        //        }
        //    } else {
        //        qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_diopterResultCondition is not met! canceled into measureStep_CalcFinished!";
        //    }
        //} else {
        //    // 继续转灯
        //    qWarning() << "Re-TurnLamp: MeasureCtrl checking not pass!??";
        //    m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_Ready);
        //}
        // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以不需此逻辑。
    } else {
        logWarning(QString("WinMeasure::slot_captureThread_TurnLampOnce(): err: ") + err_msg + "\n", CGlobal::LOG_MEASURE);

        // 转灯图集数量异常计数
        gTurnLampFrameListErrorCnt++;
        setTurnLampFrameListErrorCnt();

        //
        if (!_is_aborted) {
            //
            m_countFrameLoss++;

            // 若连续多次转灯图缺帧，则使相机重上电
            static constexpr int COUNT_REPOWER_CAMERA = 3;
            if (m_countFrameLoss >= COUNT_REPOWER_CAMERA) {
                // 相机重上电
                cameraRePowerOn();

                // 计数重置
                m_countFrameLoss = 0;

                // 重上电时会重新开启测量过程，可略过后面的操作
                return;
            }
        }

        // 若正在计算结果，则中止
        //if (enDiopterCalcStat::HasBegin == m_algoInvoker->diopterCalcStat()) {
        //    m_algoInvoker->calcVisionEnd();
        //}
        // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以这个操作要禁止。

        //
        //if (!_is_aborted) {
        //    // 重启测量流程           /* 有可能是因为曝光时间过长而导致的，如果一直重复转灯，会一直失败。 */
        //    qWarning() << "Re-TurnLamp: FrameSet size not correct: " + err_msg << ", err_code: " << err_code;
        //    slot_ResetCamera();         // TODO: goIntoMeasureStep(measureStep_Ready) ??
        //
        //    //
        //    if (CGlobal::isDebugMode) {
        //        getWinManage()->showSuspensionPrompt(err_msg, -1);
        //    }
        //} else {
        //    // 重新转灯
        //    qWarning() << "Re-TurnLamp: TurnLamp aborted!";
        //    m_measureCtrl->jumpIntoMeasureStepLatter(measureStep_Ready);
        //}
        // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以取消此过程，一律继续转灯。
    }

    // 算法限制了最大转灯次数，所以这里达到最大转灯次数时，停止转灯，等待结果回调
    static constexpr int MAX_TURN_LAMP = 20;
    if (countTurnLamp() >= MAX_TURN_LAMP) {
        //
        m_isWaitingForAlgoFinish = true;

        // 等待算法结束（因为此间算法可能返回结果，避免漏掉）
        static constexpr int DELAY_MS = 1000;
        QElapsedTimer elapsed;
        elapsed.start();
        while (/*countAlgoCallback() < countTurnLamp() &&*/ !m_isFinished && elapsed.elapsed() < DELAY_MS) {
            //
            //Util::waitMs(DELAY_MS);
            qApp->processEvents(QEventLoop::AllEvents, 100);

            //
            if (m_isFinished) {
                qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): algo callback is finished, subsequent process skipped!";
                return;
            }

            //
            //QThread::msleep(1);
        }

        //
        if (m_isFinished) {
            qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): algo callback is finished, subsequent process skipped!";
            return;
        }

        //
        if (!isOpened()) {
            qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): already closed, subsequent process skipped!";
            return;
        }

        // 退出测量
        goBack();

        // 提示错误
        QString err_msg = tr("测量超时！"); // "Measurement timeout!"    // TODO: 这提示，和真正的超时区分开？     // TODO: 出现后，就一直出不了结果，重启后恢复？
        if (CGlobal::isDebugMode) {
            err_msg += QString("TurnLamp count is greater than %1").arg(MAX_TURN_LAMP);
        }
        getWinManage()->showMsgWin(err_msg);

        //
        return;
    }

    // 若不是瞳孔检测失败     // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以不需进入测量步骤。
    if (calcResultState_PupilNotFound != m_calcResultState) {
        // 且未完成，则继续转灯
        if (!m_isFinished) {
            //
            bool formalAsync = false;
            if (m_algoInvoker != nullptr) {
                const stAlgoCommandResult commandResult =
                        m_algoInvoker->executeAlgoCommand(
                                stAlgoCommand::makeQueryFormalAsyncPupilMode());
                formalAsync = commandResult.success
                        && commandResult.boolValue;
            }
            const bool distanceFit = getIsDistanceFit();
            const bool continueRound = formalAsync
                    ? distanceFit
                    : (m_isLastTurnLampeFramePupilFound && distanceFit);
            if (continueRound) {        // 正式异步模式只看距离，传统模式保留末帧瞳孔门禁
                qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): continue turnlamp ...";

                // 保持40ms硬件间隔，同时让结果回调有机会先完成，避免已出结果仍启动下一轮。
                QElapsedTimer interRoundGuard;
                interRoundGuard.start();
                while (!m_isFinished && isOpened()
                       && interRoundGuard.elapsed() < 40) {
                    qApp->processEvents(QEventLoop::AllEvents, 5);
                    QThread::msleep(1);
                }
                if (!m_isFinished && isOpened()) {
                    startTurnLamp();
                }
            } else {
                qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): going into step Ready"
                         << ", m_isLastTurnLampeFramePupilFound = " << Util::bool2str(m_isLastTurnLampeFramePupilFound)
                         << ", getIsDistanceFit() = " << Util::bool2str(getIsDistanceFit());

                // 正式任务已决定结束，先取消并清理旧会话，再回到Ready。
                if (m_algoInvoker) {
                    m_algoInvoker->executeAlgoCommand(
                            stAlgoCommand::makeCancelMeasurementRuntime());
                }
                resetInterruptedMeasurementSession();

                // 转到准备阶段
                m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_Ready);
            }
        }
    } else {
        // 若下位机是旧协议，则转到准备阶段；新协议的最终失败收尾已在
        // 转灯停止回调前置分支中完成，这里保持原有分支结构不变。
        if (!serialBaseBoard->isNewProtocal()) {
            qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): algo error occured! (old protocal)going into step Ready ...";

            // 旧协议自然停灯后也通过统一收尾函数清理本次失败会话。
            finishPupilNotFoundSession();
        } else {
            qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): algo error occured! (new protocal)do nothing.";
        }
    }

    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): exited";
}

// 处理底板指令
void WinMeasure::slot_serialBaseBoard_CmdReceived(int _cmd_id, QByteArray _pkg_data)
{
    //logDebug(QString("%1:%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    processBaseBoardCmd(_cmd_id, _pkg_data);
}

// 底板串口指令处理函数
void WinMeasure::processBaseBoardCmd(int _cmd_id, QByteArray _pkg_data)      // TODO: 指令的解析，全部移到 MySerialPort 类
{
    /* 数据帧的格式定义，见 void MySerialPort::processSerialData() */

    QString pkg_data_hex = MySerialPort::byteArrayToHexStr(_pkg_data);
    // TODO: 旧代码是解析 HEX，应改为直接解析 Bytes ？但是部分数据包不符合格式定义？

    //
    if(pkg_data_hex == "557a03f800")     //关机信号
    {
        //qDebug() << "recieve poweroff command";
        emit sendSIGNAL(sysSignal_PowerOffPressed);
        return;
    }
    else if (0 == pkg_data_hex.compare("557A02F1", Qt::CaseInsensitive))     // 下位机重启消息（下位机开机或重启后，定时发送，直到收到应答）
    {
        // 应答
        const uchar CMD[] = {0x55, 0x7A, 0x02, 0xF1};
        serialBaseBoard->write(QByteArray((char *)CMD, 4));

        // 若当前处于测量界面，则重发以下指令：打开红外；打开超声；打开彩灯和音乐
        if (this->isVisible()) {
            // 启动测距（包括红外）
            setDistanceDetectState(true);

            // 确保灯珠电流等级已设置
            //doSetLedLevel(true);                // TODO: 如果调用这个，红外无法自动恢复打开状态？

            // 彩灯
            coloredLampControl(getMusicStateCfg());
        }
    }
    else if (0xA3 == _cmd_id)         //底板软件版本号
    {   //557a07a30204000000
        QString strVer = pkg_data_hex.mid(8, 6);
        QString strC1 = strVer.mid(0, 2);
        QString strC2 = strVer.mid(2, 2);
        QString strC3 = strVer.mid(4, 2);

        aboutdevice::setStm32Version(strC1.toInt(0, 16), strC2.toInt(0, 16), strC3.toInt(0, 16));
    }
    else if(pkg_data_hex == "557a031900")     //蓝牙断开状态
    {
        emit sendSIGNAL(sysSignal_BtPowerClosed);
    }
    else if(pkg_data_hex == "557a031911")     //蓝牙连接状态
    {
        emit sendSIGNAL(sysSignal_BtConnected);
    }
    else if(pkg_data_hex == "557a03fa00")     // 直充被插入      // TODO: 下位机程序的这个数据包的长度字段值不符合数据包格式定义，且缺少CRC校验字段？
    {
        emit sendSIGNAL(sysSignal_ChargingOn);
    }
    else if(pkg_data_hex == "557a03fa11")     // 直充被拔出
    {
        emit sendSIGNAL(sysSignal_ChargingOff);
    }
    else if(pkg_data_hex == "557a03fa22")     // 直充已充满
    {
        emit sendSIGNAL(sysSignal_ChargingFull);
    }
    else if(pkg_data_hex == "557a038b0100" || pkg_data_hex == "557a038b0000")       // TODO: "557a068b01000000" 是啥？
    {
        // TODO:

    }
    else if(pkg_data_hex == "557a03fc00" && this->isVisible())    //极低电量处理
    {
        //qDebug()<<"-----电量过低,请及时充电或更换电池!";

        logWarning("WinMeasure::processBaseBoardCmd(): 电量过低，即将退出测量界面！");
        goBack();    // 关闭拍摄界面

        QString message = tr("电量过低,请及时充电或更换电池!");   // "The battery is too low, please charge or replace the battery in time!"
        QString buttonText = tr("确认");  // "OK"

        MessageWin mess;
        mess.setContent(message);
        mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
        mess.setButtonText(buttonText);
        if(mess.exec() == QDialog::Accepted){}
    } else if (0x86 == _cmd_id) {
        // 距离消息
        processDistanceCmd(pkg_data_hex);
    } else if (0x87 == _cmd_id) {       // 0x07 指令（开关超声）的应答
        // TODO: ？
    } else if (0x8B == _cmd_id) {       // 0x0B 指令（开关音乐和彩灯）的应答
        // TODO: ？
    } else {
        logWarning(QString::asprintf("WinMeasure::processBaseBoardCmd(): baseBoard CMD not found: _cmd_id = %d, cmd_hex = %s",
                                         _cmd_id, pkg_data_hex.toLocal8Bit().data()), CGlobal::LOG_DISTANCE);
        return;
    }

}

// 处理距离指令       // TODO: 通信协议里不应把距离消息和其它消息混在一起？
void WinMeasure::processDistanceCmd(QString pkg_data_hex)
{
    //if (!(enDistSensorType::Mb1010 == CGlobal::distSensorType)) {
    //    logCritical("WinMeasure::processDistanceCmd(): distSensorType must be enDistSensorType::Mb1010!", CGlobal::LOG_DISTANCE);
    //    return;
    //} /* 注意：这个“距离指令，可不仅仅是距离指令，所以不管用什么测距模块，这个指令都必须处理！” */

    /* 数据帧的格式定义，见 void MySerialPort::processSerialData() */

    //
    //static int wave_open_count = 0;

    //解析超声测距、电量信息   eg.  55 7A 09 86 0D E5 00 D2 11 00 00
    //QString lightStr        = pkg_data_hex.mid(4, 4);       // 长度 + 指令
    QString batteryStr      = pkg_data_hex.mid(8, 4);       // 数据 0-1 字节
    QString waveStr         = pkg_data_hex.mid(12, 4);      // 数据 2-3 字节
    QString chargeDetect    = pkg_data_hex.mid(16, 2);      // 数据 4 字节

    // 得到电量值
    char *batteryChar = batteryStr.toLatin1().data();
    int batt_ad = strtoul(batteryChar, NULL, 16);

    // 得到 超声 AD 值
    char *waveChar = waveStr.toLatin1().data();
    const int dist_ad = strtoul(waveChar, NULL, 16);

    /*
超声距离（显示值，单位毫米）计算方法：

▲ 超声距离的计算使用到的变量有：
（1）下位机上传的 AD 值；
（2）固定补偿值 const int DIST_OFFSET = 30;
（3）新超声读数 转为 旧超声读数 的换算比例 NewToOldRatio （即：新超声距离值 = 按旧算法算得的距离值 * 换算比例），缺省值 = 0.129；

▲ 超声测距 MB1010 的 AN 脚的输出电压：每 Vcc(mv) / 512 表示 1 英寸（1英寸 = 25.4mm）（见模块规格书 P2）。
模块供电电压是 3300mv，所以每 (3300 / 512) mv 表示 1 英寸。

▲ 下位机的 AD 转换规则是：输入范围：0-3.3V，转换范围：0-4095（最大12位），所以电压 AD 值代表的毫伏数是 mv = AD * (3300 / 4095)。

计算：
const double DIST_COEFFICIENT = ((double)3300 / 4095) / (3300 / 512) * 25.4;
最终显示的距离值 dist = AD * DIST_COEFFICIENT * NewToOldRatio + DIST_OFFSET

但是：
DIST_COEFFICIENT 按照此逻辑算得 3.175775336，但旧代码中此系数 = 3.197479248。差异来源不明，有可能是旧代码有误。
校准 NewToOldRatio 的缺省值时，是用旧程序，所以目前代码里，DIST_COEFFICIENT 的值还是用 3.197479248。
即此常量的定义为：
const double DIST_COEFFICIENT = 3.197479248;

简化后，超声读数的计算代码（dist_ad 为下位机上传的 AD 值）：
    const double DIST_COEFFICIENT = 3.197479248;
    const double NEW_TO_OLD_RATIO = 0.129;
    const int DIST_OFFSET = 30;

    double dist = dist_ad * DIST_COEFFICIENT * NEW_TO_OLD_RATIO + DIST_OFFSET;
     */

    // 计算距离值
    const double DIST_COEFFICIENT = 3.197479248;
    const int DIST_OFFSET = 30;

    double new_to_old_ratio = (enDistSensorType::Mb1010 == m_distanceDetect->sensorType() ? CGlobal::ultraCoefficient : 1.0);

    double dist = dist_ad * DIST_COEFFICIENT * new_to_old_ratio
            + DIST_OFFSET;                  // NOTE: 旧代码遗留的，或许和结构有关的距离补偿值

    // 距离改变时的处理
    if (enDistSensorType::Mb1010 == m_distanceDetect->sensorType()) {
        int dist_int = std::round(dist);
        doOnDistanceReceived(dist_int, 1);
    }

    // 得到充电状态
    bool is_charging = false;
    bool is_battery_full = false;
    if (chargeDetect == "11") {         // 正在充电
        is_charging = true;
        is_battery_full = false;
    } else if(chargeDetect == "00") {   // 未充电
        is_charging = false;
    } else if(chargeDetect == "22") {   // 已充满
        is_charging = true;         // NOTE: 充满，肯定处于充电状态
        is_battery_full = true;
    }

    // 更新电池 AD 值
    BatteryMonitor::instance()->setBatteryAD(batt_ad, is_charging, is_battery_full);

    // 处理其它消息？


    // =====================
    // TODO: 后面这些代码，是啥玩意儿啊？（2021-10-18）

    //
//    if (pkg_data_hex.isEmpty())     // TODO: 这是啥玩意儿？收到底板串口的空指令？
//    {
//        if (1 == stateOfCmd)    // 发送【转灯指令】后
//        {
//            QTime _time = QTime::currentTime().addMSecs(100);
//            while(QTime::currentTime() < _time)
//            {
//                isrun = true;
//                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
//            }
//            isrun = false;
//            stateOfCmd = 1;
//            sendTurnLampCmd();//发送转灯信号
//            serailport_status = 1;//接收抓图数据
//            qDebug() << "str.isEmpty() && state == 1---发送转灯信号";

//        }
//        return;
//    }

    //
    if (2 == stateOfCmd && !wave_status)        // 发送【打开超声指令】后
    {
        qDebug() << "!wave_status && state == 2";

        if (pkg_data_hex.compare("557a038701"))     // TODO: 这是什么鬼？调试用的？
        {
            wave_status = true;
            //wave_open_count = 0;
        }
        else
        {
//            wave_open_count++;
//            if (wave_open_count >= 3) {

//            }
        }
        return;
    }
    if (3 == stateOfCmd)    // 发送【关闭超声指令】后
    {
        qDebug() << "state == 3";

        if (pkg_data_hex.compare("557a038700"))     // TODO: 这又是什么鬼？
        {
            wave_status = false;
        }
        else
        {
            //guan bi shibai
        }
        return;
    }

    //
    //if (isrun)  // TODO: 这是？若是正在发送转灯指令？
    //{
    //    return;
    //}

    //
//    if (1 == stateOfCmd && lightStr.compare("0684") != 0)    //0684 转灯指令答复     // 1 == stateOfCmd：发送【转灯指令】后
//    {
//        qDebug() << "state == 1 && lightStr.compare(\"0684\") != 0-----发送转灯信号";
//        QTime _time = QTime::currentTime().addMSecs(100);
//        while(QTime::currentTime() < _time)
//        {
//            isrun = true;
//            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
//        }
//        isrun = false;

//        stateOfCmd = 1;
//        sendTurnLampCmd();//发送转灯信号        // TODO: 已经收到转灯指令答复了，又发送转灯指令是什么鬼？
//        serailport_status = 1;//接收抓图数据
//    }

}

// 距离改变消息的处理
void WinMeasure::doOnDistanceReceived(int _new_dist, int _surce)
{
    //
    //logDebug(QString::asprintf("WinMeasure::%s(), isrun=%s, isCalculatingVision=%d, measureStep=%d"
    //                               ", _new_dist=%d, CGlobal::distSensorType=%d, distanceFitCount=%d, _source=%d"
    //                               , __FUNCTION__, Util::bool2str(isrun), CAlgoInvoker::getIsCalculatingVision(), (int)getMeasureStep()
    //                               , _new_dist, CGlobal::distSensorType, distanceFitCount, _surce
    //                               ), CGlobal::LOG_DISTANCE);
    Q_UNUSED(_surce)

    //
    if (!this->isVisible()) {
        return;
    }

    // 若是调焦模式，不做后面处理
    //if (isFocusMode) {
    //    return;
    //}

    // 距离补偿
    _new_dist += CGlobal::distanceOffset;

    // 计算距离的显示值
    g_distanceVal = _new_dist;

    // 判断距离是否合适
    enDistanceState dist_stat = distStat_Unknown;
    if(getIsIgnoreDist()) {
        dist_stat = distStat_Fit;
    } else if (g_distanceVal < DISTANCE_FIT_NEAR) {
        dist_stat = distStat_TooNear;
    } else if (g_distanceVal >= DISTANCE_FIT_NEAR && g_distanceVal < DISTANCE_FIT_BEGIN) {
        dist_stat = distStat_FitNear;
    } else if (g_distanceVal >= DISTANCE_FIT_BEGIN && g_distanceVal <= DISTANCE_FIT_END) {
        dist_stat = distStat_Fit;
    } else if (g_distanceVal > DISTANCE_FIT_END && g_distanceVal <= DISTANCE_FIT_FAR) {
        dist_stat = distStat_FitFar;
    } else {
        dist_stat = distStat_TooFar;
    }

    // 距离合适次数计数
    if (distStat_Fit == getDistanceState()) {
        distanceFitCount++;
    } else {
        distanceFitCount = 0;
    }

    // 距离状态改变的处理
    if (dist_stat != getDistanceState()) {
        m_distanceState = dist_stat;
    }

    // 输入距离信息到转灯条件判断对象
    if (m_measureCtrl) {
        m_measureCtrl->inputDist(g_distanceVal, getDistanceState());
    } else {
        logCritical(QString("WinMeasure::%1(): measureCtrl is NULL!").arg(__FUNCTION__), CGlobal::LOG_MEASURE);
    }

    // 距离状态信号
    emit sigDistanceNotice(g_distanceVal, getDistanceState());

    //qDebug()<<"leave WinMeasure::():"<<__FUNCTION__<<enterTime.msecsTo(QTime::currentTime());
}

// 距离通知（界面的更新）      // TODO: 逻辑梳理优化
void WinMeasure::slot_this_DistanceNotice(int _dist_val, enDistanceState _dist_state)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
    if (isIgnoreDist) {
        return;
    }

    // 光路类型的自动检测
    autoCheckOpticalType(_dist_val);

    // 蓝牙发送超声波距离
    if (CGlobal::getIsExternalControl() && !CAlgoInvoker::getIsCalculatingVision()) {
        int times_interval = std::round(1000.0F / CGlobal::distanceInterval2 / 3);
        if (countDistSent >= times_interval) {          /* 9600 波特率承受不了 10次/s 的距离消息传输频率，须降低到 3次/秒 */
            countDistSent = 0;
            emit sendBlueToothData("distance:" + QString::number(_dist_val));    //蓝牙发送超声波距离
        } else {
            countDistSent++;
        }
    }

    // 更新距离值
    QString dist_str;
    if (!getIsIgnoreDist()) {
        if (distanceUnit_cm == CGlobal::distanceUnit) {
            dist_str = QString::asprintf("%.1f cm", (float)_dist_val / 10);
        } else {
            dist_str = QString("%1 mm").arg(_dist_val);
        }
        ui->lblDistVal->setText(dist_str);
    }

    // 转灯前，显示系统信息、调试信息、等
    if(measureStep_Ready == getMeasureStep())
    {
        // 更新电量值
        if (ui->lblSysInfos->isVisible()) {
            static QElapsedTimer sys_infos_elapsed;
            static bool is_sys_info_inited = false;

            if (!is_sys_info_inited || sys_infos_elapsed.elapsed() >= 2000) {
                if (!is_sys_info_inited) {
                    sys_infos_elapsed.start();
                    is_sys_info_inited = true;
                } else {
                    sys_infos_elapsed.restart();
                }

                QString sys_infos;

                sys_infos += QString("电压:") + QString::number(BatteryMonitor::getVoltage(), 'f', 2) + "V";      // 电压值

                //
                int cpu_rate = RunningStatus::getCurrCpuRate();
                int mem_rate = RunningStatus::getCurrMemRate();
                sys_infos += QString("CPU: %1%\r\n").arg(cpu_rate);     // CPU 占用率
                sys_infos += QString("RAM: %1%\r\n").arg(mem_rate);     // RAM 占用率

                // 更新温度值
                float temperature = -100;
                if (!m_bTemperatureInfoShown) {     // 不用每次测距消息都刷新温度等信息
                    QString temp_str;
                    QFile file("/sys/class/thermal/thermal_zone0/temp");
                    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
                    {
                        QTextStream in(&file);
                        temp_str = in.readLine();
                        file.close();
                    }
                    temperature = temp_str.toFloat() / 1000;
                    sys_infos += QString("温度: %1℃").arg(temperature);     // 温度值

                    //
                    m_bTemperatureInfoShown = true;
                }

                //
                ui->lblSysInfos->setText(sys_infos);
                //logInfo(QString::asprintf("Temperature: %.1f°C, CPU: %d%%, RAM: %d%%", TempValue, cpu_rate, mem_rate), CGlobal::LOG_MEASURE);
            }
        }

        // 显示调试用的距离值 log
        if (CGlobal::isDebugMode || isDistCalibration) {
            //
            const int DIST_LINE_COUNT = 18;
            QString text_dist = ui->lblDistLog->text();
            int count_line = text_dist.count("\n");
            if (count_line >= DIST_LINE_COUNT - 1) {
                int pos_newline = text_dist.indexOf("\n");
                if (pos_newline >= 0)
                    text_dist = text_dist.mid(pos_newline + 1);
            }
            if (text_dist.length() > 0)
                text_dist += "\n";
            text_dist += QString::number(distanceFitCount) + "  " + dist_str;
            ui->lblDistLog->setText(text_dist);

            //logDebug(QString::asprintf("WinMeasure::%s() distanceFitCount=%d, m_interSaturation=%d, m_beginCapture=%d",
            //                               __FUNCTION__, distanceFitCount, m_interSaturation, m_beginCapture), CGlobal::LOG_DISTANCE);

            //
            if (calcImgInfo->clarity > maxClarity)
                maxClarity = calcImgInfo->clarity;

            ui->lblImgInfo->setText(QString::asprintf("清晰度: %.2f, 峰值: %.2f, 均值: %.2f, 标准差: %.2f",
                                                      calcImgInfo->clarity, maxClarity, calcImgInfo->mean, calcImgInfo->stdDev));
        }

        // 距离校准
        if (isDistCalibration) {
            if (!distCalibration) {
                logWarning(QString("WinMeasure::%1(): distCalibration is NULL!").arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
            } else if (!calcImgInfo) {
                logWarning(QString("WinMeasure::%1(): calcImgInfo is NULL!").arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
            } else {
                if (_dist_val >= 0 && calcImgInfo->clarity >= 0) {
                    distCalibration->putDistance(_dist_val - CGlobal::distanceOffset, calcImgInfo->clarity);
                    ui->lblDistCalibration->setText(QString::asprintf("清晰度：%.2f，峰值：%.2f； 采样数：%d",
                                                                      calcImgInfo->clarity, maxClarity, distCalibration->getCount()
                                                                      ));
                } else {
                    //logWarning(QString("WinMeasure::%1(): Value not inited, can't calibrate!").arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
                }
            }
        }

        // 调焦模式，显示清晰度
        if (isFocusMode) {
            ui->lblClarity->setText(QString("清晰度：%1").arg(calcImgInfo->clarity, 10, 'f', 3));
        }
    }

    // 显示距离状态提示及瞳孔检测状态提示
    static int count_dist_not_fit = 0;

    // 距离提示状态：-1:未知，0:合适，1:太近，2:太远
    int dist_tip_state = (distStat_Fit == _dist_state ? 0 : (_dist_state < distStat_Fit ? 1 : 2));

    //
    if (0 == dist_tip_state) {
        count_dist_not_fit = 0;
    } else {
        count_dist_not_fit++;
        g_PupilState = 0;            // 距离不合适之后肯定不会有瞳孔识别提示，清掉
    }

    if (dist_tip_state != lastDistTipState && !getIsIgnoreDist() && !isBarcodeStat) {
        static const QString STYLE_DIST_DESC_FIT        = "QLabel{color:rgb(130,255,60);}";
        static const QString STYLE_DIST_DESC_NOT_FIT    = "QLabel{color:rgb(255,54,54);}";

        static const QString STYLE_ARROW_UP             = "QLabel{border-image:url(:/resource/black_theme/dist_tip_up.png);}";
        static const QString STYLE_ARROW_DOWN           = "QLabel{border-image:url(:/resource/black_theme/dist_tip_down.png);}";

        //
        //static int y_dist_desc = ui->lblDistDesc->y();

        //
        lastDistTipState = dist_tip_state;

        //
        if (!ui->frmDistBg->isVisible()) {
            ui->frmDistBg->setVisible(true);
        }

        //
        QString dist_desc;
        QString dist_desc_style;
        QString direction_style;

        if (0 == dist_tip_state) {
            dist_desc_style = STYLE_DIST_DESC_FIT;
            dist_desc = tr("请保持不动");    // "hold your position"

            // 隐藏距离值，调整距离描述的位置为垂直居中
            //if (ui->lblDistVal->isVisible()) {
            //    ui->lblDistVal->setVisible(false);
            //
            //    ui->lblDistDesc->setGeometry(ui->lblDistDesc->x(), (ui->frmDistBg->height() - ui->lblDistDesc->height()) / 2, ui->lblDistDesc->width(), ui->lblDistDesc->height());
            //}
        } else {
            dist_desc_style = STYLE_DIST_DESC_NOT_FIT;

            CAlgoInvoker::bSpotCoarseLocationCnt = 0;

            if (1 == dist_tip_state) {
                //if (count_dist_not_fit >= MAX_DIST_WAVE_COUNT)              // 避免因测距不稳定而误提示
                {
                    dist_desc = tr("距离过近"); // "too near"
                    direction_style = STYLE_ARROW_DOWN;
                }
            } else if(2 == dist_tip_state) {
                //if (count_dist_not_fit >= MAX_DIST_WAVE_COUNT)
                {
                    dist_desc = tr("距离过远"); // "too far"
                    direction_style = STYLE_ARROW_UP;
                }
            }

            // 显示距离值，调整距离描述的位置
            //if (!getIsIgnoreDist() && !ui->lblDistVal->isVisible()) {
            //    ui->lblDistVal->setVisible(true);
            //
            //    ui->lblDistDesc->setGeometry(ui->lblDistDesc->x(), y_dist_desc, ui->lblDistDesc->width(), ui->lblDistDesc->height());
            //}
        }

        ui->lblArrow->setStyleSheet(direction_style);
        ui->lblDistDesc->setText(dist_desc);
        ui->lblDistDesc->setStyleSheet(dist_desc_style);
    }
}

void WinMeasure::autoCheckOpticalType(const int _curr_dist)
{
    static constexpr int L_SHARP_BOX_DIST_MIN = 420;    // L形视筛箱距离最小值
    static constexpr int L_SHARP_BOX_DIST_MAX = 450;    // L形视筛箱距离最大值
    static constexpr int CONFIRM_DELAY = 3000;          // 延时确认（毫秒）

    static bool is_processing = false;          // 是否正在处理

    // 若正在处理，处理，阻止后续的事件处理流程进入此处，防止重复弹框等问题
    if (is_processing) {
        return;
    }

    //
    if (_curr_dist >= L_SHARP_BOX_DIST_MIN && _curr_dist <= L_SHARP_BOX_DIST_MAX) {     // 若距离读数在 L 形视筛箱范围内
        // 若L形视筛箱光路类型未确认，且当前光路类型是常规
        if (!m_isOpticalTypeConfirmed && opticalPathType_General == g_opticalPathType) {
            // 计时
            static QElapsedTimer elapsed_is_in;         // 在距离范围内的计时
            if (!elapsed_is_in.isValid()) {
                elapsed_is_in.start();
            }

            //
            if (elapsed_is_in.elapsed() >= CONFIRM_DELAY) {
                //
                elapsed_is_in.invalidate();

                //
                is_processing = true;

                // 暂停测量         // NOTE: 注意：弹出模态对话框之前，须先暂停测量！   // TODO: 原因？优化？
                stopMeasure();          // TODO: 这个函数执行后，未能确保正在执行的转灯步骤或结果计算完成后，后续流程会被取消？

                // 询问
                QString question = tr("当前好像是在L形视筛箱中使用本设备。\n是否将光路类型改为“%1”？")
                        // "It seems that this device is currently being used in an L-Shaped screening box.\nDo you want to change the optical path type to \"%1\"?"
                        .arg(COpticalPathType::getDiscrip(opticalPathType_LShape));
                bool is_l_box = getWinManage()->showNoticeWin(question, tr("是"), tr("否"), 0, true, true);
                if (is_l_box) {
                    //
                    doOnOpticalPathTypeChanged(opticalPathType_LShape);
                }

                //
                m_isOpticalTypeConfirmed = true;

                // 继续测量
                startMeasure();

                //
                is_processing = false;
            }
        }
    } else {            // 若距离读数在 L 形视筛箱范围外
        // 若L形视筛箱光路类型已确认，且当前光路类型是L形视筛箱
        if (m_isOpticalTypeConfirmed && opticalPathType_LShape == g_opticalPathType) {
            // 在距离范围外的计时
            static QElapsedTimer elapsed_is_out;
            if (!elapsed_is_out.isValid()) {
                elapsed_is_out.start();
            }

            //
            if (elapsed_is_out.elapsed() >= CONFIRM_DELAY) {
                //
                elapsed_is_out.invalidate();

                //
                is_processing = true;

                // 重置 L 形视筛箱确认状态
                m_isOpticalTypeConfirmed = false;

                //
                is_processing = false;
            }
        }
    }
}

// 开始转灯和抓图
void WinMeasure::startTurnLamp()
{
    //logDebug(QString::asprintf("WinMeasure::turnLamp() into ----- %lu", (unsigned long)QThread::currentThreadId()), CGlobal::LOG_MEASURE);
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, countTurnLamp() = " << countTurnLamp();

    // 结果已经形成时禁止竞态启动下一物理轮。
    if (m_isFinished) {
        return;
    }

    // 最终PupilNotFound等待硬件停止期间，禁止定时器、延迟回调或
    // 普通错误恢复流程重新启动转灯。
    if (m_isWaitingPupilNotFoundAbort) {
        return;
    }

    // 提前换轮等待硬件停止期间，禁止任何延迟回调再次发起转灯。
    if (m_isWaitingAnchorRetryAbort) {
        return;
    }

    //
    if (m_isWaitingForAlgoFinish) {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): isWaitingForAlgoFinish, action skipped!";
        return;
    }

    //// 停止红外测距，防止对测量光造成干扰
    //if (enDistSensorType::Mb1010 != CGlobal::distSensorType) {
    //    m_distanceDetect->setIsPaused(true);
    //}

    // 确保抓图模块当前处于瞳孔检测状态
    //if (captureStep_PupilDetect != m_captureThread->getCaptureStep()) {
    //    logWarning(QString("%1: Currently going into captureStep_PupilDetect, but m_captureThread not in captureStep_PupilDetect?")
    //               .arg(__PRETTY_FUNCTION__), CGlobal::LOG_MEASURE);
    //    m_captureThread->setCaptureStep(captureStep_PupilDetect);
    //}
    // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以不需此限制。

    //
    QString turnLampSaveSource = "auto_detect";
    if (g_isTurnLampTestMode || m_isTurnLampTest) {
        turnLampSaveSource = "turnlamp_test";
    } else if (m_isStartingManualRestartTurnLamp) {
        turnLampSaveSource = "manual_restart";
    } else if (isrun || !m_isAutoTurnLamp) {
        turnLampSaveSource = "manual_click";
    }
    m_measureCtrl->setTurnLampSaveSource(turnLampSaveSource);

    //
    m_measureCtrl->doBeforeTurnLamp();

    // 使抓图模块进入转灯状态
    m_captureThread->setCaptureStep(captureStep_TurnLamp);

    // 转灯计数
    m_countTurnLamp++;
    g_countTurnLamp++;

    // 算法模块结果计算开始      // NOTE: (2025-11-18)新算法策略
    if (!g_isTurnLampTestMode) {
        m_algoInvoker->calcVisionBegin(getCurrentAgeRange(), m_patient.getImgDirName(), g_SingleDualEye, countTurnLamp() - 1);  // NOTE: 注意：这里的“转灯转灯轮次索引”参数要求从0开始
    }

    //
    //m_algoInvoker->startCalcVisionTimeoutChecking();   // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以不需超时检查。

    // 显示转圈动画
    //showWaiting(true);

    //
    isrun = false;

}

// 底板信息查询定时器超时事件
void WinMeasure::slot_timerBaseBoardQuery_timeout()
{
    queryInfosFromBaseBoard();
}

void WinMeasure::slot_timerSonarCloseDelay_timeout()
{
    if (!g_WinMeasure->isVisible()) {
        setDistanceDetectState(false);
    }
}

//
void WinMeasure::slot_screenTimer_timeout()
{
    //
    if (isDistCalibration) {
        return;
    }

    //
    if (!isOpened()) {
        return;
    }

    //
    if (g_isTurnLampTestMode) {
        return;
    }

    //
    int measure_timeout = settings::getScreenTimeoutSecs();
    bool is_timeout = (measure_timeout > 0 && m_elapsedMeasure.elapsed() > measure_timeout * 1000);
    if (is_timeout /*&& getMeasureStep() < measureStep_Calc*/) {
        //
        if (m_elapsedMeasure.elapsed() > measure_timeout * 1000) {    // 筛查超时判断
            logWarning("WinMeasure::slot_screenTimer_timeout(): screening timeout!", CGlobal::LOG_CAPTURE);

            // 语音提示：测量超时
            playVoicePrompt(enVoicePrompt::MeasurementTimeout);

            // 若存在缓存结果，则结束测量并显示
            if (!m_resultList.empty()) {
                // 结束并显示结果
                m_measureCtrl->jumpIntoMeasureStepLatter(measureStep_MeasureFinished);
            } else {        // 否则，退出测量，并提示超时
                //
                goBack();

                QString text = tr("筛查超时！"); // "Screening timeout!"
                getWinManage()->showMsgWin(text);

                //
                if (CGlobal::getIsExternalControl()) {
                    std::string stat_str = OUT_OF_TIME;     // 拍摄超时
                    WinMeasure::setRunStat(stat_str);
                    emit sendBlueToothData(QString::fromStdString(stat_str));
                }
            }
        }
    }
}

//SDK等初始化操作
enCameraStat WinMeasure::init_SDK()
{
    //
    camerainit->start();

    //
    enCameraStat status = g_CameraIntf->getCameraStatus();      // TODO: 相机初始化过程还未执行完，这个返回值无意义？
    return status;
}

// 返回按钮
void WinMeasure::on_btnGoBack_clicked()
{
    //
    goBack();

}

//
void WinMeasure::goBack()
{
    qDebug() << "WinMeasure::goBack(): stopMeasure() into ...";

    // 如果是在 “距离校准” 状态，则询问
    if (isDistCalibration) {
        if (distCalibration->getIsStarted()) {
            int ret = QMessageBox::question(this, "请确认", "测量未完成，确定退出吗？");
            if(QMessageBox::Yes == ret) {
                distCalibration->setIsStarted(false);
            } else {
                return;
            }
        }
    }

    //
    stopMeasure();
    qDebug() << "WinMeasure::goBack(): stopMeasure() done ......";

    //
    //emit sendSIGNAL(sysSignal_MusicOff);
    //qDebug() << "WinMeasure::goBack(): sendSIGNAL(sysSignal_MusicOff) done ......";

    //
    //const enOperationMode op_mode = WinMeasure::getOperationMode();
    //if (normalReTest == op_mode || batchReTest == op_mode) {
    //    globalService()->getResultWin()->isNeedSave = false;
    //}

    //
    if (isDistCalibration) {
        getWinManage()->showWindowByType(WIN_TOOL);
    } else {
        //qDebug() << "WinMeasure::goBack(): showing WIN_HOME ......";
        getWinManage()->backToLastWidget();
    }

    appSetting::setValue("camera/cameraRestartCnt", gCameraRestartCnt);

    if (!g_isTurnLampTestMode) {
        appSetting::setValue("camera/turnLampFrameListErrorCnt", gTurnLampFrameListErrorCnt);
    }

    //
    m_measureStep = measureStep_Unknow;
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_measureStep is set to " << enumToText_MeasureStep(m_measureStep);

}

//
void WinMeasure::distCalibrationCheckAndApply()
{
    QString msg;
    int dist_best = distCalibration->getDistOfMaxClarity(&msg);
    if (dist_best > 0) {
        int dist_fix = STD_DISTANCE - dist_best;
        int ret = QMessageBox::question(this, "请确认",
                                        QString::asprintf("原距离补偿值是 %s%d mm，\r\n新距离补偿值是 %s%d mm。\r\n是否修改？",
                                                          (CGlobal::distanceOffset >= 0 ? "+" : "-"), qAbs(CGlobal::distanceOffset),
                                                          (dist_fix >= 0 ? "+" : "-"), qAbs(dist_fix)
                                                          )
                                        );
        if(QMessageBox::Yes == ret) {
            CGlobal::distanceOffset = dist_fix;
            CGlobal::saveConfs();
        }
    } else {
        QMessageBox::critical(this, "错误", msg);
    }
}

void WinMeasure::setIsQtDraw(bool _is_qt_draw)
{
#ifndef SURPORT_FRAME_BUFFER
    _is_qt_draw = true;             // 如果系统不支持帧缓存，则始终使用 Qt 绘帧
#endif

    isQtDraw = _is_qt_draw;
    frameDrawer->setIsQtDraw(_is_qt_draw);

    setIsUseFrameBuff(!_is_qt_draw);
}

void WinMeasure::setIsUseFrameBuff(bool _use_frame_buff)        // TODO: 移到全局公用模块
{
// 如果系统不支持帧缓存，则始终不使用帧缓存
#ifdef SURPORT_FRAME_BUFFER
    if (_use_frame_buff) {
        if (ioctl(devfb, FBIOBLANK, FB_BLANK_UNBLANK) < 0)
        {
            printf("set ioctl FBIOBLANK failed\n");
        }
    } else {
        if (ioctl(devfb, FBIOBLANK, FB_BLANK_NORMAL) < 0)
        {
            printf("set ioctl FBIOBLANK failed\n");
        }
    }
#else
    Q_UNUSED(_use_frame_buff)
#endif
}

void WinMeasure::setIsDistCalibration(bool _is_dist_calibration)
{
    isDistCalibration = _is_dist_calibration;
}

void WinMeasure::setIsIgnoreDist(bool _is_ignore)
{
//    if (_is_ignore == getIsIgnoreDist()) {
//        return;
//    }

    //
    isIgnoreDist = _is_ignore;

    //
    if (m_measureCtrl) {
        m_measureCtrl->setIsIgnoreDist(_is_ignore);
    }

    //
    ui->frmDistBg->setVisible(!_is_ignore);
    ui->lblDistVal->setVisible(!_is_ignore);
    ui->lblDistDesc->setVisible(!_is_ignore);
    ui->lblArrow->setVisible(!_is_ignore);
}

//void WinMeasure::showStatisticalValues(int _age_range)
//{
//    //
//    isStatisticalShown = true;
//
//    // 获取推值
//    stVisionValue vision;
//    stVisionAbnormal vision_abnormal;
//    CAlgoInvoker::getStatisticalValues(_age_range, m_patient.getBirthDate(), vision, vision_abnormal);
//
//    // 拷贝结果
//    m_calcResultState = calcResultState_Succ;
//    m_resultErrMsg.clear();
//    m_resultList.clear();
//    m_resultList.push_back(vision);
//
//    // 结束测量
//    m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_MeasureFinished);
//}

void WinMeasure::slot_algoInvoker_AlgoErr(enAlgoErrType _algo_err_type, QString _msg)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered"
             << ", err_type = " << enumToText_AlgoErrType(_algo_err_type) << ", msg = " << _msg;

    //
    if (algoErrType_DetectPupil == _algo_err_type) {
        //
    } else if (algoErrType_CalcVision == _algo_err_type) {
        //
    }

    if (CGlobal::isDebugMode) {
        QString text = enumToText_AlgoErrType(_algo_err_type) + QString(": ");
        text += _msg;
        getWinManage()->showSuspensionPrompt(text, (CGlobal::isDebugMode ? -1 : 0));
    }

}

void WinMeasure::slot_algoInvoker_MsgNotify(QString _msg)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    if (CGlobal::isDebugMode) {
        getWinManage()->showSuspensionPrompt(_msg);
    }
}

//void WinMeasure::slot_algoInvoker_CalcVisionFinished(const enCalcResultState _calc_result_state, const stVisionValue _vision, const stVisionAbnormal _vision_abnormal)
//{
//    //
//    logDebug((QString(__PRETTY_FUNCTION__) + ": into ... , calc_stat = %1")
//             .arg((int)_calc_result_state), CGlobal::LOG_MEASURE);
//
//    // 若是调焦模式，不做后面处理
//    if (isFocusMode) {
//        return;
//    }
//
//    // 若本界面已退出，则结果无效
//    if (!this->isVisible()) {
//        qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): this win has hidden, this event has been ignored!";
//        return;
//    }
//
//    //
//    enCalcResultState calc_result_state = _calc_result_state;
//    if (calcResultState_Unknown == calc_result_state) {
//        //logCritical();
//        calc_result_state = calcResultState_Fail;
//    }
//
//    //
//    m_calcResultState = calc_result_state;
//    m_visionValue = _vision;
//    m_visionAbnormal = _vision_abnormal;
//
//    // MeasureStep-04. 进入【结果处理】步骤
//    bool succ_into = m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_CalcFinished);
//    if (!succ_into) {
//        //
//
//    }
//}

void WinMeasure::slot_algoInvoker_CalcVisionCallbackReceived(
        const enCalcResultState _calc_result_state, const int _round_idx,
        const stVisionValue _vision, const stVisionAbnormal _vision_abnormal,
        const std::vector<stVisionValue> &_result_set, bool _is_finished, bool _is_questionable)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered"
             << ", _calc_result_state = " << enumToName_CalcResultState(_calc_result_state)
             << ", _round_idx = " << _round_idx
             << ", _is_finished = " << Util::bool2str(_is_finished)
             << ", countAlgoCallback() = " << countAlgoCallback()
             << ", getMeasureStep() = " << enumToText_MeasureStep(getMeasureStep())
                ;

    //
    m_countAlgoCallback++;      // NOTE: 目前（20260427）的算法未能确保每轮转灯都会有回调，所以此变量暂无法使用

    // 若是调焦模式，不做后面处理
    if (isFocusMode) {
        return;
    }

    // 若本模块已经退出，那么图集已被清理，不可再处理算法结果，否则将崩溃
    if (!isOpened()) {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): Measurement is stopped, action skipped!";
        return;
    }

    //
    enCalcResultState calc_result_state = _calc_result_state;
    if (calcResultState_Unknown == calc_result_state) {
        //logCritical();
        calc_result_state = calcResultState_Fail;
    }

    // RetryNextRound只是当前物理轮的锚点失败通知，不是测量失败结果。
    // 不改写正式结果、不调用普通错误处理，只等待当前转灯停止后换轮。
    if (calcResultState_RetryNextRound == calc_result_state) {
        // 失败通知通过队列到达时，旧轮可能已经自然结束并启动了新轮。
        // 必须核对失败轮次，避免迟到回调中止当前正在运行的新轮。
        const int currentRoundIdx = countTurnLamp() - 1;
        if (_round_idx < 0 || _round_idx != currentRoundIdx) {
            qWarning() << S_CLASS_NAME
                       << ":: RetryNextRound ignored as stale callback: failedRound="
                       << _round_idx << ", currentRound=" << currentRoundIdx;
            return;
        }
        if (m_isFinished) {
            qWarning() << S_CLASS_NAME
                       << ":: RetryNextRound ignored after final result";
            return;
        }
        if (m_isWaitingAnchorRetryAbort) {
            // 同一轮迟到的重复通知只允许第一次进入停灯流程。
            return;
        }

        // 必须先置标志，再发送停止命令，避免硬件回调过快到达。
        m_isWaitingAnchorRetryAbort = true;
        const bool turnLampRunning = m_captureThread
                && captureStep_TurnLamp == m_captureThread->getCaptureStep();
        if (turnLampRunning) {
            if (serialBaseBoard->isNewProtocal()) {
                serialBaseBoard->write(CMD_ABORT_TURN_LAMP);
            }
            // 新协议等待主动停灯回调；旧协议等待当前轮自然停止回调。
            return;
        }

        // 硬件已经停止时，sigTurnLampOnce 可能已经发出但仍在 Qt 队列中。
        // 不能在这里直接启动下一轮，否则排队的停灯回调随后会被误认为新轮结束。
        // 保留等待标志，统一由 slot_captureThread_TurnLampOnce() 完成本轮收尾和换轮。
        if (m_captureThread
                && captureStep_TurnLampFinished == m_captureThread->getCaptureStep()) {
            return;
        }

        // 理论上转灯失败通知只会在转灯或转灯结束状态到达；其他状态不应长期
        // 保留等待标志，但同样禁止从算法回调直接启动下一轮。
        qWarning() << S_CLASS_NAME
                   << ":: RetryNextRound received without a pending turn-lamp callback";
        m_isWaitingAnchorRetryAbort = false;
        return;
    }

    // 迟到的最终结果优先于尚未完成的提前换轮请求；清除短生命周期标志，
    // 防止最终结果停灯回调被误当成换轮回调。
    if (_is_finished) {
        m_isWaitingAnchorRetryAbort = false;
    }

    // 当前须处于转灯或结果计算阶段
//    enMeasureStep step = getMeasureStep();
//    if (step < measureStep_Collect || step > measureStep_Calc) {
//        QString msg = QString("Current step(=%1) is out of [Collect, Calc], algo result event not valid!")
//                .arg(enumToText_MeasureStep(step));
//        qWarning() << msg;
//#if OS_TYPE == 2
//        if (CGlobal::isDebugMode) {
//            getWinManage()->showSuspensionPrompt(msg, -1);
//        }
//#endif
//        return;
//    }
    // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以任何步骤下都可以接受结果回调。

    // 添加算法结果
    m_calcResultState = calc_result_state;
    m_resultErrMsg.clear();

    //
    if (calcResultState_Succ == calc_result_state) {
        qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): result list changed: one item appended!";
        // 添加测量结果
        //m_resultList.push_back(_vision);
        // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以直接拷贝整个结果集合。

        m_visionValue           = _vision;
        m_visionAbnormal        = _vision_abnormal;
        m_resultList            = _result_set;
        m_isFinished            = _is_finished;
        m_isResultQuestionable  = _is_questionable;

        // MeasureStep-04. 进入【结果处理】步骤
        if (m_isFinished) {                         // NOTE: 只有 _is_finished 才会显示结果，否则转灯结束时已自动转灯，此处无需处理
            // 结果到达时若下一轮已经启动，新协议立即终止，避免继续缓存无效帧。
            if (serialBaseBoard->isNewProtocal()) {
                serialBaseBoard->write(CMD_ABORT_TURN_LAMP);
            }
            bool succ_into = m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_CalcFinished);
            if (!succ_into) {
                //

            }
        } else {
            qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): _calc_result_state == true, but _is_finished == false, do nothing!";
        }
    } else {
        // 错误处理
        doOnAlgoResultError();

        // 若是瞳孔检测失败
        if (calcResultState_PupilNotFound == m_calcResultState) {
            // 新协议主动发送停止命令；旧协议等待本轮自然结束。
            if (serialBaseBoard->isNewProtocal()) {
                // 同一失败回调可能因异步收尾重复到达，等待期间只保留第一次停止请求。
                if (m_isWaitingPupilNotFoundAbort) {
                    return;
                }

                const bool turnLampRunning = m_captureThread
                        && captureStep_TurnLamp == m_captureThread->getCaptureStep();
                if (turnLampRunning) {
                    // 必须先置标志，再发停止命令，防止硬件停止回调过快到达。
                    m_isWaitingPupilNotFoundAbort = true;
                    serialBaseBoard->write(CMD_ABORT_TURN_LAMP);
                    return;
                }

                // 转灯已经停止时不再等待，仍通过统一收尾函数完成取消和Ready切换。
                finishPupilNotFoundSession();
                return;
            }
        }
    }

}

void WinMeasure::showResult(const std::vector<stVisionValue> &_results)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //if (!this->isVisible()) {     // TODO: 这个检查限制有必要吗？
    //    logWarning(QString("%1: this window not visible, returned!").arg(__PRETTY_FUNCTION__), CGlobal::LOG_MEASURE);
    //    return;
    //}

    //
    showWaiting(false);

    // 语音提示：结束测量
    playVoicePrompt(enVoicePrompt::MeasurementFinished);

    // 裁减版，屏蔽瞳孔直径、眼位、上睑下垂等数据。实现方式是在测量完成之后，显示结果之前，将这些数据清零。   // 20210520 Henry
    /*
    if (CGlobal::isReducedVersion) {
        values.replace(8, "0");     // left pupil diameter
        values.replace(9, "0");     // right pupil diameter
        UEP[0] = false;             // left ptosis
        UEP[1] = false;             // right ptosis
        g_strabismusValue.clear();    // left and right gaze
    }
    */  // 不可用这种方法，因为比如瞳孔直径，在后面的历史浏览等地方需要用来判断单双眼模式，如 Result::judgeSingleDualEyeMode()

    //
    if (_results.size() < 1) {
        getWinManage()->showMsgWin(tr("程序异常：结果集为空！"));   // "Program Error: Results List is empty!"
        return;
    }

    // 左、右眼是否需要计算
    bool is_has_right  = (singleDualEyeMode_Right & g_SingleDualEye);
    bool is_has_left   = (singleDualEyeMode_Left & g_SingleDualEye);

    // 求多次测量结果的均值
    //stVisionValue vision_avg = _results.at(0).visionValue;
    //stVisionAbnormal vision_abnormal_avg = _results.at(0).visionAbnormal;
    //for (int i = 1; i < _results.size(); i++) {
    //    const stVisionCalcResult &result = _results.at(i);
    //    vision_avg          = addVisionValue(vision_avg, result.visionValue);
    //    vision_abnormal_avg = addVisionAbnormal(vision_abnormal_avg, result.visionAbnormal);
    //}
    // NOTE: (2025-11-20)取消此结果融合方法，应用算法开发的新方法

    stVisionValue vision_avg = m_visionValue;
    stVisionAbnormal vision_abnormal_avg = m_visionAbnormal;

    // 视筛箱的屈光数据修正（需求提出：刘宇 2023-07）
    if (opticalPathType_General == g_opticalPathType) {
        vision_avg.RSph += CGlobal::resultCorrectSph_General;
        vision_avg.RCyl += CGlobal::resultCorrectCyl_General;
        vision_avg.LSph += CGlobal::resultCorrectSph_General;
        vision_avg.LCyl += CGlobal::resultCorrectCyl_General;
    } else if (opticalPathType_Square == g_opticalPathType) {
        vision_avg.RSph += CGlobal::resultCorrectSph_Square;
        vision_avg.RCyl += CGlobal::resultCorrectCyl_Square;
        vision_avg.LSph += CGlobal::resultCorrectSph_Square;
        vision_avg.LCyl += CGlobal::resultCorrectCyl_Square;
    } else if (opticalPathType_LShape == g_opticalPathType) {
        vision_avg.RSph += CGlobal::resultCorrectSph_LSharp;
        vision_avg.RCyl += CGlobal::resultCorrectCyl_LSharp;
        vision_avg.LSph += CGlobal::resultCorrectSph_LSharp;
        vision_avg.LCyl += CGlobal::resultCorrectCyl_LSharp;
    }

    // SPH范围限制 [+-10D] 区间，CYL范围限制 [+-3D] 区间     //（需求提出：刘宇 2023-09，2025-11-24）
    static const double MIN_SPH = -10.0;
    static const double MAX_SPH = 10.0;
    static const double MIN_CYL = -3.0;
    static const double MAX_CYL = 3.0;

    vision_avg.RSph = Util::clamp(vision_avg.RSph, MIN_SPH, MAX_SPH);
    vision_avg.LSph = Util::clamp(vision_avg.LSph, MIN_SPH, MAX_SPH);
    vision_avg.RCyl = Util::clamp(vision_avg.RCyl, MIN_CYL, MAX_CYL);
    vision_avg.LCyl = Util::clamp(vision_avg.LCyl, MIN_CYL, MAX_CYL);

    // 取整到设定的精度
    const double VISION_PREC = (g_MinResolution ? 0.01 : 0.25);

    vision_avg.RSph = Util::roundDouble(vision_avg.RSph, VISION_PREC);
    vision_avg.RCyl = Util::roundDouble(vision_avg.RCyl, VISION_PREC);

    vision_avg.LSph = Util::roundDouble(vision_avg.LSph, VISION_PREC);
    vision_avg.LCyl = Util::roundDouble(vision_avg.LCyl, VISION_PREC);

    // 计算 SE
    double r_se = CAlgoInvoker::getSE(vision_avg.RSph, vision_avg.RCyl);
    double l_se = CAlgoInvoker::getSE(vision_avg.LSph, vision_avg.LCyl);

    // 对 SE 取整
    r_se = Util::roundDouble(r_se, VISION_PREC);
    l_se = Util::roundDouble(l_se, VISION_PREC);

    // 转字符串
    QString r_sph_str, r_cyl_str, r_axis_str, r_se_str, r_ps_str;
    QString l_sph_str, l_cyl_str, l_axis_str, l_se_str, l_ps_str;
    QString pd_str;

    // 右眼数据
    if (is_has_right) {
        // 若轴位为 0，记作 180
        if (0 == vision_avg.RAx) {
            vision_avg.RAx = 180;
        }

        //
        r_sph_str   = QString::number(vision_avg.RSph, 'f', 2);
        r_cyl_str   = QString::number(vision_avg.RCyl, 'f', 2);
        r_axis_str  = QString::number(vision_avg.RAx, 10);
        r_se_str    = QString::number(r_se, 'f', 2);
        r_ps_str    = QString::number(vision_avg.RPs, 'f', 1);

        if(vision_avg.RSph > M_FLOAT_PRECISION)
            r_sph_str.prepend("+");
        if(r_se > M_FLOAT_PRECISION)
            r_se_str.prepend("+");

        // 若柱镜度为 0（精度修整后），则柱镜、轴位都显示为空（包括结果界面、小票、导出、数据上传）    /* 需求提出人：刘宇，说是模仿拓普康验光仪 */
        // TODO: 这可能是不合理的？因为留空还另有用途，即表示未知，而 0 和 未知 是完全不同的。
        if (Util::compDouble(vision_avg.RCyl, 0) == 0) {
            //r_cyl_str = "";       /* 0 柱镜度还是应该保存到数据库，只是显示的时候显示为空 */
            r_axis_str = "";
        }
    }

    // 左眼数据
    if (is_has_left) {
        // 若轴位为 0，记作 180
        if (0 == vision_avg.LAx) {
            vision_avg.LAx = 180;
        }

        //
        l_sph_str   = QString::number(vision_avg.LSph, 'f', 2);
        l_cyl_str   = QString::number(vision_avg.LCyl, 'f', 2);
        l_axis_str  = QString::number(vision_avg.LAx, 10);
        l_se_str    = QString::number(l_se, 'f', 2);
        l_ps_str    = QString::number(vision_avg.LPs, 'f', 1);

        if(vision_avg.LSph > M_FLOAT_PRECISION)
            l_sph_str.prepend("+");
        if(l_se > M_FLOAT_PRECISION)
            l_se_str.prepend("+");

        // 若柱镜度为 0（精度修整后），则柱镜、轴位都显示为空（包括结果界面、小票、导出、数据上传）
        if (Util::compDouble(vision_avg.LCyl, 0) == 0) {
            //l_cyl_str = "";       /* 0 柱镜度还是应该保存到数据库，只是显示的时候显示为空 */
            l_axis_str = "";
        }
    }

    // 瞳距
    if (is_has_right && is_has_left) {
        pd_str = QString::number(vision_avg.PD, 10);
    }

    // 异常值处理
    static const QString ABNORMAL_REPLACE_CYL   = "0";      // 不可信 柱镜度 的显示值     // TODO: 将异常值显示为 0，不合理吧？
    static const QString ABNORMAL_REPLACE_AXIS  = "0";      // 不可信 轴位 的显示值      // TODO: 将异常值显示为 0，不合理吧？

    //if (is_has_right) {
    //    if (vision_abnormal_avg.RCylUntrusted) {
    //        r_cyl_str = ABNORMAL_REPLACE_CYL;
    //        r_axis_str = ABNORMAL_REPLACE_AXIS;
    //    } else if (vision_abnormal_avg.RAxisUntrusted) {
    //        r_axis_str = ABNORMAL_REPLACE_AXIS;
    //    }
    //}
    //
    //if (is_has_left) {
    //    if (vision_abnormal_avg.LCylUntrusted) {
    //        l_cyl_str = ABNORMAL_REPLACE_CYL;
    //        l_axis_str = ABNORMAL_REPLACE_AXIS;
    //    } else if (vision_abnormal_avg.LAxisUntrusted) {
    //        l_axis_str = ABNORMAL_REPLACE_AXIS;
    //    }
    //}
    Q_UNUSED(vision_abnormal_avg)

    // 将各个值设置到实体对象
    m_patient.patientlefteyesph   = l_sph_str;
    m_patient.patientlefteyecyl   = l_cyl_str;
    m_patient.patientlefteyeax    = l_axis_str;
    m_patient.patientleftse       = l_se_str;
    m_patient.patientleftpd       = l_ps_str;
    m_patient.patientleftptosis   = vision_avg.LPtosis;

    m_patient.patientrighteyesph  = r_sph_str;
    m_patient.patientrighteyecyl  = r_cyl_str;
    m_patient.patientrighteyeax   = r_axis_str;
    m_patient.patientrightse      = r_se_str;
    m_patient.patientrightpd      = r_ps_str;
    m_patient.patientrightptosis  = vision_avg.RPtosis;

    m_patient.patientpd           = pd_str;

    if (is_has_right) {                                            // 眼位
        m_patient.patientrighths = QString::number(vision_avg.RHz);
        m_patient.patientrightvs = QString::number(vision_avg.RVz);
    }

    if (is_has_left) {
        m_patient.patientlefths  = QString::number(vision_avg.LHz);
        m_patient.patientleftvs  = QString::number(vision_avg.LVz);
    }

    const stVisionValue *v1 = _results.size() >= 1 ? &(_results.at(0)) : nullptr;
    const stVisionValue *v2 = _results.size() >= 2 ? &(_results.at(1)) : nullptr;
    const stVisionValue *v3 = _results.size() >= 3 ? &(_results.at(2)) : nullptr;

    if (is_has_right) {                                            // 多次测量结果
        if (v1) m_patient.RESULT_1_R_SPH    = v1->RSph;
        if (v1) m_patient.RESULT_1_R_CYL    = v1->RCyl;
        if (v1) m_patient.RESULT_1_R_AX     = v1->RAx;

        if (v2) m_patient.RESULT_2_R_SPH    = v2->RSph;
        if (v2) m_patient.RESULT_2_R_CYL    = v2->RCyl;
        if (v2) m_patient.RESULT_2_R_AX     = v2->RAx;

        if (v3) m_patient.RESULT_3_R_SPH    = v3->RSph;
        if (v3) m_patient.RESULT_3_R_CYL    = v3->RCyl;
        if (v3) m_patient.RESULT_3_R_AX     = v3->RAx;
    }

    if (is_has_left) {
        if (v1) m_patient.RESULT_1_L_SPH    = v1->LSph;
        if (v1) m_patient.RESULT_1_L_CYL    = v1->LCyl;
        if (v1) m_patient.RESULT_1_L_AX     = v1->LAx;

        if (v2) m_patient.RESULT_2_L_SPH    = v2->LSph;
        if (v2) m_patient.RESULT_2_L_CYL    = v2->LCyl;
        if (v2) m_patient.RESULT_2_L_AX     = v2->LAx;

        if (v3) m_patient.RESULT_3_L_SPH    = v3->LSph;
        if (v3) m_patient.RESULT_3_L_CYL    = v3->LCyl;
        if (v3) m_patient.RESULT_3_L_AX     = v3->LAx;
    }

    m_patient.IS_MULTI = (v2 ? true : false);

    m_patient.isTest              = true;

    //m_patient.source              = QString::number(CAlgoInvoker::visionValueSource);

    // 虚构连续测量三次的值   /* 虚构方法：SPH, CYL 随机上下浮动 0.25D，轴位随机浮动正负5度。 */
    //simulateMultiResults(m_patient, vision_avg, VISION_PREC, is_has_right, is_has_left);

    qDebug() << "=============================END============================";
    qDebug() << "==== batNo:" << m_patient.batchNo;

    // 检查结果的可信度
    //bool is_reliable = checkResultReliability(vision_avg);
    // NOTE: (2025-11-20)取消此判断方法，应用算法开发的新方法

    bool is_reliable = !m_isResultQuestionable;

    //
    if (m_captureThread->isRunning()) {
        stopMeasure();
    }

    //记录曝光值
    if (CGlobal::isDebugMode) {
        m_patient.comment1=QString::number(g_CameraIntf->getExposureTime());
    }

    // 显示结果页面
    Result *win_result = getWinManage()->getWindow<Result>(WIN_RESULT);
    if (win_result) {
        win_result->setIsReliable(is_reliable);

        win_result->setIsNeedSave(true);
        win_result->setPatientSource(m_patientSource);
        win_result->setPatient(m_patient);

        getWinManage()->showWindow(win_result);     // TODO: 数据异常信息应该也传给结果模块？
    } else {
#if (OS_TYPE == 2)
        getWinManage()->showSuspensionPrompt(tr("内部错误：获取“测量结果”窗口失败"));  // "Internal error: Failed to obtain 'Measurement Result' window"
#endif
    }
}

void WinMeasure::simulateMultiResults(CPatient &_pat, const stVisionValue &_vision, const double _vision_prec,
                                      const bool _is_has_right, const bool _is_has_left)
{
    if (_is_has_right) {
        //
        double sph_1, sph_2, sph_3;
        CAlgoInvoker::generateDoubles(_vision.RSph, 0.50, _vision_prec, sph_1, sph_2, sph_3);

        double cyl_1, cyl_2, cyl_3;
        CAlgoInvoker::generateDoubles(_vision.RCyl, 0.50, _vision_prec, cyl_1, cyl_2, cyl_3);

        double ax_1, ax_2, ax_3;
        CAlgoInvoker::generateDoubles(_vision.RAx, 10, 1, ax_1, ax_2, ax_3);

        // NOTE: 若柱镜度为 0（精度修整后），则柱镜、轴位都显示为空（包括结果界面、小票、导出、数据上传）
        if (Util::compDouble(cyl_1, 0) == 0) {
            ax_1 = 0;
        }
        if (Util::compDouble(cyl_2, 0) == 0) {
            ax_2 = 0;
        }
        if (Util::compDouble(cyl_3, 0) == 0) {
            ax_3 = 0;
        }

        //
        _pat.RESULT_1_R_SPH  = sph_1;
        _pat.RESULT_1_R_CYL  = cyl_1;
        _pat.RESULT_1_R_AX   = CAlgoInvoker::correctAxis(qRound(ax_1));

        _pat.RESULT_2_R_SPH  = sph_2;
        _pat.RESULT_2_R_CYL  = cyl_2;
        _pat.RESULT_2_R_AX   = CAlgoInvoker::correctAxis(qRound(ax_2));

        _pat.RESULT_3_R_SPH  = sph_3;
        _pat.RESULT_3_R_CYL  = cyl_3;
        _pat.RESULT_3_R_AX   = CAlgoInvoker::correctAxis(qRound(ax_3));
    }

    if (_is_has_left) {
        //
        double sph_1, sph_2, sph_3;
        CAlgoInvoker::generateDoubles(_vision.LSph, 0.50, _vision_prec, sph_1, sph_2, sph_3);

        double cyl_1, cyl_2, cyl_3;
        CAlgoInvoker::generateDoubles(_vision.LCyl, 0.50, _vision_prec, cyl_1, cyl_2, cyl_3);

        double ax_1, ax_2, ax_3;
        CAlgoInvoker::generateDoubles(_vision.LAx, 10, 1, ax_1, ax_2, ax_3);

        // NOTE: 若柱镜度为 0（精度修整后），则柱镜、轴位都显示为空（包括结果界面、小票、导出、数据上传）
        if (Util::compDouble(cyl_1, 0) == 0) {
            ax_1 = 0;
        }
        if (Util::compDouble(cyl_2, 0) == 0) {
            ax_2 = 0;
        }
        if (Util::compDouble(cyl_3, 0) == 0) {
            ax_3 = 0;
        }

        //
        _pat.RESULT_1_L_SPH  = sph_1;
        _pat.RESULT_1_L_CYL  = cyl_1;
        _pat.RESULT_1_L_AX   = CAlgoInvoker::correctAxis(qRound(ax_1));

        _pat.RESULT_2_L_SPH  = sph_2;
        _pat.RESULT_2_L_CYL  = cyl_2;
        _pat.RESULT_2_L_AX   = CAlgoInvoker::correctAxis(qRound(ax_2));

        _pat.RESULT_3_L_SPH  = sph_3;
        _pat.RESULT_3_L_CYL  = cyl_3;
        _pat.RESULT_3_L_AX   = CAlgoInvoker::correctAxis(qRound(ax_3));
    }
}

//void WinMeasure::showMonthAgeVision(std::vector<unsigned char *> &_img_list, enAgeRange _age_range, QDate _birth_date)
//{
//    // 虚构的计算等待时间
//    Util::waitMs(800);
//
//    //
//    stVisionValue vision;
//    vision = {};
//    stVisionAbnormal vision_abnormal;
//    vision_abnormal = {};
//
//    //
//    int pd = 0;
//    stPupilInfo pupil_info_r, pupil_info_l;
//    bool is_pupil_succ = m_algoInvoker->detectPupil(_img_list.at(1), 1, _age_range, pupil_info_r, pupil_info_l, false, singleDualEyeMode_Both);
//    if (is_pupil_succ) {
//        pd = std::round(std::sqrt(std::pow(pupil_info_r.center.x - pupil_info_l.center.x, 2) + std::pow(pupil_info_r.center.y - pupil_info_l.center.y, 2)) * PIX_TO_PHY);
//    }
//
//    QDate *birth_date_ptr = (_birth_date.isValid() ? &_birth_date : Q_NULLPTR);
//
//    if (!birth_date_ptr && (singleDualEyeMode_Both == g_SingleDualEye)) {
//        vision.PD = pd;
//    }
//
//    bool is_succ = m_algoInvoker->setMonthAgeVisionByBirthday(vision, getCurrentAgeRange(), birth_date_ptr);
//    if (is_succ) {
//        vision.PD = pd;         // 确保有瞳距，避免目前因为数据库修改麻烦而用 PD 和 PS（瞳孔直径） 是否都等于 0 来判断某一只眼睛是否是单眼模式下的未测眼，而月龄视力目前没有 PS，若无 PD，会导致结果页面为空
//
//        stVisionAbnormal vision_abnormal;
//        vision_abnormal = {};       // 按月龄估值后，值异常状态清零
//
//        // 结果拷贝
//        m_calcResultState = calcResultState_Succ;
//        m_resultErrMsg.clear();
//        m_resultList.clear();
//        m_resultList.push_back(vision);
//
//        // 结束测量
//        m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_MeasureFinished);
//    } else {
//        //
//        QString err_msg = tr("测量失败： ") + "ProgramError: (m-a-v)!";   // "Measurement failed: "
//
//        // 月龄估值发生异常，只能结束测量
//        m_calcResultState = calcResultState_Fail;
//        m_resultErrMsg = err_msg;
//        m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_MeasureFinished);
//    }
//}

bool WinMeasure::checkResultReliability(const stVisionValue Value)
{
    if (!g_AutoTest)
    {
        double maxDs, minDs, maxDc, minDc;
        switch(getCurrentAgeRange())
        {
        case 3://6-20岁
            maxDs = 4.75; minDs = -6; maxDc = 0; minDc = -3;
            break;
        case 4://20-100岁
            maxDs = 4; minDs = -7; maxDc = 0; minDc = -3;
            break;
        default:
            maxDs = 1000; minDs = -1000; maxDc = 1000; minDc = -1000;
            break;
        }

        if(Value.LSph > maxDs || Value.LSph < minDs //左眼球镜
                || Value.RSph > maxDs || Value.RSph < minDs//右眼球镜
                || Value.LCyl > maxDc || Value.LCyl < minDc //左眼柱镜
                || Value.RCyl > maxDc||Value.RCyl < minDc)//右眼柱镜
        {
            qDebug() << "compare result:" << Value.LSph << Value.RSph << Value.LCyl << Value.RCyl << maxDs << minDs << maxDc << minDc;
            return false;
        }
    }
    return true;
}

void WinMeasure::paintSingleEyeCover(QLabel *_label)
{
    Q_UNUSED(_label)
    // TODO: 绘制对角方向的网格线
}

void WinMeasure::setDebugPanelExpanded(bool _is_expanded)
{
    ui->wgtDebug->setVisible(_is_expanded);
    ui->btnDebugPanelSize->setVisible(_is_expanded);
    ui->btnDebugPanelExpand->setText(_is_expanded ? "<" : ">");
}

bool WinMeasure::isDebugPanelVisible()
{
    return ui->wgtDebug->isVisible();
}

void WinMeasure::doOnOpticalPathTypeChanged(enOpticalPathType _optical_type)
{
    Q_UNUSED(_optical_type)

    // 保存配置
    g_opticalPathType = _optical_type;
    appSetting::setValue("/tool/versiontype", (int)g_opticalPathType);
    appSetting::sync();

    //
    if (singleDualEyeMode_Both != g_SingleDualEye) {
        updateUi();
    }

    //
    updateView_btnOpticalType();

    // 设置“是否忽略距离”
    bool is_ignore_dist = (!settings::getCfg_IsEnableDistance() || opticalPathType_General != g_opticalPathType);
    setIsIgnoreDist(is_ignore_dist);
}

void WinMeasure::doOnSingleDualEyeChanged(enSingleDualEyeMode _single_dual_eye)
{
    Q_UNUSED(_single_dual_eye)

    // 保存配置
    g_SingleDualEye = _single_dual_eye;      // NOTE: 【单双眼模式】不永久保存

    //
    updateUi();

    //
    updateView_btnSingleDualEye();
}

//void WinMeasure::keyPressEvent(QKeyEvent *event)
//{
//    //
//    if (!this->isVisible()) {
//        return;
//    }
//
//    //
//    int key = event->key();
//    Qt::Key_Camera
//    if (key == Qt::Key_Escape && serailport_status == -1) {       /* Qt::Key_Escape 按键消息已统一在 WindowsManagers::eventFilter() 处理。 */
//        serailport_status = 0;
//        m_interSaturation = false;
//        timer->start(100);
//        if (!wave_status) {
//            emit wave_open();
//        }
//    }
    //qDebug()<<"key = "<<key;

/*
    QDir dir("/media/cut");
    if(!dir.exists()){
        dir.mkdir("/media/cut");
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow(0);
    QString filePathName = "/media/cut/cut-";
    filePathName += this->objectName();
    filePathName += ".png";

    if(!pixmap.save(filePathName,"png"))
    {
        qDebug()<<"cut save png failed"<<endl;
    }
*/

//}

void WinMeasure::showWaiting(bool _is_show)
{
    logDebug(QString::asprintf("WinMeasure::showWaiting(%s)", Util::bool2str(_is_show)), CGlobal::LOG_MEASURE);

    loadingMovie->setIsPlaying(_is_show);

    if (_is_show) {
        ui->frmDistBg->setVisible(false);
    }

}

// 左上角扫码按钮
void WinMeasure::on_btnScanBarcode_clicked()
{
    qDebug() << "press on_pushButton_clicked";
    setIsBarcodeStat(!isBarcodeStat);
}

void WinMeasure::setIsBarcodeStat(bool _is_barcode_stat)
{
    //if (_is_barcode_stat != isBarcodeStat)
    {
        isBarcodeStat = _is_barcode_stat;

        //
        ui->lblBarcodeScanMask->setVisible(isBarcodeStat);

        //
        if (isBarcodeStat) {
            barcodeSendCount = 0;
        }
    }
}

void WinMeasure::on_btnMusic_clicked()
{
    //
    setMusicStateCfg(!getMusicStateCfg());

    // 音乐状态切换
    musicControl(getMusicStateCfg());
    coloredLampControl(getMusicStateCfg());

    //
    qDebug() << "music State changed:" << getMusicStateCfg();
    updateView_btnMusic();

}

void WinMeasure::musicControl(bool _is_opened)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, _is_opened = " << Util::bool2str(_is_opened);

    // 若本产品的功能未包含音乐，则跳过     // NOTE: 函数参数 _is_opened 来自本模块的音乐开关配置，而 CGlobal::getIsMusicEnabled() 是全局的产品特性的配置
    if (!CGlobal::getIsMusicEnabled()) {
        logDebug("this product model has no music function!");
        return;
    }

    // 若已打开语音提示，则不播放音乐      // NOTE: 音乐和语音是互斥的，即若已开启了“语音提示”，则不再播放音乐
    if (CGlobal::isVoicePrompt) {       // NOTE: 语音提示不需执行停止动作
        logDebug(QString("%1::%2(): VoicePrompt is opened, music playing skipped!").arg(S_CLASS_NAME).arg(__FUNCTION__));
        return;
    }

    // 播放音乐
    if (_is_opened) {
#if (OS_TYPE == 1)
    system("echo 1 > /sys/class/gpio/gpio80/value");        // NOTE: 天嵌平台启用声卡
#endif
        g_SoundIntf->playMusics();
    } else {
        g_SoundIntf->stop();
#if (OS_TYPE == 1)
    system("echo 0 > /sys/class/gpio/gpio80/value");        // NOTE: 天嵌平台关闭声卡
#endif
    }
}

void WinMeasure::playVoicePrompt(enVoicePrompt _voice)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    // 若本产品的功能未包含音乐，则跳过         // NOTE: CGlobal::getIsMusicEnabled() 是全局的产品特性的配置，而本模块的音乐开关配置是 WinMeasure::getMusicStateCfg()
    if (!CGlobal::getIsMusicEnabled()) {
        logDebug("this product model has no music function!");
        return;
    }

    //
    if (!CGlobal::isVoicePrompt) {
        //logDebug(QString("%1::%2(): VoicePrompt is not opened!").arg(S_CLASS_NAME).arg(__FUNCTION__));
        return;
    }

    //
    if (g_SoundIntf->isPlaying()) {
        qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): is playing?! action skipped!";
        return;
    }

    // 播放语音提示
    g_SoundIntf->playVoice(_voice);
}

void WinMeasure::coloredLampControl(bool _is_opened)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    if (!CGlobal::getIsColoredLampEnabled()) {
        logDebug("this product model has no colored lamp function!");
        return;
    }

    //
    if (_is_opened) {
        //Util::waitMs(100);                    /* 这里加延时是为了避免连续发送指令导致无效，但是串口已加入了延时机制，所以这里不再需要。 */
        serialBaseBoard->write(COMMAND_COLOR_LAMP_ON);
        //Util::waitMs(100);
    } else {
        //Util::waitMs(100);
        serialBaseBoard->write(COMMAND_COLOR_LAMP_OFF);
        //Util::waitMs(100);
    }
}

void WinMeasure::slot_upLoadWork_MeasureCtrl(string cmd)
{
    //
    qDebug() << "slot_upLoadWork_MeasureCtrl";
    if (!CGlobal::getIsExternalControl()) {     // 如果不是受控模式，逻辑错误，返回
        qDebug() << "logic error: not CGlobal::getIsExternalControl() !";
        return;
    }

    qDebug() << "cmd:" << QString::fromStdString(cmd);
    if(cmd == FUNC_START){
        g_PupilState = 0;
        WinMeasure::setOperationMode(operationMode_InputMeasure);

        getWinManage()->openMeasureWin(m_patient, patientSource_Command);
    }
    else if(cmd == FUNC_STOP){
        if(dev_stat == AVAILABLE)
            return;
        if(isOpened()){
            logDebug("WinMeasure::slot_upLoadWork_MeasureCtrl(): 收到退出指令，即将退出测量界面！");
            goBack();
            qDebug()<<"stop camera!";
        }
    }
    else if (cmd == FUNC_GRAB_FRAME)
    {
        qDebug()<<"grab frame";
        bool succ_into = m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_Collect);    // TODO: 根据当前状态是否适合转灯来处理？
        if (!succ_into) {
            // TODO:

        } else {
            // TODO: 应答当前状态不可转灯

        }
    }
    else if (cmd == FUNC_POWER_OFF)
    {
        // 返回主页
        goBack();

        // 提示关机
        emit sendSIGNAL(sysSignal_PowerOffCommand);
    }
}

void WinMeasure::slot_captureThread_FrameCaptured(int _img_idx, uchar *_img_data, int _img_num)
{
#if ENABLE_PREVIEW_FRAME_VERBOSE_LOG
    qDebug() << QString("%1::%2(): entered, %3").arg(S_CLASS_NAME).arg(__FUNCTION__)
                .arg(_img_num >= 0 ? QString(" TurnLamp image: %1").arg(_img_num) : " not TurnLamp image");
#endif

    //
    m_captureThread->setCountFrameSent(m_captureThread->countFrameSent() - 1);

    //
    if (countRePowerOn > 0) {
        countRePowerOn = 0;
    }

    //
    if (!isOpened()) {
        return;
    }

    //
    currentFrameData = _img_data;

    // 如果是扫码状态
    if (isBarcodeStat) {
        if (!isBarcodeImgSent) {
            emit sigGotBarcodeImg(_img_data);
            isBarcodeImgSent = true;
            barcodeSendCount++;
        }
    }

    // 是否转灯图
    bool is_turn_lamp_img = (_img_num >= 0);

    // 是否需要时别瞳孔
    bool is_need_detect = (getIsDistanceNearFit() && (getMeasureStep() < measureStep_Collect) &&
                           !isBarcodeStat && !isDistCalibration && !isFocusMode && !m_isWaitingForAlgoFinish);

    // 是否旧描圆算法
    bool is_old_draw_circle = (algoVerAll_2019 == CGlobal::getCurrentPupilAlgoVer());

    // 若是转灯图，推送
    if (is_turn_lamp_img) {
        //
        if (!g_isTurnLampTestMode && !m_isFinished) {
            m_measureCtrl->inputTurnLampImg(_img_idx, _img_num);
        }
    }

    // 若是最后一个转灯帧，正式异步模式由算法判断轮次质量；传统模式保留末帧门禁。
    if (22 == _img_num && !m_isFinished) {
        bool formalAsync = false;
        if (m_algoInvoker != nullptr) {
            const stAlgoCommandResult commandResult =
                    m_algoInvoker->executeAlgoCommand(
                            stAlgoCommand::makeQueryFormalAsyncPupilMode());
            formalAsync = commandResult.success && commandResult.boolValue;
        }
        if (!formalAsync) {
            stPupilInfo pupil_info_r, pupil_info_l;
            m_isLastTurnLampeFramePupilFound = m_algoInvoker->detectPupil(_img_data, _img_idx, (enAgeRange)getCurrentAgeRange(), pupil_info_r, pupil_info_l, false, g_SingleDualEye);
        }
    }

    // 确定是否需要绘帧
    bool is_need_paint = true;
    //bool is_need_paint = (getMeasureStep() < measureStep_Collect);
    static int count_frame = 0;
    count_frame++;
    if (getMeasureStep() >= measureStep_Collect) {
        if(count_frame % 4 != 0)
            is_need_paint = false;
    }

    if (!is_need_paint) {
        return;
    }

    // 如果是调试模式，计算测试所需的图像信息
    if (CGlobal::isDebugMode || isDistCalibration || isFocusMode) {
        emit sigCalcImgInfo(_img_data);
    }

    // 推送图像给绘帧对象（拷贝到【待绘制的图像】中）
    bool is_need_enhance = (isFocusMode ? true : !isShowRawImg);    // 是否增强图像
    frameDrawer->pushImage(_img_data, is_need_enhance);

    // 瞳孔识别
    if (is_need_detect) {
        if (!is_old_draw_circle) {
            // 发送瞳孔识别信号（【准备阶段且距离合适】 或 【采集阶段】）
            if (is_need_detect) {
                bool is_need_calc_expo = ( getIsDistanceFit() && !(m_exposureAdjuster->getIsFinished()) );
                emitPupilDetect(_img_data, _img_idx, is_need_calc_expo);
            }
        } else {
            // 旧算法识别瞳孔并描圆到图像数据中
            bool is_old_algo_succ = false;      // 旧算法瞳孔时别是否成功
            is_old_algo_succ = frameDrawer->detectPupilAndDrawCircle(_img_data);  // 旧简单算法识别瞳孔并描圆（已包含将图像拷贝到【待绘制的图像】中）
            if (getIsDistanceFit()) {
                if (is_old_algo_succ) {
                    if (0 == countPupilDetection) {
                        mTimeFirstPupilDetected.restart();
                    }
                    countPupilDetection++;
                } else {
                    countPupilDetection = 0;     // 出现识别瞳孔失败的情况时，识别瞳孔数清零
                }
            }

        }
    }

    // 确定刷帧时的图像翻转模式（为了适配操作者的感知）
    int drawing_flip_mode = -2;     // NOTE: -2：不处理，[-1~1]：与 OpenCV 的 cvFlip() 函数的 flip_mode 参数含义一致：-1:同时垂直和水平翻转，0:垂直翻转（上下翻转），1:水平翻转（左右翻转）
    // NOTE: core_c.h -> cvFlip() 里的备注“around horizontal (flip=0)”是指绕着水平线即x轴翻转，就是垂直翻转（上下翻转），而不是指水平翻转（左右翻转）！

    bool is_flip_hori = false;
    bool is_flip_vert = false;
    if (opticalPathType_Square == g_opticalPathType) {             // 若使用了方形视筛箱，则图像垂直镜像，来适配操作者的感知
        is_flip_vert = true;
    } else if (opticalPathType_LShape == g_opticalPathType) {      // 若使用了 L 形视筛箱，则图像无需调整，来适配操作者的感知
        //
    }

    if (is_flip_hori && is_flip_vert) {
        drawing_flip_mode = -1;
    } else if (!is_flip_hori && is_flip_vert) {
        // 设置为垂直翻转（上下翻转）
        drawing_flip_mode = 0;
    } else if (is_flip_hori && !is_flip_vert) {
        // 设置为水平翻转（左右翻转）
        drawing_flip_mode = 1;
    }

    // 刷帧
    if (is_need_paint) {
        // 限制刷帧频率
        //if (elapsedFrameGot.elapsed() < 50) {
        //    return;
        //}
        //elapsedFrameGot.restart();

        // 绘帧
        bool is_draw_circle = (is_need_detect && !is_old_draw_circle);
        frameDrawer->updateFrame(is_draw_circle, isQtDraw, drawing_flip_mode);

        //
        this->update();
    }

    //qDebug() << QString("%1::%2(): exited").arg(S_CLASS_NAME).arg(__FUNCTION__);
}

void WinMeasure::slot_captureThread_finished()
{
    logDebug("WinMeasure::slot_captureThread_finished() into ...", CGlobal::LOG_MEASURE);

    // 检查抓图线程是否正常退出，否则做相应处理
    if (m_captureThread->getIsUnexpectlyQuit() && (this->isVisible() || isOpened()))
    {
        logCritical("WinMeasure::slot_captureThread_finished(): capture thread quitted unexpectedly!", CGlobal::LOG_CAPTURE);

        //
        if (CGlobal::isDebugMode)
        {
            getWinManage()->showSuspensionPrompt("Capture thread quitted unexpectedly!", -1);
        }

        // 若在测量过程中，则重启相机
        //if (m_measureStep >= measureStep_Ready && m_measureStep <= measureStep_Collect)
        {
            slot_ResetCamera();
        }

        // TODO: 更完备的处理逻辑？？

    }

}

/******************* 2020.10.12 tao *******************/
//获取设备状态
void WinMeasure::getDevStat(string& _stat)
{
//    dLocker.lock();
    _stat = dev_stat;
//    dLocker.unlock();
    return;
}

//获取运行状态
void WinMeasure::getRunStat(string& _stat)
{
//    rLocker.lock();
    _stat = run_stat;
//    rLocker.unlock();
    return;
}

//设置设备状态
void WinMeasure::setDevStat(const string _stat)
{
    dev_stat = _stat;
    QString mStat = "dev_stat";
//    emit sendBlueToothData(mStat);
    return;
}

//设置设备运行状态
void WinMeasure::setRunStat(const string _stat)
{
    run_stat = _stat;
    return;
}

enOperationMode WinMeasure::getOperationMode()
{
    return m_operationMode;
}

void WinMeasure::setOperationMode(enOperationMode _mode)
{
    m_operationMode = _mode;
}

// 获取当前年龄段
enAgeRange WinMeasure::getCurrentAgeRange()
{
    /** 当前年龄段确定规则：
     *      若是用户按下物理按键触发的测量，则当前年龄段为上一次测量时的年龄段。若是其它情况触发的测量，都须传入年龄段，传入的年龄段即为当前年龄段，否则使用设定的缺省年龄段。
     */

    return currentAgeRange;
}

enAgeRange WinMeasure::getAgeRangeLimited(const enAgeRange &_age_range)
{
    enAgeRange age_range = _age_range;

    if (_age_range < ageRange_Min) {
        age_range = ageRange_Min;
    } else if (_age_range > ageRange_Max) {
        age_range = ageRange_Max;
    }

    return age_range;
}

// 设置当前年龄段
void WinMeasure::setCurrentAgeRange(enAgeRange _age_range)
{
    currentAgeRange = _age_range;
}

void WinMeasure::setLampPwrOpened(bool _is_opened)
{
    if (_is_opened) {
        serialBaseBoard->write(openUSandIR);
    } else {
        serialBaseBoard->write(closeIR);
    }
}

void WinMeasure::sendTurnLampCmd(int _interval)
{
    qDebug() << QString("%1::%2(): entered, _interval = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(_interval);

    // NOTE: 硬触发间隔需大于等于0才有效，否则最终的指令发送函数会取缺省值 18ms

    //
    stateOfCmd = 1;

    //
    if (_interval <= 0) {
        if (CGlobal::hardTriggerIntervalDelayMs > 0.00001 && m_exposureAdjuster->getIsFinished()) {     // 若“触发间隔附加延时”大于0，且已完成调光，则指定硬触发间隔
            _interval = qCeil((double)m_measureCtrl->getExposureTime() / 1000 + CGlobal::hardTriggerIntervalDelayMs);   // NOTE: (2026-01-07)因为下位机在电位下拉完成后才开始延时，所以上升沿触发时不需补上相机的触发延时
        }
    }

    //
    static const int LEN_CMD = 8;
    static char cmd[LEN_CMD];

    memcpy(cmd, capture_command.constData(), LEN_CMD);
    if (_interval > 0) {
        uchar speed = (uchar)(_interval);
        cmd[5] = speed;
    }

    serialBaseBoard->write((uchar *)cmd, LEN_CMD);
}

qint64 WinMeasure::writeCaliSerial(const char *_data, qint64 _size)
{
    Q_UNUSED(_data)
    Q_UNUSED(_size)

    return 0;

    // TODO:

}

QByteArray WinMeasure::readCaliSerialAll()
{
    return QByteArray("");

    // TODO:

}

void WinMeasure::clearCaliSerial(QSerialPort::Direction _direction)
{
    Q_UNUSED(_direction)
    // TODO:
}

// 从底板查询超声、电量等信息
void WinMeasure::queryInfosFromBaseBoard()
{
    //logDebug(QString("%1:%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    // 若底板版本号无效，查询一次
    stVerInfo ver_base_baord = MySerialPort::getFirewareVersion();
    if (ver_base_baord.isNull()) {
        //
        aboutdevice::sendQueryStm32Version();
        QThread::msleep(20);
    }

    //
    stateOfCmd = 0;
    //Util::waitMs(20);         /* 这里加延时是为了避免连续发送指令导致无效，但是串口已加入了延时机制，所以这里不再需要。 */
    serialBaseBoard->write(wave_command);
}

void WinMeasure::on_ckbIsTurnLampTest_clicked(bool _checked)
{
    m_isTurnLampTest = _checked;
}

void WinMeasure::on_btnGoIntoDetect_clicked()
{
    bool succ_into_detect = m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_Ready);
    if (!succ_into_detect) {
        qDebug() << __PRETTY_FUNCTION__ << ": failed to into step_ready!";
        // TODO:

    }
}

void WinMeasure::on_btnTurnLamp_clicked()
{
    if (m_captureThread && captureStep_TurnLamp == m_captureThread->getCaptureStep()) {
        if (m_isForceRestartTurnLamp) {
            return;
        }

        // 转灯中再次点击“转灯”：中止当前轮，保存残轮图，再由回调开启新一轮。
        m_isForceRestartTurnLamp = true;
        m_measureCtrl->setTurnLampSaveSource("manual_restart");
        m_captureThread->abordTurnLamp();
        return;
    }

    // 手动转灯
    isrun = true;
    bool succ_into_turnlamp = m_measureCtrl->jumpIntoMeasureStepImmediately(measureStep_Collect, true);
    isrun = false;
    if (!succ_into_turnlamp) {
        qDebug() << __PRETTY_FUNCTION__ << ": failed to into step_turnlamp!";
        // TODO:

    }
}

// 距离检测对象【距离改变信号】槽函数
void WinMeasure::slot_distanceDetect_DistanceChanged(int _new_dist)
{
    doOnDistanceReceived(_new_dist);
}

void WinMeasure::slot_distanceDetect_CheckSensorTypeFinished()
{
    globalService()->checkStartupEvent(3);
}

void WinMeasure::on_ckbLightOn_clicked(bool checked)
{
    setLampPwrOpened(checked);
}

void WinMeasure::on_ckbFixExposure_clicked(bool _checked)
{
    if (_checked) {
        //
        int expo = ui->edtExposure->text().toInt();
        if (expo < 0) {
            expo = 0;
        }

        QString expo_str = QString::number(expo, 'f', 0);
        ui->edtExposure->setText(expo_str);

        //
        setFixedExposure(expo);

        //
        getWinManage()->showSuspensionPrompt(QString("固定曝光时间为 %1 微秒").arg(expo_str));
    } else {
        //
        setFixedExposure(-1);
    }

    //
    measureStatView->updateMeasureStat();
}

void WinMeasure::setFixedExposure(int _expo)
{
    if (_expo > 0) {
#if ENABLE_EXPOSURE_TIMING_LOG
        const int previousExposure = g_CameraIntf->getExposureTime();
        const auto setExposureStartedAt = std::chrono::steady_clock::now();
#endif
        m_measureCtrl->setExposureTime(&_expo);
#if ENABLE_EXPOSURE_TIMING_LOG
        m_exposureAdjuster->recordExposureCommandTime(
                std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - setExposureStartedAt).count(),
                previousExposure,
                g_CameraIntf->getExposureTime());
#endif
        m_exposureAdjuster->setIsFixed(true);
        m_fixedExposure = _expo;
        measureStatView->setIsExposureFixed(true);
    } else {
        m_exposureAdjuster->setIsFixed(false);
        m_fixedExposure = -1;
        measureStatView->setIsExposureFixed(false);
    }
}

void WinMeasure::on_edtExposure_textEdited(const QString &_arg1)
{
    if (ui->ckbFixExposure->isChecked()) {
        //
        int expo = _arg1.toInt();
        if (expo < 0) {
            expo = 0;
        }

        QString expo_str = QString::number(expo, 'f', 0);
        ui->edtExposure->setText(expo_str);

        //
        setFixedExposure(expo);
        measureStatView->updateMeasureStat();
    }
}

void WinMeasure::on_btnSetGain_clicked()
{
    float v = ui->edtGain->text().toFloat();
    if (v < 1.0) {
        v = 1.0;
    } else if (v > 4.0) {
        v = 4.0;
    }
    g_CameraIntf->setAnalogGain(v);

    ui->edtGain->setText(QString::number(v, 'f', 2));

    getWinManage()->showSuspensionPrompt(QString("增益已设置为 %1 倍").arg(v));
}

void WinMeasure::on_btnSetPwmDuty_clicked()
{
    int v = ui->edtPwmDuty->text().toInt();

    if (v < 0) {
        v = 0;
    } else if (v > 100) {
        v = 100;
    }

    pwmDutyPercent = v;
    serialBaseBoard->sendSetPwmDuty(pwmDutyPercent);

    //Util::waitMs(300);        /* 这里加延时是为了避免连续发送指令导致无效，但是串口已加入了延时机制，所以这里不再需要。 */
    sendTurnLampCmd(3);         /* 设置电流参数后，要转一次灯才生效 */

    ui->edtPwmDuty->setText(QString::number(v));

    getWinManage()->showSuspensionPrompt(QString("占空比已设置为 %1 %").arg(v));
}

void WinMeasure::on_ckbIgnoreDistance_clicked(bool checked)
{
    setIsIgnoreDist(checked);
}

void WinMeasure::on_btnSaveDistCalitrData_clicked()
{
    if (distCalibration) {
        if (distCalibration->getIsStarted()) {
            QMessageBox::information(this, "提示", "请先停止");
            return;
        } else if (distCalibration->getCount() == 0) {
            QMessageBox::information(this, "提示", "请先采样");
            return;
        }

        QString disk_path = Util::CUDisk::getPath();
        if (disk_path.length() > 0) {
            //
            Util::CUDisk::remount();        // TODO: 支持中文文件名      // TODO: 为什么加上之后保存不了了？

            //
            //QTextCodec *codec_old = QTextCodec::codecForLocale();       /* 当前编码是不确定的，因为有两三个模块改变了本地编码 */     // TODO: 编码的统一，禁止改变全局编码，如 QTextCodec::setCodecForLocale() ？
            //QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF8"));

            //
            myEditLine edit;
            logDebug("WinMeasure::on_btnSaveDistCalitrData_clicked(): showing keyboard", CGlobal::LOG_TEMP);
            getWinManage()->showKeyboard(&edit, this, true);
            while (!edit.getIsKeyboardUpdated()) {              // TODO: 这个应该要在独立线程执行，否则会干扰本线程？
                QCoreApplication::processEvents();
                QThread::msleep(10);
            }
            logDebug("WinMeasure::on_btnSaveDistCalitrData_clicked(): keyboard returned", CGlobal::LOG_TEMP);

            QString file_name_tag = edit.text();
            if (file_name_tag.length() > 0)
                file_name_tag = QString("_") + file_name_tag;

            disk_path = Util::CUDisk::getPath();
            QString file_path = disk_path + QString("/clarity_distance_%1%2.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")).arg(file_name_tag);
            QString msg;
            distCalibration->saveDataToFile(file_path, &msg);

            //
            Util::CUDisk::sync();
            //Util::CUDisk::umount();

            //
            //QTextCodec::setCodecForLocale(codec_old);

            //
            if (msg.length() == 0) {
                logDebug(QString("save to: ") + file_path, CGlobal::LOG_TEMP);
                QMessageBox::information(this, "提示", "保存成功");
            } else {
                QMessageBox::information(this, "提示", QString("保存失败：") + msg);
            }
        } else {
            QMessageBox::information(this, "提示", "请插入 U 盘");
        }
    }
}

void WinMeasure::on_btnStart_clicked()
{
    if (ui->btnStart->text() == TEXT_START) {
        distCalibration->setIsStarted(true);
        ui->btnStart->setText(TEXT_STOP);
    } else {
        distCalibration->setIsStarted(false);
        ui->btnStart->setText(TEXT_START);

        distCalibrationCheckAndApply();
    }
}

void WinMeasure::on_ckbIsShowRawImg_clicked(bool checked)
{
    if (m_captureThread) {
        this->isShowRawImg = checked;

        m_captureThread->setIsUseRawImg(this->isShowRawImg);
    }
}

void WinMeasure::on_ckbIsQtDraw_clicked(bool checked)
{
    setIsQtDraw(checked);
}

void WinMeasure::on_cbbPupilAlgoVer_currentIndexChanged(int index)
{
    if (!this->isVisible()) {
        return;
    }

    // 用户切换算法时，自动强制指定算法         // TODO: 这里不因该改变“是否强制指定算法”的设置？  // TODO: 程序内部修改此值，也会触发此事件？
    //CGlobal::isSpecifiedAlgo = true;

    // 切换算法
    CGlobal::setPupilAlgoVer((enAlgoVerAll)index);
    CGlobal::judgeAndSetPupilAlgoVer(getCurrentAgeRange());
    m_algoInvoker->setCurrentPupilAlgoVer(CGlobal::getCurrentPupilAlgoVer());
}

void WinMeasure::on_ckbIsPressKeySave_clicked(bool checked)
{
    isPressKeySave = checked;

    // 检查确认 U 盘存在
    if (isPressKeySave) {
        QString udisk_path = Util::CUDisk::getPath();
        if (udisk_path.length() == 0) {
            isPressKeySave = false;
            ui->ckbIsPressKeySave->setChecked(false);

            QMessageBox::information(this, "提示", "请插入 U 盘");
        }
    }
}

void WinMeasure::on_ckbIsUserTriggerPupilDetectMode_clicked(bool checked)
{
    isPupilDetectUserTriggerMode = checked;

    if (isPressKeySave) {
        // 检查确认 U 盘存在
        if (isPupilDetectUserTriggerMode) {
            QString udisk_path = Util::CUDisk::getPath();
            if (udisk_path.length() == 0) {
                isPupilDetectUserTriggerMode = false;
                ui->ckbIsUserTriggerPupilDetectMode->setChecked(false);

                QMessageBox::information(this, "提示", "请插入 U 盘");
            }
        }
    }

    //
    ui->lblPupilDetectCount->setVisible(isPupilDetectUserTriggerMode);
}

void WinMeasure::slotPhysicButtonPressed()
{
    //qDebug() << "===== WinMeasure::doAfterKeyEscapePressed() ==========";

    // 若本窗体不是当前窗体，则不处理此信号
    if (this != getWinManage()->getCurrentWin()) {
        return;
    }

    //
    if (isPupilDetectUserTriggerMode) {     // “瞳孔测试模式”优先处理
        isPupilDetectUserTriggered = true;
    } else if (isPressKeySave) {            // “按键存图”
        QString img_dir_name = m_patient.getImgDirName();
        QString relative_path = QString("capture/%1_%2.bmp").arg(img_dir_name).arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz"));
        this->saveImgToUdisk(relative_path, currentFrameData);
    }
}

//
void WinMeasure::slot_algoInvoker_PupilDetectionResult(uchar *_img_data, int _img_idx, bool _succ, stPupilInfo _pupil_info_r,
                                                 stPupilInfo _pupil_info_l, int _avg, bool _over_expo)
{
    logDebug(QString("%1:%2(): entered, _img_idx = %3, _is_succ = %4").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(_img_idx).arg(Util::bool2str(_succ)));

    // 相关计数
    if (_succ) {    /* 这里得到瞳孔识别结果时，距离必合适，不需判断 */
        //
        if (0 == countPupilDetection) {
            mTimeFirstPupilDetected.restart();
        }
        countPupilDetection++;
        countPupilDetectionSucc++;
    } else {
        countPupilDetection = 0;
        countPupilDetectionSucc = 0;
    }

    //
    if (!this->isVisible()) {
        return;
    }

    // 成功识别到瞳孔之后，显示原始图像（不做图像增强）
    if (algoMode_Professional == CGlobal::algoMode) {               // 专业模式才这样显示
        isShowRawImg = _succ;
    }

    // 受控模式的数据发送
    if ((!_succ) && CGlobal::getIsExternalControl()) {
        std::string stat_str = UNDETECTED;      // 发送事件：瞳孔检测失败
        WinMeasure::setRunStat(stat_str);
        UpLoadThread::sendRunStat(stat_str);
    }

    // 瞳孔算法测试状态的处理
    if (isPupilDetectUserTriggered) {
        if (isPressKeySave) {
            QString udisk_path = Util::CUDisk::getPath();
            if (udisk_path.length() > 0) {
                QString relative_path = QString("pupil-detec/%1_%2_%3_%4.bmp")
                        .arg(m_patient.patientid)
                        .arg(ui->cbbPupilAlgoVer->currentText())
                        .arg(_succ ? "succ" : "fail")
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz"));
                this->saveImgToUdisk(relative_path, _img_data, udisk_path);
            } else {
                //
            }
        }

        //
        pupilDetectUserTriggerCount++;
        if (_succ) {
            pupilDetectUserTriggerSuccCount++;
        }

        //
        isPupilDetectUserTriggered = false;
    }

    // 曝光时间调整
    if (!m_exposureAdjuster->getIsFinished()) {
        const bool hasValidAvg = _succ && _avg >= 0;
        if (hasValidAvg || !_succ) {
            const int currentExposure = g_CameraIntf->getExposureTime();
            int new_expo = m_exposureAdjuster->inputExposureInfo(_succ, _avg, _over_expo, currentExposure);
            if (new_expo > 0) {
                // 若连续多次得到的曝光时间相等，则自动调整曝光时间失败，弹出提示
                //if (_succ && m_exposureAdjuster->countExpoSame() == 5) {      // NOTE: 只触发一次提示；只有瞳孔识别成功时才会进入根据灰度调整阶段
                //    QString err_msg = tr("调整曝光时间失败：\n");            // "Failed to adjust exposure time: \n"
                //    if (m_exposureAdjuster->isMax()) {
                //        //err_msg += tr("曝光时间已调至最大但瞳孔灰度仍过低！");  // "The exposure time has been adjusted to the maximum, but the pupil grayscale is still too low!"
                //        err_msg += tr("瞳孔亮度过低！");   // "Pupil brightness too low!"
                //    } else if (m_exposureAdjuster->isMin()) {
                //        err_msg += tr("瞳孔亮度过高！");   // "Pupil brightness too high!"
                //    }
                //    getWinManage()->showSuspensionPrompt(err_msg, -1);
                //}

                //
#if ENABLE_EXPOSURE_TIMING_LOG
                const auto setExposureStartedAt = std::chrono::steady_clock::now();
#endif
                m_measureCtrl->setExposureTime(&new_expo);      // TODO: 这里能确保相机参数生效前，不会出现连续多帧曝光一致，且数量超出了前面的检查的情况出现吗？
#if ENABLE_EXPOSURE_TIMING_LOG
                m_exposureAdjuster->recordExposureCommandTime(
                        std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - setExposureStartedAt).count(),
                        currentExposure,
                        g_CameraIntf->getExposureTime());
#endif
            }
        } else if (getIsDistanceFit()) {
            // 瞳孔识别成功但灰度无效时，按失败输入推动曝光状态机，避免状态停滞。
            const int currentExposure = g_CameraIntf->getExposureTime();
            int new_expo = m_exposureAdjuster->inputExposureInfo(false, -1, _over_expo, currentExposure);
            if (new_expo > 0) {
#if ENABLE_EXPOSURE_TIMING_LOG
                const auto setExposureStartedAt = std::chrono::steady_clock::now();
#endif
                m_measureCtrl->setExposureTime(&new_expo);
#if ENABLE_EXPOSURE_TIMING_LOG
                m_exposureAdjuster->recordExposureCommandTime(
                        std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - setExposureStartedAt).count(),
                        currentExposure,
                        g_CameraIntf->getExposureTime());
#endif
            }
            logWarning("WinMeasure::slot_algoInvoker_PupilDetectionResult(): exposure params error!");
        }
    }

    // 若在采集阶段，则输入瞳孔识别信息到控制器
    //if (measureStep_Collect == getMeasureStep() && m_measureCtrl) {
    //    m_measureCtrl->inputPupilDetectInfo(_succ, _img_idx, _pupil_info_r, _pupil_info_l);
    //}

    // 转灯判断
    if (_succ && (measureStep_Ready == getMeasureStep())) {
        m_measureCtrl->judgeTurnLamp();
    }

    //
    if (!_succ && measureStep_Ready == getMeasureStep() && !g_SoundIntf->isPlaying()) {
        // 语音提示：注视固视灯
        playVoicePrompt(enVoicePrompt::FocusOnLight);
    }

    // 更新测量状态显示部件
    measureStatView->updateMeasureStat();

    // 设置瞳孔信息到绘帧
    frameDrawer->setPupilInfo(_succ, _pupil_info_r, _pupil_info_l);

}

void WinMeasure::slot_measureCtrl_GoIntoMeasureStep(enMeasureStep _step)
{
    logDebug(QString("%1: into ..., _step = %2, threadId = %3").arg(__PRETTY_FUNCTION__).arg(_step).arg((unsigned long)QThread::currentThreadId()), CGlobal::LOG_MEASURE);

    //
    //g_WinMeasure->goIntoMeasureStep(_step);
    m_measureCtrl->jumpIntoMeasureStepImmediately(_step);

    //
    logDebug(QString("%1: exited."), CGlobal::LOG_MEASURE);
}

void WinMeasure::slot_measureCtrl_ErrMsg(QString _err_str)
{
    getWinManage()->showSuspensionPrompt(_err_str);
}

void WinMeasure::slot_measureCtrl_StartMeasure()
{
    // 开始测量过程
    startMeasure();
}

void WinMeasure::slot_exposureAdjuster_MsgNotify(QString _msg)
{
    getWinManage()->showSuspensionPrompt(_msg);
}

//
void WinMeasure::slot_captureThread_CaptureErr(enCaptureError _err_code, QString _err_str)
{
    logCritical("WinMeasure::slot_captureThread_CaptureErr() into ...");

    if (CGlobal::isDebugMode) {
        if (captureError_FrameLoss == _err_code) {
            ui->lblDistLog->setText(ui->lblDistLog->text() + "\n Lost Frame! " + _err_str);
        } else if (captureError_FrameExcess == _err_code) {
            ui->lblDistLog->setText(ui->lblDistLog->text() + "\n More Frame! " + _err_str);
        }
    }

    //
    if (captureError_CameraUnusable == _err_code) {
        // 相机重上电
        cameraRePowerOn();
    }
}

void WinMeasure::cameraRePowerOn()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    static bool is_restoring = false;
    if (!is_restoring) {
        is_restoring = true;        // 避免重复重上电      // TODO: 有必要？足够严谨？

        /* 相机发生不可恢复错误后，退出到主界面 */
        //stopMeasure();
        //getWinManage()->showSuspensionPrompt(QString(language ? "相机错误：" : "camera err: ") + _err_str, -1);
        //getWinManage()->showWindowByType(WIN_HOME);

        /* 相机发生不可恢复错误后，重启相机电源，重新初始化相机，并重新进入测量。
         * 非连拍，一直自动重上电；连拍模式，限若干次自动重上电；调试模式，一律不自动重上电；
         */                                                                     // TODO: 有必要限制次数吗？过多重上电是否对相机寿命有损害？
        bool is_re_power_on = true;
        if (g_AutoTest) {
            if (countRePowerOn > 10) {          // 自拍模式，限制重上电次数
                is_re_power_on = false;
            }
        } else {
            if (CGlobal::isDebugMode) {         // 调试模式下禁用自动重上电
                is_re_power_on = false;
            }
        }

        if (is_re_power_on) {
            countRePowerOn++;

            // 等待信息显示（最好锁定界面）
            int msg_id = getWinManage()->showSuspensionPrompt(tr("加载中(%1)，请稍后 ...").arg(countRePowerOn), 8000); // "loading(%1), please wait ..."
            qApp->processEvents();

            // 结束测量
            stopMeasure();

            // 相机重上电
            bool is_succ = CameraInitThread::cameraRePowerOn();
            if (!is_succ) {
                // TODO:
            }

            // 重新进入测量
            getWinManage()->hideSuspensionPrompt(msg_id);

            startMeasure();
        } else {
            goBack();
            getWinManage()->showSuspensionPrompt(tr("相机无法使用"), -1); // "camera unable to use"
        }

        //
        is_restoring = false;
    }
}

void WinMeasure::saveImgToUdisk(QString _relative_path, uchar *_img_data, QString _udisk_path)
{
    if (_img_data) {
        if (_udisk_path.length() == 0)
            _udisk_path = Util::CUDisk::getPath();
        if (_udisk_path.length() > 0) {
            // 保存当前帧图像
            QString file_path = _udisk_path + QDir::separator() + _relative_path;
            Util::makePath(Util::getDirOfPath(file_path));
            Util::saveImgDataToImgFile(_img_data, IMG_WIDTH, IMG_HEIGHT, file_path);

            //QMessageBox::information(this, "提示", "已保存当前帧");

            //
            Util::CUDisk::sync();
        } else {
            QMessageBox::information(this, "提示", "请插入 U 盘");
        }
    } else {
        QMessageBox::information(this, "errir", "_img_data is null!");
    }
}

bool WinMeasure::goIntoMeasureStep(const enMeasureStep _into_step, bool _is_force)
{
    qDebug() << QString("%1::%2(): entered, _into_step = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(enumToText_MeasureStep(_into_step));

    //
    if (!isOpened()) {
        qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): LogicError: measure not opened, action skipped!";
        output_trace<void>(4);
        return false;
    }

    //
    QString err_str = "";
    bool is_can_go_on = true;
    bool is_err = false;
    bool succ_into = false;

    //
    is_can_go_on = judgeIntoMeasureStep(_into_step, is_err, err_str);

    //
    if (is_can_go_on || _is_force) {
        succ_into = doIntoMeasureStep(_into_step);
    } else if (!is_err) {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): doIntoMeasureStep() no error skipped: " << err_str;
        succ_into = true;
    } else {
        err_str = QString("LogicError: Failed to judge into measureStep %1:\n%2").arg(enumToText_MeasureStep(_into_step)).arg(err_str);
        qWarning() << err_str;
        succ_into = false;
    }

    //
#if OS_TYPE == 2
    if (!succ_into && CGlobal::isDebugMode) {
        getWinManage()->showSuspensionPrompt(QString("Failed to enter step %1: ")
                                             .arg(enumToText_MeasureStep(_into_step)) + err_str, (CGlobal::isDebugMode ? -1 : 0));
    }
#endif

    //
    return succ_into;
}

bool WinMeasure::judgeIntoMeasureStep(const enMeasureStep _into_step, bool &_is_err, QString &_err_str)        // TODO: 逻辑完善：防止重复进入、等
{
    //
    bool can_into = true;
    _err_str.clear();
    _is_err = false;

    //
    if (_into_step < measureStep_Min || _into_step > measureStep_Max) {
        _err_str = QString("measureStep %1 is valid!").arg(enumToText_MeasureStep(_into_step));
        _is_err = true;
        return false;
    }

    // 重复进入的检查
    if (_into_step == getMeasureStep()) {
        //
        bool is_allow_repeat = true;

        // 自动转灯时，不可重复进入转灯步骤
        if (m_isAutoTurnLamp && measureStep_Collect == getMeasureStep()) {
            is_allow_repeat = false;
        }

        //
        if (!is_allow_repeat) {
            _err_str = "repeated entered";
            _is_err = false;
            return false;
        }
    }

    //
    switch (_into_step) {
    case measureStep_Ready:

        //
        break;
    case measureStep_Collect:
        //if (measureStep_Ready != getMeasureStep()) {
        //    _err_str = ;
        //    _is_err = true;
        //    can_into = false;
        //}

        if (!m_exposureAdjuster->getIsFinished() && !CGlobal::isDebugMode) {
            _err_str = "Exposure adjusting not finished!";
            _is_err = true;
            can_into = false;
        }

        //
        break;
    case measureStep_Calc:
        if (measureStep_Collect != getMeasureStep()) {
            _err_str = QString("Current step(=%1) is not %2!")
                    .arg(enumToText_MeasureStep(getMeasureStep())).arg(enumToText_MeasureStep(measureStep_Collect));
            _is_err = true;
            can_into = false;
        }

        //
        break;
    case measureStep_CalcFinished:
        //if (measureStep_Calc != getMeasureStep()) {
        //    _err_str = ;
        //    _is_err = true;
        //    can_into = false;
        //}     // TODO: 如果是推值？

        //
        break;
    case measureStep_MeasureFinished:
        //if (measureStep_CalcFinished != getMeasureStep()) {
        //    _err_str = ;
        //    _is_err = true;
        //    can_into = false;
        //}

        //
        break;
    default:
        break;
    }

    //
    return can_into;
}

bool WinMeasure::doIntoMeasureStep(const enMeasureStep _into_step)
{
    qDebug() << QString("%1::%2(): entered, _into_step = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(enumToText_MeasureStep(_into_step));

    // 防止重复进入
    //if (_into_step == m_measureStep) {
    //    qDebug() << QString("%1: repeated entered! exiting ... step = %2").arg(__PRETTY_FUNCTION__)
    //             .arg(enumToText_MeasureStep(_into_step));
    //    return true;
    //}

    //
    if (m_isWaitingForAlgoFinish) {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): isWaitingForAlgoFinish, action skipped!";
        return false;
    }

    //
    bool is_step_done = true;           // 是否已成功执行指定步骤

    //
    switch (_into_step)
    {
    case measureStep_Ready:
    {
        // 每次转灯的初始化
        initMeasure();

        // 确保抓图模块进入瞳孔识别状态
        if (captureStep_PupilDetect != m_captureThread->getCaptureStep()) {
            qWarning() << QString("%1: Currently going into captureStep_PupilDetect, but m_captureThread not in captureStep_PupilDetect?")
                       .arg(__PRETTY_FUNCTION__);
            m_captureThread->setCaptureStep(captureStep_PupilDetect);
        }

        // 重置测距状态
        m_distanceState = distStat_Unknown;
        lastDistTipState = -1;

        //
        break;
    }
    case measureStep_Collect:
    {
        if (measureStep_Collect == getMeasureStep()) {
            //

        }

        if (!m_exposureAdjuster->getIsFinished()) {
            // 已进入正式拍摄但曝光尚未结束时，结束本次曝光统计，
            // 便于定位强制或调试路径下的曝光时序问题。
            m_exposureAdjuster->finishExposureTiming(
                    "entered_formal_early",
                    g_CameraIntf->getExposureTime());
        }

        // 受控模式的数据发送
        if (CGlobal::getIsExternalControl()) {
            std::string stat_str = GRAB_FRAME;      // 发送事件：可转灯
            WinMeasure::setRunStat(stat_str);
            UpLoadThread::sendRunStat(stat_str);        /* 注意：这里发送蓝牙数据须异步发送，否则会耗时过长从而影响整个流程。 */
        }

        // 开始转灯
        if (m_isAutoTurnLamp) {                                     // 自动转灯
            // 执行转灯
            startTurnLamp();
        } else {                                                    // 非自动转灯，先进入转灯状态，等用户按“转灯”按钮时再执行转灯
            if (measureStep_Collect == getMeasureStep()) {          // 若已经进入转灯状态，则判定为是用户按下了“转灯”按钮（用户按下“转灯”前，测量流程已进入转灯，用户按下后再次进入）
                // 执行转灯
                startTurnLamp();
            } else {
                getWinManage()->showSuspensionPrompt(
                            tr("提醒：当前为手动转灯模式！\n请点击“转灯”按钮继续测量。"), -1); // "Reminder: Currently under manual turn lamp mode!\nPlease press \"TurnLamp\" button to continue measuring."
            }
        }

        //
        break;
    }
    case measureStep_Calc:
    {
        //
        if (m_isTurnLampTest) {
            qDebug() << "isTurnLampTest, calc skipped!";
            break;
        }

        // 抓图模块进入瞳孔识别状态（使画面刷新）
        //m_captureThread->setCaptureStep(captureStep_PupilDetect);     // NOTE: 2025-12-24：连续转灯逐帧计算流程下，此步骤已废弃。

        // 受控模式的数据发送
        if (CGlobal::getIsExternalControl()) {
            std::string stat_str = CALCULATING;
            WinMeasure::setRunStat(stat_str);       // 发送事件：正在计算结果
            UpLoadThread::sendRunStat(stat_str);
        }

        // 计算结果
        //std::vector<unsigned char *> *img_set = m_measureCtrl->getImgSet();
        //if (CGlobal::getIsNeedMonthAgeVision(getCurrentAgeRange())) {   // 按月龄估值
        //    showMonthAgeVision(*img_set, getCurrentAgeRange(), m_patient.getBirthDate());
        //} else {
        //    // 发射【计算视力】信号
        //    emit sigCalcVision(img_set, m_patient);
        //}
        // NOTE: (2025-11-18)新算法策略，不需发送结果计算信号

        //
        break;
    }
    case measureStep_CalcFinished:
    {
        /* 测量控制模块的转灯控制及结果回调的处理逻辑(2025-12-24)：
         * 1、测量控制模块自动连续转灯。
         * 2、连续转灯过程中，算法模块可能有结果回调：
         *    若是 calcResultState_Succ：
         *        若 _is_finished 为 true：
         *            结束转灯并显示结果；
         *        否则：
         *            缓存 { stVisionValue, std::vector<stVisionValue>, questionable }；
         *    否则：
         *        弹出错误消息。
         * 3、发生测量超时后：
         *    若有缓存的结果：则显示；
         *    否则：弹出超时错误，并退出测量。
         */
        // NOTE: measureStep_Calc 步骤实际已废弃。
        // NOTE: 注意：连续转灯流程下，只有结果状态为 Succ，且 isFinished == true 时，流程才会进到这里

        // 根据是否成功的状态分别处理
        if (calcResultState_Succ == m_calcResultState) {        // 若是结果计算成功
            // 执行算法策略
            //bool is_finished = CMeasureCtrl::executeAlgoPolicy(m_resultList, m_visionValue, m_visionAbnormal, m_isResultQuestionable);
            //if (is_finished) {
            //    // 结束并显示结果
            //    m_measureCtrl->jumpIntoMeasureStepLatter(measureStep_MeasureFinished);
            //} else {
            //    // 重新转灯
            //    qWarning() << "Re-TurnLamp: required by algo policy!";
            //    m_measureCtrl->jumpIntoMeasureStepLatter(measureStep_Ready);
            //}
            // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以这里不需要。

            //
            if (m_isFinished) {
                // 结束并显示结果
                m_measureCtrl->jumpIntoMeasureStepLatter(measureStep_MeasureFinished);
            } else {
                // 继续转灯
                // NOTE: 已在转灯完成事件里继续转灯
            }
        } else {                                                // 若是结果计算失败
            //
            //bool is_algo_err_handled = false;           // 算法错误是否已处理

            // 错误处理
            //doOnAlgoResultError();

            // 若算法失败，判断是否推值
            //if (CGlobal::maxAlgoFail > 0) {
            //    // 前两年龄段，转灯失败达到设定次数后出统计值
            //    if (currentAgeRange <= 1 && m_countTurnLamp >= CGlobal::maxAlgoFail && CGlobal::isStatisticalEnabled) {
            //        showStatisticalValues(currentAgeRange);     // TODO: 递归调用可能导致状态错乱？改用 measureCtrl 信号？
            //        is_algo_err_handled = true;
            //    }
            //}

            // 若算法错误未处理
            //if (!is_algo_err_handled) {
            //    // 重新转灯
            //    qWarning() << QString("Re-TurnLamp: Algo returned not succ(=%1)!").arg(enumToText_CalcResultState(m_calcResultState));
            //    m_measureCtrl->jumpIntoMeasureStepLatter(measureStep_Ready);
            //}
            // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以这里不需要。
        }

        //
        break;
    }
    case measureStep_MeasureFinished:
    {
        //
        if (calcResultState_Succ == m_calcResultState) {
            // PDF报表预览图（经过直方图均衡化处理的）的临时保存（在结果保存时还要移动到正式路径）  // TODO: 存图逻辑及代码位置的优化，以及变换为与“直线”光路类型一致
            //if (g_isSavePreviewImage ||
            //        (DataTransmiter::IsUploadImage && connMode_Http == DataInterfaceCfg_to_ConnMode(WinDataTrans::getCfg_intfType()))
            //        ) {
            //    //
            //    // TODO: 若是方形视筛箱，图像倒置，或设置相机输出倒置的图像
            //
            //
            //    // TODO: 开启存图后，图像预览看不到了？
            //
            //
            //
            //    // 若是方形视筛箱，存图前变换图像使之与直线光路一致
            //    //if (opticalPathType_Square == g_opticalPathType) {
            //    //    IplImage *img = cvCreateImageHeader(cvSize(g_CameraIntf->getImgWidth(), g_CameraIntf->getImgHeight()), IPL_DEPTH_8U, 1);
            //    //    cvSetData(img, currentFrameData, g_CameraIntf->getImgWidth());
            //    //    cvFlip(img, NULL, -1);      // -1, 旋转180度
            //    //    cvReleaseImageHeader(&img);
            //    //    img = NULL;
            //    //}
            //    m_algoInvoker->savePdfPreviewImg(currentFrameData);
            //}
            // NOTE: 此图已废弃（2025-06-04），改用 12、18 灯号图像处理后代替

            // 保存预览图
            m_measureCtrl->savePreviewImages();

            // 显示结果页面
            showResult(m_resultList);
        } else {
            // 退出测量界面
            goBack();

            // 错误提示
            if (!m_resultErrMsg.isEmpty()) {
                getWinManage()->showMsgWin(m_resultErrMsg);
            } else {
                getWinManage()->showMsgWin(tr("测量失败： 未知错误！"));     // "Measurement failed: unknown error!"
            }
        }

        // 受控模式的数据发送
        if (CGlobal::getIsExternalControl()) {
            std::string msg = (calcResultState_Succ == m_calcResultState ? MEASURE_SUCC : MEASURE_FAIL);  // 发送事件：测量结果（成功|失败）
            WinMeasure::setRunStat(msg);
            UpLoadThread::sendRunStat(msg);
        }

        //
        break;
    }
    default:
    {
        break;
    }
    }

    //
    if (is_step_done) {
        m_measureStep = _into_step;
        qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_measureStep is set to " << enumToText_MeasureStep(m_measureStep);
    }

    //
    measureStatView->updateMeasureStat();

    //
    qDebug() << QString("%1: exiting ... step = %2").arg(__PRETTY_FUNCTION__)
             .arg(enumToText_MeasureStep(_into_step));
    return is_step_done;
}

void WinMeasure::doOnAlgoResultError()
{
    // 错误提示
    if (!m_resultErrMsg.isEmpty()) {
        getWinManage()->showSuspensionPrompt(m_resultErrMsg);
    } else {
        // 根据错误类型处理
        if (calcResultState_GazeOver == m_calcResultState) {          // 固视不准的处理
            countGazeOver++;

            //
            if (countGazeOver >= 3) {
                countGazeOver = 0;

                logDebug("GazeVoer too much, promting user ...", CGlobal::LOG_MEASURE);
                m_resultErrMsg = tr("固视不准，请被测者注视视筛灯"); // "Please ask the subject to gaze at the screener"

                // 语音提示：注视固视灯
                playVoicePrompt(enVoicePrompt::FocusOnLight);
            }
        } else if (calcResultState_Blinked == m_calcResultState) {
            m_resultErrMsg = tr("请保持眼睛睁开");    // "Please keep your eyes open"
        } else if (calcResultState_PupilNotFound == m_calcResultState) {
            m_resultErrMsg = tr("找不到瞳孔");  // "Pupil not found"

            // 语音提示：瞳孔检测失败
            playVoicePrompt(enVoicePrompt::PupilNotDetected);
        } else if (calcResultState_ResultCallbackTimeout == m_calcResultState) {
            if (CGlobal::isDebugMode) {
                m_resultErrMsg = tr("算法结果回调超时！");     // ""
            }
        } else {
            m_resultErrMsg = (tr("结果计算错误") + tr("：") + "%1").arg(enumToText_CalcResultState(m_calcResultState));      // "Result calculation error", ": "
        }

        //
        if (!m_resultErrMsg.isEmpty()) {
            getWinManage()->showSuspensionPrompt(m_resultErrMsg);
        }
    }

    //
    if ((calcResultState_GazeOver != m_calcResultState)) {
        countGazeOver = 0;
    }

}

void WinMeasure::emitPupilDetect(uchar *_img_data, int _img_idx, bool _is_need_calc_expo)
{
    // 新算法，绘帧时需要瞳孔信息，而且通过信号槽调用瞳孔识别函数，如果是异步调用，瞳孔描圆滞后，如果是同步调用，画面不流畅，所以改为在直接调用
    if (CGlobal::getCurrentPupilAlgoVer() > algoVerAll_2019) {
        // 新算法，直接调用瞳孔识别函数（不用信号槽方式）
        if (m_captureThread->countFrameSent() < 2) {
            //
            enSingleDualEyeMode single_dual_eye =  g_SingleDualEye;

            // 若非“直线”光路类型，且是单眼模式，则需变换眼别，确保算法识别的眼侧是实际需要测试的那一只
            if (opticalPathType_General != g_opticalPathType && singleDualEyeMode_Both != g_SingleDualEye) {
                if (opticalPathType_Square == g_opticalPathType) {          // 方形视筛箱：单眼模式调用识别瞳孔时，【需检眼别】参数换为另一只即可，不翻转图像，减少运算量
                    if (singleDualEyeMode_Right == g_SingleDualEye) {
                        single_dual_eye = singleDualEyeMode_Left;
                    } else if (singleDualEyeMode_Left == g_SingleDualEye) {
                        single_dual_eye = singleDualEyeMode_Right;
                    }
                } else if (opticalPathType_LShape == g_opticalPathType) {   // L形视筛箱：单眼模式调用识别瞳孔时，【需检眼别】参数换为另一只即可，不翻转图像，减少运算量
                    if (singleDualEyeMode_Right == g_SingleDualEye) {
                        single_dual_eye = singleDualEyeMode_Left;
                    } else if (singleDualEyeMode_Left == g_SingleDualEye) {
                        single_dual_eye = singleDualEyeMode_Right;
                    }
                }
            }

            //
            stPupilInfo pupil_info_r, pupil_info_l;
            //memset(&pupil_info_r, 0, sizeof(stPupilInfo));
            //memset(&pupil_info_l, 0, sizeof(stPupilInfo));

            bool succ = m_algoInvoker->detectPupil(_img_data, _img_idx, (enAgeRange)getCurrentAgeRange(), pupil_info_r, pupil_info_l, false, single_dual_eye);
            int avg = -1;
            bool over_expo = false;
            if (succ) {
                // 计算曝光时间
                if (_is_need_calc_expo) {
                    int avg_param = -1;
                    bool over_expo_param = false;
                    bool is_calc_succ = m_algoInvoker->calcExposure(_img_data, _img_idx, pupil_info_r, pupil_info_l, avg_param, over_expo_param, single_dual_eye);

                    //记录平均灰度
                    if (CGlobal::isDebugMode) {
                        m_patient.Comment2=QString::number(avg_param);
                    }

                    if (is_calc_succ) {
                        avg = avg_param;
                        over_expo = over_expo_param;
                    } else {
                        logWarning("WinMeasure::emitPupilDetect(): calcExposure() failed!", CGlobal::LOG_CAPTURE);

                        // 若计算曝光时间失败连续失败若干次，悬浮提示
                        // TODO:

                    }
                }
            }

            slot_algoInvoker_PupilDetectionResult(_img_data, _img_idx, succ, pupil_info_r, pupil_info_l, avg, over_expo);
        }
    } else {
        // 2019 版算法，通过信号槽方式调用瞳孔识别函数
        if (countPupilDetectSent < 2)
        {
            countPupilDetectSent++;
            emit sigPupilDetect(_img_data, _img_idx, (enAgeRange)getCurrentAgeRange(), _is_need_calc_expo);
        }
    }

#ifdef TEST_MODE
    {
        // 调试代码
        bool is_save_img = false;
        if (is_save_img) {
            cv::Mat mat(cv::Size(IMG_WIDTH, IMG_HEIGHT), CV_8UC1, _img_data);
            QString file_path = QString("/root/debug") + QDir::separator() + QString("CaptureFrame_%1.bmp").arg(_img_idx);
            cv::imwrite(file_path.toStdString(), mat);
        }
    }
#endif

}

//
void WinMeasure::on_ckbDistDetectLight_clicked(bool checked)
{
    /* 设置是否能在图像中看到测距光：
     * 1、只有需要外部触发的光测距模块，才能在相机中看到测距光。
     * 2、须强制关闭帧同步。
     */

    //
    isForceNotFrameSync = checked;
    m_captureThread->setIsSyncFrame(m_distanceDetect->getIsOuterTrigger() && !isForceNotFrameSync);
}

//
void WinMeasure::on_btnCameraRestartCnt_clicked()
{
    appSetting::setValue("camera/cameraRestartCnt", 0);
    appSetting::setValue("camera/turnLampFrameListErrorCnt", 0);
    gCameraRestartCnt = 0;
    gTurnLampFrameListErrorCnt = 0;
    ui->btnCameraRestartCnt->setText("重启|错帧:00|00");
}

void WinMeasure::setLedLevel(int _delay_ms, bool _is_force)
{
    logDebug("WinMeasure::setLedLevel() into ...", CGlobal::LOG_CAPTURE);

    //
    if (!m_ledSetTimer) {
        m_ledSetTimer = new QTimer();
        QObject::connect(m_ledSetTimer, &QTimer::timeout, [this, _is_force]() {
            this->doSetLedLevel(_is_force);
            this->isLedSettingDelaying = false;
        });
    }

    //
    if (isLedSetted && !_is_force) {
        logDebug("WinMeasure::setLedLevel(): is setted", CGlobal::LOG_CAPTURE);
        return;
    }

    // 覆盖上次的定时设置
    if (m_ledSetTimer->isActive()) {
        logDebug("WinMeasure::setLedLevel(): is setting", CGlobal::LOG_CAPTURE);
        m_ledSetTimer->stop();
    }

    //
    isLedSettingDelaying = true;
    m_ledSetTimer->setSingleShot(true);
    m_ledSetTimer->start(_delay_ms);
}

void WinMeasure::doSetLedLevel(bool _is_force)
{
    logDebug("WinMeasure::doSetLedLevel(): into ...", CGlobal::LOG_CAPTURE);

    do {
        //
        if (!CGlobal::isSetLedLevel) {
            break;
        }

        if (isLedSetted && !_is_force) {
            break;
        }

        // 灯板上电     /* 不管当前是否已上电，都可以发送上电指令 */
        {
            //Util::waitMs(20);         /* 这里加延时是为了避免连续发送指令导致无效，但是串口已加入了延时机制，所以这里不再需要。 */
            setLampPwrOpened(true);
            //Util::waitMs(20);
        }

        //
        serialBaseBoard->sendSetLedLevel(CGlobal::ledLevelMiddle, CGlobal::ledLevelEccentric);
        //Util::waitMs(20);

        sendTurnLampCmd(3);             /* 设置电流参数后，要转一次灯才生效 */
        //Util::waitMs(300);

        // 灯板断电     /* 若当前非测量状态，则关闭灯板 */
        if (!this->isVisible())
        {
            setLampPwrOpened(false);        // TODO: 前面通过信号槽，这里直接调用，不可确保先后顺序如预期？
            //Util::waitMs(20);
        }

        logDebug("WinMeasure::doSetLedLevel(): executed", CGlobal::LOG_CAPTURE);

        //
        QTimer::singleShot(1000, this, []() {
            // 清空帧缓存
            int count_cleared = 0;
            g_CameraIntf->clearFrameBuffer(0, count_cleared);
            logDebug(QString::asprintf("WinMeasure::doSetLedLevel(): clear %d frames.", count_cleared), CGlobal::LOG_CAPTURE);
        });

        //
        isLedSetted = true;
    } while (false);
}

void WinMeasure::updateView_btnMusic()
{
    /* 音乐/彩灯按钮图标逻辑：只要有音乐功能，都显示音乐图标，否则显示彩灯图标。 */
    if (getMusicStateCfg()) {
        if (themeType_Black == getSysThemeType()) {
            ui->btnMusic->setIcon(QIcon(CGlobal::getIsMusicEnabled() ? ":/resource/black_theme/music_on.png" : ":/resource/black_theme/colored_lamp_on.png"));
        } else {
            //ui->btnMusic->setIcon(QIcon(":/resource/music_on.png"));
        }
    } else {
        if (themeType_Black == getSysThemeType()) {
            ui->btnMusic->setIcon(QIcon(CGlobal::getIsMusicEnabled() ? ":/resource/black_theme/music_off.png" : ":/resource/black_theme/colored_lamp_off.png"));
        } else {
            //ui->btnMusic->setIcon(QIcon(":/resource/music_off.png"));
        }
    }
}

// 继续上一次年龄段的测量（按下物理按键后）
void WinMeasure::continueMeasuring()
{
    WinMeasure::setOperationMode(operationMode_NormalMeasure);
//        emit sendSIGNAL(sysSignal_06);
    getWinManage()->openClinicMeasure(WinMeasure::getCurrentAgeRange());
}

qint64 WinMeasure::elapsedMeasure() const
{
    return (m_elapsedMeasure.isValid() ? m_elapsedMeasure.elapsed() : -1);
}

void WinMeasure::on_btnOpticalType_clicked()
{
    // 暂停测量
    stopMeasure();

    // 弹出选择框
    WidgetOpticalTypeOptions *dialog = WidgetOpticalTypeOptions::instance();
    dialog->setParent(getWinBase());
    Util::Ui::centerWidget(dialog);
    dialog->setModal(true);
    dialog->setWindowModality(Qt::WindowModality::WindowModal);
    this->setEnabled(false);            // NOTE: 因对话框未能屏蔽对话框区域外的部件的点击事件，暂时禁用本窗口     // TODO: 点击事件遮盖未有效原因分析及解决？
    dialog->exec();
    this->setEnabled(true);
    dialog->setParent(nullptr);

    enOpticalPathType optical_type = dialog->selectedOpticalPathType();

    // 应用新的【光路类型】属性
    doOnOpticalPathTypeChanged(optical_type);

    // 继续测量
    startMeasure();
}

void WinMeasure::on_btnSingleDualEye_clicked()
{
    // 暂停测量         // NOTE: 注意：弹出模态对话框之前，须先暂停测量！   // TODO: 原因？优化？
    stopMeasure();

    //
    enSingleDualEyeMode curr_single_dual_eye = g_SingleDualEye;
    curr_single_dual_eye = (enSingleDualEyeMode)((int)curr_single_dual_eye + 1);
    if (curr_single_dual_eye > singleDualEyeMode_Max) {
        curr_single_dual_eye = singleDualEyeMode_Min;
    }

    // 应用新的【单双眼】属性
    doOnSingleDualEyeChanged(curr_single_dual_eye);

    // 继续测量
    startMeasure();
}

void WinMeasure::on_btnHighDiopter_clicked()
{
    //
    g_isHmMode = !g_isHmMode;

    // 配置保存
    // NOTE: 不永久保存

    //
    updateView_btnHighDiopter();
}

///=====================================================================================================================
/// class CFrameDrawer

//
CFrameDrawer::CFrameDrawer(QWidget *_parent, int _left, int _top, int _width, int _height) : QObject(_parent)
{
    computeImgBuf = (unsigned char *)malloc(IMG_WIDTH * IMG_HEIGHT);

    imgDrawingRaw = cvCreateImage(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 3);
    if (IMG_WIDTH == _width && IMG_HEIGHT == _height) {
        imgDrawingView = imgDrawingRaw;
    } else {
        imgDrawingView = cvCreateImage(cvSize(_width, _height), IPL_DEPTH_8U, 3);
    }

    imgDataMagnifiedR = (unsigned char *)malloc(imgDrawingView->width * imgDrawingView->height / 2 * 3);    /* 图像最大宽度是 UI 中图像的一半 */
    imgDataMagnifiedL = (unsigned char *)malloc(imgDrawingView->width * imgDrawingView->height / 2 * 3);

    //
    gvFrame = new QGraphicsView(_parent);
    gvFrame->setGeometry(_left, _top, _width, _height);
    gvFrame->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gvFrame->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gvFrame->setEnabled(false);
    gvFrame->setFrameShape(QFrame::NoFrame);

    m_pGraphScene = new QGraphicsScene(this);
    gvFrame->setScene(m_pGraphScene);

    //for(int i = 0; i < 256; i++)
    //   grayColourTable.append(qRgb(i, i, i));

}

CFrameDrawer::~CFrameDrawer()
{
    if (imgDrawingView != imgDrawingRaw) {
        cvReleaseImage(&imgDrawingView);
        imgDrawingView = Q_NULLPTR;
    }
    cvReleaseImage(&imgDrawingRaw);
    imgDrawingRaw = Q_NULLPTR;
}

void CFrameDrawer::setIsQtDraw(bool _is_qt_draw)
{
    gvFrame->setVisible(_is_qt_draw);
}

void CFrameDrawer::drawPupilCircle()
{
    if (isPupilInfoValid) {
        drawPupilCircle(pupilInfoR, pupilInfoL);
    }
}

void CFrameDrawer::drawPupilCircle(stPupilInfo _pupil_info_r, stPupilInfo _pupil_info_l)
{
    const int line_width = 2;
    const CvScalar color = cvScalar(0, 180, 0);
    //const CvScalar color = CV_RGB(0xE0, 0xE0, 0);

    //
    int radius = max(_pupil_info_r.radius, _pupil_info_l.radius) + 4;

    bool is_right_valid = (_pupil_info_r.center.x > 0.1 && _pupil_info_r.center.y > 0.1 && _pupil_info_r.radius > 0.1);
    if (is_right_valid) {
        cvCircle(imgDrawingRaw, cvPoint(_pupil_info_r.center.x, _pupil_info_r.center.y), radius, color, line_width);
    }

    bool is_left_valid = (_pupil_info_l.center.x > 0.1 && _pupil_info_l.center.y > 0.1 && _pupil_info_l.radius > 0.1);
    if (is_left_valid) {
        cvCircle(imgDrawingRaw, cvPoint(_pupil_info_l.center.x, _pupil_info_l.center.y), radius, color, line_width);
    }
}

void CFrameDrawer::updateFrame(bool _is_draw_circle, bool _is_qt_draw, int _flip_mode)
{
    //
    countFrame++;

    //logDebug(QString::asprintf("CFrameDrawer::slotFrameCaptured() into, WinMeasure::s_isOpened=%s", Util::bool2str(WinMeasure::s_isOpened)), CGlobal::LOG_MEASURE);

    // 描圆
    if (_is_draw_circle) {
        drawPupilCircle();
    }

    // 缩放图像以适应屏幕
    if (imgDrawingView != imgDrawingRaw) {
        cvResize(imgDrawingRaw, imgDrawingView);
    }

    // 瞳孔区域放大
    if (CGlobal::isMagnifyPupilImg && isPupilInfoValid) {
        const double MAGNIFI_TIMES = 2;

        int w_org = (pupilInfoR.radius > pupilInfoL.radius ? pupilInfoR.radius : pupilInfoL.radius) * 1.5;
        int w = w_org * MAGNIFI_TIMES;

        if (w > IMG_WIDTH / 2)
            w = IMG_WIDTH / 2;
        if (w > IMG_HEIGHT / 2)
            w = IMG_HEIGHT / 2;

        IplImage *img_r = cvCreateImageHeader(cvSize(w, w), IPL_DEPTH_8U, 3);
        IplImage *img_l = cvCreateImageHeader(cvSize(w, w), IPL_DEPTH_8U, 3);

        cvSetData(img_r, imgDataMagnifiedR, CV_AUTOSTEP);
        cvSetData(img_l, imgDataMagnifiedL, CV_AUTOSTEP);

        double img_ratio = (double)imgDrawingView->width / imgDrawingRaw->width;        // 屏幕图像和相机图像的尺寸缩放比例
        CvPoint center_r = cvPoint(pupilInfoR.center.x * img_ratio, pupilInfoR.center.y * img_ratio);
        CvPoint center_l = cvPoint(pupilInfoL.center.x * img_ratio, pupilInfoL.center.y * img_ratio);

        CvRect rect_r_org = cvRect(center_r.x - w_org / 2, center_r.y - w_org / 2, w_org, w_org);
        CvRect rect_l_org = cvRect(center_l.x - w_org / 2, center_l.y - w_org / 2, w_org, w_org);

        cvSetImageROI(imgDrawingView, rect_r_org);
        cvResize(imgDrawingView, img_r);

        cvSetImageROI(imgDrawingView, rect_l_org);
        cvResize(imgDrawingView, img_l);

        CvRect rect_r = cvRect(center_r.x - w / 2, center_r.y - w / 2, w, w);
        CvRect rect_l = cvRect(center_l.x - w / 2, center_l.y - w / 2, w, w);

        if (rect_r.x < 0)
            rect_r.x = 0;
        else if (rect_r.x + rect_r.width > imgDrawingView->width - 1)
            rect_r.x = imgDrawingView->width - rect_r.width - 1;
        if (rect_l.x < 0)
            rect_l.x = 0;
        else if (rect_l.x + rect_l.width > imgDrawingView->width - 1)
            rect_l.x = imgDrawingView->width - rect_l.width - 1;

        if (rect_r.y < 0)
            rect_r.y = 0;
        else if (rect_r.y + rect_r.height > imgDrawingView->height - 1)
            rect_r.y = imgDrawingView->height - rect_r.height - 1;
        if (rect_l.y < 0)
            rect_l.y = 0;
        else if (rect_l.y + rect_l.height > imgDrawingView->height - 1)
            rect_l.y = imgDrawingView->height - rect_l.height - 1;

        cvSetImageROI(imgDrawingView, rect_r);
        cvCopy(img_r, imgDrawingView);

        cvSetImageROI(imgDrawingView, rect_l);
        cvCopy(img_l, imgDrawingView);

        cvResetImageROI(imgDrawingView);

        cvReleaseImageHeader(&img_r);
        cvReleaseImageHeader(&img_l);
    }

    // 图像翻转处理
    if (_flip_mode >= -1 && _flip_mode <= 1) {
        // 翻转图像
        //cv::Mat tmp_mat(cv::Size(g_CameraIntf->getImgWidth(), g_CameraIntf->getImgHeight()), CV_8UC1, _img_data);
        //cv::flip(tmp_mat, tmp_mat, _flip_mode);

        cvFlip(imgDrawingView, NULL, _flip_mode);

        // NOTE: 瞳孔坐标不需要做同步的翻转，因为在图像翻转之前，前面的“描圆”步骤已经将坐标绘制到图像
    }

    // 刷帧
#ifdef SURPORT_FRAME_BUFFER
    static int convert = 0;

    if (!_is_qt_draw)
    {
        try {
            if (convert == 0)
            {
                copyToFrameBuffer(fbp, scrinfo);
                show_framebuffer_0(devfb, scrinfo);
                convert = 1;
            }
            else if (convert == 1)
            {
                copyToFrameBuffer(fbp + one_screensize, scrinfo);
                show_framebuffer_1(devfb, scrinfo);
                convert = 2;
            }
            else if (convert == 2)
            {
                copyToFrameBuffer(fbp + one_screensize * 2, scrinfo);
                show_framebuffer_2(devfb, scrinfo);
                convert = 0;
            }
        } catch (...) {
            qDebug() << "caught exception : errno = " << errno << ", errstr = " << strerror(errno);

            gWinMeasure->goBack();
            qApp->exit(0);
        }

    }
    else
    {
        drawFrame();
    }
#else
    Q_UNUSED(_is_qt_draw)

# if (TEST_MODE && (OS_TYPE == 2))
    static bool is_need_save = false;
    if (is_need_save) {
        int len = SCREEN_WIDTH * SCREEN_HEIGHT * 3;
        char *buff = (char *)malloc(len);
        memset(buff, 255, len);

#  ifdef SURPORT_FRAME_BUFFER
        copyToFrameBuffer(buff, scrinfo);
#  else
        copyToFrameBuffer(buff);
#  endif

        //Util::saveImgDataToImgFile((uchar *)buff, SCREEN_WIDTH, SCREEN_HEIGHT, "/root/debug/img_frame_buffer.jpg", 3);
        //Util::saveImgDataToImgFile((uchar *)imgDrawingView->imageData, imgDrawingView->width, imgDrawingView->height, "/root/debug/img_drawing.jpg", 3);

        Util::saveImgDataToImgFile2((uchar *)buff, SCREEN_WIDTH, SCREEN_HEIGHT, "/root/debug/img_frame_buffer2.jpg", 3);
        Util::saveImgDataToImgFile2((uchar *)imgDrawingView->imageData, imgDrawingView->width, imgDrawingView->height, "/root/debug/img_drawing2.jpg", 3);

        free(buff);
        buff = nullptr;
        is_need_save = false;
    }
# endif

    drawFrame();
#endif

    //
    isPupilInfoValid = false;

    //
    //logDebug("CaptureDraw::slotCaptureFrame() out ");
}

void CFrameDrawer::reset()
{

}

void CFrameDrawer::setCountFrame(unsigned int _count)
{
    countFrame = _count;
}

int CFrameDrawer::getCountFrame()
{
    return countFrame;
}

void CFrameDrawer::setPupilInfo(bool _succ, const stPupilInfo &_pupil_info_r, const stPupilInfo &_pupil_info_l)
{
    // NOTE: 此处的瞳孔信息的眼别，与实际眼别无关。应避免测量模块的眼别逻辑扩散到这里。

    //
    isPupilInfoValid = _succ;
    if (_succ) {
        pupilInfoR = _pupil_info_r;
        pupilInfoL = _pupil_info_l;
    }
}

QGraphicsView *CFrameDrawer::getViewer()
{
    return gvFrame;
}

QRect CFrameDrawer::getImgViewRect()
{
    return gvFrame->geometry();
}

#if (1 == OS_TYPE || 2 == OS_TYPE)
  #ifdef SURPORT_FRAME_BUFFER
void CFrameDrawer::copyToFrameBuffer(char * const fbp, struct fb_var_screeninfo &scrinfo)    // TODO: 用 memcpy() 逐行拷贝效率会否更高？
  #else
void CFrameDrawer::copyToFrameBuffer(char * const fbp)
  #endif
{
  #ifdef SURPORT_FRAME_BUFFER
    assert(SCREEN_WIDTH == scrinfo.xres);
    assert(SCREEN_HEIGHT == scrinfo.yres);
  #endif

    // 得到图像数据首指针及图像通道数
    uchar *src_ptr = (unsigned char *)imgDrawingView->imageData;
    int src_channels = 3;
    if (measureStep_Collect == g_WinMeasure->getMeasureStep()) {        // 转灯时，imgDrawing 是无效的，须用原图      // TODO: 好像没有必要停止更新 imgDrawing ？
        src_ptr = currentFrameData;
        src_channels = 1;
    }

    // 缓存的绘图区域
    QRect img_rect = getImgViewRect();

    int dest_row_begin  = img_rect.top();                               // 首行
    int dest_col_begin  = (SCREEN_WIDTH - img_rect.width()) / 2;        // 首列

    int row_count       = img_rect.height();
    int col_count       = img_rect.width();

    //
    int dest_curr_row_head;     // 本行首位置索引（以左上角地址为零计算）
    int dest_curr_pos;          // 当前位置索引
    int src_curr_row_head;
    int src_curr_pos;
    for (int src_row = 0; src_row < row_count; src_row++) {     // 逐行拷贝
        dest_curr_row_head = (dest_row_begin + src_row) * SCREEN_WIDTH * 3;
        src_curr_row_head = src_row * imgDrawingView->widthStep * src_channels;

        for (int src_col = 0; src_col < col_count; src_col++) {
            dest_curr_pos = dest_curr_row_head + (dest_col_begin + src_col) * 3;
            src_curr_pos = src_curr_row_head + src_col * src_channels;

            for (int k = 0; k < src_channels; k++) {       // 目标像素各通道赋值
                fbp[dest_curr_pos + k] = src_ptr[src_curr_pos + (3 == src_channels ? k : 0)];
            }
        }
    }

    //qDebug()<<"----------===============src_ptr: "<<src_ptr[0]<<src_ptr[1]<<src_ptr[2]<<src_ptr[3]<<src_ptr[4]<<src_ptr[5]<<src_ptr[6]<<src_ptr[7]<<src_ptr[8]<<src_ptr[9];
}
#endif

void CFrameDrawer::drawFrame()
{
//    static QTime time;
//    static bool is_time_started = false;
//    if (!is_time_started) {
//        is_time_started = true;
//        time.start();
//    }

//    if (time.elapsed() < 80 && countUpdateFrame > 0)    // 限制帧率
//        return;
//    else
//        time.restart();

    // TODO: 改为 QPainter::drawPixmap() 方法刷帧，效率是否更高？

    //if (countUpdateFrame < 5)
    {
        QImage img((unsigned char *)imgDrawingView->imageData, imgDrawingView->width, imgDrawingView->height, QImage::Format_RGB888);
        //img.setColorTable(grayColourTable);

        if (m_pGraphPixmapItem)
        {
            m_pGraphScene->removeItem(m_pGraphPixmapItem);            /* 实测，这个操作会释放内存 */
            delete m_pGraphPixmapItem;
            m_pGraphPixmapItem = NULL;
        }

        QPixmap pixmap = QPixmap::fromImage(img);
        m_pGraphPixmapItem = m_pGraphScene->addPixmap(pixmap);        /* 实测，这个操作会拷贝内存 */

        //m_pGraphScene->setSceneRect(0, 0, img.width(), img.height());

        //ui->gvFrame->viewport()->update();
        //m_pGraphScene->update();
    }
}

void CFrameDrawer::pushImage(uchar *_img_data, bool _is_need_enhance)
{
    static IplImage *img_src = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
    static IplImage *img_dest = cvCreateImage(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
    static CvMat* lut_mat = cvCreateMatHeader(1, 256, CV_8UC1);
    static uchar lut[256];

    //
    static bool is_init = false;
    if (!is_init) {
        static const double GAMMA = 2.9;
        static double gamma = 1.0 / GAMMA;
        for (int i = 0; i < 256; i++) {
            lut[i] = std::round(std::pow((double)i / 255.0, gamma) * 255);
        }
        cvSetData(lut_mat, lut, CV_AUTOSTEP);
        //
        is_init = true;
    }

    //
    if (_is_need_enhance) {
        memcpy(computeImgBuf, _img_data, img_src->imageSize);
        cvSetData(img_src, computeImgBuf, IMG_WIDTH);

        //cvEqualizeHist(img_src, img_dest);
        cvLUT(img_src, img_dest, lut_mat);

        //
        cvCvtColor(img_dest, imgDrawingRaw, CV_GRAY2BGR);
    } else {
        memcpy(img_dest->imageData, _img_data, img_dest->imageSize);

        //
        cvCvtColor(img_dest, imgDrawingRaw, CV_GRAY2BGR);
    }
}

bool CFrameDrawer::detectPupilAndDrawCircle(uchar *_img_data)
{
    //cout<<"before detectPupil----"<<QTime::currentTime().msec()<<endl;
    bool detected = false;

    // ----------------------------图像降维-----------------------------------
    static IplImage *pyrImg = Q_NULLPTR;
    if (!pyrImg) {
        pyrImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);     //创建图像首地址，并不会初始化空间内的数据
        cvSetData(pyrImg, _img_data, IMG_WIDTH);    //  将源图像数据复制到目标图像头
    }

    if(true) //如在转灯抓图时,检测描圆会有些卡,影响抓图
    {
        int w = pyrImg->width;
        int h = pyrImg->height;
        int ratio =  4;     // 调整为原来1/ratio
        int sw = w / ratio;
        int sh = h / ratio;
        IplImage *SampleImg = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);   //创建图像首地址，并分配存储空间
        IplImage *destbwImg = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);
        cvSetZero(destbwImg);

        uchar *data = (uchar *)SampleImg->imageData;
        for (int i = 0; i < sh; i++)
        {
            for (int j = 0; j < sw; j++)
            {
                *(data + i * SampleImg->widthStep + j) = ((uchar *)(pyrImg->imageData + i * ratio * pyrImg->widthStep))[j * ratio];
            }
        }

        IplImage *bwImg = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);
        cvSetZero(bwImg);

        cv::Mat tImg0Mat;
        cv::medianBlur(cv::cvarrToMat(SampleImg), tImg0Mat, 1);
//#ifdef M_OPENCV_3_2
        //IplImage tImg0 = IplImage(tImg0Mat);        /* opencv 3.2 */
//#else
        IplImage tImg0 = cvIplImage(tImg0Mat);      /* opencv 3.4 */
//#endif
        bwImg->imageData = tImg0.imageData;

        int HistogramBins = 256;
        float HistogramRange1[2] = {0, 255};
        float *HistogramRange[1] = {&HistogramRange1[0]};
        CvHistogram *Histogram1 = cvCreateHist(1, &HistogramBins, CV_HIST_ARRAY, HistogramRange);   //创建直方图
        cvCalcHist(&bwImg, Histogram1); //计算图像数组直方图

        float  MaxValueH;
        double arc;
        //                 double ThresHc;
        double ThresHc = 0;
        int MaxLocationh;

        cvSetReal1D(Histogram1->bins, 0, 0);    //改变/设置 一个数组元素的值
        //qDebug()<< num <<" Histogram1 before= "<<num;
        cvGetMinMaxHistValue(Histogram1, 0, &MaxValueH, 0, &MaxLocationh);      //获取直方图的最小和最大值及标号

        for (int i = 0; i < HistogramBins; i++)
        {
            arc = cvGetReal1D(Histogram1->bins, i);     //获得单通道（灰度图）的某一点像素值。
            //                     //qDebug()<< num <<" arc = "<<arc;
            //        arc = cvGetReal1D(Histogram1 ,i);
            if ( arc < 2 && i > MaxLocationh)
            {
                ThresHc = i;
                break;
            }
        }

        cvThreshold(bwImg, destbwImg, ThresHc, 255, CV_THRESH_BINARY);      //对灰度图像进行阈值操作得到二值图像

        CvSeq *contours = NULL;
        CvMemStorage *mem_storage = cvCreateMemStorage(0);
        int contours_num = cvFindContours(destbwImg, mem_storage, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, cvPoint(0, 0)); //从二值图像中检索轮廓，并返回检测到的轮廓的个数。

        int minDTH = 2;
        int minDTW = 4;
        int maxDTH = 20;

        pupil_Map.clear();
        if(contours_num > 0)
        {
            for (; contours; contours = contours->h_next)
            {
                CvRect rect = cvBoundingRect(contours, 0);  //检索矩形边界轮廓,返回矩形左上角坐标和宽高

                int Tmp_H = rect.height;
                int Tmp_W = rect.width;

                if (Tmp_H >= minDTH && Tmp_W >= minDTW && Tmp_H <= maxDTH && Tmp_W <= maxDTH/*&& area1 > minAreaTH && area1 < maxAreaTH*/)
                {
                    CvPoint3D32f pt;
                    float R = (float)(((rect.height + rect.width) / 4) * 4);
                    pt.x = rect.x * 4 + R;
                    pt.y = rect.y * 4 + R;
                    pt.z = R * 1.2;         //圆半径
                    float Fix = R - fabs(rect.y * 4 - 240) / 7 - fabs(fabs(rect.x * 4 + (pt.z / 2) - 400) - 200) / 5;
                    if(fabs(rect.x * 4 + (pt.z / 2) - 400) < 80)
                        continue;
                    pupil_Map.insert(Fix, pt);
                }
            }

        }

        int max;
        if(singleDualEyeMode_Both == g_SingleDualEye)
            max = 2;
        else
            max = 1;

        //
        detected = (pupil_Map.size() >= 2);    // 是否识别瞳孔

        //
        while(pupil_Map.size() > 0)
        {
            if(max == 0)
                break;

            CvPoint3D32f pt = pupil_Map.last();
            int R = pt.z;
            pupil_Map.remove(pupil_Map.lastKey());

            if(singleDualEyeMode_Left == g_SingleDualEye)
            {
                if(pt.x < 410)
                    continue;
            }
            else if(singleDualEyeMode_Right == g_SingleDualEye)
            {
                if(pt.x + R * 2 > 390)
                    continue;
            }
            cvCircle(imgDrawingRaw, cvPoint(pt.x, pt.y), R, cvScalar(0, 180, 0), 3); //画圆函数
            max--;
        }

        //for(int i=0;i<pupil_Vec.size();i++){
        //    CvRect rect = pupil_Vec.at(i);
        //    CvPoint RealRect;
        //    int R = ((rect.height + rect.width)/4)*4;
        //    RealRect.x = rect.x*4 + R;
        //    RealRect.y = rect.y*4 + R;
        //    cvCircle(drawImg,RealRect,R,CV_RGB(0,180,0),5); //画圆函数
        //}

        //emit sendPyr(*drawImg);


        tImg0Mat.release();
        cvReleaseImage(&SampleImg);
        cvClearMemStorage(mem_storage);
        cvReleaseMemStorage(&mem_storage);
        cvReleaseImage(&bwImg);
        cvReleaseImage(&destbwImg);
        cvReleaseHist(&Histogram1);
    }

    //memcpy(_img_data,(unsigned char*)drawImg->imageData,drawImg->imageSize);
    //cvReleaseImageHeader(&pyrImg);

    return detected;
    //cout<<"After detectPupil----"<<QTime::currentTime().msec()<<endl;
}

void WinMeasure::on_btnDebugPanelExpand_clicked()
{
    setDebugPanelExpanded(!isDebugPanelVisible());
}
