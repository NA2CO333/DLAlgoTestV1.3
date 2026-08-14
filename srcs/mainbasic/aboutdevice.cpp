//关于设备
#include "aboutdevice.h"
#include "ui_aboutdevice.h"

#include "windowsmanager.h"
#include "global.h"
#include "sysinfo.h"
#include "otaupdatedefs.h"
#include "batterymonitor.h"
#include "winupdatesetup.h"
#include "utilui.h"

#include "remote-service/commandhandler.h"

/*
// 编译时的年、月、日（整型）
#define YEAR ((((__DATE__ [7] - '0') * 10 + (__DATE__ [8] - '0')) * 10 \
+ (__DATE__ [9] - '0')) * 10 + (__DATE__ [10] - '0'))

#define MONTH ((__DATE__ [0] == 'J' && __DATE__ [1] == 'a' && __DATE__ [2] == 'n') ? 1 \
: (__DATE__ [0] == 'F' && __DATE__ [1] == 'e' && __DATE__ [2] == 'b') ? 2 \
: (__DATE__ [0] == 'M' && __DATE__ [1] == 'a' && __DATE__ [2] == 'r') ? 3 \
: (__DATE__ [0] == 'A' && __DATE__ [1] == 'p' && __DATE__ [2] == 'r') ? 4 \
: (__DATE__ [0] == 'M' && __DATE__ [1] == 'a' && __DATE__ [2] == 'y') ? 5 \
: (__DATE__ [0] == 'J' && __DATE__ [1] == 'u' && __DATE__ [2] == 'n') ? 6 \
: (__DATE__ [0] == 'J' && __DATE__ [1] == 'u' && __DATE__ [2] == 'l') ? 7 \
: (__DATE__ [0] == 'A' && __DATE__ [1] == 'u' && __DATE__ [2] == 'g') ? 8 \
: (__DATE__ [0] == 'S' && __DATE__ [1] == 'e' && __DATE__ [2] == 'p') ? 9 \
: (__DATE__ [0] == 'O' && __DATE__ [1] == 'c' && __DATE__ [2] == 't') ? 10 \
: (__DATE__ [0] == 'N' && __DATE__ [1] == 'o' && __DATE__ [2] == 'v') ? 11 : 12)

#define DAY ((__DATE__ [4] == ' ' ? 0 : __DATE__ [4] - '0') * 10 \
+ (__DATE__ [5] - '0'))
*/

// UI 程序版本号
QString g_UiVersionPrefix = "V ";           // 版本号的前缀，遵从产品注册文件的命名规范而设

int g_verMajor = 1;                      // 主版本号。该级版本号的变更，不确保前后兼容性
int g_verMinor = 5;                      // 次版本号。该级版本号的增加，应保持向前兼容
int g_verPatch = 11;                     // 修订版本号。该级版本号的变更，应保持向前和向后兼容

//QString g_UiVer_Date = QString::asprintf("%d%.02d%.02d", YEAR, MONTH, DAY);     // 版本日期       /* "版本日期"应该是指代码的最后修改日期，而与编译日期无关。 */
QString g_verDate = "20260807";          // 版本日期（代码的最后修改日期，每次 commit 时须手动修改），格式：yyyyMMdd

QString g_UiVersionSuffix = "";             // 版本号的后缀，遵从产品注册的命名规范而设，之前的写法是“(V1)”，其中“1”是主版本号
//QString g_UiVersionSuffix = "(T)";          // 版本号的后缀，若括号中的是“T”，则表示是测试版

int g_UiVer_Build = int(BUILD_VER);         // 编译版本号

// 底板程序版本号
QString g_Stm32VersionPrefix = "";          // 底板程序版本号前缀
QString g_Stm32VersionStr = "";             // 底板程序版本号字符串

int g_Stm32VerMajor {-1};                   // 底板程序版本号：主版本号
int g_Stm32VerMinor {-1};                   // 底板程序版本号：次版本号
int g_Stm32VerPatch {-1};                   // 底板程序版本号：修订版本号

///=============================================================================================================
/// class aboutdevice
///

//
aboutdevice::aboutdevice(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::aboutdevice)
{
    ui->setupUi(this);

    //
    Util::Ui::clearStyleSheet(this);

    //
    isShowStatusBar = true;

    //
    ui->pushButton_Back->setStyleSheet("border:none");  // 隐藏边框线
    ui->pushButton_Back->setFocusPolicy(Qt::NoFocus);   // 影藏焦点框
    ui->pushButton_Back->setFlat(true);                 // 融入背景

    // 设备远程服务
    remoteService = new Net::Remote::CRemoteService;

    //
    winLog = new WinLog(getWinBase());
    winLog->setVisible(false);

    //
    ui->btnRemoteSvcLogs->setVisible(false);

    // 暂时停用
    ui->lblRemoteSvc->setVisible(false);
    ui->sbtnRemoteSvc->setVisible(false);

}

aboutdevice::~aboutdevice()
{
    delete ui;

    //
    remoteService->setIsOpened(false);

    remoteService->deleteLater();
    remoteService = Q_NULLPTR;

    //
    winLog->deleteLater();

}

QString aboutdevice::getAppVerBase()
{
    return QString::asprintf("%d.%d.%d", g_verMajor, g_verMinor, g_verPatch);
}

QString aboutdevice::getAppVerDate()
{
    return g_verDate;
}

QString aboutdevice::getAppVerFull()
{
    return aboutdevice::getAppVerBase() + "." + g_verDate;
}

QString aboutdevice::getAppVerBuild()
{
    return QString::number(g_UiVer_Build);
}

QString aboutdevice::getAppVerAll()
{
    return getAppVerFull()  + "_" + aboutdevice::getAppVerBuild();
}

QString aboutdevice::getVerStrForReg()
{
    return g_UiVersionPrefix + aboutdevice::getAppVerBase() + g_UiVersionSuffix + "." + g_verDate;
}

void aboutdevice::sendQueryStm32Version()
{
    qDebug() << "sendQueryStm32Version()";

    MySerialPort::instance()->write(GetVersion);  //获取底板版本号
}

void aboutdevice::setStm32Version(int _ver_major, int _ver_minor, int _ver_patch)
{
    g_Stm32VerMajor = _ver_major;
    g_Stm32VerMinor = _ver_minor;
    g_Stm32VerPatch = _ver_patch;

    g_Stm32VersionStr = QString("%1.%2.%3").arg(g_Stm32VerMajor).arg(g_Stm32VerMinor).arg(g_Stm32VerPatch);
}

void aboutdevice::getStm32Version(int &_ver_major, int &_ver_minor, int &_ver_patch)
{
    _ver_major = g_Stm32VerMajor;
    _ver_minor = g_Stm32VerMinor;
    _ver_patch = g_Stm32VerPatch;
}

const QString &aboutdevice::getStm32VersionStr()
{
    return g_Stm32VersionStr;
}

stVerInfoApp aboutdevice::getAppVerInfoOfCode()
{
    stVerInfoApp ver_info = {g_verMajor, g_verMinor, g_verPatch, QDate::fromString(g_verDate, "yyyyMMdd"), g_UiVer_Build};
    return ver_info;
}

bool aboutdevice::strToVersionInfo(const QString &_ver_str, stVerInfoApp &_ver_info, int _fail_val)
{
    bool is_succ = false;
    do {
        // 长度须大于0
        if (_ver_str.length() == 0) {
            break;
        }

        // 格式检查：只能包含[0-9],'.','_'，'_'只能有1个或没有，'.' 个数须等于2或3
        int count_dot = 0, count_underline = 0;
        int idx_underline = -1;
        for (int i = 0; i < _ver_str.length(); i++) {
            if (_ver_str.at(i) == '.') {
                count_dot++;
            } else if (_ver_str.at(i) == '_') {
                count_underline++;
                idx_underline = i;
            } else if (_ver_str.at(i) < '0' || _ver_str.at(i) > '9') {
                logDebug("format invalid: char not valid");
                break;
            }
        }
        if (!(count_dot == 2 || count_dot == 3)) {
            logDebug("format invalid: count of '.' not equal to 2 or 3");
            break;
        }
        if (! ((count_dot == 2  && count_underline == 0) || (count_dot == 3  && count_underline <= 1))) {   // 若不包含版本日期，就不可能包含编译号
            logDebug("format invalid: count of '_' not valid");
            break;
        }

        // 得到编译号
        if (count_underline == 1) {
            if (idx_underline < 0) {
                logCritical("logic error: index of underline exceptional");
                break;
            }
            QString ver_build = _ver_str.mid(idx_underline + 1);
            _ver_info.verBuild = ver_build.toUInt();
        } else {
            _ver_info.verBuild = 0;
        }

        // 4段版本号
        QString str_vers;
        if (idx_underline > 0) {
            str_vers = _ver_str.left(idx_underline);
        } else {
            str_vers = _ver_str;
        }
        QStringList list_str = str_vers.split(".");
        if (list_str.size() >= 3) {
            _ver_info.verMajor  = list_str[0].toUInt();
            _ver_info.verMinor  = list_str[1].toUInt();
            _ver_info.verPatch  = list_str[2].toUInt();
        } else {
            logCritical("logic error: count of section less then 3");
            break;
        }
        if (list_str.size() == 4) {
            _ver_info.verDate   = QDate::fromString(list_str[3], "yyyyMMdd");
            if (!_ver_info.verDate.isValid()) {
                logCritical("format error: version date str is not valid");
                break;
            }
        }

        //
        is_succ = true;
    } while (false);

    if (!is_succ) {
        if (0 == _fail_val) {
            _ver_info = {{0, 0, 0}, QDate(1, 1, 1), 0};
        } else if (1 == _fail_val) {
            _ver_info = {{g_verMajor, g_verMinor, g_verPatch}, QDate::fromString(g_verDate, "yyyyMMdd"), g_UiVer_Build};
        }
    }

    return is_succ;
}

QString aboutdevice::versionInfoToStr(const stVerInfoApp &_ver_info)
{
    QString ver_str = QString("%1.%2.%3.%4").arg(_ver_info.verMajor).arg(_ver_info.verMinor).arg(_ver_info.verPatch)
            .arg(_ver_info.verDate.toString("yyyyMMdd"));
    if (_ver_info.verBuild > 0) {
        ver_str += QString("_%1").arg(_ver_info.verBuild);
    }
    return ver_str;
}

void aboutdevice::showEvent(QShowEvent *)
{
    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 样式刷新
    QPalette palette;
    if(themeType_Black == getSysThemeType()){
        //if (language) {
        //    palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/c_about_b.png"));  //黑色主题(中)
        //} else {
        //    palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/e_about_b.png"));  //黑色主题(英)
        //}
        palette.setBrush(this->backgroundRole(), QBrush(QColor(qRgb(0, 0, 0))));

        QPixmap pixmap_about_b(QString(":/resource/black_theme/about_b_%1.png").arg(CGlobal::language));
        ui->lblDiagram->setPixmap(pixmap_about_b);

        ui->lblMainProgramVer->setStyleSheet("color:rgb(204,204,204);");
        ui->lblBaseBoardVer->setStyleSheet("color:rgb(204,204,204);");
        ui->lblDistanceType->setStyleSheet("color:rgb(204,204,204);");
        ui->lblBluetoothType->setStyleSheet("color:rgb(204,204,204);");
        //ui->lblIsReducedVersion->setStyleSheet("color:rgb(204,204,204);");
        ui->lblRemoteSvc->setStyleSheet("color:rgb(204,204,204);");

        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");
        ui->lblDeviceCode->setStyleSheet("color:rgb(204,204,204);");
        ui->lblProductModel->setStyleSheet("color:rgb(204,204,204);");
        ui->lblFirmwareVer->setStyleSheet("color:rgb(204,204,204);");
        ui->lblRootfsVer->setStyleSheet("color:rgb(204,204,204);");
        ui->lblOsVer->setStyleSheet("color:rgb(204,204,204);");
        //QImage aImg(":/resource/black_theme/about_b.png");
        //ui->label->setPixmap(QPixmap::fromImage(aImg));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
        ui->btnDataBackup->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(28,28,30); color:rgb(250,250,252);}");
        ui->btnDataRestore->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(28,28,30); color:rgb(250,250,252);}");
        ui->btnRemoteSvcLogs->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(28,28,30); color:rgb(250,250,252);}");
        ui->lblQtCopyright->setStyleSheet("color:rgb(204,204,204);");
        ui->lblQrCodeTitle->setStyleSheet("color:rgb(204,204,204);");
        ui->frmQrCode->setStyleSheet("QFrame#frmQrCode { border: 1px solid rgb(61, 62, 64); } QLabel { color:rgb(204,204,204); }");
    }
    else if(themeType_White == getSysThemeType()){
        //if (language) {
        //    palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/c_about_w.png"));  //白色主题(中)
        //} else {
        //    palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/e_about_w.png"));  //白色主题(英)
        //}
        palette.setBrush(this->backgroundRole(), QBrush(QColor(qRgb(0, 0, 0))));

        //QPixmap pixmap_about_w(x);
        //ui->lblDiagram->setPixmap(pixmap_about_w);

        ui->lblMainProgramVer->setStyleSheet("color:rgb(1,1,1);");
        ui->lblBaseBoardVer->setStyleSheet("color:rgb(1,1,1);");
        ui->lblDistanceType->setStyleSheet("color:rgb(1,1,1);");
        ui->lblBluetoothType->setStyleSheet("color:rgb(1,1,1);");
        //ui->lblIsReducedVersion->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");
        ui->lblDeviceCode->setStyleSheet("color:rgb(1,1,1);");
        ui->lblProductModel->setStyleSheet("color:rgb(1,1,1);");
        ui->lblFirmwareVer->setStyleSheet("color:rgb(204,204,204);");
        ui->lblRootfsVer->setStyleSheet("color:rgb(204,204,204);");
        ui->lblOsVer->setStyleSheet("color:rgb(204,204,204);");
        //QImage aImg(":/resource/white_theme/about_w.png");
        //ui->label->setPixmap(QPixmap::fromImage(aImg));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
        ui->btnDataBackup->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(201,201,204); color:rgb(1,1,1);}");
        ui->btnDataRestore->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(201,201,204); color:rgb(1,1,1);}");
        ui->lblQtCopyright->setStyleSheet("color:rgb(204,204,204);");
        ui->lblQrCodeTitle->setStyleSheet("color:rgb(204,204,204);");
        ui->frmQrCode->setStyleSheet("QFrame#frmQrCode { border: 1px solid rgb(61, 62, 64); } QLabel { color:rgb(204,204,204); }");
    }
    this->setPalette(palette);
    this->setAutoFillBackground(true);

    //
    //if (language) {
    //    getWinManage()->updateWindowTitle(this, "关于");
    //
    //    ui->btnDataBackup->setText("数据备份");
    //    ui->btnDataRestore->setText("数据还原");
    //    ui->label_Back->setText("返回");
    //} else {
    //    getWinManage()->updateWindowTitle(this, "About");
    //
    //    ui->btnDataBackup->setText("Data Backup");
    //    ui->btnDataRestore->setText("Data Restore");
    //    ui->label_Back->setText("Back");
    //}

    // 标题
    getWinManage()->updateWindowTitle(this, tr("关于"));  // "About"

    //
    ui->lblMainProgramVer->setText(tr("主程序: %1").arg(aboutdevice::getAppVerAll()));    // "MainVer.: %1"
    ui->lblBaseBoardVer->setText(tr("底板程序: %1").arg(g_Stm32VersionPrefix + getStm32VersionStr()));   // "BaseBoardVer.: %1"
    ui->lblFirmwareVer->setText(tr("固件版本: %1").arg(CSysInfo::firmwareVersion()));  // "FirmwareVer.: %1"
    ui->lblRootfsVer->setText(tr("rootfs: %1").arg(CSysInfo::rootfsVersion())); // "rootfs: %1"
    ui->lblOsVer->setText(tr("操作系统: %1").arg(CSysInfo::osVersion()));   // "OS: %1"
    ui->lblDeviceCode->setText(tr("设备编号: %1").arg(CGlobal::devNum));    // "DeviceCode: %1"
    ui->lblProductModel->setText(tr("设备型号：%1").arg(getProductModelStr(CGlobal::productModel))); // "DeviceModel: %1"

    ui->lblQtCopyright->setText(tr("引用了以下开源项目：Qt，OpenCV，The Android Open Source Project。"));    // "Referenced following open source projects: QT, OpenCV, The Android Open Source Project."

    ui->lblDistanceType->setText(enumToText_DistSensorType(/*CGlobal::distSensorType*/ g_WinMeasure->distanceDetect()->sensorType())
                                 + ", " + QString::number(CGlobal::ultraCoefficient, 'g'));     /* 中文太易懂，易引起没必要在意这个信息的普通用户也注意，所以全用英文？ */

#if (1 == BLUETOOTH_TYPE)
    ui->lblBluetoothType->setText("Bluetooth: serial");
#else
    ui->lblBluetoothType->setText("Bluetooth: RK");
#endif

}

void aboutdevice::mouseReleaseEvent(QMouseEvent *)
{
    static int count = 0;

    count++;
    if (count >= 5) {
        //
        bool is_visible = ui->btnRemoteSvcLogs->isVisible();
        setRemoteSvcLogVisible(!is_visible);

        //
        count = 0;
    }
}

void aboutdevice::setRemoteSvcLogVisible(bool _is_visible)
{
    //
    ui->btnRemoteSvcLogs->setVisible(_is_visible);

    //
    static bool is_sig_slot_connected = false;
    if (_is_visible) {
        if (!is_sig_slot_connected) {
            QObject::connect(remoteService, &Net::Remote::CRemoteService::sigLog, winLog, &WinLog::slot_remoteService_Log, Qt::QueuedConnection);
            is_sig_slot_connected = true;
        }
    } else {
        if (is_sig_slot_connected) {
            QObject::disconnect(remoteService, &Net::Remote::CRemoteService::sigLog, winLog, &WinLog::slot_remoteService_Log);
            is_sig_slot_connected = false;
        }
    }

}

void aboutdevice::on_pushButton_Back_clicked()
{
    getWinManage()->backToLastWidget();
}

void aboutdevice::on_btnDataBackup_clicked()
{
    // 须电量足够才允许操作
    int battery_stat = WindowsManagers::checkBatteryLevelForOta();
    if (0 != battery_stat) {
        QString msg = tr("电量小于 %1 格，无法启动数据备份。");    // "Battery level less than %1 cells, can't start data backup."
        getWinManage()->showMsgWin(msg.arg(battery_stat));
        return;
    }

    //
    bool resp = getWinManage()->showNoticeWin(tr("确定要启动数据备份吗？"));   // "sure want to start data backup?"
    if (resp) {
        WindowsManagers::configOtaUpdate();

        //
        if (OtaUpdate::runUserBackup()) {
            globalService()->closeApp();
        } else {
            getWinManage()->showSuspensionPrompt(tr("启动数据备份失败"));   // "Failed to start data backup"
        }
    }
}

void aboutdevice::on_btnDataRestore_clicked()
{
    // 须电量足够才允许操作
    int battery_stat = WindowsManagers::checkBatteryLevelForOta();
    if (0 != battery_stat) {
        QString msg = tr("电量小于 %1 格，无法启动数据还原。");  // "Battery level less than %1 cells, can't start data restoration."
        getWinManage()->showMsgWin(msg.arg(battery_stat));
        return;
    }

    //
    bool resp = getWinManage()->showNoticeWin(tr("确定要启动数据还原吗？"));   // "sure want to start data restoration?"
    if (resp) {
        WindowsManagers::configOtaUpdate();

        //
        if (OtaUpdate::runUserRestore()) {
            globalService()->closeApp();
        } else {
            getWinManage()->showSuspensionPrompt(tr("启动数据还原失败"));   // "Failed to start data restoration"
        }
    }
}

void aboutdevice::on_sbtnRemoteSvc_clicked()
{
    bool is_on = ui->sbtnRemoteSvc->getIsOn();

    //
    setIsRemoteServiceOn(is_on);

}

void aboutdevice::setIsRemoteServiceOn(bool _is_on)
{
    //
    QString svr_host;
    int svr_port;
    QString svr_socket_path = "/websocket/refraction";
    bool is_https;
    QString svr_upload_path = "/api-socket/file/uploadEquipmentRunInfoFile";

    if (CGlobal::isDebugMode) {
        svr_host = "120.25.254.38";
        svr_port = 9006;
        is_https = false;
    } else {
        svr_host = "opt.manylinksmed.com";
        svr_port = 443;
        is_https = true;
    }

    //
    doSetIsRemoteServiceOn(_is_on, svr_host, svr_port, svr_socket_path, is_https, svr_upload_path, CGlobal::devNum);

}

void aboutdevice::doSetIsRemoteServiceOn(bool _is_on, QString _svr_host, int _svr_port, QString _svr_socket_path, bool _is_https, QString _svr_upload_path, QString _dev_num)
{
    //
    static bool is_init = false;
    static Net::Remote::CDirListHandler *dir_list_handler = Q_NULLPTR;

    //
    if (!is_init) {
        dir_list_handler = remoteService->getCommandHandler<Net::Remote::CDirListHandler*>(Net::Remote::TYPE_NAME_DIR_LIST);
        if (dir_list_handler) {
            dir_list_handler->addTopDir("/media/algoLog");
            dir_list_handler->addTopDir("/media/log");
            dir_list_handler->addTopDir("/media/pdfPreviewImg");
            dir_list_handler->addTopDir("/media/photo");
            dir_list_handler->addTopDir("/media/report-cfg");
            dir_list_handler->addTopDir("/media/reports");

            dir_list_handler->addTopDir("/root");

            is_init = true;
        } else {
            QMessageBox::critical(this, "error", "程序异常：获取文件请求处理者失败");
            return;
        }
    }

    //
    if (_is_on) {
        remoteService->setSvrHost(_svr_host);
        remoteService->setSvrPort(_svr_port);
        remoteService->setSvrPath(_svr_socket_path);
        remoteService->setIsHttps(_is_https);
        remoteService->setSenderNum(_dev_num);

        QString url_str = QString("%1://%2:%3%4").arg(_is_https ? "https" : "http").arg(_svr_host).arg(_svr_port).arg(_svr_upload_path);
        QUrl url(url_str);
        dir_list_handler->setUploadSvcUrl(url);

        remoteService->setIsOpened(true);

    } else {
        remoteService->setIsOpened(false);

    }

}

void aboutdevice::on_btnRemoteSvcLogs_clicked()
{
    winLog->show();
    winLog->raise();
}
