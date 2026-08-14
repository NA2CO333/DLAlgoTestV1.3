#ifndef CBLUETOOTHRK_H
#define CBLUETOOTHRK_H

#include <atomic>

#include <QThread>
#include <QElapsedTimer>
#include <QTime>
#include <QMutex>
#include <QTimer>
#include <QMap>

#include "RkWifiBt/RkBtBase.h"
#include "RkWifiBt/RkBtSpp.h"
#include "RkWifiBt/RkBle.h"
#include "RkWifiBt/RkBleClient.h"

#include "bluetoothintf.h"

// 特征的性质（不是属性的权限？）                          // TODO: 这个定义，该向富联芯索取？
#define BLE_PROPS_BROADCAST         0x01
#define BLE_PROPS_READ              0x02
#define BLE_PROPS_WRITE_WITHOUT     0x04
#define BLE_PROPS_WRITE             0x08
#define BLE_PROPS_NOTIFY            0x10
#define BLE_PROPS_INDICATE          0x20
#define BLE_PROPS_WRITE_SIGNED      0x40
#define BLE_PROPS_EXTENDED          0x80

//
class CBluetoothRk : public CBluetoothIntf
{
    Q_OBJECT

    friend class CBluetoothIntf;
    friend class CBtRkTest;

public:
    ~CBluetoothRk();

    int getDelayMs() override;

    bool setName(const QString &_name) override;
    QString getName() override;

    bool setIsOpened(bool _is_open, QString &_msg) override;        /* 注意：所有对蓝牙库的调用，都须放槽函数中。 */     // TODO: 待进一步封装为一个内部类
    bool searchDevices(QString &_msg) override;
    bool connectDevice(QString _addr, QString &_msg) override;
    bool stopSearching(QString &_msg) override;
    bool disconnectBt(QString _addr = "") override;

    bool getIsOpened() override;
    bool getIsSearched() override;
    bool getIsConnected() override;
    QString getAddr() override;
    QString getAddrBle() override;
    void setAddrBle(QByteArray _addr_ble) override;

Q_SIGNALS:
    // （私有）
    void sigApiGotDev(QString _address, QString _name, unsigned int _class, int _rssi, int _change);        // API 的回调获得设备信息
    void sigApiGotStatus(int Status, enBtProtocol _protocol, QString _addr, QString _name);
    void sigApiRcvData(QByteArray _bytes);
    void sigApiSearchEnd();
    void sigApiStateOnOffChanged(bool _is_bt_on);

    void sigNeedRestart(enBtProtocol _protocol);
    void sigNeedReInit();

    void sigSetIsOpened(bool _is_open);
    void sigSearchDevices();
    void sigConnectDevice(const QString _addr, stBtDevInfo *_dev_info);
    void sigStopSearching();
    void sigDisconnectBt(const QString _addr, stBtDevInfo *_dev_info);

protected Q_SLOTS:
    void slot_rkBt_ApiGotDev(QString _addr, QString _name, unsigned int _class, int _rssi, int _change);
    void slot_rkBt_ApiGotStatus(int _status, enBtProtocol _protocol, QString _addr, QString _name);
    void slot_rkBt_ApiRcvData(QByteArray _bytes);
    void slot_rkBt_ApiStateOnOffChanged(bool _is_bt_on);
    void slot_rkBt_ApiSearchEnd();

    void slot_this_NeedRestart(enBtProtocol _protocol);
    void slot_this_NeedReInit();

    void slot_this_SetIsOpened(bool _is_open);
    void slot_this_SearchDevices();
    void slot_this_ConnectDevice(const QString _addr, stBtDevInfo *_dev_info);
    void slot_this_StopSearching();
    void slot_this_DisconnectBt(const QString _addr, stBtDevInfo *_dev_info);

    void slot_timerSearchTimeLimit_timeout();
    void slot_timerConnectTimeout_timeout();

protected:
    explicit CBluetoothRk(QObject *parent = 0);
    bool sendBtData(int _conn_id, QByteArray _data_bytes) override;

    //
    static CBluetoothRk *instanceBtRk;
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    char nameSpp[32] = "RkBtSpp";
    char nameBle[32] = "RkBtBle";
    QString m_name;

    RkBtContent bt_content;

    //QThread *btThread = Q_NULLPTR;
    // TODO: 如果通过子线程访问蓝牙库，有一定概率发生多线程互斥锁冲突，libpthread.so -> libc.so 抛出 SIGABRT 异常？
    // NOTE: 底层输出log：“pthread_mutex_lock.c:117: __pthread_mutex_lock: Assertion `mutex->__data.__owner == 0' failed.”

    QString addrConnected = "";         // 已连接的 address
    char uuidWritable[40] = {0};

    bool isIniting = false;

    QMutex *mutexSppWrite = Q_NULLPTR;
    QMutex *mutexBleWrite = Q_NULLPTR;

    QString addrSelf = "";              // 自身 address
    bool isInited = false;              // 已初始化
    bool isOpened = false;              // 本模块是否已启动     // TODO: 状态细化（关闭、正在启动、空闲、正在搜索……）？另外增加工作状态变量（正在启动、正在搜索……）？
    bool isSearched = false;            // 执行过启动后的第一次搜索

    bool isSearching = false;           // 正在搜索
    bool isConnecting = false;          // 正在搜索
    QString addrConnecting = "";        // 正在连接的设备 address （申请连接时记录，连接完成后清掉）

    QTimer *timerConnectTimeout = Q_NULLPTR;    // 定时检查正在连接的蓝牙 address 是否被清除，否则清除
    QTimer *timerSearchTimeLimit = Q_NULLPTR;   // 搜索超时定时器。防止由于异常没得到搜索结束消息而导致界面一直处于等待状态
    //QTimer *timerInitCheck = Q_NULLPTR;         // 初始化蓝牙时定时检查状态是否成功（程序是通过状态回调获得状态的，但若有异常，可能得不到该状态）

    QElapsedTimer *elapsedInit = Q_NULLPTR;     // 初始化计时

    QMap<QString, struct stBtDevInfo *> listScanResult;         // 扫描结果列表   // TODO: 线程安全的检查确认优化？
    QMutex mutexListScanResult;

    QList<QString> listConnectedDev;        // 已连接设备列表

    static bool isEnabledSpp;           // 是否启用 SPP
    static bool isEnabledBleServer;     // 是否启用 BLE server
    static bool isEnabledBleClient;     // 是否启用 BLE client

    bool isNeedRestartBleServer = false;            // 是否需要重新打开 BLE Server

    static CBluetoothRk *getInstanceRkBt();

    void initBt();
    void uninitBt();
    bool openSpp();
    void closeSpp();
    bool openBleServer();
    void closeBleServer();
    bool openBleClient();
    void closeBleClient();

    bool doScan(int _stop_after_ms);

    void clearScanResultList();
    stBtDevInfo *getDevInfo(QString _addr);
    void addConnectedDev(QString _addr, QString _name, enBtProtocol _protocol);
    void removeConnectedDev(QString _addr, enBtProtocol _protocol);

    static void state_cb(RK_BT_STATE state);
    static void bond_cb(const char *bd_addr, const char *name, RK_BT_BOND_STATE state);
    static void scan_status_cb(RK_BT_DISCOVERY_STATE status);
    static void scan_cb(const char *address,const char *name, unsigned int bt_class, int rssi, int change);

    static void spp_status_callback(RK_BT_SPP_STATE type);
    static void spp_recv_callback(char *data, int len);

    static void ble_status_callback(const char *bd_addr, const char *name, RK_BLE_STATE state);
    static void ble_recv_data_callback(const char *_uuid, char *_data, int _len);
    static void ble_request_data_callback(const char *uuid);

    static void ble_mtu_callback(const char *bd_addr, unsigned int mtu);

    static void ble_client_state_callback(const char *bd_addr, const char *name, RK_BLE_CLIENT_STATE state);
    static void ble_client_recv_data_callback(const char *uuid, char *data, int len);

    int ble_write(const char *_uuid, QByteArray &_bytes, bool _is_ble_server);
    int ble_write(const char *_uuid, const char *_data, int _len, bool _is_ble_server);

    void emit_apiRecvDev(QString _address, QString _name, unsigned int _class, int _rssi, int _change);
    void emit_apiRecvStatus(int bStatus, enBtProtocol _protocol, QString _addr, const QString _name);
    void emit_apiRecvData(QByteArray &_bytes);
    void emit_apiSearchEnd();
    void emit_apiStateOnOffChanged(bool _is_bt_on);

    void openContinue();
    void close();
    void restartProtocal(enBtProtocol _protocol);

    void initBtDevice();
    char *getBleClientWritableUuid(QString &_addr);

    void afterState_Spp(int _stat);
    void afterState_BleServer(RK_BLE_STATE _stat, QString _addr, QString _name);
    void afterState_BleClient(RK_BLE_CLIENT_STATE _stat, QString _addr, QString _name);

    void doOnApiConnStateChanged(bool _connected, int _conn_id, QString _addr, QString _name, enBtProtocol _bt_protocol);       // 蓝牙连接状态改变

    QString getAddrByApi();                     // 获取本机 address (SPP)
    QString getAddrBleServerByApi();            // 获取本机 address (SppServer)

};

/// ============================================================================================================
/// class CRkBtTest

//
class CBtRkTest : public QObject
{
    Q_OBJECT

public:
    explicit CBtRkTest(QObject *parent = 0, CBluetoothIntf *_bt_intf = 0);

    CBluetoothRk *getBtRk();

    QString getAddrByApi();
    QString getAddrBleServerByApi();

public slots:
    void slot_initBt();
    void slot_uninitBt();
    void slot_openSpp();
    void slot_closeSpp();
    void slot_openBleServer();
    void slot_closeBleServer();
    void slot_openBleClient();
    void slot_closeBleClient();

protected:
    CBluetoothRk *btRk = Q_NULLPTR;

};

#endif // CBLUETOOTHRK_H
