#include "authintf.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>

#include "global.h"

// 授权接口应答数据_试用期限信息
class CDataAuthIntfResp_TrialInfo
{
public:
    QString nowDate;            // 当前日期，日期格式 “yyyy-MM-dd”
    QString endDate;            // 到期日期，日期格式 “yyyy-MM-dd”
    int status {0};             // 授权状态：1-试用，2-永久（同 enAuthType）
    int activaionStatus {0};    // 是否已激活。0 否，1 是
};

// 授权接口应答数据
class CDataAuthIntfResp
{
public:
    int code = 0;                           // http 状态码，若成功，则是 200，失败则是 400
    QString status;                         // 成功或失败状态，"success"-成功，"fail"-失败
    QString msg;
    QString redirectUrl;
    bool back = false;
    bool refresh = false;
    CDataAuthIntfResp_TrialInfo data;       // 授权信息数据
    QString id;                             // ？（服务端返回 JSON 里有此字段）

public:
    static constexpr int CODE_SUCC = 200;               // 状态码：成功
    static constexpr int CODE_FAIL = 400;               // 状态码：成功
    static constexpr char STATUS_SUCC[] = "success";    // 状态字符串：成功
    static constexpr char STATUS_FAIL[] = "fail";       // 状态字符串：失败
};

bool jsonToObj(QJsonDocument &_json_doc, CDataAuthIntfResp &_obj)
{
    /* 数据样例：
    { "code": 400, "status": "fail", "msg": "设备未绑定", "redirectUrl": null, "back": false, "refresh": false, "data": null, "id": null }
    { "code": 200, "status": "success", "msg": null, "redirectUrl": null, "back": false, "refresh": false, "data": { "status": 2 }, "id": null }
    { "code":200,"status":"success","deviceStatus":null,"msg":null,"redirectUrl":null,"back":false,"refresh":false,"data":{"activaionStatus":1,"status":2},"id":null }
    { "code": 200, "status": "success", "msg": null, "redirectUrl": null, "back": false, "refresh": false, "data": { "nowDate": "2023-05-10", "endDate": "2023-05-09", "status": 1 }, "id": null }
    {"code":400,"status":"fail","deviceStatus":null,"msg":"设备不存在","redirectUrl":null,"back":false,"refresh":false,"data":null,"id":null}
    */

    //
    if (_json_doc.isObject()) {
        QJsonObject obj_root = _json_doc.object();
        QStringList keys = obj_root.keys();
        for (QString key : keys){
            QJsonValue value = obj_root.value(key);
            if (value.isString()) {                                     // 字符串类型字段
                qDebug() << key << ": " << value.toString();
                if ("status" == key) {
                    _obj.status = value.toString();
                } else if ("msg" == key) {
                    _obj.msg = value.toString();
                }
            } else if (value.isDouble()) {                              // 数字类型字段
                qDebug() << key << ": " << value.toInt();
                if ("code" == key) {
                    _obj.code = value.toInt();
                }
            } else if (value.isArray()) {                               // 数组类型字段
                QJsonArray arr = value.toArray();
                for (int i = 0; i < arr.count(); ++i) {
                    if(arr.at(i).isString()){
                        qDebug() << key << ": "<< arr.at(i).toString();
                    }
                }
            } else if (value.isObject()) {                              // 对象类型字段
                if ("data" == key) {
                    QJsonObject obj_data = value.toObject();
                    QStringList subKeys = obj_data.keys();
                    for (auto key_data : subKeys) {
                        QJsonValue value_data = obj_data.value(key_data);
                        if (value_data.isString()) {
                            qDebug() << key_data <<": "<< value_data.toString();
                            if ("nowDate" == key_data) {
                                _obj.data.nowDate = value_data.toString();
                            } else if ("endDate" == key_data) {
                                _obj.data.endDate = value_data.toString();
                            }
                        } else if (value_data.isDouble()) {
                            qDebug() << key_data << ": " << value_data.toInt();
                            if ("status" == key_data) {
                                _obj.data.status = value_data.toInt();
                            } else if ("activaionStatus" == key_data) {
                                _obj.data.activaionStatus = value_data.toInt();
                            }
                        }
                    }
                }
            }
        }

        qDebug() << _obj.code << "  " << _obj.data.status << " " << _obj.data.nowDate << " " << _obj.data.endDate << endl;
    } else {
        return false;
    }

    return true;
}

//
const QString CAuthIntf::S_SERVICE_PATH {"/api-v1/equipment/getTrialTime?serial=%1&classification=%2"};     // 服务路径

QNetworkAccessManager *CAuthIntf::s_netManager {nullptr};

CAuthIntf::CAuthIntf(QObject *_parent)
    : QObject(_parent)
{
    if (!s_netManager) {
        logWarning(QString("%1: QNetworkAccessManager not been setted! Created internal!").arg(__PRETTY_FUNCTION__));
        s_netManager = new QNetworkAccessManager();
    }

}

CAuthIntf::~CAuthIntf()
{

}

void CAuthIntf::setNetworkAccessManager(QNetworkAccessManager *_net_manager)
{
    s_netManager = _net_manager;
}

void CAuthIntf::setDevType(enAuthDevType _dev_type)
{
    devType = _dev_type;
}

void CAuthIntf::setDevNum(QString _dev_num)
{
    devNum = _dev_num;
}

void CAuthIntf::setIsUseTestEnv(bool _is_produce)
{
    isUseTestEnv = _is_produce;
}

const CAuthIntf::stAuthInfo *CAuthIntf::getAuthInfo()
{
    return m_lastAuthInfo;
}

void CAuthIntf::slot__QueryAuthInfo()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": enter ...", CGlobal::LOG_SYS);

    lastErr.clear();
    enAuthIntfErrType err_type = queryAuthInfo(m_lastAuthInfo);

    emit sigQueryAuthInfoFinished(err_type, lastErr);

}

CAuthIntf::enAuthIntfErrType CAuthIntf::queryAuthInfo(stAuthInfo *&_auth_info)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": enter ...", CGlobal::LOG_SYS);

    /* 接口说明文档：20230519_试用机激活接口.doc
     *
     */

    static const QString SVC_ADDR_PRODUCE   = "https://opt.manylinksmed.com";   // 服务地址（生产环境）
    static const QString SVC_ADDR_DEVELOP   = "http://120.25.254.38:8080";      // 服务地址（开发环境）

    enAuthIntfErrType ret = authIntfErrType_Fail;

    // 若已有授权信息不为空，则清空
    if (_auth_info) {
        delete _auth_info;
        _auth_info = nullptr;
    }

    //
    QString url_str = QString("%1%2").arg(!isUseTestEnv ? SVC_ADDR_PRODUCE : SVC_ADDR_DEVELOP).arg(S_SERVICE_PATH);
    url_str = url_str.arg(devNum).arg(devType);

    QNetworkRequest request;        // TODO: 改用 Common::Net::sendHttpRequest()？

    request.setUrl(QUrl(url_str));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json;charset=utf-8"));
    QNetworkReply* reply = s_netManager->get(request);

    // 开启事件循环，直到请求完成
    QEventLoop event_loop;
    connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
    event_loop.exec();

    if (QNetworkReply::NoError == reply->error()) {
        QByteArray data_bytes = reply->readAll();
        qDebug() << "Reply data:" << QString::fromUtf8(data_bytes) << endl;

        QJsonParseError json_err;
        QJsonDocument json_doc = QJsonDocument::fromJson(data_bytes, &json_err);
        if (QJsonParseError::NoError == json_err.error) {
            CDataAuthIntfResp data_obj;
            bool json_valid = jsonToObj(json_doc, data_obj);
            if (json_valid) {
                if (CDataAuthIntfResp::CODE_SUCC == data_obj.code) {                    // 查询成功
                    if (authType_Trial == (enAuthType)data_obj.data.status) {
                        _auth_info = new stAuthInfo();
                        _auth_info->authType        = authType_Trial;
                        _auth_info->today           = QDate::fromString(data_obj.data.nowDate, "yyyy-MM-dd");
                        _auth_info->expiryDate      = QDate::fromString(data_obj.data.endDate, "yyyy-MM-dd");

                        //
                        if (!_auth_info->today.isValid()) {
                            qDebug() << "data err: 'nowDate' not valid";

                            ret = authIntfErrType_DataValueInvalid;
                            lastErr = "服务器返回的当前日期不合法";
                        } else if (!_auth_info->expiryDate.isValid()) {
                            qDebug() << "data err: 'endDate' not valid";

                            ret = authIntfErrType_DataValueInvalid;
                            lastErr = "服务器返回的到期日期不合法";
                        } else {
                            qDebug() << "query succeeded";

                            ret = authIntfErrType_Succ;
                            lastErr.clear();
                        }
                    } else if (authType_Permanent == (enAuthType)data_obj.data.status) {
                        _auth_info = new stAuthInfo();
                        _auth_info->authType = authType_Permanent;

                        //
                        ret = authIntfErrType_Succ;
                        lastErr.clear();
                    } else {
                        qDebug() << "data err: 'status' not valid";

                        ret = authIntfErrType_DataValueInvalid;
                        lastErr = "服务器返回的授权类型不合法";
                    }

                    // 设备激活状态
                    _auth_info->isDevActivated = (1 == data_obj.data.activaionStatus);
                } else if (CDataAuthIntfResp::CODE_FAIL == data_obj.code) {
                     _auth_info = new stAuthInfo();
                     _auth_info->authType = authType_NotSet;

                     lastErr = data_obj.msg;

                     ret = authIntfErrType_Fail;
                } else {
                    qDebug() << "data err: 'code' not valid";

                    ret = authIntfErrType_DataValueInvalid;
                    lastErr = QString("服务器返回的请求状态失败，code=%1").arg(data_obj.code);
                }
            } else {
                qDebug() << "json format invalid!";

                ret = authIntfErrType_JsonInvalid;
                lastErr = "服务器返回的 JSON 不合法";
            }
        } else {
            qDebug() << "response str to json failed, error:" << json_err.error << ", " << json_err.errorString();

            ret = authIntfErrType_RespToJsonFail;
            lastErr = "服务器返回的 JSON 不合法";
        }
    } else {
        qDebug() << "request error:" << reply->error();

        ret = authIntfErrType_QueryFail;
        lastErr = QString("请求失败，err=%1").arg(QVariant::fromValue(reply->error()).toString());
    }

    //
    if (reply) {
        reply->deleteLater();       // NOTE: QNetworkReply 须由调用者释放。
    }

    //
    return ret;
}

