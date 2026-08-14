#include "bluetoothserial.h"

//#include <QEventLoop>           // 用 QEventLoop 使 QSerialPort 脱离 UI 线程？
//#include <QSerialPortInfo>      // TODO: 查询并检查可用串口？

#include "util-common.h"
#include "global.h"

#include <QDebug>

// 初始化之后的冻结时间（延时之后才允许后续操作）  /* 据富联芯说法，调用蓝牙 init() 后，会开关蓝牙电源一次，然后 sleep 4s，所以上层应用应至少等待 5s */
#define DELAY_MS            6000

//
const QString BT_NAME_SUFFIX_SPP    = "-SPP";       // SPP 设备名后缀
const QString BT_NAME_SUFFIX_BLE    = "-BLE";       // BLE 设备名后缀
const QString BT_NAME_SUFFIX_SHOW   = "-SPP/BLE";   // 显示设备名后缀

//
CBluetoothSerial::CBluetoothSerial(QObject *parent) : CBluetoothIntf(parent)
{
    // ASSERT: parent == null_ptr   /* 要 QObject::moveToThread() 成功，this 不能有 parent ？见该函数的帮助。 */
    //if (parent)
    //    logWarning("CBluetoothSerial::CBluetooth(): parent should be NULL!", CGlobal::LOG_BLUETOOTH);

    // 串口
    serialPort = new CSerialPort();
    QObject::connect(serialPort, &CSerialPort::sigDataReceived, this, &CBluetoothSerial::slot_serialPort_DataReceived, Qt::QueuedConnection);

    const QString port_name = QString(G_COM_BLUETOOTH);

    stSerialPortCfg serial_cfg;
    serial_cfg.portName     = port_name;                        // 端口路径
    serial_cfg.baudRate     = QSerialPort::Baud9600;            // 波特率9600      // TODO: 提高波特率？九辰协议，距离消息的传输频率较高时，对带宽的要求超过了这个波特率的速度上限
    serial_cfg.dataBits     = QSerialPort::Data8;               // 8数据位
    serial_cfg.parity       = QSerialPort::NoParity;            // 无奇偶校验
    serial_cfg.stopBits     = QSerialPort::OneStop;             // 1停止位
    serial_cfg.flowControl  = QSerialPort::NoFlowControl;       // 无流控

    serialPort->setSerialPortCfg(serial_cfg);

    // 检查是否 StartOK 定时器
    timerCheckStartOk = new QTimer();
    timerCheckStartOk->setSingleShot(true);
    QObject::connect(timerCheckStartOk, &QTimer::timeout, this, &CBluetoothSerial::slot_timerCheckStartOk_timeout, Qt::QueuedConnection);

    // 检查 addrConnecting 定时器
    timerConnectingMacClear = new QTimer();
    timerConnectingMacClear->setSingleShot(true);
    QObject::connect(timerConnectingMacClear, &QTimer::timeout, this, &CBluetoothSerial::slot_timerConnectingMacClear_timeout, Qt::QueuedConnection);

    // 搜索超时定时器
    timerSearchTimeLimit = new QTimer();
    timerSearchTimeLimit->setSingleShot(true);
    QObject::connect(timerSearchTimeLimit, &QTimer::timeout, this, &CBluetoothSerial::slot_timerSearchTimeLimit_timeout, Qt::QueuedConnection);

    // 时间值初始化   /* 否则在调用 start() 之前，它处于 invalid 状态，elapsed() 值将一直是 0。 */
    timeLastOpen.start();

    //
    writeLocker = new QMutex();

    // 串口工作线程
    workThread = new QThread();
    this->moveToThread(workThread);
    workThread->start();

}

CBluetoothSerial::~CBluetoothSerial()
{
    QString msg;
    setIsOpened(false, msg);

    if (serialPort) {
        delete serialPort;
        serialPort = Q_NULLPTR;
    }
    if (writeLocker) {
        delete writeLocker;
        writeLocker = Q_NULLPTR;
    }
}

int CBluetoothSerial::getDelayMs()
{
    return DELAY_MS;
}

// 设置蓝牙是否打开
/** 设计要点：
 *    1、串口蓝牙没有打开和关闭的状态，只有是否上电的状态。
 *    2、目前（2022-04-15）硬件上还未实现 “EN” 脚的控制，所以蓝牙连接上之后，无法主动的通过指令断开。如果实现了，可以在关闭时断开连接。
 */
bool CBluetoothSerial::setIsOpened(bool _is_open, QString &_msg)
{
    if (_is_open == isOpened) {
        logWarning(QString("CBluetoothSerial::setIsOpened(): ") + (_is_open ? "had opened!" : "had closed!"), CGlobal::LOG_BLUETOOTH);
        return true;
    }

    // 限制开关时间间隔
    if (!setIsOpenedCheck(_is_open, _msg)) {
        return false;
    }
    timeLastOpen.start();

    //
    Util::waitMs(500);      // 硬件加电后，等待 500ms

    // 刚加电的延时
    //if (_is_open && !isInited)          // TODO: 此时电源一般是刚打开，所以可能需要延时？
    //    Util::waitMs(100);

    // 确保串口已打开
    serialPort->setIsOpened(true);

    //
    isOpened = _is_open;

    // 状态变量重置
    //if (_is_open)
    {
        isStartOK = false;
        isInited = false;               // 每次打开都应该重新初始化
        isSearching = false;
        isSearched = false;
        isConnecting = false;
    }

    //
    if (_is_open) {
        // 延时检查是否启动成功，否则软复位
        timerCheckStartOk->setSingleShot(true);
        timerCheckStartOk->start(DELAY_MS);
    } else {
        btPrinter->setIsConnected(false);
        btDatatrans->setIsConnected(false);
    }

    //
    return true;
}

// 检查当前是否可以执行【打开/关闭】操作
bool CBluetoothSerial::setIsOpenedCheck(bool _is_open, QString &_msg)
{
    // 限制开关时间间隔
    if ((_is_open && timeLastOpen.elapsed() < 1000) || (!_is_open && timeLastOpen.elapsed() < DELAY_MS + 1000)) {
        _msg = tr("操作太频繁，请稍等。");    // "Operation too frequent, please wait."
        return false;
    }
    return true;
}

// 查询蓝牙是否打开
bool CBluetoothSerial::getIsOpened()
{
    return isOpened;
}

bool CBluetoothSerial::getIsSearching()
{
    return isSearching;
}

bool CBluetoothSerial::getIsSearched()
{
    return isSearched;
}

// 初始设置蓝牙设备
bool CBluetoothSerial::initConfig()           // TODO: 通过 write(), flush(), waitForReadyRead() ... 模式来实现这种问答式通信过程？
{
    //
    QString cmd;
    QString msg;

    //Util::waitMs(INTERVAL_CMD_MS);

    // 设置蓝牙广播名
    cmd = getAtCmdSetNameSpp(getNameSpp());
    writeAtCmd(cmd, msg);
    if (msg.length() > 0)
        logCritical(QString("CBluetoothSerial::initConfig(): send cmd set name spp failed: ") + msg);

    Util::waitMs(INTERVAL_CMD_MS);

    cmd = getAtCmdSetNameBle(getNameBle());
    writeAtCmd(cmd, msg);
    if (msg.length() > 0)
        logCritical(QString("CBluetoothSerial::initConfig(): send cmd set name ble failed: ") + msg);

    Util::waitMs(INTERVAL_CMD_MS);

    // 查询 MAC 地址
    cmd = getAtCmdAddr();
    writeAtCmd(cmd, msg);
    if (msg.length() > 0)
        logCritical(QString("CBluetoothSerial::initConfig(): send cmd get mac failed: ") + msg);

//    // 设置 MTU
//    Util::waitMs(sleep_ms);
//    cmd = getAtCmdSetMtu(mtu);
//    writeAtCmd(cmd, msg);
//    if (msg.length() > 0)
//        logCritical(QString("CBluetoothSerial::initConfig(): send cmd set mtu failed: ") + msg);
//    Util::waitMs(sleep_ms_normal);

    //
    isInited = true;

    //
    return true;
}

// 串口读数据槽函数
void CBluetoothSerial::slot_serialPort_DataReceived(QByteArray _data)
{
    logDebug("CBluetoothSerial::slot_serialPort_DataReceived(): recieved data: \"" + _data.toHex() + "\"", CGlobal::LOG_BLUETOOTH);

    // TODO: rk3568 主板，为什么断电时会收到一个"\0"字节？
    if ((char)_data[0] == 0) {
        logWarning("CBluetoothIntf::slot_serialPort_readyRead(): received \\0 bytes ?!", CGlobal::LOG_BLUETOOTH);

        // 去掉 \0 字节
        for (int i = _data.length() - 1; i >= 0; i--) {
            if (_data.at(i) == 0) {
                _data.remove(i, 1);
            }
        }
    }

    if (_data.length() == 0) {
        logDebug("CBluetoothSerial::slot_serialPort_DataReceived(): data len = 0", CGlobal::LOG_BLUETOOTH);
        return;
    }

    //
    buffer.append(_data);
    //logDebug("CBluetoothSerial::slotDataReady(): buffer: \"" + buffer.toHex() + "\"", CGlobal::LOG_BLUETOOTH);

    // 判断是 AT 消息还是蓝牙数据，分别调用不同的函数来处理
    /***
    // TODO: 如果没有 STAT 脚，连接状态下也可能收到断连 AT 消息？（断连状态下也可能收到连接消息，但此时 STAT 脚状态与其它 AT 消息一致）
    // TODO: 如果发生串口堵塞的情况，那么不管有没有状态脚，两种数据都混到一起了，无法区分？
    //       所以这里没法做到真正严谨？某些异常情况下的数据，只能放弃？或容许异常情况下数据种类判断错误？
    */
    bool is_at_msg;
    if (!getIsConnected()) {        // 若未连接，则必为 AT 消息
        is_at_msg = true;
    } else {                        // TODO: 连接后，收到的所有蓝牙数据都要进行两次正则表达式匹配，效率低？
        is_at_msg = false;

        // 若已连接，且有 AT 断连消息，则视为 AT 消息，否则视为蓝牙数据
        if (hasDisconnectedMsg(buffer))
            is_at_msg = true;
        // 若已连接，且有 AT 连接消息，则视为 AT 消息，否则视为蓝牙数据
        if (hasConnectedMsg(buffer))
            is_at_msg = true;
    }

    //
    if (is_at_msg) {
        processAtMsg();
    } else {
        emit sigLog(QString("\nreceived:\n") + _data);
        processBtData();
    }

    //
    //buffer.clear();   // TODO: 异常情况下清空缓冲区？
}

// 检查是否有 AT 断连消息
bool CBluetoothSerial::hasDisconnectedMsg(QByteArray &_data)
{
    /* eg. SPP: "+DISCONNECTED=1\r\n", BLE: "+DISCONNECTED-ID=0\r\n" */

    // TODO: 模式匹配？
    QRegExp reg_exp("^\\+DISCONNECTED(-ID)?=[0-7]{1}\\r\\n");
    return (reg_exp.indexIn(_data) >= 0);

    //
    //int idx = _data.indexOf("+DISCONNECTED");
    //if (0 == idx || (idx >=2 && _data[idx-2] == '\r' && _data[idx-1] == '\n'))
    //    return true;
    //else
    //    return false;
}

// 检查是否有 AT 连接消息
bool CBluetoothSerial::hasConnectedMsg(QByteArray &_data)
{
    /* eg. SPP: "+CONNECTED>>0x21040150256C,1\r\n", BLE: "+CONNECTED-ID=0\r\n" */

    // TODO: 模式匹配？
    QRegExp reg_exp("^\\+CONNECTED(>>0x[0-9A-Fa-f]{12},|-ID=)[0-7]{1}\\r\\n");
    return (reg_exp.indexIn(_data) >= 0);

    //
    //int idx = _data.indexOf("+CONNECTED");
    //if (0 == idx || (idx >=2 && _data[idx-2] == '\r' && _data[idx-1] == '\n'))
    //    return true;
    //else
    //    return false;
}

// 处理 AT 消息
bool CBluetoothSerial::processAtMsg()
{
    logDebug("CBluetoothSerial::processAtMsg(): into ... buffer=\"" + buffer.toHex() + "\"", CGlobal::LOG_BLUETOOTH);

    QByteArray line_str;
    do {
        // 将 AT 消息拆分为多个行
        Util::readLine(buffer, line_str);

        if (line_str.length() > 0) {
            // 触发信号
            doOnReceivedAtMsg(line_str);

            // 清掉已触发的消息行
            buffer.remove(0, line_str.length());        // TODO: 处理失败的消息？
        }
    } while (line_str.length() > 0);

    return true;
}

// 处理蓝牙数据
bool CBluetoothSerial::processBtData()
{
    logDebug("CBluetoothSerial::processBtData(): into ... ", CGlobal::LOG_BLUETOOTH);

    // 蓝牙数据接收，一律认为来自数据通信设备
    btDatatrans->pushReceivedData(buffer);

    // 蓝牙所接收数据缓冲变量，外传后即可清空
    buffer.clear();

    //
    return true;
}

// 处理 AT 消息行
bool CBluetoothSerial::processAtMsgLine(QByteArray _msg_line)
{
    logDebug(QString("CBluetoothSerial::processAtMsgLine(): data = ") + _msg_line, CGlobal::LOG_BLUETOOTH);

    // "OK" 指令
    if (_msg_line == "OK\r\n") {
        isReceivedAtOk = 1;
        isReceivedAtResp = true;

        return true;
    } else {
        int idx_plus = _msg_line.indexOf('+');
        if (idx_plus > 0) {
            _msg_line.remove(0, idx_plus);
        }

        if (_msg_line[0] != '+') {       /* 除了“OK”指令，其它指令都以“+”开头 */
            logCritical("CBluetoothSerial::processAtMsgLine(): first char is not '+'!", CGlobal::LOG_BLUETOOTH);
            // TODO: 如果串口通信不畅，AT 消息可能与前面的蓝牙通信数据混在一起？

            return false;
        }
    }

    //
    QString key = "";
    int idx_cmd = -1;

    do {
        // “+OK” 消息
        /* eg.: "+OK\r\n" */
        key = "+OK";
        idx_cmd = _msg_line.indexOf(key);
        if (idx_cmd >= 0) {
            isReceivedAtOk = 1;

            //
            break;
        }

        // 设备启动成功
        /* eg.: "+START=OK\r\n" */
        key = "+START=OK";
        idx_cmd = _msg_line.indexOf(key);
        if (idx_cmd >= 0) {
            // ASSERT: isOpened = true
            if (!isOpened)
            {
                logCritical("CBluetoothSerial::processAtMsgLine(): ASSERT failed! isOpened should be true", CGlobal::LOG_BLUETOOTH);
            }
            // ASSERT: isSearched = false && isSearching = false
            if (!(!isSearched && !isSearching))
            {
                logCritical("CBluetoothSerial::processAtMsgLine(): ASSERT failed! isSearched & isSearching should be false", CGlobal::LOG_BLUETOOTH);
            }

            //
            if (isOpened) {
                if (!isStartOK) {               // TODO: (2022-05-26)测试中遇到过收到连续的几万条 “+START=OK” 的情况？怎么回事？好像是一次性收到的，是缓存？
                    isStartOK = true;
                    doOnStartOk();
                } else {
                    logWarning("CBluetoothSerial::processAtMsgLine(): isStartOK is already true!", CGlobal::LOG_BLUETOOTH);
                }
            }

            //
            break;
        }

        // 搜索到蓝牙设备
        /* eg.: "+DEV:2=210406662545,2105E-017-SPP\r\n" */
        key = "+DEV:";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0)
        {
            QString id_str = "";    // TODO: 这个序号好像没什么用？
            QString mac_str = "";
            QString name_str = "";

            bool found = false;
            QString str1, str2;
            if (Util::getSeparatedStr(_msg_line, ":", "\r", str1, idx_cmd)) {
                if (Util::splitStr(str1, "=", id_str, str2, idx_cmd)) {
                    if (Util::splitStr(str2, ",", mac_str, name_str, idx_cmd) >= 0) {
                        found = true;
                    }
                }
            }

            if (found) {
                stBtDevInfo dev_info;
                dev_info.protocol = btProtocol_SPP;
                dev_info.addr = mac_str;
                dev_info.name = name_str;

                devsFound.append(dev_info);

                //
                emit sigFoundDevice(name_str, mac_str);
            } else {
                logCritical("CBluetoothSerial::processAtMsgLine(): parse +DEV sentence error!", CGlobal::LOG_BLUETOOTH);
            }

            //
            break;
        }

        // 查询到 SPP 广播名
        /* eg.: "+NAME:TEST0-SPP\r\n" */
        key = "+NAME:";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0)
        {
            QString name = "";
            if (Util::getSeparatedStr(_msg_line, ":", "\r", name, idx_cmd)) {
                //emit sig(name);
                logDebug(QString("GetNameSpp (%1)").arg(name), CGlobal::LOG_BLUETOOTH);
            }

            //
            break;
        }

        // 查询到 MAC 地址
        /* eg.: "+LADDR:21040150256C\r\n" */
        key = "+LADDR:";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0) {
            QString mac = "";
            if (Util::getSeparatedStr(_msg_line, ":", "\r", mac, idx_cmd)) {
                this->addrSelf = mac;
                //emit sigGetMac(mac);
                logDebug(QString("GetMac (%1)").arg(mac), CGlobal::LOG_BLUETOOTH);
            }

            //
            break;
        }

        // 蓝牙搜索结束
        /* eg.: "+SINQ\r\n" */
        key = "+SINQ";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0)
        {
            isSearching = false;
            emit sigSearchEnd();

            //
            break;
        }

        // 蓝牙连接成功（SPP）
        /* eg.: "+CONNECTED>>0x21040150256C,1\r\n" */
        key = "+CONNECTED>>";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0)
        {
            int conn_id = -1;
            QString mac = "";

            QString str = _msg_line.replace(key + "0x", "");
            str = str.replace("\r\n", "");
            QString str_id;
            if (Util::splitStr(str, ",", mac, str_id)) {
                conn_id = str_id.toInt();
            }

            if (conn_id >= 0)
                processConnected(conn_id, mac, btProtocol_SPP);

            //
            break;
        }

        // 蓝牙连接成功（BLE）
        /* eg.: "+CONNECTED-ID=0\r\n" */
        key = "+CONNECTED-ID=";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0) {
            int conn_id = -1;

            QString str;
            if (Util::getSeparatedStr(_msg_line, "=", "\r", str, idx_cmd))
                conn_id = str.toInt();

            if (conn_id >= 0)
                processConnected(conn_id, "", btProtocol_BleServer);

            //
            break;
        }

        // 蓝牙连接已断开（SPP）
        /* eg.: "+DISCONNECTED=1\r\n" */
        key = "+DISCONNECTED=";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0)
        {
            QString str_conn_id;
            Util::getSeparatedStr(_msg_line, "=", "\r", str_conn_id, idx_cmd);
            int conn_id = str_conn_id.toInt();
            processDisconnected(conn_id, btProtocol_SPP);

            //
            break;
        }

        // 蓝牙连接已断开（BLE）
        /* eg.: "+DISCONNECTED-ID=0\r\n" */
        key = "+DISCONNECTED-ID=";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0)
        {
            QString str_conn_id;
            Util::getSeparatedStr(_msg_line, "=", "\r", str_conn_id, idx_cmd);
            int conn_id = str_conn_id.toInt();
            processDisconnected(conn_id, btProtocol_BleServer);

            //
            break;
        }

        // 蓝牙连接超时（实测JDY-34 发送 CONA 指令后，若 MAC address 对应设备已关闭，约 15s 后回复超时消息）
        /* eg.: "+CONN TIMEOUT\r\n" */
        key = "+CONN TIMEOUT";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0)
        {
            //
            addrConnecting = "";
            isConnecting = false;

            //
            emit sigConnTimeout();

            //
            break;
        }

        // 错误消息
        /* eg.: "+ERR=1001\r\n" */
        key = "+ERR=";
        idx_cmd = _msg_line.indexOf(key);
        if(idx_cmd >= 0)
        {
            //
            logCritical(QString("CBluetoothSerial::processAtMsgLine(): received +ERR msg!") + _msg_line, CGlobal::LOG_BLUETOOTH);

            //
            break;
        }

    } while (false);

    //
    isReceivedAtResp = (idx_cmd >= 0);

    //
    if (-1 == idx_cmd) {
        logCritical(QString("CBluetoothSerial::processAtMsgLine(): can't recognize data: ") + _msg_line, CGlobal::LOG_BLUETOOTH);
    }

    //
    return isReceivedAtResp;
}

// 处理蓝牙连接消息
void CBluetoothSerial::processConnected(int _conn_id, QString _addr, enBtProtocol _bt_protocol)
{
    // 检查：设备号应该不在已连接设备中
    // TODO:

    // 判断本机是否主机
    bool is_master = ((_addr.length() > 0) && (_addr == addrConnecting));
    if (is_master) {
        addrConnecting = "";
        isConnecting = false;
    }

    // 构造设备信息
    stBtDevInfo dev_info;
    bool dev_found = false;
    if (_addr.length() > 0) {
        foreach (auto info, devsFound) {
            if (info.addr == _addr) {
                dev_info = info;
                dev_found = true;
                break;
            }
        }
    }
    if (!dev_found) {
        dev_info.addr = _addr;
        dev_info.name = "(unknown)";
        dev_info.protocol = _bt_protocol;

        // ASSERT: 如果在已发现列表中没有找到，那么应该是 BLE Server
        if (btProtocol_BleServer != _bt_protocol) {
            logWarning("CBluetoothSerial::processConnected(): ASSERT failed! SPP device no record?", CGlobal::LOG_BLUETOOTH);
        }
    }

    // ASSERT: 如果本机是主机，那么在 devsFound 里应能找到
    if (is_master && !dev_found) {
        logWarning("CBluetoothSerial::processConnected(): ASSERT failed! Is master but not in devsFound?", CGlobal::LOG_BLUETOOTH);
    }
    // ASSERT: 如果本机是主机，那么传入参数中协议应该是 SPP
    if (is_master) {
        if (btProtocol_SPP != _bt_protocol)
            logWarning("CBluetoothSerial::processConnected(): ASSERT failed! Is master but _bt_protocol not SPP?", CGlobal::LOG_BLUETOOTH);
    }
    // ASSERT: 如果传入参数中协议是 BLE，那么本机应该是 BLE Server
    if (btProtocol_BleServer == _bt_protocol) {
        if (is_master)
            logWarning("CBluetoothSerial::processConnected(): ASSERT failed! _bt_protocol is BLE but local is master?", CGlobal::LOG_BLUETOOTH);
    }

    // 设置连接对象
    CBtConnection *conn_prebuilded = Q_NULLPTR;    // 匹配的预建连接
    conn_prebuilded = is_master ? btPrinter : btDatatrans;     /* 目前(2021-10-13)，本机主动连接的设备只有蓝牙打印机，而被动连接的只有信息通信设备 */

    conn_prebuilded->connId = _conn_id;

    // 多重连接的检查：不允许在后的连接替换了在前的连接。即同一类连接，若已存在，则屏蔽后入的连接。
    if (conn_prebuilded->getIsConnected()) {
        logWarning("CBluetoothSerial::processConnected(): multiple connection!", CGlobal::LOG_BLUETOOTH);

        // 屏蔽该连接            // TODO: 目前所用蓝牙串口模块，只能设置 sendid 在发送方面屏蔽，却屏蔽不了数据接收。怎么处理？立即 AT+DISC 它？
        // TODO: 在 pwrc 脚的应用实现之前，存在连接后修改 sendid 无法实现

    }
    // TODO: 多重连接处理逻辑的完善？
    // TODO: 设置 SENDID，避免后面被动连接后发送通道被修改，导致数据无法发送给原连接设备？（JAD-34实测存在此问题）

    sendId = _conn_id;

    //
    conn_prebuilded->devInfo = dev_info;
    conn_prebuilded->isMaster = is_master;
    conn_prebuilded->devType = (is_master ? btDevType_Printer : btDevType_Datatrans);

    conn_prebuilded->setIsConnected(true);     // 触发蓝牙连接的连接改变事件

    // 触发蓝牙连接事件
    emit sigConnStateChanged(true, _conn_id, _addr, "", conn_prebuilded->devType);

    // TODO: 连接状态改变时检查并清空串口接收数据缓存变量？


}

// 处理蓝牙断连消息
void CBluetoothSerial::processDisconnected(int _conn_id, enBtProtocol _bt_protocol)
{
    // 剔除连接信息列表的 item

    // 以 connId 为 key 搜索好像不够，因为实测发现可能收到 mac 相同的多个连接信息，所以还要以 mac 为 key 检查是否有重复的连接信息


    // TODO: 如果还存在连接，则必须设置 sendid，否则无法发送数据

//    if (hasPwrcPin) {       // TODO: 目前没有 PWRC 脚，怎么处理？
//        setPwrcPinVal(0);

//        bool succ = writeAtCmd(getAtCmdSetSendId(matched_conn->connId));
//        if (!succ)
//            logWarning("CBluetoothSerial::processConnected(): set sendid failed!", CGlobal::LOG_BLUETOOTH);

//        setPwrcPinVal(1);
//    }

    CBtConnection *conn_prebuilded = Q_NULLPTR;

    if (_conn_id == btPrinter->connId)
        conn_prebuilded = btPrinter;
    else if (_conn_id == btDatatrans->connId)
        conn_prebuilded = btDatatrans;

    QString addr = "";
    enBtDevType dev_type = btDevType_Unknown;
    if (conn_prebuilded) {
        addr = conn_prebuilded->devInfo.addr;
        dev_type = conn_prebuilded->devType;

        conn_prebuilded->setIsConnected(false);
    }

    //
    emit sigConnStateChanged(false, _conn_id, addr, "", dev_type);
}

// 查找蓝牙连接
CBtConnection *CBluetoothSerial::findConn(int _conn_id)
{
    foreach (CBtConnection *conn, conns) {
        if (_conn_id == conn->connId)
            return conn;
    }
    return Q_NULLPTR;
}

// 发送 AT 指令
//bool CBluetoothSerial::writeAtCmd(QString _cmd)
//{
//    QString msg = "";
//    bool succ = writeAtCmd(_cmd, msg);
//    if (!succ) {
//        logCritical(QString("CBluetoothSerial::writeAtCmd(): failed, msg=") + msg, CGlobal::LOG_BLUETOOTH);
//    }
//    return succ;
//}

// 发送 AT 指令
bool CBluetoothSerial::writeAtCmd(QString _cmd, QString &_msg)
{
    logCritical(QString("CBluetoothSerial::writeAtCmd(): writing at cmd: ") + _cmd, CGlobal::LOG_BLUETOOTH);

    //
    if (getIsConnected() && !hasPwrcPin) {
        _msg = tr("蓝牙已连接，请关闭蓝牙后再打开。");  // "Bluetooth is connected.\n Please close it then open again."

        return false;
    }

    //
    writeSerialData(_cmd.toLatin1(), true, -1, 200);

    //
    return true;
}

// 搜索蓝牙设备
bool CBluetoothSerial::searchDevices(QString &_msg)
{
    if (!isOpened) {
        logWarning("CBluetoothSerial::searchDevices(): not opened!", CGlobal::LOG_BLUETOOTH);
        _msg = tr("请先打开蓝牙。");   // "Please open bluetooth first."
        return false;
    }

    if (!isInited) {
        logWarning("CBluetoothSerial::searchDevices(): not inited!", CGlobal::LOG_BLUETOOTH);
        _msg = tr("初始化未完成，请稍侯。");   // "Initialization not finished, please wait."
        return false;
    }

    if (!isStartOK) {
        _msg = tr("设备正在启动，请稍后。\n您可以尝试重新启动蓝牙。"); // "Device is starting, please wait.\nYou may try to reboot the Bluetooth device."
        return false;
    }

    // 搜索中，禁止再次搜索
    if (isSearching) {
        logWarning("CBluetoothSerial::searchDevices(): is searching!", CGlobal::LOG_BLUETOOTH);
        _msg = tr("搜索中，请稍侯。");  // "Is searching, please wait."
        return false;
    }

    // 连接中，禁止搜索
    if (isConnecting) {
        logWarning("CBluetoothSerial::searchDevices(): is connecting!", CGlobal::LOG_BLUETOOTH);
        _msg = tr("连接中，请稍侯。");  // "Is connecting, please wait."
        return false;
    }

    //
    bool succ = writeAtCmd(getAtCmdSearch(), _msg);
    if (succ) {
        isSearched = true;
        isSearching = true;
    } else {
        logWarning("CBluetoothSerial::searchDevices(): Failed to write command!", CGlobal::LOG_BLUETOOTH);
        //_msg = isChinese ? "写入命令失败！" : "Failed to write command.";
    }

    //
    if (succ) {
        timerSearchTimeLimit->start(20000);
    }

    //
    return succ;
}

// 停止搜索
bool CBluetoothSerial::stopSearching(QString &_msg)
{
    if (!isSearching)
        return true;

    bool succ = writeAtCmd(getAtCmdStopSearching(), _msg);

    emit sigSearchEnd();

    return succ;
}

// 设置 PWRC GPIO 值
bool CBluetoothSerial::setPwrcPinVal(int _val)
{
    // TODO:
    return _val == -1;
}

// 获取 PWRC GPIO 值
int CBluetoothSerial::getPwrcPinVal()
{
    // TODO:
    return true;
}

// 获取 STAT GPIO 值
int CBluetoothSerial::getStatPinVal()
{
    // TODO:
    return false;
}

// 判断是否已连接
bool CBluetoothSerial::getIsConnected()
{
//    if (hasStatPin) {
//        return getStatPinVal();
//    }
//    else      /* 虽然以 STAT 脚的值判断是否有连接可能更可靠，但是还是应该以程序能识别的连接来判断，否则影响后面的通信。 */
    {
        return (btPrinter->isConnected || btDatatrans->isConnected);
    }
}

// 供外部获得 MAC 地址
QString CBluetoothSerial::getAddr()
{
    return addrSelf;
}

// 获得【设置 SPP 模式广播名】的AT指令
QString CBluetoothSerial::getAtCmdSetNameSpp(QString _name)
{
    return QString("AT+NAME") + _name + "\r\n";
}

// 获得【设置 BLE 模式广播名】的AT指令
QString CBluetoothSerial::getAtCmdSetNameBle(QString _name)
{
    return QString("AT+NAMB") + _name + "\r\n";
}

// 获得【设置 MTU】的AT指令
QString CBluetoothSerial::getAtCmdSetMtu(int _val)
{
    return QString("AT+MTU") + QString::number(_val) + "\r\n";
}

// 获得【查询 SPP 模式广播名】的AT指令
QString CBluetoothSerial::getAtCmdNameSpp()
{
    return QString("AT+NAME") + "\r\n";
}

// 获得【查询 BLE 模式广播名】的AT指令
QString CBluetoothSerial::getAtCmdNameBle()
{
    return QString("AT+NAMB") + "\r\n";
}

// 获得【查询 MAC 地址】的AT指令
QString CBluetoothSerial::getAtCmdAddr()
{
    return QString("AT+LADDR") + "\r\n";
}

// 获得【搜索设备】的AT指令
QString CBluetoothSerial::getAtCmdSearch()
{
    return QString("AT+INQ") + "\r\n";
}

// 获得【停止搜索】的AT指令
QString CBluetoothSerial::getAtCmdStopSearching()
{
    return QString("AT+SINQ") + "\r\n";
}

// 获得【软复位模块】的AT指令
QString CBluetoothSerial::getAtCmdReset()
{
    return QString("AT+RESET") + "\r\n";
}

// 获得【连接指定 MAC address 的设备】的 AT 指令
QString CBluetoothSerial::getAtCmdConnect(QString _addr)
{
    return QString("AT+CONA") + _addr + "\r\n";
}

// 获得【设置 sendid】的AT指令
QString CBluetoothSerial::getAtCmdSetSendId(int _id)
{
    return QString("AT+SENDID") + QString::number(_id) + "\r\n";
}

// 写串口数据
bool CBluetoothSerial::writeSerialData(QByteArray _data, bool _is_cmd, int _conn_id, int _wait_write)
{
    //logDebug(QString::asprintf("CBluetoothSerial::writeSerialData(): currentThreadId = %lld", (qintptr)QThread::currentThreadId()), CGlobal::LOG_BLUETOOTH);

    //
    bool had_set_pwrc = false;
    if (_is_cmd) {
        // 设置 PWRC 引脚
        if (getIsConnected()) {
            if (hasPwrcPin) {
                // 拉低 PWRC 电位
                setPwrcPinVal(0);
                had_set_pwrc = true;
            } else {
                logWarning("CBluetoothSerial::writeSerialData(): isConnected but not hasPwrcPin, can't send AT Command.", CGlobal::LOG_BLUETOOTH);

                return false;
            }
        }
    } else {
        // 设置 SENDID
        if (_conn_id != sendId) {
            isReceivedAtOk = 0;
            QString msg;
            bool succ = writeAtCmd(getAtCmdSetSendId(_conn_id), msg);
            if (succ) {             // TODO: 若这个 SENDID 设置错误，将导致数据发送不出去！
                bool is_ok = Util::waitMs(500, &isReceivedAtOk, 1);

                if (is_ok)
                    sendId = _conn_id;
            } else {
                // TODO:

            }
        }
    }

    //
    bool succ = false;
    writeLocker->lock();        // TODO: 这有必要吗？QSerialPort::write() 应该是线程安全的？
    try {
        serialPort->write(_data);

        emit sigLog(_data);
    } catch (std::exception &ex) {
        logCritical(QString::asprintf("CBluetoothSerial::writeSerialData(): catch exception: %s", ex.what()), CGlobal::LOG_BLUETOOTH);
    } catch (...) {
        int err_no = errno;
        logCritical(QString::asprintf("CBluetoothSerial::writeSerialData(): catch unknown exception: no=%d, msg=%s", err_no, strerror(err_no)), CGlobal::LOG_BLUETOOTH);
    }
    writeLocker->unlock();

    //
    if (_is_cmd) {
        // 恢复 PWRC 引脚
        if (had_set_pwrc) {
            setPwrcPinVal(1);
        }
    }

    //
    return succ;
}

// 连接指定 MAC address 的设备
bool CBluetoothSerial::connectDevice(QString _addr, QString &_msg)
{
    if (btPrinter->getIsConnected() && !hasPwrcPin) {
        _msg = tr("已有连接存在，需重启蓝牙才能再连接。");    // "Connection existed, please reboot the bluetooth."
        return false;
    }

    //
    bool succ = writeAtCmd(getAtCmdConnect(_addr), _msg);
    if (succ) {
        addrConnecting = _addr;
        isConnecting = true;

        // 延时清掉正在连接的 mac address，防止连接失败而一直存在
        timerConnectingMacClear->start(10000);
    } else {
        //_msg = isChinese ? "写入命令失败！" : "Failed to write command.";
    }

    //
    return succ;
}

// 发送蓝牙数据
bool CBluetoothSerial::sendBtData(int _conn_id, QByteArray _data)
{
    if (!isOpened) {
        logWarning("CBluetoothSerial::sendBtData(): bluetooth not opened!", CGlobal::LOG_BLUETOOTH);
        return false;
    }

    //
    writeSerialData(_data, false, _conn_id, 200);

    //
    return true;
}

// 查询设备信息，若成功则返回 true，否则返回 false
bool CBluetoothSerial::getDevInfo(int _conn_id, stBtDevInfo &_dev_info)
{
    return false;
}

bool CBluetoothSerial::disconnectBt(QString _addr)
{
    return false;
}

bool CBluetoothSerial::setName(const QString &_name)
{
    if (!getIsConnected()) {
        name = _name;

        // TODO: 如何立即生效？
//        // 软复位
//        //const int sleep_ms_reset = 3500;    // reset 指令回复延时（实测大概2.5秒）
//        QString cmd = getAtCmdReset();
//        QString msg = "";
//        writeAtCmd(cmd, msg);
//        if (msg.length() > 0)
//            logger.critical(QString("CBluetoothSerial::slot_timerCheckStartOk_timeout(): send cmd reset failed: ") + msg);
//        //Util::waitMs(sleep_ms_normal);

        //
        return true;
    } else {
        return false;
    }
}

QString CBluetoothSerial::getName()
{
    return name;
}

QString CBluetoothSerial::getNameSpp()
{
    return name + BT_NAME_SUFFIX_SPP;
}

QString CBluetoothSerial::getNameBle()
{
    return name + BT_NAME_SUFFIX_BLE;
}

// 处理 AT 消息行接收槽函数
void CBluetoothSerial::doOnReceivedAtMsg(QByteArray _msg_line)
{
    bool succ = processAtMsgLine(_msg_line);
    if (!succ) {
        logWarning(QString("CBluetoothSerial::slotReceivedAtMsg(): process at msg faild, msg = \"") + _msg_line + "\"");
    }
}

// 蓝牙模块启动完成槽函数
void CBluetoothSerial::doOnStartOk()
{
    // 设备准备好后，先延时一段时间，再继续操作
    Util::waitMs(300);

    // 初始化设备
    if (!isInited) {
        bool succ = initConfig();
        if (!succ) {
            logCritical("CBluetoothSerial::slotStartOk(): init failed!", CGlobal::LOG_BLUETOOTH);
        }
    }

    // 触发 “设备准备好” 信号
    Util::waitMs(INTERVAL_CMD_MS);
    emit sigSetIsOpenedFinished(true);
}

// 蓝牙模块 START=OK 超时检查定时器定时事件
void CBluetoothSerial::slot_timerCheckStartOk_timeout()
{
    //
    if (!isStartOK) {       // 如果到一定时间后还未收到 START=OK 消息，则复位模块
        logWarning("CBluetoothSerial::slot_timerCheckStartOk_timeout(): starting timeout!", CGlobal::LOG_BLUETOOTH);

        // 软复位
        //const int sleep_ms_reset = 3500;    // reset 指令回复延时（实测大概2.5秒）
        QString cmd = getAtCmdReset();      // RESET 取消，
        QString msg = "";
        writeAtCmd(cmd, msg);
        if (msg.length() > 0) {
            logCritical(QString("CBluetoothSerial::slot_timerCheckStartOk_timeout(): send cmd reset failed: ") + msg);
            emit sigNotice(msg);
        }
        //Util::waitMs(sleep_ms_normal);
    }

    //
    timerCheckStartOk->stop();
}

// 搜索超时定时器超时事件
void CBluetoothSerial::slot_timerSearchTimeLimit_timeout()      // TODO: 这个合理吗？即使必要，也应放到蓝牙模块内部？实测发现刚启动时好像搜索时间比较长？
{
    // 若还处于搜索状态，停止搜索
    if (isSearching) {
        QString msg;
        stopSearching(msg);
    }

    //
    timerSearchTimeLimit->stop();
}

// “正在连接的蓝牙mac清除”定时器定时事件
void CBluetoothSerial::slot_timerConnectingMacClear_timeout()
{
    if (addrConnecting.length() > 0) {
        logWarning("CBluetoothSerial::slot_timerConnectingMacClear_timeout(): connection timeout!", CGlobal::LOG_BLUETOOTH);

        //
        addrConnecting = "";
        isConnecting = false;

        //
        emit sigConnTimeout();
    }

    //
    timerConnectingMacClear->stop();
}

