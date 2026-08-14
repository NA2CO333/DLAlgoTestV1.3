#include "data-intf-huayi.h"

#include "logger.h"
#include "nettools.h"
#include "DataTransmit.h"
#include "windowsmanager.h"
#include "windatatrans.h"

using DataTrans::DataTransmiter;

//
CDataIntfHuaYi *CDataIntfHuaYi::s_instance {nullptr};
const char * const CDataIntfHuaYi::S_CLASS_NAME = CDataIntfHuaYi::staticMetaObject.className();

CDataIntfHuaYi *CDataIntfHuaYi::instance()
{
    if (!s_instance) {
        s_instance = new CDataIntfHuaYi();
    }
    return s_instance;
}

CDataIntfHuaYi::CDataIntfHuaYi(QObject *_parent) : QObject(_parent)
{
    //
    QObject::connect(this, &CDataIntfHuaYi::sigQueryPatienInfo, this, &CDataIntfHuaYi::slot_this_QueryPatienInfo, Qt::QueuedConnection);
}

CDataIntfHuaYi::~CDataIntfHuaYi()
{

}

void CDataIntfHuaYi::setWorkerThread(QThread *_thread)
{
    instance()->moveToThread(_thread);
}

bool CDataIntfHuaYi::isMyQrCode(const QByteArray &_line_bytes)
{
    // NOTE: 文档《20260202_视筛新版需求_刘宇\华谊对接华谊对接流程.docx》

    //
    QString qr_code = QString::fromUtf8(_line_bytes);
    return (qr_code.startsWith("http"));
}

bool CDataIntfHuaYi::parseQrCode(const QByteArray _line_bytes, QString &_patient_id)
{
    // NOTE: 文档《20260202_视筛新版需求_刘宇\华谊对接华谊对接流程.docx》

    //
    int idx_last_separator = _line_bytes.lastIndexOf('/');
    if (idx_last_separator > 0) {
        _patient_id = QString::fromUtf8(_line_bytes.mid(idx_last_separator + 1).trimmed());
        if (!_patient_id.isEmpty()) {
            return true;
        } else {
            logWarning(QString("%1::%2(): failed to get patient ID!").arg(S_CLASS_NAME).arg(__FUNCTION__));
            return false;
        }
    } else {
        logWarning(QString("%1::%2(): failed to get last separator index! idx_last_separator = %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(idx_last_separator));
        return false;
    }
}

bool CDataIntfHuaYi::sendPatienInfoQuery(const QByteArray &_line_bytes, QString &_err_msg)
{
    // NOTE: 文档《20260202_视筛新版需求_刘宇\华谊对接华谊对接流程.docx》

    // 条件检查：二维码内容不可为空
    if (_line_bytes.isEmpty()) {
        _err_msg = tr("二维码内容为空！");  // "The QR code content is empty!"
        return false;
    }

    // 条件检查：网络须已连接
    if (!g_WifiIntf->getIsConnected()) {
        _err_msg = tr("网络未连接!");    // "Network is disconnect!"
        return false;
    }

    // 条件检查：接口设置须正确
    if (dataInterfaceCfg_Http != WinDataTrans::getCfg_intfType()) {
        _err_msg = tr("数据接口设置错误") + tr("：")     // "Data interface type setting error", ": "
                + tr("须是“%1”接口！").arg(enumToText_DataInterfaceCfg(dataInterfaceCfg_Http));    // "Must be '%1' interface!"
        return false;
    }
    if (DataTransmiter::ReceiverAddr.empty()) {
        _err_msg = tr("数据接口设置错误") + tr("：")     // "Data interface type setting error", ": "
                + tr("“IP/域名”不可为空！").arg(enumToText_DataInterfaceCfg(dataInterfaceCfg_Http));    // "'IP/DomainName' cannot be empty!"
        return false;
    }
    if (DataTransmiter::PathClient.empty()) {
        _err_msg = tr("数据接口设置错误") + tr("：")     // "Data interface type setting error", ": "
                + tr("“被测者信息接口”不可为空!");    // "'Subject Query path' cannot be empty!"

        return false;
    }

    //
    QString patient_id;
    bool succ = parseQrCode(_line_bytes, patient_id);
    if (succ) {
        emit sigQueryPatienInfo(patient_id, QPrivateSignal());
        return true;
    } else {
        _err_msg = tr("解析二维码失败！"); // "Failed to parse QrCode!"
        return false;
    }
}

void CDataIntfHuaYi::slot_this_QueryPatienInfo(QString _patient_id)
{
    EHuayiPatientInfo patient_info;
    QString err_msg;
    bool is_succ = queryPatienInfo(_patient_id, patient_info, err_msg);

    QDate birthday      = QDate::fromString(patient_info.data.birthday, "yyyy-MM-dd");
    QString business    = patient_info.data.business;
    QString name        = patient_info.data.name;
    QString pid         = patient_info.data.pid;
    int age             = patient_info.data.age.toInt();

    emit sigReceivedPatientInfo(is_succ, err_msg, birthday, business, name, pid, age);
}

bool CDataIntfHuaYi::queryPatienInfo(const QString &_patient_id, EHuayiPatientInfo &_patient_info, QString &_err_msg)
{
    //
    QString protocal = DataTransmiter::IsUseHttps ? "https" : "http";
    QString domain = QString::fromStdString(DataTransmiter::ReceiverAddr);
    QString port_str = DataTransmiter::ReceiverPort > 0 ? ":" + QString::number(DataTransmiter::ReceiverPort) : "";
    QString path = QString::fromStdString(DataTransmiter::PathClient);
    QString url_str = QString("%1://%2%3%4").arg(protocal).arg(domain).arg(port_str).arg(path);
    QUrl url(url_str);

    //
    QByteArray post_data = QString("{\"userId\":%1}").arg(_patient_id).toUtf8();
    QString post_content_type = "application/json";

    //
    QByteArray reply_data;
    QString reply_content_type;
    int http_stat;
    bool succ_request = Common::Net::sendHttpRequest(url, true, &post_data, &post_content_type, &reply_data, &reply_content_type, &http_stat, &_err_msg);
    if (succ_request) {
        //
        if (200 == http_stat) {
            // 条件检查：应答的类型须为 JSON
            if (!reply_content_type.toUpper().contains("JSON")) {
                _err_msg = tr("应答数据不是合法JSON！") + "\nLeft 100 = \"" + reply_data.left(100) + "\"";   // "The response data is not valid JSON!"
                return false;
            }

            // JSON 反序列化
            QJsonParseError json_err;
            QJsonDocument json_doc = QJsonDocument::fromJson(reply_data, &json_err);
            if (QJsonParseError::ParseError::NoError != json_err.error) {
                _err_msg = tr("JSON解析出错") + tr("：") + json_err.errorString();   // "JSON parsing error", ": "
                return false;
            }
            if (json_doc.isEmpty()) {
                _err_msg = tr("应答数据不是合法JSON！") + "\nLeft 100 = \"" + reply_data.left(100) + "\"";   // "The response data is not valid JSON!"
                return false;
            }
            if (!json_doc.isObject()) {
                _err_msg = tr("应答数据不是合法JSON！") + "\nLeft 100 = \"" + reply_data.left(100) + "\"";   // "The response data is not valid JSON!"
                return false;
            }

            _patient_info.fromJson(json_doc.object());

            // JSON合法性检查
            if (_patient_info.code == 0) {
                _err_msg = tr("应答数据不是合法JSON！") + "\nLeft 100 = \"" + reply_data.left(100) + "\"";   // "The response data is not valid JSON!"
                return false;
            }
            if (200 == _patient_info.code && _patient_info.data.pid.isEmpty()) {
                _err_msg = tr("应答数据不是合法JSON！") + "\nLeft 100 = \"" + reply_data.left(100) + "\"";   // "The response data is not valid JSON!"
                return false;
            }

            // 服务端错误状态的检查处理
            if (200 != _patient_info.code) {
                _err_msg = tr("服务端返回错误消息") + tr("：")    // "The server returned an error message", ": "
                        + QString("code=%1, msg=%2").arg(_patient_info.code).arg(_patient_info.message);
                return false;
            }

            //
            return true;
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
