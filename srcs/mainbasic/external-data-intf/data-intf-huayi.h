#ifndef CDATAINTFHUAYI_H
#define CDATAINTFHUAYI_H

#include <QObject>
#include <QString>
#include <QDate>
#include <QThread>

#include "qserializer.h"

/* 华谊通信样例：
 *
 * 二维码内容：
 * http://hysafour.huayiyunxinxi.com/system/medical/scanqr/MTU.23/6430
 *
 * 得到的受检者查询URL：
 * http://hysafour.huayiyunxinxi.com/wx/MTU.23/loadUserInfo
 *
 * JSON of body: {"userId":6430}
 *
 * JSON of response:
 * {
 *     "code": 200,
 *     "data": {
 *         "birthday": "2014-02-01",
 *         "business": "MTU.23",
 *         "name": "华谊测试",
 *         "pid": "6430",
 *         "age": "12"
 *     },
 *     "message": "数据获取成功！"
 * }
 */

//
class EHuayiPatientInfoData : public QSerializer
{
    Q_GADGET
    QS_SERIALIZABLE

    QS_FIELD(QString, birthday, "");
    QS_FIELD(QString, business, "");
    QS_FIELD(QString, name    , "");
    QS_FIELD(QString, pid     , "");
    QS_FIELD(QString, age     , "");
};

//
class EHuayiPatientInfo : public QSerializer
{
    Q_GADGET
    QS_SERIALIZABLE

    QS_FIELD(int, code, 0);
    QS_OBJECT(EHuayiPatientInfoData, data);
    QS_FIELD(QString, message, "");
};

// 华谊通信功能封装
class CDataIntfHuaYi : public QObject
{
    Q_OBJECT
public:
    static CDataIntfHuaYi *instance();
    ~CDataIntfHuaYi();

    static void setWorkerThread(QThread *_thread);                              // 设置工作线程

    static bool isMyQrCode(const QByteArray &_line_bytes);                          // 检查指定的二维码内容是否是本协议的
    static bool parseQrCode(const QByteArray _line_bytes, QString &_patient_id);    // 解析获取二维码内容中的相关字段

    bool sendPatienInfoQuery(const QByteArray &_line_bytes, QString &_err_msg);     // 发送受检者信息查询（异步）

Q_SIGNALS:
    void sigReceivedPatientInfo(bool _is_succ, QString _err_msg, QDate _birthday, QString _business, QString _name, QString _pid, int _age);
    /* 私有信号 */
    void sigQueryPatienInfo(QString _patient_id, QPrivateSignal);

protected Q_SLOTS:
    void slot_this_QueryPatienInfo(QString _patient_id);

protected:
    explicit CDataIntfHuaYi(QObject *_parent = nullptr);
    static CDataIntfHuaYi *s_instance;
    static const char * const S_CLASS_NAME;     // 本类的类名

    bool queryPatienInfo(const QString &_patient_id, EHuayiPatientInfo &_patient_info, QString &_err_msg);
};

#endif // CDATAINTFHUAYI_H
