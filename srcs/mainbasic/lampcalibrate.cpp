#include "lampcalibrate.h"
#include "ui_lampcalibrate.h"

#include <QMouseEvent>
#include <QDebug>
#include <QPainter>
#include <QTime>
#include <QSerialPort>
#include <QMessageBox>
#include <QDir>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
//#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "CameraIntf.h"

#include "global.h"

#ifndef SCREENER_TEST
# include "windowsmanager.h"
# include "camerainit.h"
#else
# include "screener_test.h"
#endif

//
//// 临时设置电流等级到内存（指令头部，后再加 23 bytes 表示 0-22 号灯珠的电流等级）      // TODO: STM32F103RB 的 Flash 擦写寿命 10000 次，保存 20 年，所以这条指令可能不是必须的？
//char CMD_SET_LED_LEVEL[27]       = {0x55, 0x7A, 0x19, (char)0x0D};
// 永久保存电流等级到 Flash（指令头部，同上）
char CMD_SAVE_FLASH[27]     = {0x55, 0x7A, 0x19, (char)0x0E};
// 读取电流等级（指令头部，同上）
char CMD_READ_CFG[5]        = {0x55, 0x7A, 0x03, (char)0x0F, 0x00};     // 55 7A 03 0F 00
// 读取电流等级的应答（指令头部，同上）
char CMD_READ_CFG_REP[27]   = {0x55, 0x7A, 0x19, (char)0x8F};

//
LampCalibrate::LampCalibrate(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::LampCalibrate)
{
    ui->setupUi(this);

    // 变量初始化
    isUseCameraApiProcess = false;          // 是否使用相机的图像处理 API 函数

    // 构造数据
    createVecLedPositions();
    for (int i = 0; i < vecLedPositions.size(); i++) {
        QWidget *w = vecLedPositions[i];
        //w->setAutoFillBackground(false);
        w->setVisible(false);
    }
    createVecLedInfos();

    // 相机对象
    cameraThread = new CCapture();
    QObject::connect(cameraThread, &CCapture::sigGetImg, this, &LampCalibrate::slot_cameraThread_GetImg, Qt::QueuedConnection);
    QObject::connect(cameraThread, &CCapture::sigRunEnd, this, &LampCalibrate::slot_cameraThread_RunEnd, Qt::QueuedConnection);

    // 灯板图像载入
    imgLedBoard = new QImage(":/resource/lamp-board.png");

    // 电流等级值编辑框     // TODO: 改为下拉选框，不可随意输入任意值

/* 电流等级
0
7
9
11
12
14
16
18
19
20
21
22
23
24
*/

    spxCurrent = new mySpinBox((QWidget *)ui->frmSetCurrent);
    spxCurrent->setGeometry(ui->spxCurrent0->geometry());
    spxCurrent->setMinimum(MIN_CURRENT_LEVEL);
    spxCurrent->setMaximum(MAX_CURRENT_LEVEL);
    spxCurrent->setValue(20);

    // 控件缺省状态
    ui->lblLedBoard->raise();
    ui->lblLedBoard->setVisible(false);

    ui->btnAutoConfig->setEnabled(false);
    ui->btnSave->setEnabled(false);

    ui->frmWaiting->setVisible(false);
    ui->frmWaiting->raise();

    ui->spxCurrent0->setVisible(false);

    ui->frmSetCurrent->raise();
    ui->frmSetCurrent->setVisible(false);

    ui->ckbShowImgBright->setChecked(isShowImgBright);
    ui->ckbSaveTurnLampImg->setChecked(isSaveTurnLampImg);
    ui->ckbSaveBrightnessValues->setChecked(isSaveBrightnessValues);
    ui->ckbSaveBrightnessValues->setEnabled(false);

}

LampCalibrate::~LampCalibrate()
{
    if (imgDataTemp) {
        free(imgDataTemp);
        imgDataTemp = Q_NULLPTR;
    }

    //if (edtCurrent) {                 /* 有 parent 的 QObject 已经被 parent 释放，不可再释放。 */
    //    delete edtCurrent;
    //    edtCurrent = Q_NULLPTR;
    //}

    //
    delete ui;
}

void LampCalibrate::showEvent(QShowEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    //

}

void LampCalibrate::hideEvent(QHideEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }


}

bool LampCalibrate::initCamera()
{
    // 设置相机参数
    enCameraStat succ_api = cameraStat_Unknow;
    do {
        // 相机 SDK Stop
        succ_api = g_CameraIntf->stop();
        if (cameraStat_Succ != succ_api) {
            logDebug(QString::asprintf("camera api err: %d!", succ_api));
            break;
        }

        // 曝光时间
        int exposure_time = 4000;
        succ_api = g_CameraIntf->setExposureTime(&exposure_time);
        if (cameraStat_Succ != succ_api) {
            logDebug(QString::asprintf("camera api err: %d!", succ_api));
            break;
        };

        // 触发模式
        succ_api = g_CameraIntf->setTriggerMode(cameraTriggerMode_Hard);
        if (cameraStat_Succ != succ_api) {
            logDebug(QString::asprintf("camera api err: %d!", succ_api));
            break;
        }

        // 恢复默认设置，设置手动曝光、模拟增益
        // TODO:


        // 相机 SDK Play
        succ_api = g_CameraIntf->play();
        if (cameraStat_Succ != succ_api) {
            logDebug(QString::asprintf("camera api err: %d!", succ_api));
            break;
        }

        // 先自动触发并抓图若干帧（可减少丢帧？）
        // TODO:


    } while (false);

    //
    return succ_api;
}

// 初始化标识灯珠位置的 QWidget 控件集合
void LampCalibrate::createVecLedPositions()
{
    vecLedPositions.push_back(ui->wgt_00);
    vecLedPositions.push_back(ui->wgt_01);
    vecLedPositions.push_back(ui->wgt_02);
    vecLedPositions.push_back(ui->wgt_03);
    vecLedPositions.push_back(ui->wgt_04);
    vecLedPositions.push_back(ui->wgt_05);
    vecLedPositions.push_back(ui->wgt_06);
    vecLedPositions.push_back(ui->wgt_07);
    vecLedPositions.push_back(ui->wgt_08);
    vecLedPositions.push_back(ui->wgt_09);
    vecLedPositions.push_back(ui->wgt_10);
    vecLedPositions.push_back(ui->wgt_11);
    vecLedPositions.push_back(ui->wgt_12);
    vecLedPositions.push_back(ui->wgt_13);
    vecLedPositions.push_back(ui->wgt_14);
    vecLedPositions.push_back(ui->wgt_15);
    vecLedPositions.push_back(ui->wgt_16);
    vecLedPositions.push_back(ui->wgt_17);
    vecLedPositions.push_back(ui->wgt_18);
    vecLedPositions.push_back(ui->wgt_19);
    vecLedPositions.push_back(ui->wgt_20);
    vecLedPositions.push_back(ui->wgt_21);
    vecLedPositions.push_back(ui->wgt_22);
}

void LampCalibrate::createVecLedInfos()
{
    for (int i = 0; i < vecLedPositions.size(); i++) {
        QWidget *w = vecLedPositions[i];
        stLedInfo led_info;
        //memset(&led_info, 0, sizeof(stLedInfo));
        led_info.x = w->x();
        led_info.y = w->y();
        led_info.w = w->width();
        led_info.brightnessImg = -1;
        led_info.brightnessLed = -1;
        led_info.currentLevel = -1;

        vecLedInfos.push_back(led_info);
    }
}

void LampCalibrate::mouseReleaseEvent(QMouseEvent *_event)
{
    int idx = -1;
    for (int i = 0; i < 23; i++) {
        if (_event->x() >= vecLedInfos[i].x && _event->x() <= vecLedInfos[i].x + vecLedInfos[i].w &&
                _event->y() >= vecLedInfos[i].y && _event->y() <= vecLedInfos[i].y + vecLedInfos[i].w
                ) {
            idx = i;
            break;
        }
    }

    if (idx >= 0 && vecLedInfos[idx].currentLevel > 0) {
        qDebug() << "led " << idx << " clicked";

        // 显示电流等级设置控件
        showEdit(idx);
    } else {
        showEdit(-1);
    }
}

void LampCalibrate::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    // 绘灯板图
    painter.drawImage(0, 0, *imgLedBoard);

    // 绘灯号
    painter.setPen(Qt::black);
    //painter.setFont(QFont(this->font().family(), 12));
    for (int i = 0; i < vecLedInfos.size(); i++) {
        int x = vecLedInfos[i].x + 7;
        int y = vecLedInfos[i].y + vecLedInfos[i].w - 10;

        QString num_str = QString::asprintf("%02d", i);
        painter.drawText(x, y, num_str);
    }

    if (isShowImgBright) {
        // 绘 Img 亮度
        painter.setPen(Qt::darkYellow);
        //painter.setFont(QFont(this->font().family(), 12));
        for (int i = 0; i < vecLedInfos.size(); i++) {
            int x = vecLedInfos[i].x;
            int y = vecLedInfos[i].y - 4;

            QString brightness_str = (vecLedInfos[i].brightnessImg >= 0 ? QString::asprintf("%.2f", vecLedInfos[i].brightnessImg) : "");
            painter.drawText(x, y, brightness_str);
        }
    }

    // 绘 Led 亮度
    painter.setPen(Qt::red);
    //painter.setFont(QFont(this->font().family(), 12));
    for (int i = 0; i < vecLedInfos.size(); i++) {
        int x = vecLedInfos[i].x;
        int y = vecLedInfos[i].y + vecLedInfos[i].w + 13;

        QString brightness_str = (vecLedInfos[i].brightnessLed >= 0 ? QString::asprintf("%.2f", vecLedInfos[i].brightnessLed) : "");
        painter.drawText(x, y, brightness_str);
    }

    // 绘电流等级
    painter.setPen(Qt::blue);
    //painter.setFont(QFont(this->font().family(), 12));
    for (int i = 0; i < vecLedInfos.size(); i++) {
        int x = vecLedInfos[i].x + vecLedInfos[i].w + 3;
        int y = vecLedInfos[i].y + vecLedInfos[i].w - 10;

        QString current_level_str = (vecLedInfos[i].currentLevel >= 0 ? QString::asprintf("%02d", vecLedInfos[i].currentLevel) : "");
        painter.drawText(x, y, current_level_str);
    }

}

// 测量亮度
void LampCalibrate::on_btnMeasureBrightness_clicked()
{
    // 创建存图目录
    currImgsDir = QString("/media/photo/lamp_calibrate/") + QDateTime::currentDateTime().toString("yyyyMMdd_mmss");
    QDir dir(currImgsDir);
    dir.mkpath(dir.path());

    // 显示等待界面
    ui->frmWaiting->setGeometry(
                ui->lblLedBoard->width() + 10, 10,
                this->width() - ui->lblLedBoard->width() - 20, this->height() - 20 - 60
                );
    ui->frmWaiting->setVisible(true);
    ui->btnSave->setVisible(false);

    // 重置亮度数据
    for (int i = 0; i < vecLedInfos.size(); i++) {
        vecLedInfos[i].brightnessImg = -1;
        vecLedInfos[i].brightnessLed = -1;
    }

    // 打开红外电源
    g_WinMeasure->setLampPwrOpened(true);

    // 运行抓图线程       /* 相机线程优先，这样好像可以减少丢帧 */
    captureCount = 0;
    cameraThread->isUseCameraApiProcess = this->isUseCameraApiProcess;
    cameraThread->start();      /* 独立线程运行抓图过程 */

    // 延时           /* 如果不延时，出现过后面的转灯指令无效的情况 */
    Util::waitMs(300);

    // 发送转灯指令
    g_WinMeasure->sendTurnLampCmd();

}

// 槽函数：抓图线程运行结束
void LampCalibrate::slot_cameraThread_RunEnd(QString _err_msg)
{
    // 关闭红外电源
    g_WinMeasure->setLampPwrOpened(false);

    // 隐藏等待界面
    ui->frmWaiting->setVisible(false);
    ui->btnSave->setVisible(true);

    // 显示错误
    if (captureCount != 23) {
        getWinManage()->showMsgWin(QString("img count = %1,\r\n").arg(captureCount) + _err_msg);
        return;
    }

    // 计算 LED 亮度
    const double GAMMA_OFFSET = 0;      // 由于 gamma 曲线不是理想直线引起的误差（“光强-亮度”曲线的斜率是随光强增加而下降的？）

    double brightness_0 = vecLedInfos[0].brightnessImg;
    vecLedInfos[0].brightnessLed = brightness_0;
    for (int i = 1; i < 23; i++) {
        vecLedInfos[i].brightnessLed = vecLedInfos[i].brightnessImg - brightness_0 - GAMMA_OFFSET;
    }

    // 刷新界面
    this->repaint();
}

// 槽函数：相机获得图像帧
void LampCalibrate::slot_cameraThread_GetImg(uchar *_img_raw, int _capture_count)
{
    static cv::Mat mat;

    if (!imgDataTemp) {
        imgDataTemp = (uchar *)malloc(cameraThread->getImgSize());
        mat = cv::Mat(cameraThread->getImgHeight(), cameraThread->getImgWidth(), CV_8UC1, imgDataTemp);
    }

    //
    captureCount++;

    // 图像处理（相机出来的原始图像是上下翻转的）
    if (isUseCameraApiProcess) {
        g_CameraIntf->imageProcess(_img_raw, imgDataTemp);
    } else {
        cv::Mat mat_raw = cv::Mat(cameraThread->getImgHeight(), cameraThread->getImgWidth(), CV_8UC1, _img_raw);
        cv::flip(mat_raw, mat, 0);      // 垂直翻转
    }

    // 存图
    if (isSaveTurnLampImg) {
        QString file_path = currImgsDir + QDir::separator() + QString::asprintf("%02d.png", _capture_count - 1);
        cv::imwrite(file_path.toLatin1().data(), mat);
    }

    // 释放图像内存
    free(_img_raw);
    _img_raw = Q_NULLPTR;

    //
    if (_capture_count > 0 && _capture_count <= 23) {
        // 计算图像灰度均值
        double mean, std_dev;
        CAlgoInvoker::calcMeanStdDev(mat, mean, std_dev);

        // 更新内存数据
        vecLedInfos[_capture_count - 1].brightnessImg = mean;
    } else {
        //
        logCritical(QString::asprintf("cameraThread_GetImg(): count = %d", _capture_count));
    }

}

// “读入设置” 按钮
void LampCalibrate::on_btnReadCfg_clicked()
{
    // 读取电流等级设置
    bool succ = readCfg();
    if (succ) {
        this->repaint();
    } else {
        QMessageBox::warning(this, "error", "读取设置失败！\r\n串口线没接？");
    }
}

// “自动设置” 按钮
void LampCalibrate::on_btnAutoConfig_clicked()
{
    // TODO:


}

// “保存” 按钮
void LampCalibrate::on_btnSave_clicked()
{
    writeCfg();

    //
    ui->btnSave->setEnabled(false);

    //
    this->repaint();
}

// 返回
void LampCalibrate::on_btnGoBack_clicked()
{
    // 结束未完成的流程
    // TODO:


    // 返回上一个窗口
    getWinManage()->backToLastWidget();
}

// 读入配置
bool LampCalibrate::readCfg()
{
    bool succ = false;

    //
    g_WinMeasure->clearCaliSerial(QSerialPort::Input);

    //
    g_WinMeasure->writeCaliSerial(CMD_READ_CFG, 5);              // 55 7A 03 0F 00

    QByteArray reply_head = QByteArray(CMD_READ_CFG_REP, 4);    // 55 7A 19 8F

    QByteArray reply_data;
    int idx;
    bool succ_read;

    QTime time;
    time.start();
    do {
        Util::waitMs(100);
        reply_data += g_WinMeasure->readCaliSerialAll();
        idx = reply_data.indexOf(reply_head);
        succ_read = (idx >= 0);                                 // 55 7A 19 8F 10 17 17 14 17 12 15 16 15 16 14 16 15 17 16 13 15 16 17 17 16 18 18
        qDebug() << reply_data.toHex();
    } while (!succ_read && time.elapsed() < 1 * 1000);

    if(succ_read)
    {
        // 更新内存数据
        for(int i = 0; i < 23; i++)
        {
            int current_level = (int)(reply_data[idx + 4 + i]);
            vecLedInfos[i].currentLevel = current_level;
        }
        succ = true;
    }

    //
    return succ;
}

// 保存配置
void LampCalibrate::writeCfg()
{
    //QByteArray cmd_refresh(CMD_REFRESH, 4);
    QByteArray cmd_savecfg(CMD_SAVE_FLASH, 4);

    char c;
    for(int i = 0; i < 23; i++)
    {
        c = (char)vecLedInfos[i].currentLevel;
        //cmd_refresh += c;
        cmd_savecfg += c;
    }

    //gWinMeasure->writeCaliSerial(cmd_refresh.data(), cmd_refresh.length());
    g_WinMeasure->writeCaliSerial(cmd_savecfg.data(), cmd_savecfg.length());
}

// 编辑控件的 “确定” 按钮
void LampCalibrate::on_btnOk_clicked()
{
    int val = spxCurrent->value();
    if (val >= MIN_CURRENT_LEVEL && val <= MAX_CURRENT_LEVEL) {
        vecLedInfos[indexEditing].currentLevel = val;

        // TODO: 值的有效性检查（值范围不是连续的，从底板程序来看，有效值只有 14 个）

        //
        ui->btnSave->setEnabled(true);
    } else {
        QMessageBox::warning(this, "error", QString::asprintf("电流等级须在 [%d, %d] 区间内", MIN_CURRENT_LEVEL, MAX_CURRENT_LEVEL));
    }

    //
    this->repaint();

    //
    ui->frmSetCurrent->setVisible(false);
    indexEditing = -1;
}

// 显示编辑控件
void LampCalibrate::showEdit(int _idx)
{
    if (_idx < 0) {
        if (ui->frmSetCurrent->isVisible()) {
            ui->frmSetCurrent->setVisible(false);
        }
    } else {
        indexEditing = _idx;

        // 显示编辑控件
        QRect rect = ui->frmSetCurrent->geometry();
        rect.setX(vecLedInfos[indexEditing].x + 55);
        rect.setY(vecLedInfos[indexEditing].y - 10);
        rect.setWidth(ui->frmSetCurrent->width());
        rect.setHeight(ui->frmSetCurrent->height());

        ui->frmSetCurrent->setGeometry(rect);
        spxCurrent->setValue(vecLedInfos[indexEditing].currentLevel);

        ui->frmSetCurrent->setVisible(true);
    }
}

// “显示抓图亮度” 复选框
void LampCalibrate::on_ckbShowImgBright_clicked(bool checked)
{
    isShowImgBright = checked;
}

// “存转灯图” 复选框
void LampCalibrate::on_ckbSaveTurnLampImg_clicked(bool checked)
{
    isSaveTurnLampImg = checked;
}

// “存数据” 复选框
void LampCalibrate::on_ckbSaveBrightnessValues_clicked(bool checked)
{
    isSaveBrightnessValues = checked;
}

