#ifndef LAMPCALIBRATE_H
#define LAMPCALIBRATE_H

#include <QWidget>
#include <QVector>
#include <QThread>

#include "CameraIntf.h"
#include "baseform.h"

#include "opencv2/core/core.hpp"

#ifndef SCREENER_TEST
# include "myeditline.h"
#else
# include "screener_test.h"
#endif

namespace Ui {
class LampCalibrate;
}

// LED 灯珠信息
struct stLedInfo {
    int x;                  // 在界面上的 x 坐标
    int y;                  // 在界面上的 y 坐标
    int w;                  // 在界面上的宽度
    double brightnessImg;   // 抓图亮度（并不是单个灯珠）
    double brightnessLed;   // LED 亮度
    int currentLevel;       // 电流档位
};

// 灯板校准
class LampCalibrate : public CBaseWidget
{
    Q_OBJECT

public:
    explicit LampCalibrate(QWidget *parent = 0);
    ~LampCalibrate();

protected:
    const int MIN_CURRENT_LEVEL = 07;           // 最小电流等级
    const int MAX_CURRENT_LEVEL = 24;           // 最大电流等级

    QVector<QWidget *> vecLedPositions;
    QVector<stLedInfo> vecLedInfos;
    QImage *imgLedBoard;
    CCapture *cameraThread;
    int captureCount;
    bool isUseCameraApiProcess;             // 是否使用相机的图像处理 API 函数

    uchar *imgDataTemp = Q_NULLPTR;
    mySpinBox *spxCurrent;
    int indexEditing = -1;

    bool isShowImgBright = false;
    bool isSaveTurnLampImg = false;
    bool isSaveBrightnessValues = false;

    QString currImgsDir;

    void mouseReleaseEvent(QMouseEvent *_event);
    void paintEvent(QPaintEvent *);
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

    void createVecLedPositions();
    void createVecLedInfos();

    bool readCfg();
    void writeCfg();
    void showEdit(int _idx);
    bool initCamera();

private slots:
    void on_btnMeasureBrightness_clicked();
    void on_btnReadCfg_clicked();
    void on_btnAutoConfig_clicked();
    void on_btnSave_clicked();
    void on_btnGoBack_clicked();
    void on_btnOk_clicked();
    void on_ckbShowImgBright_clicked(bool checked);
    void on_ckbSaveTurnLampImg_clicked(bool checked);
    void on_ckbSaveBrightnessValues_clicked(bool checked);

    void slot_cameraThread_GetImg(uchar *_img_raw, int _capture_count);
    void slot_cameraThread_RunEnd(QString _err_msg);

private:
    Ui::LampCalibrate *ui;
};

#endif // LAMPCALIBRATE_H
