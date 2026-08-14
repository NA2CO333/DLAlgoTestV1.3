#ifndef WINDOWSMANAGER_H
#define WINDOWSMANAGER_H

#include <QWidget>
#include <QHostInfo>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QMutex>

#include "mainwindow.h"
#include "winclinic.h"
#include "winpersonalrecord.h"
#include "winmeasure.h"
#include "result.h"
#include "tool.h"
#include "winscreen.h"
#include "personalinfos.h"
#include "myserialport.h"
#include "mysqlitepatients.h"
#include "statusbarform.h"
#include "eyesightstandard.h"
#include "printersetting.h"
#include "musicsetting.h"
#include "noticewin.h"
#include "wifibt/winwifi.h"
#include "uploadthread.h"
#include "batterymonitor.h"
#include "winupdatesetup.h"
#include "settings/loginwin.h"
#include "windatatrans.h"
#include "datepage.h"
#include "progresswindow.h"
#include "previewimage.h"
#include "settings/settings.h"
#include "themebackground.h"
#include "aboutdevice.h"
#include "runningstatus.h"
#include "engineermode/engineermode.h"
#include "DataTransmit.h"
#include "lampcalibrate.h"
#include "keyboard.h"
#include "soundintf.h"
#include "winbluetooth.h"
#include "basewindow.h"
#include "suspensionpromptbox.h"
#include "winmanage.h"
#include "print-intf.h"
#include "windiagnosticstandard.h"
#include "windiagnosissuggestion.h"
#include "mpro-sys-communic.h"
#include "winupdateprogress.h"

// TODO: 这里引用太多，而这个又是公用模块，导致多个模块的头文件一旦有一点小改动，都要重新编译大量模块？
// TODO: 改为前置声明？

// 系统状态
enum enSystemState
{
    systemState_Normal          = 0,        // 正常状态
    systemState_DarkLight,                  // 暗光状态（息屏前先调为改状态一段时间）
    systemState_LowPower,                   // 省电状态（省电状态下也有可能亮屏，但并不退出省电状态，比如插拔充电插头时）
};

///=============================================================================================================
/// class PowerControl

// 电源控制
class PowerControl : public QObject
{
    Q_OBJECT

public:
    static PowerControl* getlInstance();

    enSystemState getSystemState();
    void reset();

    static void setScreenBrightnessNormal();
    static void setScreenBrightnessDark();
    static void setScreenBrightnessClosed();

    static void setScreenBrightnessPercent(int _percent);

    static void setSystemState(enSystemState _state);

    static void lowPower();                         // 进入省电模式
    static void wakeUp();

private slots:
    void slot_timer_timeout();

private:
    explicit PowerControl();

    QTimer m_timer;
    long timeCnt;
    static enSystemState systemState;
    static PowerControl *powerCtl;

    void controller();

};

///=============================================================================================================
/// class WindowsManagers

// 前置声明
class QNetworkAccessManager;

// 条码内容事件的回调。返回值：是否可以继续（若否，则拦截底层的后续处理过程）。若返回false，回调函数须负责提示用户。回调函数中，不需做隐藏自身窗口的操作。
typedef bool (*funcCallbackCanProcessQrCode)();

// 程序运行的基本对象的构建，提供程序全局的功能及的程序全局事件交互
class WindowsManagers : public QWidget
{
    Q_OBJECT

public:
    static WindowsManagers *getInstance();
    ~WindowsManagers();

    void init();

    void updateUItheme();
    static bool checkUdiskAndUpdate(QString *_msg = Q_NULLPTR);     // 检查 U 盘是否有更新，若有，则询问；返回更新是否完成

    Result *getResultWin();     // TODO: 临时写法，需机构优化
    MainWindow *getHomeWin();
    Tool *getToolWin();

    void showLogin();

    void syncData();
    void closeApp(int _exit_code = EXIT_SUCCESS);
    void powerOff();
    void reboot();

    /**
     * @brief 判断内存空间是否足够
     * @param _is_force: 是否强制重新获取
     * @param value：空间阀值，单位MB，默认1G
     * @return ：true：空间大于阀值，false：空间小于阀值或者读取异常
     */
    static bool isStorageSpaceEnough(bool _is_force = false, ulong value = 1024);   // TODO: 和 RunningStatus::getIsStorageFull() 合并？

    void checkStartupEvent(int _event_type);
    bool getIsStartupFinished();                                // 获取 “程序启动完成” 状态

    void setMProSysPushSvcAddr(const QString &_url_str);                    // 设置【MPro系统推送服务通信】模块的服务地址（若未设置，则使用默认值）
    void autoSetStatOfMProSysPushSvcCommunic(bool _is_wifi_connected);      // 自动设置【MPro系统推送服务通信】模块的状态（可能启动，也可能关闭）

    Net::Remote::CMProSysPushSvcCommunic *mproSysPushSvcCommunic();
    Net::Remote::CMProSysCommunic *mproSysCommunic();

    void setOperationLocked(bool _locked, QString &_msg);                   // 设置【操作是否锁定】       // NOTE: 操作锁定，指禁止执行一些关键操作，目前(2025-09-03)只有【进入测量】操作
    bool isOperationLocked(QString &_msg) { _msg = m_operationLockedMsg; return m_isOperationLocked; }

    void setScreenLocked(bool _locked, QString &_msg);                      // 设置【屏幕是否锁定】       // NOTE: 屏幕锁定后，显示提示框，禁止任何屏幕操作
    bool isScreenLocked(QString &_msg) { _msg = m_screenLockedMsg; return m_isScreenLocked; }

    /**
     * @brief 注册扫码的侦听者窗口（注意：1、同一时间只允许存在一个帧听者。2、帧听者不需使用扫码事件后，须反注册。）
     * @param _listener
     * @param _scanned_callback 扫码事件回调函数，可用类的静态函数形式定义
     */
    void regKbReader(const QWidget *_listener, funcCallbackCanProcessQrCode _scanned_callback = nullptr);

    /**
     * @brief 反注册扫码的帧听者窗口（帧听者窗口应确保在自己的隐藏中调用此方法）
     * @param _listener
     */
    void unregKbReader(const QWidget *_listener);

    static void configOtaUpdate();                              // 配置 OTA 更新模块（执行 OTA 其它功能函数前先执行本过程对 OTA 进行必要的设置）
    static int checkBatteryLevelForOta();                       // 检查电量是否满足 OTA 的最低要求（若满足，则返回0，否则返回最低电量）

    void showDevActivateDialog();                               // 显示设备激活对话框
    void hideDevActivateDialog();                               // 隐藏设备激活对话框

signals:
    void sendSIGNAL(enSysSignal _sys_signal);
    void senderSignal();
    void sigConfigLoaded();         // 配置载入后
    void sigSavePrintscreen();
    void sigPhysicButtonPressed();  // “物理按键被按下”信号  /* 注意：这个信号有多个接收者，所以需要接收者判断自己当前是否需要处理该信号 */  // TODO: 应该定义不同的信号发给不同的接收者吗？
    void sigStartupFinished();                                  // 程序启动完成事件

protected slots:
    void slot__SysSignalReceived(enSysSignal _sys_signal);
    void slot_this_SavePrintscreen();
    void slot_this_StartupFinished();
    void slot_this_PhysicButtonPressed();
    void slot_WifiIntf_OpenedStateChanged(bool _is_opened);             // “WiFi是否已打开”状态改变事件
    void slot_WifiIntf_ConnectedStateChanged(bool _is_connected);       // “WiFi是否已连接”状态改变事件
    void slot_KbReader_Getline(QByteArray _line_bytes);
    void slot_timerPublic_timeout();
    void slot_DevActivateStatReceived(bool _is_activated);
    void slot_mproSysPushSvc_ReceivedOutpatientArchive(Net::Remote::stOutpatientArchive _archive);      // 云端的档案接收事件
    void slot_usbDriveMonitor_UsbDriveOnlineChanged(bool _is_on);

public slots:
    void slotAboutToExit();
    void slot_uploadThread_DataTransmiterGetNewSubject(DataTrans::Client _client);      // 【DataTransmiter 收到开启测量指令】槽函数
    void slot_uploadThread_DataTransmiterOperationLocked(bool _locked, QString _msg);   // 【DataTransmiter 收到操作锁定】槽函数

    // 事件：二维码 - 接收到 编码
    void doOn_QrCode_ReceivedCode(QByteArray _line_bytes);

    /**
     * @brief 事件：二维码 - 接收到 受检者对象
     * @param _pat
     * @param _is_number_only   二维码中是否仅含编号
     * @param _old_win_not_keep 见 CWinManage::showWindowByType()
     */
    void doOn_QrCode_ReceivedPatient(const CPatient &_pat, bool _is_number_only = false, QWidget *_old_win_not_keep = nullptr);

protected:
    bool eventFilter(QObject *_obj, QEvent *_event);
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    void buildGlobalObjs();

    QString m_mproSysPushSvcAddr;           // 【MPro系统推送服务通信】地址（含动态参数字段）（若未设置，则使用默认值）

    bool m_isOperationLocked {false};       // 操作是否已锁定      // NOTE: 操作锁定，指禁止执行一些关键操作，目前(2025-09-03)只有【进入测量】操作
    QString m_operationLockedMsg;           // 操作锁定消息

    bool m_isScreenLocked {false};          // 屏幕是否已锁定      // NOTE: 屏幕锁定后，显示提示框，禁止任何屏幕操作
    QString m_screenLockedMsg;              // 屏幕锁定消息

private:
    explicit WindowsManagers(QWidget *parent = 0);
    static WindowsManagers *wm;

    MainWindow *homeWin;
    Tool *toolWin;
    Result *resultWin;
    PersonalInfos *perWin;
    WinDataTrans *DataTranSet;
    MySQLitePatients *mysql;
    std::vector<CPatient> mLk;
    QThread *threadUploadWork;
    QThread *btThread;    //2020.10.12  tao
    bool flag;//sun
    //frmInputNew *input;//sun
    BatteryMonitor *bMonitor; // wim
    WinUpdateSetup *winUpdateSetup;
    PowerControl *powerCtl;
    aboutdevice *about;
    datePage *DatePage;
    printerSetting *Printer;
    eyesightstandard *Eyesight;
    MusicSetting *Music;
    settings *Setting;
    themebackground *tmemepic;
    RunningStatus *Device;
    previewimage *PreviewImage;
    CEngineerMode *engineerMode;
    //LampCalibrate *lampCalibrate;
    //ThreadSend *myThread;
    WinDiagnosticStandard *winDiagnosticStandard;
    WinDiagnosisSuggestion *winDiagnosisSuggestion;

    WinUpdateProgress *winUpdateProgress = nullptr;

#if (1 == OS_TYPE)
    QTimer *timerNetTimeSync = Q_NULLPTR;
#endif
    QTimer *m_timerPublic {nullptr};            // 公用定时器（为其它需要简单定时需求的模块提供每秒一次的定时事件）

    bool isExitExecuted = false;                // “退出”槽函数是否已执行

    bool m_isStartupFinished = false;             // 程序启动是否已完成

    bool m_isCameraOK = false;
    bool m_isBatteryGot = false;
    bool m_isDistSensorChecked = false;

    const QWidget *m_qrCodeScannedListener {nullptr};
    funcCallbackCanProcessQrCode m_callbackCanProcessQrCode {nullptr};         // 【能否处理扫码内容】回调（用于检查当前状态下是否可允许进入【扫码之后】事件）

};

///=============================================================================================================
/// extern variable

extern UpLoadThread *g_uploadThread;

extern CCameraIntf *g_CameraIntf;
extern WinMeasure *g_WinMeasure;

extern WinScreen *batchscr;
extern WinClinic *historyWin;

extern CWifiIntf *g_WifiIntf;
extern WinWifi *g_WinWifi;
extern CBluetoothIntf *g_Bluetooth;
extern CSoundIntf *g_SoundIntf;

extern CSerialDatatrans *g_SerialDatatrans;

extern WinBluetooth *btWin;

extern bool g_AutoTest;

extern Common::CPrintIntf *g_printIntf;

QThread *getCommonThread();                 // 获取公用线程

inline WindowsManagers *globalService() { return WindowsManagers::getInstance(); }

extern QElapsedTimer g_elapsedTimer;

/*inline*/ QNetworkAccessManager *networkManager();     // 全局网络访问管理器

#endif // WINDOWSMANAGER_H
