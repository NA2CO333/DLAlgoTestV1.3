#include "serialdatatrans.h"

#include <sys/prctl.h>

#include <QFile>
#include <QDir>
#include <QDebug>

#include "global.h"
#include "windatatrans.h"

//
CSerialDatatrans::CSerialDatatrans(QObject *parent) : QObject(parent)
{
    serialPortPath = getUsbSerialPortPath();

    // 串口
    serialPort = new QSerialPort();
    QObject::connect(serialPort, &QSerialPort::readyRead, this, &CSerialDatatrans::slot_serialPort_readyRead, Qt::QueuedConnection);

    writeLocker = new QMutex();

    //workerThread = new QThread;
    //serialPort->moveToThread(workerThread);
    //this->moveToThread(workerThread);
    //workerThread->start(QThread::NormalPriority);

    usbSerialMonitor = new CUsbSerialMonitor;
    QObject::connect(usbSerialMonitor, &CUsbSerialMonitor::sigUsbSerialDevChanged, this, &CSerialDatatrans::slot_usbSerialMonitor_UsbSerialDevChanged, Qt::QueuedConnection);

    usbSerialMonitor->setIsActive(true);

}

CSerialDatatrans::~CSerialDatatrans()
{

}

bool CSerialDatatrans::isUsbSerialExists()
{
    QString port_path = getUsbSerialPortPath();
    return port_path.length() > 0;
}

// 设置用户设定的串口路径
void CSerialDatatrans::setPortPathSetted(QString _port_path)
{
    QString msg;

    // 如果已打开，先关闭，并记下当前状态
    bool is_opened = serialPort->isOpen();
    if (is_opened) {
        setIsOpened(false, msg);
    }

    // 修改 “串口路径设置值”，如果为空，则取 USB 串口路径
    serialPortPathCfg = _port_path;
    if (serialPortPathCfg.length() == 0) {
        serialPortPath = getUsbSerialPortPath();
    }

    // 如果之前是打开的，则重新打开
    if (is_opened) {
        setIsOpened(true, msg);
    }
}

// 设置打开或关闭状态
bool CSerialDatatrans::setIsOpened(bool _is_opened, QString &_msg, bool _force)
{
    logDebug(QString::asprintf("CSerialDatatrans::setIsOpened(): _is_opened =  %s", Util::bool2str(_is_opened)), CGlobal::LOG_DATATRANS);

    // TODO: 若串口被占用或其它原因肯定无法打开，而每次写数据时又尝试打开，资源消耗情况？有必要设法减少打开串口频率吗？


    //
    m_isOpened = _is_opened;

    if(_is_opened)
    {
        serialPort->clear();

        // 如果设置了串口路径，则使用串口路径，否则获取 USB 串口路径
        if (serialPortPathCfg.length() > 0) {
            serialPortPath = serialPortPathCfg;
        } else {
            serialPortPath = getUsbSerialPortPath();
        }

        if (serialPortPath.length() == 0) {
            _msg = tr("未发现USB串口！"); // "USB Serial Port not found!"
            return false;
        }

        if (serialPort->isOpen()) {
            if (_force) {
                serialPort->close();
            } else {
                return true;
            }
        }

        //
        if (!usbSerialMonitor->getIsActive()) {
            usbSerialMonitor->setIsActive(true);
        }

        //
        QString port_path = serialPortPath;
        serialPort->setPortName(port_path);                         // 端口路径
        serialPort->setBaudRate(CGlobal::dataTransSerialBaud);      // 波特率
        serialPort->setDataBits(QSerialPort::Data8);                // 8数据位
        serialPort->setStopBits(QSerialPort::OneStop);              // 1停止位
        serialPort->setParity(QSerialPort::NoParity);               // 无奇偶校验
        serialPort->setFlowControl(QSerialPort::NoFlowControl);     // 无流控

        bool is_opened = serialPort->open(QIODevice::ReadWrite);    //打开串口设备
        QSerialPort::SerialPortError err = QSerialPort::NoError;
        if (is_opened) {
            logDebug(QString("CSerialDatatrans::setIsOpened(): port_path = ") + port_path + ", baud = " + QString::number(CGlobal::dataTransSerialBaud), CGlobal::LOG_DATATRANS);

        } else {
            err = serialPort->error();
            _msg = tr("打开串口失败：") + serialPort->errorString();   // "Open serial port failed: "
            logCritical(QString::asprintf(
                                "CSerialDatatrans::setIsOpened(): SerialPort opening failed, error: %d, %s!",
                                (int)err, _msg.toLocal8Bit().data()
                                ), CGlobal::LOG_DATATRANS);

            //timerCheckOpen->start(5000);        // TODO: 5 秒的定时检查，平均 2.5 秒触发？
        }
        if (QSerialPort::OpenError == err)
            is_opened = true;

        //serialPort->clear();

        return is_opened;
    } else {
        if (serialPort->isOpen()) {
            serialPort->close();
        }

        return true;
    }
}

QString CSerialDatatrans::getUsbSerialPortPath()
{
    QString path = "";

    QString str_dir = "/dev";
    QDir dir(str_dir);
    QStringList filter_names;
    filter_names << "ttyUSB*";
    QStringList name_list = dir.entryList(filter_names, QDir::System, QDir::Name);
    if (name_list.count() > 0) {
#if (OS_TYPE != 2)
        QString name = name_list[0];                        // 嵌入式机器，取第一个“ttyUSB”设备作为串口通信设备
#else
        QString name = name_list[name_list.count() - 1];    // 桌面版，取最后一个“ttyUSB”设备作为串口通信设备      /* 注意：如果没有插入该用途的 USB 串口，且打开了这个串口，将会占用测距或蓝牙的串口，难以发现原因！ */
#endif
        path = QString("/dev") + QDir::separator() + name;
    }

    return path;
}

void CSerialDatatrans::checkAndSet(QString *_msg)
{
    QString msg;
    bool is_need_opened = (DataTrans::connMode_UsbUart == DataTrans::DataTransmiter::ConnMode || DataTrans::connMode_Uart == DataTrans::DataTransmiter::ConnMode);

    QString port_path = (DataTrans::connMode_Uart == DataTrans::DataTransmiter::ConnMode ? G_COM_DISTANCE : "");
    setIsOpened(false, msg);
    setPortPathSetted(port_path);

    bool succ_open = setIsOpened(is_need_opened, msg);
    if (!succ_open) {
        logDebug(QString::asprintf("WinDataTrans::checkAndSave(): open datatrans serialport failed: %s", msg.toUtf8().data()), CGlobal::LOG_DATATRANS);
        if (_msg) {
            *_msg = msg;
        }
    } else {
        logDebug(QString::asprintf("WinDataTrans::checkAndSave(): is_opened =  %s", Util::bool2str(is_need_opened)), CGlobal::LOG_DATATRANS);
    }
}

bool CSerialDatatrans::writeData(QByteArray _data, bool _is_wait)
{
    logDebug(QString::asprintf("CSerialDatatrans::writeData(): writing data: %s", _data.data()), CGlobal::LOG_DATATRANS);

    //
    if (!serialPort->isOpen()) {
        QString msg;
        if (!setIsOpened(true, msg)) {
            logCritical(QString::asprintf("CSerialDatatrans::writeData(): open serialport failed: %s!", msg.toUtf8().data()), CGlobal::LOG_DATATRANS);
            return false;
        }
    }

    //
    bool succ = false;
    writeLocker->lock();        // TODO: 这有必要吗？QSerialPort::write() 应该是线程安全的？
    try {
        int n = serialPort->write(_data);       // TODO: 即使返回值等于数据长度也不代表数据发送成功了？
        succ = (n == _data.length());

        if (_is_wait) {
            succ = serialPort->waitForBytesWritten(500);
            if (!succ) {
                logWarning("CSerialDatatrans::writeData(): write() timeout or error!", CGlobal::LOG_DATATRANS);
            }
            serialPort->clear(QSerialPort::Input);
        } else {
            serialPort->flush();
        }
    } catch (std::exception &ex) {
        logCritical(QString::asprintf("CSerialDatatrans::writeData(): catch exception: %s!", ex.what()), CGlobal::LOG_DATATRANS);
    } catch (...) {
        int err_no = errno;
        logCritical(QString::asprintf("CSerialDatatrans::writeData(): catch unknown exception: no=%d, msg=%s!", err_no, strerror(err_no)), CGlobal::LOG_DATATRANS);
    }
    writeLocker->unlock();

    //
    return succ;
}

void CSerialDatatrans::slot_serialPort_readyRead()
{
    QByteArray data = serialPort->readAll();
    //serialPort->clear(QSerialPort::Input);

    logDebug(QString::asprintf("CSerialDatatrans::slot_serialPort_readyRead(): received data: %s", data.data()), CGlobal::LOG_DATATRANS);

    emit sigReceivedData(data);

}

void CSerialDatatrans::slot_usbSerialMonitor_UsbSerialDevChanged(int _port_num, QString _port_name, bool _is_added)
{
    Q_UNUSED(_port_num)
    Q_UNUSED(_port_name)
    Q_UNUSED(_is_added)

    // 如果设定的串口路径，则不需处理
    if (serialPortPathCfg.length() > 0) {
        return;
    }

    //
    QString path_new = getUsbSerialPortPath();
    QString path_old = serialPortPath;
    serialPortPath = path_new;

    logDebug(QString::asprintf("CSerialDatatrans::slot_usbSerialMonitor_UsbSerialDevChanged(): path_new: %s", path_new.toLocal8Bit().data()), CGlobal::LOG_DATATRANS);

    // 如果已经被用户打开，则重新检查确认状态一致性
    if (m_isOpened) {
        if (path_new.length() > 0) {        // 如果有设备：若路径不同，则重连
            if (path_new != path_old) {
                QString msg;
                bool succ_open = setIsOpened(true, msg, true);
                if (!succ_open)
                    logWarning(QString::asprintf("CSerialDatatrans::slot_usbSerialMonitor_UsbSerialDevChanged(): Open failed: %s!", msg.toLocal8Bit().data()), CGlobal::LOG_DATATRANS);
            }
        } else {            // 如果没设备：关闭
            QString msg;
            bool succ_open = setIsOpened(false, msg);
            m_isOpened = true;        // 关闭串口，但是打开标志应不变
            if (!succ_open)
                logWarning(QString::asprintf("CSerialDatatrans::slot_usbSerialMonitor_UsbSerialDevChanged(): Close failed: %s!", msg.toLocal8Bit().data()), CGlobal::LOG_DATATRANS);
        }
    }
}

// =================================================================================================
// class CUsbSerialMonitor:

#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define UEVENT_BUFFER_SIZE 2048

CUsbSerialMonitor::CUsbSerialMonitor(QObject *parent)
{
    Q_UNUSED(parent)


}

CUsbSerialMonitor::~CUsbSerialMonitor()
{

}

void CUsbSerialMonitor::setIsActive(bool _is_opened)
{
    isActive = _is_opened;

    if (isActive) {
        if (!this->isRunning())
            this->start();
    } else {
        //if (this->isRunning())
        //    this->quit();
    }
}

bool CUsbSerialMonitor::getIsActive()
{
    return isActive;
}

void CUsbSerialMonitor::run()
{    
    // 设置线程名称
    prctl(PR_SET_NAME, "CUsbSerialMonitor", nullptr, nullptr, nullptr);

    //
    isActive = true;
    try {
        struct sockaddr_nl client;
        struct timeval tv;
        int fd, rcvlen, ret;
        fd_set fds;
        int buffersize = 1024;
        fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);      // 侦听 USB 插拔消息
        memset(&client, 0, sizeof(client));
        client.nl_family = AF_NETLINK;

        //client.nl_pid = getpid();     /* 可能别的地方也这么写，导致冲突 */
        client.nl_pid = 0;              // 设为0，自动分配

        client.nl_groups = 1;
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(buffersize));
        int ret_bind = bind(fd, (struct sockaddr*)&client, sizeof(client));
        if (ret_bind >= 0) {
            QString msg;
            while (isActive) {
               char buf[UEVENT_BUFFER_SIZE] = { 0 };
               FD_ZERO(&fds);
               FD_SET(fd, &fds);
               tv.tv_sec = 0;
               tv.tv_usec = 300 * 1000;
               ret = select(fd + 1, &fds, NULL, NULL, &tv);
               if(ret < 0)
                   continue;
               if(!(ret > 0 && FD_ISSET(fd, &fds)))
                   continue;
               rcvlen = recv(fd, &buf, sizeof(buf), 0);         // TODO: 用 select() 避免长时间阻塞，导致不及时处理退出指令
               if (rcvlen > 0) {
                   //printf("CUsbSerialMonitor::run(): %s\n", buf);     // TODO: printf() 在 Qt 调试时没见到输出？
                   logDebug(QString::asprintf("CUsbSerialMonitor::run(): Got uevent: %s", buf), CGlobal::LOG_DATATRANS);

                   msg = buf;
                   const QString DEV_NAME_KEY = "ttyUSB";
                   int idx = msg.lastIndexOf(DEV_NAME_KEY);
                   if (idx > 0) {
                       QString num_str = msg.mid(idx + DEV_NAME_KEY.length());

                       bool is_num = true;          /* “ttyUSB”之后应有且仅有一个最多2位的十进制整数 */
                       if (num_str.length() > 2)
                           is_num = false;
                       int num = num_str.toInt(&is_num, 10);

                       if (is_num) {
                           int change_type = -1;
                           if (msg.startsWith("add@"))
                               change_type = 1;
                           else if (msg.startsWith("remove@")) {
                               change_type = 0;
                           }
                           if (change_type >= 0) {
                               emit sigUsbSerialDevChanged(num, msg.mid(idx), (bool)change_type);
                           }
                       }
                   }
               }
           }
        } else {
            //perror("bind failed: ");
            logCritical(QString::asprintf("CUsbSerialMonitor::run(): bind failed: errno=%d, errmsg=%s", errno, strerror(errno)), CGlobal::LOG_DATATRANS);
        }
        close(fd);
    } catch (...) {
        logCritical(QString::asprintf("CUsbSerialMonitor::run(), exception catched: %s !", strerror(errno)), CGlobal::LOG_DATATRANS);
    }
    isActive = false;
}
