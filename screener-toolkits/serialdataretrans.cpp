#include "serialdataretrans.h"

CSerialDataRetrans::CSerialDataRetrans(QObject *parent) : QObject(parent)
{
    com1 = new QSerialPort;
    com2 = new QSerialPort;
    QObject::connect(com1, &QSerialPort::readyRead, this, &CSerialDataRetrans::slot_com1_readyRead, Qt::QueuedConnection);
    QObject::connect(com2, &QSerialPort::readyRead, this, &CSerialDataRetrans::slot_com2_readyRead, Qt::QueuedConnection);
}



bool CSerialDataRetrans::setIsActive(bool _is_active, QString *_msg)
{
    if (_is_active) {
        _msg->clear();
        QString com1_path = "/dev/ttySAC0";
        bool succ1 = setComIsOpened(com1, true, &com1_path, _msg);
        if (!succ1)
            return false;

        _msg->clear();
        QString com2_path = "/dev/ttyUSB0";
        bool succ2 = setComIsOpened(com2, true, &com2_path, _msg);
        if (!succ2)
            return false;

        return (succ1 && succ2);
    } else {
        bool succ1 = setComIsOpened(com1, false, Q_NULLPTR, Q_NULLPTR);
        bool succ2 = setComIsOpened(com2, false, Q_NULLPTR, Q_NULLPTR);
        return (succ1 && succ2);
    }
}

bool CSerialDataRetrans::setComIsOpened(QSerialPort *_serial_port, bool _is_active, QString *_path, QString *_msg)
{
    if (_is_active) {
        if (!_path) {
            if (_msg) {
                *_msg = isChinese ? "路径不可为空！" : "Path can't be NULL!";
            }
            return false;
        }

        QString port_name = *_path;
        _serial_port->setPortName(port_name);
        _serial_port->setBaudRate(QSerialPort::Baud115200);
        _serial_port->setDataBits(QSerialPort::Data8);
        _serial_port->setParity(QSerialPort::NoParity);
        _serial_port->setStopBits(QSerialPort::OneStop);
        _serial_port->setFlowControl(QSerialPort::NoFlowControl);

        bool opened = _serial_port->open(QIODevice::ReadWrite);
        QSerialPort::SerialPortError err = QSerialPort::NoError;
        if (!opened) {
            err = _serial_port->error();
            if (_msg) {
                *_msg = QString(isChinese ? "打开蓝牙串口失败：" : "Open bluetooth serial port failed: ") + _serial_port->errorString();
            }
            //logger.critical(QString::asprintf(
            //                    "CSerialDataRetrans::setComIsOpened: SerialPort opening failed, error: %d, %s",
            //                    (int)err, _msg.toLocal8Bit().data()
            //                    ), CGlobal::LOG_BLUETOOTH);
        }
        if (QSerialPort::OpenError == err)
            opened = true;

        //_serial_port->clear();

        return opened;
    } else {
        _serial_port->close();

        return true;
    }
}

void CSerialDataRetrans::slot_com1_readyRead()
{
    QByteArray data = com1->readAll();
    com2->write(data);
}

void CSerialDataRetrans::slot_com2_readyRead()
{
    QByteArray data = com2->readAll();
    com1->write(data);
}

