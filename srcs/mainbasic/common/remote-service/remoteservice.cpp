#include "remoteservice.h"

#include <QWebSocket>
#include <QJsonDocument>
#include <QDebug>

#include "websocketconn.h"

namespace Net {
namespace Remote {

// 协议版本号
const QString Protocol   = "1.0";

// 模块代码版本号
const QString Version    = "1.1";

// 模块最后修改日期
const QString LastEdit   = "2024-01-16";

///=============================================================================================================
/// class CRemoteService

//
CRemoteService::CRemoteService(QObject *parent) : QObject(parent)
{
    // WebSocket 连接对象
    webSocket = new CWebSocketConn(this, QString(), QWebSocketProtocol::VersionLatest, this);

    mapCmdHandler = new QMap<QString, CCommandHandler *>;

    // “文件浏览及下载”处理者对象
    dirListHandler = new CDirListHandler(this, this);
    addCommandHandler(dirListHandler);

    // 独立的工作线程
    workThread = new QThread;

    this->moveToThread(workThread);     // 线程转移（包括其子对象）

    workThread->start();

    //
    qRegisterMetaType<Net::Remote::enLogType>("Net::Remote::enLogType");

    // 信号槽连接
    QObject::connect(webSocket, &CWebSocketConn::textFrameReceived, this, &CRemoteService::slot_webSocket_textFrameReceived, Qt::QueuedConnection);
    QObject::connect(webSocket, &CWebSocketConn::binaryFrameReceived, this, &CRemoteService::slot_webSocket_binaryFrameReceived, Qt::QueuedConnection);

}

CRemoteService::~CRemoteService()
{
    // TODO: 有没可能正在处理接收到的数据？

    //
    webSocket->setIsOpened(false);

    // TODO:


}

QString CRemoteService::getVersion()
{
    return QString("Protocal:%1, Version:%2, LastEdit:%3").arg(Protocol).arg(Version).arg(LastEdit);
}

void CRemoteService::setSvrHost(const QString &_host)
{
    svrHost = _host;
}

void CRemoteService::setSvrPort(const int _port)
{
    svrPort = _port;
}

void CRemoteService::setSvrPath(const QString &_path)
{
    svrPath = _path;
}

void CRemoteService::setIsHttps(bool _is_https)
{
    isHttps = _is_https;
}

void CRemoteService::setSenderNum(QString _sender_num)
{
    senderNum = _sender_num;
}

QString CRemoteService::getSenderNum()
{
    return senderNum;
}

CWebSocketConn *CRemoteService::getWebSocket()
{
    return webSocket;
}

bool CRemoteService::setIsOpened(bool _is_open, QString *_msg)
{
    //
    QString url_str = QString("%1://%2:%3%4/%5").arg(isHttps ? "wss" : "ws").arg(svrHost).arg(svrPort).arg(svrPath).arg(senderNum);
    QUrl url(url_str);
    webSocket->setServiceUrl(url);

    bool is_succ = webSocket->setIsOpened(_is_open, _msg);

    // 输出连接参数log
    if (_is_open) {
        emit sigLog(logType_info, QString("WebSocket 接口 URL = \"%1\"\nFileUpload 接口 URL = \"%2\"").arg(url_str).arg(dirListHandler->getUploadSvcUrl().toString()));
    }

    //
    return is_succ;
}

bool CRemoteService::getIsOpened()
{
    return webSocket->getIsOpened();
}

void CRemoteService::emitLog(enLogType _log_type, QString _log_msg)
{
    emit sigLog(_log_type, _log_msg);
}

void CRemoteService::addCommandHandler(CCommandHandler *_handler)
{
    QStringList cmd_list;
    _handler->getSupportedCmds(cmd_list);
    for (int i = 0; i < cmd_list.count(); i++) {
        mapCmdHandler->insert(cmd_list.at(i), _handler);
    }
}

void CRemoteService::slot_webSocket_textFrameReceived(const QString &_frame, bool _is_last_frame)
{
    // 添加到文本帧缓冲区
    textBuffer += _frame;

    // 若非最后一帧，则退出
    if (!_is_last_frame) {
        return;
    }

    //
    emit sigLog(logType_info, "received text message: " + textBuffer);

    // JSON string 到 数据对象
    CCommunicMessage communic_msg;
    bool parse_msg_succ = communic_msg.fromJson(textBuffer);

    //
    if (parse_msg_succ) {
        QString cmd = communic_msg.command;
        if (mapCmdHandler->contains(cmd)) {
            emit sigLog(logType_info, QString("got command \"%1\"").arg(cmd));

            CCommandHandler *handler = mapCmdHandler->value(cmd);
            if (handler) {
                QString err_msg;
                bool is_succ = handler->processCmd(cmd, communic_msg.data, err_msg);
                if (!is_succ) {
                    emit sigLog(logType_error, QString("handler of command \"%1\" executing failed!").arg(cmd));

                    sendErrorResponse(cmd, err_msg);
                }
            } else {
                emit sigLog(logType_error, QString("Internal error: handler of command \"%1\" not found!").arg(cmd));

                QString err_msg = QString("程序异常：获取指令处理者失败！");
                sendErrorResponse(cmd, err_msg);

                qWarning() << err_msg;
            }
        } else {
            emit sigLog(logType_error, QString("command \"%1\" not supported!").arg(cmd));

            QString err_msg = QString("未支持指令 '%1'").arg(cmd);
            sendErrorResponse(cmd, err_msg);

            qWarning() << err_msg;
        }
    } else {
        emit sigLog(logType_error, "JSON parsing failed!");

        QString err_msg = "JSON 解析失败";
        sendErrorResponse("", err_msg);

        qWarning() << err_msg;
    }

    // 处理完后，清空 buffer
    textBuffer.clear();
}

void CRemoteService::slot_webSocket_binaryFrameReceived(const QByteArray &_frame, bool _is_last_frame)
{
    // 添加到二进制帧缓冲区
    binaryBuffer += _frame;

    // 若非最后一帧，则退出
    if (!_is_last_frame) {
        return;
    }

    //
    // TODO:



    // 处理完后，清空 buffer
    binaryBuffer.clear();
}

void CRemoteService::sendErrorResponse(const QString &_request_cmd, const QString &_err_msg)
{
    emit sigLog(logType_error, _err_msg);

    QString cmd = (_request_cmd.length() > 0 ? (QString("r_") + _request_cmd) : QString("r_unknown"));

    CCommunicMessage communic_msg;
    communic_msg.command = cmd;
    communic_msg.stat = STAT_FAIL;
    communic_msg.msg = _err_msg;
    communic_msg.version = Protocol;
    communic_msg.sender = senderNum;
    QString json_str = communic_msg.toJson();

    webSocket->sendTextMessage(json_str);
}

}   // namespace Remote
}   // namespace Net
