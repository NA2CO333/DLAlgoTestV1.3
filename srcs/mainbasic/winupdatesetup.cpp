#include "winupdatesetup.h"
#include "ui_winupdatesetup.h"

#include "update.h"
#include "appsetting.h"
#include "global.h"
#include "winmanage.h"
#include "windowsmanager.h"
#include "otaupdatedefs.h"

/// ==================================================================================
/// class WinUpdateSetup

WinUpdateSetup::WinUpdateSetup(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::WinUpdateSetup)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    // 信号槽连接
    QObject::connect(this, &WinUpdateSetup::sigUpdateAddressChanged, g_update, &CUpdate::slotUpdateAddressChanged, Qt::QueuedConnection);
    QObject::connect(this, &WinUpdateSetup::sigAutoCheckUpdateChanged, g_update, &CUpdate::slotAutoCheckUpdateChanged, Qt::QueuedConnection);
    QObject::connect(this, &WinUpdateSetup::sigCheckUpdate, g_update, &CUpdate::slotCheckUpdate, Qt::QueuedConnection);

    // 设置软件更新模块
    emit sigUpdateAddressChanged(getCfg_updateAddress());
    emit sigAutoCheckUpdateChanged(getCfg_autoCheckUpdate());

}

WinUpdateSetup::~WinUpdateSetup()
{
    delete ui;
}

QString WinUpdateSetup::getCfg_updateAddress()
{
    QString url_update = appSetting::value("/update/updateServer").toString();
    if (url_update.isEmpty()) {
        url_update = DEFAULT_UPDATE_PATH;
        appSetting::setValue("/update/updateServer", url_update);
    }
    return url_update;
}

void WinUpdateSetup::setCfg_updateAddress(QString _url_str)
{
    appSetting::setValue("/update/updateServer", _url_str);
}

bool WinUpdateSetup::getCfg_autoCheckUpdate()
{
    bool is_ignore = appSetting::value("/update/ignore").toBool();
    return (!is_ignore);
}

void WinUpdateSetup::setCfg_autoCheckUpdate(bool _auto_check)
{
    bool is_ignore = (!_auto_check);
    appSetting::setValue("/update/ignore", is_ignore);
}

//void WinUpdateSetup::updateLanguage()
//{
//    if (language) {
//        getWinManage()->updateWindowTitle(this, "软件更新");
//
//        ui->lblHome->setText("主页");
//        ui->lblBack->setText("返回");
//        ui->lblUpdateAddress->setText("服务地址");
//        ui->lblAutoCheck->setText("自动检查更新");
//        ui->btnNetworkUpdate->setText("立即检查更新");
//        ui->btnUDiskUpdate->setText("U 盘更新");
//    } else {
//        getWinManage()->updateWindowTitle(this, "Software update");
//
//        ui->lblHome->setText("Home");
//        ui->lblBack->setText("Back");
//        ui->lblUpdateAddress->setText("Service Address");
//        ui->lblAutoCheck->setText("Auto Check");
//        ui->btnNetworkUpdate->setText("Update Imediately");
//        ui->btnUDiskUpdate->setText("UDisk Update");
//    }
//
//}

void WinUpdateSetup::showEvent(QShowEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("软件更新"));    // "Software update"

    // 更新 UI 控件的值
    updateUiValues();

    // 更新主题
    updateTheme(getSysThemeType());

    // 设置控件的有效性、可见性等
    ui->edtServer->setReadOnly(!CGlobal::isDebugMode);
    if (CAuthIntf::authType_Trial == CGlobal::authType) {          // 若是试用版，禁止修改更新地址
        ui->edtServer->setReadOnly(true);
    }

}

void WinUpdateSetup::updateUiValues()
{
    // 更新服务的地址
    ui->edtServer->setText(getCfg_updateAddress());

    //
    ui->sbtnIsAutoCheck->setIsOn(getCfg_autoCheckUpdate());

    //
    ui->lblAppVersion->setText(tr("软件版本：V%1").arg(aboutdevice::getAppVerFull()));   // "Application Vertion: V%1"

}

void WinUpdateSetup::updateTheme(enThemeType _theme)
{
    //QPalette palette;
    if (themeType_Black == _theme) {
        // 自身的背景
        //palette.setBrush(this->backgroundRole(), QColor(qRgb(1, 1, 1)));

        //
        this->setStyleSheet(QString()
                            + "QWidget { background-color: transparent; color: rgb(250,250,252); } \n"
                            + "QPushButton { border-radius: 5px; } \n"
                            + "QPushButton#btnNetworkUpdate { background-color: rgb(28, 28, 30); } \n"
                            + "QPushButton#btnUDiskUpdate { background-color: rgb(28, 28, 30); } \n"
                            + "QPushButton#btnOtaUpdate { background-color: rgb(28, 28, 30); } \n"
                            + "QLineEdit { background-color: rgb(28,28,30); color: rgb(144,143,148); border-radius: 3px; } \n"
                            );

        //
        this->setAutoFillBackground(true);

    } else {
        // TODO:
    }
    //this->setPalette(palette);

}

void WinUpdateSetup::setAutoCheckUpdate(bool _auto_check)
{
    //
    setCfg_autoCheckUpdate(_auto_check);

    //
    emit sigAutoCheckUpdateChanged(_auto_check);

    //
    if (ui->sbtnIsAutoCheck->getIsOn() != _auto_check) {
        ui->sbtnIsAutoCheck->setIsOn(_auto_check);
    }

}

void WinUpdateSetup::on_sbtnIsAutoCheck_clicked()
{
    setAutoCheckUpdate(ui->sbtnIsAutoCheck->getIsOn());
}

void WinUpdateSetup::on_btnNetworkUpdate_clicked()
{
    if (!update_flag) {
        update_flag = true;
        emit sigAutoCheckUpdateChanged(false);
        emit sigCheckUpdate();
    }
}

void WinUpdateSetup::on_btnHome_clicked()
{
    getWinManage()->showWindowByType(WIN_HOME);
}

void WinUpdateSetup::on_btnBack_clicked()
{
    getWinManage()->showWindowByType(WIN_TOOL);
}

void WinUpdateSetup::on_edtServer_textChanged(const QString &arg1)
{
    if (arg1 != getCfg_updateAddress()) {
        setCfg_updateAddress(arg1);
        emit sigUpdateAddressChanged(arg1);
    }
}

void WinUpdateSetup::on_btnUDiskUpdate_clicked()
{
    QString msg;
    bool succ = WindowsManagers::checkUdiskAndUpdate(&msg);
    if (!succ && msg.length() > 0) {
        getWinManage()->showMsgWin(/*(language ? "U 盘更新失败：\n" : "UDisk update failed:\n") +*/ msg);
    }
}

void WinUpdateSetup::on_btnOtaUpdate_clicked()
{
    // 须电量足够才允许操作
    int battery_stat = WindowsManagers::checkBatteryLevelForOta();
    if (0 != battery_stat) {
        QString msg = (tr("电量小于 %1 格，无法启动固件更新。"));  // "Battery level less than %1 cells, can't start firmware update."
        getWinManage()->showMsgWin(msg.arg(battery_stat));
        return;
    }

    //
    bool resp = getWinManage()->showNoticeWin(tr("确定要启动固件更新吗？"));   // "sure want to start firmware update?"
    if (resp) {
        WindowsManagers::configOtaUpdate();

        //
        if (OtaUpdate::runOtaUpdate()) {
            globalService()->closeApp();
        } else {
            getWinManage()->showSuspensionPrompt(tr("启动固件更新失败"));   // "Failed to start firmware update"
        }
    }
}
