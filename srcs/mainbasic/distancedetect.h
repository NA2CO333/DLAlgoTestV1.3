#ifndef CDISTANCEDETECT_H
#define CDISTANCEDETECT_H

#include <atomic>

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QMutex>

#include <vector>
#include <deque>
//#include <list>
#include <functional>

#include "globalclass.h"
#include "algo-invoker.h"

// TODO: 以 "\xaa" 形式定义的常量，检查修正，避免 \x0 导致的异常

//
class CDistSensor;

using funcCheckPkg = std::function<bool(uchar *)>;          // 距离数据包格式检查函数指针

// 封装距离检测功能及距离判断逻辑
class CDistanceDetect : public QObject
{
    Q_OBJECT
public:
    explicit CDistanceDetect(QObject *parent = 0);
    ~CDistanceDetect();

    static bool checkAndCorrectDistType(enDistSensorType &_dist_type);      // 检查修正传感器类型
    static bool isSensorNeedPort3(enDistSensorType _dist_type);             // 传感器是否需要 Port3

    void setSensorType(enDistSensorType _sensor_type, bool _is_init = false);   // 设置传感器类型
    inline enDistSensorType sensorType() { return m_sensorType; }

    void init();                                        // 初始化

    void setRequestInterval(int _interval);             // 设置查询间隔

    bool setIsOpened(bool _is_open);                    // 设置测距通信串口开关状态
    void setIsPaused(bool _is_paused);                  // 设置暂停

    int getLastDistance();                              // 查询最后的距离信息（单位：mm）
    bool getIsDistanceOK();

    bool setIsOuterTrigger(bool _is_outer_trigger, bool _is_force = false);     // 设置是否由外部触发查询距离，返回本次设置是否成功
    bool getIsOuterTrigger();

public slots:
    void queryDistOnce(bool *_is_done_ptr = 0);     // @_is_done_ptr 指示指令是否已完成；若为 true，则表示指令已失效，阻塞的等待立即返回

signals:
    void sigDistanceChanged(int _new_dist);                 // 距离值改变信号
    void sigIsDistanceOKChanged(bool _is_ok);               // 状态值“距离是否合适”的改变信号
    void sigMessage(QString _msg);
    void sigCheckSensorTypeFinished();                      // 【检测传感器类型】完成信号

    /* 私有 */
    void sigSetSensorType(enDistSensorType _sensor_type, bool _is_init = false);
    void sigSetIsOuterTrigger(bool _is_outer_trigger, bool _is_force = false);     // 设置是否由外部触发查询距离，返回本次设置是否成功
    void sigInit();
    void sigSetIsOpened(bool _is_open);
    void sigSetIsPaused(bool _is_paused);

protected slots:
    void slot_this_SetSensorType(enDistSensorType _sensor_type, bool _is_init = false);
    void slot_this_SetIsOuterTrigger(bool _is_outer_trigger, bool _is_force = false);     // 设置是否由外部触发查询距离，返回本次设置是否成功
    void slot_this_Init();
    void slot_this_SetIsOpened(bool _is_open);
    void slot_this_SetIsPaused(bool _is_paused);
    void slot_serialDistDetect_readyRead();                 // 【测距通信数据读】信号槽函数
    void slot_timerRequest_timeout();

protected:
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    void sensorInit();                                      // 初始化

    void doOnDistanceChanged(int _new_dist);
    void checkSensorType();                         // 检测传感器类型
    void doOnCheckSensorTypeFinished();             // 【检测传感器类型】完成事件

    std::atomic<enDistSensorType> m_sensorType = enDistSensorType::Unknown;

    QSerialPort *m_serialPort = Q_NULLPTR;
    QMutex m_mutex_serialPort {QMutex::Recursive};      // NOTE: 递归锁，同一线程可重复加锁

    QByteArray m_dataBuff;

    std::atomic<int> m_requestInterval = 200;
    std::atomic<int> m_lastDistance = -1;
    std::atomic<bool> m_isDistanceOK = false;                            // 距离是否合适
    std::atomic<bool> m_isOuterTrigger = false;                          // 是否由外部触发单次查询

    QTimer *m_timerRequest = Q_NULLPTR;
    bool m_isPaused = true;
    std::deque<bool *> m_isQueryFinishedPtrList;

    CDistSensor *m_distSensor {nullptr};                    // 测距传感器 - 当前的
    CDistSensor *m_distSensor_Mb1010 {nullptr};             // 测距传感器 - Mb1010
    CDistSensor *m_distSensor_SIMAN_SDM10 {nullptr};        // 测距传感器 - SIMAN_SDM10

    bool m_isSensorTypeChecking {false};                    // 是否正在检测传感器类型

};

// 数据包参数
struct stPkgParams {
    uchar *frameHead {nullptr};     // 包头       // TODO: 或改为 QByteArray 形式？
    int lenHead {0};                // 包头长度  /* 一般情况下，包头不应存在 \x0，但是难以确保，所以最好指定包头长度，且避免以字符串（因为用到以 \x0 结束这个规则）的形式访问 */
    int lenPkg {0};                 // 包长
    int maxBuffLen {1024};          // 缓冲区限制长度
};

/// ----------------------------------------------------------------------------

// 距离传感器基类（对应类型 enDistSensorType::Unknown）
class CDistSensor : public QObject
{
    Q_OBJECT

    friend class CDistanceDetect;

public:
    explicit CDistSensor(QObject *_parent = nullptr);
    virtual ~CDistSensor();

protected:
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    bool getIsNeedInit();
    bool getIsNeedRequest();

    virtual void getSerialParams(QSerialPort *_serial_port);                                // 获取串口参数
    virtual void init(QSerialPort *_serial_port);                                           // 初始化（打开串口成功后执行）
    virtual void distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush = true);     // 单次查询距离
    virtual bool checkPackage(uchar *_frame_bytes);                                         // 检查数据包合法性
    virtual bool parsePackage(uchar *_frame_bytes, int &_dist);                             // 解包（Xkc）

    virtual void stop(QSerialPort *_serial_port) { Q_UNUSED(_serial_port) }

    void processDataBuff(QByteArray &_buff, std::vector<int> &_dist_list);      // 处理缓冲区数据

    // 从数据包缓冲区中检出数据包（已知长度的）
    static void pickDataPkgFromBuff(QByteArray &_buff, stPkgParams &_pkg_params, funcCheckPkg _func_pkg_check, std::vector<QByteArray> &_pkg_list);

    stPkgParams m_pkgParams {};
    bool m_isNeedInit = true;
    bool m_isNeedRequest = true;

};

/// ----------------------------------------------------------------------------

//
class CDistSensor_Xkc_DYP_A06 : public CDistSensor
{
    Q_OBJECT
public:
    CDistSensor_Xkc_DYP_A06();
    ~CDistSensor_Xkc_DYP_A06() override;

protected:
    void getSerialParams(QSerialPort *_serial_port) override;                               // 设置串口参数
    void init(QSerialPort *_serial_port) override;                                          // 初始化（打开串口成功后执行）
    void distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush = true) override;    // 单次查询距离
    bool checkPackage(uchar *_frame_bytes) override;                                        // 检查数据包合法性
    bool parsePackage(uchar *_frame_bytes, int &_dist) override;                            // 解包（Xkc）
};

//
class CDistSensor_Xkc_KL200 : public CDistSensor
{
    Q_OBJECT
public:
    CDistSensor_Xkc_KL200();
    ~CDistSensor_Xkc_KL200() override;

protected:
    void getSerialParams(QSerialPort *_serial_port) override;                               // 设置串口参数
    void init(QSerialPort *_serial_port) override;                                          // 初始化（打开串口成功后执行）
    void distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush = true) override;    // 单次查询距离
    bool checkPackage(uchar *_frame_bytes) override;                                        // 检查数据包合法性
    bool parsePackage(uchar *_frame_bytes, int &_dist) override;                            // 解包（Xkc）
};

//
class CDistSensor_TFLC02 : public CDistSensor
{
    Q_OBJECT
public:
    CDistSensor_TFLC02();
    ~CDistSensor_TFLC02() override;

protected:
    void getSerialParams(QSerialPort *_serial_port) override;                               // 设置串口参数
    void init(QSerialPort *_serial_port) override;                                          // 初始化（打开串口成功后执行）
    void distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush = true) override;    // 单次查询距离
    bool checkPackage(uchar *_frame_bytes) override;                                        // 检查数据包合法性
    bool parsePackage(uchar *_frame_bytes, int &_dist) override;                            // 解包（Xkc）

protected:
    // 常量
    const uchar FRAME_TAIL = '\xFA';
    const uchar CMD_CODE = '\x81';

};

//
class CDistSensor_TFLuna : public CDistSensor
{
    Q_OBJECT
public:
    CDistSensor_TFLuna();
    ~CDistSensor_TFLuna() override;

protected:
    void getSerialParams(QSerialPort *_serial_port) override;                               // 设置串口参数
    void init(QSerialPort *_serial_port) override;                                          // 初始化（打开串口成功后执行）
    void distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush = true) override;    // 单次查询距离
    bool checkPackage(uchar *_frame_bytes) override;                                        // 检查数据包合法性
    bool parsePackage(uchar *_frame_bytes, int &_dist) override;                            // 解包（Xkc）

    void stop(QSerialPort *_serial_port) override;

protected:

};

//
class CDistSensor_Siman_SDM10 : public CDistSensor
{
    Q_OBJECT
public:
    explicit CDistSensor_Siman_SDM10(QObject *_parent = nullptr);
    ~CDistSensor_Siman_SDM10() override;

protected:
    void getSerialParams(QSerialPort *_serial_port) override;                               // 设置串口参数
    void init(QSerialPort *_serial_port) override;                                          // 初始化（打开串口成功后执行）
    void distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush = true) override;    // 单次查询距离
    bool checkPackage(uchar *_frame_bytes) override;                                        // 检查数据包合法性
    bool parsePackage(uchar *_frame_bytes, int &_dist) override;                            // 解包（Xkc）

    void stop(QSerialPort *_serial_port) override;

    void start(QSerialPort *_serial_port);

protected:
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    uint8_t checkSum(uint8_t *_pbuf, uint16_t _cmdLen);

    CNumericMovingAvgFilter<int> *m_distFilter {nullptr};
    int m_count_pkg {0};

};

#endif // CDISTANCEDETECT_H
