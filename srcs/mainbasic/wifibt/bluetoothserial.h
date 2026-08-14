#ifndef BLUETOOTHSERIAL_H
#define BLUETOOTHSERIAL_H

#include <QObject>
#include <QSerialPort>
#include <QMutex>
#include <QTimer>
#include <QElapsedTimer>

#include "bluetoothintf.h"
#include "cserialport.h"

// 串口蓝牙访问封装（蓝牙串口透传模块 JDY-34 设计开发。Henry 2021-09-20）
/***
 * 多重连接的处理比较麻烦，主要思路如下：
 * 1、建立连接时：
 *      (1) 所有能正常使用的连接都是“预建连接”。目前预建连接只有两个：一是主动连接的打印机连接，二是被动连接的数据通信连接。
 *      (2) 须防止后建立的同样功能的连接覆盖了前面的连接。功能类别相同的多个连接，目前只会在被动连接的数据通信连接中出现。
 *          但是 JDY-34 模块无法防止被多个设备连接，目前又无 PWRC 脚，无法 DISC。设置密码？实测 JDY-34 设置密码无效。
 *          好像只能设置 sendid 这是
 * 2、收发数据时：
 *      (1) 发数据时，通过设置 sendid 来发送给不同的连接。
 *      (2) 收数据时，目前只有数据通信连接会收到数据，所以如果收到的串口数据不是AT消息，那么肯定就是来自于数据通信连接。
 */
class CBluetoothSerial : public CBluetoothIntf
{
    Q_OBJECT

    friend class CBluetoothIntf;

public:
    ~CBluetoothSerial();

    int getDelayMs() override;

    bool setName(const QString &_name) override;
    QString getName() override;

    bool setIsOpened(bool _is_open, QString &_msg) override;
    bool searchDevices(QString &_msg) override;
    bool connectDevice(QString _addr, QString &_msg) override;
    bool stopSearching(QString &_msg) override;
    bool sendBtData(int _conn_id, QByteArray _data) override;

    bool disconnectBt(QString _addr = "") override;

    bool getIsOpened() override;
    bool getIsSearched() override;
    bool getIsConnected() override;
    QString getAddr() override;
    QString getAddrBle() override { return ""; }

protected:
    explicit CBluetoothSerial(QObject *parent = 0);

    const int INTERVAL_CMD_MS = 100;    // 常量：一般指令间隔（毫秒）

    CSerialPort *serialPort = Q_NULLPTR;
    QThread *workThread = Q_NULLPTR;

    QString name = "WLBQ-Screener";
    QString sppSuffix = "SPP";
    QString bleSuffix = "BLE";

    QString addrSelf = "";               // 自身 MAC address 地址
    bool isOpened = false;              // 本模块是否已启动     // TODO: 状态细化（关闭、正在启动、空闲、正在搜索……）？另外增加工作状态变量（正在启动、正在搜索……）？
    bool isInited = false;              // 已初始化

    bool isStartOK = false;             // 蓝牙模块“StartOK”消息
    bool isSearching = false;           // 正在搜索
    bool isSearched = false;            // 执行过启动后的第一次搜索
    bool isConnecting = false;          // 正在搜索

    bool hasStatPin = false;            // 硬件是否支持 STAT 引脚
    bool hasPwrcPin = false;            // 硬件是否支持 PWRC 引脚

    QMutex *writeLocker;
    QByteArray buffer;

    // 会话期间临时变量
    bool isReceivedAtResp = false;      // 收到 AT 指令应答
    QString addrConnecting = "";        // 正在连接的设备 MAC address（申请连接时记录，连接完成后清掉）
    int isReceivedAtOk   = 0;           // 收到了AT消息“+OK”

    QTimer *timerSearchTimeLimit;       // 搜索超时定时器。防止由于异常没得到搜索结束消息而导致界面一直处于等待状态

    QElapsedTimer timeLastOpen;         // 上次开启蓝牙的时间。用于限制开关的时间间隔。

    //
    int mtu = 1;
    int btMode = 1;
    int sendId = -1;
    QVector<stBtDevInfo> devsFound;     // 已搜索到的设备（用于连接建立时由 mac address 查找 name，所以这里都是 SPP 设备）
    QVector<CBtConnection *> conns;     // 已建立的连接

    QTimer *timerCheckStartOk;          // 定时检查是否收到 StartOK 消息
    QTimer *timerConnectingMacClear;    // 定时检查正在连接的蓝牙 mac address 是否被清除，否则清除

    //
    bool initConfig();

    bool processAtMsg();
    bool processBtData();

    //bool writeAtCmd(QString _cmd);
    bool writeAtCmd(QString _cmd, QString &_msg);

    bool setPwrcPinVal(int _val);
    int getPwrcPinVal();
    int getStatPinVal();

    bool hasDisconnectedMsg(QByteArray &_data);
    bool hasConnectedMsg(QByteArray &_data);

    QString getAtCmdSetNameSpp(QString _name);
    QString getAtCmdSetNameBle(QString _name);
    QString getAtCmdSetMtu(int _val);
    QString getAtCmdNameSpp();
    QString getAtCmdNameBle();
    QString getAtCmdAddr();
    QString getAtCmdSearch();
    QString getAtCmdStopSearching();
    QString getAtCmdReset();
    QString getAtCmdConnect(QString _addr);
    QString getAtCmdSetSendId(int _id);

    QString getNameSpp();
    QString getNameBle();

    bool processAtMsgLine(QByteArray _msg_line);
    void processConnected(int _conn_id, QString _addr, enBtProtocol _bt_protocol);
    void processDisconnected(int _conn_id, enBtProtocol _bt_protocol);
    CBtConnection *findConn(int _conn_id);

    bool setIsOpenedCheck(bool _is_open, QString &_msg);
    bool getIsSearching();

    bool getDevInfo(int _conn_id, stBtDevInfo &_dev_info);
    bool writeSerialData(QByteArray _data, bool _is_cmd, int _conn_id, int _wait_write = 0);    // 写串口数据

    void doOnStartOk();
    void doOnReceivedAtMsg(QByteArray _msg_line);

private slots:
    void slot_serialPort_DataReceived(QByteArray _data);
    void slot_timerCheckStartOk_timeout();
    void slot_timerSearchTimeLimit_timeout();
    void slot_timerConnectingMacClear_timeout();

};

#endif // BLUETOOTHSERIAL_H
