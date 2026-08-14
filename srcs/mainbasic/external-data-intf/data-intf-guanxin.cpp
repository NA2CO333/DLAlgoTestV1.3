#include "data-intf-guanxin.h"

#include <iostream>

#include <QNetworkRequest>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkReply>
#include <QDebug>
#include <QDir>

#include "global.h"
#include "logger.h"

//
namespace  {

/**
 * @brief 状态码字符串 转 BOOL 值
 * @param _stat_str     状态码字符串
 * @param[out] _stat    是否成功
 * @return 字符串是否符合规则
 */
bool statStrToBool(const QString &_str, bool &_stat)
{
    static const QString SUCC = "success";
    static const QString FAIL = "failure";

    if (_str.toLower() == SUCC) {
        _stat = true;
        return true;
    } else if (_str.toLower() == FAIL) {
        _stat = false;
        return true;
    } else {
        return false;
    }
};

}   // namespace Entity

//
constexpr char CResultItemNames::TongJu            [];
constexpr char CResultItemNames::Ltongkongdaxiao   [];
constexpr char CResultItemNames::Rtongkongdaxiao   [];
constexpr char CResultItemNames::Laxialview        [];
constexpr char CResultItemNames::Raxialview        [];
constexpr char CResultItemNames::LZhuJing          [];
constexpr char CResultItemNames::RZhuJing          [];
constexpr char CResultItemNames::LYanQiuJing       [];
constexpr char CResultItemNames::RYanQiuJing       [];
constexpr char CResultItemNames::ChuShaiZhenDuan   [];

//
const QString CDataIntfGuanXin::DATE_FORMAT = "yyyy-MM-dd";

QString CDataIntfGuanXin::PATH_AREAS_CFG;
QVector<stAreaInfo> CDataIntfGuanXin::s_areaList;

//
CDataIntfGuanXin::CDataIntfGuanXin(QNetworkAccessManager *_net_manager, QObject *_parent)
    : QObject(_parent)
    , m_netManager(_net_manager)
{
    if (!m_netManager) {
        logWarning(QString("%1: QNetworkAccessManager not been setted! Created internal!").arg(__PRETTY_FUNCTION__));
        m_netManager = new QNetworkAccessManager();
    }

}

CDataIntfGuanXin::~CDataIntfGuanXin()
{

}

bool CDataIntfGuanXin::init()
{
    PATH_AREAS_CFG = CGlobal::pathConfig() + QDir::separator() + "area-list.txt";
    return true;
}

QVector<stAreaInfo> &CDataIntfGuanXin::areaList()
{
    static bool is_load = false;
    if (!is_load) {
        loadConfig();
        is_load = true;
    }
    return s_areaList;
}

void CDataIntfGuanXin::setConfig(const stGuanXinIntfCfg &_cfg)
{
    //
    m_cfg = _cfg;

    //
    if (m_cfg.port <= 0) {
        m_cfg.port = 80;
    }

    if (!m_cfg.pathUpload.isEmpty() && !m_cfg.pathUpload.startsWith('/')) {
        m_cfg.pathUpload = '/' + m_cfg.pathUpload;
    }

    if (!m_cfg.pathQuery.isEmpty() && !m_cfg.pathQuery.startsWith('/')) {
        m_cfg.pathQuery = '/' + m_cfg.pathQuery;
    }

}

bool CDataIntfGuanXin::queryTesteeList(const Entity::ETesteeQueryRequest &_request,
                                       Entity::ETesteeQueryResponse &_response, QString &_err_msg)
{
    // 接口配置检查
    QString err_msg;
    do {
        if (m_cfg.ip.isEmpty()) {
            err_msg = tr("IP 不可为空！");   // "IP cannot be empty!"
            break;
        }
        if (m_cfg.pathQuery.isEmpty()) {
            err_msg = tr("名单获取路径不可为空！");   // "Name list retrieval path cannot be empty!"
            break;
        }

        // TODO:

    } while (false);
    if (!err_msg.isEmpty()) {
        _err_msg = err_msg;
        return false;
    }

    // http 请求
    QNetworkRequest request;        // TODO: 改用 Common::Net::sendHttpRequest()？

    QString url_str = QString("%1://%2:%3%4").arg(m_cfg.isHttps ? "https" : "http").arg(m_cfg.ip).arg(m_cfg.port)
            .arg(m_cfg.pathQuery);
    QUrl url(url_str);
    request.setUrl(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    //
    QByteArray data_request = _request.toRawJson();
    QString str_request = QString::fromUtf8(data_request);
    //qDebug().noquote() << str_request;
    std::cout << str_request.toStdString() << std::endl;

    //
    QNetworkReply *reply = m_netManager->post(request, data_request);
    bool succ_response = false;

    //
    static const int TIMEOUT_RESPONSE = 30 * 1000;              // 超时时长（毫秒），指连续无数据传输的时长

    QElapsedTimer elapsedTimer;                                 // 超时的计时
    elapsedTimer.start();

    QByteArray data_response;
    qint64 bytes_sent;
    qint64 bytes_total;

    bool is_timeout = false;
    QObject::connect(reply, &QNetworkReply::uploadProgress, [&](qint64 _bytes_sent, qint64 _bytes_total) {     // 通过上传进度信号检测超时
        if (_bytes_sent > 0) {                                  // 如果有数据传输，重置计时器
            bytes_sent = _bytes_sent;
            bytes_total = _bytes_total;
            elapsedTimer.restart();
        }
        if (elapsedTimer.elapsed() > TIMEOUT_RESPONSE) {        // 检查是否超时
            data_response = reply->readAll();       // abort() 后无法再读取，所以先读取
            reply->abort();                 // 若超时，中断请求
            is_timeout = true;
        }   // TODO: 这里检测不到超时？因为如果有 uploadProgress 事件，是否有必要超时？若没 uploadProgress 事件，这里不会被执行？改为用定时器定时检查？
    });

    // 通过事件循环等待请求执行结束           // TODO: 这里好像没必要用事件循环来阻塞？可通过信号槽
    QEventLoop event_loop;

    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);      // TODO: 在这个事件里读取即可，没必要用 QEventLoop 阻塞？
    event_loop.exec(QEventLoop::ExcludeUserInputEvents);

    // http 应答 body
    if (QNetworkReply::NoError == reply->error()) {
        data_response = reply->readAll();

        QString str_response = QString::fromUtf8(data_response);
        //qDebug().noquote() << str_response;
        std::cout << str_response.toStdString() << std::endl;

        if (!data_response.isEmpty()) {
            _response.fromJson(data_response);

            bool is_succ = false;
            bool is_valid = statStrToBool(_response.status, is_succ);
            if (is_valid) {
                if (is_succ) {
                    succ_response = true;
                } else {
                    _err_msg = "Server Error: " + _response.message;
                }
            } else {
                _err_msg = tr("服务器应答数据非法: ") + data_response.left(150);     // "The server's response data is illegal: "
            }
        } else {
            _err_msg = tr("应答数据为空！");      // "The response data is empty!"
        }
    } else {
        _err_msg = tr("请求失败：%1！").arg(reply->errorString());      // "Request failed: %1!"
    }

    //
    if (reply) {
        reply->deleteLater();       // NOTE: QNetworkReply 须由调用者释放。
    }

    //
    return succ_response;
}

bool CDataIntfGuanXin::uploadResult(const QString &_area_code, const Entity::EResultRequest &_request,
                                    Entity::EResultResponse &_response, QString &_err_msg)
{
    // 接口配置检查
    QString err_msg;
    do {
        if (m_cfg.ip.isEmpty()) {
            err_msg = tr("IP 不可为空！");   // "IP cannot be empty!"
            break;
        }
        if (m_cfg.pathUpload.isEmpty()) {
            err_msg = tr("结果上传路径不可为空！");   // "The result upload path cannot be empty!"
            break;
        }

        // TODO:

    } while (false);
    if (!err_msg.isEmpty()) {
        _err_msg = err_msg;
        return false;
    }

    // http 请求
    QNetworkRequest request;        // TODO: 改用 Common::Net::sendHttpRequest()？

    QString url_str = QString("%1://%2:%3%4?areacode=%5").arg(m_cfg.isHttps ? "https" : "http").arg(m_cfg.ip).arg(m_cfg.port)
            .arg(m_cfg.pathUpload).arg(_area_code);
    QUrl url(url_str);
    request.setUrl(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    //
    QByteArray data_request = _request.toRawJson();
    QString str_request = QString::fromUtf8(data_request);
    //qDebug().noquote() << str_request;
    std::cout << str_request.toStdString() << std::endl;

    //
    QNetworkReply *reply = m_netManager->post(request, data_request);
    bool succ_response = false;

    //
    static const int TIMEOUT_RESPONSE = 30 * 1000;              // 超时时长（毫秒），指连续无数据传输的时长

    QElapsedTimer elapsedTimer;                                 // 超时的计时
    elapsedTimer.start();

    QByteArray data_response;
    qint64 bytes_sent;
    qint64 bytes_total;

    bool is_timeout = false;
    QObject::connect(reply, &QNetworkReply::uploadProgress, [&](qint64 _bytes_sent, qint64 _bytes_total) {     // 通过上传进度信号检测超时
        if (_bytes_sent > 0) {                                  // 如果有数据传输，重置计时器
            bytes_sent = _bytes_sent;
            bytes_total = _bytes_total;
            elapsedTimer.restart();
        }
        if (elapsedTimer.elapsed() > TIMEOUT_RESPONSE) {        // 检查是否超时
            data_response = reply->readAll();       // abort() 后无法再读取，所以先读取
            reply->abort();                 // 若超时，中断请求
            is_timeout = true;
        }   // TODO: 这里检测不到超时？因为如果有 uploadProgress 事件，是否有必要超时？若没 uploadProgress 事件，这里不会被执行？改为用定时器定时检查？
    });

    // 通过事件循环等待请求执行结束           // TODO: 这里好像没必要用事件循环来阻塞？可通过信号槽
    QEventLoop event_loop;

    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);      // TODO: 在这个事件里读取即可，没必要用 QEventLoop 阻塞？
    event_loop.exec(QEventLoop::ExcludeUserInputEvents);

    // http 应答 body
    if (QNetworkReply::NoError == reply->error()) {
        data_response = reply->readAll();

        QString str_response = QString::fromUtf8(data_response);
        //qDebug().noquote() << str_response;
        std::cout << str_response.toStdString() << std::endl;

        if (!data_response.isEmpty()) {
            _response.fromJson(data_response);

            bool is_succ = false;
            bool is_valid = statStrToBool(_response.status, is_succ);
            if (is_valid) {
                if (is_succ) {
                    succ_response = true;
                } else {
                    _err_msg = "Server Error: " + _response.data;
                }
            } else {
                _err_msg = tr("服务器应答数据非法: ") + data_response.left(150);     // "The server's response data is illegal: "
            }
        } else {
            _err_msg = tr("应答数据为空！");      // "The response data is empty!"
        }
    } else {
        _err_msg = tr("请求失败：%1！").arg(reply->errorString());      // "Request failed: %1!"
    }

    //
    if (reply) {
        reply->deleteLater();       // NOTE: QNetworkReply 须由调用者释放。
    }

    //
    return succ_response;
}

bool CDataIntfGuanXin::loadConfig()
{
    //
    if (!s_areaList.isEmpty()) {
        logWarning(QString("%1: Area List has been loaded!").arg(__PRETTY_FUNCTION__));
        s_areaList.clear();
    }

    //
    if (QFile::exists(PATH_AREAS_CFG)) {
        QStringList lines;
        bool succ_read = Util::readFileToQStrList(PATH_AREAS_CFG, lines);
        if (succ_read) {
            for (int i = 0; i < lines.size(); i++) {
                const QString &line_str = lines.at(i);
                if (line_str.isEmpty()) {
                    continue;
                }
                int idx = line_str.indexOf(',');
                if (idx >= 0) {
                    s_areaList.append(stAreaInfo(line_str.left(idx), line_str.mid(idx + 1)));
                } else {
                    logWarning(QString("%1: Error on loading Area List: failed to find ',' of line %2!").arg(__PRETTY_FUNCTION__).arg(i + 1));
                    return false;
                }
            }
        } else {
            logWarning(QString("%1: Failed to read Area List from file '%2'!").arg(__PRETTY_FUNCTION__).arg(PATH_AREAS_CFG));
            return false;
        }
    } else {
        //
        s_areaList.append(stAreaInfo("7061764a-192e-4859-8448-df1a39615a2a", "伊吾县下马崖乡卫生院"            ));
        s_areaList.append(stAreaInfo("85f4a182-e647-47cc-be12-f1d9d521ff35", "伊吾县前山哈萨克民族乡卫生院"        ));
        s_areaList.append(stAreaInfo("eba673bf-86c1-472b-b69d-0b5eda9a1700", "伊吾县吐葫芦乡卫生院"            ));
        s_areaList.append(stAreaInfo("0d025a07-e72a-486b-974d-c79c32fad475", "伊吾县山南开发区卫生院"           ));
        s_areaList.append(stAreaInfo("455aff10-088a-432a-b784-fa332a1f31a1", "伊吾县淖毛湖镇卫生院"            ));
        s_areaList.append(stAreaInfo("caf29902-9789-4919-ad62-7b8a7c80ba51", "伊吾县盐池乡卫生院"             ));
        s_areaList.append(stAreaInfo("11770752-b747-411d-9a07-a23b2d33c292", "伊吾县苇子峡乡卫生院"            ));

        //
        saveConfig();
    }

    //
    if (!s_areaList.isEmpty()) {
        return true;
    } else {
        logWarning(QString("%1: Failed to load Area List!").arg(__PRETTY_FUNCTION__));
        return false;
    }
}

bool CDataIntfGuanXin::saveConfig()
{
    QStringList lines;
    for (int i = 0; i < s_areaList.size(); i++) {
        const stAreaInfo &area_info = s_areaList.at(i);
        lines.append(area_info.code + ',' + area_info.name);
    }

    bool succ_write = Util::writeQStrListToFile(lines, PATH_AREAS_CFG);
    if (succ_write) {
        return true;
    } else {
        logWarning(QString("%1: Failed to write Area List to file '%2'!").arg(__PRETTY_FUNCTION__).arg(PATH_AREAS_CFG));
        return false;
    }
}
