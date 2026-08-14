//事件管理，配置硬件
#include "windowsmanager.h"

#include <QDebug>
#include <QMetaType>
#include <QFontDatabase>
#include <QScreen>
#include <QDesktopWidget>
#include <QApplication>
#include <QElapsedTimer>
#include <QtNetwork/QNetworkAccessManager>

#include "system/systemini.h"
#include "printertransmit.h"
#include "DataTransmit.h"
#include "global.h"
#include "appsetting.h"
#include "hardware.h"
#include "authintf.h"
#include "winmanage.h"
#include "otaupdatedefs.h"
#include "update.h"
#include "global_obj.h"
#include "form-dev-activate.h"
#include "data-intf-huayi.h"
#include "nettools.h"
#include "maintenance.h"
#include "data-intf-an-hui-screen.h"
#include "data-intf-other.h"
#include "usb-drive-monitor.h"

//using namespace AeaQt;

using Common::CPrintIntf;

//
bool g_AutoTest = false;        // 是否启用连拍

bool loginState = false;
loginWin *mLogin = Q_NULLPTR;

UpLoadThread *g_uploadThread = Q_NULLPTR;
CCameraIntf *g_CameraIntf = Q_NULLPTR;
WinMeasure *g_WinMeasure = Q_NULLPTR;

CWifiIntf *g_WifiIntf = Q_NULLPTR;
WinWifi *g_WinWifi = Q_NULLPTR;

CBluetoothIntf *g_Bluetooth = Q_NULLPTR;
//QThread *g_threadBluetooth = Q_NULLPTR;   // TODO: 这线程应该放到蓝牙模块内？

WinBluetooth *btWin = Q_NULLPTR;

CSerialDatatrans *g_SerialDatatrans = Q_NULLPTR;
CSoundIntf *g_SoundIntf = Q_NULLPTR;                    // TODO: 这种既有全局指针，又有静态单例函数的情况，是否应该把静态单例函数屏蔽掉，避免两种用法同时存在，不好维护？

QThread *g_commonThread = Q_NULLPTR;            // 公用事件线程
CPrintIntf *g_printIntf = Q_NULLPTR;            // 打印接口

Net::Remote::CMProSysPushSvcCommunic *g_mproSysPushSvcCommunic {nullptr};       // 门诊档案推送的接收
Net::Remote::CMProSysCommunic *g_mproSysCommunic = nullptr;     // MPro系统通信

WinScreen *batchscr = Q_NULLPTR;
WinClinic *historyWin = Q_NULLPTR;
WinPersonalRecord *winPerRec = Q_NULLPTR;

QElapsedTimer g_elapsedTimer;
QNetworkAccessManager *g_networkManager = nullptr;      // 全局网络访问管理器

CMaintenance *g_maintenance {nullptr};                  // 维护模块

CUsbDriveMonitor *g_usbDriveMonitor = nullptr;          // U 盘插拔侦听

///=============================================================================================================
/// class WindowsManagers

//
WindowsManagers *WindowsManagers::wm = Q_NULLPTR;
Result *WindowsManagers::getResultWin()
{
    return resultWin;
}

MainWindow *WindowsManagers::getHomeWin()
{
    return homeWin;
}

Tool *WindowsManagers::getToolWin()
{
    return toolWin;
}

void WindowsManagers::showLogin()
{
    bool needLogin = appSetting::value("/login/needlogin").toBool();
    if(needLogin)
    {
        qDebug() << "show loginWin";
        getWinManage()->showWindowByType(WIN_LOG);
    }
    else
    {
        qDebug() << "show homeWin";
        getWinManage()->showWindowByType(WIN_HOME);
    }
}

void WindowsManagers::syncData()
{
    appSetting::sync();

    system("sync");         // TODO: 这个系统命令和 C 函数 sync() 相比，前者是阻塞的，后者是非阻塞的？所以后者返回时，缓冲未必已经写入？

}

void WindowsManagers::closeApp(int _exit_code)
{
    //
    g_usbDriveMonitor->setIsStarted(false);

    // 数据同步
    this->syncData();

    //
    //slotAboutToExit();        // TODO: 这是事件槽函数，已被自动调用？

    //
    QApplication::exit(_exit_code);
}

void WindowsManagers::powerOff()
{
    logWarning(QString(__PRETTY_FUNCTION__) + ": into ...");

    // 退出程序
    closeApp();

    // 同步系統时间到硬件
    CHardware::syncHardwareDateTime(true);
    Util::waitMs(200);          // TODO: 是否必要？

    // 向底板发送关闭电源指令
    for(int i=0;i<3;i++)
    {
        MySerialPort::instance()->write(power_off_command);
        Util::waitMs(20);
    }
}

void WindowsManagers::reboot()
{
    // 退出程序
    closeApp();

    // 重启系统
#if (OS_TYPE != 2)
    system("reboot");
#endif

}

bool WindowsManagers::isStorageSpaceEnough(bool _is_force, ulong value)
{
    static const qint64 TIME_EFFECTIVE = 3 * 60 * 1000;     // 有效时间（号秒）

    static ulong space_last = 0;
    static QElapsedTimer elapsed_timer;

    if (!_is_force && space_last != 0 && elapsed_timer.isValid() && elapsed_timer.elapsed() < TIME_EFFECTIVE) {
        return (space_last > value);
    }

    ulong space;
    if (Util::getDirSpace("/root", &space) == 0) {
        space_last = space;
        elapsed_timer.start();

        return (space > value);
    }

    return false;
}

/**
 * @brief 接收程序启动事件，检查程序启动是否已完成，并且若完成，则做相关处理
 * @param _event_type : 1-相机初始化完成，2-收到电量，3-测距模块类型自检完成
 */
void WindowsManagers::checkStartupEvent(int _event_type)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, _event_type = " << _event_type;

    //
    if (m_isStartupFinished) {
        return;
    }

    /* 启动完成判断逻辑：
     * 1、有电量。
     * 2、相机初始化已完成。
     * 3、测距模块类型自检完成。
     */
    if (1 == _event_type) {
        m_isCameraOK = true;
    } else if (2 == _event_type) {
        m_isBatteryGot = true;
    } else if (3 == _event_type) {
        m_isDistSensorChecked = true;
    }

    //
    if (m_isCameraOK && m_isBatteryGot && m_isDistSensorChecked) {
        m_isStartupFinished = true;
    }

    //
    qDebug() << "app startup finished =================";

    // 启动完成后的处理
    if (m_isStartupFinished) {
        // 启动完成后减少 log 输出：将 log 等级设为 Info；关闭 log 输出；
        CGlobal::setIsLogEnabled(false);
        //CGlobal::setLogLevel(logLevel_Info);

    }

    //
    if (m_isStartupFinished) {
        emit sigStartupFinished();
    }

}

bool WindowsManagers::getIsStartupFinished()
{
    return m_isStartupFinished;
}

void WindowsManagers::setMProSysPushSvcAddr(const QString &_url_str)
{
    m_mproSysPushSvcAddr = _url_str;
}

void WindowsManagers::autoSetStatOfMProSysPushSvcCommunic(bool _is_wifi_connected)
{
    qDebug() << "WindowsManagers::autoSetStatOfMProSysPushSvcCommunic() entered, _is_wifi_connected = " << Util::bool2str(_is_wifi_connected);

    //
    bool is_condition_ok = false;

    // 判断当前是否满足启动条件，若是，则启动
    do {
        // wifi 须已连接
        if (!_is_wifi_connected) {
            break;
        }

        // 设备编号须已设置
        if (!(CGlobal::devNum.length() > 0)) {
            qInfo() << "Device Number not setted.";
            break;
        }

        // “数据传输”页面的接口设置的须是“万灵云端”的接口，或当前为调试模式，或设备未激活   // NOTE: 不是“万灵云端”的接口，数据不会上传万灵云端，连接推送服务没有意义
        //if (!(WinDataTrans::isManylinksDataIntf() || CGlobal::isDebugMode || !CGlobal::isDevActivated)) {
        //    qInfo() << "DataTrans IntfType not CloudOutPatient.";
        //    break;
        //}
        // NOTE: 当前数据接口不管是否万灵云端接口，都要接收设备激活状态

        //
        is_condition_ok = true;
    } while (false);
    qInfo() << "Conditions of starting MProSysPushSvcCommunic is " << Util::bool2str(is_condition_ok);

    // 若满足条件，则启动，否则关闭
    if (is_condition_ok) {
        qWarning() << "opening MProSysPushSvcCommunic ...";

        if (!(g_mproSysPushSvcCommunic->getIsOpened())) {
            static const QString WS_ADDR_TEMPLATE_PRODUCE   = "wss://opt.manylinksmed.com";     // 正式环境地址（格式：“协议://IP:端口”）
            static const QString WS_ADDR_TEMPLATE_TEST      = "ws://120.25.254.38:9006";        // 测试环境地址（格式：“协议://IP:端口”）

            QUrl url_addr(!m_mproSysPushSvcAddr.isEmpty() ? m_mproSysPushSvcAddr : (CGlobal::isDebugMode ? WS_ADDR_TEMPLATE_TEST : WS_ADDR_TEMPLATE_PRODUCE));
            qDebug() << "MPro Pushing Svc Addr Url: " << url_addr.toString();

            g_mproSysPushSvcCommunic->setServiceAddr(url_addr);
            g_mproSysPushSvcCommunic->setRequestParam_DevCode(CGlobal::devNum);
            g_mproSysPushSvcCommunic->setIsOpened(true);
        }
    } else {
        qWarning() << "closing MProSysPushSvcCommunic ...";

        if (g_mproSysPushSvcCommunic->getIsOpened()) {
            g_mproSysPushSvcCommunic->setIsOpened(false);
        }
    }
}

Net::Remote::CMProSysPushSvcCommunic *WindowsManagers::mproSysPushSvcCommunic()
{
    return g_mproSysPushSvcCommunic;
}

Net::Remote::CMProSysCommunic *WindowsManagers::mproSysCommunic()
{
    return g_mproSysCommunic;
}

void WindowsManagers::setOperationLocked(bool _locked, QString &_msg)
{
    //
    m_isOperationLocked = _locked;
    m_operationLockedMsg = _msg;
}

void WindowsManagers::setScreenLocked(bool _locked, QString &_msg)
{
    //
    m_isScreenLocked = _locked;
    m_screenLockedMsg = _msg;

    //
    if (_locked) {
        // 返回主页


        // 弹出消息框，且不可点击

    } else {
        // 隐藏消息框

    }
}

void WindowsManagers::regKbReader(const QWidget *_listener, funcCallbackCanProcessQrCode _scanned_callback)
{
    if (!CGlobal::isReadBarcodeByQt) {
        // 向底层注册帧听者
        kbReader()->RegListener(_listener);

        // 设置当前帧听者及回调函数
        if (_listener != m_qrCodeScannedListener) {
            m_qrCodeScannedListener = _listener;
            m_callbackCanProcessQrCode = _scanned_callback;
        } else {
            getWinManage()->showSuspensionPrompt("LogicError: QrCodeScanned listener already registered!", -1);
        }

        // 当帧听者数量为 1 时，连接扫码事件处理函数（不必且不应连接多次，且须确保有帧听者存在时此信号槽已连接）
        if (kbReader()->countListener() == 1) {
            QObject::connect(kbReader(), &Util::CBarcodeDataDecoder::sigGetLine, this, &WindowsManagers::slot_KbReader_Getline,
                             (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
        }
    } else {
        getWinManage()->showSuspensionPrompt("LogicError: QrCodeScanningMethod is wrong!", -1);
    }
}

void WindowsManagers::unregKbReader(const QWidget *_listener)
{
    if (!CGlobal::isReadBarcodeByQt) {
        // 向底层反注册帧听者
        kbReader()->UnregListener(_listener);

        // 若反注册的帧听者为当前帧听者时，清除当前帧听者及回调函数（有可能后注册的帧听者注册后，前注册者才反注册）
        if (_listener == m_qrCodeScannedListener) {
            m_qrCodeScannedListener = nullptr;
            m_callbackCanProcessQrCode = nullptr;
        }

        // 当帧听者数量为 0 时，连接扫码事件处理函数（去除不必要的信号槽连接，且须确保没有帧听者存在时此信号槽连接被断开）
        if (kbReader()->countListener() == 0) {
            QObject::disconnect(kbReader(), &Util::CBarcodeDataDecoder::sigGetLine, this, &WindowsManagers::slot_KbReader_Getline);
        }
    } else {
        getWinManage()->showSuspensionPrompt("LogicError: QrCodeScanningMethod is wrong!", -1);
    }
}

void WindowsManagers::configOtaUpdate()
{
    static const QString DATA_BACKUP_NAME_PREFIX = "ScreenerDataBackup";    // 数据备份文件名前缀（用作查找备份文件的关键词）

    //
    OtaUpdate::setIsChinese(G_LANGUAGE_CHINESE == CGlobal::language);
    OtaUpdate::setBackupFilePrefix(DATA_BACKUP_NAME_PREFIX);
}

int WindowsManagers::checkBatteryLevelForOta()
{
    static const int MIN_BATTERY = 2;

    //
    bool is_charging = BatteryMonitor::getIsCharging();
    if (is_charging) {
        return 0;
    } else {
        int battery_level = BatteryMonitor::getBattLevel();
        bool is_battery_ok = (battery_level >= MIN_BATTERY);
        return (is_battery_ok ? 0 : MIN_BATTERY);
    }
}

void WindowsManagers::showDevActivateDialog()
{
    //
    FormDevActivate *form_activate = FormDevActivate::instance();
    if (!form_activate->isVisible()) {
        if (!about->isVisible()) {
            if (!engineerMode->isVisible()) {
                getWinManage()->showWidgetAsDialogView(form_activate);
            } else {
                logDebug(QString("%1: EngineerMode Win already shown! DevActivate Win showing skipped.").arg(__PRETTY_FUNCTION__));
            }
        } else {
            logDebug(QString("%1: About Win already shown! DevActivate Win showing skipped.").arg(__PRETTY_FUNCTION__));
        }
    } else {
        logDebug(QString("%1: DevActivate Win already shown! DevActivate Win showing skipped.").arg(__PRETTY_FUNCTION__));
    }
}

void WindowsManagers::hideDevActivateDialog()
{
    FormDevActivate *form_activate = FormDevActivate::instance();
    if (form_activate->isVisible()) {
        form_activate->hide();
    }
}

void WindowsManagers::buildGlobalObjs()
{
    // 公用线程
    g_commonThread = new QThread;
    g_commonThread->start();

    //
    g_networkManager = new QNetworkAccessManager();

    // WiFi 接口
    g_WifiIntf = CWifiIntf::instance();

    QObject::connect(g_WifiIntf, &CWifiIntf::sigOpenedStateChanged, this, &WindowsManagers::slot_WifiIntf_OpenedStateChanged, Qt::QueuedConnection);
    QObject::connect(g_WifiIntf, &CWifiIntf::sigConnectedStateChanged, this, &WindowsManagers::slot_WifiIntf_ConnectedStateChanged, Qt::QueuedConnection);

    // 蓝牙接口
    g_Bluetooth = CBluetoothIntf::getInstance();
    //g_threadBluetooth = new QThread;
    //g_Bluetooth->moveToThread(g_threadBluetooth);

//    // 构建及设置数据传输串口连接对象
//    g_SerialDatatrans = new CSerialDatatrans();     // TODO: 这个应该要放到窗体构建的前面，但是因为 DataTrans::DataTransmiter 的设置在 datatrans 窗体里载入，所以 ...
//    g_SerialDatatrans->isChinese = language;
//    g_SerialDatatrans->checkAndSet();

    // 声音播放接口
    g_SoundIntf = CSoundIntf::getInstance();

    // 直连打印机接口
    g_printIntf = CPrintIntf::instance();
    g_printIntf->moveToThread(g_commonThread);

    // 创建程序更新模块对象
    g_update = new CUpdate(g_networkManager);

    QObject::connect(g_update, &CUpdate::sigRebootSystem, this, &WindowsManagers::reboot, Qt::QueuedConnection);

    //
    g_maintenance = new CMaintenance();

    QObject::connect(this, &WindowsManagers::sigConfigLoaded, g_maintenance, &CMaintenance::slotConfigLoaded, Qt::DirectConnection);    // NOTE: 这里须直连，否则无法确保在相机初始化前完成

    //
    g_uploadThread = new UpLoadThread;
    threadUploadWork = new QThread;

    g_CameraIntf = CCameraIntf::newInstance();

    //
    Net::Remote::CMProSysCommunic::setConfigDirPath(CGlobal::pathConfig());

    g_mproSysCommunic = new Net::Remote::CMProSysCommunic();

    g_mproSysCommunic->setDevCode(CGlobal::devNum);

    //
    g_usbDriveMonitor = CUsbDriveMonitor::instance();
    QObject::connect(g_usbDriveMonitor, &CUsbDriveMonitor::sigUsbDriveOnlineChanged, this, &WindowsManagers::slot_usbDriveMonitor_UsbDriveOnlineChanged, Qt::QueuedConnection);

    // 全局接口
    CGlobalObj *global_obj = new CGlobalObj();
    setGlobalIntf(global_obj);

}

WindowsManagers::WindowsManagers(QWidget *parent) :
    QWidget(parent)
{
    qDebug() << __PRETTY_FUNCTION__ << ": threadAddr = " << reinterpret_cast<uintptr_t>(QThread::currentThread())
             << ", currentThreadId = " << reinterpret_cast<uintptr_t>(QThread::currentThreadId());

    // 更新UI主题
    //setSysThemeType(getSysThemeType());
    //setSysThemeType(themeType_Black, false);      // TODO: 全局设置还是逐个窗体设置？应该是逐个窗体设置好一点？

    //updateUItheme();

    //调用字体
    //int fontId = QFontDatabase::addApplicationFont("/usr/font/wqydkwmh.ttf");
    int fontId = QFontDatabase::addApplicationFont("/usr/font/NotoSansHans-Regular.otf");
    qDebug() << "fontID:" << fontId;
    if(fontId != -1)
    {
        QString msyh = QFontDatabase::applicationFontFamilies(fontId).at(0);
        QFont font0(msyh, 12);
        qDebug() << "font family(): " << font0.family();
        qApp->setFont(font0);
    }

    //弹出加载过渡框
    MessageWin  *msg = new MessageWin();
    msg->setWindowModality(Qt::WindowModal);
    msg->setContent(tr("正在初始化..."));    // "Initializing..."
    msg->setButtonEnable(false);
    msg->show();

    // 注册自定义类型      // TODO: 这个是不是应该放到信号的定义类中？
    qRegisterMetaType<QByteArray>("QByteArray");
    qRegisterMetaType<CPatient>("CPatient");
    qRegisterMetaType<vector<CPatient> >("vector<CPatient>");
    qRegisterMetaType<QVector<QString> >("QVector<QString>");
    qRegisterMetaType<QVector<int> >("QVector<int>");
    qRegisterMetaType<DataTrans::Client>("DataTrans::Client");
    qRegisterMetaType<std::vector<CPatient> >("std::vector<CPatient>");
    qRegisterMetaType<CheckState>("CheckState");
    qRegisterMetaType<string>("string");
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<stPupilInfo>("stPupilInfo");
    qRegisterMetaType<stVisionValue>("stVisionValue");
    qRegisterMetaType<stVisionAbnormal>("stVisionAbnormal");
    qRegisterMetaType<enCaptureError>("enCaptureError");
    qRegisterMetaType<CvPoint3D32f>("CvPoint3D32f");
    qRegisterMetaType<enCameraStat>("enCameraStat");
    qRegisterMetaType<enDistanceState>("enDistanceState");
    qRegisterMetaType<uchar>("uchar");
    qRegisterMetaType<const char *>("const char *");
    qRegisterMetaType<QVector<uchar *>>("QVector<uchar*>");
    qRegisterMetaType<enMeasureStep>("enMeasureStep");
    qRegisterMetaType<enAlgoErrType>("enAlgoErrType");
    qRegisterMetaType<QStringList>("QStringList");
    qRegisterMetaType<enCalcResultState>("enCalcResultState");
    qRegisterMetaType<enAgeRange>("enAgeRange");
    qRegisterMetaType<enSingleDualEyeMode>("enSingleDualEyeMode");
    qRegisterMetaType<CAuthIntf::enAuthIntfErrType>("CAuthIntf::enAuthIntfErrType");
    qRegisterMetaType<enBtProtocol>("enBtProtocol");
    qRegisterMetaType<enBtDevType>("enBtDevType");
    qRegisterMetaType<enSysSignal>("enSysSignal");
    qRegisterMetaType<enWindowType>("enWindowType");
    qRegisterMetaType<QDate>("QDate");
    qRegisterMetaType<Net::Remote::CWebSocket::enConnStat>("Net::Remote::CWebSocket::enConnStat");
    qRegisterMetaType<std::vector<stVisionValue>>("std::vector<stVisionValue>");

    // 检查异常设置文件                             // TODO: 这段代码是干嘛的？！这些奇奇怪怪的操作，需求也不注明！
    //QDir dir("/home/root");
    //QFileInfoList fileList = dir.entryInfoList();
    //foreach (QFileInfo fileInfo, fileList)
    //{
    //    if(fileInfo.fileName().contains("manylinks"))
    //    {
    //        if(!fileInfo.fileName().endsWith("manylinks"))
    //        {
    //            QFile file(fileInfo.absoluteFilePath());
    //            if(file.remove())
    //                qDebug()<<"remove file:"<<fileInfo.absoluteFilePath();
    //        }
    //    }
    //}

    //
    QTextCodec *codec = QTextCodec::codecForLocale();
    if (codec) {
        qDebug() << __PRETTY_FUNCTION__ << ": Local Codec: " << codec->name();
    }

    // 读取wifi设置
    SystemIni::readIni();//for wifi connect

    // 载入全局变量值
    CBusiDataDataTrans data_datatrans;
    WinDataTrans::configToBusiData(data_datatrans);
    WinDataTrans::syncToDataTransmiter(data_datatrans);

    //
    CReport::init();

    // 构建全局对象
    buildGlobalObjs();

    // 全局网络访问管理器的设置
    CUpdate::setNetworkAccessManager(networkManager());
    CAuthIntf::setNetworkAccessManager(networkManager());
    Net::Remote::CMProSysCommunic::setNetworkAccessManager(networkManager());
    Net::Remote::CFileUpload::setNetworkAccessManager(networkManager());
    Common::Net::setNetworkAccessManager(networkManager());

    // 底窗口
    CBaseWindow *win_base = getWinBase();

    // 窗口管理对象
    getWinManage();

    // 初始化界面
    engineerMode = new CEngineerMode(win_base);     // TODO: 这个旧代码里部分全局变量的初始化放到这个窗体的构造函数里，所以这个要先创建。待优化
    g_WinMeasure = new WinMeasure(win_base);
    btWin = new WinBluetooth((win_base), g_Bluetooth);
    homeWin = new MainWindow(win_base);
    historyWin = new WinClinic(win_base);
    winPerRec = new WinPersonalRecord(win_base);
    batchscr = new WinScreen(win_base);
    toolWin = new Tool(win_base);
    resultWin = new Result(win_base);
    perWin = new PersonalInfos(win_base);
    g_WinWifi = new WinWifi(win_base, g_WifiIntf);
    DataTranSet = new WinDataTrans(win_base);
    tmemepic = new themebackground(win_base);
    about = new aboutdevice(win_base);
    Printer = new printerSetting(win_base);
    Music = new MusicSetting(win_base);
    DatePage = new datePage(win_base);
    Eyesight = new eyesightstandard(win_base);
    Setting = new settings(win_base);
    Device = RunningStatus::getInstance(win_base);
    PreviewImage = new previewimage(win_base);
    //lampCalibrate = new LampCalibrate(win_base);
    winDiagnosticStandard = new WinDiagnosticStandard(win_base);
    winDiagnosisSuggestion = new WinDiagnosisSuggestion(win_base);
    winUpdateSetup = new WinUpdateSetup(win_base);
    winUpdateProgress = new WinUpdateProgress(win_base);

    //
    QObject::connect(g_update, &CUpdate::sigShowProgress, winUpdateProgress, &WinUpdateProgress::slotShowProgress, Qt::QueuedConnection);
    QObject::connect(g_update, &CUpdate::sigCurrentFileChanged, winUpdateProgress, &WinUpdateProgress::slotCurrentFileChanged, Qt::QueuedConnection);
    QObject::connect(g_update, &CUpdate::sigProgressChanged, winUpdateProgress, &WinUpdateProgress::slotProgressChanged, Qt::QueuedConnection);
    QObject::connect(g_update, &CUpdate::sigSpeedChanged, winUpdateProgress, &WinUpdateProgress::slotSpeedChanged, Qt::QueuedConnection);

    QObject::connect(winUpdateProgress, &WinUpdateProgress::sigCancelUpdate, g_update, &CUpdate::slotCancelUpdate, Qt::QueuedConnection);

    QObject::connect(g_update, &CUpdate::setAutoCheckUpdate, winUpdateSetup, &WinUpdateSetup::setAutoCheckUpdate, Qt::QueuedConnection);

    //2020.10.12 蓝牙数据处理线程 tao
    //btThread = new QThread;

    flag = false;

    // 构建及设置数据传输串口连接对象
    g_SerialDatatrans = new CSerialDatatrans();     // TODO: 这个应该要放到窗体构建的前面，但是因为 DataTrans::DataTransmiter 的设置在 datatrans 窗体里载入，所以 ...
    g_SerialDatatrans->checkAndSet();

    //关联槽函数
    QObject::connect(homeWin, &MainWindow::sendSIGNAL, this, &WindowsManagers::slot__SysSignalReceived);
    QObject::connect(toolWin, &Tool::sendSIGNAL, this, &WindowsManagers::slot__SysSignalReceived);
    QObject::connect(g_WinMeasure, &WinMeasure::sendSIGNAL, this, &WindowsManagers::slot__SysSignalReceived);
    QObject::connect(g_WinMeasure, &WinMeasure::sendSIGNAL, g_WinWifi, &WinWifi::slot_RecvSysSignal);
    QObject::connect(resultWin, &Result::sendSIGNAL, this, &WindowsManagers::slot__SysSignalReceived);
    QObject::connect(batchscr, &WinScreen::sendSIGNAL, this, &WindowsManagers::slot__SysSignalReceived);
    QObject::connect(batchscr, &WinScreen::batchPrintSig, resultWin, &Result::batchPrint);

    QObject::connect(this, &WindowsManagers::senderSignal, resultWin, &Result::receieve); //将历史界面中的信息导入到结果界面中

    QObject::connect(resultWin, &Result::sigUpLoadData, g_uploadThread, &UpLoadThread::slotUploadData);    //自动打印
    QObject::connect(g_uploadThread, &UpLoadThread::sigUpLoadDataFeedback, resultWin, &Result::slotUpLoadDataFeedback);

    QObject::connect(historyWin, &WinClinic::sendSIGNAL, this, &WindowsManagers::slot__SysSignalReceived);
    QObject::connect(historyWin, &WinClinic::batchPrintSig, resultWin, &Result::batchPrint);
    QObject::connect(historyWin, &WinClinic::sigUpLoadData, g_uploadThread, &UpLoadThread::slotUploadData);
    QObject::connect(g_uploadThread, &UpLoadThread::sigUpLoadDataFeedback, historyWin, &WinClinic::slotUpLoadDataFeedback);

    QObject::connect(winPerRec, &WinPersonalRecord::batchPrintSig, resultWin, &Result::batchPrint);
    QObject::connect(winPerRec, &WinPersonalRecord::sigUpLoadData, g_uploadThread, &UpLoadThread::slotUploadData);
    QObject::connect(g_uploadThread, &UpLoadThread::sigUpLoadDataFeedback, winPerRec, &WinPersonalRecord::slotUpLoadDataFeedback);

    QObject::connect(batchscr, &WinScreen::sigUpLoadData, g_uploadThread, &UpLoadThread::slotUploadData);
    QObject::connect(g_uploadThread, &UpLoadThread::sigUpLoadDataFeedback, batchscr, &WinScreen::slotUpLoadDataFeedback);

    QObject::connect(batchscr, &WinScreen::batchImportSig, g_uploadThread, &UpLoadThread::clientListRequest);
    QObject::connect(g_uploadThread, &UpLoadThread::requestClientListFeedback, batchscr, &WinScreen::slot_batchImportFeedback);

    QObject::connect(g_WinMeasure, &WinMeasure::sendBlueToothData, g_uploadThread, &UpLoadThread::handleBlueToothCmd);
    QObject::connect(g_uploadThread, &UpLoadThread::cameraCtl, g_WinMeasure, &WinMeasure::slot_upLoadWork_MeasureCtrl, Qt::QueuedConnection);

    //QObject::connect(btWin, &WinBlueTooth::sigGetBlueToothData, g_uploadThread, &UpLoadThread::handleBlueToothData);
    //QObject::connect(g_uploadThread, &UpLoadThread::writeBlueToothData, btWin, &WinBlueTooth::writeData);

    QObject::connect(g_uploadThread, &UpLoadThread::sigDataTransmiterGetNewSubject, this, &WindowsManagers::slot_uploadThread_DataTransmiterGetNewSubject, Qt::QueuedConnection);
    QObject::connect(g_uploadThread, &UpLoadThread::sigDataTransmiterOperationLocked, this, &WindowsManagers::slot_uploadThread_DataTransmiterOperationLocked, Qt::QueuedConnection);

    QObject::connect(engineerMode, &CEngineerMode::sigDevActivateStatReceived, this, &WindowsManagers::slot_DevActivateStatReceived, Qt::QueuedConnection);

    //
    g_uploadThread->setBtConnection(g_Bluetooth->getBtDatatrans());
    g_uploadThread->setSerialDatatrans(g_SerialDatatrans);

    g_uploadThread->moveToThread(threadUploadWork);
    threadUploadWork->start();

    //btWin->moveToThread(btThread);  //2020.10.12  tao
    //btThread->start();  //2020.10.12  tao

    QThread *printThead = new QThread;
    printerTransmit *printTrans = new printerTransmit;
    QObject::connect(resultWin, &Result::printSig, printTrans, &printerTransmit::slotPrintTicket);
    QObject::connect(printTrans, &printerTransmit::sigDataSendFinished, resultWin, &Result::slot_printTrans_DataSendFinished);

    //QObject::connect(printTrans, &printerTransmit::sendToBtPrint, btWin, &WinBlueTooth::writeData);   //2020.11.6  tao
    printTrans->setBtConnection(g_Bluetooth->getBtPrinter());        // TODO: 如果不存在主动连接，就将被动连接作为打印机连接？龙岩爱尔使用蓝牙打印输出作为结果数据来源

    printTrans->moveToThread(printThead);
    printThead->start();

    //QObject::connect(g_WinWifi, &WinWifi::sigSendSysSignal, this, &WindowsManagers::slot__SysSignalReceived);

    // add by wim
    bMonitor = BatteryMonitor::instance();      //查询充电插孔信息
    QObject::connect(bMonitor, &BatteryMonitor::sigBatteryChanged, WgtStatusBar::instance(), &WgtStatusBar::slot_BatteryChanged, Qt::QueuedConnection);

#if (1 == OS_TYPE)
    QObject::connect(Music, &MusicSetting::sendSIGNAL, toolWin, &Tool::sendSIGNAL);
#endif

    // 定时检查是否已联网，若是，则同步网络时间，且成功后或失败若干次后停止（rk3568系统已经存在联网自动同步，不需要此操作）
#if (1 == OS_TYPE)
    timerNetTimeSync = new QTimer(this);
    QObject::connect(timerNetTimeSync, &QTimer::timeout, DatePage, &datePage::slot_updateNetworkTime, Qt::QueuedConnection);
    timerNetTimeSync->start(2500);
#endif
    /* 已在 WiFi 接口增加 sigConnectedStateChanged() 信号，不需定期检查 */

    // 进程守护消息的发送        // TODO: 待完善
    //PipeWrite *pipe = new PipeWrite;
    //pipe->start();

    //
    mLogin = new loginWin(win_base);
    mLogin->setMode(loginWin::login);
    QObject::connect(mLogin, &loginWin::showMainwindow, this, &WindowsManagers::slot__SysSignalReceived);
    mLogin->setVisible(false);

    mysql = MySQLitePatients::getInstance();
    QObject::connect(mysql, &MySQLitePatients::progressSig, getWinManage(), &CWinManage::showProgress);

    getWinManage()->initKeyboard(win_base);

    //添加界面到窗口控制器 getWinManage()
    getWinManage()->addWidget(g_WinMeasure);
    getWinManage()->addWidget(btWin);
    getWinManage()->addWidget(homeWin);
    getWinManage()->addWidget(historyWin);
    getWinManage()->addWidget(winPerRec);
    getWinManage()->addWidget(batchscr);
    getWinManage()->addWidget(toolWin);
    getWinManage()->addWidget(resultWin);
    getWinManage()->addWidget(g_WinWifi);
    getWinManage()->addWidget(mLogin);
    getWinManage()->addWidget(perWin);
    getWinManage()->addWidget(DataTranSet);
    getWinManage()->addWidget(tmemepic);
    getWinManage()->addWidget(about);
    getWinManage()->addWidget(DatePage);
    getWinManage()->addWidget(Printer);
    getWinManage()->addWidget(Eyesight);
    getWinManage()->addWidget(Music);
    getWinManage()->addWidget(Setting);
    getWinManage()->addWidget(Device);
    getWinManage()->addWidget(PreviewImage);
    getWinManage()->addWidget(engineerMode);
    //getWinManage()->addWidget(lampCalibrate);
    getWinManage()->addWidget(winDiagnosticStandard);
    getWinManage()->addWidget(winDiagnosisSuggestion);
    getWinManage()->addWidget(winUpdateSetup);
    getWinManage()->addWidget(winUpdateProgress);


//    ProgressWindow *progressWin = new ProgressWindow;
//    getWinManage()->initProgressWin(progressWin);

    msg->reject();
    msg->hide();
    msg->deleteLater();

    //
    powerCtl = PowerControl::getlInstance();

    // power save
    //QObject::connect(powerCtl, &PowerControl::toggleWiFiState, g_WinWifi, &WinWifi::slotSetIsConnected);     //2020.9.13  tao 屏蔽

    //
    QObject::connect(this, &WindowsManagers::sigSavePrintscreen, this, &WindowsManagers::slot_this_SavePrintscreen, Qt::QueuedConnection);
    QObject::connect(this, &WindowsManagers::sigStartupFinished, this, &WindowsManagers::slot_this_StartupFinished, Qt::QueuedConnection);
    QObject::connect(this, &WindowsManagers::sigPhysicButtonPressed, this, &WindowsManagers::slot_this_PhysicButtonPressed, Qt::QueuedConnection);  // TODO: 不同窗体的物理按键事件统一使用同一个信号槽连接？

    // 状态栏
    win_base->isShowStatusBar = true;
    WgtStatusBar::instance()->setCurrParent(win_base);
    //WgtStatusBar::getInstance()->raise();

    //
#if (1 == OS_TYPE || 2 == OS_TYPE)
    QObject::connect(g_WinMeasure, &WinMeasure::sendSIGNAL, btWin, &WinBluetooth::slotBaseBoardBtConnStateChanged);
#endif

    // “万灵 MPro 推送的接收” 对象的创建及设置
    g_mproSysPushSvcCommunic = new Net::Remote::CMProSysPushSvcCommunic();
    g_mproSysPushSvcCommunic->setWorkThread(g_commonThread);

    QObject::connect(g_mproSysPushSvcCommunic->webSocket(), &Net::Remote::CWebSocket::sigConnStatChanged, DataTranSet, &WinDataTrans::slot_mproSysPushSvc_ConnStatChanged, Qt::QueuedConnection);
    QObject::connect(g_mproSysPushSvcCommunic, &Net::Remote::CMProSysPushSvcCommunic::sigReceivedOutpatientArchive, this, &WindowsManagers::slot_mproSysPushSvc_ReceivedOutpatientArchive, Qt::QueuedConnection);
    QObject::connect(g_mproSysPushSvcCommunic, &Net::Remote::CMProSysPushSvcCommunic::sigDevActivateStatReceived, engineerMode, &CEngineerMode::slot_mproSysPushSvc_DevActivateStatReceived, Qt::QueuedConnection);

    //
    CDataIntfHuaYi::setWorkerThread(g_commonThread);

    // 公用定时器
    m_timerPublic = new QTimer();

    QObject::connect(m_timerPublic, &QTimer::timeout, this, &WindowsManagers::slot_timerPublic_timeout, Qt::QueuedConnection);

    m_timerPublic->setInterval(1000);
    m_timerPublic->start();

    // 清理临时数据                   // TODO: 这个放到这里合适吗？
    //system("rm /media/photo/P00*_* -r");      // 清理失败存图（v1.3 v1.4 旧代码是在 RunningStatus 构造时执行这个操作，暂时移出到这里）

    // 刷新 存储 使用率
    RunningStatus::refreshStorageRate();

}

WindowsManagers::~WindowsManagers()
{
    CSoundIntf::releaseInstance();

    g_mproSysPushSvcCommunic->setIsOpened(false);
    g_mproSysPushSvcCommunic->deleteLater();
    g_mproSysPushSvcCommunic = nullptr;

}

void WindowsManagers::init()
{
    // 先查询一次电量
    QTimer::singleShot(500, this, []() {
        g_WinMeasure->queryInfosFromBaseBoard();
    });

#if (2 == BLUETOOTH_TYPE)
    // rk3568平台，关闭旧蓝牙电源
    QTimer::singleShot(1000, this, []() {
        MySerialPort::instance()->write(CloseBT);
    });
#endif

    // GPIO3-16控制声音
#if OS_TYPE==1
    system("echo 80 > /sys/class/gpio/export");
    system("echo out > /sys/class/gpio/gpio80/direction");
    system("echo 0 > /sys/class/gpio/gpio80/value");            // NOTE: 天嵌平台关闭声卡
#endif

    //
    emit sigConfigLoaded();

    // 相机初始化
    /*enCameraStat ret =*/ g_WinMeasure->init_SDK();
//    qDebug()<< "init is success = " << ret;

}

void WindowsManagers::slotAboutToExit()
{
    if (isExitExecuted)
        return;

    //
    MySerialPort::instance()->write(closeUSandIR);

    g_CameraIntf->disconnectCamera();
    g_CameraIntf->uninitCamera();

    MySQLitePatients *db = MySQLitePatients::getInstance();
    if (db) {
        delete db;
        db = nullptr;
    }

    PowerControl::setScreenBrightnessDark();

    appSetting::sync();

    //
    isExitExecuted = true;
}

WindowsManagers *WindowsManagers::getInstance()
{
    if (!WindowsManagers::wm) {
        WindowsManagers::wm = new WindowsManagers();
    }
    return WindowsManagers::wm;
}

// 总事件过滤器
bool WindowsManagers::eventFilter(QObject *_obj, QEvent *_event)    /* 同一个事件信号，可能由多个对象发射到此处，要注意避免重复处理 */
{
    //qDebug() << "WindowsManagers::eventFilter(): obj_name = " << _obj->objectName() << ", event_type = " << _event->type();
    bool is_processed = false;

    //
    if (_event->type() == QEvent::KeyPress) {
        //qDebug() << "WindowsManagers::eventFilter(): obj_name = " << _obj->objectName() << ", event_type = " << _event->type();

        QKeyEvent *key_event = dynamic_cast<QKeyEvent *>(_event);
        if (key_event) {
            //qDebug() << QString("got key: 0x%1").arg(key_event->key(), 8, 16, QLatin1Char('0'));

            // 右上角物理按键事件侦听
            if (key_event->key() == (int)Qt::Key_Escape) {
                //logDebug("WindowsManagers::eventFilter(): Key_Escape event is got", CGlobal::LOG_SYS);
                qDebug() << "Key_Escape event is got";

                static const int ESC_KEY_FILTER_ID = Util::CEventDelayFilter::getInstance()->registerDelayFilter(1000);

                int count = Util::CEventDelayFilter::getInstance()->invokeDelayFilter(ESC_KEY_FILTER_ID);           // 延时防抖过滤
                if (1 == count) {
                    //
                    QWidget *curr_widget = getWinManage()->getCurrentWin();
                    if (curr_widget) {
                        QString obj_name = curr_widget->objectName();
                        enWindowType win_type = getWinManage()->getWindowTypeByName(obj_name);

                        logDebug(QString("WindowsManagers::eventFilter(): Key_Escape event is valid, obj_name = '%1'").arg(obj_name), CGlobal::LOG_SYS);

                        if (isPrintScreen) {                                        // 系统截屏，优先处理
                            logDebug("sending sigSavePrintscreen()", CGlobal::LOG_SYS);
                            emit sigSavePrintscreen();
                        } else {
                            // 若当前窗口是这几个窗口，则发送物理按键按下消息      /* 注意：这个信号有多个接收者，所以需要接收者判断自己当前是否需要处理该信号 */  // TODO: 应该定义不同的信号发给不同的接收者吗？
                            if (WIN_HOME == win_type || WIN_MEASURE == win_type || WIN_RESULT == win_type || WIN_SCREEN == win_type ||
                                    WIN_PER == win_type || WIN_CLINIC == win_type || WIN_PER_REC == win_type
                                    ) {
                                logDebug("sigPhysicButtonPressed() emitted", CGlobal::LOG_SYS);

                                if (!getWinManage()->getIsShowingKeyboard()) {
                                    emit sigPhysicButtonPressed();
                                }
                            } else {
                                logDebug(QString("WindowsManagers::eventFilter(): Key_Escape event, but not used, obj_name = %1").arg(_obj->objectName()), CGlobal::LOG_SYS);
                            }
                        }

                        //
                        is_processed = true;
                    }

                    // 若是息屏或低功耗状态，则点亮屏幕
                    powerCtl->reset();

                } else {
                    logDebug(QString("WindowsManagers::eventFilter(): Key_Escape event not valid, obj_name = %1, count = %2")
                             .arg(_obj->objectName()).arg(count), CGlobal::LOG_SYS);
                }
            }
        }
    } else if (_event->type() == QEvent::MouseButtonRelease) {
        //qDebug() << "WindowsManagers::eventFilter(): obj_name = " << _obj->objectName() << ", event_type = " << _event->type();

        // 用户点击屏幕后，通知系统的电源管理模块      // TODO: 这里没有做重复过滤，同一个事件，可能触发多次，降低系统效率？
        powerCtl->reset();

    }

    //
    return is_processed;
}

// 槽函数：截屏
void WindowsManagers::slot_this_SavePrintscreen()
{
    //
    QString disk_path = Util::CUDisk::getPath();
    if (disk_path.length() > 0) {
        bool succ = false;
        QString msg = "";

        // 创建目录
        QDir sourceDir(QString("%1/PrintScreen").arg(disk_path));
        if(!sourceDir.exists())
            sourceDir.mkdir(QString("%1/PrintScreen").arg(disk_path));

        QString file_path = QString("%1/PrintScreen/%2.jpg").arg(disk_path).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

#if (OS_TYPE == 1 || OS_TYPE == 2)
        //QPixmap picture = QGuiApplication::primaryScreen()->grabWindow(getWinManage()->getCurrentWin()->winId());
        QPixmap picture = QGuiApplication::primaryScreen()->grabWindow(getWinBase()->winId());
#elif (OS_TYPE == 3)
        //QPixmap picture = QGuiApplication::primaryScreen()->grabWindow(0);                                    /* 实测，得不到图像 */
        //QPixmap picture = QGuiApplication::primaryScreen()->grabWindow(QApplication::desktop()->winId());     /* 实测，得不到图像 */
        //QPixmap picture = QApplication::desktop()->grab();                                                    /* 实测，得到的是空白桌面的图像 */
        //QPixmap picture = getWinManage()->getCurrentWin()->grab();                                            /* 实测，可得到窗口图像 */
        QPixmap picture = QApplication::activeWindow()->grab();
#endif

        if (!picture.isNull()) {
            succ = picture.save(file_path, "jpg");
            if (succ) {
                Util::CUDisk::sync();
            }
            msg = (succ ? tr("保存路径:") + file_path : tr("保存截屏失败！")); // "Save path:" "failed to save screen!"
        } else {
            msg = tr("截屏失败！");  // "grab screen failed!"
        }

        //
        getWinManage()->showSuspensionPrompt(msg, 3000);
    } else {
        QString text = tr("未检测到U盘!");   // "U disk was not detected!"
        getWinManage()->showSuspensionPrompt(text, 3000);
    }
}

void WindowsManagers::slot_this_StartupFinished()
{
    // OTA 升级的处理
    if (OtaUpdate::isNeedOtaRestore()) {         // 如果 OTA 更新后未执行配置还原
        /* 这里不检查电量。
         * 因为正常情况下，这是 OTA 升级后的第一次重启，对于电量的判断，OTA 升级前已执行，而且这个还原的数据量很少，对电量要求低。
         */
        WindowsManagers::configOtaUpdate();
        if (OtaUpdate::runOtaRestore()) {
            // 若启动成功，退出本程序
            this->closeApp();
            return;
        } else {
            // 若启动失败，显示提示
            QString msg = tr("启动数据还原失败");   // "Failed to start data restoration"
            getWinManage()->showMsgWin(msg);
        }
    }

    // 最大记录数检查及提示
    if (mysql->getMeasureRecordsCount() > MAX_RECORD_COUNT * 0.8) {
        getWinManage()->showMsgWin(tr("历史数据过多，请及时上传或导出，然后删除部分数据")); // "Too much historical data. Please upload or export it, then delete some"
    }

    // PC 平台，网络默认已连接，发送 WiFi 已连接事件
#if (OS_TYPE == 2)
    QTimer::singleShot(3000, this, []() {
        g_WifiIntf->emitConnectedStateChanged(true);
    });
#endif

    // 开机后发送一次转灯指令，使相机触发脚电位与触发前一致（否则第一次转灯的第一次触发会失败）
    g_WinMeasure->sendTurnLampCmd(3);

    // U 盘插拔侦听
    g_usbDriveMonitor->start();

    //
    //WgtStatusBar::instance()->setTrialDesc(CWinManage::getTrialDesc());

}

void WindowsManagers::slot_this_PhysicButtonPressed()
{
    QWidget * const curr_win = getWinManage()->getCurrentWin();
    if (curr_win == homeWin) {
        homeWin->slotPhysicButtonPressed();
    } else if (curr_win == resultWin) {
        resultWin->slotPhysicButtonPressed();
    } else if (curr_win == perWin) {
        perWin->slotPhysicButtonPressed();
    } else if (curr_win == batchscr) {
        batchscr->slotPhysicButtonPressed();
    } else if (curr_win == winPerRec) {
        winPerRec->slotPhysicButtonPressed();
    } else if (curr_win == historyWin) {
        historyWin->slotPhysicButtonPressed();
    } else if (curr_win == g_WinMeasure) {
        g_WinMeasure->slotPhysicButtonPressed();
    } else {
        getWinManage()->showSuspensionPrompt(tr("当前窗口未支持拍摄按键！"));   // "Current window not support the shooting button!"
    }
}

void WindowsManagers::slot_WifiIntf_OpenedStateChanged(bool _is_opened)
{
    qInfo() << QString("%1: entered, _is_opened = %2").arg(__PRETTY_FUNCTION__).arg(Util::bool2str(_is_opened));

    //
    WgtStatusBar::instance()->setIsWifiOpened(_is_opened);
}

//
void WindowsManagers::slot_WifiIntf_ConnectedStateChanged(bool _is_connected)
{
    qInfo() << QString("%1: entered, _is_connected = %2").arg(__PRETTY_FUNCTION__).arg(Util::bool2str(_is_connected));

    // 联网后，检查更新试用机信息（“授权信息”）
    static bool is_timer_setted = false;        // 防止定时器被重复设置
    if (_is_connected) {
        // TODO: 确保主窗口已显示？


        //
        QTimer::singleShot(10000, this, [this]() {      // 延时一段时间再访问网络，否则可能发生 QNetworkReply::UnknownNetworkError 或 QNetworkReply::HostNotFoundError 错误？
            //
            if (!is_timer_setted) {
                return;
            }

            // 若已查询成功，则不再查询
            //if (authCheckingState_Succ == engineerMode->getAuthCheckingState()) {
            //    return;
            //}

            // 授权（试用机）状态查询
            engineerMode->syncTrialStat();

            // 设备激活状态同步
            // NOTE: 试用机状态查询的结果已包含此信息，略过

            // 若设备未激活，显示激活二维码对话框
            if (!CGlobal::isDevActivated) {
                // 显示激活二维码对话框
                globalService()->showDevActivateDialog();
            }

            //
            is_timer_setted = false;
        });

        //
        is_timer_setted = true;
    } else {
        if (is_timer_setted) {
            is_timer_setted = false;
        }
    }

    // 自动设置 “万灵 MPro 系统推送的接收” 模块的状态（可能启动，也可能关闭）
    autoSetStatOfMProSysPushSvcCommunic(_is_connected);

    //
    WgtStatusBar::instance()->setIsWifiConnected(_is_connected);

}

void WindowsManagers::slot_KbReader_Getline(QByteArray _line_bytes)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, _line_bytes = \n" << _line_bytes;

    // 逻辑错误检查：当前窗体须是当前注册的扫码帧听者
    if (getWinManage()->getCurrentWin() != m_qrCodeScannedListener) {
        getWinManage()->showSuspensionPrompt("LogicError: Current QrCodeListener not current window!");
        return;
    }

    // 扫码内容不可为空
    if (_line_bytes.length() == 0 || _line_bytes.at(0) == '\n') {
        getWinManage()->showSuspensionPrompt(tr("扫码得到的内容为空！")); // "The content obtained by scanning the code is empty!"
        return;
    }

    // 扫码内容最短长度检查，及调用条码内容处理过程
    const int MIN_BARCODE_LEN = 2;
    bool is_valid  = (_line_bytes.length() > MIN_BARCODE_LEN
            //&& (barcodeData[barcodeData.length() - 1] == QChar('\r') || barcodeData[barcodeData.length() - 1] == QChar('\n'))
            );
    if (is_valid) {
        doOn_QrCode_ReceivedCode(_line_bytes);
    } else {
        getWinManage()->showSuspensionPrompt(tr("二维码内容过短！"));    // "The QR code content to short!"
    }
}

void WindowsManagers::slot_timerPublic_timeout()
{
    // 若设备未激活，且已联网，定时查询设备激活状态，直到已激活
    bool not_activated = !CGlobal::isDevActivated;
    if (not_activated && g_WifiIntf->getIsConnected() && !engineerMode->isVisible()) {
        static constexpr int INTERVAL = 60;     // 间隔（定时事件次数）
        static int count = 0;

        count++;
        if (count >= INTERVAL) {
            engineerMode->syncTrialStat();
            count = 0;
        }
    }

    // TODO: 若上次查询时间距当前时间超过一天，立即查询一次


}

void WindowsManagers::slot_DevActivateStatReceived(bool _is_activated)
{
    if (_is_activated != CGlobal::isDevActivated) {
        //
        CGlobal::isDevActivated = _is_activated;
        CGlobal::saveConfs();

        //
        WgtStatusBar::instance()->updateTitle();

        //
        if (_is_activated) {
            //
            hideDevActivateDialog();

            //
            getWinManage()->showMsgWin(tr("设备已通过云端激活！"));    // "The device has been activated through the cloud!"
        } else {
            //
            getWinManage()->showMsgWin(tr("设备已被云端置为未激活！"));    // "The device has been set as inactive by the cloud!"
        }
    }
}

void WindowsManagers::slot_mproSysPushSvc_ReceivedOutpatientArchive(Net::Remote::stOutpatientArchive _archive)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__
             << "(): entered, number = " << _archive.treatmentNumber << ", system = " << _archive.system;

    //
    if (_archive.isOutpatient()) {
        historyWin->slot_mproSysPushSvc_ReceivedOutpatientArchive(_archive);
    } else {
        batchscr->slot_mproSysPushSvc_ReceivedOutpatientArchive(_archive);
    }
}

void WindowsManagers::slot_usbDriveMonitor_UsbDriveOnlineChanged(bool _is_on)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, _is_on = " << Util::bool2str(_is_on);

    //
    WgtStatusBar::instance()->setIsUsbDrivePlugged(_is_on);
}

void WindowsManagers::doOn_QrCode_ReceivedCode(QByteArray _line_bytes)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, _line_bytes = \n" << _line_bytes;

    // NOTE: 此处得到的二维码数据里的，并未去除换行符

    // 扫码事件回调
    if (m_callbackCanProcessQrCode) {
        bool is_can_continue = m_callbackCanProcessQrCode();
        if (!is_can_continue) {
            return;
        }
    }

    /*
     * 视筛扫码后，据二维码内容判定二维码类型，并根据二维码类型走不同的处理流程：
     *   若 以 http 开头        → “华谊”二维码的处理流程
     *   若 以 @ 结尾           → “山东勤成”二维码的处理流程
     *   若 逗号数量 ≥ 4        → “安徽筛查系统”二维码的处理流程  注释：万灵云端的的二维码内容是经过百分号编码的，只有反编码后才有逗号
     *   否则                  → 万灵云端二维码的处理流程
    */

    // 判断二维码所属系统
    enQrCodeSystem code_sys = enQrCodeSystem::Unknown;
    do {
        // 判定是否“华谊”的二维码         // NOTE: 判定逻辑：是否以 "http" 开头
        bool is_code_huayi = CDataIntfHuaYi::isMyQrCode(_line_bytes);
        if (is_code_huayi) {
            code_sys = enQrCodeSystem::Huayi;
            break;
        }

        // 判定是否“山东勤成”的二维码       // NOTE: 判定逻辑：是否以 '@' 结尾
        bool is_code_shandongqincheng = CDataIntfOther::shanDongQinCheng_isMyQrCode(_line_bytes);
        if (is_code_shandongqincheng) {
            code_sys = enQrCodeSystem::ShanDongQinCheng;
            break;
        }

        // 判定是否“安徽筛查系统”的二维码     // NOTE: 判定逻辑：逗号个数大于等于4（万灵云端的码只有百分号反编码后才有逗号）
        bool is_code_anhuiscreen = CDataIntfAnHuiScreen::isMyQrCode(_line_bytes);
        if (is_code_anhuiscreen) {
            code_sys = enQrCodeSystem::AnHuiScreen;
            break;
        }

        //
        code_sys = enQrCodeSystem::Manylinks;

    } while (false);
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): The system to which the QR code belongs is '" << enumToName_QrCodeSystem(code_sys) << "'";

    //
    if (enQrCodeSystem::Unknown == code_sys) {
        getWinManage()->showMsgWin(tr("未支持的二维码数据结构！"));   // "Unsupported QR code data structure!"
        return;
    }

    // 根据不同的二维码所承载的数据的格式来处理
    bool is_need_show_per = false;       // 是否需要显示被测者信息界面       // TODO: 重构：不需显示被测者信息页面，直接查询
    if (enQrCodeSystem::Huayi == code_sys) {                        // “华谊”的二维码数据格式的处理
        // 通知受检者信息模块调用“华谊”数据接口查询受检者信息
        perWin->doOnReceivedHuayiQrCode(_line_bytes);           // TODO: 此处理过程应从 被测者 信息模块剥离？在获得被测者信息查询结果之前，没必要先显示窗口？最后的窗口显示过程统一到本过程？
    } else if (enQrCodeSystem::AnHuiScreen == code_sys) {           // “安徽筛查系统”的二维码数据格式的处理
        // 由条码内容得到被测者实体对象
        CPatient pat;
        pat.reset();

        QString err_msg;
        bool is_succ_parse = CDataIntfAnHuiScreen::qrCodeToPatient(_line_bytes, pat, err_msg);
        if (!is_succ_parse) {
            getWinManage()->showMsgWin(tr("二维码内容解析失败") + tr("：") + err_msg);   // "QR code content parsing failed"
            return;
        }

        // 定为批量筛查码
        pat.isBatch = true;

        // 得到扫码后的受检者对象后的处理
        doOn_QrCode_ReceivedPatient(pat);
    } else if (enQrCodeSystem::ShanDongQinCheng == code_sys) {      // “山东勤成”二维码的处理
        // 由条码内容得到被测者实体对象
        CPatient pat;
        pat.reset();

        QString err_msg;
        bool is_succ_parse = CDataIntfOther::shanDongQinCheng_parseQrCode(_line_bytes, pat, err_msg);
        if (!is_succ_parse) {
            getWinManage()->showMsgWin(tr("二维码内容解析失败") + tr("：") + err_msg);   // "QR code content parsing failed"
            return;
        }

        // 定为批量筛查码
        pat.isBatch = true;

        // 得到扫码后的受检者对象后的处理
        doOn_QrCode_ReceivedPatient(pat);
    } else {                                                        // 万灵帮桥的二维码数据格式的处理
        // 由条码内容得到被测者实体对象
        CPatient pat;
        pat.reset();
        QString code_decoded;
        QString err_msg;
        QString qr_code = QString::fromUtf8(_line_bytes);
        enQrCodeType code_type = PersonalInfos::barcodeDataToEntity(qr_code, code_decoded, pat, err_msg);
        bool is_code_valid = (code_type != qrCodeType_Unknown);
        if (is_code_valid) {
            if (qrCodeType_CSV == code_type || qrCodeType_JSON == code_type) {    /* 如果二维码包含完整被测者信息，则直接开始测量 */
                ////
                //QString num = (qrCodeType_Number == code_type ? code_decoded : pat.patientid);
                //
                //// 查询数据库中是否已存在，若已存在，则取消
                //vector<CPatient> mypats = MySQLitePatients::getInstance()->findRecordByPatientid(num);
                //bool exists = (mypats.size() > 0);
                //if (exists) {       // 若编号已存在       // TODO: 若为门诊，则编号可重复，若为筛查，则不允许编号重复？
                //    bool is_replace = false;
                //    if (code_type != qrCodeType_Number) {    // 若条码含完整信息，则询问是否覆盖
                //        QString msg_sub = (mypats.at(0).isTest ? tr("，且已测量") : ""); // ", and has been measured"
                //        QString msg = tr("编号已存在%1！是否覆盖？").arg(msg_sub); // "Number already exists%1! Overwrite it?"
                //        is_replace = getWinManage()->showNoticeWin(msg);
                //        if (is_replace) {               // 若选择覆盖，则显示条码中的信息，并记下已有记录的 id
                //            pat.id = mypats[0].id;      // 记下已有记录 id，使之后的保存替换已有记录
                //        } else {                        // 若选择不覆盖，则显示数据库的信息
                //            pat.cloneFrom(mypats[0]);
                //        }
                //    } else {                    // 若条码仅含编号，则显示数据库的数据
                //        pat.cloneFrom(mypats[0]);
                //    }
                //}
                // TODO: 门诊和筛查都不禁止编号重复？

                // 确保为筛查记录              // NOTE: 注意：云端的二维码，不能固定为批量筛查码，因为门诊的和学校的都有。见 DataTrans::batchClient2Patient()
                //pat.isBatch = true;

                // 得到扫码后的受检者对象后的处理
                doOn_QrCode_ReceivedPatient(pat);
            } else if (qrCodeType_Number == code_type){   // 仅含编号的情况，见 PersonalInfos::doAfterGetEntityFromBarcode()
                // 调用被测者信息界面处理基本信息查询过程      // TODO: 重构：不需显示被测者信息页面，直接查询
                is_need_show_per = true;
            } else {
                getWinManage()->showMsgWin(tr("二维码内容解析失败：")     // "QR code content parsing failed:"
                                           + "\n" + tr("未知的二维码内容格式类型"));      // "Unknown QR code content format type"
            }
        } else {
            getWinManage()->showMsgWin(tr("二维码内容解析失败：") + "\n" + err_msg);  // "QR code content parsing failed:"
        }
    }

    //
    if (is_need_show_per) {
        // 显示被测者信息界面
        WinMeasure::setOperationMode(operationMode_InputMeasure);
        QString qr_code = QString::fromUtf8(_line_bytes);
        PersonalInfos::showPersonalInfo(PersonalInfos::modeFlag_FromBarcode, patientSource_Scanning, nullptr, qr_code);
    }
}

void WindowsManagers::doOn_QrCode_ReceivedPatient(const CPatient &_pat, bool _is_number_only, QWidget *_old_win_not_keep)
{
    //
    CPatient pat_new;
    pat_new.cloneFrom(_pat);

    // 检查受检者是否重复
    vector<CPatient> mypats = MySQLitePatients::getInstance()->findRecordByPatientid(_pat.patientid);
    bool exists = (mypats.size() > 0);
    if (exists) {       // 若编号已存在
        const CPatient &pat_old = mypats.at(0);
        bool is_batch_old = pat_old.isBatch;

        // 检查基本信息是否一致
        bool is_same = true;
        if (!_is_number_only) {
            is_same = _pat.isBasicInfoSame(pat_old);
        }

        // 若基本信息一致，则询问是否置顶
        if (is_same) {
            QString msg = tr("已有相同档案存在，是否置顶？");
            // "A matching patient profile already exists. Would you like to pin this entry to the top of the list?"
            bool yes = getWinManage()->showNoticeWin(msg);
            if (yes) {
                // 置顶（更新记录的时间，并修改展示此记录的界面的排序方式为按时间降序）
                if (!is_batch_old) {
                    // 批量修改
                    PersonalInfos::pinToTop(_pat.patientid);
                } else {
                    // 只改新记录
                    pat_new.creattime = QDateTime::currentDateTime().toString(CPatient::dateTimeFormat());
                }

                // 修改 UI 的排序方法
                if (!is_batch_old) {
                    WinPersonalRecord::setConfig_SortType(enSortType::ByTimeDsc);
                    WinClinic::setConfig_SortType(enSortType::ByTimeDsc);
                } else {
                    WinScreen::setConfig_SortType(enSortType::ByTimeDsc);
                }
            } else {
                // do nothing
            }
        } else {
            // 否则询问是否更新
            QString msg = tr("已有相同档案存在，检测到档案信息有变化，是否更新原档案？");
            // "A matching patient profile already exists, and differences were found in the new profile. Would you like to update the existing record?"
            bool yes = getWinManage()->showNoticeWin(msg, tr("确定"), tr("取消"));
            if (yes) {
                // 更新基本信息
                PersonalInfos::editTesteeInfoOfNumber(_pat.patientid, pat_new);

                // 置顶
                pat_new.creattime = QDateTime::currentDateTime().toString(CPatient::dateTimeFormat());

                // 修改 UI 的排序方法
                if (!is_batch_old) {
                    WinPersonalRecord::setConfig_SortType(enSortType::ByTimeDsc);
                    WinClinic::setConfig_SortType(enSortType::ByTimeDsc);
                } else {
                    WinScreen::setConfig_SortType(enSortType::ByTimeDsc);
                }
            } else {
                // 若取消更新，则取消扫码操作
                //return;

                // 拷贝旧的基本信息
                PersonalInfos::cloneDataObjOfUiFields(pat_old, pat_new);

                pat_new.creattime   = pat_old.creattime;
                pat_new.isBatch     = pat_old.isBatch;
            }
        }

        // 将旧实体的 ID 赋值给新实体，否则保存时必然生成新的记录
        pat_new.id = pat_old.id;
    }

    // 开启测量界面
    WinMeasure::setOperationMode(operationMode_InputMeasure);
    getWinManage()->openMeasureWin(pat_new, patientSource_Scanning, _old_win_not_keep);
}

// 检查 U 盘是否有更新，若有，则询问；返回更新是否完成
bool WindowsManagers::checkUdiskAndUpdate(QString *_msg)
{
//#if (OS_TYPE == 2)
//#endif
    QString udisk_path = Util::CUDisk::getPath();

    if(udisk_path == ""){
        QString msg = tr("没找到 U 盘");    // "UDisk not found"
        qDebug() << "UDisk Update failed: " << msg;
        if (_msg) {
            *_msg = msg;
        }
        return false;
    }

    qDebug()<<"find Udisk:"<<udisk_path;

# if (OS_TYPE == 1)
    QString app_file_name = "main";
# else
    QString app_file_name = "screener";
# endif

    QString app_src_path = udisk_path + QDir::separator() + app_file_name;
    if (!QFile::exists(app_src_path)) {
        QString msg = tr("在 U 盘中没找到文件 “%1”").arg(app_file_name);    // "File \"%1\" not found on UDisk"
        qDebug() << "file '" << app_src_path << "' not exists, U-Disk update cancel";
        if (_msg) {
            *_msg = msg;
        }
        return false;
    }

    // 检查 U 盘里的文件和设备内部的文件的 md5 是否一致，若一致，则忽略
    // TODO:


    //
    NoticeWin msg_confirm(getWinBase());
    //msg_confirm.setWindowModality(Qt::ApplicationModal);    // 注意：这个不能缺，否则在 rk3568 的 buildroot 系统用户点击对话框外的区域，背景窗口会被切换为当前窗口，从而导致界面无法继续操作
    msg_confirm.setContent(tr("检测到U盘\n是否更新程序？"));   // "Udisk detected, update firmware?"
    if (msg_confirm.exec() == QDialog::Accepted)
    {
        QString app_backup_file_name = app_file_name + "-backup";

        QString app_dst_path = QString("/bin") + QDir::separator() + app_file_name;     // TODO: "/bin" -> const APP_DIR
        QString app_dst_backup_path = QString("/bin") + QDir::separator() + app_backup_file_name;

        if (QFile::exists(app_dst_backup_path)){
            qDebug() << "remove old app backup file";
            QFile(app_dst_backup_path).remove();
        }
        if (QFile::exists(app_dst_path)) {
            QFile old_file(app_dst_path);
            if (old_file.rename(app_dst_backup_path))
                qDebug() << "rename old app file to filename: " << app_backup_file_name;
            else
                qDebug() << "rename old app file failed!";
        } else {
            qDebug() << "old dst app file not found!";
        }

        if (QFile::copy(app_src_path, app_dst_path)) {
            qDebug() << "copy file " << app_file_name << " success!";
            MessageWin msg_reboot;
            msg_reboot.setContent(tr("更新成功,即将重启设备"));  // "Update success，going to reboot"
            msg_reboot.setButtonEnable(false);
            msg_reboot.setTimeout(3);
            msg_reboot.exec();

            globalService()->reboot();

        } else {
            qDebug() << "copy file " << app_file_name << " failed!";
            if (QFile(app_dst_backup_path).rename(app_dst_path)) {
                qDebug() << "restore old file \"" << app_file_name << "\" !";

                MessageWin msg_failed;
                msg_failed.setContent(tr("更新失败！"));    // "Update failed！"

                msg_failed.exec();
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    //
    return true;
}

// 更新UI主题
void WindowsManagers::updateUItheme()
{
//    QFile file("/usr/theme");
//    if(!file.exists())
//    {
//        if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
//        {
//            qDebug() << "file theme doesn't exit & create failed";
//            theme = 1;
//        }
//        else
//        {
//            QTextStream write_file(&file);
//            write_file << QString::number(1, 10);
//            file.flush();
//            file.close();
//        }
//    }
//    if(!file.open(QIODevice::ReadOnly | QFile::Text))
//    {
//        qDebug("open brightness file failed!");
//        return;
//    }
//    else
//    {

//        QTextStream textData(&file);
//        QString str;
//        textData >> str;
//        theme = str.toInt();
//        qDebug() << "theme:" << theme;
//        file.close();

//    }
}


/*
 * _sys_signal = 1  : mainwindow.ui show,history.ui hide
 * _sys_signal = 2  : mainwindow.ui hide,history.ui show
 * _sys_signal = 3  : mainwindow.ui show,tool.ui hide
 * _sys_signal = 4  : mainwindow.ui hide,tool.ui show
 * _sys_signal = 5  : mainwindow.ui show,camera.ui hide
 * _sys_signal = 6  : mainwindow.ui hide,camera.ui show
 * _sys_signal = 7  : camera.ui show,reuslt.ui hide
 * _sys_signal = 8  : camera.ui hide,reuslt.ui show
 * _sys_signal = 9  : reuslt.ui show,history.ui hide
 * _sys_signal = 10 : mainwindow.ui show,result.ui hide
 */
//槽函数处理信号值（显示和隐藏ui界面）
void WindowsManagers::slot__SysSignalReceived(enSysSignal _sys_signal)
{
    qDebug() << "slot__SysSignalReceived = " << _sys_signal << ",sender():" << sender();

    if (sysSignal_01 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_HOME);   // TODO: 这一批控制窗口显示和隐藏的语句，原先是直接调用指定窗体指针的显示和隐藏函数。全局搜 sendSIGNAL 未发现这些值有被发送，已弃用？须去掉，改用统一的窗口调度函数。 2022-08-18
    } else if (sysSignal_02 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_CLINIC);
    } else if (sysSignal_03 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_HOME);
    } else if (sysSignal_04 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_TOOL);
    } else if (sysSignal_05 == _sys_signal) {
        if (false) {
            //globalService()->colorLampOff();
            qDebug() << "emit music off command";
        }
//    } else if (sysSignal_06 == _sys_signal) {
//        if (false) {
//            globalService()->colorLampOn();
//            qDebug() << "emit music on command";
//        }
//        getWinManage()->openMeasureWin();
//    } else if (sysSignal_07 == _sys_signal) {
//        if (false)
//        {
//            globalService()->colorLampOn();
//            qDebug() << "emit music on command";
//        }
//        getWinManage()->openMeasureWin();
//    } else if (sysSignal_08 == _sys_signal) {
//        getWinManage()->showWindowByType(WIN_RESULT);
//        if (false) {
//            globalService()->colorLampOff();
//            qDebug() << "emit music off command";
//        }
    } else if (sysSignal_09 == _sys_signal) {
        emit senderSignal();
        getWinManage()->showWindowByType(WIN_RESULT);
    } else if (sysSignal_10 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_HOME);
    } else if (sysSignal_11 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_CLINIC);
    } else if (sysSignal_12 == _sys_signal) {
        //SystemIni::readIni();
        //gWinWifi = new WinWifi();
        //gWinWifi->show();
        //toolWin->hide();
        getWinManage()->showWindowByType(WIN_WIFI);
    } else if (sysSignal_13 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_TOOL);
    } else if (sysSignal_14 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_BT);
    } else if (sysSignal_15 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_TOOL);
    } else if (sysSignal_16 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_SCREEN);
    } else if (sysSignal_17 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_HOME);
//    } else if (sysSignal_18 == _sys_signal) {        // 隐藏批量筛查界面打开结果界面
//        //resultWin->showVisionJudgementDesc();
//        resultWin->loadDataToUi();
//        batchscr->hide();
//    } else if (sysSignal_20 == _sys_signal) {
//        if (false) {
//            globalService()->colorLampOn();
//            qDebug() << "emit music on command";
//        }
//        getWinManage()->openMeasureWin();
    } else if (sysSignal_21 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_SCREEN);
        if (false) {
            //globalService()->colorLampOff();
            qDebug() << "emit music off command";
        }
    } else if (sysSignal_22 == _sys_signal) {
        getWinManage()->showWindowByType(WIN_SCREEN);
    } else if (sysSignal_PowerOffPressed == _sys_signal) {    //sun poweroff
        if (g_WinMeasure->isVisible()) {    // 窗体可见
            return;
        }

        if (!flag) {
            flag = true;
        } else {
            return;
        }

        // 设置正常屏幕亮度
        PowerControl::setScreenBrightnessNormal();

        //
        QString text = tr("确认关机？"); // "shut down the system?"
        bool ret = getWinManage()->showNoticeWin(text);
        if (ret) {
            qDebug("select ok");
            globalService()->powerOff();
        } else {
            qDebug("select cancel");
            flag = false;
        }
//    } else if (sysSignal_24 == _sys_signal) {
//        qDebug("hide input");
//        getWinManage()->showWindowByType(WIN_CLINIC);

    } else if (sysSignal_ChargingOn == _sys_signal) {   //充电状态
        //BatteryMonitor::setIsCharging(true);        // NOTE: 计算电量需要充电状态，所以外部需一起设置 AD 和充电状态，否则可能出现用充电电压计算电量的瞬间！
        //BatteryMonitor::setIsChargedFull(false);

        // 查询一次电量
        g_WinMeasure->queryInfosFromBaseBoard();
    } else if (sysSignal_ChargingOff == _sys_signal) {  //非充电状态
        //BatteryMonitor::setIsCharging(false);       // NOTE: 计算电量需要充电状态，所以外部需一起设置 AD 和充电状态，否则可能出现用充电电压计算电量的瞬间！

        // 查询一次电量
        g_WinMeasure->queryInfosFromBaseBoard();
    } else if (sysSignal_ChargingFull == _sys_signal) { // NOTE: 充满，肯定处于充电状态
        //BatteryMonitor::setIsCharging(true);        // NOTE: 计算电量需要充电状态，所以外部需一起设置 AD 和充电状态，否则可能出现用充电电压计算电量的瞬间！
        //BatteryMonitor::setIsChargedFull(true);

        // 查询一次电量
        g_WinMeasure->queryInfosFromBaseBoard();

    //} else if (sysSignal_MusicOn == _sys_signal) {
    //    globalService()->colorLampOn();
    //    qDebug() << "emit music on command";
    //} else if (sysSignal_MusicOff == _sys_signal) {
    //    globalService()->colorLampOff();
    //    qDebug() << "emit music off command";
    } else if (sysSignal_PowerOffCommand == _sys_signal) {     // 提醒关机，并自动选择“是”
        if (!flag) {
            flag = true;
        } else {
            return;
        }

        QString msg = tr("收到外部系统关机指令，是否继续?");   // "Received power off command from external system, continue?"
        bool ret = getWinManage()->showNoticeWin(msg, "Yes", "No", 10, true);
        if (ret) {
            appSetting::sync();
            qApp->processEvents();

            qDebug("select ok");
            globalService()->powerOff();
        } else {
            qDebug("select cancel");
            flag = false;
        }
    } else {
        if (CGlobal::isDebugMode) {
            getWinManage()->showSuspensionPrompt(QString("slot__SysSignalReceived(): signal %3 not processed!").arg(_sys_signal));
        }
    }
}

void WindowsManagers::slot_uploadThread_DataTransmiterGetNewSubject(DataTrans::Client _client)
{
    //
    CPatient pat;
    pat.reset();
    DataTrans::client2Patient(_client, pat);
    pat.isBatch = true;

    // 显示测量界面
    WinMeasure::setOperationMode(operationMode_InputMeasure);
    getWinManage()->openMeasureWin(pat, patientSource_Command);
}

void WindowsManagers::slot_uploadThread_DataTransmiterOperationLocked(bool _locked, QString _msg)
{
    setOperationLocked(_locked, _msg);
}

///=============================================================================================================
/// class PowerControl

//
PowerControl *PowerControl::powerCtl = Q_NULLPTR;
enSystemState PowerControl::systemState = systemState_LowPower;

//
PowerControl::PowerControl()
{
    timeCnt = 0;

    QObject::connect(&m_timer, SIGNAL(timeout()), this, SLOT(slot_timer_timeout()));
    m_timer.start(1000);
}

PowerControl *PowerControl::getlInstance()
{
    if (powerCtl == NULL) {
        powerCtl = new PowerControl;
    }

    return powerCtl;
}

void PowerControl::reset()
{
    timeCnt = 0;
    controller();
}

// 设置正常状态下的屏幕亮度
void PowerControl::setScreenBrightnessNormal()
{
    qDebug() << "setScreenBrightnessNormal()";

#if (1 == OS_TYPE)
    setScreenBrightnessPercent(94);
#elif (3 == OS_TYPE)
    setScreenBrightnessPercent((int)CGlobal::getScreenBrightnessCfg());
#endif

    PowerControl::setSystemState(systemState_Normal);

    if (globalService()->getIsStartupFinished()) {
        wakeUp();                           // TODO: 逻辑梳理优化
    }

}

// 设置暗光状态下的屏幕亮度
void PowerControl::setScreenBrightnessDark()
{
    qDebug() << "setScreenBrightnessDark()";

#if (1 == OS_TYPE)
    setScreenBrightnessPercent(5);
#elif (3 == OS_TYPE)
    setScreenBrightnessPercent(10);
#endif

    PowerControl::setSystemState(systemState_DarkLight);
}

// 设置息屏状态下的屏幕亮度
void PowerControl::setScreenBrightnessClosed()
{
    qDebug() << "setScreenBrightnessClosed()";

    //
    setScreenBrightnessPercent(0);

    PowerControl::setSystemState(systemState_LowPower);

    if (globalService()->getIsStartupFinished()) {
        lowPower();                         // TODO: 逻辑梳理优化
    }

}

void PowerControl::lowPower()
{
    // 相机关闭
    CameraInitThread::cameraTurnOff();

}

void PowerControl::wakeUp()
{
    // 相机打开
    CameraInitThread::cameraTurnOn();

}

// 设置屏幕亮度（百分数）
void PowerControl::setScreenBrightnessPercent(int _percent)
{
#if (1 == OS_TYPE)
    qDebug() << "set brightness:" << _percent;

    if(_percent > 94)
        _percent = 94;
    if (_percent < 0)
        _percent = 0;

    QFile file("/sys/class/backlight/pwm-backlight/brightness");
    if(!file.exists())
        file.setFileName("/sys/class/backlight/backlight_lcd/brightness");
    if(!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QFile::Text))
    {
        qDebug("open brightness file failed!");
        return false;
    }
    else
    {
        QTextStream backlight(&file);
        backlight << _percent ;
        file.close();
    }
#elif (3 == OS_TYPE)
    logDebug(QString("seting screen brightness to %1%").arg(_percent));

    int brightness = std::round(((double)255 * _percent) / 100);
    QString cmd = QString("echo %1 > /sys/class/backlight/backlight/brightness").arg(brightness);
    system(cmd.toLatin1().data());
#else
    Q_UNUSED(_percent)
#endif
}

void PowerControl::setSystemState(enSystemState _state)
{
    systemState = _state;
}

enSystemState PowerControl::getSystemState()
{
    return systemState;
}

void PowerControl::controller()
{
    //
    static bool autoCloseWiFi = false;

    // 【自动息屏】的处理
    const int backlight_timeout_off = enumToInt_AutoScreenOff(CGlobal::autoScreenOff);                  // 背光超时_关闭（秒数）
    const int backlight_timeout_dark = (backlight_timeout_off > 0 ? backlight_timeout_off - 60 : 0);    // 背光超时_调暗（秒数）
    if (backlight_timeout_off <= 0) {
        if (getSystemState() != systemState_Normal) {
            setScreenBrightnessNormal();
        }
    } else {
        if (timeCnt < backlight_timeout_dark) {                   // 恢复屏幕亮度
            if (getSystemState() != systemState_Normal) {
                //getWinManage()->hideMsgWin();     // TODO: 这里隐藏所有消息干嘛？

                setScreenBrightnessNormal();    // 设置屏幕亮度
                if (getSystemState() == systemState_LowPower && autoCloseWiFi)
                {
                    //QWidget *activeWindow = qApp->activeWindow();

                    //QWidget widget(activeWindow);
                    //widget.setObjectName("Restituting");
                    //widget.setWindowFlag(Qt::Tool, true);
                    //widget.setAttribute(Qt::WA_TranslucentBackground);
                    //widget.setAttribute(Qt::WA_X11DoNotAcceptFocus, true);
                    //widget.setGeometry(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                    //QLabel label(&widget);
                    //label.setText(language?"正在恢复...":"Restituting...");
                    //label.setGeometry(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                    //label.setStyleSheet("background-color:rgba(0,0,0,150);color:white;");
                    //QFont font = qApp->font();
                    //font.setPixelSize(24);
                    //label.setFont(font);
                    //label.setAlignment(Qt::AlignCenter);
                    //WgtStatusBar::setCurrParent(&widget);
                    //widget.show();
                    //qApp->processEvents();
                    //2020.9.13  tao 屏蔽
                    //if(autoCloseWiFi)
                    //{
                    //    autoCloseWiFi = false;
                    //    emit toggleWiFiState();
                    //}

                    // WiFi模块重新供电后，需要重新初始化，可以根据实际情况修改这里的延时
                    //QThread::msleep(200);
                    //widget.hide();
                }

                qDebug() << "PowerControl: stayWakeup";
            }
        } else if (timeCnt >= backlight_timeout_dark && timeCnt < backlight_timeout_off) {      // 调暗屏幕
            if (getSystemState() != systemState_DarkLight) {
                setScreenBrightnessDark();

                qDebug() << "PowerControl: darkBacklight";
            }
        } else if(timeCnt >= backlight_timeout_off) {                                           // 关闭屏幕
            if (getSystemState() != systemState_LowPower) {
                //getWinManage()->hideMsgWin();     // TODO: 这里隐藏所有消息干嘛？

                setScreenBrightnessClosed();

                // 断开WiFi电源     //2020.9.13  tao 屏蔽
                //if(gWifiIntf->getIsOpened())
                //{
                //    autoCloseWiFi = true;
                //    emit toggleWiFiState();
                //}

                //todo close redlight
                qDebug() << "PowerControl: systemsleep";
            }
        }
    }

    // 【自动关机】的处理
    unsigned int poweroff_timeout_secs = settings::getPoweroffTimeSecs();
    if (poweroff_timeout_secs > 0) {
        if (timeCnt >= poweroff_timeout_secs && !g_AutoTest && !BatteryMonitor::getIsCharging() && (!CGlobal::getIsExternalControl())) {
            QPoint pos(0,0);
            QMouseEvent EventPress(QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(this,&EventPress);      // 发送键盘按下事件 // TODO: 这是干嘛的？是因为如果是息屏状态，使系统唤醒？

            setScreenBrightnessNormal();

            qDebug() << "PowerControl: CLOSE_SYSTEM ";

#if (1 == OS_TYPE)
            MySerialPort::instance()->sigWrite(COMMAND_MUSIC_ON, 8);
#endif
            // notice voice
            g_SoundIntf->playPrompt(soundPrompt_LowBattery);

            NoticeWin warnWin;
            QTimer *timer = new QTimer(&warnWin);
            QObject::connect(timer, SIGNAL(timeout()), &warnWin, SLOT(accept()));
            warnWin.setContent(tr("即将关机，节省电量"));    // "Going to shut down"
            warnWin.setButtonText(tr("确认"), tr("取消"));  // "OK", "Cancel"

            timer->start(30000);    //30s

            if(warnWin.exec() == QDialog::Accepted)
            {
                globalService()->powerOff();
            }
            else
            {
                g_SoundIntf->stop();
                timer->stop();
                delete timer;
                timer = nullptr;

                reset();        // TODO: 防止死循环？
            }
        }
    }
}

void PowerControl::slot_timer_timeout()
{
    if (WinMeasure::isOpened()) {
        if (getSystemState() != systemState_Normal) {
            //wakeUp();     /* 旧代码调用了这个原未实现的函数，待梳理 */
        }

        return;
    }

    timeCnt++;
    controller();
}

///=============================================================================================================
/// extern variable

QThread *getCommonThread()
{
    return g_commonThread;
}

QNetworkAccessManager *networkManager()
{
    return g_networkManager;
}
