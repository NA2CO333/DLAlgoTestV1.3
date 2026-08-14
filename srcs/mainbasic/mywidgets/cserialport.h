#ifndef CSERIALPORT_H
#define CSERIALPORT_H

#include <QObject>
#include <QSerialPort>
#include <QThread>
#include <QTimer>

// 串口配置
struct stSerialPortCfg {
    QString                     portName;
    QSerialPort::BaudRate       baudRate;
    QSerialPort::DataBits       dataBits;
    QSerialPort::Parity         parity;
    QSerialPort::StopBits       stopBits;
    QSerialPort::FlowControl    flowControl;
};

// 串口读写封装
class CSerialPort : public QObject
{
    Q_OBJECT
public:
    explicit CSerialPort(QObject *parent = nullptr);
    ~CSerialPort();

    void setSerialPortCfg(stSerialPortCfg _serial_cfg);
    bool getIsOpened();

    void setIsOpened(bool _is_open);
    bool write(QByteArray &_data);

// 共有信号
signals:
    void sigDataReceived(QByteArray _data);
    void sigNotice(QString _msg);

protected:
    QSerialPort *serialPort = Q_NULLPTR;
    QThread *workThread = Q_NULLPTR;

    stSerialPortCfg config;

    QTimer *timerRead = Q_NULLPTR;

// 私有信号
signals:
    void sigSetIsOpened(bool _is_open/*, QPrivateSignal*/);
    void sigWrite(QByteArray _data/*, QPrivateSignal*/);

private slots:
    void slot_this_setIsOpened(bool _is_open);
    void slot_this_write(QByteArray _data);

    void slot_serialPort_readyRead();
    void slot_timerRead_timeout();

};

#endif // CSERIALPORT_H
