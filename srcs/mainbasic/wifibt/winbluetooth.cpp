#include "winbluetooth.h"
#include "ui_winbluetooth.h"

#include <iostream>
#include <unistd.h>

#include <QEventLoop>
#include <QThread>
#include <QtCore>
#include <QDebug>
#include <QApplication>
#include <QMovie>
#include <QElapsedTimer>

#include "appsetting.h"
#include "mainwindow.h"
#include "windowsmanager.h"
#include "uploadthread.h"
#include "global.h"

//
#define LENGTH 2048

//
WinBluetooth::WinBluetooth(QWidget *parent, CBluetoothIntf *_bt_intf) :
    CBaseWidget(parent),
    ui(new Ui::WinBluetooth)
{
    //
    edtName = new myEditLine(this);
    edtName->setVisible(false);
    //QObject::connect(edtName, &myEditLine::textEdited, this, &WinBluetooth::slot_edtName_textEdited, Qt::QueuedConnection);
    edtName->setObjectName("edtName");

    //
    ui->setupUi(this);

    //
    isShowStatusBar = true;

    // 初始化蓝牙通信对象
    btIntf = _bt_intf;
    if (!btIntf) {
        getWinManage()->showSuspensionPrompt(tr("蓝牙通信对象未创建！")); // "Bluetooth Object not Built!"

        //return;
    }

    QString bt_name = getBtNameCfg();
    if (bt_name.length() == 0) {
        bt_name = getBtNameByDevNum();
        //setBtNameCfg(bt_name);            /* 默认的蓝牙名不应保存。否则，之后的设备编号变更，蓝牙名称不会同步变更。 */
    }
    btIntf->setName(bt_name);

    btIntf->setAddrBle(CGlobal::devNum.toLatin1().right(6));        // 2024-05-30 以产品编号最后 6 字节为 BLE MAC 地址

#ifndef TEST_RKWIFIBT
    QObject::connect(btIntf, &CBluetoothIntf::sigSetIsOpenedFinished, this, &WinBluetooth::slot_btIntf_SetIsOpenedFinished, Qt::QueuedConnection);
    QObject::connect(btIntf, &CBluetoothIntf::sigFoundDevice, this, &WinBluetooth::slot_btIntf_FoundDevice, Qt::QueuedConnection);
    QObject::connect(btIntf, &CBluetoothIntf::sigSearchEnd, this, &WinBluetooth::slot_btIntf_SearchEnd, Qt::QueuedConnection);
    QObject::connect(btIntf, &CBluetoothIntf::sigConnStateChanged, this, &WinBluetooth::slot_btIntf_ConnStateChanged, Qt::QueuedConnection);
    QObject::connect(btIntf, &CBluetoothIntf::sigConnTimeout, this, &WinBluetooth::slot_btIntf_ConnTimeout, Qt::QueuedConnection);
    QObject::connect(btIntf, &CBluetoothIntf::sigLog, this, &WinBluetooth::slot_btIntf_Log, Qt::QueuedConnection);
    QObject::connect(btIntf, &CBluetoothIntf::sigNotice, this, &WinBluetooth::slot_btIntf_Notice, Qt::QueuedConnection);
#endif

    // 创建其它对象
    timeLastSearch.start();

    // 初始化界面
    ui->lblBtName->setText(bt_name);                // TODO: 这个应该移到 showEvent() ？
    ui->lblAddr->setText("");

    lblSearching = new CMyLabel(this);
    lblSearching->setGeometry(ui->btnSearch->geometry());
    lblSearching->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    //lblSearching->setFrameShape(QFrame::Box);
    QObject::connect(lblSearching, &CMyLabel::clicked, this, &WinBluetooth::slot_lblSearching_clicked, Qt::QueuedConnection);

    movieSearching = new QMovie(":/resource/loading.gif");
    movieSearching->setScaledSize(QSize(lblSearching->width() - 10, lblSearching->height() - 10));
    lblSearching->setMovie(movieSearching);

    lblConnecting = new CMyLabel(this);
    int lblconn_w = ui->btnConnect->height() - 5;
    int lblconn_x = ui->btnConnect->x() - lblconn_w - 5;
    int lblconn_y = ui->btnConnect->y() + 2;
    lblConnecting->setGeometry(lblconn_x, lblconn_y, lblconn_w, lblconn_w);
    //lblConnecting->setFrameShape(QFrame::Box);

    movieConnecting = new QMovie(":/resource/uploading.gif");
    movieConnecting->setScaledSize(QSize(lblConnecting->width(), lblConnecting->height()));
    lblConnecting->setMovie(movieConnecting);

    updateView_btnSearch(false);
    updateView_btnConnect(btConnState_NotConnect);

    // 自身信号槽连接
    QObject::connect(this, &WinBluetooth::sigSetIsOpened, this, &WinBluetooth::slot_this_SetIsOpened, Qt::QueuedConnection);

    // 搜索结果表格设置
    ui->tblBtDevices->setColumnWidth(0, 400);
    ui->tblBtDevices->setColumnWidth(1, 150);
    ui->tblBtDevices->setColumnWidth(2, 100);
    ui->tblBtDevices->setColumnWidth(3, 100);

    ui->tblBtDevices->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblBtDevices->setSelectionMode(QAbstractItemView::MultiSelection);     //设置只能选择一行，不能多行选中
    ui->tblBtDevices->setEditTriggers(QAbstractItemView::NoEditTriggers);      //设置每行内容不可更改
    ui->tblBtDevices->setFocusPolicy(Qt::NoFocus);
    ui->tblBtDevices->horizontalHeader()->setSectionsClickable(false);         //水平方向的头不可点击
    ui->tblBtDevices->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);    //影藏水平滚动条

    // 初始化蓝牙 Log 界面
    initBtLogUi();

    // 如果上次开机时的蓝牙开关状态时开的，则打开蓝牙          // TODO: 设备控制应放到外部，不应放到本构造函数里，本模块仅作为视图
bool is_open_cfg = false;
#if (2 != BLUETOOTH_TYPE)
    is_open_cfg = getBtStatCfg();
#else
# ifndef TEST_RKWIFIBT
    is_open_cfg = true;
# else
    is_open_cfg = false;
# endif
#endif

    //if (!g_hasBluetooth) {        // TODO: 屏蔽掉“有蓝牙模块”的设置？
    //    is_open_cfg = false;
    //}

    if (is_open_cfg) {
        QTimer::singleShot(1500, this, [this]() {
            this->setIsOpened(true);
        });
    }

    //
    qDebug() << "WinBluetooth() ended";
}

WinBluetooth::~WinBluetooth()
{
    // 关闭蓝牙
    QString msg;
    btIntf->setIsOpened(false, msg);

    //
    delete ui;
}

// 获得“蓝牙开关”配置值
bool WinBluetooth::getBtStatCfg()
{
    return appSetting::value("bluetooth/btState").toBool();
}

// 设置“蓝牙开关”配置值
void WinBluetooth::setBtStatCfg(bool _is_opened)
{
    appSetting::setValue("bluetooth/btState", _is_opened);
}

//
void WinBluetooth::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    // 自动搜索
    //constexpr int MS_SEARCH = 1 * 60 * 1000;
    //
    //if (btIntf->getIsOpened() && btIntf->getIsSearched() && !btIntf->getIsConnected() && (timeLastSearch.elapsed() > MS_SEARCH)) {
    //    doSearch(true);
    //}

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // TODO: rk3568 若已连接，关闭再开再连接会崩溃。完善后按配置开关按钮，且取消按钮的隐藏
#if (2 == BLUETOOTH_TYPE)
    // TODO: rkwifibt 有个 bug：初始化->打开->连接SPP->断开SPP->反初始化，再初始化时失败。但是重开进程，又正常了？
    if (btIntf->getIsOpened() && !CGlobal::isDebugMode) {
        ui->btnIsOpened->setVisible(false);
    }
#endif

    //
    ui->lblBtName->setText(btIntf->getName());
    ui->lblAddr->setText(QString("SPP: %1\nBLE: %2").arg(btIntf->getAddr(), btIntf->getAddrBle()));

    // 界面刷新
    updateLanguage();
    updateTheme();

    //
    ui->edtBtTest->setVisible(CGlobal::isDebugMode);

}

void WinBluetooth::mouseReleaseEvent(QMouseEvent *_e)
{
    static bool is_inited = false;
    static int count = 0;
    static QElapsedTimer last_click;

    if (!is_inited) {
        last_click.start();
        is_inited = true;
    }

    //
    if (last_click.elapsed() > 1000) {
        count = 0;
    }

    //
    bool is_clicked = false;
    if (_e->y() > ui->btnConnect->y() && _e->x() > ui->btnConnect->x() + ui->btnConnect->width() && _e->x() < ui->btnBack->x()) {
        is_clicked = true;
    }

    //
    if (is_clicked) {
        count++;
        last_click.start();
        if (count >= 5) {
            count = 0;
            showBtLogUi();
        }
    }
}

void WinBluetooth::updateLanguage()
{
    // 刷新标题
    getWinManage()->updateWindowTitle(this, tr("蓝牙设置"));    // "Bluetooth Setting"

    //
    //if (language) {
    //    ui->lblBtNameTitle->setText("本机名称:");
    //    ui->label_Home->setText("主页");
    //    ui->label_Search->setText("搜索");
    //    ui->label_Back->setText("返回");
    //    ui->btnEditName->setText("修改");
    //} else {
    //    ui->lblBtNameTitle->setText("Name:");
    //    ui->label_Home->setText("Home");
    //    ui->label_Search->setText("Search");
    //    ui->label_Back->setText("Back");
    //    ui->btnEditName->setText("Edit");
    //}

    // 设置“连接”按钮状态
    updateView_btnConnect(lblConnecting->isVisible() ? btConnState_Connecting : (btIntf->getBtPrinter()->getIsConnected() ? btConnState_Connected : btConnState_NotConnect));

    // 表格
    //QStringList header;
    //header << "设备名称" << "地址" << "连接状态" << "功能类别";   // << "Device" << "Address" << "Connected" << "Type"
    //ui->tblBtDevices->setHorizontalHeaderLabels(header);

    ui->tblBtDevices->setSelectionMode(QAbstractItemView::SingleSelection);

    QTableWidgetItem *item_stat = Q_NULLPTR, *item_type = Q_NULLPTR;
    for (int i = 0; i < ui->tblBtDevices->rowCount(); i++) {
        item_stat = ui->tblBtDevices->item(i, 2);
        item_type = ui->tblBtDevices->item(i, 3);

        bool is_conn = false;
        if (item_stat) {
            is_conn = item_stat->data(Qt::UserRole).toBool();
            QString str_conn = (is_conn ? tr("已连接") : "");  // "Yes"

            item_stat->setText(str_conn);
        }
        if (is_conn && item_type) {
            int dev_type = item_type->data(Qt::UserRole).toInt();
            QString str_type = "(unknown))";
            if (btDevType_Printer == (enBtDevType)dev_type)
                str_type = tr("蓝牙打印");  // "Printer"
            else if (btDevType_Datatrans == (enBtDevType)dev_type)
                str_type = tr("数据传输");  // "Transmission"

            item_type->setText(str_type);
        }
    }

}

void WinBluetooth::updateTheme()
{
    //QPixmap pixmap;
    //QPalette palette;
    if(themeType_Black == getSysThemeType()){
        //pixmap.load(":/resource/black_theme/blackground_color_b.png");

        ui->tblBtDevices->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(50,50,50); color:rgb(204,204,204);}");
        ui->tblBtDevices->setStyleSheet("QTableWidget { background-color: transparent; color:rgb(204,204,204); border: 1px solid black; gridline-color: rgb(40,42,48); }");
        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p, list_Label)
            p->setStyleSheet("color:rgb(204,204,204);");
        ui->btnConnect->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(28,28,30); color:rgb(250,250,252);}");
        ui->btnHome->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->btnSearch->setIcon(QIcon(":/resource/black_theme/find_b.png"));
        ui->btnBack->setIcon(QIcon(":/resource/black_theme/back_b.png")) ;
        ui->btnEditName->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(28,28,30); color:rgb(250,250,252);}");
    }
    else{
        //pixmap.load(":/resource/white_theme/whiteground_color_w.png");

        ui->tblBtDevices->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
        ui->tblBtDevices->setStyleSheet("QTableWidget{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p, list_Label)
            p->setStyleSheet("color:rgb(1,1,1);");
        ui->btnConnect->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(201,201,204); color:rgb(1,1,1);}");
        ui->btnHome->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->btnSearch->setIcon(QIcon(":/resource/white_theme/find_w.png"));
        ui->btnBack->setIcon(QIcon(":/resource/white_theme/back_w.png"));
        ui->btnEditName->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(201,201,204); color:rgb(1,1,1);}");
    }
    //palette.setBrush(backgroundRole(), QBrush(pixmap));
    //setPalette(palette);

    updateView_btnIsOpened(btIntf->getIsOpened());

}

QString WinBluetooth::getBtNameCfg()
{
    QString bt_name = appSetting::value("/global/btName").toString();
    return bt_name;
}

void WinBluetooth::setBtNameCfg(const QString &_name)
{
    appSetting::setValue("/global/btName", _name);
}

QString WinBluetooth::getBtNameByDevNum()
{
    static const QString DEFAULT_BT_NAME = "WLBQ-Screener";

    QString product_num = CGlobal::devNum;

    QString bt_name;
    static const QString OLD_NAME_START = "SL-100-";
    if (product_num.startsWith(OLD_NAME_START) && product_num.length() > OLD_NAME_START.length()) {
        bt_name = product_num.mid(OLD_NAME_START.length());
    } else {
        bt_name = (product_num.length() > 0 ? product_num : DEFAULT_BT_NAME);
    }

    return bt_name;
}

void WinBluetooth::slot_btIntf_FoundDevice(QString _name, QString _addr)
{
    logDebug((QString(__PRETTY_FUNCTION__) + " into ... name = %1, addr = %2").arg(_name).arg(_addr), CGlobal::LOG_BLUETOOTH);

    int row = ui->tblBtDevices->rowCount();
    ui->tblBtDevices->insertRow(row);
    ui->tblBtDevices->setItem(row, 0, new QTableWidgetItem(_name));
    ui->tblBtDevices->setItem(row, 1, new QTableWidgetItem(_addr));
}

void WinBluetooth::slot_btIntf_SearchEnd()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    // “搜索” 按钮状态
    updateView_btnSearch(false);

    // 自动连接上次连接的蓝牙设备
    QTimer::singleShot(5000, this, [this] () {
        this->connectLastConnAddr();
    });

}

QString WinBluetooth::getLastConnAddr()
{
    return appSetting::value("bluetooth/lastConnAddr").toString();
}

void WinBluetooth::setLastConnAddr(QString _addr)
{
    appSetting::setValue("bluetooth/lastConnAddr", _addr);
}

// 自动连接上次连接的蓝牙设备
void WinBluetooth::connectLastConnAddr()
{
    qDebug() << "WinBluetooth::connectLastConnAddr() into ...";

    // 若已连接，或用户已点“连接”，则退出
    if (btIntf->getIsConnected() || (lblConnecting->isVisible())) {
        return;
    }

    //
    QString last_conn_addr = getLastConnAddr();
    if (last_conn_addr.length() > 0) {
        bool found = false;
        QTableWidgetItem *item_addr;
        for (int i = 0; i < ui->tblBtDevices->rowCount(); i++) {
            item_addr = ui->tblBtDevices->item(i, 1);
            if (item_addr->text() == last_conn_addr) {
                found = true;
                break;
            }
        }

        if (found) {
            //logWarning(QString("WinBluetooth::connectLastConnAddr(): connecting to ") + last_conn_addr, CGlobal::LOG_BLUETOOTH);
            qDebug() << QString("WinBluetooth::connectLastConnAddr(): connecting to ") + last_conn_addr;

            bool succ = connectDeviceByAddr(last_conn_addr);
            if (!succ) {
                //logWarning("WinBluetooth::connectLastConnAddr(): auto connect failed!", CGlobal::LOG_BLUETOOTH);
                qDebug() << "WinBluetooth::connectLastConnAddr(): auto connect failed!";

                //getWinManage()->showSuspensionPrompt(QString("auto connect to ") + last_conn_addr + "failed!");
            }
        } else {
            qDebug() << QString("addr %1 not found!").arg(last_conn_addr);
        }
    }
}

void WinBluetooth::slot_btIntf_ConnStateChanged(bool _connected, int _conn_id, QString _addr, QString _name, enBtDevType _dev_type)
{
    Q_UNUSED(_conn_id)

    logDebug((QString(__PRETTY_FUNCTION__) + " into ... _connected = %1, _addr = %2").arg(Util::bool2str(_connected)).arg(_addr), CGlobal::LOG_BLUETOOTH);

    // 在表格中查找 address 对应行，然后修改信息
    bool is_addr_found = false;

    QString str_stat = _connected ? tr("已连接") : ""; // "Yes"
    QString str_type = "";
    if (_connected) {
        if (btDevType_Printer == _dev_type)
            str_type = tr("蓝牙打印");  // "Printer"
        else if (btDevType_Datatrans == _dev_type)
            str_type = tr("数据传输");  // "Transmission"
    }

    //if (_addr.length() > 0)
    {
        QTableWidgetItem *item_addr, *item_stat, *item_type;
        for (int i = 0; i < ui->tblBtDevices->rowCount(); i++) {
            item_addr = ui->tblBtDevices->item(i, 1);
            if (item_addr && item_addr->text() == _addr) {     // 若找到，则修改状态信息
                is_addr_found = true;

                //
                item_stat = ui->tblBtDevices->item(i, 2);
                if (!item_stat) {
                    item_stat = new QTableWidgetItem("");
                    item_stat->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    ui->tblBtDevices->setItem(i, 2, item_stat);
                }

                item_type = ui->tblBtDevices->item(i, 3);
                if (!item_type) {
                    item_type = new QTableWidgetItem("");
                    item_type->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
                    ui->tblBtDevices->setItem(i, 3, item_type);
                }

                item_stat->setText(str_stat);
                item_stat->setData(Qt::UserRole, (_connected ? 1 : 0));
                item_type->setText(str_type);
                item_type->setData(Qt::UserRole, (int)_dev_type);

                break;
            }
        }
    }
    //else
    //{
    //     logWarning();
    //}

    // 若找不到且是连接状态，则添加一行
    if (!is_addr_found) {
        if (_connected) {
            int row = -1;
            for (int i = 0; i < ui->tblBtDevices->rowCount(); i++) {
                QTableWidgetItem *item_addr = ui->tblBtDevices->item(i, 1);
                if (!item_addr || item_addr->text().length() == 0) {
                    row = i;
                    break;
                }
            }
            if (row < 0) {
                row = ui->tblBtDevices->rowCount();
                ui->tblBtDevices->insertRow(row);
            }

            QTableWidgetItem *item_name = new QTableWidgetItem(_name.length() > 0 ? _name : tr("（未知）"));    // "(Unknown)"
            ui->tblBtDevices->setItem(row, 0, item_name);

            QTableWidgetItem *item_addr = new QTableWidgetItem(_addr);
            ui->tblBtDevices->setItem(row, 1, item_addr);

            QTableWidgetItem *item_connected = new QTableWidgetItem(str_stat);
            item_connected->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            ui->tblBtDevices->setItem(row, 2, item_connected);

            QTableWidgetItem *item_function = new QTableWidgetItem(str_type);
            item_function->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            ui->tblBtDevices->setItem(row, 3, item_function);
        } else {
            logWarning("WinBluetooth::slot_btIntf_ConnStateChanged: disconnected address not found in table!", CGlobal::LOG_BLUETOOTH);
        }
    }

    // 清空所有行的“连接状态”和“功能类别”
    //if (!_connected) {
    //    for (int i = ui->tblBtDevices->rowCount() - 1; i > 0; i++) {
    //        ui->tblBtDevices->item(i, 2)->setText("");
    //        ui->tblBtDevices->item(i, 3)->setText("");
    //    }
    //}

    // 保存最后手动连接的 address 到配置文件
    if (_connected && (_addr == addrConnecting)) {
        setLastConnAddr(_addr);
    }

    // “连接”按钮状态
    if (btDevType_Printer == _dev_type)
    {
        enBtConnState conn_stat = (_connected ? btConnState_Connected : btConnState_NotConnect);
        updateView_btnConnect(conn_stat);
    }

    // 使“连接”按钮有效
#if (1 == BLUETOOTH_TYPE)
    ui->btnConnect->setEnabled(!_connected);
#else
    ui->btnConnect->setEnabled(true);
#endif

    //
    WgtStatusBar::instance()->setIsBtConnected(_connected);
}

void WinBluetooth::slot_btIntf_ConnTimeout()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    if (this->isVisible()) {
        getWinManage()->showSuspensionPrompt("Connection timeout!");
    }

    updateView_btnConnect(btConnState_NotConnect);
}

void WinBluetooth::slot_btIntf_Log(QString _txt)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    appendBtLog(_txt);
}

void WinBluetooth::slot_btIntf_Notice(QString _msg)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ... msg : " + _msg, CGlobal::LOG_BLUETOOTH);

    if (this->isVisible()) {
        getWinManage()->showSuspensionPrompt(_msg);
    }
}

// “开关蓝牙” 按钮点击事件
void WinBluetooth::on_btnIsOpened_clicked()
{
    // 暂时禁用按钮
    ui->btnIsOpened->setEnabled(false);   // TODO: 如果要失效按钮，这里应显示缓冲图像，否则容易导致用户以为按钮失效？

    // 用信号槽方式使本函数及时返回，刷新界面，因为后面的过程耗时较长
    bool is_open = !btIntf->getIsOpened();
    emit sigSetIsOpened(is_open);

    // 使“连接”按钮的无效
    ui->btnConnect->setEnabled(false);

    // JDY-34 关闭蓝牙时删掉自动连接的地址
#if (1 == BLUETOOTH_TYPE)
    setLastConnAddr("");
#endif

}

// 槽函数：切换蓝牙电源状态
void WinBluetooth::slot_this_SetIsOpened(bool _is_open)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ... setting bt stat to : " + Util::bool2str(_is_open), CGlobal::LOG_BLUETOOTH);

    //
    bool succ = setIsOpened(_is_open);
    bool is_open = (succ ? _is_open : !_is_open);

    // 打开或关闭之后的操作
    if (!is_open) {
        // 清空设备搜索结果表
        ui->tblBtDevices->clearContents();
        ui->tblBtDevices->setRowCount(0);

        // 重置搜索按钮状态
        updateView_btnSearch(false);
        updateView_btnConnect(btConnState_NotConnect);
    }

    // 同步蓝牙电源开关状态
    updateView_btnIsOpened(is_open);        // TODO: 应该在接收到蓝牙接口的开关状态改变事件后更新

    // 禁用搜索按钮（不管是打开还是关闭，都先禁用）
    ui->btnSearch->setEnabled(false);

    // 启用按钮（因为按下按钮时禁用了）
    ui->btnIsOpened->setEnabled(true);
}

// 设置底板的蓝牙电源及本程序的蓝牙模块状态
bool WinBluetooth::setIsOpened(bool _is_open)
{
    logDebug(QString("WinBluetooth::setIsOpened(): _is_opened = ") + Util::bool2str(_is_open), CGlobal::LOG_BLUETOOTH);
    qDebug() << "WinBluetooth::setIsOpened(): _is_opened = " << _is_open;

    // 若是串口蓝牙，底板加电指令
#if (1 == OS_TYPE || 2 == OS_TYPE)
    MySerialPort::instance()->write(_is_open ? OpenBT : CloseBT);
#endif

    // 同步改变蓝牙模块状态
    QString msg;
    bool is_succ = btIntf->setIsOpened(_is_open, msg);
    if (!is_succ) {
        getWinManage()->showSuspensionPrompt(msg);

        //
        return false;
    }

    //
    return true;
}

// 设置蓝牙“开关”按钮状态
void WinBluetooth::updateView_btnIsOpened(bool _is_open, bool _force)
{
    qDebug() << "setting bt button stat to " << Util::bool2str(_is_open);

    //
    setBtStatCfg(_is_open);

    //
    if (_is_open) {
        // 设置按钮图像
        if (themeType_Black == getSysThemeType())
            ui->btnIsOpened->setIcon(QIcon(":/resource/black_theme/switch-on_b.png"));
        else
            ui->btnIsOpened->setIcon(QIcon(":/resource/white_theme/switch-on_w.png"));
    } else {
        // 设置按钮图像
        if (themeType_Black == getSysThemeType())
            ui->btnIsOpened->setIcon(QIcon(":/resource/black_theme/switch-off_b.png"));
        else
            ui->btnIsOpened->setIcon(QIcon(":/resource/white_theme/switch-off_w.png"));
    }
    ui->btnIsOpened->update();
    qApp->processEvents(QEventLoop::AllEvents);
}

// 设置“搜索”按钮状态
void WinBluetooth::updateView_btnSearch(bool _is_searching)
{
    ui->btnSearch->setVisible(!_is_searching);
    lblSearching->setVisible(_is_searching);

    if (_is_searching)
        movieSearching->start();
    else
        movieSearching->stop();
}

// 设置“连接”按钮状态
void WinBluetooth::updateView_btnConnect(enBtConnState _conn_state)
{
    bool is_connecting = (btConnState_Connecting == _conn_state);
    lblConnecting->setVisible(is_connecting);
    if (is_connecting) {
        movieConnecting->start();
    } else {
        movieConnecting->stop();
    }

    //
#if (1 == BLUETOOTH_TYPE)
    ui->btnConnect->setText(tr("连接"));  // "Connect"
#else
    bool is_connected = (btConnState_Connected == _conn_state);
    if (is_connected) {
        ui->btnConnect->setText(tr("断开"));  // "Disconnect"
    } else {
        ui->btnConnect->setText(tr("连接"));  // "Connect"
    }
#endif
}

//搜索蓝牙
void WinBluetooth::on_btnSearch_clicked()
{
    doSearch();
}

//搜索蓝牙
void WinBluetooth::doSearch(bool _is_silent)
{
    logDebug("WinBluetooth::doSearch() into ...", CGlobal::LOG_BLUETOOTH);

    // TODO: 状态检查？

    // 检查：正在连接时禁止搜索
    if (!lblConnecting->isHidden()) {
        if (!_is_silent) {
            getWinManage()->showSuspensionPrompt(tr("连接中，请稍后...")); // "Connecting, please wait..."
        }

        return;
    }

    // 若已连接，禁止再搜索
    if (btIntf->getBtPrinter()->getIsConnected()) {
        //logDebug("WinBluetooth::doSearch(): is connected, prohibit search!", CGlobal::LOG_BLUETOOTH);
        if (!_is_silent) {
            getWinManage()->showSuspensionPrompt(tr("蓝牙已连接，若要搜索，请先断开"));    // "bluetooth connected, to search, \nplease disconnect first"
        }
        return;
    }

    // 若未打开，禁止搜索
    if (!btIntf->getIsOpened()) {
        //logDebug("WinBluetooth::doSearch(): not opened, prohibit search!", CGlobal::LOG_BLUETOOTH);
        if (!_is_silent) {
            getWinManage()->showSuspensionPrompt(tr("蓝牙未打开"));  // "Bluetooth not turned on"
        }
        return;
    }

    //
    QString msg;
    if (btIntf->searchDevices(msg)) {
        doBeforeSearch();
    } else {
        if (!_is_silent) {
            getWinManage()->showSuspensionPrompt(msg);
        }

        updateView_btnSearch(false);
    }

    logDebug("WinBluetooth::doSearch() ended", CGlobal::LOG_BLUETOOTH);
}

void WinBluetooth::doBeforeSearch()
{
    // 清空设备搜索结果表
    ui->tblBtDevices->clearContents();
    ui->tblBtDevices->setRowCount(0);

    // 设置“搜索”按钮状态为可用
    updateView_btnSearch(true);

    //
    timeLastSearch.start();
}

//
void WinBluetooth::on_btnConnect_clicked()
{
    bool is_need_conn = (!btIntf->getBtPrinter()->getIsConnected());
    setConnectState(is_need_conn, true);
}

//连接蓝牙
bool WinBluetooth::setConnectState(bool _is_need_connect, bool _is_by_user)
{
    qDebug() << "WinBluetooth::setConnectState() into, conn_stat = " << Util::bool2str(_is_need_connect);

    bool succ = false;

    // 检查：正在搜索时禁止连接或断开
    if (lblSearching->isVisible()) {
        getWinManage()->showSuspensionPrompt(tr("搜索中，请稍后...")); // "Searching, please wait..."

        return succ;
    }

    // 检查：正在连接时禁止连接或断开
    if (!lblConnecting->isHidden()) {
        getWinManage()->showSuspensionPrompt(tr("连接中，请稍后...")); // "Searching, please wait..."

        return succ;
    }

    // 执行连接
    if (_is_need_connect) {
        // 检查：须选择行
        QList<QTableWidgetSelectionRange> sel = ui->tblBtDevices->selectedRanges();
        if (sel.size() == 0) {
            getWinManage()->showSuspensionPrompt(tr("未选定行"));   // "no row selected"

            return succ;
        }

        // 检查：选择行须有 address
        int row_sel = sel[0].topRow();
        QString addr = ui->tblBtDevices->item(row_sel, 1)->text();
        if (addr.length() == 0) {
            getWinManage()->showSuspensionPrompt(tr("数据错误：选定行没有地址。"));  // "Data error: selected row has no address."

            return succ;
        }
        qDebug() << "on_btnConnect_clicked(): connecting, addr = " << addr;

        // 更新按钮状态
        updateView_btnConnect(btConnState_Connecting);

        // 暂时禁用“断开”按钮
        //ui->btnConnect->setEnabled(false);      // TODO: 如果 连接/断开 失败，怎么恢复？

        // 连接
        succ = connectDeviceByAddr(addr);

        // 记录正在手动连接的 addr
        if (succ) {
            addrConnecting = (succ ? addr : "");
        }
    } else {
        // 暂时禁用“断开”按钮
        //ui->btnConnect->setEnabled(false);      // TODO: 如果 连接/断开 失败，怎么恢复？

        // 断开
        succ = btIntf->disconnectBt();

        // 若是手动关闭蓝牙，把自动连接的 address 清掉
        if (_is_by_user) {
            setLastConnAddr("");
        }
    }

    //
    return succ;
}

// 连接蓝牙设备 by address
bool WinBluetooth::connectDeviceByAddr(QString _addr)
{
    QString msg;
    bool succ_cmd = btIntf->connectDevice(_addr, msg);
    if (succ_cmd) {
        // 连接缓冲动画
        updateView_btnConnect(btConnState_Connecting);
    } else {
        updateView_btnConnect(btIntf->getBtPrinter()->getIsConnected() ? btConnState_Connected : btConnState_NotConnect);
        if (this->isVisible()) {
            getWinManage()->showSuspensionPrompt(msg);
        }
    }

    return succ_cmd;
}

//返回主页
void WinBluetooth::on_btnHome_clicked()
{
    getWinManage()->showWindowByType(WIN_HOME);
}

//返回
void WinBluetooth::on_btnBack_clicked()
{
    getWinManage()->showWindowByType(WIN_TOOL);
}

void WinBluetooth::on_edtBtTest_returnPressed()
{
    QByteArray data = ui->edtBtTest->text().toLocal8Bit();

    logDebug(QString("WinBluetooth::on_edtBtTest_returnPressed() into ... data=\"") + data + "\"", CGlobal::LOG_BLUETOOTH);

    // 转义字符解析
    Util::parseEscapeChar(data);

    //
    CBtConnection * conn = btIntf->getBtDatatrans();
    if (!conn->getIsConnected()) {
        conn = btIntf->getBtPrinter();
    }
    if (!conn->getIsConnected()) {
        conn = Q_NULLPTR;
    }
    if (conn) {
        conn->pushSendingData(data + "\r\n");
    }

    logDebug("WinBluetooth::on_edtBtTest_returnPressed() ended", CGlobal::LOG_BLUETOOTH);

}

// 停止搜索
void WinBluetooth::stopSearch(bool _is_silent)
{
    logWarning("WinBluetooth::stopSearch(): timeout to stop searching", CGlobal::LOG_BLUETOOTH);

    //
    QString msg;
    bool succ = btIntf->stopSearching(msg);
    if (succ) {
        updateView_btnSearch(false);
    } else {
        if (!_is_silent) {
            getWinManage()->showSuspensionPrompt(msg);
        } else {
            updateView_btnSearch(false);
        }
    }
}

// 搜索缓冲按钮点击事件
void WinBluetooth::slot_lblSearching_clicked()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    stopSearch();
}

void WinBluetooth::slot_btIntf_SetIsOpenedFinished(bool _is_open)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    ui->btnSearch->setEnabled(_is_open);

    if (_is_open) {
        ui->lblAddr->setText(QString("SPP: %1\nBLE: %2").arg(btIntf->getAddr(), btIntf->getAddrBle()));

        // 自动搜索
        //Util::waitMs(1000);
        //doSearch(true);
    }

    //
    WgtStatusBar::instance()->setIsBtOpened(_is_open);
}

//
void WinBluetooth::on_btnEditName_clicked()
{
#if (1 == BLUETOOTH_TYPE)
    if (!btIntf->getIsOpened()) {
        getWinManage()->showSuspensionPrompt(tr("请先打开蓝牙")); // "please open blue tooth first"
        return;
    }
#endif

#if (1 == BLUETOOTH_TYPE)
    if (!btIntf->getIsConnected())
#endif
    {
        QString bt_name = ui->lblBtName->text();
        edtName->setText(bt_name);
        getWinManage()->showKeyboard(edtName, NULL);
    }
#if (1 == BLUETOOTH_TYPE)
    else
    {
        getWinManage()->showSuspensionPrompt(tr("蓝牙已连接，请先断开")); // "BlueTooth is connected, please disconnect it first"
    }
#endif

}

void WinBluetooth::on_edtName_textEdited(const QString &_text)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    resetBtName(_text);
}

void WinBluetooth::resetBtName(const QString &_str)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    setBtNameCfg(_str);

    ui->lblBtName->setText(_str);

    btIntf->setName(_str);

#if (1 == BLUETOOTH_TYPE)
    getWinManage()->showSuspensionPrompt(tr("请重新打开蓝牙"));    // "please reopen bluetooth"
#endif

}

// 初始化蓝牙 Log 界面
void WinBluetooth::initBtLogUi()
{
    hideBtLogUi();

    //
    ui->frmBtLog->setGeometry(5, 30, this->width() - 10, this->height() - 30 - 5);
    ui->txtBtLog->setGeometry(5, 5, ui->frmBtLog->width() - 10, ui->frmBtLog->height() - 10);
    ui->btnBtLogClear->setGeometry(ui->frmBtLog->width() - ui->btnBtLogClear->width() - 20, 10, ui->btnBtLogClear->width(), ui->btnBtLogClear->height());
    ui->btnBtLogBack->setGeometry(ui->frmBtLog->width() - ui->btnBtLogBack->width() - 20, ui->frmBtLog->height() - ui->btnBtLogBack->height() - 10, ui->btnBtLogBack->width(), ui->btnBtLogBack->height());

}

// 显示蓝牙 Log 界面
void WinBluetooth::showBtLogUi()
{
    ui->frmBtLog->setVisible(true);
}

// 隐藏蓝牙 Log 界面
void WinBluetooth::hideBtLogUi()
{
    ui->frmBtLog->setVisible(false);
}

// 添加 Bt Log
void WinBluetooth::appendBtLog(QString &_txt)
{
    // 限制文本框中的最大字符数
    const int MAX_CHARS = 1024 * 10;
    QString text = ui->txtBtLog->toPlainText();
    if (text.length() > MAX_CHARS) {
        ui->txtBtLog->clear();              // TODO: 从前往后删除行，直到字数小于最大值
    }

    //
    ui->txtBtLog->appendPlainText(_txt);
}

//
void WinBluetooth::on_btnBtLogClear_clicked()
{
    ui->txtBtLog->clear();
}

void WinBluetooth::on_btnBtLogBack_clicked()
{
    hideBtLogUi();
}

#if (1 == OS_TYPE || 2 == OS_TYPE)
// 底板蓝牙连接状态变化事件处理
void WinBluetooth::slotBaseBoardBtConnStateChanged(int _num)
{
    if (29 == _num || 30 == _num)
        logDebug(QString::asprintf("bluetoothWin::slotReceivedBtPowerState() called, Num=%d ========================", _num), CGlobal::LOG_BLUETOOTH);

    if(_num == 29 && btIntf->getIsOpened())        // 【蓝牙连接已断开】消息
    {
        // TODO: 这个好像没用到？
    }
    else if(_num == 30 && !btIntf->getIsOpened())  // 【蓝牙连接已建立】消息
    {
        // TODO: 这个好像没用到？
    }
}
#endif

