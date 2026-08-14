#include "myserialport.h"

#include <QThread>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QDateTime>
#include <QDataStream>
#include <QElapsedTimer>

#include "global.h"
#include "aboutdevice.h"

//
/* 数据帧的格式定义，见 void MySerialPort::processSerialData() */

//
const QByteArray COMMAND_COLOR_LAMP_ON  = QByteArray::fromRawData("\x55\x7A\x06\x0B\x01\x00\x00\x00", 8);       // 打开彩灯（需已打开红外）
const QByteArray COMMAND_COLOR_LAMP_OFF = QByteArray::fromRawData("\x55\x7A\x06\x0B\x00\x00\x00\x00", 8);       // 关闭彩灯
const QByteArray power_off_command      = QByteArray::fromRawData("\x55\x7A\x06\x08\x11\x00\x00\x00", 8);       // 关机

const QByteArray capture_command        = QByteArray::fromRawData("\x55\x7A\x06\x04\x00\x12\x01\x00", 8);       // 转灯指令  data[0]data[1](0x0012):转灯速度，data[2](0x01)转灯圈数
const QByteArray wave_command           = QByteArray::fromRawData("\x55\x7A\x06\x06\x00\x00\x00\x00", 8);       // 超声指令(打开相机后定时调用)
const QByteArray wave_open_command      = QByteArray::fromRawData("\x55\x7A\x06\x07\x01\x00\x00\x00", 8);       // 打开超声     // TODO: 这是红外的？这条指令没真正用到
const QByteArray wave_close_command     = QByteArray::fromRawData("\x55\x7A\x06\x07\x00\x00\x00\x00", 8);       // 关闭超声
const QByteArray CMD_ABORT_TURN_LAMP    = QByteArray::fromRawData("\x55\x7A\x03\x1A\x00"            , 5);       // 终止转灯

const QByteArray openUSandIR            = QByteArray::fromRawData("\x55\x7a\x06\x07\x01\x01\x00\x00", 8);       // 打开超声和红外灯（超声和红外控制：00关，01开，FF不变）     // TODO: 红外和超声不应该混一起？
const QByteArray closeUSandIR           = QByteArray::fromRawData("\x55\x7a\x06\x07\x00\x00\x00\x00", 8);       // 关闭超声和红外灯
const QByteArray lowBatteryFlash        = QByteArray::fromRawData("\x55\x7A\x06\x13\x00\x96\x00\x00", 8);       // 低电量LED闪烁

const QByteArray closeIR                = QByteArray::fromRawData("\x55\x7a\x06\x07\x00\x01\x00\x00", 8);       // 关闭红外灯（保留超声打开）    // TODO: 底板的红外和超声电源好像是同步的？shit!

//新硬件增加指令
const QByteArray openWiFiModule         = QByteArray::fromRawData("\x55\x7A\x06\x16\x01\xFF\xFF\x00", 8);       // 打开wifi电源
const QByteArray closeWiFiModule        = QByteArray::fromRawData("\x55\x7A\x06\x16\x00\xFF\xFF\x00", 8);       // 关闭wifi电源
const QByteArray CloseBT                = QByteArray::fromRawData("\x55\x7a\x06\x0c\x00\x00\x00\x00", 8);       // 关闭蓝牙电源
const QByteArray OpenBT                 = QByteArray::fromRawData("\x55\x7a\x06\x0c\x01\x00\x00\x00", 8);       // 打开蓝牙电源

const QByteArray GetVersion             = QByteArray::fromRawData("\x55\x7a\x06\x23\x00\x00\x00\x00", 8);       // 获取底板程序版本号

//工程模式转灯调试指令
const QByteArray cmd1                   = QByteArray::fromRawData("\x55\x7A\x06\x04\x00\x2A\x01\x00"        ,  8);      // 转灯
const QByteArray cmd2                   = QByteArray::fromRawData("\x55\x7A\x08\xf0\x01\x02\x03\x04\x05\x06", 10);      // 0 度
const QByteArray cmd3                   = QByteArray::fromRawData("\x55\x7A\x08\xf0\x07\x08\x09\x0a\x0b\x0c", 10);      // 60 度
const QByteArray cmd4                   = QByteArray::fromRawData("\x55\x7A\x08\xf0\x0d\x0e\x0f\x10\x11\x12", 10);      // 120 度
const QByteArray cmd5                   = QByteArray::fromRawData("\x55\x7A\x08\xf0\x13\x14\x15\x16\x17\x18", 10);      // 四角

const QByteArray CMD_SET_CURR_AND_PWM           = QByteArray::fromRawData("\x55\x7A\x06\x02\x30\xFF\x00\x00", 8);       // 设置占空比（data[0]: 电流，data[1]: 占空比）  // TODO: 这里的 data[2] 下位机并未用到？
const QByteArray CMD_SET_LED_LEVEL              = QByteArray::fromRawData("\x55\x7A\x1A\x0D", 4);                       // 设置电流等级（数据长度23字节，分别表示对应灯号的电流等级）
const QByteArray CMD_QUERY_LED_CURRENT_LEVEL    = QByteArray::fromRawData("\x55\x7A\x03\x0F\x00", 5);                   // 读取 LED 电流等级

const QByteArray CMD_QUERY_CHARGING_CURRENT     = QByteArray::fromRawData("\x55\x7A\x03\x11\x00", 5);                   // 查询 充电电流、运行电流、红外led电流、纽扣电池电压、温度

// TODO: 杨顺闻：0A指令设置下位机充电状态，不合理？

// TODO: 上面的指令，CRC 呢？

//
MySerialPort *MySerialPort::instance()
{
    if (!s_instance) {
        s_instance = new MySerialPort(QString(G_COM_BASEBOARD));
    }
    return s_instance;
}

MySerialPort::MySerialPort(const QString strComName, QObject *_parent)
        : QObject(_parent)
        , m_strComName(strComName)
        , m_workerThread(new QThread())
        , m_pCom(new QSerialPort())
        , m_bOpen(false)
        , m_iLen(-1)
{
    // TODO: 检查串口路径是否存在

    //
    m_pCom->moveToThread(m_workerThread);
    this->moveToThread(m_workerThread);
    m_workerThread->start(QThread::Priority::TimeCriticalPriority);

    connect(this, &MySerialPort::sigOpen, this, &MySerialPort::slotOpen, Qt::QueuedConnection);
    connect(this, &MySerialPort::sigClose, this, &MySerialPort::slotClose, Qt::QueuedConnection);
    connect(this, &MySerialPort::sigClear, this, &MySerialPort::slotClear, Qt::QueuedConnection);
    connect(this, &MySerialPort::sigWrite, this, &MySerialPort::slotWrite, Qt::QueuedConnection);

    connect(m_pCom, &QSerialPort::readyRead, this, &MySerialPort::slotDataReady, Qt::DirectConnection);

    qDebug()<<"MySerialPort::MySerialPort build finish!";
}

MySerialPort::~MySerialPort()
{
#ifdef QT_DEBUG
//    qDebug() << "~MySerialPort:" << QThread::currentThreadId() << "\n";
#endif
    close();
    m_workerThread->quit();
    m_workerThread->wait(10000);
    delete m_pCom;
    m_pCom = nullptr;
    delete m_workerThread;
    m_workerThread = nullptr;
}

bool MySerialPort::isOpen() const
{
    return m_bOpen;
}

void MySerialPort::open()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    emit sigOpen();
}
void MySerialPort::close()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    emit sigClose();
}

void MySerialPort::clear()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    emit sigClear();
}

void MySerialPort::write(const QByteArray &byteArray)
{
    write((uchar *)byteArray.data(), byteArray.size());
}

void MySerialPort::write(const uchar *data, qint64 maxSize)
{
    emit sigWrite(data, maxSize);
}

void MySerialPort::slotOpen()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    m_pCom->close();
    m_bOpen = false;

    //
    if(m_strComName.isEmpty()) {
        return;
    }

    //
    m_pCom->setPortName(m_strComName);

    m_pCom->setBaudRate(QSerialPort::Baud115200);
    m_pCom->setDataBits(QSerialPort::Data8);
    m_pCom->setParity(QSerialPort::NoParity);
    m_pCom->setStopBits(QSerialPort::OneStop);
    m_pCom->setFlowControl(QSerialPort::NoFlowControl);

    if (! m_pCom->isOpen()) {
        m_pCom->blockSignals(true);     // 临时阻塞信号以避免刚打开时收到关闭期间产生的大量垃圾数据

        if(! m_pCom->open(QIODevice::ReadWrite))
        {
            qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): Failed to open \"" << m_strComName << "\"! error = "
                        << QVariant::fromValue(m_pCom->error()).toString() << ", Desc = " << m_pCom->errorString();

        } else {
            //bool is_clear_succ = m_pCom->clear(QSerialPort::AllDirections);    // TODO: 清不掉？还是有大量数据输出？这是因为这个函数是异步的？
            //qApp->processEvents();
            //if (!is_clear_succ) {
            //    qWarning() << "serialport clear failed!";
            //}

            //
            //m_pCom->setDataTerminalReady(true);

            //
            QByteArray data;
            int count_clear = 0;
            do {
                data = m_pCom->readAll();
                count_clear++;
            } while (data.length() > 1024);
            qDebug() << "readAll() after open(), count = " << count_clear;
        }

        m_pCom->blockSignals(false);
    }

    m_bOpen = true;

    qDebug()<<"--------setCom--------------";
}

void MySerialPort::slotClose()
{
    qDebug() << "slotClose:" << QThread::currentThreadId() <<"\n";
    m_pCom->close();
    m_bOpen = false;
}

void MySerialPort::slotClear()
{
    m_pCom->clear(QSerialPort::AllDirections);
}

void MySerialPort::slotWrite(const uchar *_data, qint64 _size, int _time_interval)
{
    //qDebug() << "MySerialPort::slotWrite, stat =" << stat;

    if (!m_bOpen)  {
        qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): port not opened!";
        return;
    }

    /* 注意：经实测，如果不延时一定时间，如果前面刚刚发过指令，则接下来发送的指令不能被成功执行。
     * 底板程序：v54.0.0     2023-09-12
     */
    static const int MIN_INTERVAL = 20;             // 最短时间间隔
    static QElapsedTimer timer;

    //
    int interval = (_time_interval >= MIN_INTERVAL ? _time_interval : MIN_INTERVAL);
    if (interval > 0) {
        if (timer.isValid()) {
            while (timer.elapsed() < interval) {
                QThread::msleep(5);
            }
            //
            timer.restart();
        } else {
            timer.start();
        }
    }

    //
    m_iLen = -1;
    //qDebug()<<"slotWrite++++++++++lock";
    //QString str = byteArrayToHexStr(QByteArray(_data));

    if (m_pCom->isOpen()) {
        m_iLen =  m_pCom->write((char *)_data, _size);
        m_pCom->flush();
        //if (stat == 4)
        //qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): _data = " << QByteArray((const char *)_data, _size).toHex() << "; _size = " << _size;
    } else {
        qDebug() << "m_pCom is close";
    }

    /* sun
    QByteArray data = m_pCom->readAll();
    QDataStream out(&data,QIODevice::ReadWrite);    //将字节数组读入
    QString str;
    while(!out.atEnd())
    {
        qint8 outChar = 0;
        out>>outChar;   //每字节填充一次，直到结束
        //十六进制的转换
        str += QString("%1").arg(outChar & 0xFF,2,16,QLatin1Char('0'));
    }
//    qDebug()<<"slotWrite  str = "<<str;
    emit sigCmdReceived(str);
*/

//    slotClear();

}

void MySerialPort::slotErrorOccurred(QSerialPort::SerialPortError _error)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): SerialPort \"" << m_strComName << "\" ErrorOccurred: "
                << QVariant::fromValue(_error).toString() << ", Desc = " << m_pCom->errorString();
}

void MySerialPort::slotDataReady()
{
    QByteArray data = m_pCom->readAll();        // TODO: 为什么刚刚打开时收到大量数据（一次连续调试时收到 200 次 * 4k）？

    if (data.length() > 1024) {
        qWarning() << "data size = " << data.length() << ", junk data?";
        return;
    }

    buff.append(data);
    //logDebug("MySerialPort::slotDataReady(): received data = " + data.toHex(), CGlobal::LOG_BASEBOARD);

    processSerialData();
}

void MySerialPort::processSerialData()
{
    const QByteArray PKG_HEADR = "\x55\x7a";

    //static QTime time_received = QTime::currentTime();    // 收到指令时间，如果检测不到下一个数据包的包头，则延时后下发给处理过程（通信协议有点乱，不确定包完成性检查是正确的）

    // 数据帧格式（来源：单片机代码“Inc\hal_cmd.h”）：
    struct stBaseBoardCmdPkt
    {
        uint8_t sync;        // 同步头 0x55
        uint8_t stx;         // 包头 0x7A
        uint8_t len;         // 包长（从 len 到 crc 的长度，即帧全长度 - 2？见底板程序(2022-06-12) cmd_Receive3() -> cmd_send3((uint8_t *)p,p->len+2)）
        uint8_t cmd;         // 指令码
        uint8_t data[23];    // 数据
        uint8_t crc;         // crc
    };

    // 数据分包
    QByteArray pkg_data;
    int idx_head = -1;
    int len = -1;       // “长度”字节的值
    do {
        pkg_data.clear();

        //
        idx_head = buff.indexOf(PKG_HEADR);
        if (idx_head >= 0) {       // 如果找到包头
            len = (unsigned char)buff[idx_head + 2];
            if (buff.length() - idx_head >= len) {          // 且已收到完整数据包
                pkg_data = buff.mid(idx_head, len + 2);
                //qDebug() << "MySerialPort::processSerialData(): get pkg_data = " << pkg_data.toHex();
                buff.remove(0, idx_head + len + 2);
            } else {
                if (idx_head > 0) {
                    buff = buff.mid(idx_head);
                }
            }
        }

        //
        if (pkg_data.length() > 0) {
            int cmd_id = (unsigned char)pkg_data[3];
            //QByteArray cmd_data = pkg_data.mid(3, pkg_data.length() - 5);
            //emit sigCmdReceived(cmd_id, cmd_data);
            emit sigCmdReceived(cmd_id, pkg_data);
            //qDebug() << "MySerialPort::processSerialData(): get cmd_id = 0x" << QString::number(cmd_id, 16) << ", cmd_data = " + cmd_data.toHex();
        }
    } while (pkg_data.length() > 0);

    //
    if (buff.size() > 1024000) {  // 防止异常导致数据一直未清除
        buff.clear();
        logWarning("MySerialPort::processSerialData(): buffer size abnormal!", CGlobal::LOG_DISTANCE);
    }

    //qDebug()<<"void MySerialPort::processSerialData()";
}

QString MySerialPort::byteArrayToHexStr(QByteArray data)
{
    QDataStream out(&data,QIODevice::ReadWrite);    //将字节数组读入
    QString str;
    while(!out.atEnd())
    {
        qint8 outChar = 0;
        out>>outChar;   //每字节填充一次，直到结束
        //十六进制的转换
        str += QString("%1").arg(outChar & 0xFF,2,16,QLatin1Char('0'));     // TODO: 确保十六进制字符串具有一致的大小写状态
    }

    return str;
}

stVerInfo MySerialPort::getFirewareVersion()
{
    int ver_major, ver_minor, ver_patch;
    aboutdevice::getStm32Version(ver_major, ver_minor, ver_patch);
    return stVerInfo {ver_major, ver_minor, ver_patch};
}

bool MySerialPort::isNewProtocal()
{
    // NOTE: （参见《/docs/视筛仪下位机版本变更历史.txt》）
    stVerInfo ver_curr = getFirewareVersion();
    bool is_new = (!ver_curr.isNull() && ver_curr.verMajor < 50);
    return is_new;
}

bool MySerialPort::isProtocalSupportChargedFull()
{
    // NOTE: （参见《/docs/视筛仪下位机版本变更历史.txt》）}
    static const stVerInfo VER_MIN {1, 6, 1};
    stVerInfo ver_curr = getFirewareVersion();
    bool is_support = (!ver_curr.isNull() && ver_curr.verMajor < 50 && ver_curr.compareWith(VER_MIN) >= 0);
    return is_support;
}

void MySerialPort::sendSetPwmDuty(int _percent)
{
    static const int LEN_CMD = 8;
    static uchar cmd[LEN_CMD];

    memcpy(cmd, CMD_SET_CURR_AND_PWM.constData(), LEN_CMD);
    uchar duty = (uchar)(std::round((float)0xFF * _percent / 100));
    cmd[5] = duty;

    write(cmd, LEN_CMD);
}

void MySerialPort::sendSetLedLevel(int _center, int _around)
{
    if (_center < 0) {
        _center = 0;
    } else if (_center > 24) {
        _center = 24;
    }

    if (_around < 0) {
        _around = 0;
    } else if (_around > 24) {
        _around = 24;
    }

    //
    QByteArray cmd_bytes(CMD_SET_LED_LEVEL);
    uchar c;

    c = _center;
    cmd_bytes += c;

    c = _around;
    for (int i = 1; i <= 22; i++) {
        cmd_bytes += c;
    }

    c = 0;
    cmd_bytes += c;

    logDebug(cmd_bytes.toHex());
    write((uchar *)cmd_bytes.data(), cmd_bytes.length());
}
