#ifndef CSERIALDATATRANS_H
#define CSERIALDATATRANS_H

#include <QObject>
#include <QSerialPort>
#include <QMutex>
#include <QTimer>
#include <QThread>

//
class CUsbSerialMonitor;

// 数据传输的串口连接封装
class CSerialDatatrans : public QObject
{
    Q_OBJECT
public:
    explicit CSerialDatatrans(QObject *parent = 0);
    ~CSerialDatatrans();

    static QString getUsbSerialPortPath();

    bool isUsbSerialExists();
    void setPortPathSetted(QString _port_path = "");

    bool setIsOpened(bool _is_opened, QString &_msg, bool _force = false);
    inline bool isOpened() { return m_isOpened; }
    bool writeData(QByteArray _data, bool _is_wait = false);
    void checkAndSet(QString *_msg = Q_NULLPTR);

protected:
    QSerialPort *serialPort;
    QMutex *writeLocker;
    //QThread *workThread;

    bool m_isOpened = false;
    QTimer *timerCheckOpen = Q_NULLPTR;
    CUsbSerialMonitor * usbSerialMonitor = Q_NULLPTR;

    QString serialPortPathCfg = "";     // 用户设定的串口路径（如果为空，则自动检测 USB 串口）
    QString serialPortPath = "";        // 当前串口路径（可能是自动检测的）

signals:
    void sigReceivedData(QByteArray _data);

private slots:
    void slot_serialPort_readyRead();
    void slot_usbSerialMonitor_UsbSerialDevChanged(int _port_num, QString _port_name, bool _is_added);

public slots:

};

// USB串口监视器，定时查询当前可用 USB 串口
class CUsbSerialMonitor : public QThread
{
    Q_OBJECT
public:
    explicit CUsbSerialMonitor(QObject *parent = 0);
    ~CUsbSerialMonitor();

    void setIsActive(bool _is_opened);
    bool getIsActive();

protected:
    void run();

    bool isActive = false;

signals:
    void sigUsbSerialDevChanged(int _port_num, QString _port_name, bool _is_added);

};

#endif // CSERIALDATATRANS_H
