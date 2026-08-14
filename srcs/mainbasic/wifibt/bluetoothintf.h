#ifndef BLUETOOTHINTF_H
#define BLUETOOTHINTF_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVector>

//
class CBluetoothIntf;

// 蓝牙协议类型
enum enBtProtocol {
    btProtocol_Unknown      = 0,
    btProtocol_SPP          = 1,
    btProtocol_BleServer    = 2,    // 本端作为 BLE Server
    btProtocol_BleClient    = 3,    // 本端作为 BLE Client
};

// 蓝牙设备类型
enum enBtDevType {
    btDevType_Unknown   = 0,
    btDevType_Printer   = 1,        // 打印机
    btDevType_Datatrans = 2,        // 数据通信设备
};

// 蓝牙设备信息
struct stBtDevInfo {
    enBtProtocol protocol;          // 蓝牙协议
    QString addr;                   // 地址
    QString name;                   // 设备名
    /* RK BT 模块所需成员 */
    unsigned int btClass;
    int rssi;
    int change;     // TODO: 这是？
};

// 连接状态
enum enBtConnState {
    btConnState_NotConnect,
    btConnState_Connecting,
    btConnState_Connected,
};

// 预声明
class CBtConnection;

// 蓝牙接口                 /* 接口要尽量简洁 */
/**
 * 蓝牙接口：
 * 1、打开/关闭：
 *      call        setIsOpened(bool)
 *      callback    setIsOpenedFinished(bool)
 * 2、搜索：
 *      call        searchDevices()
 *      callback    searchBegun(), searchEnded()
 * 3、连接/断开：
 *      call        connDev()/disconnDev()
 *      callback    connStateChanged(bool)
 */
class CBluetoothIntf : public QObject
{
    Q_OBJECT

    friend class CBtConnection;

public:
    static CBluetoothIntf *getInstance();
    ~CBluetoothIntf();

    //
    virtual int getDelayMs() = 0;

    virtual bool setName(const QString &_name) = 0;
    virtual QString getName() = 0;

    virtual bool setIsOpened(bool _is_open, QString &_msg) = 0;
    virtual bool searchDevices(QString &_msg) = 0;
    virtual bool connectDevice(QString _addr, QString &_msg) = 0;
    virtual bool stopSearching(QString &_msg) = 0;

    virtual bool disconnectBt(QString _addr = "") = 0;

    virtual bool getIsOpened() = 0;
    virtual bool getIsSearched() = 0;
    virtual bool getIsConnected() = 0;
    virtual QString getAddr() = 0;
    virtual QString getAddrBle() = 0;
    virtual void setAddrBle(QByteArray _addr_ble) { Q_UNUSED(_addr_ble) }

    /* 预先内建必需要有的蓝牙连接 */
    CBtConnection *getBtPrinter();
    CBtConnection *getBtDatatrans();

protected:
    explicit CBluetoothIntf(QObject *parent = 0);
    static CBluetoothIntf *instance;

    QVector<CBtConnection *> conns;     // 已建立的连接

    CBtConnection *btPrinter = Q_NULLPTR;
    CBtConnection *btDatatrans = Q_NULLPTR;

    QByteArray m_addrBle;

    virtual bool sendBtData(int _conn_id, QByteArray _data) = 0;

signals:
    // （公有）
    void sigSetIsOpenedFinished(bool _is_open);                                     // 【打开/关闭】操作已完成
    void sigFoundDevice(QString _name, QString _addr);                              // 发现设备
    void sigSearchEnd();                                                            // 搜索结束
    void sigConnStateChanged(bool _connected, int _conn_id, QString _addr, QString _name, enBtDevType _dev_type);       // 蓝牙连接状态改变
    void sigConnTimeout();                                                          // 连接超时
    void sigLog(QString _txt);                                                      // 输出 log
    void sigNotice(QString _msg);                                                   // 消息通知

};

/// ============================================================================
/// CBtConnection
/// ============================================================================

// 蓝牙连接
class CBtConnection : public QObject
{
    Q_OBJECT

    friend class CBluetoothIntf;
#if (1 == BLUETOOTH_TYPE)
    friend class CBluetoothSerial;
#elif (2 == BLUETOOTH_TYPE)
    friend class CBluetoothRk;
#else
;xxx; // TODO:
#endif

public:
    explicit CBtConnection(CBluetoothIntf *_parent);
    ~CBtConnection();

    bool getIsConnected();
    void setIsConnected(bool _is_connected);

    void pushReceivedData(const QByteArray &_data);         // 推送接收到的数据（由具体的蓝牙模块调用）
    void pushSendingData(const QByteArray &_data);          // 推送正在发送的数据（由业务模块调用）

signals:
    void sigStateChanged(bool _connected);                          // 状态改变（是否连接）
    void sigReceivedData(QByteArray _data);                         // 收到蓝牙数据

    /* 私有信号 */
    void sigSendingData(QByteArray _data);

protected:

    CBluetoothIntf *bluetooth;
    enBtDevType devType = btDevType_Unknown;
    bool isConnected = false;
    bool isMaster = false;                      // 本机是否作为主机
    int connId;                                 // 连接 ID
    stBtDevInfo devInfo;

protected slots:
    void slot_this_SendingData(QByteArray _data);

};

#endif // BLUETOOTHINTF_H
