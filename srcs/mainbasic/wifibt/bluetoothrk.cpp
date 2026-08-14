#include "bluetoothrk.h"

#include <unistd.h>

#include <QtDebug>
#include <QTextCodec>
#include <QtMath>

#include "global.h"

// 是否启用 SPP
#define ENABLE_SPP          true

// 是否启用 BLE server
#define ENABLE_BLE_SERVER   true

// 是否启用 BLE client
#define ENABLE_BLE_CLIENT   false

// 初始化之后的冻结时间（延时之后才允许后续操作）  /* 据富联芯说法，调用蓝牙 init() 后，会开关蓝牙电源一次，然后 sleep 4s，所以上层应用应至少等待 5s */
#define INIT_DELAY_MS   5000

// 搜索超时（毫秒）
#define DISCOVERY_TIMEOUT   10000

// 连接超时
#define CONNECTION_TIMEOUT  15000

// 初始化超时
#define INIT_TIMEOUT        10000

//
//#define SERVICE_UUID		"00001910-0000-1000-8000-00805f9b34fb"
//#define BLE_UUID_SEND		"dfd4416e-1810-47f7-8248-eb8be3dc47f9"
//#define BLE_UUID_RECV		"9884d812-1810-4a24-94d3-b2c11a851fac"

#define SERVICE_UUID		"0000FFE0-0000-1000-8000-00805F9B34FB"
#define BLE_UUID_SEND		"0000FFE1-0000-1000-8000-00805F9B34FB"

//
bool CBluetoothRk::isEnabledSpp         = ENABLE_SPP;           // 是否启用 SPP
bool CBluetoothRk::isEnabledBleServer   = ENABLE_BLE_SERVER;    // 是否启用 BLE server
bool CBluetoothRk::isEnabledBleClient   = ENABLE_BLE_CLIENT;    // 是否启用 BLE client

//
static unsigned int g_mtu = 0;

static int g_bleWriteMaxLen;        // BLE Server 的最大一次写入长度

//
CBluetoothRk *CBluetoothRk::instanceBtRk = Q_NULLPTR;

//
CBluetoothRk::CBluetoothRk(QObject *parent) : CBluetoothIntf(parent)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    //
    QObject::connect(this, &CBluetoothRk::sigApiGotDev, this, &CBluetoothRk::slot_rkBt_ApiGotDev, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigApiGotStatus, this, &CBluetoothRk::slot_rkBt_ApiGotStatus, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigApiRcvData, this, &CBluetoothRk::slot_rkBt_ApiRcvData, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigApiStateOnOffChanged, this, &CBluetoothRk::slot_rkBt_ApiStateOnOffChanged, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigApiSearchEnd, this, &CBluetoothRk::slot_rkBt_ApiSearchEnd, Qt::QueuedConnection);

    QObject::connect(this, &CBluetoothRk::sigNeedRestart, this, &CBluetoothRk::slot_this_NeedRestart, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigNeedReInit, this, &CBluetoothRk::slot_this_NeedReInit, Qt::QueuedConnection);

    QObject::connect(this, &CBluetoothRk::sigSetIsOpened, this, &CBluetoothRk::slot_this_SetIsOpened, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigSearchDevices, this, &CBluetoothRk::slot_this_SearchDevices, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigConnectDevice, this, &CBluetoothRk::slot_this_ConnectDevice, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigStopSearching, this, &CBluetoothRk::slot_this_StopSearching, Qt::QueuedConnection);
    QObject::connect(this, &CBluetoothRk::sigDisconnectBt, this, &CBluetoothRk::slot_this_DisconnectBt, Qt::QueuedConnection);

    //btThread = new QThread();
    //this->moveToThread(btThread);       // TODO: 在构造方法里 this->moveToThread() 有没问题？
    //btThread->start();

    //logDebug(QString("%1: btThread = %2").arg(__PRETTY_FUNCTION__).arg(reinterpret_cast<quintptr>(btThread)), CGlobal::LOG_BLUETOOTH);
    //logDebug(QString("%1: QThread::currentThread() = %2, this->thread()->currentThread() = %3")
    //         .arg(__PRETTY_FUNCTION__)
    //         .arg(reinterpret_cast<quintptr>(QThread::currentThread()))
    //         .arg(reinterpret_cast<quintptr>(this->thread()->currentThread()))
    //         , CGlobal::LOG_BLUETOOTH);
    logDebug(QString("%1: QThread::currentThreadId() = %2, this->thread()->currentThreadId() = %3")
             .arg(__PRETTY_FUNCTION__)
             .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
             .arg(reinterpret_cast<quintptr>(this->thread()->currentThreadId()))
             , CGlobal::LOG_BLUETOOTH);

    //btPrinter->moveToThread(btThread);
    //btDatatrans->moveToThread(btThread);

    //
    mutexSppWrite = new QMutex();
    mutexBleWrite = new QMutex();

    // 连接超时检查定时器
    timerConnectTimeout = new QTimer();
    timerConnectTimeout->setSingleShot(true);

    //timerConnectTimeout->moveToThread(btThread);

    QObject::connect(timerConnectTimeout, &QTimer::timeout, this, &CBluetoothRk::slot_timerConnectTimeout_timeout, Qt::QueuedConnection);

    // 搜索超时检查定时器
    timerSearchTimeLimit = new QTimer();
    timerSearchTimeLimit->setSingleShot(true);

    //timerSearchTimeLimit->moveToThread(btThread);

    QObject::connect(timerSearchTimeLimit, &QTimer::timeout, this, &CBluetoothRk::slot_timerSearchTimeLimit_timeout, Qt::QueuedConnection);

    //
    elapsedInit = new QElapsedTimer;

}

//
CBluetoothRk::~CBluetoothRk()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    QString msg;
    setIsOpened(false, msg);        // TODO: 退出程序时并未关闭蓝牙？

    delete mutexSppWrite;
    mutexSppWrite = Q_NULLPTR;

    delete mutexBleWrite;
    mutexBleWrite = Q_NULLPTR;

}

/// ================================================
/// rkbt callback

//
void CBluetoothRk::state_cb(RK_BT_STATE state)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): into ...";
    //qDebug() << __PRETTY_FUNCTION__ << ": into ...";

    switch(state) {
    case RK_BT_STATE_TURNING_ON:
        qDebug() << "RK_BT_STATE_TURNING_ON";
        break;
    case RK_BT_STATE_ON:
        qDebug() << "RK_BT_STATE_ON";

        //
        //rk_bt_free_paired_devices(NULL);      // TODO: 释放不了？

        //
        CBluetoothRk::getInstanceRkBt()->emit_apiStateOnOffChanged(true);

        //
        break;
    case RK_BT_STATE_TURNING_OFF:
        qDebug() << "RK_BT_STATE_TURNING_OFF";
        break;
    case RK_BT_STATE_OFF:
        qDebug() << "RK_BT_STATE_OFF";

        //
        CBluetoothRk::getInstanceRkBt()->emit_apiStateOnOffChanged(false);

        //
        break;
    }
}

void CBluetoothRk::bond_cb(const char *bd_addr, const char *name, RK_BT_BOND_STATE state)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): into ...";
    switch (state) {
    case RK_BT_BOND_STATE_NONE:
    case RK_BT_BOND_STATE_BONDING:
        break;
    case RK_BT_BOND_STATE_BONDED:
        qDebug() << "RK_BT_BOND_STATE_BONDED" << name << bd_addr;
        //CBluetoothRk::getInstanceRkBt()->emitConnected(bd_addr, name);
        break;
    }
}

void CBluetoothRk::scan_status_cb(RK_BT_DISCOVERY_STATE status)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): into, status = " << (int)status;

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    switch (status) {
    case RK_BT_DISC_STARTED:
        printf("+++++ RK_BT_DISC_STARTED +++++\n");
        break;
    case RK_BT_DISC_START_FAILED:
        printf("+++++ RK_BT_DISC_START_FAILED +++++\n");
        break;
    case RK_BT_DISC_STOPPED_BY_USER:
        printf("+++++ RK_BT_DISC_STOPPED_BY_USER +++++\n");
        break;
    case RK_BT_DISC_STOPPED_AUTO:
        printf("+++++ RK_BT_DISC_STOPPED_AUTO +++++\n");
        break;
    }

    // 扫描停止时
    if (status != RK_BT_DISC_STARTED) {     /* 除了 RK_BT_DISC_STARTED 之外的状态都是扫描已经结束的 */
        printf("+++++++++ deregister scan callback +++++++++\n");
        rk_bt_register_discovery_callback(NULL);
        rk_bt_register_dev_found_callback(NULL);

        // 扫描停止消息
        CBluetoothRk::getInstanceRkBt()->emit_apiSearchEnd();

    }

    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): ended";
}

// 设备搜索回调
void CBluetoothRk::scan_cb(const char *address,const char *name, unsigned int bt_class, int rssi, int change)
{
    printf("scan_cb(): address: %s, name: %s, class: 0x%x, rssi: %d, change: %d \n", address, name, bt_class, rssi, change);

    CBluetoothRk::getInstanceRkBt()->emit_apiRecvDev(address, name, bt_class, rssi, change);

}

/******************************************/
/*               BLE SERVER               */
/******************************************/
//
void CBluetoothRk::ble_status_callback(const char *bd_addr, const char *name, RK_BLE_STATE state)
{
    printf("%s(): status: %d.\n", __func__, state);

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    switch (state) {
    case RK_BLE_STATE_IDLE:
        printf("+++++ RK_BLE_STATE_IDLE +++++\n");

        //static bool is_ble_addr_setted = false;
        //if (!is_ble_addr_setted) {
        //    rk_ble_set_address((char *)CBluetoothRk::getInstanceRkBt()->bt_content.ble_content.ble_addr);
        //    is_ble_addr_setted = true;
        //}

        break;
    case RK_BLE_STATE_CONNECT:
        printf("+++++ RK_BLE_STATE_CONNECT: %s, %s +++++\n", name, bd_addr);

        break;
    case RK_BLE_STATE_DISCONNECT:
        printf("+++++ RK_BLE_STATE_DISCONNECT: %s, %s +++++\n", name, bd_addr);
        g_mtu = 0;

        break;
    }

    //
    CBluetoothRk::getInstanceRkBt()->emit_apiRecvStatus(state, btProtocol_BleServer, bd_addr, name);
}

//
void CBluetoothRk::ble_request_data_callback(const char *uuid)
{
    printf("=== %s(): uuid: %s===\n", __func__, uuid);
    //rk_ble_write(uuid, "Hello Rockchip", strlen("Hello Rockchip"));

}

//
void CBluetoothRk::ble_recv_data_callback(const char *_uuid, char *_data, int _len)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): into ...";
    printf("=== %s(): uuid: %s===\n", __func__, _uuid);

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    //char data_t[512];
    //memcpy(data_t, _data, _len);
    //for (int i = 0 ; i < _len; i++) {
    //    printf("%02x ", data_t[i]);
    //}
    //printf("\n");

    //
    QByteArray bytes(_data, _len);
    CBluetoothRk::getInstanceRkBt()->emit_apiRecvData(bytes);

    // 回复一个空格       /* 若不回复，底层会自动回复与收到的数据一致的数据 */       // TODO: 避免需要回复，这个需要供应商富联芯解决
    char rep[] = " ";
    rk_ble_write(BLE_UUID_SEND, rep, 1);
    //CBluetoothRk::getInstanceRkBt()->ble_write(BLE_UUID_SEND, rep, 1, true);      // TODO: 用这个会死锁？

}

//
void CBluetoothRk::ble_mtu_callback(const char *bd_addr, unsigned int mtu)
{
    printf("=== %s():: bd_addr: %s, mtu: %d ===\n", __func__, bd_addr, mtu);
    g_mtu = mtu;

    //
    g_bleWriteMaxLen = BT_ATT_DEFAULT_LE_MTU;      //TODO: 每次写入的长度？算法来自《RK3568_industio/deviceio_release/test/bt_test.c》
    if(g_mtu > BT_ATT_HEADER_LEN)
        g_bleWriteMaxLen = g_mtu;

    g_bleWriteMaxLen -= BT_ATT_HEADER_LEN;
    if(g_bleWriteMaxLen > BT_ATT_MAX_VALUE_LEN)
        g_bleWriteMaxLen = BT_ATT_MAX_VALUE_LEN;

    g_bleWriteMaxLen -= 1;

    logDebug(QString("g_bleWriteMaxLen = %1").arg(g_bleWriteMaxLen), CGlobal::LOG_BLUETOOTH);

    //
    // TODO: 由于 BLE client 的连接状态回调没有被调用，临时用这个回调做为连接成功事件源
    //CBluetoothRk::getInstanceRkBt()->emit_recvStatus(RK_BLE_CLIENT_STATE_CONNECT, btProtocol_BleClient, bd_addr, NULL);

}

/******************************************/
/*               BLE CLIENT               */
/******************************************/
//
void CBluetoothRk::ble_client_state_callback(const char *bd_addr, const char *name, RK_BLE_CLIENT_STATE state)
{
    //printf("%s(): called: addr=%s, name=%s, state=%d\n", __FUNCTION__, bd_addr, name, state);
    printf("%s(): called: addr=%s, name=%s, state=%d\n", __func__, bd_addr, name, state);

    switch (state) {
    case RK_BLE_CLIENT_STATE_IDLE:
        printf("+++++ RK_BLE_CLIENT_STATE_IDLE +++++\n");

        break;
    case RK_BLE_CLIENT_STATE_CONNECT:
        printf("+++++ RK_BLE_CLIENT_STATE_CONNECT(%s, %s) +++++\n", bd_addr, name);

        break;
    case RK_BLE_CLIENT_STATE_DISCONNECT:
        printf("+++++ RK_BLE_CLIENT_STATE_DISCONNECT(%s, %s) +++++\n", bd_addr, name);
        g_mtu = 0;

        break;
    case RK_BLE_CLIENT_WRITE_SUCCESS:
        printf("+++++ RK_BLE_CLIENT_WRITE_SUCCESS(%s, %s) +++++\n", bd_addr, name);

        break;
    case RK_BLE_CLIENT_WRITE_ERROR:
        printf("+++++ RK_BLE_CLIENT_WRITE_ERROR(%s, %s) +++++\n", bd_addr, name);

        break;
    }

    //
    CBluetoothRk::getInstanceRkBt()->emit_apiRecvStatus(state, btProtocol_BleClient, bd_addr, name);
}

//
void CBluetoothRk::ble_client_recv_data_callback(const char *uuid, char *data, int len)
{
    printf("+++++ recv data +++++\n");
    printf("	uuid: %s\n", uuid);
    printf("	data len: %d\n	", len);
    for (int i = 0 ; i < len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");

    //QByteArray bytes(data);
    //CBluetoothRk::getInstanceRkBt()->emit_apiRecvData(bytes, btProtocol_Ble_Client);
}

/******************************************/
/*                  SPP                   */
/******************************************/
//
void CBluetoothRk::spp_status_callback(RK_BT_SPP_STATE type)
{
    switch(type) {
        case RK_BT_SPP_STATE_IDLE:
            printf("+++++++ RK_BT_SPP_STATE_IDLE +++++\n");
            break;
        case RK_BT_SPP_STATE_CONNECT:
            printf("+++++++ RK_BT_SPP_EVENT_CONNECT +++++\n");
            break;
        case RK_BT_SPP_STATE_DISCONNECT:
            printf("+++++++ RK_BT_SPP_EVENT_DISCONNECT +++++\n");
            break;
        default:
            printf("+++++++ BT SPP NOT SUPPORT TYPE! +++++\n");
            break;
    }

    //
    CBluetoothRk::getInstanceRkBt()->emit_apiRecvStatus(type, btProtocol_SPP, NULL, NULL);
}

void CBluetoothRk::spp_recv_callback(char *data, int len)
{
    if (len) {
        printf("+++++++ RK BT SPP RECV DATA: +++++\n");
        printf("\tRECVED(%d):%s\n", len, data);

        //QByteArray bytes(data);
        //CBluetoothRk::getInstanceRkBt()->emit_apiRecvData(bytes, btProtocol_SPP);
    }
}

/******************************************/
/*                  emit                  */
/******************************************/
void CBluetoothRk::emit_apiRecvData(QByteArray &_bytes)
{
    emit sigApiRcvData(_bytes);
}

void CBluetoothRk::emit_apiSearchEnd()
{
    emit sigApiSearchEnd();
}

void CBluetoothRk::emit_apiStateOnOffChanged(bool _is_bt_on)
{
    //logDebug((QString(__PRETTY_FUNCTION__) + ": currentThreadId = %1").arg((qintptr)QThread::currentThreadId()), CGlobal::LOG_BLUETOOTH);
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    emit sigApiStateOnOffChanged(_is_bt_on);
}

void CBluetoothRk::emit_apiRecvDev(QString _address, QString _name, unsigned int _class, int _rssi, int _change)
{
    emit sigApiGotDev(_address, _name, _class, _rssi, _change);
}

void CBluetoothRk::emit_apiRecvStatus(int Status, enBtProtocol _protocol, QString _addr, const QString _name)
{
    emit sigApiGotStatus(Status, _protocol, _addr, _name);
}

/// rkbt callback
/// ================================================

//
CBluetoothRk *CBluetoothRk::getInstanceRkBt()
{
    if (!instanceBtRk) {
        instanceBtRk = dynamic_cast<CBluetoothRk *>(CBluetoothIntf::getInstance());
    }
    return instanceBtRk;
}

//
void CBluetoothRk::initBt()
{
    //logDebug((QString("\n") + QString(__PRETTY_FUNCTION__) + ": currentThreadId: %1").arg((qintptr)QThread::currentThreadId()), CGlobal::LOG_BLUETOOTH);
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    // 冻结时间计时
    isIniting = true;
    elapsedInit->start();
    logDebug("elapsedInit.start() ...", CGlobal::LOG_BLUETOOTH);

    //
    printf("%s(): BT BLUETOOTH INIT\n", __func__);

    //
    memset(&bt_content, 0, sizeof(RkBtContent));

    bt_content.bt_name = this->nameSpp;
    if (m_addrBle.length() >= DEVICE_ADDR_LEN) {
        bt_content.bt_addr = m_addrBle.left(DEVICE_ADDR_LEN).constData();
    }

    bt_content.ble_content.ble_name = this->nameBle;

    if (m_addrBle.length() >= DEVICE_ADDR_LEN) {
        bt_content.ble_content.ble_addr[0] = bt_content.bt_addr[0];
        bt_content.ble_content.ble_addr[1] = bt_content.bt_addr[1];
        bt_content.ble_content.ble_addr[2] = bt_content.bt_addr[2];
        bt_content.ble_content.ble_addr[3] = bt_content.bt_addr[3];
        bt_content.ble_content.ble_addr[4] = bt_content.bt_addr[4];
        bt_content.ble_content.ble_addr[5] = bt_content.bt_addr[5];
    }

    bt_content.ble_content.server_uuid.uuid = SERVICE_UUID;
    bt_content.ble_content.server_uuid.len = UUID_128;
    bt_content.ble_content.chr_uuid[0].uuid = BLE_UUID_SEND;
    bt_content.ble_content.chr_uuid[0].len = UUID_128;
    //bt_content.ble_content.chr_uuid[1].uuid = BLE_UUID_RECV;
    //bt_content.ble_content.chr_uuid[1].len = UUID_128;
    bt_content.ble_content.chr_cnt = 1;

    bt_content.ble_content.advDataType = BLE_ADVDATA_TYPE_USER;

    //标识设备 LE 物理连接的功能
    bt_content.ble_content.advData[1] = 0x02;
    bt_content.ble_content.advData[2] = 0x01;
    bt_content.ble_content.advData[3] = 0x02;

    //service uuid(SERVICE_UUID)
    bt_content.ble_content.advData[4] = 0x03;
    bt_content.ble_content.advData[5] = 0x03;
    bt_content.ble_content.advData[6] = 0x10;
    bt_content.ble_content.advData[7] = 0x19;

    //ble name
    int len_ble_name = strlen(bt_content.ble_content.ble_name);
    printf("ble_name_len: %s(%d)\n", bt_content.ble_content.ble_name, len_ble_name);

    //int len_remain = 31
    //           - (bt_content.ble_content.advData[1] + 1)
    //           - (bt_content.ble_content.advData[4] + 1);       // TODO: 这是什么鬼？31 是否应该改用 MXA_ADV_DATA_LEN - 1 ?
    //int len_cpy = std::min(len_ble_name, len_remain);           // TODO: 这不应该是 std::min(MXA_ADV_DATA_LEN - 1 - 10 - 1, len_ble_name)
    //printf("\nmemcpy len = %d\n", len_cpy);

    int len_cpy = std::min(MXA_ADV_DATA_LEN - 1 - 10 - 1, len_ble_name);
    printf("\nmemcpy len = %d\n", len_cpy);

    bt_content.ble_content.advData[8] = len_cpy + 1;
    bt_content.ble_content.advData[9] = 0x09;

    memcpy(&bt_content.ble_content.advData[10], bt_content.ble_content.ble_name, len_cpy);

    bt_content.ble_content.advData[0] = bt_content.ble_content.advData[1] + 1
                                      + bt_content.ble_content.advData[4] + 1
                                      + bt_content.ble_content.advData[8] + 1;
    bt_content.ble_content.advDataLen = bt_content.ble_content.advData[0] + 1;

    //==========================rsp======================
    bt_content.ble_content.respData[1] = 0x16;  //长度
    bt_content.ble_content.respData[2] = 0xFF;  //字段类型

    /*厂商编码*/
    bt_content.ble_content.respData[3] = 0x46;
    bt_content.ble_content.respData[4] = 0x00;

    bt_content.ble_content.respData[5] = 0x02;  //项目代号长度

    /*项目代号*/
    bt_content.ble_content.respData[6] = 0x1c;
    bt_content.ble_content.respData[7] = 0x02;

    bt_content.ble_content.respData[8] = 0x04;  //版本号长度
    bt_content.ble_content.respData[9] = 'T';   //版本号类型
    /*版本号*/
    bt_content.ble_content.respData[10] = 0x01;
    bt_content.ble_content.respData[11] = 0x00;
    bt_content.ble_content.respData[12] = 0x00;

    bt_content.ble_content.respData[13] = 0x08;	// SN长度
    /*SN号*/
    bt_content.ble_content.respData[14] = 0x54;
    bt_content.ble_content.respData[15] = 0x00;
    bt_content.ble_content.respData[16] = 0x00;
    bt_content.ble_content.respData[17] = 0x00;
    bt_content.ble_content.respData[18] = 0x00;
    bt_content.ble_content.respData[19] = 0x00;
    bt_content.ble_content.respData[20] = 0x00;
    bt_content.ble_content.respData[21] = 0x36;

    bt_content.ble_content.respData[22] = 0x01;	//绑定信息长度
    bt_content.ble_content.respData[23] = 0x00;	//绑定信息

    bt_content.ble_content.respData[0] = bt_content.ble_content.respData[1] + 1;  //长度
    bt_content.ble_content.respDataLen = bt_content.ble_content.respData[0] + 1;

    //
    bt_content.ble_content.cb_ble_recv_fun      = (isEnabledBleServer ? ble_recv_data_callback : NULL);         // TODO: BLE 还未 start，这里为什么要设置这个？
    bt_content.ble_content.cb_ble_request_data  = (isEnabledBleServer ? ble_request_data_callback : NULL);

    //
    rk_bt_register_state_callback(&CBluetoothRk::state_cb);
    rk_bt_register_bond_callback(&CBluetoothRk::bond_cb);

    rk_bt_init(&bt_content);

    qDebug() << __PRETTY_FUNCTION__ << ": ended";
}

void CBluetoothRk::openContinue()
{
    if (isEnabledSpp) {
        openSpp();
    }

    //
    Util::waitMs(2000);      // 有必要吗？这样写有问题吗？

    //
    if (isEnabledBleServer) {
        openBleServer();
    }

    //
    Util::waitMs(500);      // 有必要吗？这样写有问题吗？

    //
    if (isEnabledBleClient) {
        openBleClient();
    }

    //
    isOpened = true;

    //
    Util::waitMs(500);      // 有必要吗？这样写有问题吗？

    //
    emit sigSetIsOpenedFinished(true);

}

void CBluetoothRk::close()
{
    if (isEnabledSpp) {
        closeSpp();
    }

    if (isEnabledBleServer) {
        closeBleServer();
    }

    if (isEnabledBleClient) {
        closeBleClient();
    }

    // 延时
    //QThread::msleep(1000);        // TODO: 需要吗？合适吗？
    Util::waitMs(1000);

    //
    this->uninitBt();

    //
    isOpened = false;

}

//
void CBluetoothRk::restartProtocal(enBtProtocol _protocol)
{
    if (btProtocol_SPP == _protocol) {
        Util::waitMs(500);

        closeSpp();

        Util::waitMs(1000);

        openSpp();
    } else if (btProtocol_BleServer == _protocol) {
        Util::waitMs(500);

        closeBleServer();

        Util::waitMs(1000);

        openBleServer();
    }
}

//
void CBluetoothRk::uninitBt()
{
    //logDebug((QString(__PRETTY_FUNCTION__) + ": currentThreadId: %1").arg((qintptr)QThread::currentThreadId()), CGlobal::LOG_BLUETOOTH);
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    // 蓝牙服务反初始化
    rk_bt_deinit();

}

bool CBluetoothRk::openSpp()
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    int ret = -1;

    // 打开 SPP
    //if (isEnabledSpp)
    {
        char *data_spp = NULL;
        ret = rk_bt_spp_open(data_spp);

        rk_bt_spp_register_status_cb(&CBluetoothRk::spp_status_callback);
        rk_bt_spp_register_recv_cb(&CBluetoothRk::spp_recv_callback);
        //rk_bt_spp_listen();

        std::cout << "SPP opening " << (ret >= 0 ? "succ" : "fail") << ", ret = " << ret << " ..." << std::endl;
    }

    //
    return ret;
}

void CBluetoothRk::closeSpp()
{
    qDebug() << "closing BT SPP";

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    // 关闭 SPP
    //if (isEnabledSpp)
    {
        rk_bt_spp_close();
    }

}

bool CBluetoothRk::openBleServer()
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    int ret = -1;

    // 打开 BLE server
    //if (isEnabledBleServer)
    {
        //if (isEnabledSpp) {
        //    Util::waitMs(2000);
        //}

        rk_ble_register_status_callback(&CBluetoothRk::ble_status_callback);
        rk_ble_register_recv_callback(&CBluetoothRk::ble_recv_data_callback);
        rk_ble_register_request_data_callback(&CBluetoothRk::ble_request_data_callback);

        rk_ble_register_mtu_callback(&CBluetoothRk::ble_mtu_callback);

        ret = rk_ble_start(&bt_content.ble_content);

        std::cout << "BLE server opening " << (ret >= 0 ? "succ" : "fail") << ", ret = " << ret << " ..." << std::endl;
    }

    //
    return ret;
}

void CBluetoothRk::closeBleServer()
{
    qDebug() << "closing BT BLE Server";

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    // 关闭 BLE server
    //if (isEnabledBleServer)
    {
        rk_ble_stop();
    }

}

bool CBluetoothRk::openBleClient()
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    int ret = -1;

    // 打开 BLE client
    //if (isEnabledBleClient)
    {
        //if (isEnabledBleServer) {
        //    Util::waitMs(2000);
        //}

        rk_ble_client_register_state_callback(&CBluetoothRk::ble_client_state_callback);
        rk_ble_client_register_recv_callback(&CBluetoothRk::ble_client_recv_data_callback);

        rk_ble_client_register_mtu_callback(&CBluetoothRk::ble_mtu_callback);

        ret = rk_ble_client_open(true);

        std::cout << "BLE client opening " << (ret >= 0 ? "succ" : "fail") << ", ret = " << ret << " ..." << std::endl;
    }

    //
    return ret;
}

void CBluetoothRk::closeBleClient()
{
    qDebug() << "closing BT BLE Client";

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    // 关闭 BLE client
    //if (isEnabledBleClient)
    {
        rk_ble_client_close();
    }

}

bool CBluetoothRk::doScan(int _stop_after_ms)
{
    std::cout << "CBluetoothRk::scanDelay() into ..." << std::endl;

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    if (!isEnabledSpp && (!isEnabledBleServer && !isEnabledBleClient)) {
        std::cout << "bt mode config invalid! not need discovery!" << std::endl;
        return false;
    }

    /* 新版卡需要主动关闭扫描，时间参数无效 */
    RK_BT_SCAN_TYPE scan_type = SCAN_TYPE_AUTO;
    if (!isEnabledBleServer && !isEnabledBleClient) {
        scan_type = SCAN_TYPE_BREDR;        // TODO: SCAN_TYPE_SPP 是干嘛的？和 SCAN_TYPE_BREDR 不同？命令行测试程序里也有这个类型
    } else if (!isEnabledSpp) {
        scan_type = SCAN_TYPE_LE;
    }
    std::cout << "scan type = " << (int)scan_type << std::endl;

    rk_bt_register_discovery_callback(&CBluetoothRk::scan_status_cb);
    rk_bt_register_dev_found_callback(&CBluetoothRk::scan_cb);

    rk_bt_start_discovery(_stop_after_ms, scan_type);

    QTimer::singleShot(_stop_after_ms * 1.1, this, []() {
        rk_bt_cancel_discovery();

        std::cout << "\nscaned devs:" << std::endl;
        rk_bt_display_devices();
        std::cout << std::endl;

        //
        //rk_bt_get_scaned_devices(...);
        //
        //rk_bt_free_scaned_devices(...);

    });

    std::cout << "CBluetoothRk::scanDelay() ended" << std::endl;

    //
    return true;
}

//
int CBluetoothRk::ble_write(const char *_uuid, QByteArray &_bytes, bool _is_ble_server)
{
    char *data = _bytes.data();
    int len = _bytes.size();

    return ble_write(_uuid, data, len, _is_ble_server);
}

//
int CBluetoothRk::ble_write(const char *_uuid, const char *_data, int _len, bool _is_ble_server)
{
    logDebug((QString(__PRETTY_FUNCTION__) + " into, uuid = %1, len = %2, protocol = %3")
             .arg(_uuid).arg(_len).arg(_is_ble_server ? "BleServer" : "BleClient"), CGlobal::LOG_BLUETOOTH);

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    // lock
    //mutexBleWrite->lock();      // TODO: 好像会导致死锁？

    //
    int ret = -1;

    //
    const int BLE_WRITE_MAX_LEN = 64;

    //int len_per_frame = g_bleWriteMaxLen;
    int len_per_frame = (g_bleWriteMaxLen > BLE_WRITE_MAX_LEN ? BLE_WRITE_MAX_LEN : g_bleWriteMaxLen);

    char *write_buf;
    write_buf = (char *)malloc(len_per_frame + 1);
    write_buf[len_per_frame] = '\0';

    /* 经实测，有时会丢失第一个包，所以这里发送一个无用的包，避免后面有用的数据丢失 */        // TODO: 有更好的解决方法？
    //char c_test[] = "\x00";
    //rk_ble_write(_uuid, c_test, 1);
    //QThread::msleep(30);

    //
    int write_count = 0;
    int write_succ = -1;
    int len_curr;

    int max_loop = _len / (len_per_frame - 1) + 3;
    int i = 0;
    while (write_count < _len) {
        len_curr = len_per_frame;
        if (len_curr > _len - write_count) {
            len_curr = _len - write_count;
        }
        strncpy(write_buf, _data, len_curr);
        write_buf[len_curr] = '\0';

        if (_is_ble_server) {
            /* 经实测（工具为小程序“HCBLE传口助手”）：
             * 1、若一次发送数据大于 g_bleWriteMaxLen (=512)，则后面的数据都将收不到。
             * 2、若发送一次后不延时，则只能收到最后一次发送的数据。
             */
            write_succ = rk_ble_write(_uuid, write_buf, len_curr);      // TODO: 这个函数的返回值好像不是写入成功数，成功时返回的是0
        } else {
            write_succ = rk_ble_client_write(_uuid, write_buf, len_curr);
        }
        logDebug((QString(_is_ble_server ? "rk_ble_write" : "rk_ble_client_write") + "(\"%1\"\n, %2) -> %3")
                 .arg(write_buf).arg(len_curr).arg(write_succ), CGlobal::LOG_BLUETOOTH);

        if (write_succ < 0) {
            logWarning(QString("write failed! data_len = %1, write_count = %2").arg(_len).arg(write_count), CGlobal::LOG_BLUETOOTH);
            break;
        } else {
            write_count += len_curr;
            _data += len_curr;
        }

        // 延时
        int delay_ms = qCeil((double)len_per_frame * 1.0);            /* 经实测，这里延时最短是 (发送字节数 / 10) ？ */
        //if (0 == i) {
        //    delay_ms += 100;
        //}
        Util::waitMs(delay_ms);             // TODO: 若不延时，为什么只能收到最后一次发送的数据？

        // 防死循环
        i++;
        logDebug(QString("write count: %1").arg(i), CGlobal::LOG_BLUETOOTH);
        if (i > max_loop) {
            logCritical(QString(__PRETTY_FUNCTION__) + ": logic error! loop too much!", CGlobal::LOG_BLUETOOTH);
            write_succ = -1;
            break;
        }
    }

    //
    free(write_buf);
    write_buf = nullptr;

    //
    if (write_succ >= 0) {
        ret = write_count;
    }

    // unlock
    //mutexBleWrite->unlock();

    //
    //qDebug() << __PRETTY_FUNCTION__ << ": sleeping in thread " << reinterpret_cast<quintptr>(QThread::currentThreadId());
    //QThread::msleep(10000);

    //
    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_BLUETOOTH);
    return ret;

}

// 清空搜索结果列表中除了已连接的设备外的其它设备
void CBluetoothRk::clearScanResultList()
{
    mutexListScanResult.lock();

    QList<QString> keys = listScanResult.keys();
    QString key;
    for (int i = keys.size() - 1; i >= 0; i--) {
        key = keys[i];
        if (!listConnectedDev.contains(key)) {
            delete listScanResult.value(key);
            listScanResult.remove(key);
        }
    }

    mutexListScanResult.unlock();
}

//
stBtDevInfo *CBluetoothRk::getDevInfo(QString _addr)
{
    mutexListScanResult.lock();

    stBtDevInfo *dev_info = listScanResult[_addr];

    mutexListScanResult.unlock();

    return dev_info;
}

void CBluetoothRk::addConnectedDev(QString _addr, QString _name, enBtProtocol _protocol)
{
    if (btProtocol_SPP == _protocol) {
        _addr = addrConnected;
    }

    //
    mutexListScanResult.lock();

    //
    if (listScanResult.contains(_addr)) {           // 若已在扫描结果表中存在，则只需添加已连接设备编号
        listConnectedDev.append(_addr);
    } else {                                        // 否则需要创建设备信息，并添加到扫描结果表
        logWarning(QString(__PRETTY_FUNCTION__) + ": addr not exists in listScanResult, creating info.", CGlobal::LOG_BLUETOOTH);

        //
        stBtDevInfo *dev_info = new stBtDevInfo;

        dev_info->addr = _addr;
        dev_info->name = _name;
        dev_info->btClass = -1;
        dev_info->rssi = -9999;
        dev_info->change = 0;
        dev_info->protocol = _protocol;

        listScanResult.insert(dev_info->addr, dev_info);
        logDebug(QString("inserted one dev_info into listScanResult, addr = %1, name = %2, protocol = %3").arg(_addr).arg(_name).arg((int)_protocol));

        //
        if (!listConnectedDev.contains(_addr)) {
            listConnectedDev.append(_addr);
        }
    }

    //
    mutexListScanResult.unlock();
}

void CBluetoothRk::removeConnectedDev(QString _addr, enBtProtocol _protocol)
{
    if (btProtocol_SPP == _protocol) {
        _addr = addrConnected;
    }

    int count_removed = listConnectedDev.removeAll(_addr);
    logDebug(QString("%! connected dev_info has been removed.").arg(count_removed), CGlobal::LOG_BLUETOOTH);
}

//
int CBluetoothRk::getDelayMs()
{
    return INIT_DELAY_MS;
}

// 初始化系统的蓝牙设备配置
void CBluetoothRk::initBtDevice()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    if (isInited) {
        return;
    }

    //
    // TODO: 调用脚本 /usr/bin/bt_init.sh 对系统蓝牙设备初始化？


    //
    isInited = true;
}

//
char *CBluetoothRk::getBleClientWritableUuid(QString &_addr)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    if (!uuidWritable[0]) {
        logDebug("CBluetoothRk::getWritableUuid(): finding uuid", CGlobal::LOG_BLUETOOTH);
        RK_BLE_CLIENT_SERVICE_INFO info;
        int ret = rk_ble_client_get_service_info(_addr.toLatin1().data(), &info);
        if (ret >= 0) {
            logDebug(QString::asprintf("CBluetoothRk::getWritableUuid(): service count = %d", info.service_cnt), CGlobal::LOG_BLUETOOTH);
            bool is_found = false;
            unsigned int props = (BLE_PROPS_WRITE_WITHOUT | BLE_PROPS_READ);
            for (int i = 0; i < info.service_cnt; i++) {
                RK_BLE_CLIENT_SERVICE &service = info.service[i];
                for (int j = 0; j < service.chrc_cnt; j++) {
                    RK_BLE_CLIENT_CHRC &chrc = service.chrc[j];
                    logDebug(QString::asprintf("chrc.props = 0x%x", chrc.props), CGlobal::LOG_BLUETOOTH);
                    if ((chrc.props & props) == props) {
                        strncpy(uuidWritable, chrc.uuid, sizeof (uuidWritable));
                        logDebug(QString::asprintf("CBluetoothRk::getWritableUuid(): get uuid = %s", uuidWritable), CGlobal::LOG_BLUETOOTH);
                        is_found = true;
                        break;
                    }
                }
                if (is_found) {
                    break;
                }
            }
            if (!is_found) {
                logCritical("writable uuid not found!", CGlobal::LOG_BLUETOOTH);
            }
        } else {
            logCritical("rk_ble_client_get_service_info() failed!");
        }
    }
    return uuidWritable;
}

bool CBluetoothRk::setName(const QString &_name)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    //
    m_name = _name;

    //
    strncpy(this->nameSpp, (_name /*+ "_SPP"*/).toLatin1().data(), 32);      // TODO: 两种协议的广播名，没必要区分吧？普通用户会迷惑的，而专业用户可通过 address 等信息区分
    strncpy(this->nameBle, (_name + "_BLE").toLatin1().data(), 32);

    //
    if (getIsOpened() && ENABLE_BLE_SERVER) {
        // 设置 SPP name
        //rk_bt_set_device_name(this->nameSpp);

        // 设置 BLE name
        //emit sigNeedRestart(btProtocol_BleServer);
        emit sigNeedReInit();                               /* 经测试，重开 BLE Server 后，BLE name 并未变，重初始化后才有效 */
    }

    //
    return true;
}

QString CBluetoothRk::getName()
{
    return m_name;
}

bool CBluetoothRk::setIsOpened(bool _is_open, QString &_msg)
{
    logDebug((QString(__PRETTY_FUNCTION__) + " into ... _is_open = %1").arg(Util::bool2str(_is_open)), CGlobal::LOG_BLUETOOTH);

    if (_is_open) {
        if (!isEnabledSpp && (!isEnabledBleServer && !isEnabledBleClient)) {
            _msg = "config error! spp and ble not enabled both!";
            logCritical(QString(__PRETTY_FUNCTION__) + ": " + _msg, CGlobal::LOG_BLUETOOTH);
            return false;
        }
    } else {
        //
        if (getIsConnected()) {
            QString addr = addrConnected;
            qDebug() << "CBluetoothRk::setIsOpened() disconnecting addr : " << addr;
            disconnectBt(addr);
        }
    }

    //
    emit sigSetIsOpened(_is_open);

    //
    return true;
}

// 获取本机 address (SPP)
QString CBluetoothRk::getAddrByApi()
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    QString addr_str;

    constexpr int LEN = DEVICE_ADDR_LEN * 6;    /* 经调试，若这里设置的长度不够 18(DEVICE_ADDR_LEN * 3)，则返回为空 */
    char addr_c[LEN] = {0};
    int ret = rk_bt_get_device_addr(addr_c, LEN);       /* 经调试，这是 SPP 的地址，而 BLE 的地址不是这个 */
    qDebug() << "rk_bt_get_device_addr() -> " << ret << ", addr = " << addr_c;

    bool succ = (0 == ret);
    if (succ) {
        addr_str = addr_c;
    } else {
        addr_str = "";
    }

    return addr_str;
}

// 获取本机 address (BleServer)
QString CBluetoothRk::getAddrBleServerByApi()
{
    QByteArray addr_byte = QByteArray((char *)bt_content.ble_content.ble_addr, DEVICE_ADDR_LEN);
    std::reverse(addr_byte.begin(), addr_byte.end());       /* 调试发现这里的字节顺序和在蓝牙调试助手里搜到的 address 的字节顺序是相反的 */
    QString addr = addr_byte.toHex(':').toUpper();
    return addr;
}

bool CBluetoothRk::searchDevices(QString &_msg)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    //
    _msg = "";

    //
    clearScanResultList();

    //
    emit sigSearchDevices();

    //
    return true;
}

// 槽函数：接收到设备信息
void CBluetoothRk::slot_rkBt_ApiGotDev(QString _addr, QString _name, unsigned int _class, int _rssi, int _change)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    mutexListScanResult.lock();

    // 过滤重复蓝牙
    bool is_repeat = listScanResult.contains(_addr);        // TODO: 同一个 addr 可以有多个 class ？实测，小票打印机的 addr 有 0x0 和 0x40680 两种 class
    if (!is_repeat) {
        qDebug() << "new dev found: name=" << _name << ", addr=" << _addr << ", class=" << _class;

        //
        if (_class > 0 && !((0x1F00 & _class) == 0x100)) {       /* COD 的 8~12 位 为 0b00001 （规范中定义为 Computer）的，应该不是打印机 */
            stBtDevInfo *dev_info = new stBtDevInfo;

            dev_info->addr = _addr;
            dev_info->name = _name;
            dev_info->btClass = _class;
            dev_info->rssi = _rssi;
            dev_info->change = _change;

            dev_info->protocol = (_class > 0 ? btProtocol_SPP : (isEnabledBleServer ? btProtocol_BleServer : btProtocol_BleClient));
            /* 经调试，目前(2023-05-23)所用的新旧两款小票打印机的 SPP 都是 > 0 的(0x40680)，ble 都是 = 0 的 */

            listScanResult.insert(dev_info->addr, dev_info);
            logDebug(QString("inserted one dev_info into listScanResult, addr = %1, name = %2, protocol = %3").arg(dev_info->addr).arg(dev_info->name).arg((int)dev_info->protocol), CGlobal::LOG_BLUETOOTH);

            logDebug(QString("emit bluetooth device found event: name: %1, addr: %2").arg(_name).arg(_addr), CGlobal::LOG_BLUETOOTH);
            emit sigFoundDevice(_name, _addr);   // 只将 SPP 类型的设备通知上层
        } else {
            logDebug(QString("cod %1 of addr %2 seen not spp, filterd").arg(_class).arg(_addr), CGlobal::LOG_BLUETOOTH);
        }
    } else {
        qDebug() << _addr << " repeated";
    }

    mutexListScanResult.unlock();
}

// 槽函数：连接状态改变
void CBluetoothRk::slot_rkBt_ApiGotStatus(int _status, enBtProtocol _protocol, QString _addr, QString _name)
{
    logDebug((QString(__PRETTY_FUNCTION__) + " into ... stat = %1, protocol = %2, addr = %3, name = %4").arg(_status).arg(_protocol).arg(_addr).arg(_name), CGlobal::LOG_BLUETOOTH);

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    if (btProtocol_SPP == _protocol) {
        afterState_Spp(_status);
    } else if (btProtocol_BleServer == _protocol) {
        afterState_BleServer((RK_BLE_STATE)_status, _addr, _name);
    } else if (btProtocol_BleClient == _protocol) {
        afterState_BleClient((RK_BLE_CLIENT_STATE)_status, _addr, _name);
    } else {
        logCritical((QString(__PRETTY_FUNCTION__) + ": protocol type %1 not valid!").arg((int)_protocol), CGlobal::LOG_BLUETOOTH);
    }
}

void CBluetoothRk::afterState_Spp(int _stat)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    RK_BT_SPP_STATE bt_stat = (RK_BT_SPP_STATE)_stat;

    switch (bt_stat) {
        case RK_BT_SPP_STATE_IDLE: {        // 空闲状态
            //
        }
        break;
        case RK_BT_SPP_STATE_CONNECT: {
            isConnecting = false;
            timerConnectTimeout->stop();

            //
            if (addrConnecting.length() > 0) {
                addrConnected = addrConnecting;
                addrConnecting = "";

                //
                doOnApiConnStateChanged(true, 0, addrConnected, "", btProtocol_SPP);    // TODO: 从底层得到正在连接的 address ？
            } else {                            // 若正在连接 address 为空，则是 SPP 被动连接
                // TODO: 这种情况下，现有 API 无法得到 addr ，也无法主动断连？
                // TODO: 怎么办？SPP 被动连接后，蓝牙打印也无法进行了？

                doOnApiConnStateChanged(true, -1, "", "", btProtocol_SPP);
            }
        }
        break;
        case RK_BT_SPP_STATE_CONNECT_FAILED: {
            isConnecting = false;
            timerConnectTimeout->stop();

            emit sigConnStateChanged(false, 0, addrConnecting, "", btDevType_Printer);
            emit sigNotice("Connecting failed!");

            addrConnecting = "";
        }
        break;
        case RK_BT_SPP_STATE_DISCONNECT: {
            doOnApiConnStateChanged(false, -1, addrConnected, "", btProtocol_SPP);
        }
        break;
    default:
        break;
    }
}

void CBluetoothRk::afterState_BleServer(RK_BLE_STATE _stat, QString _addr, QString _name)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    switch (_stat) {
        case RK_BLE_STATE_IDLE: {        // 空闲状态
            //
        }
        break;
        case RK_BLE_STATE_CONNECT: {
            isConnecting = false;
            timerConnectTimeout->stop();

            //addrConnected = addrConnecting;
            addrConnected = _addr;
            addrConnecting = "";

            doOnApiConnStateChanged(true, 1, _addr, _name, btProtocol_BleServer);
        }
        break;
        case RK_BLE_STATE_DISCONNECT: {
            isConnecting = false;

            doOnApiConnStateChanged(false, -1, _addr, _name, btProtocol_BleServer);

            //
            if (isNeedRestartBleServer) {
                restartProtocal(btProtocol_BleServer);
            }
        }
        break;
    default:
        break;
    }
}

void CBluetoothRk::afterState_BleClient(RK_BLE_CLIENT_STATE _stat, QString _addr, QString _name)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    switch (_stat) {
        case RK_BLE_CLIENT_STATE_IDLE: {        // 空闲状态
            //
        }
        break;
        case RK_BLE_CLIENT_STATE_CONNECT: {
            isConnecting = false;
            timerConnectTimeout->stop();

            //addrConnected = addrConnecting;
            addrConnected = _addr;
            addrConnecting = "";

            doOnApiConnStateChanged(true, 0, addrConnected, _name, btProtocol_BleClient);
        }
        break;
        case RK_BLE_CLIENT_STATE_DISCONNECT: {
            isConnecting = false;

            doOnApiConnStateChanged(false, -1, _addr, _name, btProtocol_BleClient);
        }
        break;
        case RK_BLE_CLIENT_WRITE_SUCCESS: {

        }
        break;
        case RK_BLE_CLIENT_WRITE_ERROR: {

        }
        break;
    default:
        break;
    }
}

// 槽函数：
void CBluetoothRk::slot_rkBt_ApiRcvData(QByteArray _bytes)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ... recieved data: " + _bytes, CGlobal::LOG_BLUETOOTH);

    // 蓝牙数据接收，一律认为来自数据通信设备
    btDatatrans->pushReceivedData(_bytes);

    //
    emit sigLog(QString("\n接收：\n") + QString::fromUtf8(_bytes));

}

void CBluetoothRk::slot_rkBt_ApiStateOnOffChanged(bool _is_bt_on)
{
    logDebug((QString(__PRETTY_FUNCTION__) + " into ... _is_bt_on = %1").arg(Util::bool2str(_is_bt_on)), CGlobal::LOG_BLUETOOTH);

    //logDebug((QString(__PRETTY_FUNCTION__) + ": currentThreadId: %1").arg((qintptr)QThread::currentThreadId()), CGlobal::LOG_BLUETOOTH);
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    if (_is_bt_on) {     /* 到这里时，只是 RkBt 库初始化完成，并未打开 SPP 或 BLE */
        //
        isIniting = false;

        // 延时
        if (elapsedInit->isValid()) {                   // TODO: 这里得到的 elapsed 是负数，调试了挺久，原因未明。改用其它计时方式？
            int elapsed_init = elapsedInit->elapsed();
            logDebug(QString("elapsed_init = %1").arg(elapsed_init), CGlobal::LOG_BLUETOOTH);
            int delay = INIT_DELAY_MS - elapsed_init;
            if (delay > 0) {        // TODO: 这里的线程是？如果是主线程。。。
                logDebug(QString("delaying %1 ms for init ...").arg(delay), CGlobal::LOG_BLUETOOTH);
                //QThread::msleep(delay);
                Util::waitMs(delay);
            }
        } else {
            logCritical(QString(__PRETTY_FUNCTION__) + ": logic err: elapsedInit not valid !", CGlobal::LOG_BLUETOOTH);
        }

        // 获取本机 address
        addrSelf = getAddrByApi();
        qDebug() << "addrSelf = " << addrSelf;

        //
        Util::waitMs(500);      // 有必要吗？这样写有问题吗？

        // 继续打开 SPP 或 BLE
        this->openContinue();
    } else {            /* 到这里时，RkBt 库反初始化完成 */
        emit sigSetIsOpenedFinished(false);
    }
}

// 连接指定地址的设备
bool CBluetoothRk::connectDevice(QString _addr, QString &_msg)
{
    logDebug(QString("CBluetoothRk::connectDevice(): into ... addr = ") + _addr, CGlobal::LOG_BLUETOOTH);

    //
    stBtDevInfo *dev_info = this->getDevInfo(_addr);
    if (!dev_info) {
        _msg = QString("dev \"%1\" not found! can\'t connect!").arg(_addr);
        logDebug(QString(__PRETTY_FUNCTION__) + ": " + _msg, CGlobal::LOG_BLUETOOTH);
        return false;
    }

    //
    emit sigConnectDevice(_addr, dev_info);

    //
    return true;
}

// 断连
bool CBluetoothRk::disconnectBt(QString _addr)
{
    logDebug("CBluetoothRk::disconnectBt() into ...", CGlobal::LOG_BLUETOOTH);

    //
    if (_addr.length() == 0) {
        _addr = addrConnected;
    }

    // 若 addr 为空，以目前调试情况，应该是 SPP 被连了，这种情况下，只能重启 SPP
    if (_addr.length() == 0) {
        logDebug((QString(__PRETTY_FUNCTION__) + ": SPP has been connected by external dev, restarting SPP ..."), CGlobal::LOG_BLUETOOTH);
        emit sigNeedRestart(btProtocol_SPP);
        emit sigConnStateChanged(false, -1, "", "", btDevType_Printer);

        return false;
    }

    //
    stBtDevInfo *dev_info = this->getDevInfo(_addr);
    if (!dev_info) {
        QString msg = QString("address \"%1\" not found! failed to disconnect!").arg(_addr);
        logDebug(QString(__PRETTY_FUNCTION__) + ": " + msg, CGlobal::LOG_BLUETOOTH);

        if (CGlobal::isDebugMode) {
            emit sigNotice(msg);
        }

        return false;
    }

    //
    emit sigDisconnectBt(_addr, dev_info);

    //
    return true;
}

//
bool CBluetoothRk::stopSearching(QString &_msg)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    //
    if (!isSearching)
        return true;

    //
    _msg = "";

    //
    emit sigStopSearching();

    //
    return true;
}

// 发送蓝牙数据
bool CBluetoothRk::sendBtData(int _conn_id, QByteArray _data_bytes)
{
    logDebug((QString(__PRETTY_FUNCTION__) + " into ... , _conn_id = %1, data len = %2").arg(_conn_id).arg(_data_bytes.size()), CGlobal::LOG_BLUETOOTH);

    //int debug_data_len = _data_bytes.size();
    //int debug_pos = 0;
    //int debug_row_len = 50;
    //while (debug_pos < debug_data_len) {
    //    logDebug(QString(_data_bytes.mid(debug_pos, debug_row_len).toHex(' ')), CGlobal::LOG_BLUETOOTH);
    //    debug_pos += debug_row_len;
    //}

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    // 需已打开
    if (!isOpened) {
        qDebug() << "CBluetoothRk::sendBtData(): bluetooth not opened!";
        return false;
    }

    // conn id 参数检查
    if (_conn_id < 0) {
        logDebug(QString(__PRETTY_FUNCTION__) + ": _conn_id is invalid, sending failed!", CGlobal::LOG_BLUETOOTH);
        return false;
    }

    // 设备类型判断
    enBtDevType dev_type = (0 == _conn_id ? btDevType_Printer : btDevType_Datatrans);  // TODO: 逻辑完善

    // 协议类型判断
    enBtProtocol protocol_type = (1 == _conn_id ? btProtocol_BleServer : btProtocol_SPP); // TODO: connId 暂时根据协议类型设定，应动态自增

    // 需已连接
    if (btProtocol_SPP == protocol_type) {
        if (!btPrinter->getIsConnected()) {
            qDebug() << "CBluetoothRk::sendBtData(): bt printer not connected!";
            return false;
        }
    } else if (btProtocol_BleServer == protocol_type) {
        if (!btDatatrans->getIsConnected()) {
            qDebug() << "CBluetoothRk::sendBtData(): bt datatrans not connected!";
            return false;
        }
    }

    // 根据协议类型写入发送数据
    int ret = 0;
    try {
        //
        if (btProtocol_SPP == protocol_type) {
            char *buf = _data_bytes.data();
            int len = _data_bytes.size();

            mutexSppWrite->lock();                  // TODO: 有必要吗？
            ret = rk_bt_spp_write(buf, len);
            mutexSppWrite->unlock();
        } else if (btProtocol_BleServer == protocol_type) {
            ret = this->ble_write(BLE_UUID_SEND, _data_bytes, true);
        } else if (btProtocol_BleClient == protocol_type) {
            QString addr = addrConnected;

            stBtDevInfo *dev_info = this->getDevInfo(addr);
            if (!dev_info) {
                logDebug(QString("CBluetoothRk::sendBtData(): dev \"%1\" not found! can\'t send data!").arg(addr), CGlobal::LOG_BLUETOOTH);
                return false;
            }

            const char *uuid = getBleClientWritableUuid(addr);
            if (uuid) {
                logDebug(QString::asprintf("CBluetoothRk::sendBtData(): ble client send data use uuid \"%s\"", uuid), CGlobal::LOG_BLUETOOTH);
                ret = this->ble_write(uuid, _data_bytes, false);
            } else {
                logCritical("CBluetoothRk::sendBtData(): ble client get uuid failed! so writing failed!", CGlobal::LOG_BLUETOOTH);
            }
        }
    } catch (...) {
        int err_no = errno;
        logWarning(QString::asprintf("CBluetoothRk::sendBtData(): unknown exception: no=%d, msg=%s", err_no, strerror(err_no)), CGlobal::LOG_BLUETOOTH);
    }

    if (ret < 0) {
        printf("%s(): failed\n", __func__);
    }

    //
    QString data_text;
    if (btDevType_Printer == dev_type) {
        QTextCodec *codec = QTextCodec::codecForName("gb2312");    // TODO: 这个编码的逻辑应该不属于这里，移出去？
        data_text = codec->toUnicode(_data_bytes);
    } else {
        data_text = QString::fromUtf8(_data_bytes);
    }
    emit sigLog(QString("\n%1(conn=%2)：\n").arg(ret >= 0 ? "已发送" : "发送失败").arg(_conn_id) + data_text);

    //
    return (ret >= 0);
}

// 判断是否已连接
bool CBluetoothRk::getIsConnected()
{
    //logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    //qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    //{
    //    static bool is_connected = false;
    //    static QElapsedTimer *timer = Q_NULLPTR;
    //
    //    static const int QUERY_INTERVAL_MS = 2000;
    //
    //    //
    //    if (!timer) {
    //        timer = new QElapsedTimer();
    //
    //        //rk_bt_spp_get_state();
    //        int ret = rk_bt_is_connected();
    //        is_connected = (ret != 0);
    //
    //        timer->start();
    //    } else if (timer->elapsed() > QUERY_INTERVAL_MS) {
    //        //rk_bt_spp_get_state();
    //        int ret = rk_bt_is_connected();       /* 就算限制调用次数，但是调用 rk_bt_is_connected() 时还是会卡顿一下，这使测量界面的刷帧体验很不好！除非将电源模块对此函数的定时调用改为信号槽方式？ */
    //        is_connected = (ret != 0);
    //
    //        timer->start();
    //    }
    //
    //    //
    //    return is_connected;
    //}

    {
        return (getBtPrinter()->isConnected || getBtDatatrans()->isConnected);
    }
}

void CBluetoothRk::slot_rkBt_ApiSearchEnd()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    if (!isSearching) {
        logDebug(QString(__PRETTY_FUNCTION__) + ": escaped", CGlobal::LOG_BLUETOOTH);
        return;
    }

    //
    isSearched = true;

    timerSearchTimeLimit->stop();
    isSearching = false;

    //
    emit sigSearchEnd();

    /* 底层缺陷弥补处理逻辑：
     * 由于扫描后，BLE 广播会被停掉，需要关闭后再打开，因此加上以下控制逻辑：
     * 在扫描结束后，若 ble 未连接，则关闭再打开 ble，若 ble 已连接，则将“需要重开ble”标志置为 true，然后在 ble 断连时，检查该标志，若 true 则关闭再打开 ble 。
     */
    RK_BLE_STATE state_ble;
    rk_ble_get_state(&state_ble);
    if (RK_BLE_STATE_CONNECT != state_ble) {
        restartProtocal(btProtocol_BleServer);
    } else {
        isNeedRestartBleServer = true;
    }

}

void CBluetoothRk::slot_this_NeedRestart(enBtProtocol _protocol)
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    restartProtocal(_protocol);
}

void CBluetoothRk::slot_this_NeedReInit()
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    uninitBt();

    //
    Util::waitMs(4000);

    //
    initBt();
}

void CBluetoothRk::slot_this_SetIsOpened(bool _is_open)
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    if (_is_open) {
        //
        listConnectedDev.clear();

        // 初始化结果检查定时器
        //timerInitCheck.start(INIT_TIMEOUT);
        // TODO: 有必要吗？

        //
        this->initBt();

    } else {
        // 停止设备扫描
        rk_bt_cancel_discovery();

        //
        this->close();
    }

    // 重置状态
    getBtPrinter()->setIsConnected(false);
    addrConnecting = "";
    addrConnected = "";
    isSearching = false;
    isSearched = false;
    isConnecting = false;
    uuidWritable[0] = 0;
    isNeedRestartBleServer = false;

    //
    //if (!_is_open) {
    //    isInited = false;
    //}

}

void CBluetoothRk::slot_this_SearchDevices()
{
    //
    isSearching = true;

    //
    static int scan_count = 0;

    int scan_timeout_ms = DISCOVERY_TIMEOUT;
    if (0 == scan_count) {
        scan_timeout_ms *= 1.5;
    }

    //
    bool succ = this->doScan(scan_timeout_ms);        // TODO: BR/EDR 和 LE 设备要分开扫描？

    //
    if (succ) {
        timerSearchTimeLimit->start(scan_timeout_ms * 1.5);
    } else {
        QString msg = "search failed!";
        logDebug(QString(__PRETTY_FUNCTION__) + ": " + msg, CGlobal::LOG_BLUETOOTH);
    }

    //
    scan_count++;

}

void CBluetoothRk::slot_this_ConnectDevice(const QString _addr, stBtDevInfo *_dev_info)
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    enBtProtocol bt_protocol = _dev_info->protocol;

    char addr_c[32] = {};
    strncpy(addr_c, _addr.toLatin1().data(), 32);
    logDebug(QString::asprintf("addr_c[] = \"%s\"", addr_c), CGlobal::LOG_BLUETOOTH);

    //
    int ret = -1;
    //ret = rk_bt_pair_by_addr(addr_c);
    //qDebug() << "rk_bt_pair_by_addr() -> " << ret;

    if (btProtocol_SPP == bt_protocol) {
        ret = rk_bt_spp_connect(addr_c);
        qDebug() << "rk_bt_spp_connect(" << addr_c << ") -> " << ret;
    } else if (btProtocol_BleClient == bt_protocol) {
        ret = rk_ble_client_connect(addr_c);
        qDebug() << "rk_ble_client_connect(" << addr_c << ") -> " << ret;
    }

    bool succ = (0 == ret);
    if (succ) {
        addrConnecting = _addr;
        isConnecting = true;

        uuidWritable[0] = 0;

        // 延时清掉正在连接的 addr，防止连接失败而一直存在
        timerConnectTimeout->start(CONNECTION_TIMEOUT);
    } else {
        QString msg = "connection failed!";
        logDebug(QString(__PRETTY_FUNCTION__) + ": " + msg, CGlobal::LOG_BLUETOOTH);
    }

}

void CBluetoothRk::slot_this_StopSearching()
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    int ret = rk_bt_cancel_discovery();
    qDebug() << "CBluetoothRk::stopSearching(): rk_bt_cancel_discovery() -> " << ret;
    bool succ = (0 == ret);

    timerSearchTimeLimit->stop();
    //isSearching = false;

    //emit sigSearchEnd();

    if (!succ) {
        QString msg = "stop search failed!";
        logDebug(QString(__PRETTY_FUNCTION__) + ": " + msg, CGlobal::LOG_BLUETOOTH);
    }

}

void CBluetoothRk::slot_this_DisconnectBt(const QString _addr, stBtDevInfo *_dev_info)
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    enBtProtocol dev_type = _dev_info->protocol;

    char addr_c[32] = {0};
    //memset(addr_c, 0, 32);
    strncpy(addr_c, _addr.toLatin1().data(), 32);
    logDebug(QString::asprintf("CBluetoothRk::disconnectBt(): addr_c = \"%s\"", addr_c), CGlobal::LOG_BLUETOOTH);

    int ret = -1;
    if (btProtocol_SPP == dev_type) {
        ret = rk_bt_spp_disconnect(addr_c);
        qDebug() << "rk_bt_spp_disconnect() -> " << ret;
    } else if (btProtocol_BleServer == dev_type) {
        ret = rk_ble_disconnect();
        qDebug() << "rk_ble_disconnect() -> " << ret;
    } else if (btProtocol_BleClient == dev_type) {
        ret = rk_ble_client_disconnect(addr_c);
        qDebug() << "rk_ble_client_disconnect() -> " << ret;
    }

    Util::waitMs(20);
    ret = rk_bt_unpair_by_addr(addr_c);
    qDebug() << "rk_bt_unpair_by_addr() -> " << ret;

    //
    bool succ = (0 == ret);
    if (!succ) {
        logDebug(QString("CBluetoothRk::disconnectBt(): rk_bt_spp_disconnect() -> %1, failed!").arg(ret), CGlobal::LOG_BLUETOOTH);
    } else {
        // TODO: BLE 由于 rk 库的 bug 无法收到断连消息，先在这里发送。待完善
        if (btProtocol_BleClient == dev_type) {
            //emit_ConnStateChanged(false, 0, addrConnected, "", btProtocol_BleClient);
        }
    }

}

// 搜索超时定时器超时事件
void CBluetoothRk::slot_timerSearchTimeLimit_timeout()      // TODO: 这个合理吗？即使必要，也应放到蓝牙模块内部？实测发现刚启动时好像搜索时间比较长？
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    // 若还处于搜索状态，停止搜索
    if (isSearching) {
        QString msg;
        stopSearching(msg);
    }

    //
    //timerSearchTimeLimit->stop();
}

// “正在连接的蓝牙 address 清除”定时器定时事件
void CBluetoothRk::slot_timerConnectTimeout_timeout()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_BLUETOOTH);

    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    if (addrConnecting.length() > 0) {
        logWarning("CBluetoothRk::slot_timerConnectTimeout_timeout(): connection timeout!", CGlobal::LOG_BLUETOOTH);

        //
        addrConnecting = "";
        isConnecting = false;

        //
        emit sigConnTimeout();
    }

    //
    //timerConnectTimeout->stop();
}

//
void CBluetoothRk::doOnApiConnStateChanged(bool _connected, int _conn_id, QString _addr, QString _name, enBtProtocol _bt_protocol)
{
    logDebug((QString(__PRETTY_FUNCTION__) + " into , addr = %1, name = %2").arg(_addr).arg(_name), CGlobal::LOG_BLUETOOTH);

    //
    if (_connected) {
        this->addConnectedDev(_addr, _name, _bt_protocol);
    }

    //
    if (btProtocol_SPP == _bt_protocol) {
        getBtPrinter()->setIsConnected(_connected);       /* SPP 只用于打印 */
    } else if (btProtocol_BleServer == _bt_protocol) {
        getBtDatatrans()->setIsConnected(_connected);     /* BLE Server 只用于数据传输，数据传输仅有被动连接 */
    } else if (btProtocol_BleClient == _bt_protocol) {
        getBtPrinter()->setIsConnected(_connected);
        if (_connected) {
            getBleClientWritableUuid(addrConnected);
        }
    }

    //
    enBtDevType dev_type = ((btProtocol_SPP == _bt_protocol || btProtocol_BleClient == _bt_protocol) ? btDevType_Printer : btDevType_Datatrans);

    // 暂时根据协议类型设定 connId
    if (_connected) {
        if (btDevType_Printer == dev_type) {
            btPrinter->connId = 0;                          // TODO: 应动态自增？
        } else if (btDevType_Datatrans == dev_type) {
            btDatatrans->connId = 1;
        }
    }

    //
    emit sigConnStateChanged(_connected, _conn_id, _addr, "", dev_type);

    //
    if (!_connected) {
        this->removeConnectedDev(_addr, _bt_protocol);

        //
        addrConnected = "";
    }

}

// 查询蓝牙是否打开
bool CBluetoothRk::getIsOpened()
{
    return isOpened;
}

bool CBluetoothRk::getIsSearched()
{
    return isSearched;
}

// 供外部获得地址
QString CBluetoothRk::getAddr()
{
    return addrSelf;
}

QString CBluetoothRk::getAddrBle()
{
    return getAddrBleServerByApi();
}

void CBluetoothRk::setAddrBle(QByteArray _addr_ble)
{
    std::reverse(_addr_ble.begin(), _addr_ble.end());       /* 调试发现这里的字节顺序和在蓝牙调试助手里搜到的 address 的字节顺序是相反的 */
    m_addrBle = _addr_ble;
}

/// ============================================================================================================
/// class CRkBtTest

CBtRkTest::CBtRkTest(QObject *parent, CBluetoothIntf *_bt_intf) : QObject(parent)
{
    if (!_bt_intf) {
        btRk = CBluetoothRk::getInstanceRkBt();
    } else {
        btRk = dynamic_cast<CBluetoothRk *>(_bt_intf);
    }
    //this->moveToThread(btRk->btThread);

    //
    qRegisterMetaType<enBtProtocol>("enBtProtocol");
    qRegisterMetaType<enBtDevType>("enBtDevType");

}

CBluetoothRk *CBtRkTest::getBtRk()
{
    return btRk;
}

QString CBtRkTest::getAddrByApi()
{
    return btRk->getAddrByApi();
}

QString CBtRkTest::getAddrBleServerByApi()
{
    return btRk->getAddrBleServerByApi();
}

void CBtRkTest::slot_initBt()
{
    btRk->initBt();
}

void CBtRkTest::slot_uninitBt()
{
    btRk->uninitBt();
}

void CBtRkTest::slot_openSpp()
{
    btRk->openSpp();
}

void CBtRkTest::slot_closeSpp()
{
    btRk->closeSpp();
}

void CBtRkTest::slot_openBleServer()
{
    btRk->openBleServer();
}

void CBtRkTest::slot_closeBleServer()
{
    btRk->closeBleServer();
}

void CBtRkTest::slot_openBleClient()
{
    btRk->openBleClient();
}

void CBtRkTest::slot_closeBleClient()
{
    btRk->closeBleClient();
}

