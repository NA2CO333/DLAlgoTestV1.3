#include "bluetoothintf.h"

#include <QEventLoop>           // 用 QEventLoop 使 QSerialPort 脱离 UI 线程？
#include <QSerialPortInfo>      // TODO: 查询并检查可用串口？

#include "util-common.h"
#include "global.h"

#if (1 == BLUETOOTH_TYPE)
# include "bluetoothserial.h"
#elif (2 == BLUETOOTH_TYPE)
# include "bluetoothrk.h"
#else
;xxx; // TODO:
#endif

//
CBluetoothIntf *CBluetoothIntf::instance = Q_NULLPTR;

//
CBluetoothIntf *CBluetoothIntf::getInstance()
{
    if (!instance) {

#if (1 == BLUETOOTH_TYPE)
        instance = new CBluetoothSerial();
#elif (2 == BLUETOOTH_TYPE)
        instance = new CBluetoothRk();
#else
        ;xxx; // TODO:
#endif

    }

    return instance;
}

//
CBluetoothIntf::CBluetoothIntf(QObject *parent) : QObject(parent)
{
    // ASSERT: parent == null_ptr   /* 要 QObject::moveToThread() 成功，this 不能有 parent ？见该函数的帮助。 */
    //if (parent)
    //    logWarning("CBluetooth::CBluetooth(): parent should be NULL!", CGlobal::LOG_BLUETOOTH);

    // 预建的连接对象
    btPrinter = new CBtConnection(this);
    btPrinter->devType = btDevType_Printer;
    btPrinter->isConnected = false;
    btPrinter->connId = -1;
    btPrinter->isMaster = true;

    btDatatrans = new CBtConnection(this);
    btDatatrans->devType = btDevType_Datatrans;
    btDatatrans->isConnected = false;
    btDatatrans->connId = -1;
    btDatatrans->isMaster = false;

    //
    conns.append(btPrinter);
    conns.append(btDatatrans);

    //

}

CBluetoothIntf::~CBluetoothIntf()
{

}

CBtConnection *CBluetoothIntf::getBtPrinter()
{
    return btPrinter;
}

CBtConnection *CBluetoothIntf::getBtDatatrans()
{
    return btDatatrans;
}

/// ============================================================================
/// CBtConnection
/// ============================================================================

//
CBtConnection::CBtConnection(CBluetoothIntf *_parent) : bluetooth(_parent)
{
    // ASSERT: _parent can't be NULL
    // TODO:

    QObject::connect(this, &CBtConnection::sigSendingData, this, &CBtConnection::slot_this_SendingData, Qt::QueuedConnection);

}

//
CBtConnection::~CBtConnection()
{

}

bool CBtConnection::getIsConnected()
{
    return isConnected;
}

void CBtConnection::setIsConnected(bool _is_connected)
{
    if (_is_connected != isConnected) {
        isConnected = _is_connected;
        emit sigStateChanged(isConnected);
    }
}

void CBtConnection::pushReceivedData(const QByteArray &_data)
{
    emit sigReceivedData(_data);
}

//
void CBtConnection::pushSendingData(const QByteArray &_data)
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    if (getIsConnected()) {
        emit sigSendingData(_data);
    } else {
        logWarning(QString("%1: bt not connected, sending cancelled").arg(__PRETTY_FUNCTION__), CGlobal::LOG_BLUETOOTH);
    }
}

void CBtConnection::slot_this_SendingData(QByteArray _data)
{
    bool succ = bluetooth->sendBtData(connId, _data);
    if (!succ) {
        //logWarning("bluetooth data sending failed!");
    }
}
