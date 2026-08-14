#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariant>

#include "remoteservicedefs.h"

//
Q_DECLARE_METATYPE(QWebSocketProtocol::CloseCode)

//
namespace Net {
namespace Remote {

// 前置声明
class CRemoteService;

///=============================================================================================================
/// class CWebSocketConn

// WebSocket 连接控制逻辑的封装
class CWebSocketConn : public QWebSocket
{
    Q_OBJECT
public:
    explicit CWebSocketConn(CRemoteService *_service,
                            const QString &origin = QString(),
                            QWebSocketProtocol::Version version = QWebSocketProtocol::VersionLatest,
                            QObject *parent = nullptr);

    void setHeartBeatInterval(int _secs);                   // 设置心跳包的发送时间间隔（秒）
    void setPongTimeout(int _secs);                         // 设置 pong 超时时间（秒）
    void setReconnectInterval(int _secs);                   // 设置掉线重连的尝试时间间隔（秒）

    void setServiceUrl(const QUrl &_url);                           // 设置服务接口 URL

    void setWorkThread(QThread *_thread);                   // 设置工作线程

    bool setIsOpened(bool _is_open, QString *_msg = Q_NULLPTR);     // 设置“是否已打开”状态
    bool getIsOpened();                                             // 获取“是否已打开”状态

signals:
    void sigSetIsOpened(bool _is_open);
    void sigReconnect();                        // “重连”信号（重连状态时，定时发射）

protected slots:
    void slot_this_pong(quint64 _elapsed_time, const QByteArray &_payload);
    void slot_this_connected();
    void slot_this_disconnected();
    void slot_this_stateChanged(QAbstractSocket::SocketState _state);
    void slot_this_error(QAbstractSocket::SocketError _error);
    void slot_this_SetIsOpened(bool _is_open);
    void slot_this_Reconnect();                                                 // “重连”槽函数（重连状态时，定时被执行）
    void slot_timer_timeout();

protected:
    enum enTimerStat {
        timerStat_stopped   = -1,       // 停止状态
        timerStat_heartBeat,            // 心跳状态
        timerStat_reconnect,            // 重连状态
    };

    CRemoteService *service = Q_NULLPTR;

    QTimer *timer = Q_NULLPTR;                      // 定时器      /* 规范：只能在 setTimerStat() 内访问 */
    enTimerStat timerStat = timerStat_stopped;      // 定时器的工作状态

    int heartBeatInterval = 0;                      // 心跳间隔（秒）
    int pongTimeout = 0;                            // pong 超时（秒）
    int reconnectInterval = 0;                      // 重连间隔（秒）

    QUrl serviceUrl;
    bool isOpened = false;

    QElapsedTimer pongElapsed;                              // pong 计时变量（用于检查服务端的 pong 是否超时）
    int reconnectingStat = 0;                               // 重连过程的状态值（若为 0，则表示刚开始启动重连流程）

    void setTimerStat(const enTimerStat _timer_stat);       // 设置定时器的工作模式

};

}   // namespace Remote
}   // namespace Net

#endif // WEBSOCKET_H
