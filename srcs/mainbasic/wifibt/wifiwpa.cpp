#include "wifiwpa.h"

////
//#include <iostream>

//#include <QIcon>
//#include <QHeaderView>
//#include <QDesktopWidget>
//#include <QRect>
//#include <unistd.h>
//#include <QDebug>
//#include <QRegExp>
//#include <QDir>
//#include <QtAlgorithms>
//#include <QHostInfo>
//#include <QHostAddress>
//#include <QList>
//#include <QStringList>
//#include <QNetworkConfigurationManager>

//#include "windowsmanager.h"
////#include "3G_4G/u3GModule.h"
//#include "statusbarform.h"
//#include "tool.h"
//#include "appsetting.h"

//using namespace std;

////
//extern bool inputWiFiFlag;        // 是否正在输入密码？

////
//#define button_width    85
//#define button_heigth   45

////
//QString g_strIP;
//QString g_strGateWay;
//QString g_strNetMask;

//CWifiWpa *wifiWpa;
//WgtStatusBar *mstatuBar;
//extern QString signalIP;

//int CWifiWpa::WifiSignalItem = 0;
//bool CWifiWpa::isConnect = false;
//bool Get_Network_Time_flag = false;
//QString CWifiWpa::currentIP = "";
//QProcess *CWifiWpa::process = 0;
//QStringList wifiRecords;
//int powerOnconnect=0;
//NetworkInfo *network;

//// wifi模块
//CWifiWpa::CWifiWpa(QObject *parent) :
//    CWifiIntf(parent)
//{
//    ui->setupUi(this);

//    isShowStatusBar = true;
//    mstatuBar = WgtStatusBar::getInstance();

//    wifiIntf = CWifiIntf.getInstance();

//    network = new NetworkInfo();
//    QFont font;
//    font.setPixelSize(22);

//    ui->pushButton_Back->setFocusPolicy(Qt::NoFocus);   //影藏焦点框

//    wifiRecords = winWifi->getWifiHistPwds();

//    ui->tableWidget->horizontalHeader()->setDefaultSectionSize(40);
//    ui->tableWidget->setColumnWidth(0, 500);     //第1列宽度
//    ui->tableWidget->setColumnWidth(1, 86);      //第2列宽度
//    ui->tableWidget->setColumnWidth(2, 86);      //第3列宽度
//    ui->tableWidget->setColumnWidth(3, 86);      //第4列宽度
//    ui->tableWidget->setCurrentCell(0, 0);
//    ui->tableWidget->horizontalHeader()->setSectionsClickable(false);       //水平方向的头不可点击
//    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);    //设置每行内容不可更改
//    ui->tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  //影藏水平滚动条
//    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
//    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

//    //m_ifn = new QInputFileName();
//    //connect(m_ifn, SIGNAL(sendWifiText(QString)), this, SLOT(slotAfterInputPassword(QString)));

//    QHeaderView *hv = ui->tableWidget->verticalHeader();
//    hv->hide();
//    //获取光标所在行列指向的内容
//    connect(ui->tableWidget,SIGNAL(currentItemChanged(QTableWidgetItem*,QTableWidgetItem*)),this,SLOT(slot_itemChanged(QTableWidgetItem*,QTableWidgetItem*)));
//    connect(ui->pushButton_on_off, SIGNAL(clicked()), this, SLOT(slot_open_clicked()));     //打开/关闭网络
//    connect(ui->pushButton_connect, SIGNAL(clicked()), this, SLOT(pbConnectClicked()));     //连接断开
//    connect(ui->pushButton_Search, SIGNAL(clicked()), this, SLOT(slot_scanning()));         //搜索网络
//    connect(ui->pushButton_prev_page, SIGNAL(clicked()), this, SLOT(slot_back()));          //上一页
//    connect(ui->pushButton_next_page, SIGNAL(clicked()), this, SLOT(slot_next()));          //下一页

//    QThread *wpa_work = new QThread;
//    wpa = new WpaCommit;
//    connect(wpa, SIGNAL(signal_read_wifilist()), this, SLOT(slot_read_wifilist()));
//    connect(wpa,SIGNAL(signal_wifiState(bool)),this,SLOT(slot_wifiState(bool)));
//    connect(this,SIGNAL(wpaSig(int)),wpa,SLOT(ctrlCmdHandler(int)));
//    wpa->moveToThread(wpa_work);
//    qt = new QTimer(this);
//    connect(qt, SIGNAL(timeout()), this, SLOT(slot_timeout()));

//    WifiUpdateCount = 0;
//    wifiStatus = new QTimer(this);
//    connect(wifiStatus, SIGNAL(timeout()), this, SLOT(slot_wifiStatus()));      //查看wifi当前状态及重连
//    //wifiStatus->start(1000);
//    wifiListName = new QTimer(this);
//    connect(wifiListName, SIGNAL(timeout()), this, SLOT(UpdateWifiListName())); //显示wifi界面后定时刷新wifi列表
//    wifiListName->stop();

//    inputPwd = new inputWiFipwd(this);
//    connect(inputPwd, SIGNAL(sendWifiText(QString)), this, SLOT(slotAfterInputPassword(QString)));     //密码确认信号
//    connect(inputPwd, SIGNAL(sendCancelFlag(bool)), this, SLOT(CancelEnterPwd(bool)));  //密码取消信号
//    inputPwd->hide();
//    connect(this, SIGNAL(signal_pwd_text_clear()), inputPwd, SLOT(wifi_pwd_text_clear()));  //发送清空密码信号

//    aboutwifi = new wifidetails(this);
//    connect(this, SIGNAL(sendAboutsingal(WIFI,QString)), aboutwifi, SLOT(slot_Aboutsingal(WIFI,QString)));  //获取wifi详细信息
//    connect(aboutwifi, SIGNAL(disconnectWiFi()), this, SLOT(pbConnectClicked()));
//    aboutwifi->hide();

//    m_posPage = 0;
//    m_allPage = 0;
//    setButtonShow();
//    isScanning = false;     //搜索wifi标志
//    isConnect = false;      //连接状态标志
//    manualConnect = false;  //断开,打开wifi标志
//    ui->pushButton_connect->setEnabled(false);
//    ui->pushButton_Search->setEnabled(false);
//    wpa_work->start();
//    wifi_state = winWiFi->getCfg_isWifiOpened();     //获取wifi打开关闭状态
//    if(wifi_state)      //如果关机前wifi是打开状态
//        slot_open();
//    else
//        slot_close();
//}

//CWifiWpa::~CWifiWpa()
//{
//    delete ui;
//}

//bool CWifiWpa::getIsConnected()
//{
//#if (OS_TYPE == 2)
//    return true;
//#else
//    return isConnect;
//#endif
//}

//void CWifiWpa::slot_currentChanged(int index)
//{
//       if(tabWidget->tabText(index) == "IP"){

//       }
//}

//void CWifiWpa::showEvent(QShowEvent *event)
//{
//    if(Tool::SearchNetworkFlag){
//        Tool::SearchNetworkFlag = false;
//        int cmd = CTRL_CMD_SCAN;
//        emit wpaSig(cmd);       //搜索网络
//    }
//    wifiStatus->stop();         //进入wifi界面停止刷新网络
//    wifiListName->start(8000);
//    wifi_open = winWiFi->getCfg_isWifiOpened();     //获取wifi打开关闭状态
//    if(wifi_state){
//        //ui->pushButton_on_off->click();       //不能加这句,否则一进wifi界面就关闭wifi
//        readWifiListState = true;
//        qDebug()<<"--m_open->click()--";
//    }
//    else
//        readWifiListState = false;

//    QPixmap pixmap;
//    QPalette palette;
//    if(theme == 1){
//        pixmap.load(":/resource/black_theme/blackground_color_b.png");
//        ui->tableWidget->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(50,50,50); color:rgb(204,204,204);}");
//        ui->tableWidget->setStyleSheet("QTableWidget{background-color:rgb(20,20,20); color:rgb(204,204,204);}");
//        ui->pushButton_connect->setStyleSheet("background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px; padding:2px 4px;");

//        ui->pushButton_prev_page->setStyleSheet("background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px; padding:2px 4px;");
//        ui->pushButton_next_page->setStyleSheet("background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px; padding:2px 4px;");
//        ui->label_state->setStyleSheet("color:rgb(204,204,204);");
//        ui->label_page->setStyleSheet("color:rgb(204,204,204);");
//        ui->label_Home->setStyleSheet("color:rgb(204,204,204);");
//        ui->label_Search->setStyleSheet("color:rgb(204,204,204);");
//        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");
//        if(wifi_open)
//            ui->pushButton_on_off->setIcon(QIcon(":/resource/black_theme/switch-off_b.png"));
//        else
//            ui->pushButton_on_off->setIcon(QIcon(":/resource/black_theme/switch-on_b.png"));
//        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
//        ui->pushButton_Search->setIcon(QIcon(":/resource/black_theme/find_b.png"));
//        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png")) ;
//    }
//    else{
//        pixmap.load(":/resource/white_theme/whiteground_color_w.png");
//        ui->tableWidget->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
//        ui->tableWidget->setStyleSheet("QTableWidget{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
//        ui->pushButton_connect->setStyleSheet("background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");

//        ui->pushButton_prev_page->setStyleSheet("background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");
//        ui->pushButton_next_page->setStyleSheet("background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");
//        ui->label_state->setStyleSheet("color:rgb(1,1,1);");
//        ui->label_page->setStyleSheet("color:rgb(1,1,1);");
//        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
//        ui->label_Search->setStyleSheet("color:rgb(1,1,1);");
//        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");
//        if(wifi_open)
//            ui->pushButton_on_off->setIcon(QIcon(":/resource/white_theme/switch-off_w.png"));
//        else
//            ui->pushButton_on_off->setIcon(QIcon(":/resource/white_theme/switch-on_w.png"));
//        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
//        ui->pushButton_Search->setIcon(QIcon(":/resource/white_theme/find_w.png"));
//        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
//    }
//    palette.setBrush(backgroundRole(), QBrush(pixmap));
//    setPalette(palette);
//    //获取wifi信息
//    isConnectWlan();

//    QStringList header;
//    if(language){
//        mstatuBar->setTitle("网络配置");
//        ui->pushButton_prev_page->setText("上一页");
//        ui->pushButton_next_page->setText("下一页");
//        if(isConnect)
//            ui->pushButton_connect->setText("断开");
//        else
//            ui->pushButton_connect->setText("连接");
//        ui->label_Home->setText("主页");
//        ui->label_Search->setText("搜索");
//        ui->label_Back->setText("返回");
//        header.append("网络名称(SSID)");
//        header.append("数据加密");
//        header.append("信号强度");
//        header.append("详细信息");
//    }
//    else{
//        mstatuBar->setTitle("Networkconfig");
//        ui->pushButton_prev_page->setText("Prev");
//        ui->pushButton_next_page->setText("Next");
//        if(isConnect)
//            ui->pushButton_connect->setText("Disconnect");
//        else
//            ui->pushButton_connect->setText("Connect");
//        ui->label_Home->setText("Home");
//        ui->label_Search->setText("Search");
//        ui->label_Back->setText("Back");
//        header.append("SSID");
//        header.append("Entry");
//        header.append("Signal");
//        header.append("Details");
//    }
//    ui->tableWidget->setHorizontalHeaderLabels(header);
//    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
//    mstatuBar->setCurrentThemeType(theme);
//    QWidget::update();
//}

//void CWifiWpa::hideEvent(QHideEvent *event)
//{
//    wifiStatus->start(1000);    //退出界面重启wifi新刷新功能
//    wifiListName->stop();       //退出界面停止刷新wifi列表
//}

//void CWifiWpa::slot_itemChanged(QTableWidgetItem *current, QTableWidgetItem *previous)
//{
//    if(ui->tableWidget->currentItem() == NULL)
//        return;
//    QTableWidgetItem *item = ui->tableWidget->item(current->row(),0);   //提取光标所在行,列不要,默认第0列
//    QString m_ssid = item->text();
//    if(current->column()==3){
//        for(int n=0;n<m_wifi.size();n++)    //查找wifi名
//        {
//            if(m_wifi[n].ssid == m_ssid && !inputWiFiFlag)
//                emit sendAboutsingal(m_wifi[n],currentSSID);
//        }
//    }
//    ui->tableWidget->setCurrentItem(item);  //设置光标在当前选中行的第一格
//}

////输入密码后就进入到这一步
//void CWifiWpa::slotAfterInputPassword(QString WifiPwdtText)
//{
//    ReceivePWD = WifiPwdtText;
//    qDebug()<<"--QWiFiList::slotAfterInputPassword()"<<" ReceivePWD:"<<ReceivePWD;
//    if(WpaCommit::currentWIFI.ssid.isEmpty())
//    {
//        qDebug()<<"WpaCommit::currentWIFI.ssid==NULL";
//        //m_ifn->close();
//        return;
//    }
//    if(isConnectWlan()){    //查询连接信息
//        slot_close();
//        slot_open();
//        qDebug()<<"--re open";
//    }
//    ui->pushButton_connect->setEnabled(false);
//    ui->pushButton_Search->setEnabled(false);

////    WpaCommit::currentWIFI = m_wifi.at(ui->tableWidget->currentItem()->row() + (m_posPage - 1)*10);
//    WpaCommit::currentWIFI.pwd = ReceivePWD;

//    currentSSID = WpaCommit::currentWIFI.ssid;
//    currentPWD = WpaCommit::currentWIFI.pwd;
//    qDebug()<<"--currentSSID:"<<currentSSID<<",currentPWD:"<<currentPWD<<"--";
//    //m_ifn->close();
//    QString c_str = language ? "正在进行身份验证...":"Identity verification in progress...";
//    ui->label_state->setText(c_str);
//    ui->pushButton_connect->setEnabled(false);
//    //wpa->wpa_connect_wifi(wifi);
//    conCnt = 0;
//    conTime = 0;
//    int cmd = WPA_CONNECT;
//    emit wpaSig(cmd);   //请求连接
//    qt->start(500);     //定时器查询连接状态
//}

//void CWifiWpa::CancelEnterPwd(bool CancelFlag)
//{
//    ui->pushButton_connect->setEnabled(CancelFlag);
//    QString c_str = language ? "wifi未连接":"wifi disconnect";
//    ui->label_state->setText(c_str);
//}

////断开连接
//void CWifiWpa::pbConnectClicked()
//{
//    if(!wifi_open)  //wifi打开,关闭标志(关闭wifi时清除wifi列表了,所以该判断可有可无)
//    {
//        QString c_str;
//        manualConnect = true;
//        qDebug()<<"--connect clicked!,manualConnect:"<<manualConnect;
//        if(ui->pushButton_connect->text().contains("断开") || ui->pushButton_connect->text().contains("Disconnect")){  //断开wifi
//            isConnect = false;
//            int cmd = CTRL_CMD_DISCON;  //断开连接
//            emit wpaSig(cmd);
//            cmd = CTRL_CMD_SCAN;    //搜索wifi
//            emit wpaSig(cmd);
//            c_str = language ? "wifi未连接" : "wifi disconnect";
//            ui->label_state->setText(c_str);
//            QString connectText = language ? "连接" : "Connect";
//            ui->pushButton_connect->setText(connectText);
//            ui->pushButton_connect->setEnabled(true);
//            ui->pushButton_Search->setEnabled(true);
//            readWifiListState = false;
//            currentIP = "";
//            currentSSID = "";
//            currentPWD = "";
//            wifiStatus->stop();
//        }
//        else{   //连接wifi
//            readWifiListState = true;
//            if(ui->tableWidget->currentItem()==NULL)
//            {
//                return;
//            }
//            c_str = language ? "连接中..." : "Connecting...";
//            ui->label_state->setText(c_str);
//            ui->pushButton_connect->setEnabled(false);
//            WpaCommit::currentWIFI = m_wifi.at(ui->tableWidget->currentItem()->row() + (m_posPage - 1)*10);
//            if(m_wifi.at(ui->tableWidget->currentItem()->row() + (m_posPage - 1)*10).encry) //如果tableWidget某行指向的wifi是加密的
//                inputPwd->EnterPwdText(m_wifi.at(ui->tableWidget->currentItem()->row() + (m_posPage - 1)*10).ssid,true); //m_ifn->onShow(wifi名字,true)
//            else    //否则tableWidget某行指向的wifi是不需要密码的
//            {
//                inputPwd->wifi_pwd_text->clear();
//                slotAfterInputPassword("");
//            }
//        }
//        return;
//    }
//}

//QStringList CWifiWpa::getKO()
//{
//    QStringList res;
//    QString path = QString("/lib/modules/%1/wifi").arg(configure.platform);
//    QDir dir(path);
//    if(dir.exists()){
//        QFileInfoList list = dir.entryInfoList();
//        foreach (QFileInfo file, list) {
//            if(file.fileName().contains("ko")){
//                res.append(file.fileName().mid(0, file.fileName().length()-3));
//            }
//        }
//    }
//    return res;
//}

////请求获取IP地址
//void CWifiWpa::getIP()
//{
//    if(process == 0)
//    {
//        process = new QProcess;
//        process->setProgram("udhcpc");
//        QStringList list;
//        list.append("-i");
//        list.append("wlan0");
//        process->setArguments(list);

//        processTimer = new QTimer;
//        processTimer->setSingleShot(true);

//        connect(process, SIGNAL(finished(int)), this, SLOT(getIPSlot(int)));
//        connect(processTimer,SIGNAL(timeout()), this, SLOT(processTimeout()));
//    }
//    if(process->state() == QProcess::Running)
//        process->kill();
//    process->start();
//    processTimer->start(10000);

//    manualConnect = false;
//}

////获取IP地址
//void CWifiWpa::getIPSlot(int exitCode)
//{
//    processTimer->stop();
//    QString c_str,f_str;
//    if(exitCode < 0)
//    {
//        c_str = language ? "连接失败:" : "Connected failed";
//        f_str = language ? "\n本机IP获取失败" : "\nLocal IP acquisition failed";
//        ui->label_state->setText(c_str+ currentSSID +f_str);
//        qDebug()<<"exitCode < 0--连接失败: IP获取失败";
//        QString connectText = language ? "连接" : "Connect";
//        ui->pushButton_connect->setText(connectText);
//        isConnect = false;
//        wpa->wpa_ctrl_cmd("disconnect 0");
//    }
//    else
//    {
//        wifiWpa->network->Name = "wlan0";   //设备节点
//        getLocalInfo(wifiWpa->network);
//        QString localHostName = QHostInfo::localHostName();
//        //qDebug()<<"--localHostName:"<<localHostName;        //本地主机名

//        getLocalInfo(network);
//        if(!(network->Ip.isEmpty()))        //连接成功(..............end)
//        {
//            //qDebug()<<"--connecting to "<<network->Ip;      //已连接上的wifi地址
//            //qDebug() << "currentSSID" << currentSSID;       //已连接上的wifi名字
//            QString c_str = language ? "已连接:":"Connected:";
//            ui->label_state->setText(c_str+ currentSSID);
//            currentIP = network->Ip;
//            signalIP = network->Ip;
//            QString disconnectText = language ? "断开" : "Disconnect";
//            ui->pushButton_connect->setText(disconnectText);
//            readWifiListState = false;
//            isConnect = true;
//            emit signal_pwd_text_clear();   //连接成功则清空输入的密码文本
//            wifiStatus->start(1000);    //2020.12.10  tao
//            Get_Network_Time_flag = true;
//        }
//        else
//        {
//            c_str = language ? "连接失败:" : "Connected failed";
//            f_str = language ? "\n本机IP获取失败" : "\nLocal IP acquisition failed";
//            ui->label_state->setText(c_str+ currentSSID +f_str);
//            qDebug()<<"-----连接失败: IP获取失败";
//            QString connectText = language ? "连接" : "Connect";
//            ui->pushButton_connect->setText(connectText);
//            isConnect = false;
//            wpa->wpa_ctrl_cmd("disconnect 0");
//        }
//    }

//    ui->pushButton_connect->setEnabled(true);
//    ui->pushButton_Search->setEnabled(true);
//}

//void CWifiWpa::processTimeout()
//{
//    process->kill();
//    QString c_str = language ? "连接失败:" : "Connected failed";
//    QString f_str = language ? "\n本机IP获取失败" : "\nLocal IP acquisition failed";
//    ui->label_state->setText(c_str+ currentSSID +f_str);
//    isConnect = false;
//    wpa->wpa_ctrl_cmd("disconnect 0");
//    qDebug()<<"processTimeout--连接失败:\nIP获取失败";
//}

//bool CWifiWpa::checkNetwork()
//{
////    QNetworkConfigurationManager mgr;

////    if(mgr.isOnline()){
////        qDebug()<<"checkNetwork::device is online";
////        return true;
////    }
////    else{
////        qDebug()<<"checkNetwork::device is offline";
////        return false;
////    }
//    return CWifiWpa::getIsConnected();
//}

////读取连接信息
//bool CWifiWpa::isConnectWlan()
//{
//    network->clear();
//    network->Name = "wlan0";
//    QString c_str;

//    getLocalInfo(network);  //读取本地网络信息
//    if(!(network->Ip.isEmpty()) && checkNetwork()){
//        isConnect = true;
//        QString localHostName = QHostInfo::localHostName();     //本地主机名
//        //qDebug()<<"--localHostName:"<<localHostName;
//        //qDebug()<<"network->Ip:"<<network->Ip;    //IP地址
//        c_str = language ? "已连接:":"Connected:";
//        ui->label_state->setText(c_str+ currentSSID);

//    } else {
//        isConnect = false;
//        c_str = language ? "wifi未连接":"wifi disconnect";
//        ui->label_state->setText(c_str);
//        c_str = language ? "连接":"Connect";
//        ui->pushButton_connect->setText(c_str);
//        ui->pushButton_connect->setEnabled(true);   //2020.9.1  tao
//    }

//    return isConnect;
//}

//void CWifiWpa::setButtonShow()
//{
//    QString page = language ? "页":"Page";
//    ui->label_page->setText(QString("%1/%2").arg(m_posPage).arg(m_allPage)+page);
//    if(m_posPage <= 1)
//        ui->pushButton_prev_page->setEnabled(false);
//    else
//        ui->pushButton_prev_page->setEnabled(true);
//    if(m_posPage >= m_allPage)
//        ui->pushButton_next_page->setEnabled(false);
//    else
//        ui->pushButton_next_page->setEnabled(true);
//}

//void CWifiWpa::sort()
//{
//    temp_wifi.clear();
//    temp_wifi.append(WpaCommit::m_wifilist);

//    QMap<int,int> sortMap;
//    for(int i=0;i<temp_wifi.size();i++){
//        WIFI temp = temp_wifi.at(i);
//        int strength = temp.strength.toInt();
//        sortMap.insert(-strength,i);
//    }

//    m_wifi.clear();
//    QMap<int,int>::iterator it = sortMap.begin();
//    for(int i=0;it!=sortMap.end(),i<sortMap.size();it++,i++){
////        if(i>20)
////            break;
//        int index = it.value();
//        //qDebug()<<"index:"<<index;
//        m_wifi.append(temp_wifi.at(index));
//    }

//    /*
//    for(QVector<WIFI>::Iterator it = m_wifi.begin(); it != m_wifi.end(); it++){
//        for(QVector<WIFI>::Iterator ip = m_wifi.begin() + 1 ; ip != m_wifi.end(); ip++){
//            if(!SortByM1(*it, *ip)){
//                WIFI temp;
//                temp = *it;
//                *it = *ip;
//                *ip = temp;
//            }
//        }
//    }
//    */
//}

////向tableWidget中添加内容
//void CWifiWpa::addTabWidgetItem()
//{
//    int pos = 0;
//    ui->tableWidget->clear();
//    //sort();
//    QStringList header;
//    //QString signalconnect = language ? "连接状态" : "Connect";
//    QString ssidText = language ? "网络名称(SSID)" : "SSID";
//    QString dataText = language ? "数据加密" : "Encrypt";
//    QString signalText = language ? "信号强度" : "Signal";
//    QString signalDetails = language ? "详细信息" : "Details";
//    header.append(ssidText);
//    header.append(dataText);
//    header.append(signalText);
//    header.append(signalDetails);
//    ui->tableWidget->setHorizontalHeaderLabels(header);
//    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
//    ui->tableWidget->setRowCount(10);
//    for(pos = 0; pos < (m_wifi.length() - (m_posPage-1)*10) && pos < 10; pos++){
//        int i = pos + (m_posPage-1)*10;
//        WIFI wifi = m_wifi.at(i);
//        //网络名称
//        ui->tableWidget->setItem(pos, 0, new QTableWidgetItem(wifi.ssid.toStdString().c_str()));
//        if(theme == 1){
//            //数据加密
//            //ui->tableWidget->setItem(pos, 1, new QTableWidgetItem(wifi.getEncry().toStdString().c_str()));
//            if(wifi.encry)
//                ui->tableWidget->setItem(pos, 1, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_lock_b.png")),""));
//            else
//                ui->tableWidget->setItem(pos, 1, new QTableWidgetItem(""));
//            //信号强度
//            int strength = wifi.getStrength();
//            if(strength>=-60 && strength<0)
//                ui->tableWidget->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_signal4_b.png")),""));
//            else if(strength>=-80 && strength<-60)
//                ui->tableWidget->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_signal3_b.png")),""));
//            else if(strength>=-90 && strength<-80)
//                ui->tableWidget->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_signal2_b.png")),""));
//            else if(strength>=-100 && strength<-90)
//                ui->tableWidget->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_signal1_b.png")),""));
//            //qDebug()<<"-----strength:"<<strength;
//            //wifi详细信息
//            ui->tableWidget->setItem(pos, 3, new QTableWidgetItem(QIcon(QPixmap(":/resource/black_theme/wifi_message_b.png")),""));
//        }
//        else{
//            if(wifi.encry)
//                ui->tableWidget->setItem(pos, 1, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_lock_w.png")),""));
//            else
//                ui->tableWidget->setItem(pos, 1, new QTableWidgetItem(""));
//            int strength = wifi.getStrength();
//            if(strength>=-60 && strength<0)
//                ui->tableWidget->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_signal4_w.png")),""));
//            else if(strength>=-80 && strength<-60)
//                ui->tableWidget->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_signal3_w.png")),""));
//            else if(strength>=-90 && strength<-80)
//                ui->tableWidget->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_signal2_w.png")),""));
//            else if(strength>=-100 && strength<-90)
//                ui->tableWidget->setItem(pos, 2, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_signal1_w.png")),""));
//            ui->tableWidget->setItem(pos, 3, new QTableWidgetItem(QIcon(QPixmap(":/resource/white_theme/wifi_message_w.png")),""));
//        }
//        ui->tableWidget->setRowHeight(pos, 40);       //设置行高度
//    }
//}

//void CWifiWpa::pbCloseClicked() {
////    if(isConnect){
////        if(QMessageBox::question(this, "提示", "是否断开wifi连接" ) == QMessageBox::Yes){
////            slot_close();
////        }
////    }
////    gWiFi->close();
//}

//void CWifiWpa::slot_timeout()
//{
//    //qDebug()<<"--slot_timeout() conTime++"<<conTime;
//    QString c_str;
//    conTime++;
//    if(conTime>10){     //2020.6.2
//        qt->stop();
//        conTime=0;
//        c_str = language ? "wifi连接失败":"wifi connection failed";
//        ui->label_state->setText(c_str);
//        isConnect = false;
//        wpa->wpa_ctrl_cmd("disconnect 0");  //取消连接
//        ui->pushButton_connect->setEnabled(true);
//        ui->pushButton_Search->setEnabled(true);
//    }
//    //qDebug()<<"slot_timeout::--emit status request--"<<QThread::currentThreadId();
//    //int cmd = CTRL_REQUEST_STATUS;
//    //emit wpaSig(cmd);

//    char reply[2048];
//    size_t reply_len = sizeof(reply) -1;
//    wpa->ctrlRequest("STATUS", reply, &reply_len);
//    reply[reply_len] = '\0';
//    QString bss(reply);
//    QStringList lines = bss.split(QRegExp("\\n"));
//    QString state;
//    for(QStringList::Iterator it = lines.begin();
//        it != lines.end(); it++) {
//        int pos = (*it).indexOf('=') + 1;
//        if (pos < 1)
//            continue;
//        if ((*it).startsWith("wpa_state=")){
//            state = (*it).mid(pos);
//            if(state.contains("COMPLETED")){
//                qt->stop();
//                c_str = language ? "wifi已连接\t正在获取IP地址":"wifi connected\tObtaining IP address";
//                ui->label_state->setText(c_str);
//                ui->label_state->update();
//                isConnect = true;
//                ui->pushButton_connect->setEnabled(false);
//                ui->pushButton_Search->setEnabled(false);
//                getIP();    //请求获取IP地址
//                winWifi->addWifiHistPwd(currentSSID, currentPWD);    //把wifi密码保存到本地
//            }else  if(state.contains("4WAY_HANDSHAKE") && conTime > 5){
//                qDebug()<<"--WiFiList::slot_timeout(),STOP";
//                qt->stop();
//                c_str = language ? "wifi连接失败":"wifi connection failed";
//                ui->label_state->setText(c_str);
//                isConnect = false;
//                wpa->wpa_ctrl_cmd("disconnect 0");
//                ui->pushButton_connect->setEnabled(true);
//                ui->pushButton_Search->setEnabled(true);
//            }
//        }
//    }
//}

////下一页
//void CWifiWpa::slot_next()
//{
//    m_posPage++;
//    setButtonShow();
//    addTabWidgetItem();
//}

////上一页
//void CWifiWpa::slot_back()
//{
//    m_posPage--;
//    setButtonShow();
//    addTabWidgetItem();
//}

////搜索网络
//void CWifiWpa::slot_scanning()
//{
//    ui->pushButton_Search->setEnabled(false);
//    QString c_str = language ? "正在扫描附近wifi...":"Searching for wifi...";
//    ui->label_state->setText(c_str);
//    //wpa->wpa_ctrl_cmd("SCAN");
//    readWifiListState = true;
//    int cmd = CTRL_CMD_SCAN;
//    emit wpaSig(cmd);
//    isScanning = true;
//}

//void CWifiWpa::slot_open_clicked()
//{
//    qDebug()<<"--slot_open_clicked--";
//    wifi_open = winWiFi->getCfg_isWifiOpened();     //获取wifi按钮打开关闭状态
//    if(wifi_open){
//        if(theme == 1)
//            ui->pushButton_on_off->setIcon(QIcon(":/resource/black_theme/switch-on_b.png"));
//        else
//            ui->pushButton_on_off->setIcon(QIcon(":/resource/white_theme/switch-on_w.png"));
//        //2020.9.13  屏蔽  tao
//        //qApp->processEvents();
//        //emit sigWrite(openWiFiModule,7);    //关闭wifi电源
//        //qDebug() << "打开WIFI电源";
//        //QThread::msleep(2000);
//        //QEventLoop loop;
//        //QTimer timer;
//        //timer.setSingleShot(true);
//        //connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
//        //timer.start(2000);
//        //loop.exec();

//#if (OS_TYPE != 2)
//        slot_open();
//#endif

//        wifi_state = true;
//        wifi_open = false;
//    }
//    else{
//        //emit sigWrite(closeWiFiModule,7);   //2020.9.13  屏蔽  tao
//        //qDebug() << "关闭WIFI电源";
//        if(theme == 1)
//            ui->pushButton_on_off->setIcon(QIcon(":/resource/black_theme/switch-off_b.png"));
//        else
//            ui->pushButton_on_off->setIcon(QIcon(":/resource/white_theme/switch-off_w.png"));

//#if (OS_TYPE != 2)
//        slot_close();
//#endif

//        ui->tableWidget->clearContents();   //清除wifi列表
//        wifi_state = false;
//        isConnect = false;
//        wifi_open = true;
//    }
//}

////关闭wifi
//void CWifiWpa::slot_close()
//{
//    isConnect = false;
//    qt->stop();
//    wifiStatus->stop();
//    //wpa->wpa_ctrl_cmd("REMOVE_NETWORK 0");
//    //wpa->wpa_ctrl_cmd("TERMINATE");
//    //system("ifconfig wlan0 down");
//    //system("dhclient -r wlan0");
//    int cmd = CTRL_CMD_CLOSE;
//    emit wpaSig(cmd);
//    QString c_str = language ? "wifi未连接":"wifi disconnect";
//    ui->label_state->setText(c_str);
//    ui->pushButton_connect->setEnabled(false);
//    ui->pushButton_Search->setEnabled(false);
//    winWiFi->setCfg_isWifiOpened(false);
//}

////打开wifi
//void CWifiWpa::slot_open()
//{
//    system("ifconfig eth0 down");
//    QString c_str = language ? "wifi开启中...":"wifi on...";
//    ui->label_state->setText(c_str);
//    qDebug()<<"wifi开启中...";
//    ui->label_state->update();
//    QString connectText = language ? "连接" : "Connect";
//    ui->pushButton_connect->setText(connectText);
//    readWifiListState = true;
//    manualConnect = false;
//    conCnt = 0;
//    conTime = 0;
//    //wpa->WPA_init();
//    int cmd = WPA_INIT;
//    emit wpaSig(cmd);
//    isScanning = true;
//    winWiFi->setCfg_isWifiOpened(true);

//    //2020.9.16   tao   打开wifi后搜索网络
//    //ui->label_state->setText("正在扫描附近wifi...");
//    QEventLoop loop;
//    QTimer timer;
//    timer.setSingleShot(true);
//    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
//    timer.start(2000);
//    loop.exec();
//    slot_scanning();    //搜索网络
//}

//void CWifiWpa::autoConnect()
//{
//    qDebug()<<"--enter autoConnect--";
//    if(WinMeasure::isOpened()){
//        qDebug()<<"Camera is running,return;";
//        return;
//    }
//    if(conTime > 5){
//        qDebug()<<"conTime > 5,return";
//        return;
//    }
////     QList<QStringList> wifiRecords = winWifi->getWifiHistPwds();
////     int recordSize = wifiRecords.size();
//    qDebug()<<"wifiRecords.size="<<wifiRecords.size();

//     int wifiListSize = m_wifi.size();
//     for(int k=0;k<wifiRecords.size();k++){
//         QString recordData = wifiRecords.at(k);
//         QString ssid = recordData.section(',',0,0);
//         QString pwd = recordData.section(',',1,1);

//         for(int i=0;i<wifiListSize;i++){
//              WpaCommit::currentWIFI = m_wifi.at(i);
//             qDebug()<<"-----wifiInfo.ssid:"<<WpaCommit::currentWIFI.ssid<<",ssid:"<<ssid;
//             if(WpaCommit::currentWIFI.ssid == ssid){
//                 qDebug()<<"--QWiFiList::autoConnect-find ssid:"<<ssid<<",pwd record:"<<pwd;

//                 ui->pushButton_connect->setEnabled(false);
//                 ui->pushButton_Search->setEnabled(false);

//                 WpaCommit::currentWIFI.pwd = pwd;
//                 currentSSID = ssid;
//                 currentPWD = pwd;
////                 if(wifi.pwd==""){
////                     wifi.key_mgmt == "NONE";
////                     wifi.encry = false;
////                     qDebug()<<"--"<<wifi.ssid<<"has no pwd";
////                 }
//                 conCnt = 0;
//                 conTime = 0;
////                 m_pbConnect->setText("取消");
//                 QString c_str = language ? "正在进行身份验证...":"Authenticating...";
//                 ui->label_state->setText(c_str);
////                 wpa->wpa_connect_wifi(WpaCommit::currentWIFI);
//                 int cmd = WPA_CONNECT;
//                 emit wpaSig(cmd);

//                 qt->start(500);
//                 return;
//             }
//         }
//     }
//     qDebug()<<"--leave autoConnect--";
//}

//void CWifiWpa::slot_wifiStatus()
//{
//    //    isConnectWlan();
//    //    if(isConnect){
//    //        powerOnconnect = 0;
//    //        int cmd = CTRL_REQUEST_STATUS;  //定时查看wifi状态
//    //        emit wpaSig(cmd);
//    //    }
//    //    else{
//    //        //wifi异常断开续连   2020.7.23
//    //        qDebug()<<"-----try connect! ssid:"<<WpaCommit::currentWIFI.ssid<<"  pwd:"<<WpaCommit::currentWIFI.pwd;
//    //        conCnt = 0;
//    //        conTime = 0;
//    //        powerOnconnect++;
//    //        if(powerOnconnect >= 6)     //wifi上电后连接失败的重新搜索网络3次
//    //            powerOnconnect = 6;
//    //        if(powerOnconnect <= 5){
//    //            int cmd = CTRL_CMD_SCAN;
//    //            emit wpaSig(cmd);         //搜索网络
//    //            readWifiListState = true;
//    //        }
//    //        int cmd = WPA_CONNECT;
//    //        emit wpaSig(cmd);   //请求连接
//    //        qt->start(500);     //定时器查询连接状态
//    //    }

//    //2020.12.10  tao
//    WifiUpdateCount++;
//    if(WifiUpdateCount%2 == 0)
//    {
//        if(WifiUpdateCount > 1000)
//            WifiUpdateCount = 0;
//        QFile file1("/sys/class/net/wlan0/operstate");
//        if(file1.open(QIODevice::ReadOnly | QIODevice::Text))
//        {
//            QTextStream in(&file1);
//            QString Wlan0ConnectState = in.readLine();
//            file1.close();

//            if(Wlan0ConnectState.contains("down")) //已断开
//            {
//                //qDebug()<<"-----Wlan0DisConnecrt:"<<Wlan0ConnectState;
//                int cmd1 = CTRL_CMD_SCAN;
//                emit wpaSig(cmd1);         //搜索网络
//                readWifiListState = true;
//                QThread::msleep(50);
//                int cmd2 = WPA_CONNECT;
//                emit wpaSig(cmd2);   //请求连接
//                qt->start(500);     //定时器查询连接状态
//            }
//        }
//        else
//            qDebug()<<"-----operstate file open faile!";
//    }

//    QFile file2("/proc/net/wireless");
//    if (file2.open(QIODevice::ReadOnly | QIODevice::Text))
//    {
//        QString StrLine;
//        QTextStream in(&file2);             //用文件构造流
//        StrLine = in.readLine();            //读取一行放到字符串里()

//        while(!StrLine.isNull())            //字符串有内容
//        {
//            StrLine=in.readLine();          //循环读取下行
//            if(StrLine.contains("wlan0"))   //已断开
//            {
//                QStringList wStrList = StrLine.split(".");
//                QString SignalNums = wStrList.at(1);
//                QString SignalNum = SignalNums.trimmed();   //去除首尾空格
//                //qDebug()<<"-----SignalNum:"<<SignalNum;     //当前连接wifi的信号强度
//                if(!isConnect && SignalNum.toInt() == 0)    //断开状态
//                    WifiSignalItem = 0;
//                else if(SignalNum.toInt()>0 && SignalNum.toInt()<=25)
//                    WifiSignalItem = 1;
//                else if(SignalNum.toInt()>25 && SignalNum.toInt()<=50)
//                    WifiSignalItem = 2;
//                else if(SignalNum.toInt()>50 && SignalNum.toInt()<=75)
//                    WifiSignalItem = 3;
//                else if(SignalNum.toInt()>75 && SignalNum.toInt()<=100)
//                    WifiSignalItem = 4;
//            }
//        }
//    }
//    file2.close();
//}

//void CWifiWpa::UpdateWifiListName()
//{
//    //qDebug()<<"-----update wifi list";
//    if(isConnect && ui->label_state->text()==NULL || isConnect && ui->tableWidget->item(0,0)->text()==NULL)
//        readWifiListState = true;
//    slot_read_wifilist();
//}

//void CWifiWpa::slot_RecvSysSignal(enSysSignal _sys_signal)
//{
//    if(wifi_status == 100){
//        wifiStatus->stop();
//    }
//    if(wifi_status == 101 && !manualConnect && isConnect){
//        wifiStatus->start(1000);
//        if(wifi_state)  //2020.7.23
//            readWifiListState = true;
//        else
//            readWifiListState = false;
//        isConnectWlan();        //获取wifi信息
//    }
//}

////读取wifi列表
//void CWifiWpa::slot_read_wifilist()
//{
//    //qDebug()<<"-----------slot_read_wifilist-----------"<<QThread::currentThreadId();

//    if(WinMeasure::isOpened()){
//        qDebug()<<"Camera is running,return;";
//        return;
//    }

//    if(!readWifiListState){
//        //qDebug()<<"readWifiListState:"<<readWifiListState<<",return";
//        return;
//    }

//    if(isScanning){
//        QString c_str = language ? "wifi扫描成功":"wifi scan successfully";
//        ui->label_state->setText(c_str);
//    }
//    isScanning = false;
//    ui->pushButton_connect->setEnabled(true);
//    ui->pushButton_Search->setEnabled(true);

//    sort();     //wifi列表添加到容器中

////    m_wifi.clear();
////    m_wifi.append(temp_wifi);
//    m_posPage = 1;
//    if(m_wifi.length()%10 > 0)
//        m_allPage = m_wifi.length() / 10 + 1;
//    else m_allPage = m_wifi.length() / 10;
//    setButtonShow();    //页数显示
//    addTabWidgetItem(); //wifi列表添加到TabWidget中

//    if(!manualConnect){ //打开wifi标志
//        autoConnect();  //本地密码连接
//        isConnectWlan();//读取连接信息
//    }
//}

////查询wif连接状态
//void CWifiWpa::slot_wifiState(bool state)
//{
//    qt->stop();
//    QString c_str;
//    if(WinMeasure::isOpened())
//        return;
//    //qDebug()<<"-----WiFiList::slot_wifiState():"<<state;

//    if(state){
//        conCnt = 0;
//        conTime = 0;
//        //ui->label_state->setText("wifi已连接\t正在获取IP地址");
//        ui->label_state->update();
//        isConnect = true;
//        getIP();
//        winWifi->addWifiHistPwd(currentSSID, currentPWD);
//        QString disconnectText = language ? "断开" : "Disconnect";
//        ui->pushButton_connect->setText(disconnectText);
//    }
//    else
//    {
//        if(!isConnectWlan())
//        {
//            c_str = language ? "wifi连接失败":"wifi connection failed";
//            ui->label_state->setText(c_str);
//            qDebug()<<"--wifi连接失败,cnt:"<<conCnt;
//            QString connectText = language ? "连接" : "Connect";
//            ui->pushButton_connect->setText(connectText);
//            wpa->wpa_ctrl_cmd("disconnect 0");//******
//            isConnect = false;
//        }
//    }
//    ui->pushButton_connect->setEnabled(true);
//    ui->pushButton_Search->setEnabled(true);
//}

//void CWifiWpa::on_pushButton_Home_clicked()
//{
//    if(!inputWiFiFlag)
//        getWinManage()->showWindowByType(WIN_HOME);
//}

//void CWifiWpa::on_pushButton_Back_clicked()
//{
//    qDebug()<<"--pushButton_Back_Clicked";
//    //emit sendSIGNAL(sysSignal_13);
//    if(!inputWiFiFlag)
//        getWinManage()->showWindowByType(WIN_TOOL);
//}

bool CWifiWpa::setIsOpened(bool _is_opened)
{
    // 记下旧的连接状态
    static bool is_conn_last = false;       /* 默认为未连接 */

    //
    m_isOpened = _is_opened;
    m_isConnected = _is_opened;

    //TOTO:


    // 检查“是否已连接”状态的改变
    QTimer::singleShot(3000, this, [this] () {
        bool is_conn_new = getIsConnected();
        if (is_conn_new != is_conn_last) {
            is_conn_last = is_conn_new;
            emit sigConnectedStateChanged(is_conn_new);     // 发射“是否已连接”状态改变信号
        }
    });

    return true;
}

bool CWifiWpa::getIsOpened()
{
    return m_isOpened;
}

bool CWifiWpa::getIsConnected()
{
#if OS_TYPE!=2
    return m_isConnected;
#else
    static bool is_connected = true;
    return is_connected;
#endif
}

