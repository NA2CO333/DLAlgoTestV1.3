//工程模式界面 add by sun 20180826

#include "engineermode.h"
#include "ui_engineermode.h"

#include <QMessageBox>
#include <QDir>
#include <QDebug>
#include <QNetworkInterface>

#include "algointf.h"
#include "messagewin.h"
#include "winmeasure.h"
#include "windowsmanager.h"
#include "global.h"
#include "util-common.h"
#include "logger.h"
#include "hardware.h"
#include "winunittest.h"

//
bool saveImage = false;             // 是否存图标志位   // TODO: 将这些全局设置移到 global 模块
bool isPrintScreen = false;         // 是否屏幕截图
bool g_hasBluetooth = true;         // 是否有蓝牙硬件模块
bool voltageState = false;          // 是否显示电压

CWinUnitTest *winUnitTest = Q_NULLPTR;

// 灯珠电流等级
static const QList<int> LED_LEVEL_LIST = QList<int>() << 0  << 7  << 9  << 11 << 12 << 14 << 16 << 18 << 19 << 20 << 21 << 22 << 23;

static constexpr char UI_STR_DEV_ACTIVATE[]     = "激活设备(离线)";
static constexpr char UI_STR_DEV_DEACTIVATE[]   = "反激活设备(离线)";

//
bool CEngineerMode::s_isPasswordEnabled {true};

CEngineerMode::CEngineerMode(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::engineerMode)
{
    ui->setupUi(this);

    ledMode = 0;

    //iniSetting = new QSettings("manylinks",QSettings::IniFormat);

    ui->version->setText(QString("曲线-") + CURVE_VERSION);
    ui->version->setEnabled(false);

    connect(ui->updateFont_pushbutton,SIGNAL(clicked(bool)),this,SLOT(updateFont()));
    connect(ui->updatePsplash_pushbutton,SIGNAL(clicked(bool)),this,SLOT(updatePsplash()));

    ledTimer = new QTimer(this);
    connect(ledTimer,SIGNAL(timeout()),this,SLOT(cycleTest()));

    ui->curve_comboBox->hide();
    ui->label_curve->hide();
    /*
    ui->curve_comboBox->setCurrentIndex(CAlgoInvoker::getCurve() - 1);
    if(CAlgoInvoker::getCurve()==1){    verStr.append("-低曲线");}
    if(CAlgoInvoker::getCurve()==2){    verStr.append("-中曲线");}
    if(CAlgoInvoker::getCurve()==3){    verStr.append("-高曲线");}
    */
    QString password = appSetting::value("tool/rootPassword").toString();
    if(password == "") {
        appSetting::setValue("tool/rootPassword","WLBQ");
        appSetting::setValue("tool/enginPassword","181818");
    }

    //
    ui->cbbDistanceType->addItem(enumToText_DistSensorType(enDistSensorType::Mb1010         , CGlobal::isDebugMode), (int)enDistSensorType::Mb1010      );
    //ui->cbbDistanceType->addItem(enumToText_DistSensorType(enDistSensorType::Xkc_DYP_A06    , CGlobal::isDebugMode), (int)enDistSensorType::Xkc_DYP_A06 );
    //ui->cbbDistanceType->addItem(enumToText_DistSensorType(enDistSensorType::Xkc_KL200      , CGlobal::isDebugMode), (int)enDistSensorType::Xkc_KL200   );
    //ui->cbbDistanceType->addItem(enumToText_DistSensorType(enDistSensorType::TFLC02         , CGlobal::isDebugMode), (int)enDistSensorType::TFLC02      );
    //ui->cbbDistanceType->addItem(enumToText_DistSensorType(enDistSensorType::TFLuna         , CGlobal::isDebugMode), (int)enDistSensorType::TFLuna      );
    //ui->cbbDistanceType->addItem(enumToText_DistSensorType(enDistSensorType::TFminiS        , CGlobal::isDebugMode), (int)enDistSensorType::TFminiS     );
    ui->cbbDistanceType->addItem(enumToText_DistSensorType(enDistSensorType::SIMAN_SDM10    , CGlobal::isDebugMode), (int)enDistSensorType::SIMAN_SDM10 );

    //
    for (int i = algoVerAll_Min; i <= algoVerAll_Max; i++) {
        ui->cbbPupilAlgoVer->addItem(CGlobal::pupilAlgoVerDesc[i], i);
    }

    // min log level
    //ui->cbbLogLevelMin->addItem("",             (int)-1);
    ui->cbbLogLevelMin->addItem("Min Debug",     (int)logLevel_Debug);
    ui->cbbLogLevelMin->addItem("Min Info",      (int)logLevel_Info);
    ui->cbbLogLevelMin->addItem("Min Warning",   (int)logLevel_Warning);
    ui->cbbLogLevelMin->addItem("Min Critical",  (int)logLevel_Critical);
    ui->cbbLogLevelMin->addItem("Min Fatal",     (int)logLevel_Fatal);

    // Log Filter
    ui->cbbLogFilter->addItem("all",                    QString(CGlobal::LOG_ALL));
    ui->cbbLogFilter->addItem(CGlobal::LOG_TEMP,        QString(CGlobal::LOG_TEMP));
    ui->cbbLogFilter->addItem(CGlobal::LOG_SYS,         QString(CGlobal::LOG_SYS));
    ui->cbbLogFilter->addItem(CGlobal::LOG_BASEBOARD,   QString(CGlobal::LOG_BASEBOARD));
    ui->cbbLogFilter->addItem(CGlobal::LOG_DISTANCE,    QString(CGlobal::LOG_DISTANCE));
    ui->cbbLogFilter->addItem(CGlobal::LOG_MEASURE,     QString(CGlobal::LOG_MEASURE));
    ui->cbbLogFilter->addItem(CGlobal::LOG_CAPTURE,     QString(CGlobal::LOG_CAPTURE));
    ui->cbbLogFilter->addItem(CGlobal::LOG_ALGO,        QString(CGlobal::LOG_ALGO));
    ui->cbbLogFilter->addItem(CGlobal::LOG_BLUETOOTH,   QString(CGlobal::LOG_BLUETOOTH));
    ui->cbbLogFilter->addItem(CGlobal::LOG_WIFI,        QString(CGlobal::LOG_WIFI));
    ui->cbbLogFilter->addItem(CGlobal::LOG_DATATRANS,   QString(CGlobal::LOG_DATATRANS));
    ui->cbbLogFilter->addItem(CGlobal::LOG_DATABASE,    QString(CGlobal::LOG_DATABASE));
    ui->cbbLogFilter->addItem(CGlobal::LOG_SERIAL,      QString(CGlobal::LOG_SERIAL));

    QString filter = CGlobal::getFilter();
    int idx_log_filter = ui->cbbLogFilter->findText(filter.length() == 0 ? "all" : filter);
    if (idx_log_filter >= 0) {
        ui->cbbLogFilter->setCurrentIndex(idx_log_filter);
    } else {
        logWarning("CEngineerMode::CEngineerMode(): log filter config val not found!");
    }

    //
    ui->tabWidget->setCurrentIndex(0);

    //QRect rect_tab = ui->tabWidget->tabBar()->geometry();
    //rect_tab.setHeight(100);
    //ui->tabWidget->tabBar()->setGeometry(rect_tab);   // TODO: 为什么无效？

    QFont font = ui->tabWidget->tabBar()->font();
    font.setWeight(15);
    ui->tabWidget->tabBar()->setFont(font);

    ui->btnLampCalibrate->setEnabled(false);

    // 授权接口对象
    CAuthIntf::setNetworkAccessManager(networkManager());
    authIntf = new CAuthIntf();                                 // TODO: 移到全局管理模块
    authIntf->setDevType(CAuthIntf::authDevType_Screener);

    QObject::connect(this, &CEngineerMode::sigQueryAuthInfo, authIntf, &CAuthIntf::slot__QueryAuthInfo, Qt::QueuedConnection);
    QObject::connect(authIntf, &CAuthIntf::sigQueryAuthInfoFinished, this, &CEngineerMode::slot_authIntf_QueryAuthInfoFinished, Qt::QueuedConnection);

    authIntf->moveToThread(getCommonThread());

    //
    QObject::connect(this, &CEngineerMode::sigDevActivateStatReceived, this, &CEngineerMode::slot_this_DevActivatedChanged, Qt::QueuedConnection);

    // 禁用 SpinBox 的自动全选
    QList<CDoubleSpinBox *> list_spin_box = this->findChildren<CDoubleSpinBox *>();
    for (int i = 0; i < list_spin_box.count(); i++) {
        list_spin_box[i]->setAutoSelectDisabled(true);
    }

    // 灯珠电流等级选项
    ui->cbbLedLevelMiddle->clear();
    ui->cbbLedLevelEccentric->clear();
    for (int i = 0; i < LED_LEVEL_LIST.size(); i++) {
        QString item_str = QString::number(LED_LEVEL_LIST.at(i));
        ui->cbbLedLevelMiddle->addItem(item_str);
        ui->cbbLedLevelEccentric->addItem(item_str);
    }

    // MPro系统推送服务通信地址选项
    static const QString WS_ADDR_TEMPLATE_PRODUCE   = "wss://opt.manylinksmed.com";     // 正式环境地址（格式：“协议://IP:端口”）
    static const QString WS_ADDR_TEMPLATE_TEST      = "ws://120.25.254.38:9006";        // 测试环境地址（格式：“协议://IP:端口”）

    // RK3568 固件，不支持更新开机动画
    ui->updatePsplash_pushbutton->setVisible(false);

}

CEngineerMode::~CEngineerMode()
{
    delete ui;
}

//
void CEngineerMode::syncTrialStat()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": enter ...", CGlobal::LOG_SYS);

    if (g_WifiIntf->getIsConnected()) {
        authIntf->setDevNum(CGlobal::devNum);
        authIntf->setIsUseTestEnv(CGlobal::isDebugMode);

        emit sigQueryAuthInfo();

        authCheckingState = authCheckingState_Querying;
        ui->btnSyncTrialStat->setEnabled(false);
    } else {
        logDebug("wifi not connected, cancel query trial info", CGlobal::LOG_SYS);
        getWinManage()->showSuspensionPrompt(tr("网络未连接，获取试用机信息失败！"));   // "Network not connected, obtaining trial machine information failed!"
    }
}

enAuthCheckingState CEngineerMode::getAuthCheckingState()
{
    return authCheckingState;
}

//
void CEngineerMode::slot_authIntf_QueryAuthInfoFinished(CAuthIntf::enAuthIntfErrType _err_type, QString _err_msg)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": enter ...", CGlobal::LOG_SYS);

    //
    QString err_msg;
    bool is_succ_query = false;

    if (CAuthIntf::authIntfErrType_Succ ==_err_type) {
        const CAuthIntf::stAuthInfo *auth_info = authIntf->getAuthInfo();
        if (auth_info) {
            //
            is_succ_query = true;

            logDebug(QString("query trial info succ: authType = %1, authExpiryDate = %2")
                     .arg(auth_info->authType)
                     .arg(auth_info->expiryDate.toString("yyyy-MM-dd"))
                     , CGlobal::LOG_SYS);

            //
            //bool is_trial_changed = (auth_info->authType != CGlobal::authType);

            // 保存试用机信息
            CGlobal::authType       = auth_info->authType;
            CGlobal::authExpiryDate = (CAuthIntf::authType_Trial == auth_info->authType ? auth_info->expiryDate : QDate(9999, 1, 1));

            //
            //if (is_trial_changed) {
            //    WgtStatusBar::instance()->setTrialDesc(CWinManage::getTrialDesc());
            //}

            // 设备激活状态
            if (auth_info->isDevActivated != CGlobal::isDevActivated) {
                emit sigDevActivateStatReceived(auth_info->isDevActivated);
            }

            //
            CGlobal::saveConfs();       // TODO: 只保存试用机信息？

            // 试用机到期检查
            QString msg_auth, msg_time_sync;
            if (CAuthIntf::authType_NotSet == auth_info->authType) {
                msg_auth = "本机的授权信息未设置，请登录云端设置";
            } else if (CAuthIntf::authType_Trial == auth_info->authType) {
                // 若云端服务器日期与本地不一致，则设置本地日期
                QDate today = auth_info->today;
                if (QDate::currentDate() != today) {
                    logWarning("local date and server date not same!", CGlobal::LOG_SYS);
                    CHardware::setMachineDateTime(QDateTime(today, QTime::currentTime()));
                    msg_time_sync = "\n本地日期与授权服务器日期不一致，已同步为服务器日期。";
                }

                //
                msg_auth = QString("试用机状态同步成功！\n到期日为：") + auth_info->expiryDate.toString(DEFAULT_DATE_FORMAT);
                msg_auth += msg_time_sync;
            } else if (CAuthIntf::authType_Permanent == auth_info->authType) {
                msg_auth = "本机不是试用机。若有疑问，请登录云端检查配置";
            }

            if (msg_auth.length() > 0 && this->isVisible()) {           // 若本窗口可见，则显示试用机状态查询结果
                getWinManage()->showSuspensionPrompt(msg_auth, -1);
            }

            // 设备激活状态的更新
            WgtStatusBar::instance()->updateTitle();
        } else {
            logDebug("query trial info failed: program unknown error!", CGlobal::LOG_SYS);
            err_msg = "获取授权信息失败！\n程序逻辑错误";
        }
    } else {
        logDebug("query trial info failed!", CGlobal::LOG_SYS);
        err_msg = QString("获取授权信息失败！\nerr_code=%1，msg=“%2”").arg((int)_err_type).arg(_err_msg);
    }

    if ((/*err_msg.length() > 0 &&*/ CAuthIntf::authIntfErrType_Succ !=_err_type)
            && (CAuthIntf::authType_Trial == CGlobal::authType || this->isVisible())
            ) {      // 若查询失败，且为试用机状态，则弹出提示
        getWinManage()->showSuspensionPrompt(err_msg, -1);
    }

    authCheckingState = (is_succ_query ? authCheckingState_Succ : authCheckingState_Fail);
    ui->btnSyncTrialStat->setEnabled(true);

    // 若成功，则立即更新状态栏标题
    if (is_succ_query) {
        WgtStatusBar::instance()->updateTitle();
    } else {
        // TODO: 若失败，多尝试几次？


    }

}

void CEngineerMode::slot_mproSysPushSvc_DevActivateStatReceived(Net::Remote::stDevActivateStat _activate_stat)
{
    // 设备激活状态处理
    const bool is_activated = (_activate_stat.activaionStatus == 1);
    if (is_activated != CGlobal::isDevActivated) {
        emit sigDevActivateStatReceived(is_activated);
    }
}

void CEngineerMode::on_btnCameraRestartCnt_clicked()
{
    appSetting::setValue("camera/cameraRestartCnt", 0);
    appSetting::setValue("camera/turnLampFrameListErrorCnt", 0);
    ui->btnCameraRestartCnt->setText("重启|错帧:00|00");
}

void CEngineerMode::on_upgradeFirmwareButton_clicked()
{
    QString msg;
    bool succ = WindowsManagers::checkUdiskAndUpdate(&msg);
    if (!succ && msg.length() > 0) {
        getWinManage()->showMsgWin(/*tr("U 盘更新失败：\n") +*/ msg); // "UDisk update failed:\n"
    }
}

void CEngineerMode::showEvent(QShowEvent *)
{
    // 使底窗口的样式临时转为白色（因本窗体的样式未实现，只支持白色样式）      // TODO: 待优化
    if (themeType_White != CBaseWindow::getInstance()->getTheme()) {
        CBaseWindow::getInstance()->setTheme(themeType_White);
    }

    //
    ui->btnDevActivate->setText(CGlobal::isDevActivated ? UI_STR_DEV_DEACTIVATE : UI_STR_DEV_ACTIVATE);

    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 载入配置值
    globalToUi();       /* 注意：如果是从键盘返回，不应该再调用此函数 */

    // 型号的设置控件，在调试模式下才可见
    ui->gbModel->setVisible(CGlobal::isDebugMode);

    //打开超声和红外灯(测试)
    MySerialPort::instance()->write(openUSandIR);
    isNeedCloseIR = true;

    //
    bleLevelEdited = false;

}

void CEngineerMode::hideEvent(QHideEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    //
//    QString url = QString("/usr/theme");
//     QFile file(url);
//     if (!file.exists()) {
//         if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
//             qDebug()<< "file theme doesn't exit & create failed";
//         }
//         else{
//             QTextStream write_file(&file);
//             write_file<<QString::number(theme,10);
//             file.flush();
//             file.close();
//         }
//     }
//     else
//     {
//         file.open(QIODevice::WriteOnly | QIODevice::Truncate);
//         QTextStream write_file(&file);
//         write_file <<QString::number(theme,10);
//         file.flush();
//         file.close();
//     }

    //关闭超声和红外灯
    if (isNeedCloseIR) {
        MySerialPort::instance()->write(closeUSandIR);
        Util::waitMs(100);
        isNeedCloseIR = false;
    }

    // 使主窗口的样式从临时样式变为系统的样式      // TODO: 待优化
    if (CBaseWindow::getInstance()->getTheme() != getSysThemeType()) {
        CBaseWindow::getInstance()->setTheme(getSysThemeType());
    }

}

void CEngineerMode::on_updatePyDB_pushButton_clicked()
{
    int flag = 0;
    int ret = QMessageBox::question(this,"question","是否更新输入法字库?",QMessageBox::Yes,QMessageBox::No);
    if(ret == QMessageBox::Yes){
        QString disk_path = Util::CUDisk::getPath();
        if(disk_path.length() > 0){
            qDebug() << "found Udisk:" << disk_path;
            flag = 1;//means found Udisk
            QFile programFile(disk_path + QString("/py.db"));
            if(programFile.exists()){
                flag = 2;//means found py.db file
                if(QFile("/bin/py.dbBackup").exists()){
                    qDebug() << "remove old backup main file";
                    QFile("/bin/py.dbBackup").remove();
                }
                QFile oldFile("/bin/py.db");
                if(oldFile.rename("/bin/py.dbBackup"))
                    qDebug() << "rename old py.db file to filename: py.dbBackup";
                else
                    qDebug() << "rename old py.db file failed!";

                if(QFile::copy(programFile.fileName(), QString("/bin/py.db"))){
                    qDebug() << "copy py.db file success!";
                    QMessageBox::information(this,"info","更新成功！请重启设备.",QMessageBox::Ok);
                    return;
                }
                else{
                    qDebug() << "copy py.db file failed!";
                    if(QFile("/bin/py.dbBackup").rename("/bin/py.db")){
                        qDebug("restore old py.db file!");
                        QMessageBox::warning(this,"info","",QMessageBox::Ok);
                    }
                }
            }
        }
    }
    else if(ret == QMessageBox::No){
        return;
    }

    switch (flag) {
    case 0:
        QMessageBox::warning(this,"info","请插入U盘!",QMessageBox::Ok);
        break;
    case 1:
        QMessageBox::warning(this,"info","没有发现py.db文件！",QMessageBox::Ok);
    default:
        break;
    }
}

void CEngineerMode::mousePressEvent(QMouseEvent *e)
{
    static int click_count = 0;

    const int n = 60;
    int w = this->width();
    int x = e->x(), y = e->y();
    if ((x >= w - n && x <= w) && (y >= 0 && y <= n))
        click_count++;

    if(click_count >= 3)
    {
        ui->ckbDebugMode->click();
        click_count = 0;
    }
}

void CEngineerMode::on_led_pushButton_clicked()
{
    if(ledMode>=6)
        ledMode = 0;

    ledMode++;
    qDebug()<<"ledmode ="<<ledMode;

    if(ledMode==1)
        ledTimer->start(2000);
    else
        ledTimer->stop();

    switch (ledMode) {
    case 1:
        MySerialPort::instance()->write(cmd1);
        break;
    case 2:
        MySerialPort::instance()->write(cmd1);
        break;
    case 3:
        MySerialPort::instance()->write(cmd2);
        break;
    case 4:
        MySerialPort::instance()->write(cmd3);
        break;
    case 5:
        MySerialPort::instance()->write(cmd4);
        break;
    case 6:
        MySerialPort::instance()->write(cmd5);
        break;

    default:
        break;
    }
}

void CEngineerMode::cycleTest()
{
   MySerialPort::instance()->write(cmd1);
   qDebug()<<"cycleTest---emit cmd1";
}

void CEngineerMode::updateFont()
{
    int flag = 0;
    int ret = QMessageBox::question(this,"question","是否更新字体?",QMessageBox::Yes,QMessageBox::No);
    if(ret == QMessageBox::Yes){
        QDir target("/usr/font");
        if(!target.exists()){
            if(target.mkdir("/usr/font"))
                qDebug()<<"create dir /usr/font success";
            else{
                qDebug()<<"create dir /usr/font failed";
                return;
            }
        }

        QString disk_path = Util::CUDisk::getPath();

        if(disk_path.length() > 0) {
            qDebug() << "found Udisk:" << disk_path;
            flag = 1;//means found Udisk
            QFile programFile(disk_path + QString("/msyh.ttf"));
            if(programFile.exists()){
                flag = 2;//means found msyh.ttf file
                if(QFile("/usr/font/msyh.ttf").exists()){
                    qDebug() << "msyh.ttf is exists";
                    QMessageBox::information(this,"info","字体已存在!",QMessageBox::Ok);
                    return;
                }

                if(QFile::copy(programFile.fileName(), QString("/usr/font/msyh.ttf"))){
                    qDebug() << "copy  success:"<< programFile.fileName();
                    QMessageBox::information(this,"info","更新成功！请重启设备.",QMessageBox::Ok);
                    return;
                }
                else{
                    qDebug() << "copy failed:"<< programFile.fileName();
                    QMessageBox::warning(this,"info","更新失败!",QMessageBox::Ok);

                }
            }
        }
    }
    else if(ret == QMessageBox::No){
        return;
    }

    switch (flag) {
    case 0:
        QMessageBox::warning(this,"info","请插入U盘!",QMessageBox::Ok);
        break;
    case 1:
        QMessageBox::warning(this,"info","没有发现msyh.ttf文件！",QMessageBox::Ok);
    default:
        break;
    }
}

void CEngineerMode::updatePsplash()
{
    int flag = 0;
    int ret = QMessageBox::question(this,"question","是否更psplash?",QMessageBox::Yes,QMessageBox::No);
    if(ret == QMessageBox::Yes){
        QString disk_path = Util::CUDisk::getPath();
        if(disk_path.length() > 0){
            qDebug() << "found Udisk:" << disk_path;
            flag = 1;//means found Udisk
            QFile programFile1(disk_path + QString("/psplash"));
            if(programFile1.exists()){
                flag = 2;//means found psplash file
                QFile targetFile("/usr/bin/psplash");
                if(targetFile.exists()){
                    qDebug() << "psplash is exists";
                    targetFile.remove();
                }

                if(QFile::copy(programFile1.fileName(),QString("/usr/bin/psplash"))){
                    qDebug() << "copy  success:"<< programFile1.fileName();
                    QMessageBox::information(this,"info","更新psplash成功！请重启设备.",QMessageBox::Ok);
                }
                else{
                    qDebug() << "copy failed:"<< programFile1.fileName();
                    QMessageBox::warning(this,"info","psplash更新失败!",QMessageBox::Ok);

                }
            }

            QFile programFile2(disk_path + QString("/psplash-write"));
            if(programFile2.exists()){
                flag = 2;//means found psplash-write file
                QFile targetFile("/usr/bin/psplash-write");
                if(targetFile.exists()){
                    qDebug() << "psplash-write is exists";
                    targetFile.remove();
                }

                if(QFile::copy(programFile2.fileName(),QString("/usr/bin/psplash-write"))){
                    qDebug() << "copy  success:"<< programFile2.fileName();
                    QMessageBox::information(this,"info","更新psplash-write成功！请重启设备.",QMessageBox::Ok);
                }
                else{
                    qDebug() << "copy failed:"<< programFile2.fileName();
                    QMessageBox::warning(this,"info","psplash-write更新失败!",QMessageBox::Ok);

                }
            }
        }
    }
    else if(ret == QMessageBox::No){
        return;
    }

    switch (flag) {
    case 0:
        QMessageBox::warning(this,"info","请插入U盘!",QMessageBox::Ok);
        break;
    case 1:
        QMessageBox::warning(this,"info","没有发现 psplash 文件！",QMessageBox::Ok);
    default:
        break;
    }
}

void CEngineerMode::slot_this_DevActivatedChanged(bool _is_activated)
{
    Q_UNUSED(_is_activated)

    //
    if (this->isVisible()) {
        ui->btnDevActivate->setText(CGlobal::isDevActivated ? UI_STR_DEV_DEACTIVATE : UI_STR_DEV_ACTIVATE);
    }
}

void CEngineerMode::on_btnGetLocalIp_clicked()
{
    QString ipAddr;
    QList<QNetworkInterface> network = QNetworkInterface::allInterfaces();
    for (QNetworkInterface i : network) {
        QString netName = i.humanReadableName();
        qDebug()<<netName;
        if (netName == "无线网络连接") {      // TODO: 这个文本匹配可靠吗？不同语言环境的问题？
            qDebug()<<i.hardwareAddress();
            QList<QNetworkAddressEntry> ipAll = i.addressEntries();
            for (QNetworkAddressEntry ip : ipAll) {
                //if(ip.ip().protocol()==QAbstractSocket::IPv4Protocol)
                ipAddr = ip.ip().toString();
            }
        }
    }
    qDebug()<<"ipAddr = "<<ipAddr;
}

void CEngineerMode::on_exportLog_pushButton_clicked()        // TODO: 旧的日志功能作废，导出日志功能要重写
{
    QString msg = "";
    QString disk_path = Util::CUDisk::getPath();

    if(disk_path != "") {
        int count_fail = 0;
        int count_succ = CLoggerHelper::copyLogToDir(disk_path + "/Logs_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmm"), &count_fail);
        msg = QString::asprintf("导出 log 文件 %d 个", count_succ);
    }
    else{
        msg = "没有发现U盘!";
    }

    MessageWin msg_win;
    msg_win.setContent(msg);
    msg_win.exec();

    Util::CUDisk::sync();
    //Util::CUDisk::umount();
}

void CEngineerMode::on_curve_comboBox_currentTextChanged(const QString &arg1)
{
    return;
    qDebug()<<"--select curve:"<<arg1;
    CAlgoInvoker::setCurve(arg1.toInt());
    appSetting::setValue("tool/curve",CAlgoInvoker::getCurve());

    QString verStr = QString::number(verDate,10);
    if(CAlgoInvoker::getCurve()==1){    verStr.append("-低曲线");}
    if(CAlgoInvoker::getCurve()==2){    verStr.append("-中曲线");}
    if(CAlgoInvoker::getCurve()==3){    verStr.append("-高曲线");}
    ui->version->setText(verStr);
    qDebug()<<"--curve:"<<CAlgoInvoker::getCurve();
}

void CEngineerMode::on_exportDB_pushButton_clicked()
{
    QString msg;
    QString disk_path = Util::CUDisk::getPath();
    if(disk_path != ""){
        bool is_succ = false;

        QString db_file_name = MySQLitePatients::getInstance()->databaseFileName();
        QString old_file_path = MySQLitePatients::getInstance()->databaseFilePath();

        QString name;
        QString ext_name;
        if (Util::separateFileName(db_file_name, name, ext_name)) {
            QString new_file_path = disk_path + QDir::separator() + name + QDateTime::currentDateTime().toString("_yyyyMMdd_HHmm.") + ext_name;
            is_succ = QFile::copy(old_file_path, new_file_path);
        }

        if (is_succ) {
            Util::CUDisk::sync();
            //Util::CUDisk::umount();

            msg = "导出成功!";
        } else {
            qDebug()<<"导出文件失败!";
            msg = "导出失败!";
        }
    } else {
        msg.append("没有发现U盘!");
    }

    MessageWin win_msg;
    win_msg.setContent(msg);
    win_msg.exec();
}

void CEngineerMode::on_btnUnitDebug_clicked()
{
    if (!winUnitTest) {
        winUnitTest = new CWinUnitTest;
    }
    winUnitTest->show();
}

void CEngineerMode::on_pushButton_Poweroff_clicked()
{
    //
    QString text = tr("是否关机？"); // "Whether or not to off?"
    NoticeWin msg;
    msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    msg.setContent(text);
    if(msg.exec() == QDialog::Accepted)
    {
        qDebug()<<"-----PowerOff";
        globalService()->powerOff();
    }
}

void CEngineerMode::on_pushButton_Reboot_clicked()
{
    QString text = tr("是否关机重启？");   // "Whether to shut down and restart?"
    NoticeWin msg;
    msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    msg.setContent(text);
    if (msg.exec() == QDialog::Accepted) {
        globalService()->reboot();
    }
}

void CEngineerMode::on_ckbBluetoothUi_clicked(bool checked)
{
#if (OS_TYPE == 1)
    g_hasBluetooth = checked;
    appSetting::setValue("tool/bluetoothvisble", g_hasBluetooth);
#endif
}

void CEngineerMode::on_pushButton_back_clicked()
{
    // 数值检查
    QString number = ui->edtDevNum->text();
    for (const QChar &c : number) {                             // 编号不可包含非法字符
        if (!c.isLetterOrNumber() && c != '_' && c != '-') {
            getWinManage()->showMsgWin("编号只能包含英文字母、数字、'-'、'_'等字符！");
            return;
        }
    }

    int dist_tolerance = ui->edtDistTolerance->text().toInt();      // 距离允差
    if (!(dist_tolerance > 10)) {
        getWinManage()->showMsgWin("距离允差须 > 10");
        return;
    }

    int frame_rate = ui->edtFrameRate->text().toInt();              // 帧率
    if (!(frame_rate > 0)) {
        getWinManage()->showMsgWin("帧率须 > 0");
        return;
    }

    int max_gaze = ui->edtMaxGazeDeviation->text().toInt();         // 最大固视偏差
    if (!(max_gaze > 0)) {
        getWinManage()->showMsgWin("“最大固视偏差”的值太小");
        return;
    } else if (!(max_gaze <= 30)) {
        getWinManage()->showMsgWin("“最大固视偏差”的值太大");
        return;
    }

    double trigger_interval_delay_ms = ui->edtHardTriggerIntervalDelayMs->text().toDouble();     // 触发间隔附加延时（毫秒）
    if (trigger_interval_delay_ms < 0 || trigger_interval_delay_ms > 20) {
        getWinManage()->showMsgWin("“触发间隔附加延时”超出有效范围（0~20）");
        return;
    }

    double trigger_delay_ms = ui->edtHardTriggerDelayMs->text().toDouble();    // 触发延时（毫秒）
    if (trigger_delay_ms < 0 || trigger_delay_ms > 10) {
        getWinManage()->showMsgWin("“触发延时”超出有效范围（0~10）");
        return;
    }

    // 保存配置
    uiToGlobal();
    CGlobal::saveConfs();

    // 重设灯珠电流等级
    if (bleLevelEdited && CGlobal::isSetLedLevel) {
        g_WinMeasure->setLedLevel(1000, true);
    }

    // 转到“工具”页面
    getWinManage()->showWindowByType(WIN_TOOL);
}

void CEngineerMode::on_btnQuit_clicked()
{
    globalService()->closeApp();
}

void CEngineerMode::on_btnQuitToToolkits_clicked()
{
#if (1 == OS_TYPE || 2 == OS_TYPE)
    QString app_name = "main-toolkits";
#else
    QString app_name = "screener-toolkits";
#endif
    QString name_sub = "_desktop";

    if (QCoreApplication::applicationName().endsWith(name_sub))
        app_name += name_sub;
    QString file_path = qApp->applicationDirPath() + QDir::separator() + app_name;
    system((file_path + " &").toLocal8Bit().data());

    //
    if (!QFile::exists(file_path)) {
        getWinManage()->showMsgWin("File not found!");
        return;
    }

    //
    globalService()->closeApp();
}

void CEngineerMode::on_btnRestoreDefault_clicked()
{
    restoreValues();
}

void CEngineerMode::on_btnShellSimulate_clicked()
{
    logDebug("CEngineerMode::on_btnShellSimulate_clicked() into ...");

    if (!shell) {
        shell = new shellsimulate(this);
        //shell->setModal(true);
    }

    shell->exec();
    //shell->show();

    logDebug("::on_btnShellSimulate_clicked() out");
}

bool CEngineerMode::setIntValAfterEditChanged(myEditLine *_edit, QString _str_new, int _val_old)
{
    static bool is_editing = false;
    if (is_editing)
        return false;           // 防止函数内再次触发该事件而造成死循环（实测 setText() 函数会触发 textChanged() 信号）      // TODO: 改用 textEdited() 事件？

    logDebug("::setIntValAfterEditChanged() into ...", CGlobal::LOG_ALGO);

    is_editing = true;          // TODO: try ... catch ?
    bool is_changed = false;    // 值是否已改变

    bool is_ok;
    int val_new = _str_new.toInt(&is_ok, 10);       // 得到新值
    if (is_ok) {
        if (val_new != _val_old) {
            is_changed = true;
        }
    } else {
        getWinManage()->showMsgWin(QString("\"%1\" 不是合法的整数！").arg(_str_new), true, "OK");
    }

    QString val_new_str;        // 新值转换的字符串
    if (is_changed) {
        val_new_str = QString::number(val_new);
    } else {
        val_new_str = QString::number(_val_old);
    }

    if (val_new_str != _str_new) {
        _edit->setText(val_new_str);        // 因为经过了转换，回写以避免实际实际值与显示值不一致
    }

    is_editing = false;
    return is_changed;
}

// 文本框字符串修改后，检查及修正用户输入的内容
bool CEngineerMode::setFloatValAfterEditChanged(myEditLine *_edit, QString _str_new, float _val_old)
{
    static bool is_editing = false;
    if (is_editing)
        return false;           // 防止函数内再次触发该事件而造成死循环（实测 setText() 函数会触发 textChanged() 信号）

    logDebug("::setFloatValAfterEditChanged() into ...", CGlobal::LOG_ALGO);

    is_editing = true;          // TODO: try ... catch ?
    bool is_changed = false;    // 值是否已改变

    bool is_ok;
    float val_new = _str_new.toFloat(&is_ok);       // 得到新值
    if (is_ok) {
        if (val_new != _val_old) {
            is_changed = true;
        }
    } else {
        getWinManage()->showMsgWin(QString("\"%1\" 不是合法的小数！").arg(_str_new), true, "OK");
    }

    QString val_new_str;        // 新值转换的字符串
    if (is_changed) {
        val_new_str = QString::number(val_new, 'f', 1);
    } else {
        val_new_str = QString::number(_val_old, 'f', 1);
    }

    if (val_new_str != _str_new) {
        _edit->setText(val_new_str);        // 因为经过了转换，回写以避免实际实际值与显示值不一致
    }

    is_editing = false;
    return is_changed;
}

void CEngineerMode::on_edtMinPupilParamRatio_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setIntValAfterEditChanged(edt, arg1, CGlobal::getMinPupilParamRatio());
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtMaxAlgoFail_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setIntValAfterEditChanged(edt, arg1, CGlobal::maxAlgoFail);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtPupilAverageMin_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setIntValAfterEditChanged(edt, arg1, CGlobal::pupilAverageMin_);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtPupilAverageMax_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setIntValAfterEditChanged(edt, arg1, CGlobal::pupilAverageMax);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtExposureMsMin_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setFloatValAfterEditChanged(edt, arg1, CGlobal::exposureMsMin);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtExposureMsMax_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setFloatValAfterEditChanged(edt, arg1, CGlobal::exposureMsMax);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtExpoCoarseAdjStepMs_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setFloatValAfterEditChanged(edt, arg1, CGlobal::expoCoarseAdjStepMs);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtMinPupilStdDev_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setFloatValAfterEditChanged(edt, arg1, CGlobal::minPupilStaDev);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtCaptureInterval_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setIntValAfterEditChanged(edt, arg1, CGlobal::captureInterval);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }

    //
    static const int MAX_INTERVAL = 120;
    if (edt->text().toInt() > MAX_INTERVAL) {
        getWinManage()->showMsgWin(QString("最大值不能超过 %1 ！").arg(MAX_INTERVAL), true, "OK");
        edt->setText(QString::number(MAX_INTERVAL));
    }
}

void CEngineerMode::on_edtPupilDetectedCount_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setIntValAfterEditChanged(edt, arg1, CGlobal::minPupilDetectedCount);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_edtDistanceInterval_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setIntValAfterEditChanged(edt, arg1, CGlobal::distanceInterval2);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }

    //
    static const int MAX_INTERVAL = 1000;
    if (edt->text().toInt() > MAX_INTERVAL && !CGlobal::isDebugMode) {
        getWinManage()->showMsgWin(QString("最大值不能超过 %1 ！").arg(MAX_INTERVAL), true, "OK");
        edt->setText(QString::number(MAX_INTERVAL));
    }
}

void CEngineerMode::on_edtDistanceFix_textEdited(const QString &arg1)
{
    myEditLine *edt = dynamic_cast<myEditLine *>(sender());
    if (edt) {
        setIntValAfterEditChanged(edt, arg1, CGlobal::distanceOffset);
    } else {
        getWinManage()->showSuspensionPrompt("ProgramError: Failed to get Edit Widget!");
        return;
    }
}

void CEngineerMode::on_ckbDebugMode_clicked(bool _checked)
{
    CGlobal::isDebugMode = _checked;

    //
    showDebugModeCtrls(CGlobal::isDebugMode);

    // 型号的设置控件，在调试模式下才可见
    ui->gbModel->setVisible(CGlobal::isDebugMode);

}

// 设置调试控件的显示和隐藏
void CEngineerMode::showDebugModeCtrls(bool _is_debug_mode)
{
    //ui->btnUnitDebug->setVisible(_is_debug_mode);

    //ui->wgtPupilAverageRange->setVisible(_is_debug_mode);
    //ui->wgtExposureRange->setVisible(_is_debug_mode);
    ui->wgtMinPupilStdDev->setVisible(_is_debug_mode);
    ui->wgtPupilDetectedCount->setVisible(_is_debug_mode);

    ui->btnShellSimulate->setVisible(_is_debug_mode);
    //ui->ckbDebugMode->setVisible(_is_debug_mode);

    ui->frmAlgo->setVisible(_is_debug_mode);
    ui->ckbSpecifiedAlgo->setVisible(_is_debug_mode);
    ui->ckbPupilAccutrately->setVisible(_is_debug_mode);

    ui->btnGetContrast->setVisible(_is_debug_mode);
    ui->btnResetCameraParm->setVisible(_is_debug_mode);

    ui->ckbIsInvertImg->setVisible(_is_debug_mode);

    ui->lblDistanceTypeLabel->setVisible(/*_is_debug_mode*/ false);     // NOTE: (2026-08-07)目前只有超声和 SIMAN_SDM10 两种测距类型，且后者自动检测是否存在
    ui->cbbDistanceType->setVisible(     /*_is_debug_mode*/ false);     // NOTE: (2026-08-07)目前只有超声和 SIMAN_SDM10 两种测距类型，且后者自动检测是否存在

}

// 配置值从全局变量载入到界面控件（部分变量暂时没有在全局变量定义，先从配置文件载入）    // TODO: 全部配置值都在内存有创建
/* 注意：如果是从键盘返回，不应该再调用此函数 */
void CEngineerMode::globalToUi()
{
    // 产品编号
    ui->edtDevNum->setText(CGlobal::devNum);

    // 曲线

    // 测距调节
    ui->edtDistanceFix->setText(QString::number(CGlobal::distanceOffset));

    // 产品型号
    ui->rbtnProductSl100S->setChecked(productModel_SL100S == CGlobal::productModel);
    ui->rbtnProductSl100->setChecked(productModel_SL100 == CGlobal::productModel);
    ui->rbtnProductSl100P->setChecked(productModel_SL100P == CGlobal::productModel);

    // 彩灯和声音
    ui->ckbIsColoredLampEnabled->setChecked(CGlobal::getIsColoredLampEnabled());
    ui->ckbIsMusicEnabled->setChecked(CGlobal::getIsMusicEnabled());

    //
    int cnt_camera_restart = appSetting::value("camera/cameraRestartCnt").toInt();
    int cnt_frame_err = appSetting::value("camera/turnLampFrameListErrorCnt").toInt();
    ui->btnCameraRestartCnt->setText(QString("重启|错帧:%1|%2").arg(cnt_camera_restart).arg(cnt_frame_err));

    // 开启密码
    ui->ckbEnableEngineerPwd->setChecked(s_isPasswordEnabled);

    //
    ui->ckbAutoTest->setChecked(g_AutoTest);

    // ~开启存图
    //saveImage = appSetting::value("tool/saveimage").toBool();
    ui->ckbSaveImage->setChecked(saveImage);

    // 显示蓝牙
#if (OS_TYPE == 1)
    g_hasBluetooth = appSetting::value("tool/bluetoothvisble", g_hasBluetooth).toBool();
#endif
    ui->ckbBluetoothUi->setChecked(g_hasBluetooth);

    ui->edtMinPupilParamRatio->setText(QString::number(CGlobal::getMinPupilParamRatio()));
    ui->edtMaxAlgoFail->setText(QString::number(CGlobal::maxAlgoFail));
    ui->edtPupilAverageMin->setText(QString::number(CGlobal::pupilAverageMin_));
    ui->edtPupilAverageMax->setText(QString::number(CGlobal::pupilAverageMax));
    ui->edtExposureMsMin->setText(QString::number(CGlobal::exposureMsMin));
    ui->edtExposureMsMax->setText(QString::number(CGlobal::exposureMsMax));
    ui->edtExpoCoarseAdjStepMs->setText(QString::number(CGlobal::expoCoarseAdjStepMs));
    ui->edtMinPupilStdDev->setText(QString::number(CGlobal::minPupilStaDev, 'f', 1));
    ui->edtCaptureInterval->setText(QString::number(CGlobal::captureInterval));

    // 允许统计值
    ui->ckbIsStatisticalEnabled->setChecked(CGlobal::isStatisticalEnabled);
    ui->ckbIsEnableMonthAgeVision->setChecked(CGlobal::isEnableMonthAgeVision);
    ui->edtPupilDetectedCount->setText(QString::number(CGlobal::minPupilDetectedCount));

    //
    ui->ckbLogEnabled->setChecked(CGlobal::getIsLogEnabled());
    ui->ckbLogToFile->setChecked(CGlobal::getIsLogToFile());

    // 算法
    ui->cbbPupilAlgoVer->setCurrentIndex(CGlobal::getPupilAlgoVerCfg());
    ui->ckbPupilAccutrately->setChecked(CGlobal::isPupilAccutrately);

    ui->ckbSpecifiedAlgo->setChecked(CGlobal::isSpecifiedAlgo);

    //
    ui->ckbDebugMode->setChecked(CGlobal::isDebugMode);

    // ~显示电压
    //voltageState = appSetting::value("tool/voltage").toBool();
    //if (CGlobal::isDebugMode) {
    //    voltageState = true;
    //}
    ui->ckbShowVoltage->setChecked(voltageState);

    //
    //int idx_dist_type = ui->cbbDistanceType->findData((int)CGlobal::distSensorType);
    //if (idx_dist_type < 0 || idx_dist_type > ui->cbbDistanceType->count() - 1) {
    //    logCritical(QString("%1::%2(): CGlobal::distSensorType(=%3) is out of bound(0~%4)").arg(S_CLASS_NAME).arg(__FUNCTION__)
    //                .arg(idx_dist_type).arg(ui->cbbDistanceType->count() - 1));
    //    idx_dist_type = 0;
    //}
    //ui->cbbDistanceType->setCurrentIndex(idx_dist_type);

    ui->edtDistanceInterval->setText(QString::number(CGlobal::distanceInterval2));
    ui->cbbDistanceUnit->setCurrentIndex((int)CGlobal::distanceUnit);

    // 超声系数
    ui->edtUltraCoefficient->setText(QString::number(CGlobal::ultraCoefficient/*, 'f', 10*/));

    // 距离允差
    ui->edtDistTolerance->setText(QString::number(CGlobal::distTolerance));

    // 是否倒转图像
    ui->ckbIsInvertImg->setChecked(CGlobal::isInvertImg);

    // 是否设置灯珠电流
    ui->ckbIsSetLedLevel->setChecked(CGlobal::isSetLedLevel);

    // 灯珠电流等级
    int idx_middle = LED_LEVEL_LIST.indexOf(CGlobal::ledLevelMiddle);           // 中心灯珠
    int idx_eccentric = LED_LEVEL_LIST.indexOf(CGlobal::ledLevelEccentric);     // 偏心灯珠
    ui->cbbLedLevelMiddle->setCurrentIndex(idx_middle);
    ui->cbbLedLevelEccentric->setCurrentIndex(idx_eccentric);

    // 自控模式时是否手动转灯
    ui->ckbIsManualTurnLamp->setChecked(!CGlobal::isAutoTurnLampWhenSelfControl);

    // 放大瞳孔
    ui->ckbIsMagnifyPupilImg->setChecked(CGlobal::isMagnifyPupilImg);

    // 帧率
    ui->edtFrameRate->setText(QString::number(CGlobal::frameRate));

    // 是否单线程
    ui->ckbIsSingleThreadCalc->setChecked(CGlobal::isSingleThreadCalc);

    // 日志等级
    ui->cbbLogLevelMin->setCurrentIndex((int)CGlobal::getLogLevel());

    // 最大固视偏差
    ui->edtMaxGazeDeviation->setText(QString::number(CGlobal::maxGazeDeviation));

    // 是否支持模拟眼
    ui->ckbIsSimulatedEye->setChecked(CGlobal::isSimulatedEye);

    // 结果修正
    ui->spbResultCorrectSph_General->setValue(  CGlobal::resultCorrectSph_General   );
    ui->spbResultCorrectCyl_General->setValue(  CGlobal::resultCorrectCyl_General   );
    ui->spbResultCorrectSph_Square->setValue(   CGlobal::resultCorrectSph_Square    );
    ui->spbResultCorrectCyl_Square->setValue(   CGlobal::resultCorrectCyl_Square    );
    ui->spbResultCorrectSph_LSharp->setValue(   CGlobal::resultCorrectSph_LSharp    );
    ui->spbResultCorrectCyl_LSharp->setValue(   CGlobal::resultCorrectCyl_LSharp    );

    // 瞳孔长宽比
    ui->edtEyeWhRatio_Model->setText(QString::number(CGlobal::eyeWhRatio_Model));
    ui->edtEyeWhRatio_Human->setText(QString::number(CGlobal::eyeWhRatio_Human));

    // 结果稳定阈值
    ui->edtResultStableCountThresh->setText(QString::number(CGlobal::resultStableCountThresh));
    ui->edtResultStableDiopterThresh->setText(QString::number(CGlobal::resultStableDiopterThresh, 'f', 1));

    // 最大测量次数
    ui->edtCountMaxMeasureTimes->setText(QString::number(CGlobal::countMaxMeasureTimes));

    // 触发间隔附加延时（毫秒）
    ui->edtHardTriggerIntervalDelayMs->setText(QString::number(CGlobal::hardTriggerIntervalDelayMs));

    // 触发延时（毫秒）
    ui->edtHardTriggerDelayMs->setText(QString::number(CGlobal::hardTriggerDelayMs));

    // 触发输入类型
    ui->cbbTriggerInputType->setCurrentIndex(static_cast<int>(CGlobal::triggerInputType));

    // 是否转灯调试模式
    ui->ckbIsTurnLampTestMode->setChecked(g_isTurnLampTestMode);

    // ======================================
    // 设置控件的显示和隐藏
    showDebugModeCtrls(CGlobal::isDebugMode);

    ui->wgtMinPupilParamRatio->setVisible(false);       // 隐藏掉“最小瞳孔比例”（无意义的设置？） // NOTE: 只有最旧版的算法用到

}

// 设置值从界面控件保存到全局变量（部分变量暂时没有在全局变量定义，先直接保存到文件）    // TODO: 全部配置值都在内存有创建
/***
 * 只有那些需要做数据检查和修正的或需要即时生效的输入控件，才需要监听它的值修改事件。
 * 且在它的事件函数里只做数据检查修正，数据的保存，在离开窗口时才执行 。
 */
/* 所有需保存到配置文件的且不需立即更新的值，都放到这里保存 */
// TODO: 把对配置文件的访问分离到 global 模块，本模块只与 global 模块耦合
// TODO: 这种方式的弊端是会保存所有值，不管有没有被修改过？
void CEngineerMode::uiToGlobal()
{
    // 产品编号
    CGlobal::devNum = ui->edtDevNum->text();

    // 曲线

    // 测距调节
    CGlobal::distanceOffset = ui->edtDistanceFix->text().toInt();

    // 产品型号
    CGlobal::productModel = (ui->rbtnProductSl100S->isChecked() ?
                                 productModel_SL100S :
                                 (ui->rbtnProductSl100->isChecked() ? productModel_SL100 : productModel_SL100P)
                                 );

    // 彩灯和声音
    CGlobal::setIsColoredLampEnabled(ui->ckbIsColoredLampEnabled->isChecked());
    CGlobal::setIsMusicEnabled(ui->ckbIsMusicEnabled->isChecked());

    // 开启密码
    s_isPasswordEnabled = ui->ckbEnableEngineerPwd->isChecked();

    // 显示蓝牙
#if (OS_TYPE == 1)
    g_hasBluetooth = ui->ckbBluetoothUi->isChecked();
    appSetting::setValue("tool/bluetoothvisble", g_hasBluetooth);
#endif

    // 允许统计值
    CGlobal::isStatisticalEnabled = ui->ckbIsStatisticalEnabled->isChecked();
    CGlobal::isEnableMonthAgeVision = ui->ckbIsEnableMonthAgeVision->isChecked();
    CGlobal::minPupilDetectedCount = ui->edtPupilDetectedCount->text().toInt();

    // ~显示电压
    voltageState = ui->ckbShowVoltage->isChecked();
    //appSetting::setValue("tool/voltage", voltageState);

    // 开启屏幕截图
    isPrintScreen = ui->ckbEnablePrintScreen->isChecked();

    // 开启连拍模式
    g_AutoTest = ui->ckbAutoTest->isChecked();

    // 连拍时间间隔
    CGlobal::captureInterval = ui->edtCaptureInterval->text().toInt();

    // 最大转灯次数
    CGlobal::maxAlgoFail = ui->edtMaxAlgoFail->text().toInt();

    // 调试模式
    CGlobal::isDebugMode = ui->ckbDebugMode->isChecked();

    // 算法
    CGlobal::setPupilAlgoVer((enAlgoVerAll)ui->cbbPupilAlgoVer->currentIndex());

    CGlobal::isSpecifiedAlgo = ui->ckbSpecifiedAlgo->isChecked();

    CGlobal::isPupilAccutrately = ui->ckbPupilAccutrately->isChecked();

    // 瞳孔灰度范围
    CGlobal::pupilAverageMin_ = ui->edtPupilAverageMin->text().toInt();
    CGlobal::pupilAverageMax = ui->edtPupilAverageMax->text().toInt();

    // 曝光时间范围
    CGlobal::exposureMsMin = ui->edtExposureMsMin->text().toFloat();
    CGlobal::exposureMsMax = ui->edtExposureMsMax->text().toFloat();
    CGlobal::expoCoarseAdjStepMs = ui->edtExpoCoarseAdjStepMs->text().toFloat();

    // 最小标准偏差
    CGlobal::minPupilStaDev = ui->edtMinPupilStdDev->text().toFloat();

    // ~开启存图。关闭时停止接收新任务，后台排空后统一刷盘。
    const bool wasSaveImage = saveImage;
    saveImage = ui->ckbSaveImage->isChecked();
    if (g_WinMeasure != Q_NULLPTR && g_WinMeasure->measureCtrl() != Q_NULLPTR) {
        if (saveImage) {
            g_WinMeasure->measureCtrl()->setTurnLampImageSaveEnabled(true);
        } else {
            g_WinMeasure->measureCtrl()->setTurnLampImageSaveEnabled(false);
            if (wasSaveImage) {
                g_WinMeasure->measureCtrl()->flushTurnLampImageSaveQueue();
            }
        }
    }
    //appSetting::setValue("tool/saveimage", saveImage);

    // 启用 Log
    CGlobal::setIsLogEnabled(ui->ckbLogEnabled->isChecked());

    // Log 到文件
    CGlobal::setIsLogToFile(ui->ckbLogToFile->isChecked());

    // 最小瞳孔比例
    CGlobal::setMinPupilParamRatio(ui->edtMinPupilParamRatio->text().toInt());

    // 测距类型及时间间隔
    //enDistSensorType dist_type_old = CGlobal::distSensorType;
    //
    //QVariant var_dist_type = ui->cbbDistanceType->currentData();
    //CGlobal::distSensorType = (enDistSensorType)(Util::variantToInt(var_dist_type, -1));  /* 目前正式产品中只有一种测距模块类型，为了避免配置错误而导致故障，禁用这项配置的保存 */
    //
    //CGlobal::distanceInterval2 = ui->edtDistanceInterval->text().toInt();
    //
    //CGlobal::distanceUnit = (enDistanceUnit)ui->cbbDistanceUnit->currentIndex();
    //
    //if (dist_type_old != CGlobal::distSensorType) {
    //    g_WinMeasure->distanceDetect()->setSensorType(CGlobal::distSensorType);
    //}

    // 超声系数
    QString str_coefficient = ui->edtUltraCoefficient->text();
    CGlobal::ultraCoefficient = str_coefficient.toDouble();
    appSetting::setValue("global/ultraCoefficient", CGlobal::ultraCoefficient);

    // 距离允差
    CGlobal::distTolerance = ui->edtDistTolerance->text().toInt();

    // 是否倒转图像
    CGlobal::isInvertImg = ui->ckbIsInvertImg->isChecked();

    // 是否设置灯珠电流
    CGlobal::isSetLedLevel = ui->ckbIsSetLedLevel->isChecked();

    // 灯珠电流等级
    CGlobal::ledLevelMiddle = LED_LEVEL_LIST.at(ui->cbbLedLevelMiddle->currentIndex());         // 中心灯珠
    CGlobal::ledLevelEccentric = LED_LEVEL_LIST.at(ui->cbbLedLevelEccentric->currentIndex());   // 偏心灯珠

    // 自控模式时是否手动转灯
    CGlobal::isAutoTurnLampWhenSelfControl = !ui->ckbIsManualTurnLamp->isChecked();

    // 放大瞳孔
    CGlobal::isMagnifyPupilImg = ui->ckbIsMagnifyPupilImg->isChecked();

    // 帧率
    CGlobal::frameRate = ui->edtFrameRate->text().toInt();

    // 是否单线程
    CGlobal::isSingleThreadCalc = ui->ckbIsSingleThreadCalc->isChecked();

    // 日志等级
    CGlobal::setLogLevel((enLogLevel)ui->cbbLogLevelMin->currentIndex());

    // 最大固视偏差
    CGlobal::maxGazeDeviation = ui->edtMaxGazeDeviation->text().toInt();

    // 是否支持模拟眼
    CGlobal::isSimulatedEye = ui->ckbIsSimulatedEye->isChecked();

    // 结果修正
    CGlobal::resultCorrectSph_General   = ui->spbResultCorrectSph_General->value();
    CGlobal::resultCorrectCyl_General   = ui->spbResultCorrectCyl_General->value();
    CGlobal::resultCorrectSph_Square    = ui->spbResultCorrectSph_Square->value();
    CGlobal::resultCorrectCyl_Square    = ui->spbResultCorrectCyl_Square->value();
    CGlobal::resultCorrectSph_LSharp    = ui->spbResultCorrectSph_LSharp->value();
    CGlobal::resultCorrectCyl_LSharp    = ui->spbResultCorrectCyl_LSharp->value();

    // 瞳孔长宽比
    CGlobal::eyeWhRatio_Model = ui->edtEyeWhRatio_Model->text().toFloat();
    CGlobal::eyeWhRatio_Human = ui->edtEyeWhRatio_Human->text().toFloat();

    // 结果稳定次数阈值
    CGlobal::resultStableCountThresh = ui->edtResultStableCountThresh->text().toUInt();
    CGlobal::resultStableDiopterThresh = ui->edtResultStableDiopterThresh->text().toDouble();

    // 最大测量次数
    CGlobal::countMaxMeasureTimes = ui->edtCountMaxMeasureTimes->text().toUInt();

    // 触发间隔附加延时（毫秒）
    CGlobal::hardTriggerIntervalDelayMs = ui->edtHardTriggerIntervalDelayMs->text().toDouble();

    // 触发延时（毫秒）
    CGlobal::hardTriggerDelayMs = ui->edtHardTriggerDelayMs->text().toDouble();

    // 触发输入类型
    CGlobal::triggerInputType = static_cast<enTriggerInputType>(ui->cbbTriggerInputType->currentIndex());
    enTriggerInputType curr_trigger_type;
    stCameraStatInfo stat_info = g_CameraIntf->getTriggerInputType(curr_trigger_type);
    if (cameraStat_Succ == stat_info.cameraStat && curr_trigger_type != CGlobal::triggerInputType) {
        g_CameraIntf->setTriggerInputType(CGlobal::triggerInputType);
    }

    // =========================================================
    // 保存到配置文件
    CGlobal::saveConfs();       // TODO: 本函数内前面的 appSetting::setValue() 移到这个函数内，检查

}

// 恢复默认值
// TODO: 实现一定的自动化能力（数据感知？），提高开发效率
void CEngineerMode::restoreValues()
{
    // TODO: 某些不应重置的值？

    // TODO: 实现方式改为删除配置文件，然后重启？
    // TODO: 产品编号的从底板查询？


    //
    CGlobal::restoreConfs();
    CGlobal::saveConfs();

    //
    globalToUi();

}

void CEngineerMode::on_cbbLogFilter_activated(int index)
{
    QString tag = ui->cbbLogFilter->itemData(index).toString();
    CGlobal::setFilter(tag);
}

void CEngineerMode::on_ckbSpecifiedAlgo_clicked(bool checked)
{
    CGlobal::isSpecifiedAlgo = checked;
}

void CEngineerMode::on_ckbSaveSimulateImage_clicked(bool checked)
{
    printf("on_ckbSaveConstantTemperatureData_clicked: %d\n", checked);

    Util::setSaveSimulateImg(checked, IMG_WIDTH, IMG_HEIGHT);

    if (ui->ckbSaveSimulateImage->isChecked() != Util::getSaveSimulateImg()) {
        ui->ckbSaveSimulateImage->setChecked(Util::getSaveSimulateImg());
    }
    if (ui->ckbUseSimulateImage->isChecked() != Util::getUseSimulateImg()) {
        ui->ckbUseSimulateImage->setChecked(Util::getUseSimulateImg());
    }
}

void CEngineerMode::on_ckbUseSimulateImage_clicked(bool checked)
{
    printf("on_ckbConstantTemperature_clicked: %d\n", checked);

    Util::setUseSimulateImg(checked, IMG_WIDTH, IMG_HEIGHT);

    if (ui->ckbSaveSimulateImage->isChecked() != Util::getSaveSimulateImg()) {
        ui->ckbSaveSimulateImage->setChecked(Util::getSaveSimulateImg());
    }
    if (ui->ckbUseSimulateImage->isChecked() != Util::getUseSimulateImg()) {
        ui->ckbUseSimulateImage->setChecked(Util::getUseSimulateImg());
    }
}

void CEngineerMode::on_btnGetContrast_clicked()
{
    int contrast = -1;
    bool succ = g_CameraIntf->getContrast(&contrast);
    if (succ)
        QMessageBox::information(this, "contrast", QString::number(contrast));
    else
        QMessageBox::critical(this, "error", "Failed!");
}

void CEngineerMode::on_btnResetCameraParm_clicked()
{
    bool succ = (cameraStat_Succ == g_CameraIntf->resetParams().cameraStat);   // 参数恢复默认值后，进测量界面，为什么会花屏（迈德威视相机）？
    if (!succ)
        QMessageBox::critical(this, "error", "Failed!");
}

// 灯珠校准
void CEngineerMode::on_btnLampCalibrate_clicked()
{
    getWinManage()->showWindowByType(WIN_LAMP_CALI);
}

// 相机反初始化
void CEngineerMode::on_btnCameraUninit_clicked()
{
    ui->btnCameraUninit->setEnabled(false);

    g_CameraIntf->uninitCamera();
    getWinManage()->showSuspensionPrompt("camera uninit done", 1000);

    ui->btnCameraUninit->setEnabled(true);
}

// 相机断电
void CEngineerMode::on_btnCameraPowerOff_clicked()
{
    ui->btnCameraPowerOff->setEnabled(false);

    CHardware::cameraPowerOff();
    getWinManage()->showSuspensionPrompt("camera power off done", 1000);

    ui->btnCameraPowerOff->setEnabled(true);
}

// 相机上电
void CEngineerMode::on_btnCameraPowerOn_clicked()
{
    ui->btnCameraPowerOn->setEnabled(false);

    CHardware::cameraPowerOn();
    getWinManage()->showSuspensionPrompt("camera power on done", 1000);

    ui->btnCameraPowerOn->setEnabled(true);
}

// 相机初始化
void CEngineerMode::on_btnCameraInit_clicked()
{
    ui->btnCameraInit->setEnabled(false);

    g_CameraIntf->initCamera();
    getWinManage()->showSuspensionPrompt("camera init done", 1000);

    ui->btnCameraInit->setEnabled(true);
}

// 相机重上电
void CEngineerMode::on_btnCameraRePowerOn_clicked()
{
    ui->btnCameraRePowerOn->setEnabled(false);

    CameraInitThread::cameraRePowerOn();
    getWinManage()->showSuspensionPrompt("camera re-power-on done", 1000);

    ui->btnCameraRePowerOn->setEnabled(true);
}

void CEngineerMode::on_btnGotoFocus_clicked()
{
    isNeedCloseIR = false;

    g_WinMeasure->setIsFocusMode(true);
    getWinManage()->showWindowByType(WIN_MEASURE);
}

void CEngineerMode::on_cbbLedLevelMiddle_activated(int index)
{
    Q_UNUSED(index)
    bleLevelEdited = true;
}

void CEngineerMode::on_cbbLedLevelEccentric_activated(int index)
{
    Q_UNUSED(index)
    bleLevelEdited = true;
}

void CEngineerMode::on_btnSyncTrialStat_clicked()
{
    // 应用界面修改
    // TODO:

    //
    syncTrialStat();
}

void CEngineerMode::on_btnDevActivate_clicked()
{
    //
    bool activation_stat_new = (!CGlobal::isDevActivated);      // 新的设备激活状态

    //
    if (activation_stat_new) {
        static constexpr char DEFAULT_PASSWORD[] = "161616";

        getWinManage()->showMsgWin("本功能需要验证密码。", true, "OK", -1, true);

        myEditLine edt;
        getWinManage()->showKeyboard(&edt, nullptr);
        const QString password = edt.text();
        if (password != DEFAULT_PASSWORD) {
            logDebug(QString("%1: the password user input not equal to \"%2\"").arg(__PRETTY_FUNCTION__).arg(DEFAULT_PASSWORD));
            getWinManage()->showMsgWin("密码错误！");
            return;
        }
    }

    //
    CGlobal::isDevActivated = activation_stat_new;

    getWinManage()->showMsgWin(activation_stat_new ? "设备已激活" : "设备已反激活");

    //
    ui->btnDevActivate->setText(CGlobal::isDevActivated ? UI_STR_DEV_DEACTIVATE : UI_STR_DEV_ACTIVATE);
}

void CEngineerMode::on_ckbIsTurnLampTestMode_clicked(bool _checked)
{
    g_isTurnLampTestMode = _checked;
}
