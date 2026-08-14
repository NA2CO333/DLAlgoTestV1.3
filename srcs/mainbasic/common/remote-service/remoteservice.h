#ifndef CREMOTESERVICE_H
#define CREMOTESERVICE_H

/* 远程服务模块
 *
 * 基本结构及流程设计：
 * 1、由 CRemoteService 负责本模块的总控制，负责连接的配置、具体业务功能模块的调用，等。上层应用基本上只需要访问此类。
 * 2、每个 CWebSocketConn 对象对应一个服务端的接口（URL路径）的连接，封装了心跳和重连等基本的连接控制。
 * 3、建立“指令-处理者”映射集，CWebSocketConn 收到数据后，由总控分发给对应的处理者来处理。
 * 4、不同的业务逻辑模块封装为不同的类。如目录浏览及下载类、被测者档案接收类，等。
 *
 */

#include <QObject>
#include <QThread>
#include <QMap>
#include <QDebug>

#include "remoteservicedefs.h"
#include "commandhandler.h"

namespace Net {
namespace Remote {

// 前置声明
class CWebSocketConn;

///=============================================================================================================
/// class CRemoteService

// 设备远程服务的总控制
class CRemoteService : public QObject
{
    Q_OBJECT
public:
    explicit CRemoteService(QObject *parent = nullptr);
    ~CRemoteService();

    QString getVersion();                           // 获取版本号

    //
    void setSvrHost(const QString &_host);                          // 设置服务主机的 IP 或域名
    void setSvrPort(const int _port);                               // 设置服务主机的 端口
    void setSvrPath(const QString &_path);                          // 设置服务主机的 路径
    void setIsHttps(bool _is_https);                                // 设置是否使用 https

    void setSenderNum(QString _sender_num);                         // 设置“发送者编号”（目前协议的规定是使用设备编号）
    QString getSenderNum();

    CWebSocketConn *getWebSocket();

    bool setIsOpened(bool _is_open, QString *_msg = Q_NULLPTR);     // 设置“是否已打开”状态
    bool getIsOpened();                                             // 获取“是否已打开”状态

    void emitLog(Net::Remote::enLogType _log_type, QString _log_msg);       // 发射“日志消息”信号

    /**
     * @brief 发送错误消息应答
     * @param _request_cmd      请求的命令码（用于构造应答消息的指令码，即 "r_" + 请求码。所以通信协议中，该指令码的请求码和应答码应当是遵循此规则的）
     * @param _err_msg          错误消息
     */
    void sendErrorResponse(const QString &_request_cmd, const QString &_err_msg);

    //
    template<typename T>
    T getCommandHandler(const QString &_type_name)                  // 根据类名获取指令处理者
    {
        T handler_ptr = Q_NULLPTR;
        for (auto it = mapCmdHandler->begin(); it != mapCmdHandler->end(); it++) {
            CCommandHandler *ptr = it.value();
            if (ptr->objectName() == _type_name) {
                handler_ptr = dynamic_cast<T>(ptr);
                if (!handler_ptr) {
                    qDebug() << __PRETTY_FUNCTION__ << ": dynamic_cast failed! _type_name = " << _type_name;
                }
                //
                break;
            }
        }
        return handler_ptr;
    }

signals:
    void sigLog(Net::Remote::enLogType _log_type, QString _log_msg);        // 日志消息

protected slots:
    void slot_webSocket_textFrameReceived(const QString &_frame, bool _is_last_frame);
    void slot_webSocket_binaryFrameReceived(const QByteArray &_frame, bool _is_last_frame);

protected:
    QString svrHost;
    int svrPort = 0;
    QString svrPath;
    bool isHttps = false;

    CWebSocketConn *webSocket = Q_NULLPTR;
    CDirListHandler *dirListHandler = Q_NULLPTR;

    QThread *workThread = Q_NULLPTR;

    QMap<QString, CCommandHandler *> *mapCmdHandler = Q_NULLPTR;                // “指令-处理器”映射表

    QString textBuffer;
    QByteArray binaryBuffer;

    QString senderNum;                                  // “发送者编号”（目前协议的规定是使用设备编号，用于给服务端区分不同的设备）

    void addCommandHandler(CCommandHandler *_handler);                              // 添加指令处理者

};

// 向外公开的变量
extern const QString Version;
extern const QString Protocol;
extern const QString LastEdit;

}   // namespace Remote
}   // namespace Net

#endif // CREMOTESERVICE_H
