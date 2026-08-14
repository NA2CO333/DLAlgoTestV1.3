#ifndef MYSERIALPORT_H
#define MYSERIALPORT_H

#include <atomic>

#include <QObject>
#include <QSerialPort>
#include <QRegExp>
#include <QTimer>
#include <QThread>

#include "globalclass.h"

//
extern const QByteArray COMMAND_COLOR_LAMP_ON;      // 打开彩灯（需已打开红外）
extern const QByteArray COMMAND_COLOR_LAMP_OFF;     // 关闭彩灯
extern const QByteArray power_off_command;          // 关机
extern const QByteArray capture_command;            // 转灯指令  byte2:转灯速度(0x2A)
extern const QByteArray wave_command;               // 超声指令(打开相机后定时调用)
extern const QByteArray wave_open_command;          // 打开超声
extern const QByteArray wave_close_command;         // 关闭超声
extern const QByteArray CMD_ABORT_TURN_LAMP;        // 终止转灯
extern const QByteArray openUSandIR;                // 打开超声和红外灯
extern const QByteArray closeUSandIR;               // 关闭超声和红外灯
extern const QByteArray closeIR;                    // 关闭红外灯
extern const QByteArray lowBatteryFlash;            // 低电量LED闪烁
extern const QByteArray openWiFiModule;             // 打开wifi电源
extern const QByteArray closeWiFiModule;            // 关闭wifi电源
extern const QByteArray CloseBT;                    // 关闭蓝牙电源
extern const QByteArray OpenBT;                     // 打开蓝牙电源
extern const QByteArray GetVersion;                 // 获取底板程序版本号

extern const QByteArray cmd1;                       // 工程模式转灯调试指令
extern const QByteArray cmd2;
extern const QByteArray cmd3;
extern const QByteArray cmd4;
extern const QByteArray cmd5;

extern const QByteArray CMD_SET_CURR_AND_PWM;           // 设置占空比（data[0]: 电流，data[1]: 占空比）
extern const QByteArray CMD_SET_LED_LEVEL;              // 设置电流等级（数据长度23字节，分别表示对应灯号的电流等级）
extern const QByteArray CMD_QUERY_LED_CURRENT_LEVEL;    // 读取 LED 电流等级

extern const QByteArray CMD_QUERY_CHARGING_CURRENT;     // 查询 充电电流、运行电流、红外led电流、纽扣电池电压、温度

// 底板串口收发
class MySerialPort : public QObject
{
    Q_OBJECT
public:
    static MySerialPort *instance();
    ~MySerialPort();

    void open();
    void close();
    void clear();

    void write(const QByteArray &byteArray);          // 异步写入命令
    void write(const uchar *data, qint64 maxSize);

    //==================================================

    bool isOpen() const;

    static QString byteArrayToHexStr(QByteArray data);          // 二进制转十六进制

    static stVerInfo getFirewareVersion();                      // 获取固件版本
    static bool isNewProtocal();                                // 是否新协议                  // NOTE: 参见《/docs/视筛仪下位机版本变更历史.txt》
    static bool isProtocalSupportChargedFull();                 // 通信协议是否支持“已充满”状态   // NOTE: 参见《/docs/视筛仪下位机版本变更历史.txt》

    void sendSetPwmDuty(int _percent);                  // 发送设置占空比指令
    void sendSetLedLevel(int _center, int _around);     // 发送设置 LED 电平指令

signals:
    void sigOpen();
    void sigClose();
    void sigClear();
    void sigCmdReceived(int _cmd_id, QByteArray _pkg_data);
    void sigWrite(const uchar *_data, qint64 _size, int _time_interval = 0);

protected slots:
    void slotOpen();
    void slotClose();
    void slotClear();
    void slotWrite(const uchar *_data, qint64 _size, int _time_interval);

    void slotErrorOccurred(QSerialPort::SerialPortError _error);
    void slotDataReady();

protected:
    explicit MySerialPort(const QString strComName, QObject *_parent = nullptr);
    static inline MySerialPort *s_instance {nullptr};
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    void processSerialData();

    QString m_strComName;
    QThread *m_workerThread;
    QSerialPort *m_pCom;

    std::atomic<bool> m_bOpen {false};

    qint64 m_iLen = -1;
    QByteArray buff = "";

};

#endif // MYSERIALPORT_H
