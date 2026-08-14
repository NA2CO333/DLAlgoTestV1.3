#include "websocket.h"

#include "logger.h"

namespace Net {
namespace Remote {

///=============================================================================================================
/// class CWebSocket

// 默认心跳间隔（秒）
#define DEFAULT_HEART_BEAT_INTERVAL 10

// 默认的 pong 超时时间（秒）
#define DEFAULT_PONG_TIMEOUT        20

// 默认的重连间隔时间（秒）
#define DEFAULT_RECONNECT_INTERVAL  5

//
static const char *LOG_TAG = "datatrans";

//
CWebSocket::CWebSocket(const QString &origin, QWebSocketProtocol::Version version, QObject *parent)
    : QWebSocket(origin, version, parent)
{
    //
    heartBeatInterval = DEFAULT_HEART_BEAT_INTERVAL;
    pongTimeout = DEFAULT_PONG_TIMEOUT;
    reconnectInterval = DEFAULT_RECONNECT_INTERVAL;

    // 信号槽连接
    QObject::connect(this, &CWebSocket::connected, this, &CWebSocket::slot_this_connected, Qt::QueuedConnection);
    QObject::connect(this, &CWebSocket::disconnected, this, &CWebSocket::slot_this_disconnected, Qt::QueuedConnection);
    QObject::connect(this, &CWebSocket::pong, this, &CWebSocket::slot_this_pong, Qt::QueuedConnection);
    QObject::connect(this, &CWebSocket::stateChanged, this, &CWebSocket::slot_this_stateChanged, Qt::QueuedConnection);
    QObject::connect(this, QOverload<QAbstractSocket::SocketError>::of(&CWebSocket::error), this, &CWebSocket::slot_this_error, Qt::QueuedConnection);

    QObject::connect(this, &CWebSocket::sigSetIsOpened, this, &CWebSocket::slot_this_SetIsOpened, Qt::QueuedConnection);
    QObject::connect(this, &CWebSocket::sigReconnect, this, &CWebSocket::slot_this_Reconnect, Qt::QueuedConnection);

    QObject::connect(&timer, &QTimer::timeout, this, &CWebSocket::slot_timer_timeout, Qt::QueuedConnection);

    // 初始化 elapsedTimer，避免调用 elapsed() 的返回值不可预料
    pongElapsed.start();

}

QString CWebSocket::enumToStrEn_TimerStat(CWebSocket::enConnStat _stat)
{
    switch (_stat) {
    case connStat_stopped:
        return "stopped";
    case connStat_heartBeat:
        return "HeartBeat";
    case connStat_reconnect:
        return "Reconnect";
    default:
        return "??";
    }
}

QString CWebSocket::enumToStr_TimerStat(CWebSocket::enConnStat _stat)
{
    switch (_stat) {
    case connStat_stopped:
        return tr("停止状态");
    case connStat_heartBeat:
        return tr("心跳状态");
    case connStat_reconnect:
        return tr("重连状态");
    default:
        return "??";
    }
}

void CWebSocket::setServiceUrl(const QUrl &_url)
{
    serviceUrl = _url;
}

void CWebSocket::setIsUseStandardHeartBeat(bool _is_standard)
{
    m_isUseStandardHeartBeat = _is_standard;
}

bool CWebSocket::checkIsPong(const QString &_msg)
{
    if (!m_isUseStandardHeartBeat) {
        if (_msg == m_heartBeatPongStr) {
            static const QByteArray payload = "";

            emit pong(0, payload);

            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

void CWebSocket::setHeartBeatPingStr(const QString &_ping_str)
{
    m_heartBeatPingStr = _ping_str;
}

void CWebSocket::setHeartBeatPongStr(const QString &_pong_str)
{
    m_heartBeatPongStr = _pong_str;
}

void CWebSocket::setHeartBeatInterval(int _secs)
{
    heartBeatInterval = _secs;
}

void CWebSocket::setPongTimeout(int _secs)
{
    pongTimeout = _secs;
}

void CWebSocket::setReconnectInterval(int _secs)
{
    reconnectInterval = _secs;
}

void CWebSocket::setWorkThread(QThread *_thread)
{
    this->moveToThread(_thread);
    timer.moveToThread(_thread);
}

bool CWebSocket::setIsOpened(bool _is_open, QString *_msg)
{
    Q_UNUSED(_msg)

    emit sigSetIsOpened(_is_open);

    return true;
}

bool CWebSocket::getIsOpened()
{
    return isOpened;
}

void CWebSocket::slot_this_SetIsOpened(bool _is_open)
{
    qWarning() << __PRETTY_FUNCTION__ << ": _is_open = " << (_is_open ? "true" : "false");

    //
    if (_is_open == isOpened) {
        qWarning() << "CWebSocket is already " << (_is_open ? "opened" : "closed");
        return;
    }

    //
    if (_is_open) {
        //
        if (serviceUrl.isValid()) {
            this->open(serviceUrl);
            logDebug("CWebSocket: opened", LOG_TAG);
        } else {
            qWarning() << __PRETTY_FUNCTION__ << ": serviceUrl is not valid!";
            setTimerStat(connStat_reconnect);
        }

    } else {
        //
        setTimerStat(connStat_stopped);

        //
        this->flush();
        this->close();
    }

    //
    isOpened = _is_open;
}

void CWebSocket::setTimerStat(const enConnStat _timer_stat)
{
    if (_timer_stat == timerStat) {
        return;
    }
    qDebug() << "CWebSocket::setTimerStat(" << enumToStrEn_TimerStat(_timer_stat) << ")";

    //
    enConnStat timer_stat_old = timerStat;

    //
    timerStat = _timer_stat;

    //
    if (timer_stat_old != timerStat) {
        emit sigConnStatChanged(timerStat);
    } else {
        return;
    }

    //
    if (connStat_heartBeat == timerStat) {             // 心跳状态
        timer.stop();
        timer.start(heartBeatInterval * 1000);

        // 开启心跳状态后，重置 pong 计时变量
        pongElapsed.start();
    } else if (connStat_reconnect == timerStat) {      // 重连状态
        timer.stop();
        timer.start(reconnectInterval * 1000);
    } else if (connStat_stopped == timerStat) {        // 停止状态
        timer.stop();
    } else {
        qWarning() << "logic error: timeStat value invalid (1)";
        setTimerStat(connStat_heartBeat);
    }
}

void CWebSocket::sendPing()
{
    if (m_isUseStandardHeartBeat) {
        ping();
    } else {
        if (m_heartBeatPingStr.length() > 0) {
            sendTextMessage(m_heartBeatPingStr);
        } else {
            sendTextMessage("ping");
        }
    }
}

void CWebSocket::sendPong()
{
    if (m_isUseStandardHeartBeat) {
        //pong();
    } else {
        if (m_heartBeatPongStr.length() > 0) {
            sendTextMessage(m_heartBeatPongStr);
        } else {
            sendTextMessage("pong");
        }
    }
}

void CWebSocket::slot_timer_timeout()
{
    //
    if (connStat_heartBeat == timerStat) {                 // 心跳状态
        /* 心跳的处理逻辑：
         * 1、定时发送 ping 消息。
         * 2、每次发送心跳 ping 时，检查 pong 应答是否超时。若超时，则判定为连接失效，转换为重连过程。
         * 3、pong 超时的判断方法：
         *   （1）开启心跳状态后，重置 pong 计时变量。
         *   （2）收到 pong 后，重置 pong 计时变量。
         *   （3）若 pong 的计时变量大于超时时间，则为超时。
         */
        // TODO: 这个逻辑完善吗？比如，在传送大数据或大文件的过程中，服务端可能未能及时 pong ？所以，大文件必须分段上传？大消息也必须分段？

        // 每次发送心跳 ping 时，检查 pong 应答是否超时
        if (pongElapsed.elapsed() > pongTimeout * 1000) {
            emit sigReconnect();
            logDebug("CWebSocket: going to restart ...", LOG_TAG);
        }

        // 发送 ping 消息
        sendPing();
        //logDebug("CWebSocket: heartBeat sended", LOG_TAG);

    } else if (connStat_reconnect == timerStat) {          // 重连状态
        if (QAbstractSocket::UnconnectedState == state()) {
            emit sigReconnect();
            logDebug("CWebSocket: going to restart ...", LOG_TAG);
        } else {
            qInfo() << "";

            // TODO: ？

        }
    } else if (connStat_stopped == timerStat) {            // 停止状态
        // 停止定时器
        setTimerStat(connStat_stopped);
        logDebug("CWebSocket: stopped!", LOG_TAG);
    } else {                                                // 非法状态，停止定时器
        qCritical() << "logic error: timeStat value invalid (2)";
        setTimerStat(connStat_stopped);
        logDebug("CWebSocket: stopped!", LOG_TAG);
    }
}

void CWebSocket::slot_this_connected()
{
    // 连接后，将定时器设为心跳状态
    setTimerStat(connStat_heartBeat);
}

void CWebSocket::slot_this_disconnected()
{
    qDebug() << "CWebSocket: disconnected";

    // 断连后，将定时器设为重连状态
    if (connStat_stopped != timerStat) {
        setTimerStat(connStat_reconnect);
    }
}

void CWebSocket::slot_this_stateChanged(QAbstractSocket::SocketState _state)
{
    qDebug() << __PRETTY_FUNCTION__ << ": _state = " << QVariant::fromValue(_state).toString();

}

void CWebSocket::slot_this_error(QAbstractSocket::SocketError _error)
{
    qDebug() << __PRETTY_FUNCTION__ << ": _error = " << QVariant::fromValue(_error).toString();

}

void CWebSocket::slot_this_Reconnect()
{
    logDebug("CWebSocket: going to reconnect", LOG_TAG);

    // 若已连接，先断开
    if (QAbstractSocket::UnconnectedState != this->state()) {
        logDebug("CWebSocket: but current stat is not unconnected, closing", LOG_TAG);
        this->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection);
    }

    // 再连接
    this->open(serviceUrl);
    logDebug("CWebSocket: opened", LOG_TAG);
}

void CWebSocket::slot_this_pong(quint64 _elapsed_time, const QByteArray &_payload)
{
    Q_UNUSED(_elapsed_time)
    Q_UNUSED(_payload)

    //logDebug("CWebSocket: pong received", LOG_TAG);

    // 收到 pong 后，重置 pong 计时变量
    pongElapsed.start();

}

}   // namespace Remote
}   // namespace Net
