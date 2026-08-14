#include "websocketconn.h"

#include <QDebug>
#include <QVariant>

#include <remoteservice.h>

namespace Net {
namespace Remote {

///=============================================================================================================
/// class CWebSocketConn

// 默认心跳间隔（秒）
#define DEFAULT_HEART_BEAT_INTERVAL 10

// 默认的 pong 超时时间（秒）
#define DEFAULT_PONG_TIMEOUT        20

// 默认的重连间隔时间（秒）
#define DEFAULT_RECONNECT_INTERVAL  10

// ping 字符串
#define PING_PAYLOAD    "vision"

//
//Q_ENUM(QAbstractSocket::SocketError)

//
CWebSocketConn::CWebSocketConn(CRemoteService *_service, const QString &origin, QWebSocketProtocol::Version version, QObject *parent) :
    QWebSocket(origin, version, parent),
    service(_service)
{
    //
    heartBeatInterval = DEFAULT_HEART_BEAT_INTERVAL;
    pongTimeout = DEFAULT_PONG_TIMEOUT;
    reconnectInterval = DEFAULT_RECONNECT_INTERVAL;

    //
    timer = new QTimer(this);

    // 信号槽连接
    QObject::connect(this, &CWebSocketConn::connected, this, &CWebSocketConn::slot_this_connected, Qt::QueuedConnection);
    QObject::connect(this, &CWebSocketConn::disconnected, this, &CWebSocketConn::slot_this_disconnected, Qt::QueuedConnection);
    QObject::connect(this, &CWebSocketConn::pong, this, &CWebSocketConn::slot_this_pong, Qt::QueuedConnection);
    QObject::connect(this, &CWebSocketConn::stateChanged, this, &CWebSocketConn::slot_this_stateChanged, Qt::QueuedConnection);
    QObject::connect(this, QOverload<QAbstractSocket::SocketError>::of(&CWebSocketConn::error), this, &CWebSocketConn::slot_this_error, Qt::QueuedConnection);

    QObject::connect(this, &CWebSocketConn::sigSetIsOpened, this, &CWebSocketConn::slot_this_SetIsOpened, Qt::QueuedConnection);
    QObject::connect(this, &CWebSocketConn::sigReconnect, this, &CWebSocketConn::slot_this_Reconnect, Qt::QueuedConnection);

    QObject::connect(timer, &QTimer::timeout, this, &CWebSocketConn::slot_timer_timeout, Qt::QueuedConnection);

    // 初始化 elapsedTimer，避免调用 elapsed() 的返回值不可预料
    pongElapsed.start();

}

void CWebSocketConn::setHeartBeatInterval(int _secs)
{
    heartBeatInterval = _secs;
}

void CWebSocketConn::setPongTimeout(int _secs)
{
    pongTimeout = _secs;
}

void CWebSocketConn::setReconnectInterval(int _secs)
{
    reconnectInterval = _secs;
}

void CWebSocketConn::setServiceUrl(const QUrl &_url)
{
    serviceUrl = _url;
}

void CWebSocketConn::setWorkThread(QThread *_thread)
{
    this->moveToThread(_thread);
}

bool CWebSocketConn::setIsOpened(bool _is_open, QString *_msg)
{
    Q_UNUSED(_msg)

    emit sigSetIsOpened(_is_open);

    return true;
}

bool CWebSocketConn::getIsOpened()
{
    return isOpened;
}

void CWebSocketConn::slot_this_SetIsOpened(bool _is_open)
{
    //
    if (_is_open == isOpened) {
        qWarning() << __FUNCTION__ << ": _is_open = " << (_is_open ? "true" : "false") << " but it's already " << (_is_open ? "opened" : "closed");
        return;
    }

    //
    service->emitLog(logType_info, (_is_open ? "opening websocket ..." : "closing websocket ..."));
    if (_is_open) {
        service->emitLog(logType_info, service->getVersion());
    }

    //
    if (_is_open) {
        //
        if (serviceUrl.isValid()) {
            this->open(serviceUrl);
        } else {
            QString err_msg = "serviceUrl is not valid, open failed!";
            qWarning() << __PRETTY_FUNCTION__ << ": " + err_msg;
            service->emitLog(logType_error, err_msg);
            //setTimerStat(timerStat_reconnect);            /* 若 URL 非法，启动重连流程有意义吗？ */
        }

    } else {
        //
        setTimerStat(timerStat_stopped);

        //
        this->flush();
        this->close(QWebSocketProtocol::CloseCodeNormal, "User closed");
    }

    //
    isOpened = _is_open;
}

void CWebSocketConn::setTimerStat(const enTimerStat _timer_stat)
{
    if (_timer_stat == timerStat) {
        return;
    }
    qDebug() << "setTimerStat( " << _timer_stat << " )";

    //
    timerStat = _timer_stat;

    //
    if (timerStat_heartBeat == timerStat) {             // 心跳状态
        timer->stop();
        timer->start(heartBeatInterval * 1000);

        // 开启心跳状态后，重置 pong 计时变量
        pongElapsed.start();
    } else if (timerStat_reconnect == timerStat) {      // 重连状态
        // 重置重连的状态变量
        reconnectingStat = 0;

        //
        timer->stop();
        timer->start(reconnectInterval * 1000);
    } else if (timerStat_stopped == timerStat) {        // 停止状态
        timer->stop();
    } else {
        qWarning() << "logic error: timeStat value invalid (1)";
        setTimerStat(timerStat_heartBeat);
    }
}

void CWebSocketConn::slot_timer_timeout()
{
    //
    if (timerStat_heartBeat == timerStat) {                 // 心跳状态
        /* 心跳的处理逻辑：
         * 1、定时发送 ping 消息。
         * 2、每次发送心跳 ping 时，检查 pong 应答是否超时。若超时，则判定为连接失效，转换为重连过程。
         * 3、pong 超时的判断方法：
         *   （1）开启心跳状态后，重置 pong 计时变量。
         *   （2）收到 pong 后，重置 pong 计时变量。
         *   （3）若 pong 的计时变量大于超时时间，则判断为超时。
         */
        // TODO: 这个逻辑完善吗？比如，在传送大数据或大文件的过程中，服务端可能未能及时 pong ？所以，大文件必须分段上传？大消息也必须分段？

        // 每次发送心跳 ping 时，检查 pong 应答是否超时，若超时，则启动重连流程
        if (pongElapsed.elapsed() > pongTimeout * 1000) {
            setTimerStat(timerStat_reconnect);
            //qDebug() << "going to reconnect ...";
        }

        // 发送 ping 消息
        this->ping(PING_PAYLOAD);
        //qDebug() << "heartBeat sended";

    } else if (timerStat_reconnect == timerStat) {          // 重连状态
        // 定时发射重连信号
        emit sigReconnect();
    } else if (timerStat_stopped == timerStat) {            // 停止状态
        // 停止定时器
        setTimerStat(timerStat_stopped);
    } else {                                                // 非法状态，停止定时器
        qCritical() << "logic error: timeStat value invalid (2)";
        setTimerStat(timerStat_stopped);
    }
}

void CWebSocketConn::slot_this_connected()
{
    service->emitLog(logType_info, "websocket connected");

    // 连接后，将定时器设为心跳状态
    setTimerStat(timerStat_heartBeat);
}

void CWebSocketConn::slot_this_disconnected()
{
    QString msg = "websocket disconnected! closeCode = " + QVariant::fromValue(this->closeCode()).toString() + ", closeReason = " + this->closeReason();
    service->emitLog(logType_info, msg);

    // 断连后，若服务未关闭，则将定时器设为重连状态
    if (getIsOpened()) {
        setTimerStat(timerStat_reconnect);
    }
}

void CWebSocketConn::slot_this_stateChanged(QAbstractSocket::SocketState _state)
{
    //qDebug() << (QString("%1: _state = %2").arg(__PRETTY_FUNCTION__).arg(_state));
    service->emitLog(logType_info, QString("websocket stateChanged: %1").arg(QVariant::fromValue(_state).toString()));

}

void CWebSocketConn::slot_this_error(QAbstractSocket::SocketError _error)
{
    qDebug() << __PRETTY_FUNCTION__ << ": _error = " << _error;
    service->emitLog(logType_error, QString("websocket error: %1").arg(QVariant::fromValue(_error).toString()));

}

void CWebSocketConn::slot_this_Reconnect()
{
    //
    service->emitLog(logType_info, "webSocket reconnecting func executing ...");

    /* 重连状态下，重连函数会定时被执行。 */

    // 若是刚开始启动重连，则先断开
    if (0 == reconnectingStat) {
        service->emitLog(logType_info, "reconnection is starting, closing websocket ...");
        this->close(QWebSocketProtocol::CloseCodeAbnormalDisconnection, "Heart beat timeout, trying to reconnect");

        //
        // TODO: 等待，直到断开？


        //
        reconnectingStat = 1;
    }

    // 若当前连接状态为未连接，则调用连接
    QAbstractSocket::SocketState stat = this->state();
    if (QAbstractSocket::UnconnectedState == stat) {
        service->emitLog(logType_info, "websocket opening ...");
        this->open(serviceUrl);
    }

}

void CWebSocketConn::slot_this_pong(quint64 _elapsed_time, const QByteArray &_payload)
{
    Q_UNUSED(_elapsed_time)
    Q_UNUSED(_payload)

    //qDebug() << "pong received";

    // 收到 pong 后，重置 pong 计时变量
    pongElapsed.start();

}

}   // namespace Remote
}   // namespace Net
