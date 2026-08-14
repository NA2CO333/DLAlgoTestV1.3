#include "distancedetect.h"

#include <QDebug>
#include <QEventLoop>           // 用 QEventLoop 使 QSerialPort 脱离 UI 线程？
#include <QSerialPortInfo>      // TODO: 查询并检查可用串口？

#include "global.h"
#include "windowsmanager.h"

//
CDistanceDetect::CDistanceDetect(QObject *parent) : QObject(parent)
{
    //
    static bool is_first = true;
    if (is_first) {
        qRegisterMetaType<enDistSensorType>("enDistSensorType");

        is_first = false;
    }

    //
    m_lastDistance = -1;

    // 测距通信串口对象
    m_serialPort = new QSerialPort(this);      /* 不管什么条件都创建对象，防止意外访问导致空指针异常 */
    QObject::connect(m_serialPort, &QSerialPort::readyRead, this, &CDistanceDetect::slot_serialDistDetect_readyRead, Qt::QueuedConnection);

    // 手动查询定时器
    m_timerRequest = new QTimer(this);
    QObject::connect(m_timerRequest, &QTimer::timeout, this, &CDistanceDetect::slot_timerRequest_timeout, Qt::QueuedConnection);

    // 确保 m_distSensor 不为空
    m_distSensor = new CDistSensor(this);

    //
    m_distSensor = new CDistSensor(this);
    m_distSensor_Mb1010 = new CDistSensor();        // NOTE: 超声由底板串口接收，不需距离检测模块处理
    m_distSensor_SIMAN_SDM10 = new CDistSensor_Siman_SDM10();

    //
    QObject::connect(this, &CDistanceDetect::sigSetSensorType       , this, &CDistanceDetect::slot_this_SetSensorType       , Qt::QueuedConnection);
    QObject::connect(this, &CDistanceDetect::sigSetIsOuterTrigger   , this, &CDistanceDetect::slot_this_SetIsOuterTrigger   , Qt::QueuedConnection);
    QObject::connect(this, &CDistanceDetect::sigInit                , this, &CDistanceDetect::slot_this_Init                , Qt::QueuedConnection);
    QObject::connect(this, &CDistanceDetect::sigSetIsOpened         , this, &CDistanceDetect::slot_this_SetIsOpened         , Qt::QueuedConnection);
    QObject::connect(this, &CDistanceDetect::sigSetIsPaused         , this, &CDistanceDetect::slot_this_SetIsPaused         , Qt::QueuedConnection);

}

CDistanceDetect::~CDistanceDetect()
{

}

bool CDistanceDetect::isSensorNeedPort3(enDistSensorType _dist_type)
{
    return ( (enDistSensorType::Mb1010 != _dist_type) );         // TODO: 怎么在以后的维护中防止漏改？
}

void CDistanceDetect::setSensorType(enDistSensorType _sensor_type, bool _is_init)
{
    emit sigSetSensorType(_sensor_type, _is_init);
}

void CDistanceDetect::init()
{
    emit sigInit();
}

bool CDistanceDetect::checkAndCorrectDistType(enDistSensorType &_dist_type)
{
    if (_dist_type < enDistSensorType::Min || _dist_type > enDistSensorType::Max) {
        _dist_type = enDistSensorType::Min;      // TODO: 有没办法自动检测？
        return false;
    }
    return true;
}

void CDistanceDetect::slot_this_SetSensorType(enDistSensorType _sensor_type, bool _is_init)
{
    if (_sensor_type != sensorType() || _is_init) {
        if (m_distSensor
                && m_distSensor != m_distSensor_Mb1010
                && m_distSensor != m_distSensor_SIMAN_SDM10) {
            delete m_distSensor;
            m_distSensor = nullptr;
        }

        // 根据传感器类型构造传感器对象
        if (!m_distSensor) {
            if (enDistSensorType::Mb1010 == _sensor_type) {
                m_distSensor = m_distSensor_Mb1010;
            //} else if (enDistSensorType::Xkc_DYP_A06 == _sensor_type) {
            //    m_distSensor = new CDistSensor_Xkc_DYP_A06();
            //} else if (enDistSensorType::Xkc_KL200 == _sensor_type) {
            //    m_distSensor = new CDistSensor_Xkc_KL200();
            //} else if (enDistSensorType::TFLC02 == _sensor_type) {
            //    m_distSensor = new CDistSensor_TFLC02();
            //} else if ((enDistSensorType::TFLuna == _sensor_type) ||
            //           (enDistSensorType::TFminiS == _sensor_type)) {
            //    m_distSensor = new CDistSensor_TFLuna();
            } else if (enDistSensorType::SIMAN_SDM10 == _sensor_type) {
                m_distSensor = m_distSensor_SIMAN_SDM10;
            } else {
                logCritical(QString("%1::%2(): type not valid: %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg((int)_sensor_type), CGlobal::LOG_DISTANCE);
                m_distSensor = new CDistSensor();     /* 确保 m_distSensor 不为空 */
            }
        }

        //
        m_sensorType = _sensor_type;

        //
        m_isOuterTrigger = (false /*|| enDistSensorType::TFLuna == _sensor_type || enDistSensorType::TFminiS == _sensor_type*/);

        //
        sensorInit();
    }
}

void CDistanceDetect::setRequestInterval(int _interval)
{
    if (_interval < 30) {
        _interval = 30;
        logWarning("requestInterval too small !", CGlobal::LOG_DISTANCE);
    }

    m_requestInterval = _interval;
}

bool CDistanceDetect::setIsOpened(bool _is_open)
{
    emit sigSetIsOpened(_is_open);
    return true;
}

void CDistanceDetect::setIsPaused(bool _is_paused)
{
    emit sigSetIsPaused(_is_paused);
}

void CDistanceDetect::slot_this_SetIsOpened(bool _is_open)
{
    logDebug(QString("%1::%2: entered, _is_open = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(Util::bool2str(_is_open)), CGlobal::LOG_DISTANCE);

    if (enDistSensorType::Unknown == sensorType()) {
        logCritical(QString("%1::%2(): sensorType not setted!").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
        return;
    }

    //
    QMutexLocker locker(&m_mutex_serialPort);

    //
    if (m_serialPort->isOpen() == _is_open) {
        return;
    }

    //
    if (_is_open) {
        if (!m_serialPort->isOpen()) {
            //logDebug(QString("%1::%2(): opening...").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);

            QString port_name = G_COM_DISTANCE;
            m_serialPort->setPortName(port_name);            // 设置串口路径

            m_distSensor->getSerialParams(m_serialPort);       // 设置串口其它参数（不同传感器有不同的参数）

            bool succ = m_serialPort->open(QIODevice::ReadWrite);
            logDebug(QString("%1::%2(): opening executed, succ = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(Util::bool2str(succ)), CGlobal::LOG_DISTANCE);

            if (succ) {
                m_distSensor->init(m_serialPort);
                m_serialPort->waitForBytesWritten(300);

                // 清空发送和接收缓冲区
                m_serialPort->clear(QSerialPort::AllDirections);

                //
                slot_this_SetIsOuterTrigger(m_isOuterTrigger, true);
            } else {
                QSerialPort::SerialPortError err = m_serialPort->error();
                QString msg = QString("Open distance-detect serial port failed: err=%1, ").arg((int)err) + m_serialPort->errorString();
                emit sigMessage(msg);
                logCritical(msg, CGlobal::LOG_DISTANCE);
            }
        }

        m_isQueryFinishedPtrList.clear();
    } else {
        m_timerRequest->stop();

        if (m_serialPort->isOpen()) {
            m_distSensor->stop(m_serialPort);
            m_serialPort->waitForBytesWritten(300);

            //m_serialPort->clear();
            m_serialPort->close();
        }
    }

    // 初始化 “是否暂停” 状态值，若是打开，则为否，否则为是
    slot_this_SetIsPaused(!_is_open);

    //
    return;
}

void CDistanceDetect::slot_this_Init()
{
    logDebug(QString("%1::%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    // 检测传感器类型
    checkSensorType();
}

void CDistanceDetect::sensorInit()
{
    logDebug(QString("%1::%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    //
    if (enDistSensorType::Unknown == sensorType()) {
        // TODO:

        //
        return;
    } else if (m_distSensor->getIsNeedInit()) {
        slot_this_SetIsOpened(true);      /* 打开时已 init，不需再调用 init */
        slot_this_SetIsOpened(false);
    }
}

int CDistanceDetect::getLastDistance()
{
    return m_lastDistance;
}

bool CDistanceDetect::getIsDistanceOK()
{
    return m_isDistanceOK;
}

bool CDistanceDetect::setIsOuterTrigger(bool _is_outer_trigger, bool _is_force)
{
    emit sigSetIsOuterTrigger(_is_outer_trigger, _is_force);
    return true;
}

void CDistanceDetect::slot_this_SetIsPaused(bool _is_paused)
{
    logDebug(QString("%1::%2(): entered, _is_paused = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(Util::bool2str(_is_paused)));

    //
    if (_is_paused != m_isPaused) {
        m_isPaused = _is_paused;

        // 由暂停转为非暂停时，清空暂停期间收到的数据
        if (!_is_paused)           /* 传入状态参数为非暂停，且前面已判断与本对象当前值不同，那么旧状态肯定为暂停 */        // TODO: 不管是由暂停转为非暂停，还是反过来时，都应清除缓冲数据？
        {
            //QMutexLocker locker(&m_mutex_serialPort);     // NOTE: 此函数被 setIsOpened() 调用，而前者已加锁
            if (m_serialPort->isOpen()) {
                m_serialPort->clear(/*QSerialPort::Input*/);
            }
        }
    }
}

void CDistanceDetect::doOnDistanceChanged(int _new_dist)
{
    logDebug(QString("%1::%2(): entered, _new_dist = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(_new_dist));

    //if (_new_dist != m_lastDistance)
    {
        if (!m_isSensorTypeChecking) {
            //
            m_lastDistance = _new_dist;
            emit sigDistanceChanged(m_lastDistance);
        } else {                                        // 若正在检测传感器类型，则判定已检测到
            //
            logDebug(QString("%1::%2(): Distance received, SensorType is enDistSensorType::SIMAN_SDM10").arg(S_CLASS_NAME).arg(__FUNCTION__));
            doOnCheckSensorTypeFinished();
        }
    }
}

void CDistanceDetect::checkSensorType()
{
    logDebug(QString("%1::%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    // 测距传感器默认 SIMAN_SDM10，先自检是否存在
    slot_this_SetSensorType(enDistSensorType::SIMAN_SDM10, true);
    /*bool succ_open =*/ slot_this_SetIsOpened(true);
    //if (!succ_open) {
    //    return;
    //}
    m_isSensorTypeChecking = true;

    //
    QTimer::singleShot(3000, this, [this] () {
        // 若还在检测，则判定检测不到
        if (m_isSensorTypeChecking) {
            //
            logDebug(QString("%1::%2(): checking timeout, SensorType is enDistSensorType::Mb1010").arg(S_CLASS_NAME).arg(__FUNCTION__));
            doOnCheckSensorTypeFinished();

            //
            slot_this_SetSensorType(enDistSensorType::Mb1010);
            logInfo(QString("%1::checkSensorType(): enDistSensorType::SIMAN_SDM10 not found, set to enDistSensorType::Mb1010")
                    .arg(S_CLASS_NAME));
        }
    });
}

void CDistanceDetect::doOnCheckSensorTypeFinished()
{
    logDebug(QString("%1::%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    //
    m_isSensorTypeChecking = false;
    slot_this_SetIsOpened(false);

    //
    emit sigCheckSensorTypeFinished();
}

void CDistanceDetect::slot_timerRequest_timeout()
{
    logDebug(QString("%1::%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    queryDistOnce();
}

void CDistanceDetect::queryDistOnce(bool *_is_done_ptr)
{
    if (m_isPaused) {
        logWarning(QString("%1::%2(): isPaused, but query is called!").arg(S_CLASS_NAME).arg(__FUNCTION__));
        return;
    }

    if (!m_distSensor->getIsNeedRequest()) {
        logWarning(QString("%1::%2(): not isNeedRequest, but query is called!").arg(S_CLASS_NAME).arg(__FUNCTION__));
        return;
    }

    // 设置等待标志
    if (_is_done_ptr) {
        //{
        //    QMutexLocker locker(&m_mutex_serialPort);
        //    if (m_serialPort->isOpen()) {
        //        m_serialPort->clear(QSerialPort::AllDirections);     // 如果需要等待应答，清掉之前的输入和输出    TODO: 这里不应该清除？
        //    }
        //}

        if (m_isQueryFinishedPtrList.size() > 5) {
            logWarning(QString("%1::%2(): isQueryFinishedPtrList.size too large !").arg(S_CLASS_NAME).arg(__FUNCTION__));
        }

        m_isQueryFinishedPtrList.push_back(_is_done_ptr);
    }

    // 执行单次查询
    {
        QMutexLocker locker(&m_mutex_serialPort);
        m_distSensor->distanceSingleQuery(m_serialPort);
    }

}

bool CDistanceDetect::getIsOuterTrigger()
{
    return m_isOuterTrigger;
}

void CDistanceDetect::slot_this_SetIsOuterTrigger(bool _is_outer_trigger, bool _is_force)
{
    if (_is_outer_trigger != m_isOuterTrigger || _is_force) {
        //bool succ = false;              // 是否设置成功
        bool need_timer = false;        // 是否需要启动定时器

        do {
            if (m_distSensor->getIsNeedRequest()) {
                if (!_is_outer_trigger) {
                    need_timer = true;      // 如果不由外部触发，则需要启动定时器
                }
            } else {
                if (_is_outer_trigger) {                 // 如果传感器不需触发，则不可设为由外部触发，本次设置失败
                    break;
                }
            }

            //
            m_isOuterTrigger = _is_outer_trigger;
            //succ = true;
        } while (false);

        //
        if (need_timer) {
            m_timerRequest->start(m_requestInterval);
        }

        //
        return /*succ*/;
    }

    return /*true*/;
}

void CDistanceDetect::slot_serialDistDetect_readyRead()
{
    logDebug(QString("%1::%2(): entered ... sensorType = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg((int)sensorType()), CGlobal::LOG_DISTANCE);

    //
    if (!m_isPaused) {
        // 读取数据
        QByteArray data_new;
        {
            QMutexLocker locker(&m_mutex_serialPort);
            if (m_serialPort->isOpen()) {
                data_new = m_serialPort->readAll();
                logDebug(QString("%1::%2(): data_new = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(data_new.toHex().constData()), CGlobal::LOG_DISTANCE);
            } else {

            }
        }
        if (data_new.isEmpty()) {
            return;
        }

        // 将读取到的数据添加到缓冲区
        m_dataBuff.append(data_new);
        //logDebug(QString("%1::%2(): m_dataBuff = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(m_dataBuff.toHex().constData()), CGlobal::LOG_DISTANCE);

        // 数据处理
        static std::vector<int> dist_list;
        dist_list.clear();
        m_distSensor->processDataBuff(m_dataBuff, dist_list);

        // 应答处理
        if (m_isQueryFinishedPtrList.size() > 0) {
            bool *is_done = *(m_isQueryFinishedPtrList.begin());
            m_isQueryFinishedPtrList.pop_front();

            if (m_isQueryFinishedPtrList.size() > 5) {
                logWarning(QString("%1::%2(): isQueryFinishedPtrList.size() > 50 !").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
                m_isQueryFinishedPtrList.clear();
            }

            if ((is_done && !(*is_done)) && dist_list.size() > 0) {
                *is_done = true;
            }
        }

        // 【距离改变】信号触发
        for (int dist : dist_list) {
            logDebug(QString::asprintf("received distance value: %d", dist), CGlobal::LOG_DISTANCE);

            doOnDistanceChanged(dist);
        }
    } else {
        {
            //QMutexLocker locker(&m_mutex_serialPort);
            //if (m_serialPort->isOpen()) {
            //    m_serialPort->clear(QSerialPort::Input);
            //}
        }
    }

    //
    //logDebug(QString("%1::%2(): out").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
}

///=================================================================================================
/// class CDistSensor

//
CDistSensor::CDistSensor(QObject *_parent) : QObject(_parent)
{
    //memset(&m_pkgParams, 0, sizeof(stPkgParams));
}

CDistSensor::~CDistSensor()
{
}

bool CDistSensor::getIsNeedInit()
{
    return m_isNeedInit;
}

bool CDistSensor::getIsNeedRequest()
{
    return m_isNeedRequest;
}

bool CDistSensor::parsePackage(uchar *_frame_bytes, int &_dist)
{
    Q_UNUSED(_frame_bytes)
    Q_UNUSED(_dist)
    return false;
}

bool CDistSensor::checkPackage(uchar *_frame_bytes)
{
    Q_UNUSED(_frame_bytes)
    return false;
}

void CDistSensor::distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush)
{
    Q_UNUSED(_serial_port)
    Q_UNUSED(_is_flush)
}

void CDistSensor::init(QSerialPort *_serial_port)
{
    Q_UNUSED(_serial_port)
}

void CDistSensor::getSerialParams(QSerialPort *_serial_port)
{
    Q_UNUSED(_serial_port)
}

void CDistSensor::processDataBuff(QByteArray &_buff, std::vector<int> &_dist_list)
{
    static std::vector<QByteArray> pkg_list;
    pkg_list.clear();

    funcCheckPkg func_check_pkg = std::bind(&CDistSensor::checkPackage, this, std::placeholders::_1);       // TODO: std::bind() 的消耗怎样？不用每次都 bind 吧？
    pickDataPkgFromBuff(_buff, m_pkgParams, func_check_pkg, pkg_list);        // TODO: 非距离数据包的处理？在 checkPackage() 里发射信号？

    int dist;
    bool succ;
    for (QByteArray frame_array : pkg_list) {
        succ = parsePackage((uchar *)frame_array.data(), dist);
        if (!succ) {
            dist = -1;
        }
        _dist_list.push_back(dist);
    }
}

void CDistSensor::pickDataPkgFromBuff(QByteArray &_buff, stPkgParams &_pkg_params, funcCheckPkg _func_pkg_check, std::vector<QByteArray> &_pkg_list)
{
    // (1)如果缓冲区长度大于等于数据包长度，则循环处理（注意避免死循环的形成：检索到的包头，处理完后要移除；检索不到包头，要退出循环）
    int pos_head;
    bool is_check_ok;
    do {
        //logDebug(QString("%1::%2(): loop once, start").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);

        // (2)从头部开始检索包头
        pos_head = _buff.indexOf((char *)_pkg_params.frameHead);

        // (2)如果检索到包头，则截包。否则，退出循环
        if (pos_head >= 0) {
            //logDebug(QString("%1::%2(): pos_head = %d", pos_head), CGlobal::LOG_DISTANCE);

            // 截包
            QByteArray frame_array = _buff.mid(pos_head, _pkg_params.lenPkg);
            uchar *frame_bytes = (uchar *)frame_array.data();
            //logDebug(QString("%1::%2(): received package: ").arg(S_CLASS_NAME).arg(__FUNCTION__) + frame_array.toHex(), CGlobal::LOG_DISTANCE);

            // 检查数据包
            is_check_ok = _func_pkg_check(frame_bytes);
            if (is_check_ok) {
                _pkg_list.push_back(frame_array);
            }

            // (3)如果解包成功，则移除解析过的数据包及其位置之前的数据。否则，移除包头
            if (is_check_ok) {
                _buff.remove(0, pos_head + _pkg_params.lenPkg);
                //logDebug(QString("%1::%2(): cut %3 byte").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(pos_head + _pkg_params.lenPkg), CGlobal::LOG_DISTANCE);
            } else {
                _buff.remove(0, 1);                                     /* 不管包头多少字节，移除1字节即可，且可避免等值双字节包头处理出错 */
                //logDebug(QString("%1::%2(): cut 1 byte").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
            }
        } else {
            //logDebug(QString("%1::%2(): break").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
            break;
        }
    } while (_buff.length() >= _pkg_params.lenPkg);
    //logDebug(QString("%1::%2(): loop end").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);

    // 防止通信异常导致大量数据一直未清除
    if (_pkg_params.maxBuffLen > 0) {
        if (_buff.size() > _pkg_params.maxBuffLen) {
            _buff.clear();
            logWarning(QString("%1::%2(): size of _buff abnormal!").arg(S_CLASS_NAME).arg(__FUNCTION__), CGlobal::LOG_DISTANCE);
        }
    }
}

///=================================================================================================
/// class CDistSensor_Xkc_DYP_A06

CDistSensor_Xkc_DYP_A06::CDistSensor_Xkc_DYP_A06()
{
    m_pkgParams = {
        (uchar *)"\xFF",
        1,
        4,
    };
    m_isNeedInit = false;
    m_isNeedRequest = false;
}

CDistSensor_Xkc_DYP_A06::~CDistSensor_Xkc_DYP_A06()
{
}

void CDistSensor_Xkc_DYP_A06::getSerialParams(QSerialPort *_serial_port)
{
    _serial_port->setBaudRate(QSerialPort::Baud9600);
    _serial_port->setDataBits(QSerialPort::Data8);
    _serial_port->setStopBits(QSerialPort::OneStop);
    _serial_port->setParity(QSerialPort::NoParity);
}

void CDistSensor_Xkc_DYP_A06::init(QSerialPort *_serial_port)
{
    Q_UNUSED(_serial_port)

}

void CDistSensor_Xkc_DYP_A06::distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush)
{
    Q_UNUSED(_serial_port)
    Q_UNUSED(_is_flush)
}

bool CDistSensor_Xkc_DYP_A06::checkPackage(uchar *_frame_bytes)
{
    return (m_pkgParams.frameHead[0] == _frame_bytes[0]);
}

bool CDistSensor_Xkc_DYP_A06::parsePackage(uchar *_frame_bytes, int &_dist)
{
    int hi = _frame_bytes[1];
    int lo = _frame_bytes[2];
    _dist = ((hi << 8) + lo);
    return true;
}

///=================================================================================================
/// class CDistSensor_Xkc_KL200

CDistSensor_Xkc_KL200::CDistSensor_Xkc_KL200() : CDistSensor()
{
    m_pkgParams = {
        (uchar *)"\x62",
        1,
        9,
    };
    m_isNeedInit = true;
    m_isNeedRequest = true;
}

CDistSensor_Xkc_KL200::~CDistSensor_Xkc_KL200()
{
}

bool CDistSensor_Xkc_KL200::checkPackage(uchar *_frame_bytes)
{
    return ((uchar)'\x33' == _frame_bytes[1]);
}

bool CDistSensor_Xkc_KL200::parsePackage(uchar *_frame_bytes, int &_dist)
{
    /* 包格式：包头 0x62 + 命令码 1byte + 长度 1bbyte + 地址 2bytes + 数据（高位在前） 2bytes + 命令/应达 1byte + 校验 1byte */

    bool is_parse_ok = true;

    //
    int hi = (unsigned char)_frame_bytes[5];
    int lo = (unsigned char)_frame_bytes[6];

    _dist = ((hi << 8) + lo);

    //
    return is_parse_ok;
}

void CDistSensor_Xkc_KL200::getSerialParams(QSerialPort *_serial_port)
{
    _serial_port->setBaudRate(QSerialPort::Baud9600);
    _serial_port->setDataBits(QSerialPort::Data8);
    _serial_port->setStopBits(QSerialPort::OneStop);
    _serial_port->setParity(QSerialPort::NoParity);
}

void CDistSensor_Xkc_KL200::init(QSerialPort *_serial_port)
{
    // TODO: 恢复出厂设置？

    // 设置手动查询
    //long long n;
    /*n =*/ _serial_port->write("\x62\x34\x09\xFF\xFF\x00\x00\x00\x5F", 9);
    //logDebug(QString::asprintf("CDistSensor_Xkc::init() serial command written, n=%lld", n), CGlobal::LOG_DISTANCE);
    _serial_port->flush();

}

void CDistSensor_Xkc_KL200::distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush)
{
    _serial_port->write("\x62\x33\x09\xFF\xFF\x00\x01\x00\x59", 9);
    if (_is_flush) {
        _serial_port->flush();
    }
}

///=================================================================================================
/// class CDistSensor_TFLC02

CDistSensor_TFLC02::CDistSensor_TFLC02()
{
    m_pkgParams = {
        (uchar *)"\x55\xAA",
        2,
        8,
    };
    m_isNeedInit = false;
    m_isNeedRequest = true;
}

CDistSensor_TFLC02::~CDistSensor_TFLC02()
{
}

// 检查数据包合法性
bool CDistSensor_TFLC02::checkPackage(uchar *_frame_bytes)
{
    return (FRAME_TAIL == _frame_bytes[m_pkgParams.lenPkg - 1]) && (CMD_CODE == _frame_bytes[2]);
}

// 解包（TFLC02）
bool CDistSensor_TFLC02::parsePackage(uchar *_frame_bytes, int &_dist)
{
    /* 包结构：包头 0x55 0xAA + 1byte 命令码 + 1byte 数据字段字节长度 + 不定长数据字段（距离数值 dist_hi 1byte + dist_lo 1byte + status 1byte） + 包尾 0xFA
     */

    // 距离数据结构
    struct stDistData {
        uint16_t dist;      // 距离，单位毫米
        uint8_t status;     // 状态，0-正常，其它值表示错误码
    };

    // 错误码（可多个值逻辑或运算）
    enum enErrCode {
        enErrCode_ValidData     = 0x00,     // 正常
        enErrCode_VcselShort    = 0x01,     // VCSEL 短路？
        enErrCode_LowSignal     = 0x02,     // 未获得反射信号
        enErrCode_LowSn         = 0x04,     // 被测物过远或者被测物颜色材质吸光严重
        enErrCode_TooMuchAmb    = 0x08,     // 干扰光大
        enErrCode_WAF           = 0x10,     // 环绕错误（DEC = 16）
        enErrCode_CalErr        = 0x20,     // 计算错误（DEC = 32）
        enErrCode_CrossTalkErr  = 0x80,     // 串扰大（DEC = 128）
    };

    //
    bool is_parse_ok = false;

    //
    stDistData dist_data;
    memset(&dist_data, 0, sizeof(stDistData));

    uint16_t hi = (uchar)_frame_bytes[4];
    uint16_t lo = (uchar)_frame_bytes[5];
    dist_data.dist = (hi << 8) | lo;
    dist_data.status = (uchar)_frame_bytes[6];

    if (enErrCode_ValidData == dist_data.status) {
        _dist = dist_data.dist;

        is_parse_ok = true;
    } else {
        _dist = -1;

        logWarning(QString::asprintf("CDistSensor_TFLC02::parsePackage() errcode = %d", dist_data.status), CGlobal::LOG_DISTANCE);
    }

    //
    return is_parse_ok;
}

void CDistSensor_TFLC02::getSerialParams(QSerialPort *_serial_port)
{
    _serial_port->setBaudRate(QSerialPort::Baud115200);
    _serial_port->setDataBits(QSerialPort::Data8);
    _serial_port->setStopBits(QSerialPort::OneStop);
    _serial_port->setParity(QSerialPort::NoParity);
}

void CDistSensor_TFLC02::init(QSerialPort *_serial_port)
{
    Q_UNUSED(_serial_port)

    // TODO: 恢复出厂设置？

    // 这个型号不用初始化

}

void CDistSensor_TFLC02::distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush)
{
    _serial_port->write("\x55\xAA\x81\x00\xFA", 5);
    if (_is_flush) {
        _serial_port->flush();
    }
}

///=================================================================================================
/// class CDistSensor_TFLuna

CDistSensor_TFLuna::CDistSensor_TFLuna()
{
    m_pkgParams = {
        (uchar *)"\x59\x59",
        2,
        9,
    };
    m_isNeedInit = true;
    m_isNeedRequest = true;
}

CDistSensor_TFLuna::~CDistSensor_TFLuna()
{
}

// 检查数据包合法性
bool CDistSensor_TFLuna::checkPackage(uchar *_frame_bytes)
{
    Q_UNUSED(_frame_bytes)
    return true;
}

// 解包（TFLuna 或 TFmini-S）
bool CDistSensor_TFLuna::parsePackage(uchar *_frame_bytes, int &_dist)
{
    /* 包结构（指令）：
        Head：       包头，固定为 0x5A。
        Len：        包含包头到 Check_sum 的所有字节的长度，单位为字节，范围 4~255。
        ID：         指示如何解析 Payload 数据。
        Payload：    数据段，根据 ID 进行解析，可能没有数据段。
        Checksum：   对从 Head 到 Payload 的所有字节进行求和计算，取低 8 位。
     */
    /* 串口输出的格式（Format=0x06，9字节mm）（共有 10 种格式）：
     *  包头：       0x59 0x59，2bytes
     *  距离：       lo 1byte + hi 1byte
     *  信号强度：    lo 1byte + hi 1byte     （范围0~65535，小于 100 时数值不可靠，过曝时为 65535
     *  温度:        lo 1byte + hi 1byte     （摄氏度 = temp / 8 - 256）
     *  CheckSum:   1byte
     */

    // 距离数据结构
    struct stDistData {
        uint16_t dist;      // 距离，单位毫米
        uint16_t amp;       // 信号强度
        uint16_t temp;      // 温度
    };

    //
    bool is_parse_ok = false;

    //
    //uchar check_sum_byte = _buff[pos_head + PKG_LEN - 1];
    bool check_sum_ok = true;   // TODO: calc check sum
    bool is_format_ok = check_sum_ok;
    if (is_format_ok) {
        stDistData dist_data;
        memset(&dist_data, 0, sizeof(stDistData));

        uint16_t hi, lo;
        hi = (uchar)_frame_bytes[3];
        lo = (uchar)_frame_bytes[2];
        dist_data.dist  = (hi << 8) | lo;
        hi = (uchar)_frame_bytes[5];
        lo = (uchar)_frame_bytes[4];
        dist_data.amp   = (hi << 8) | lo;

        if (dist_data.amp >= 100) {         /* “小于 100 时数值不可靠” */
            _dist = dist_data.dist;

            is_parse_ok = true;
        } else {
            logWarning(QString::asprintf("CDistSensor_TFLuna::parsePackage() amp = %d", dist_data.amp), CGlobal::LOG_DISTANCE);
        }
    }

    //
    return is_parse_ok;
}

void CDistSensor_TFLuna::getSerialParams(QSerialPort *_serial_port)
{
    _serial_port->setBaudRate(QSerialPort::Baud115200);
    _serial_port->setDataBits(QSerialPort::Data8);
    _serial_port->setStopBits(QSerialPort::OneStop);
    _serial_port->setParity(QSerialPort::NoParity);
    _serial_port->setFlowControl(QSerialPort::NoFlowControl);
}

void CDistSensor_TFLuna::init(QSerialPort *_serial_port)
{
    // TODO: 恢复出厂设置？

    long long n;

    // 设置输出帧率（低位在前，最小值 0，最大值 TF-Luna 为 250(0x00FA)，TFmini-S 为 1000(0x03E8)。当 =0 时，即为手动查询。默认值（正常功耗）两型号都为 100(0x64)）
    char cmd_fps[] = {0x5A, 0x06, 0x03, 0x00, 0x00, 0x00};
    n = _serial_port->write(cmd_fps, 6);
    logDebug(QString::asprintf("CDistSensor_TFLuna::init() serial command written, n=%lld", n), CGlobal::LOG_DISTANCE);

    // 设置输出格式（Format = 0x06，9字节mm）
    char cmd_fmt[] = {0x5A, 0x05, 0x05, 0x06, 0x00};
    n = _serial_port->write(cmd_fmt, 5);

    // 打开输出使能
    char cmd_enabling[] = {0x5A, 0x05, 0x07, 0x01, 0x00};
    n = _serial_port->write(cmd_enabling, 5);

    //
    _serial_port->flush();
}

void CDistSensor_TFLuna::distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush)
{
    //
    char cmd_query[] = {0x5A, 0x04, 0x04, 0x00};
    _serial_port->write(cmd_query, 4);
    if (_is_flush) {
        _serial_port->flush();
    }
}

void CDistSensor_TFLuna::stop(QSerialPort *_serial_port)
{
    // 关闭输出
    char cmd_stop[] = {0x5A, 0x05, 0x07, 0x00, 0x00};
    _serial_port->write(cmd_stop, 5);
    _serial_port->flush();
}

///=================================================================================================
/// class CDistSensor_Siman_SDM10

CDistSensor_Siman_SDM10::CDistSensor_Siman_SDM10(QObject *_parent) : CDistSensor(_parent)
{
    m_pkgParams = {
        (uchar *)"\x5C",
        1,
        4,
     };
    m_isNeedInit = false;
    m_isNeedRequest = false;

    //
    m_distFilter = new CNumericMovingAvgFilter<int>(50, -1);    // NOTE: (2026-08-12) 规格书说“测量频率”是 50Hz，但实测帧率约 100 fps。

}

CDistSensor_Siman_SDM10::~CDistSensor_Siman_SDM10()
{

}

void CDistSensor_Siman_SDM10::getSerialParams(QSerialPort *_serial_port)
{
    /* 可用波特率：
     * 9600
     * 19200
     * 38400
     * 115200
     * 230400
     * 256000
     * 460800
     * （其他波特率不支持）
     *
     * 修改波特率：
     * 5A 06 02 80 04 73(checksum)
     * 5A 86 02 80 04 F3(checksum)
     */

    //
    constexpr int DEFAULT_BAUDRATE  = 460800;        // 默认波特率（可通过命令修改）

    _serial_port->setBaudRate(DEFAULT_BAUDRATE);
    _serial_port->setDataBits(QSerialPort::Data8);
    _serial_port->setStopBits(QSerialPort::OneStop);
    _serial_port->setParity(QSerialPort::NoParity);
    _serial_port->setFlowControl(QSerialPort::NoFlowControl);
}

void CDistSensor_Siman_SDM10::init(QSerialPort *_serial_port)
{
    // 先停止测距（测距模组默认上电后即开始测距）
    //stop(_serial_port);
    start(_serial_port);
    Q_UNUSED(_serial_port)
}

void CDistSensor_Siman_SDM10::distanceSingleQuery(QSerialPort *_serial_port, bool _is_flush)
{
    Q_UNUSED(_serial_port)
    Q_UNUSED(_is_flush)
}

uint8_t CDistSensor_Siman_SDM10::checkSum(uint8_t *_pbuf, uint16_t _cmdLen)
{
    uint8_t cmd_sum=0;
    uint16_t i;
    for(i=0;i<_cmdLen;i++)
    {
        cmd_sum += _pbuf[i];
    }
    cmd_sum = (~cmd_sum);
    return  cmd_sum;
}

bool CDistSensor_Siman_SDM10::checkPackage(uchar *_frame_bytes)
{
    // 从第二个字节开始到倒数第二个字节结束，求和取反
    const uint8_t sum = checkSum(_frame_bytes + 1, 2);
    return (sum == _frame_bytes[3]);
}

bool CDistSensor_Siman_SDM10::parsePackage(uchar *_frame_bytes, int &_dist)
{
    m_count_pkg++;
    logDebug(QString("%1::%2(): entered, m_count_pkg = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(m_count_pkg));

    /* 包结构：
     * Head        : 0x5C
     * Distance    : 2字节，unsigned int，小端模式
     * Checksum    : 1字节
     *
     * 若 Distance == 65535，则表示测不出距离。
     */

    //
    static constexpr uint16_t VAL_INVALID = 65535;

    //
    uint8_t hi, lo;
    hi = (uchar)_frame_bytes[2];
    lo = (uchar)_frame_bytes[1];
    _dist  = (hi << 8) | lo;

    //
    _dist = m_distFilter->inputValue(_dist);

    //
    bool is_parse_ok = (VAL_INVALID != _dist);

    //
    return is_parse_ok;
}

void CDistSensor_Siman_SDM10::start(QSerialPort *_serial_port)
{
    static const uchar CMD[] {0x5A, 0x0A, 0x02, 0x02, 0x00, 0xF1};      // 指令：开始测距（永久生效）
    static constexpr int LEN = 6;                                       // 指令长度

    _serial_port->write((const char *)CMD, LEN);
    _serial_port->flush();

    m_count_pkg = 0;
}

void CDistSensor_Siman_SDM10::stop(QSerialPort *_serial_port)
{
    static const uchar CMD[] {0x5A, 0x0A, 0x02, 0x00, 0x00, 0xF3};      // 指令：停止测距（永久生效）
    static constexpr int LEN = 6;                                       // 指令长度

    _serial_port->write((const char *)CMD, LEN);
    _serial_port->flush();
}
