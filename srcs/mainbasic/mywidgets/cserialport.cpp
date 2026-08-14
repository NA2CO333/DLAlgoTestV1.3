#include "cserialport.h"

#include <QEventLoop>

#include "global.h"

//
CSerialPort::CSerialPort(QObject *parent) : QObject(parent)
{
    QObject::connect(this, &CSerialPort::sigSetIsOpened, this, &CSerialPort::slot_this_setIsOpened, Qt::QueuedConnection);
    QObject::connect(this, &CSerialPort::sigWrite, this, &CSerialPort::slot_this_write, Qt::QueuedConnection);

    workThread = new QThread();
    this->moveToThread(workThread);
    workThread->start();

}

CSerialPort::~CSerialPort()
{
    if (serialPort) {
        serialPort->close();
        delete serialPort;
        serialPort = Q_NULLPTR;
    }

    if (workThread) {
        workThread->quit();
        workThread->wait(3000);
        delete workThread;
        workThread = Q_NULLPTR;
    }

}

void CSerialPort::setSerialPortCfg(stSerialPortCfg _serial_cfg)
{
    config = _serial_cfg;

    //
    if (getIsOpened()) {
        setIsOpened(false);
        QThread::msleep(100);
        setIsOpened(true);
    }
}

bool CSerialPort::getIsOpened()
{
    return serialPort && serialPort->isOpen();
}

void CSerialPort::setIsOpened(bool _is_open)
{
    if (getIsOpened() == _is_open) {
        return;
    }

    emit sigSetIsOpened(_is_open);
}

bool CSerialPort::write(QByteArray &_data)
{
    setIsOpened(true);

    emit sigWrite(_data);

    return true;
}

void CSerialPort::slot_this_setIsOpened(bool _is_open)
{
    if (!serialPort) {
        serialPort = new QSerialPort();
        QObject::connect(serialPort, &QSerialPort::readyRead, this, &CSerialPort::slot_serialPort_readyRead, Qt::QueuedConnection);

        timerRead = new QTimer();
        QObject::connect(timerRead, &QTimer::timeout, this, &CSerialPort::slot_timerRead_timeout, Qt::QueuedConnection);

    }

    if (_is_open) {
        serialPort->setPortName(config.portName);
        serialPort->setBaudRate(config.baudRate, QSerialPort::AllDirections);
        serialPort->setDataBits(config.dataBits);
        serialPort->setParity(config.parity);
        serialPort->setStopBits(config.stopBits);
        serialPort->setFlowControl(config.flowControl);

        // 阻塞信号，避免收到打开前的垃圾数据
        serialPort->blockSignals(true);

        //
        bool opened = serialPort->open(QIODevice::ReadWrite);

        //
        if (opened) {
            //serialPort->setDataTerminalReady(true);

            // 清空发送和接收缓冲区   // TODO: 有没可能导致数据丢失？
            serialPort->clear(QSerialPort::AllDirections);

            timerRead->start(200);
        } else {
            QSerialPort::SerialPortError err = serialPort->error();
            QString msg = QString("Open bluetooth serial port failed: ") + serialPort->errorString();
            emit sigNotice(msg);
            if (QSerialPort::OpenError != err) {
                logCritical(QString::asprintf("%s: SerialPort opening failed, error: %d, %s", __PRETTY_FUNCTION__, (int)err, msg.toLocal8Bit().data()), CGlobal::LOG_BLUETOOTH);
            } else {
                logWarning(msg, CGlobal::LOG_BLUETOOTH);
            }

            timerRead->stop();
        }

        //
        serialPort->blockSignals(true);

    } else {
        serialPort->flush();
        //serialPort->waitForBytesWritten(1000);
        serialPort->close();
    }
}

void CSerialPort::slot_this_write(QByteArray _data)
{
    int n = serialPort->write(_data);
    serialPort->flush();
    bool succ = (_data.length() == n);      // TODO: 即使返回值等于数据长度也不代表数据发送成功了？
    if (!succ) {
        QString msg = QString("serial port %1 write %2 bytes failed!").arg(config.portName).arg(_data.length());
        logCritical(QString("%1: ").arg(__PRETTY_FUNCTION__) + msg, CGlobal::LOG_BLUETOOTH);
        emit sigNotice(msg);
    }
}

void CSerialPort::slot_serialPort_readyRead()
{
    logDebug(QString("%1: ").arg(__PRETTY_FUNCTION__) + "into ... ", CGlobal::LOG_BLUETOOTH);
    QByteArray data = serialPort->readAll();
    emit sigDataReceived(data);
}

void CSerialPort::slot_timerRead_timeout()
{
    if (serialPort->bytesAvailable() > 0) {
        //serialPort->waitForReadyRead(500);        // TODO: 改用单独线程轮询，实时性高点
        QByteArray data = serialPort->readAll();
        if (data.length() > 0) {
            emit sigDataReceived(data);
        }
    }
}

