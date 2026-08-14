#ifndef WINBLUETOOTH_H
#define WINBLUETOOTH_H

#include "bluetoothintf.h"

#include <QWidget>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMutex>
#include <QTimer>
#include <QTimerEvent>
#include <QElapsedTimer>

#include "baseform.h"
#include "statusbarform.h"
#include "myeditline.h"
#include "mylabel.h"

namespace Ui {
class WinBluetooth;
}

// 蓝牙视图
class WinBluetooth : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinBluetooth(QWidget *parent, CBluetoothIntf *_bt_intf);
    ~WinBluetooth();

    void updateLanguage();

    static bool getBtStatCfg();                     // 获得“蓝牙开关”配置值
    static void setBtStatCfg(bool _is_opened);      // 设置“蓝牙开关”配置值

public slots:

#if (1 == OS_TYPE || 2 == OS_TYPE)
void slotBaseBoardBtConnStateChanged(int _num);     // 底板蓝牙连接状态变化事件处理
#endif

private slots:
    void on_btnHome_clicked();
    void on_btnBack_clicked();
    void on_btnSearch_clicked();
    void on_btnIsOpened_clicked();
    void on_btnConnect_clicked();
    void on_edtBtTest_returnPressed();
    void on_btnEditName_clicked();
    void on_btnBtLogClear_clicked();
    void on_btnBtLogBack_clicked();
    void on_edtName_textEdited(const QString &_text);

    void slot_lblSearching_clicked();

    void slot_btIntf_SetIsOpenedFinished(bool _is_open);
    void slot_btIntf_FoundDevice(QString _name, QString _addr);                             // 发现设备
    void slot_btIntf_SearchEnd();                                                           // 搜索结束
    void slot_btIntf_ConnStateChanged(bool _connected, int _conn_id, QString _addr, QString _name, enBtDevType _dev_type);      // 蓝牙连接状态改变
    void slot_btIntf_ConnTimeout();                                                         // 连接超时
    void slot_btIntf_Log(QString _txt);
    void slot_btIntf_Notice(QString _msg);

signals:
    void sigSetIsOpened(bool _is_open);        // 切换蓝牙电源状态

protected:
    CMyLabel *lblSearching;
    QMovie *movieSearching;
    CMyLabel *lblConnecting;
    QMovie *movieConnecting;

    CBluetoothIntf *btIntf;

    QElapsedTimer timeLastSearch;           // 上次搜索的时间。进入界面时，若上次搜索时间过去比较久，自动重新搜索

    QString addrConnecting = "";            // 正在手动连接的 address

    myEditLine *edtName = Q_NULLPTR;

    //
    void showEvent(QShowEvent *event);
    void mouseReleaseEvent(QMouseEvent *_e);

    //
    QString getBtNameByDevNum();                // 获得由设备编号构造的蓝牙名称

    QString getBtNameCfg();                     // 获得蓝牙名称配置
    void setBtNameCfg(const QString &_name);    // 设置蓝牙名称配置

    bool setIsOpened(bool _is_open);

    void doSearch(bool _is_silent = false);
    void doBeforeSearch();
    void stopSearch(bool _is_silent = false);

    bool connectDeviceByAddr(QString _addr);

    void updateView_btnIsOpened(bool _is_open, bool _force = false);
    void updateView_btnSearch(bool _is_searching);
    void updateView_btnConnect(enBtConnState _conn_state);

    void updateTheme();

    void initBtLogUi();
    void showBtLogUi();
    void hideBtLogUi();
    void appendBtLog(QString &_txt);

    bool setConnectState(bool _is_need_connect, bool _is_by_user = false);
    void connectLastConnAddr();

    QString getLastConnAddr();
    void setLastConnAddr(QString _addr);

    void resetBtName(const QString &_str);

protected slots:
    void slot_this_SetIsOpened(bool _is_open);

private:
    Ui::WinBluetooth *ui;
};
//end

#endif // WINBLUETOOTH_H
