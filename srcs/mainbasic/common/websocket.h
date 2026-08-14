#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QElapsedTimer>

namespace Net {
namespace Remote {

///=============================================================================================================
/// class CWebSocket

// WebSocket 连接控制逻辑的封装
class CWebSocket : public QWebSocket
{
    Q_OBJECT
public:
    explicit CWebSocket(const QString &origin = QString(),
                            QWebSocketProtocol::Version version = QWebSocketProtocol::VersionLatest,
                            QObject *parent = nullptr);

    // 连接状态
    enum enConnStat {
        connStat_stopped   = -1,        // 停止状态
        connStat_heartBeat,             // 心跳状态
        connStat_reconnect,             // 重连状态
    };
    static QString enumToStrEn_TimerStat(enConnStat _stat);
    static QString enumToStr_TimerStat(enConnStat _stat);

    //
    void setServiceUrl(const QUrl &_url);                   // 设置服务接口 URL
    void setIsUseStandardHeartBeat(bool _is_standard);      // 是否使用标准心跳         /* 注意：若使用自定义心跳，调用者须在 textMessageReceived() 信号的槽函数里调用 checkIsPong() 检测是否 pong 消息 */
    bool checkIsPong(const QString &_msg);                  // 检测是否自定义 pong 消息
    void setHeartBeatPingStr(const QString &_ping_str);     // 设置心跳 ping 字符串
    void setHeartBeatPongStr(const QString &_pong_str);     // 设置心跳 pong 字符串

    void setHeartBeatInterval(int _secs);                   // 设置心跳包的发送时间间隔（秒）
    void setPongTimeout(int _secs);                         // 设置 pong 超时时间（秒）
    void setReconnectInterval(int _secs);                   // 设置掉线重连的尝试时间间隔（秒）

    void setWorkThread(QThread *_thread);                   // 设置工作线程

    bool setIsOpened(bool _is_open, QString *_msg = Q_NULLPTR);     // 设置“是否已打开”状态
    bool getIsOpened();                                             // 获取“是否已打开”状态

Q_SIGNALS:
    void sigSetIsOpened(bool _is_open);
    void sigReconnect();
    void sigConnStatChanged(Net::Remote::CWebSocket::enConnStat _curr_stat);

protected slots:
    void slot_this_pong(quint64 _elapsed_time, const QByteArray &_payload);
    void slot_this_connected();
    void slot_this_disconnected();
    void slot_this_stateChanged(QAbstractSocket::SocketState _state);
    void slot_this_error(QAbstractSocket::SocketError _error);
    void slot_this_SetIsOpened(bool _is_open);
    void slot_this_Reconnect();
    void slot_timer_timeout();

protected:
    QTimer timer;                                   // 定时器      /* 规范：只能在 setTimerStat() 内访问 */
    enConnStat timerStat = connStat_stopped;        // 定时器的工作状态

    int heartBeatInterval = 0;
    int pongTimeout = 0;
    int reconnectInterval = 0;

    QUrl serviceUrl;
    bool m_isUseStandardHeartBeat = true;
    QString m_heartBeatPingStr;
    QString m_heartBeatPongStr;
    bool isOpened = false;

    QElapsedTimer pongElapsed;                              // pong 计时变量（用于检查服务端的 pong 是否超时）

    void setTimerStat(const enConnStat _timer_stat);        // 设置定时器的工作模式

    void sendPing();
    void sendPong();

};

}   // namespace Remote
}   // namespace Net

#endif // WEBSOCKET_H
