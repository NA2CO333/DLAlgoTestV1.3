#ifndef ENGINEERMODE_H
#define ENGINEERMODE_H

#include <QWidget>

#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QPushButton>

#include "baseform.h"
#include "myeditline.h"
#include "shellsimulate.h"
#include "authintf.h"
#include "doublespinbox.h"
#include "mpro-sys-communic.h"

namespace Ui {
class engineerMode;
}

//
extern bool g_hasBluetooth;     // 是否有蓝牙硬件模块
extern bool voltageState;
extern bool saveImage;
extern bool isPrintScreen;      // 是否屏幕截图

// 授权查询状态
enum enAuthCheckingState {
    authCheckingState_No            = 0,    // 未查询
    authCheckingState_Querying,             // 正在查询
    authCheckingState_Succ,                 // 查询成功
    authCheckingState_Fail,                 // 查询失败
};

//
class CEngineerMode : public CBaseWidget
{
    Q_OBJECT

public:
    explicit CEngineerMode(QWidget *parent = 0);
    ~CEngineerMode();

    static bool isPasswordEnabled() { return s_isPasswordEnabled; }     // 打开本界面是否需要密码

    void syncTrialStat();       // 同步试用机状态（获取试用机授权状态）
    enAuthCheckingState getAuthCheckingState();

public slots:
    void slot_authIntf_QueryAuthInfoFinished(CAuthIntf::enAuthIntfErrType _err_type, QString _err_msg);
    void slot_mproSysPushSvc_DevActivateStatReceived(Net::Remote::stDevActivateStat _activate_stat);

signals:
    void cycleTestSig();
    void sigQueryAuthInfo();
    void sigDevActivateStatReceived(bool _is_activated);

protected slots:
    void cycleTest();
    void updateFont();
    void updatePsplash();
    void slot_this_DevActivatedChanged(bool _is_activated);

protected:
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    void showEvent(QShowEvent*);
    void hideEvent(QHideEvent *);
    void mousePressEvent(QMouseEvent *e);

    void showDebugModeCtrls(bool _is_debug_mode);

    void globalToUi();
    void uiToGlobal();
    void restoreValues();

    bool setFloatValAfterEditChanged(myEditLine *_edit, QString _str_new, float _val_old);
    bool setIntValAfterEditChanged(myEditLine *_edit, QString _str_new, int _val_old);

    static bool s_isPasswordEnabled;        // 打开本界面是否需要密码

    int ledMode;

    QTimer *ledTimer = Q_NULLPTR;
    int verDate;
    bool isNeedCloseIR = false;     // 是否需要关闭红外

    shellsimulate *shell = Q_NULLPTR;

    bool bleLevelEdited = false;

    CAuthIntf *authIntf = Q_NULLPTR;                                    // TODO: 移到全局管理模块
    enAuthCheckingState authCheckingState = authCheckingState_No;

private Q_SLOTS:
    void on_btnCameraRestartCnt_clicked();
    void on_upgradeFirmwareButton_clicked();
    void on_pushButton_back_clicked();
    void on_updatePyDB_pushButton_clicked();
    void on_led_pushButton_clicked();
    void on_btnGetLocalIp_clicked();
    void on_exportLog_pushButton_clicked();
    void on_curve_comboBox_currentTextChanged(const QString &arg1);
    void on_exportDB_pushButton_clicked();
    void on_btnUnitDebug_clicked();
    void on_pushButton_Poweroff_clicked();
    void on_pushButton_Reboot_clicked();
    void on_btnQuit_clicked();
    void on_btnRestoreDefault_clicked();
    void on_ckbDebugMode_clicked(bool _checked);
    void on_ckbBluetoothUi_clicked(bool checked);
    void on_btnQuitToToolkits_clicked();
    void on_btnShellSimulate_clicked();
    void on_cbbLogFilter_activated(int index);
    void on_ckbSpecifiedAlgo_clicked(bool checked);
    void on_ckbSaveSimulateImage_clicked(bool checked);
    void on_ckbUseSimulateImage_clicked(bool checked);
    void on_btnGetContrast_clicked();
    void on_btnResetCameraParm_clicked();
    void on_btnLampCalibrate_clicked();
    void on_btnCameraUninit_clicked();
    void on_btnCameraPowerOff_clicked();
    void on_btnCameraPowerOn_clicked();
    void on_btnCameraInit_clicked();
    void on_btnCameraRePowerOn_clicked();
    void on_btnGotoFocus_clicked();
    void on_cbbLedLevelMiddle_activated(int index);
    void on_cbbLedLevelEccentric_activated(int index);
    void on_btnSyncTrialStat_clicked();
    void on_edtDistanceInterval_textEdited(const QString &arg1);
    void on_edtDistanceFix_textEdited(const QString &arg1);
    void on_edtMinPupilParamRatio_textEdited(const QString &arg1);
    void on_edtMaxAlgoFail_textEdited(const QString &arg1);
    void on_edtPupilAverageMin_textEdited(const QString &arg1);
    void on_edtPupilAverageMax_textEdited(const QString &arg1);
    void on_edtExposureMsMin_textEdited(const QString &arg1);
    void on_edtExposureMsMax_textEdited(const QString &arg1);
    void on_edtExpoCoarseAdjStepMs_textEdited(const QString &arg1);
    void on_edtMinPupilStdDev_textEdited(const QString &arg1);
    void on_edtCaptureInterval_textEdited(const QString &arg1);
    void on_edtPupilDetectedCount_textEdited(const QString &arg1);
    void on_btnDevActivate_clicked();
    void on_ckbIsTurnLampTestMode_clicked(bool _checked);
private:
    Ui::engineerMode *ui;
};

#endif // ENGINEERMODE_H
