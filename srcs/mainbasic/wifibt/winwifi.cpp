#include "winwifi.h"
#include "ui_winwifi.h"

#include <iostream>
#include <unistd.h>

#include <QIcon>
#include <QHeaderView>
#include <QDesktopWidget>
#include <QRect>
#include <QDebug>
#include <QRegExp>
#include <QDir>
#include <QtAlgorithms>
#include <QHostInfo>
#include <QHostAddress>
#include <QList>
#include <QStringList>
//#include <QNetworkConfigurationManager>
#include <QScroller>
#include <QApplication>

#include "windowsmanager.h"
#include "statusbarform.h"
#include "appsetting.h"
#include "global.h"

//
int WinWifi::WifiSignalItem = 0;
bool WinWifi::isConnect = false;

bool Get_Network_Time_flag = false;
QString WinWifi::currentIP = "";
int powerOnconnect=0;

//
QList<stWifiHistPwd> g_wifiHistPwds;        // WiFi 历史密码
bool g_wifiHistPwdsLoaded = false;          // WiFi 历史密码是否已载入

//
static const int ROWS_PER_PAGE      = 7;    // 每页行数
static const int ROW_HEIGHT         = 38;   // 行高

bool WinWifi::isWifiOpenedCfg = false;

// wifi模块
const char * const WinWifi::S_CLASS_NAME = WinWifi::staticMetaObject.className();

WinWifi::WinWifi(QWidget *parent, CWifiIntf *_wifi_intf) :
    CBaseWidget(parent),
    wifiIntf(_wifi_intf),
    ui(new Ui::WinWifi)
{
    ui->setupUi(this);

    this->setAutoFillBackground(true);

    //
    connect(wifiIntf, &CWifiIntf::sigRecvStatus, this, &WinWifi::slot_wifiIntf_RecvStatus, Qt::QueuedConnection);
    connect(wifiIntf, &CWifiIntf::sigWifiListChanged, this, &WinWifi::slot_wifiIntf_WifiListChanged, Qt::QueuedConnection);
    connect(wifiIntf, &CWifiIntf::sigNotice, this, &WinWifi::slot_wifiIntf_Notice, Qt::QueuedConnection);
    connect(wifiIntf, &CWifiIntf::sigMessage, this, &WinWifi::slot_wifiIntf_Message, Qt::QueuedConnection);

    isShowStatusBar = true;

    //
    QFont font;
    font.setPixelSize(22);

    ui->pushButton_Back->setFocusPolicy(Qt::NoFocus);   //影藏焦点框

    ui->tblWifiList->horizontalHeader()->setDefaultSectionSize(40);
    ui->tblWifiList->setColumnWidth(0, 500);     //第1列宽度
    ui->tblWifiList->setColumnWidth(1, 86);      //第2列宽度
    ui->tblWifiList->setColumnWidth(2, 86);      //第3列宽度
    ui->tblWifiList->setColumnWidth(3, 86);      //第4列宽度
    ui->tblWifiList->setCurrentCell(0, 0);
    ui->tblWifiList->horizontalHeader()->setSectionsClickable(false);       //水平方向的头不可点击
    ui->tblWifiList->setEditTriggers(QAbstractItemView::NoEditTriggers);    //设置每行内容不可更改
    ui->tblWifiList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  //影藏水平滚动条
    ui->tblWifiList->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblWifiList->setSelectionMode(QAbstractItemView::SingleSelection);

    //m_ifn = new QInputFileName();
    //connect(m_ifn, SIGNAL(sendWifiText(QString)), this, SLOT(slotAfterInputPassword(QString)));

    QHeaderView *hv = ui->tblWifiList->verticalHeader();
    hv->hide();
    //获取光标所在行列指向的内容
    connect(ui->tblWifiList, &QTableWidget::currentItemChanged, this, &WinWifi::slot_tblWifiList_currentItemChanged);

    WifiUpdateCount = 0;
//    wifiStatus = new QTimer(this);
//    //connect(wifiStatus, SIGNAL(timeout()), this, SLOT(slot_wifiStatus()));      //查看wifi当前状态及重连
//    //wifiStatus->start(1000);
//    wifiListName = new QTimer(this);
//    //connect(wifiListName, SIGNAL(timeout()), this, SLOT(UpdateWifiListName())); //显示wifi界面后定时刷新wifi列表
//    wifiListName->stop();

    winPwdInput = new inputWiFipwd(this);
    connect(winPwdInput, &inputWiFipwd::sigGotPwd, this, &WinWifi::slot_winPwdInput_GotPwd);  //密码确认信号
    //connect(winPwdInput, &inputWiFipwd::sendCancelFlag, this, &WinWifi::CancelEnterPwd);  //密码取消信号
    winPwdInput->hide();

    winWifiInfo = new wifidetails(this);
    connect(this, &WinWifi::sigShowWifiInfo, winWifiInfo, &wifidetails::slotShowWifiInfo);  //获取wifi详细信息
    connect(winWifiInfo, &wifidetails::sigForgetWifi, this, &WinWifi::slot_winWifiInfo_ForgetWifi);
    winWifiInfo->hide();

    m_posPage = 0;
    m_allPage = 0;
    updatePageWidgets();
    isScanning = false;     //搜索wifi标志
    isConnect = false;      //连接状态标志

    ui->btnIsWifiOpened->setEnabled(true);      // TODO: 建立状态模式，统一以状态模式改变这些控件的属性？
    ui->btnConnect->setEnabled(false);
    ui->btnSearch->setEnabled(false);

    // 支持触摸滚动
    //QScroller::grabGesture(ui->tblWifiList->viewport(), QScroller::TouchGesture);

    //
    QObject::connect(this, &WinWifi::sigScanWifi, this, &WinWifi::slot_ScanWifi, Qt::QueuedConnection);

    // 载入 WiFi 历史密码
    WinWifi::loadWifiRecords();

    // 如果关机前 wifi 是打开状态                     // TODO: 设备控制应放到外部，不应放到本构造函数里，本模块仅作为视图
    bool is_open = getCfg_isWifiOpened(true);
    QTimer::singleShot(500, this, [is_open]() {
        //this->setIsOpened(is_open);       // NOTE: 若初始化时用这函数，那么崩溃重启时，不会有状态返回，m_isOpening 或 m_isClosing 不会被重置，导致无法继续操作
        g_WifiIntf->setIsOpened(is_open);
    });

    //
    qDebug() << "WinWifi() ended";
}

WinWifi::~WinWifi()
{
    delete ui;
}

void WinWifi::showEvent(QShowEvent *)
{
    // 样式刷新
    //QPixmap pixmap;
    //QPalette palette;
    if (themeType_Black == getSysThemeType()) {
        //pixmap.load(":/resource/black_theme/blackground_color_b.png");

        ui->tblWifiList->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(50,50,50); color:rgb(204,204,204);}");
        ui->tblWifiList->setStyleSheet("QTableWidget { background-color: transparent; color:rgb(204,204,204); border: 1px solid black; gridline-color: rgb(40,42,48); }");
        ui->btnConnect->setStyleSheet("background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px; padding:2px 4px;");
        ui->btnHiddenSsid->setStyleSheet("background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px; padding:2px 4px;");

        ui->btnPrevPage->setStyleSheet("background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px; padding:2px 4px;");
        ui->btnNextPage->setStyleSheet("background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px; padding:2px 4px;");
        ui->lblStateDesc->setStyleSheet("color:rgb(204,204,204);");
        ui->lblPageNum->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Home->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Search->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");

        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->btnSearch->setIcon(QIcon(":/resource/black_theme/find_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png")) ;
    } else {
        //pixmap.load(":/resource/white_theme/whiteground_color_w.png");

        ui->tblWifiList->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
        ui->tblWifiList->setStyleSheet("QTableWidget{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
        ui->btnConnect->setStyleSheet("background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");
        ui->btnHiddenSsid->setStyleSheet("background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");

        ui->btnPrevPage->setStyleSheet("background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");
        ui->btnNextPage->setStyleSheet("background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");
        ui->lblStateDesc->setStyleSheet("color:rgb(1,1,1);");
        ui->lblPageNum->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Search->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");

        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->btnSearch->setIcon(QIcon(":/resource/white_theme/find_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
    }
    //palette.setBrush(backgroundRole(), QBrush(pixmap));
    //setPalette(palette);

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // UI控件的值刷新
    updateView_btnIsWifiOpened(getCfg_isWifiOpened());

    // 语言刷新
    stWifiInfo connected_info;
    wifiIntf->getConnInfo(connected_info);
    if (connected_info.ssid.length() > 0) {
        ui->lblStateDesc->setText(tr("已连接: %1").arg(connected_info.ssid)); // "connected: %1"
    } else {
        ui->lblStateDesc->setText("");
    }

    // 刷新标题
    getWinManage()->updateWindowTitle(this, tr("网络配置"));    // "NetworkConfig"

    //
    if(wifiIntf->getIsConnected())
        ui->btnConnect->setText(tr("断开"));  // "Disconnect"
    else
        ui->btnConnect->setText(tr("连接"));  // "Connect"

    //
    //if (language) {
    //    ui->btnPrevPage->setText("上一页");
    //    ui->btnNextPage->setText("下一页");
    //    ui->label_Home->setText("主页");
    //    ui->label_Search->setText("搜索");
    //    ui->label_Back->setText("返回");
    //} else {
    //    ui->btnPrevPage->setText("Prev");
    //    ui->btnNextPage->setText("Next");
    //    ui->label_Home->setText("Home");
    //    ui->label_Search->setText("Search");
    //    ui->label_Back->setText("Back");
    //}

    QStringList header;
    header << tr("网络名称(SSID)") << tr("数据加密") << tr("信号强度") << tr("详细信息");   // << "SSID" << "Encryption" << "Strength" << "Details"
    ui->tblWifiList->setHorizontalHeaderLabels(header);

    ui->tblWifiList->setSelectionMode(QAbstractItemView::SingleSelection);

    //
    bool is_chinese = (G_LANGUAGE_CHINESE == CGlobal::language);

    ui->btnConnect->resize(     (is_chinese ? 121 : 141), ui->btnConnect->height());
    ui->btnHiddenSsid->resize(  (is_chinese ? 121 : 141), ui->btnHiddenSsid->height());

    //
    QWidget::update();

    // 如果上次扫描的时间间隔太久，重新扫描
    //if (true) {     // TODO: 上次扫描时间？
    //    emit sigScanWifi();
    //}

}

void WinWifi::hideEvent(QHideEvent *)
{

}

bool WinWifi::getCfg_isWifiOpened(bool _is_reload)
{
    if (_is_reload) {
        isWifiOpenedCfg = appSetting::value("/wifi/isWifiOpened").toBool();
    }
    return isWifiOpenedCfg;
}

void WinWifi::setCfg_isWifiOpened(bool _is_open)
{
    if (isWifiOpenedCfg != _is_open) {
        isWifiOpenedCfg = _is_open;
        appSetting::setValue("/wifi/isWifiOpened", isWifiOpenedCfg);
        appSetting::sync();
    } else {
        logWarning(QString::asprintf("%s(%s): no change", __FUNCTION__, Util::bool2str(_is_open)));
    }
}

QString WinWifi::getCfg_lastWifiSsid()
{
    QString ssid = appSetting::value("/wifi/lastWifiSsid").toString();
    return ssid;
}

void WinWifi::setCfg_lastWifiSsid(QString _ssid)
{
    appSetting::setValue("/wifi/lastWifiSsid", _ssid);
    appSetting::sync();
}

const QList<stWifiHistPwd> &WinWifi::getWifiHistPwds()
{
    if (!g_wifiHistPwdsLoaded) {
        WinWifi::loadWifiRecords();
    }

    //
    return g_wifiHistPwds;
}

int WinWifi::getWifiHistPwdIndex(QString _ssid)
{
    if (!g_wifiHistPwdsLoaded) {
        WinWifi::loadWifiRecords();
    }

    //
    int idx = -1;
    for (int i = g_wifiHistPwds.count() - 1; i >= 0; i--) {
        if (g_wifiHistPwds.at(i).ssid == _ssid) {
            idx = i;
            break;
        }
    }
    return idx;
}

void WinWifi::addWifiHistPwd(QString _ssid, QString _pwd)
{
    logDebug((QString(__PRETTY_FUNCTION__) + ": enter..., _ssid = %1, _pwd = %2").arg(_ssid).arg(_pwd), CGlobal::LOG_WIFI);

    if (!g_wifiHistPwdsLoaded) {
        WinWifi::loadWifiRecords();
    }

    if (_ssid.isEmpty()) {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): ParamError: _ssid is empty!";
        return;
    }

    //
    int idx = WinWifi::getWifiHistPwdIndex(_ssid);
    if (idx >= 0) {
        stWifiHistPwd &wifi_config = g_wifiHistPwds[idx];
        wifi_config.pwd = _pwd;
    } else {
        stWifiHistPwd wifi_config = {_ssid, _pwd};
        g_wifiHistPwds.prepend(wifi_config);
    }

    WinWifi::saveWifiRecords();
}

void WinWifi::removeWifiHistPwd(QString _ssid)
{
    logDebug((QString(__PRETTY_FUNCTION__) + ": enter..., _ssid = %1").arg(_ssid), CGlobal::LOG_WIFI);

    //
    if (!g_wifiHistPwdsLoaded) {
        WinWifi::loadWifiRecords();
    }

    //
    int idx = WinWifi::getWifiHistPwdIndex(_ssid);
    if (idx >= 0) {
        g_wifiHistPwds.removeAt(idx);
        WinWifi::saveWifiRecords();
    }
}

// 保存成功连接过的 ssid 和 pwd
void WinWifi::saveWifiRecords()
{
    qDebug() << __PRETTY_FUNCTION__ << ": begin ...";

    /* 文件数据结构：
     * 1、因为防止特殊字符造成的数据解析错误，ssid 和 pwd 都先转为十六进制字符串再保存。
     * 2、最近的密码放在列表的头部。
     */

    //
    static constexpr int MAX_SIZE = 30;     // 最大保留个数（历史密码个数超出这个数量的，清掉）

    //
    if (g_wifiHistPwds.size() > MAX_SIZE) {
        for (int i = g_wifiHistPwds.size() - 1; i >= MAX_SIZE; i--) {               // 优先删掉较早的密码
            g_wifiHistPwds.removeAt(i);
        }
    }

    //
    static const QString FILE_PATH = QString("%1/wifi-history").arg(qApp->applicationDirPath());

    //
    QFile file(FILE_PATH);
    bool is_open_succ = file.open(QFile::WriteOnly | QFile::Truncate);
    if (is_open_succ) {
        QTextStream stream(&file);

        stWifiHistPwd hist_pwd;
        QString ssid_hex, pwd_hex;
        QString str_line;
        for (int i = 0; i < g_wifiHistPwds.size(); i++) {
            hist_pwd = g_wifiHistPwds.at(i);
            ssid_hex = hist_pwd.ssid.toUtf8().toHex();
            pwd_hex  = hist_pwd.pwd.toUtf8().toHex();

            str_line = QString("%1 %2\n").arg(ssid_hex).arg(pwd_hex);
            stream << str_line;
            logDebug(QString("save history password: ") + str_line, CGlobal::LOG_WIFI);
        }

        stream.flush();
        file.flush();
        file.close();
    } else {
        qDebug() << __PRETTY_FUNCTION__ << ": open file '" << FILE_PATH << "' failed!";
    }

    //
    qDebug() << __PRETTY_FUNCTION__ << ": end";
}

// 载入成功连接过的 ssid 和 pwd
void WinWifi::loadWifiRecords()
{
    qDebug() << __PRETTY_FUNCTION__ << ": begin ...";

    //
    g_wifiHistPwds.clear();

    //
    static const QString FILE_PATH = QString("%1/wifi-history").arg(qApp->applicationDirPath());

    //
    if (QFile::exists(FILE_PATH)) {
        QFile file(FILE_PATH);
        bool is_open_succ = file.open(QFile::ReadOnly);
        if (is_open_succ) {
            QString str_line;
            QString ssid_hex, pwd_hex;
            QString ssid, pwd;
            while (!file.atEnd()) {
                str_line = file.readLine().replace("\n", "");

                int idx = str_line.indexOf(" ");
                if (idx >= 0) {
                    ssid_hex = str_line.left(idx);
                    pwd_hex = str_line.mid(idx + 1);

                    ssid   = QString::fromUtf8(QByteArray::fromHex(ssid_hex.toLatin1()));
                    pwd    = QString::fromUtf8(QByteArray::fromHex(pwd_hex.toLatin1()));

                    qDebug() << QString("loaded wifi history password: ssid = %1, pwd = %2").arg(ssid).arg(pwd);

                    if (!ssid.isEmpty()) {
                        stWifiHistPwd hist_pwd;
                        hist_pwd.ssid   = ssid;
                        hist_pwd.pwd    = pwd;
                        g_wifiHistPwds.append(hist_pwd);
                    } else {
                        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): DataError: ssid is empty!";
                    }
                } else {
                    qDebug() << "separator of ssid & pwd not found!";
                    break;
                }

                //
                if (!file.canReadLine()) {
                    break;
                }
            }

        } else {
            qDebug() << __PRETTY_FUNCTION__ << ": open file '" << FILE_PATH << "' failed!";
        }
    } else {
        qDebug() << __PRETTY_FUNCTION__ << ": file '" << FILE_PATH << "' not found!";
    }

    //
    g_wifiHistPwdsLoaded = true;

    //
    qDebug() << __PRETTY_FUNCTION__ << ": end";
}

void WinWifi::slot_tblWifiList_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *)
{
    qDebug() << "WinWifi::slot_tblWifiList_currentItemChanged() into ...";

    //
    if (ui->tblWifiList->currentItem() == NULL) {
        return;
    }

    //
    QTableWidgetItem *item = ui->tblWifiList->item(current->row(), 0);   // 提取光标所在行,列不要,默认第0列
    const QString ssid_selected = item->text();
    if (current->column() == 3) {
        for (int i = 0; i  < m_wifiList.size(); i++) {    // 查找wifi名
            if (ssid_selected == m_wifiList[i].ssid) {
                stWifiInfo connected_info;
                wifiIntf->getConnInfo(connected_info);

                if (ssid_selected == connected_info.ssid) {
                    //wifiList[i].mac = mac_connected;
                    qDebug() << "mac = " << connected_info.localMac << ", ip = " << connected_info.localIp;
                }

                emit sigShowWifiInfo(m_wifiList[i], connected_info.ssid, connected_info.localMac, connected_info.localIp);
                break;
            }
        }
    }

    // 设置光标在当前选中行的第一格
    ui->tblWifiList->setCurrentItem(item);
}

//输入密码后就进入到这一步
void WinWifi::slot_winPwdInput_GotPwd(QString _ssid, QString _password)
{
    qDebug() << "WinWifi::slot_winPwdInput_GotPwd() into ...";

    //
    connectingSsid = _ssid;
    lastPassword = _password;
    qDebug() << "WinWifi::slot_winPwdInput_GotPwd()" << " lastPassword = " << lastPassword;

//    if(WpaCommit::currentWIFI.ssid.isEmpty())
//    {
//        qDebug()<<"WpaCommit::currentWIFI.ssid==NULL";
//        //m_ifn->close();
//        return;
//    }

    //if (false)
    //{
    //    setIsOpened(false);
    //    setIsOpened(true);
    //    qDebug()<<"--re open";
    //}
    //ui->btnConnect->setEnabled(false);
    //ui->btnSearch->setEnabled(false);

//    WpaCommit::currentWIFI = m_wifiList.at(ui->tblWifiList->currentItem()->row() + (m_posPage - 1)*ROWS_PER_PAGE);
//    WpaCommit::currentWIFI.pwd = lastPassword;

//    connectingSsid = WpaCommit::currentWIFI.ssid;
//    lastPassword = WpaCommit::currentWIFI.pwd;
//    qDebug()<<"--connectingSsid:"<<connectingSsid<<",lastPassword:"<<lastPassword<<"--";

    if (wifiIntf->getIsConnected()) {
        wifiIntf->discWifi();
    }
    //connectingSsid = m_wifiList.at(ui->tblWifiList->currentItem()->row() + (m_posPage - 1)*ROWS_PER_PAGE).ssid;

    qDebug() << "WinWifi::slot_winPwdInput_GotPwd() : connecting to " << connectingSsid << ", " << lastPassword;
    wifiIntf->connectTo(connectingSsid, lastPassword);

    //m_ifn->close();
    ui->lblStateDesc->setText(tr("正在进行身份验证..."));   // "Identity verification in progress..."

    //wpa->wpa_connect_wifi(wifi);
    conCnt = 0;
    conTime = 0;
//    int cmd = WPA_CONNECT;
//    emit wpaSig(cmd);   //请求连接
//    qt->start(500);     //定时器查询连接状态
}

void WinWifi::slotDisConnectWifi()
{
    disconnectWifi();
}

void WinWifi::slot_winWifiInfo_ForgetWifi(QString _ssid)
{
    wifiIntf->forgetWifi(_ssid);

    WinWifi::removeWifiHistPwd(_ssid);

    ui->btnConnect->setText(tr("连接"));  // "Connect"

}

// 连接 WiFi
void WinWifi::connectWifi(const QString &_ssid, const bool _is_encry)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): connecting ...";

    //
    if (_is_encry) {
        //
        QString pwd_hist = "";      // 该 ssid 的历史密码

        //
        if (!_ssid.isEmpty()) {
            int idx = WinWifi::getWifiHistPwdIndex(_ssid);
            if (idx >= 0) {
                pwd_hist = WinWifi::getWifiHistPwds()[idx].pwd;
                qDebug() << "get history password of " << _ssid << ": " << pwd_hist;
            } else {
                qDebug() << "get history password of " << _ssid << " failed";
            }
        }

        //
        winPwdInput->EnterPwdText(_ssid, pwd_hist);
    } else {
        slot_winPwdInput_GotPwd(_ssid, "");
    }
}

// 断开 WiFi
void WinWifi::disconnectWifi()
{
    qDebug() << __PRETTY_FUNCTION__ << ": into ...";

    // 断开wifi
    wifiIntf->discWifi();

    //int cmd = CTRL_CMD_DISCON;  //断开连接
    //emit wpaSig(cmd);
    //cmd = CTRL_CMD_SCAN;    //搜索wifi
    //emit wpaSig(cmd);

    //
    ui->lblStateDesc->setText(tr("wifi未连接"));   // "wifi not connected"

    currentIP = "";
    connectingSsid = "";

    //wifiStatus->stop();
}

//
void WinWifi::updatePageWidgets()
{
    qDebug() << "WinWifi::updatePageWidgets() into ...";

    ui->lblPageNum->setText(tr("%1/%2页").arg(m_posPage).arg(m_allPage));   // "%1/%2Page"
    if(m_posPage <= 1)
        ui->btnPrevPage->setEnabled(false);
    else
        ui->btnPrevPage->setEnabled(true);
    if(m_posPage >= m_allPage)
        ui->btnNextPage->setEnabled(false);
    else
        ui->btnNextPage->setEnabled(true);
}

//向tblWifiList中添加内容
void WinWifi::fillWifiListToTable()
{
    qDebug() << "WinWifi::fillWifiListToTable() into ...";

    int pos = 0;
    ui->tblWifiList->clear();
    QStringList header;
    //QString signalconnect = language ? "连接状态" : "Connect";
    QString ssidText = tr("网络名称(SSID)");    // "SSID"
    QString dataText = tr("数据加密");  // "Encrypt"
    QString signalText = tr("信号强度");    // "Signal"
    QString signalDetails = tr("详细信息"); // "Details"
    header.append(ssidText);
    header.append(dataText);
    header.append(signalText);
    header.append(signalDetails);
    ui->tblWifiList->setHorizontalHeaderLabels(header);
    ui->tblWifiList->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblWifiList->setRowCount(ROWS_PER_PAGE);
    for(pos = 0; pos < (m_wifiList.length() - (m_posPage - 1) * ROWS_PER_PAGE) && pos < ROWS_PER_PAGE; pos++){
        int i = pos + (m_posPage - 1) * ROWS_PER_PAGE;
        stWifiInfo wifi = m_wifiList.at(i);
        enWiFiStrengthLevel strength_level = CWifiIntf::rssiToStrengthLevel(wifi.getRssi());

        //网络名称
        ui->tblWifiList->setItem(pos, 0, new QTableWidgetItem(wifi.ssid.toStdString().c_str()));
        if(themeType_Black == getSysThemeType()){
            //数据加密
            //ui->tblWifiList->setItem(pos, 1, new QTableWidgetItem(wifi.getEncry().toStdString().c_str()));
            if(wifi.encry)
                ui->tblWifiList->setItem(pos, 1, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_lock_b.png")),""));
            else
                ui->tblWifiList->setItem(pos, 1, new QTableWidgetItem(""));
            //信号强度
            if (wiFiStrengthLevel_4 == strength_level)
                ui->tblWifiList->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_signal4_b.png")),""));
            else if (wiFiStrengthLevel_3 == strength_level)
                ui->tblWifiList->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_signal3_b.png")),""));
            else if (wiFiStrengthLevel_2 == strength_level)
                ui->tblWifiList->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_signal2_b.png")),""));
            else if (wiFiStrengthLevel_1 == strength_level)
                ui->tblWifiList->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_signal1_b.png")),""));
            //qDebug()<<"-----strength:"<<strength;
            //wifi详细信息
            ui->tblWifiList->setItem(pos, 3, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_message_b.png")),""));
        }
        else{
            if(wifi.encry)
                ui->tblWifiList->setItem(pos, 1, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_lock_w.png")),""));
            else
                ui->tblWifiList->setItem(pos, 1, new QTableWidgetItem(""));

            if (wiFiStrengthLevel_4 == strength_level)
                ui->tblWifiList->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_signal4_w.png")),""));
            else if (wiFiStrengthLevel_3 == strength_level)
                ui->tblWifiList->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_signal3_w.png")),""));
            else if (wiFiStrengthLevel_2 == strength_level)
                ui->tblWifiList->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_signal2_w.png")),""));
            else if (wiFiStrengthLevel_1 == strength_level)
                ui->tblWifiList->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_signal1_w.png")),""));
            ui->tblWifiList->setItem(pos, 3, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_message_w.png")),""));
        }
        ui->tblWifiList->setRowHeight(pos, ROW_HEIGHT);       //设置行高度
    }
}

//下一页
void WinWifi::nextPage()
{
    qDebug() << "WinWifi::nextPage() into ...";

    m_posPage++;
    updatePageWidgets();
    fillWifiListToTable();
}

//上一页
void WinWifi::prevPage()
{
    qDebug() << "WinWifi::prevPage() into ...";

    m_posPage--;
    updatePageWidgets();
    fillWifiListToTable();
}

//搜索网络
void WinWifi::scanWifi()
{
    qDebug() << "WinWifi::scanWifi() into ...";

    //
    ui->btnSearch->setEnabled(false);

    //
    ui->lblStateDesc->setText(tr("正在扫描附近wifi...")); // "Scanning nearby WiFi..."

    //
    isScanning = true;

    //
    wifiIntf->scan();

    //
    ui->btnSearch->setEnabled(true);        // TODO: 正常来说这个应该放到搜索完成状态事件之后，但是需要确保状态事件不会不产生，否则无法恢复按钮？

}

// 切换 wifi 开关的状态
void WinWifi::updateView_btnIsWifiOpened(bool _is_on)
{
    if (_is_on) {
        if(themeType_Black == getSysThemeType())
            ui->btnIsWifiOpened->setIcon(QIcon(":/resource/black_theme/switch-on_b.png"));
        else
            ui->btnIsWifiOpened->setIcon(QIcon(":/resource/white_theme/switch-on_w.png"));
    } else {
        if(themeType_Black == getSysThemeType())
            ui->btnIsWifiOpened->setIcon(QIcon(":/resource/black_theme/switch-off_b.png"));
        else
            ui->btnIsWifiOpened->setIcon(QIcon(":/resource/white_theme/switch-off_w.png"));
    }
    ui->btnIsWifiOpened->update();
    qApp->processEvents(QEventLoop::AllEvents);
}

//打开wifi
void WinWifi::openWifi()        // TODO: 和 closeWiFi() 合并？
{
    qDebug() << "WinWifi::openWifi() into ...";

    ui->lblStateDesc->setText(tr("WiFi开启中..."));    // "WiFi Opening..."
    qDebug()<<"wifi is opening ...";

    ui->lblStateDesc->update();
    qApp->processEvents(QEventLoop::AllEvents);

    ui->btnConnect->setText(tr("连接"));  // "Connect"

    conCnt = 0;
    conTime = 0;

    //
    if (!getCfg_isWifiOpened()) {
        setCfg_isWifiOpened(true);
    }

    //
    wifiIntf->setIsOpened(true);

    ui->btnSearch->setEnabled(true);

}

//关闭wifi
void WinWifi::closeWifi()
{
    qDebug() << "WinWifi::closeWifi() into ...";

//    qt->stop();
//    wifiStatus->stop();
    //wpa->wpa_ctrl_cmd("REMOVE_NETWORK 0");
    //wpa->wpa_ctrl_cmd("TERMINATE");
    //system("ifconfig wlan0 down");
    //system("dhclient -r wlan0");
//    int cmd = CTRL_CMD_CLOSE;
//    emit wpaSig(cmd);

    //
    if (getCfg_isWifiOpened()) {
        setCfg_isWifiOpened(false);
    }

    //
    wifiIntf->setIsOpened(false);

    //
    QString c_str = tr("wifi未连接");  // "wifi disconnect"
    ui->lblStateDesc->setText(c_str);

    ui->btnConnect->setEnabled(false);
    ui->btnSearch->setEnabled(false);

    // 清除wifi列表
    ui->tblWifiList->clearContents();

    m_posPage = 0;
    m_allPage = 0;
    updatePageWidgets();

    qDebug() << "WinWifi::closeWifi() ended ...";
}

void WinWifi::slot_RecvSysSignal(enSysSignal _sys_signal)
{
    if (!(sysSignal_WifiScanOff == _sys_signal || sysSignal_WifiScanOn == _sys_signal)) {
        return;
    }

    qDebug() << __PRETTY_FUNCTION__ << ": into ..., signal = " << _sys_signal;

    //
    if (_sys_signal == sysSignal_WifiScanOff){     // wifi停止刷新
        //wifiStatus->stop();
    }
    if (_sys_signal == sysSignal_WifiScanOn) {
        //if (isWifiOpenedCfg && wifiIntf->getIsConnected()) {      //wifi启动刷新
        //    wifiStatus->start(1000);
        //}
    }
}

void WinWifi::slot_ScanWifi()
{
    scanWifi();
}

//读取wifi列表
void WinWifi::updateWifiList()
{
    qDebug() << "WinWifi::updateWifiList() into, threadid = " << QThread::currentThreadId();

    if(WinMeasure::isOpened()){
        qDebug()<<"Camera is running,return;";
        return;
    }

    qDebug() << "WinWifi::updateWifiList(): size of m_wifiList = " << m_wifiList.size();

//    stWifiInfo wifi_info;
//    for (int i = 0; i < wifiList.size(); i++) {
//        wifi_info = wifiList.at(i);
//        qDebug() << "wifi_info of wifiList: " << i << ", " << wifi_info.ssid << ", " << wifi_info.strength;
//    }

//    wifiList.clear();
//    wifiList.append(wifiList);
    m_posPage = 1;
    if (m_wifiList.length() % ROWS_PER_PAGE > 0)
        m_allPage = m_wifiList.length() / ROWS_PER_PAGE + 1;
    else m_allPage = m_wifiList.length() / ROWS_PER_PAGE;
    updatePageWidgets();    //页数显示
    fillWifiListToTable(); //wifi列表添加到TabWidget中

}

void WinWifi::on_pushButton_Home_clicked()
{
    qDebug() << "WinWifi::on_pushButton_Home_clicked() into ...";

    getWinManage()->showWindowByType(WIN_HOME);
}

void WinWifi::on_pushButton_Back_clicked()
{
    qDebug() << "WinWifi::on_pushButton_Back_clicked() into ...";

    // 返回工具窗口
    getWinManage()->showWindowByType(WIN_TOOL);
}

// 槽函数：“开关”按钮
void WinWifi::on_btnIsWifiOpened_clicked()
{
    qDebug() << "WinWifi::on_btnIsWifiOpened_clicked() into ...";

    //
    bool is_open = !getCfg_isWifiOpened();

    // 状态检查：若是正在打开或关闭，则暂时拒绝操作（避免快速连续执行相同操作）
    if (/*is_to_open &&*/ m_isOpening) {
        logWarning(QString("%1::%2(): is opening, operation skipped!").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_WIFI);
        getWinManage()->showSuspensionPrompt(tr("Opening, please wait!"));    // "正在打开，请稍候！"
        return;
    } else if (/*!is_to_open &&*/ m_isClosing) {
        logWarning(QString("%1::%2(): is closing, operation skipped!").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_WIFI);
        getWinManage()->showSuspensionPrompt(tr("Closing, please wait!"));    // "正在关闭，请稍候！"
        return;
    }

    //
    setIsOpened(is_open);

}

// 槽函数：“连接”
void WinWifi::on_btnConnect_clicked()
{
    qDebug() << "WinWifi::" << __FUNCTION__ << "(): into ...";

    bool is_to_connect = !wifiIntf->getIsConnected();

    //
    ui->btnConnect->setEnabled(false);

    //
    if (is_to_connect) {
        // 连接wifi
        if (ui->tblWifiList->currentItem() == NULL) {
            getWinManage()->showSuspensionPrompt(tr("请选择想要连接的 WiFi"));  // "Please select the WiFi you want to connect to"
            return;
        }

        //
        ui->lblStateDesc->setText(tr("连接中..."));    // "Connecting..."

        //
        //WpaCommit::currentWIFI = wifiList.at(ui->tblWifiList->currentItem()->row() + (m_posPage - 1)*ROWS_PER_PAGE);
        stWifiInfo wifi_info = m_wifiList.at(ui->tblWifiList->currentItem()->row() + (m_posPage - 1) * ROWS_PER_PAGE);
        QString ssid = wifi_info.ssid;
        bool is_encry = wifi_info.encry;

        connectWifi(ssid, is_encry);
    } else {
        disconnectWifi();
    }

    //
    ui->btnConnect->setEnabled(true);       // TODO: 放到连接成功或失败状态之后？

}

// 槽函数：“搜索”按钮
void WinWifi::on_btnSearch_clicked()
{
    qDebug() << "WinWifi::on_btnSearch_clicked() into ...";

    scanWifi();

}

// 槽函数：“上一页”按钮
void WinWifi::on_btnPrevPage_clicked()
{
    qDebug() << "WinWifi::on_btnPrevPage_clicked() into ...";

    prevPage();
}

// 槽函数：“下一页”按钮
void WinWifi::on_btnNextPage_clicked()
{
    qDebug() << "WinWifi::on_btnNextPage_clicked() into ...";

    nextPage();
}

//void WinWifi::slotSetIsOpened(bool _is_open)
//{
//    setIsOpened(_is_open);
//}

// 设置开关状态
void WinWifi::setIsOpened(bool _is_open)
{
    qDebug() << "WinWifi::setIsOpened(): _is_open = " << Util::bool2str(_is_open);

    // 先禁用开关按钮，等收到打开或关闭状态后，再重新启用
    ui->btnIsWifiOpened->setEnabled(false);

    // 记录当前配置
    setCfg_isWifiOpened(_is_open);

    // 即时更新开关的视图，让用户知道机器已经收到用户的指令（用户操作时，视图值与配置值同步）
    updateView_btnIsWifiOpened(_is_open);

    //
    if (_is_open){
        //
        m_isOpening = true;
        m_isClosing = false;

        //2020.9.13  屏蔽  tao
        //qApp->processEvents();
        //emit sigWrite(openWiFiModule,7);    //关闭wifi电源
        //qDebug() << "打开WIFI电源";
        //QThread::msleep(2000);
        //QEventLoop loop;
        //QTimer timer;
        //timer.setSingleShot(true);
        //connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        //timer.start(2000);
        //loop.exec();

//#if (OS_TYPE != 2)
        openWifi();
//#endif
    } else {
        //
        m_isOpening = false;
        m_isClosing = true;

        //emit sigWrite(closeWiFiModule,7);   //2020.9.13  屏蔽  tao
        //qDebug() << "关闭WIFI电源";

//#if (OS_TYPE != 2)
        closeWifi();
//#endif
    }

    // 重新启用开关按钮
    ui->btnIsWifiOpened->setEnabled(true);      // TODO: 正常来说这个应该放到状态事件之后，但是需要确保信号有发生，否则无法恢复按钮？

}

// 槽函数：收到 WiFi 状态
void WinWifi::slot_wifiIntf_RecvStatus(enWifiConnState _status)
{
    qDebug() << "WinWifi::slot_wifiIntf_RecvStatus() into, status = " << _status;

    //
    enWifiConnState wifi_state = _status;
    switch (wifi_state) {
    case wifiConnState_Unknown:
        break;
    case wifiConnState_Idle: {
        //
        m_isOpening = false;
        m_isClosing = false;
    }
        break;
    case wifiConnState_Opened: {
        //
        ui->btnIsWifiOpened->setEnabled(true);

        m_isOpening = false;

        //
        isScanning = true;      /* 因为 rk3568 一打开就自动扫描 */

        //
        connectingSsid = "";

        //
        //scanWifi();     // WiFi 打开后，自动扫描      /* 经测试，rk3568 一打开就自动扫描了 */

    }
        break;
    case wifiConnState_Closed: {
        //
        ui->btnIsWifiOpened->setEnabled(true);

        m_isClosing = false;

        //
        connectingSsid = "";

        //

    }
        break;
    case wifiConnState_ScanResults: {      // 扫描结束状态
        //
        if (isScanning) {
            ui->lblStateDesc->setText(tr("wifi扫描成功"));  // "wifi scan successfully"
        }

        // 若不是正在连接，则将“连接”按钮设为可用
        if (connectingSsid.length() == 0) {
            ui->btnConnect->setEnabled(true);
        }

        // 若未连接，则自动连接上一次成功连接的 ssid
        if (!wifiIntf->getIsConnected() && isScanning) {
            QString last_ssid = getCfg_lastWifiSsid();
            if (last_ssid.length() > 0) {
                int idx = WinWifi::getWifiHistPwdIndex(last_ssid);
                if (idx >= 0) {
                    const stWifiHistPwd &wifi_config = WinWifi::getWifiHistPwds().at(idx);

                    connectingSsid = wifi_config.ssid;
                    lastPassword = wifi_config.pwd;

                    wifiIntf->connectTo(wifi_config.ssid, wifi_config.pwd);
                }
            }
        }

        //
        isScanning = false;

    }
        break;
    case wifiConnState_Connecting: {
        //

    }
        break;
    case wifiConnState_Connected: {
        //


        //
        connectingSsid = "";

    }
        break;
    case wifiConnState_DhcpOk: {
        //ui->lblStateDesc->setText("IP已分配");

        //
        if (!getCfg_isWifiOpened()) {
            logWarning(QString::asprintf("WinWifi::slot_wifiIntf_RecvStatus(): logic abnormal!"));
            setCfg_isWifiOpened(true);
        }

        //
        stWifiInfo connected_info;
        wifiIntf->getConnInfo(connected_info);
        qDebug() << "WinWifi::slot_wifiIntf_RecvStatus(): DhcpOk, ssid = " << connected_info.ssid << ", mac = " << connected_info.localMac << ",  = " << connected_info.localIp;

        //
        ui->lblStateDesc->setText(tr("已连接: ") + connected_info.ssid);  // "connected: "

        ui->btnConnect->setText(tr("断开"));  // "disconnect"
        ui->btnConnect->setEnabled(true);

        // 保存最后一次成功连接的 ssid
        setCfg_lastWifiSsid(connected_info.ssid);

        // 保存成功连接过的 ssid 和 pwd
        WinWifi::addWifiHistPwd(connected_info.ssid, lastPassword);

        //
        connectingSsid = "";

        //
        currentIP = connected_info.localIp;

    }
        break;
    case wifiConnState_Disconnected: {
        ui->lblStateDesc->setText(tr("已断开"));   // "disconnected"

        //
        ui->btnConnect->setText(tr("连接"));  // "Connect"
        ui->btnConnect->setEnabled(true);

        //
        connectingSsid = "";

    }
        break;
    case wifiConnState_PassWordErr: {
        ui->lblStateDesc->setText(tr("密码错误"));  // "password incorrect"

        //
        connectingSsid = "";

    }
        break;
    case wifiConnState_ConnectingFailed: {
        ui->lblStateDesc->setText(tr("连接失败"));  // "connection failed"

        //
        ui->btnConnect->setText(tr("连接"));  // "Connect"
        ui->btnConnect->setEnabled(true);

        //
        connectingSsid = "";

    }
        break;
    }
}

// 槽函数：收到收到
void WinWifi::slot_wifiIntf_WifiListChanged()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    m_wifiList = wifiIntf->getWifiList();
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_wifiList read, size = " << m_wifiList.size();

    //stWifiInfo wifi_info;
    //for (int i = 0; i < wifiList.size(); i++) {
    //    wifi_info = wifiList.at(i);
    //    qDebug() << "wifi_info of : wifiList" << i << ", " << wifi_info.ssid << ", " << wifi_info.strength;
    //}

    // 刷新表格
    updateWifiList();           // TODO: 若刚好用户点击了按钮关闭 WiFi，不应再载入搜索结果？WiFi 接口也要做此逻辑控制

}

void WinWifi::slot_wifiIntf_Notice(QString _msg)
{

    ui->lblStateDesc->setText(_msg);
}

void WinWifi::slot_wifiIntf_Message(QString _msg)
{
    if (CGlobal::isDebugMode)
    {
        getWinManage()->showSuspensionPrompt(_msg, -1);
    }
}

void WinWifi::on_btnHiddenSsid_clicked()
{
    //
    connectWifi("", true);
}
