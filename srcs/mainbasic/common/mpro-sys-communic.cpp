#include "mpro-sys-communic.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include "logger.h"
#include "global.h"
#include "nettools.h"

//
namespace Net {
namespace Remote {

//=============================================================================================================
// struct stOutpatientArchive
//=============================================================================================================

void stOutpatientArchive::reset()
{
    birthdate           = "";
    gender              = "";
    phone               = "";
    name                = "";
    treatmentNumber     = "";
    type                = "";
    system              = "";
}

bool stOutpatientArchive::fromJson(QString _json_str)
{
    QJsonParseError json_err;
    QJsonDocument json_doc = QJsonDocument::fromJson(_json_str.toUtf8(), &json_err);
    if (QJsonParseError::NoError == json_err.error) {
        if (json_doc.isNull()) {
            qDebug() << __PRETTY_FUNCTION__ << ": json doc is null";
            return false;
        }
        if (!json_doc.isObject()) {
             qDebug() << __PRETTY_FUNCTION__ << ": doc is not object";
             return false;
        }

        QJsonObject json_obj = json_doc.object();

        {
            QJsonValue json_value = json_obj.value("birthdate");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                birthdate = json_value.toString();
            }
        }
        {
            QJsonValue json_value = json_obj.value("gender");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                gender = json_value.toString();
            }
        }
        {
            QJsonValue json_value = json_obj.value("phone");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                phone = json_value.toString();
            }
        }
        {
            QJsonValue json_value = json_obj.value("name");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                name = QByteArray::fromPercentEncoding(json_value.toString().toLatin1());
            }
        }
        {
            QJsonValue json_value = json_obj.value("treatmentNumber");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                treatmentNumber = json_value.toString();
            }
        }
        {
            QJsonValue json_value = json_obj.value("type");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                type = json_value.toString();
            }
        }
        {
            QJsonValue json_value = json_obj.value("system");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                system = json_value.toString();
            }
        }

        //
        return true;
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "json parse err: err = " << json_err.error << ", " << json_err.errorString();
        return false;
    }
}

//=============================================================================================================
// struct stDevActivateStat
//=============================================================================================================

void stDevActivateStat::reset()
{
    activaionStatus = -1;
}

bool stDevActivateStat::fromJson(QString _json_str)
{
    QJsonParseError json_err;
    QJsonDocument json_doc = QJsonDocument::fromJson(_json_str.toUtf8(), &json_err);
    if (QJsonParseError::NoError == json_err.error) {
        if (json_doc.isNull()) {
            qDebug() << __PRETTY_FUNCTION__ << ": json doc is null";
            return false;
        }
        if (!json_doc.isObject()) {
             qDebug() << __PRETTY_FUNCTION__ << ": doc is not object";
             return false;
        }

        QJsonObject json_obj = json_doc.object();

        {
            QJsonValue json_value = json_obj.value("activaionStatus");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                if (json_value.isString()) {
                    activaionStatus = json_value.toString().toInt();    // NOTE: 2025-09-09 实测服务端返回的此字段是 string 类型，这里若由 QVariant 直接 toInt()，"1" 会转为 0 ！
                } else if (json_value.isDouble()) {
                    activaionStatus = json_value.toInt();
                } else {
                    qDebug() << __PRETTY_FUNCTION__ << ": type of field activaionStatus is not valid!";
                    return false;
                }
            }
        }

        //
        return true;
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "json parse err: err = " << json_err.error << ", " << json_err.errorString();
        return false;
    }
}

//=============================================================================================================
// class CMProSysPushSvcCommunic
//=============================================================================================================

static const QString PING_STR = "{\"type\":\"vision\"}";
static const QString PONG_STR = "{\"type\":\"vision\"}";

//
const QString CMProSysPushSvcCommunic::S_SERVICE_PATH {"/websocket/refraction/%1"};         // 服务路径

CMProSysPushSvcCommunic::CMProSysPushSvcCommunic(QObject *_parent) : QObject(_parent)
{
    m_webSocket = new CWebSocket;

    m_webSocket->setIsUseStandardHeartBeat(false);
    m_webSocket->setHeartBeatPingStr(PING_STR);
    m_webSocket->setHeartBeatPongStr(PONG_STR);

    m_webSocket->setHeartBeatInterval(6);
    m_webSocket->setPongTimeout(15);
    m_webSocket->setReconnectInterval(5);

    //
    QObject::connect(m_webSocket, &CWebSocket::textMessageReceived, this, &CMProSysPushSvcCommunic::slot_webSocket_textMessageReceived, Qt::QueuedConnection);

    // 类型注册
    static bool is_type_reg = false;
    if (!is_type_reg) {
        qRegisterMetaType<Net::Remote::stOutpatientArchive>("Net::Remote::stOutpatientArchive");
        qRegisterMetaType<stOutpatientArchive>("stOutpatientArchive");

        qRegisterMetaType<Net::Remote::stDevActivateStat>("Net::Remote::stDevActivateStat");
        qRegisterMetaType<stDevActivateStat>("stDevActivateStat");

        //
        is_type_reg = true;
    }

}

CMProSysPushSvcCommunic::~CMProSysPushSvcCommunic()
{
    m_webSocket->close(QWebSocketProtocol::CloseCodeNormal);

    m_webSocket->deleteLater();
    //webSocket = Q_NULLPTR;

}

void CMProSysPushSvcCommunic::setServiceAddr(const QUrl &_url)
{
    if (_url.isValid()) {
        m_serviceAddr = _url.toString();
    } else {
        logCritical(QString("%1: URL \"%2\" is not valid!").arg(__PRETTY_FUNCTION__).arg(_url.toString()));
    }
}

void CMProSysPushSvcCommunic::setWorkThread(QThread *_thread)
{
    this->moveToThread(_thread);
    m_webSocket->setWorkThread(_thread);
}

CWebSocket *CMProSysPushSvcCommunic::webSocket()
{
    return m_webSocket;
}

bool CMProSysPushSvcCommunic::setIsOpened(bool _is_open, QString *_msg)
{
    Q_UNUSED(_msg)

    //
    if (_is_open) {
        QString url_str = QString("%1%2").arg(m_serviceAddr).arg(S_SERVICE_PATH);
        url_str = url_str.arg(m_requestParam_DevCode);
        m_webSocket->setServiceUrl(url_str);
    }

    //
    m_webSocket->setIsOpened(_is_open);

    //
    return true;
}

bool CMProSysPushSvcCommunic::getIsOpened()
{
    return m_webSocket->getIsOpened();
}

void CMProSysPushSvcCommunic::slot_webSocket_textMessageReceived(const QString &_message)
{
    qDebug() << "textMessageReceived";

    //
    bool is_pong = m_webSocket->checkIsPong(_message);
    if (!is_pong) {
        /* 诊疗号推送服务推送变更（2025-09-08）：JSON 包的格式增加一种设备激活状态推送： {"activaionStatus":"1"} 1 激活 0 未激活
         * 所以，在解析 JSON 之前，还要判断是哪一种 JSON 格式。
         */

        logDebug(QString("%1: websocket message received:\n").arg(__PRETTY_FUNCTION__).arg(_message));

        // 检查 JSON 包类型
        bool is_outpatient_pkg = !(_message.contains("\"activaionStatus\":"));      // 是否门诊档案推送包

        // 根据 JSON 包类型分别处理
        if (is_outpatient_pkg) {
            // 处理门诊档案推送 JSON 包
            stOutpatientArchive archive;
            bool succ_json = archive.fromJson(_message);
            if (succ_json) {
                emit sigReceivedOutpatientArchive(archive);
            } else {
                logCritical("CMProSysPushSvcCommunic parse JSON failed! json_str = " + _message);
            }
        } else {
            // 处理设备激活状态推送 JSON 包
            stDevActivateStat activate_stat;
            bool succ_json = activate_stat.fromJson(_message);
            if (succ_json) {
                if (activate_stat.activaionStatus == 1 || activate_stat.activaionStatus == 0) {
                    emit sigDevActivateStatReceived(activate_stat);
                } else {
                    logCritical(QString("%1: activaionStatus value(%2) is not valid!").arg(__PRETTY_FUNCTION__).arg(activate_stat.activaionStatus));
                }
            } else {
                logCritical("CMProSysPushSvcCommunic parse JSON failed! json_str = " + _message);
            }
        }
    }
}

//=============================================================================================================
// class CMProSysCommunic
//=============================================================================================================

//
const QString CMProSysCommunic::S_SERVICE_PATH {"/api-v1/equipment/getDeviceInfoWxacodeunlimit?deviceName=%1"};     // 服务路径

const char CMProSysCommunic::S_WX_SVC_QR_CODE_IMG_FILE_NAME[] = "wx-svc-qr-code.png";       // 微信服务二维码图片文件名
const char CMProSysCommunic::S_WX_SVC_QR_CODE_PARAM_FILE_NAME[] = "wx-svc-qr-code.txt";     // 微信服务二维码参数文件名

QString CMProSysCommunic::s_configDirPath;                      // 配置目录路径

QNetworkAccessManager *CMProSysCommunic::s_netManager {nullptr};

CMProSysCommunic::CMProSysCommunic(QObject *_parent) : QObject(_parent)
{
    if (!s_netManager) {
        logWarning(QString("%1: QNetworkAccessManager not been setted! Created internal!").arg(__PRETTY_FUNCTION__));
        s_netManager = new QNetworkAccessManager();
    }

}

CMProSysCommunic::~CMProSysCommunic()
{

}

void CMProSysCommunic::setNetworkAccessManager(QNetworkAccessManager *_net_manager)
{
    s_netManager = _net_manager;
}

void CMProSysCommunic::setConfigDirPath(const QString &_dir_path)
{
    s_configDirPath = _dir_path;
}

void CMProSysCommunic::setServiceAddr(const QUrl &_url)
{
    if (_url.isValid()) {
        m_serviceAddr = _url.toString();
    } else {
        logCritical(QString("%1: URL \"%2\" is not valid!").arg(__PRETTY_FUNCTION__).arg(_url.toString()));
    }
}

void CMProSysCommunic::setDevCode(const QString &_dev_code)
{
    // 若产品编码的当前值不为空，且与正在设置的值不一致
    if (!m_devCode.isEmpty() && _dev_code != m_devCode) {
        // 使二维码失效
        invalidateWxServiceQrCodeImage();
    }

    //
    m_devCode = _dev_code;
}

bool CMProSysCommunic::requestWxServiceQrCodeImage(QString &_err_msg)
{
    static const QString SVC_ADDR_PRODUCE   = "https://opt.manylinksmed.com";   // 正式环境
    static const QString SVC_ADDR_TEST      = "http://120.25.254.38:8080";      // 测试环境

    //
    QString url_str = (!m_serviceAddr.isEmpty() ? m_serviceAddr : (CGlobal::isDebugMode ? SVC_ADDR_TEST : SVC_ADDR_PRODUCE)) + S_SERVICE_PATH;
    url_str = url_str.arg(m_devCode);
    QUrl url(url_str);

    //
    QByteArray reply_data;
    QString reply_content_type;
    int http_stat;
    bool succ_request = Common::Net::sendHttpRequest(url, false, nullptr, nullptr, &reply_data, &reply_content_type, &http_stat, &_err_msg, s_netManager);
    if (succ_request) {
        //
        if (200 == http_stat) {
            // 检查应答数据格式是图片还是 JSON   // NOTE: 2025-09-09 实测云端返回 JSON 错误信息时，http 头的 content-type 还是 "image/png"，所以检查 http 头的 content-type 无意义
            bool is_json = (reply_data.startsWith('{'));
            if (!is_json) {
                // 图片数据的处理
                //if ("image/png" == reply_content_type.toLower())      // NOTE: 目前(2025-09-03)服务器应答的 http 头的 content-type 为 "image/png"，但实际是 jpg 格式的，两者不一致
                {
                    // 图像数据载入
                    m_wxServiceQrCodeImage.loadFromData(reply_data);
                    if (m_wxServiceQrCodeImage.isNull()) {
                        _err_msg = QString("Failed to convert reply data to image!\nreply data: %1 ...(%2bytes)").arg(reply_data.left(50).constData()).arg(reply_data.size());
                        return false;
                    }

                    // 图像保存
                    const QString &path_img = wxSvcQrCodeImgFilePath();
                    if (QFile::exists(path_img)) {
                        bool succ_rm = QFile::remove(path_img);
                        if (!succ_rm) {
                            _err_msg = QString("Failed to load MPro WeChat service QrCode image from file \"%1\"!").arg(path_img);
                            return false;
                        }
                    }
                    bool succ_save = m_wxServiceQrCodeImage.save(path_img);
                    if (!succ_save) {
                        _err_msg = QString("Failed to save MPro WeChat service QrCode image to file \"%1\"!").arg(path_img);
                        return false;
                    }

                    // 参数保存
                    const QString path_param = QString("%1/%2").arg(s_configDirPath).arg(S_WX_SVC_QR_CODE_PARAM_FILE_NAME);;
                    QFile file(path_param);
                    bool succ_open = file.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Truncate);
                    if (!succ_open) {
                        logCritical(QString("1%: Failed to open MPro WeChat service QrCode params file \"%2\"!").arg(__PRETTY_FUNCTION__).arg(path_param));
                        return false;
                    }
                    file.write(m_devCode.toLatin1().constData());
                    file.flush();
                    file.close();
                }
                //else
                //{
                //    _err_msg = QString("http reply content type(\"%1\") is not valid!").arg(reply_content_type);
                //    return false;
                //}
            } else {
                // JSON 数据的处理
                QString msg;
                bool succ_parse = getQrCodeReplyJsonField_Msg(reply_data, msg);
                if (succ_parse) {
                    _err_msg = msg;
                    return false;
                } else {
                    _err_msg = "Failed to parse JSON!";
                    return false;
                }
            }
        } else {
            _err_msg = QString("server returned http error code: %1").arg(http_stat);
            if (!reply_data.isEmpty()) {
                _err_msg += QString("\nreply data: %1 ...(%2bytes)").arg(reply_data.left(50).constData()).arg(reply_data.size());
            }
            return false;
        }

        //
        return true;
    } else {
        _err_msg = QString("http request failed:\n") + _err_msg;
        return false;
    }
}

bool CMProSysCommunic::loadWxServiceQrCodeImageFromFile()
{
    // 先重置数据
    m_devCodeOfQrCode.clear();
    if (!m_wxServiceQrCodeImage.isNull()) {
        m_wxServiceQrCodeImage = QPixmap();
    }

    // 二维码参数
    const QString path_param = QString("%1/%2").arg(s_configDirPath).arg(S_WX_SVC_QR_CODE_PARAM_FILE_NAME);;
    if (QFile::exists(path_param)) {
        QFile file(path_param);
        bool succ_open = file.open(QFile::OpenModeFlag::ReadOnly);
        if (!succ_open) {
            logCritical(QString("1%: Failed to open MPro WeChat service QrCode params file \"%2\"!").arg(__PRETTY_FUNCTION__).arg(path_param));
            return false;
        }
        m_devCodeOfQrCode = file.readAll();
        file.close();
    }

    // 若载入的
    if (m_devCodeOfQrCode.isEmpty()) {
        logCritical(QString("%1: MPro WeChat service QrCode params loaded from file \"%2\" is empty!").arg(__PRETTY_FUNCTION__).arg(path_param));
        return false;
    }

    // 载入二维码图像
    const QString &path_img = wxSvcQrCodeImgFilePath();
    if (QFile::exists(path_img)) {
        bool succ_load = m_wxServiceQrCodeImage.load(path_img);
        if (!succ_load) {
            logCritical(QString("%1: Failed to load MPro WeChat service QrCode image from file \"%2\"!").arg(__PRETTY_FUNCTION__).arg(path_img));
            return false;
        }
    }

    //
    return true;
}

const QPixmap &CMProSysCommunic::wxServiceQrCodeImage()
{
    // 若图像为空，则载入
    if (m_wxServiceQrCodeImage.isNull()) {
        // 载入二维码
        bool succ_load = loadWxServiceQrCodeImageFromFile();
        if (!succ_load) {
            logCritical(QString("%1: Failed to load WeChat Service QrCode image from file!").arg(__PRETTY_FUNCTION__));
        }
    }

    // 二维码参数校验
    if (!m_devCodeOfQrCode.isEmpty()) {
        // 若校验不通过，则使图像失效
        if (m_devCodeOfQrCode != m_devCode) {
            invalidateWxServiceQrCodeImage();
        }

        // 校验后，不管是否通过，清空刚载入的二维码参数
        m_devCodeOfQrCode.clear();
    }

    //
    return m_wxServiceQrCodeImage;
}

bool CMProSysCommunic::invalidateWxServiceQrCodeImage()
{
    bool ret = true;

    // 重置图像对象
    if (!m_wxServiceQrCodeImage.isNull()) {
        m_wxServiceQrCodeImage = QPixmap();
    }

    // 重置“二维码图像的产品编号参数”
    m_devCodeOfQrCode.clear();

    // 删除图像文件
    const QString &path_img = wxSvcQrCodeImgFilePath();
    if (QFile::exists(path_img)) {
        bool succ_rm = QFile::remove(path_img);
        if (!succ_rm) {
            logCritical(QString("%1: Failed to remove MPro WeChat service QrCode image file \"%2\"!").arg(__PRETTY_FUNCTION__).arg(path_img));
            ret = false;
        }
    }

    // 删除参数文件
    const QString path_param = QString("%1/%2").arg(s_configDirPath).arg(S_WX_SVC_QR_CODE_PARAM_FILE_NAME);;
    if (QFile::exists(path_param)) {
        bool succ_rm = QFile::remove(path_param);
        if (!succ_rm) {
            logCritical(QString("%1: Failed to remove MPro WeChat service QrCode params file \"%2\"!").arg(__PRETTY_FUNCTION__).arg(path_param));
            ret = false;
        }
    }

    //
    return ret;
}

const QString &CMProSysCommunic::wxSvcQrCodeImgFilePath()
{
    static QString path = QString("%1/%2").arg(s_configDirPath).arg(S_WX_SVC_QR_CODE_IMG_FILE_NAME);
    return path;
}

bool CMProSysCommunic::getQrCodeReplyJsonField_Msg(const QByteArray &_reply_data, QString &_msg)
{
    /* 数据样例：{"back":false,"code":400,"msg":"设备不存在","refresh":false,"status":"fail"}
     */

    //
    _msg.clear();

    //
    QJsonParseError json_err;
    QJsonDocument json_doc = QJsonDocument::fromJson(_reply_data, &json_err);
    if (QJsonParseError::NoError == json_err.error) {
        if (json_doc.isNull()) {
            qDebug() << __PRETTY_FUNCTION__ << ": json doc is null";
            return false;
        }
        if (!json_doc.isObject()) {
             qDebug() << __PRETTY_FUNCTION__ << ": doc is not object";
             return false;
        }

        QJsonObject json_obj = json_doc.object();

        {
            QJsonValue json_value = json_obj.value("msg");
            if (!json_value.isUndefined() && !json_value.isNull()) {
                _msg = json_value.toString();
            } else {
                qDebug() << __PRETTY_FUNCTION__ << ": Field \"msg\" not found!";
                return false;
            }
        }

        //
        return true;
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "json parse err: err = " << json_err.error << ", " << json_err.errorString();
        return false;
    }
}

}   // namespace Remote
}   // namespace Net
