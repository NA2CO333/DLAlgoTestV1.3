#ifndef MPRO_SYS_COMMUNIC_H
#define MPRO_SYS_COMMUNIC_H

#include <QObject>
#include <QWebSocket>
#include <QThread>
#include <QTimer>
#include <QPixmap>
#include <QNetworkAccessManager>

#include "websocket.h"

/* 模块功能：MPro 系统（万灵帮桥云端）的通信功能封装。
 */

//
namespace Net {
namespace Remote {

// 门诊档案数据类
struct stOutpatientArchive
{
    // 字段
    QString birthdate;          // 出生日期，格式：yyyy-MM-dd
    QString gender;             // 性别，M-男，F-女
    QString phone;              // 电话
    QString name;               // 姓名（已经过 URL 编码）
    QString treatmentNumber;    // 诊疗号（档案编号）      // NOTE: 这里云端的命名可能不规范，treatmentNumber 应为 outpatientNumber
    QString type;               // 档案类型（可忽略）
    QString system;             // 系统："outpatient"门诊系统，"school"学校系统
    QString klassName;          // 班级

    // 方法
    void reset();
    bool fromJson(QString _json_str);
    bool isOutpatient() { return (system.isEmpty() || system == SYSTEM_OUTPATIENT); }

    // 常量
    static constexpr char SYSTEM_OUTPATIENT[]   = "outpatient";     // 门诊系统，"school"学校系统
    static constexpr char SYSTEM_SCHOOL[]       = "school";         // 门诊系统，"school"学校系统
};

// 设备激活状态数据类
struct stDevActivateStat
{
    static constexpr int DEFAULT_ACTIVATE_STAT = -1;

    int activaionStatus {DEFAULT_ACTIVATE_STAT};    // 激活状态：1 激活，0 未激活；（本地自定义：-1 未知（由 DEFAULT_ACTIVATE_STAT 定义））

    void reset();
    bool fromJson(QString _json_str);
};
/*  “设备激活状态”业务规则：
 * 1、若设备未激活，则禁止使用测量功能。
 */

// 【MPro系统推送服务通信】功能封装
class CMProSysPushSvcCommunic : public QObject
{
    Q_OBJECT
public:
    explicit CMProSysPushSvcCommunic(QObject *_parent = nullptr);
    ~CMProSysPushSvcCommunic();

    void setServiceAddr(const QUrl &_url);                                                              // 设置服务地址（格式：“协议://IP:端口”）
    void setRequestParam_DevCode(const QString &_dev_code) { m_requestParam_DevCode = _dev_code; }      // 设置请求参数 - 产品编号

    void setWorkThread(QThread *_thread);                   // 设置工作线程

    CWebSocket *webSocket();

    bool setIsOpened(bool _is_open, QString *_msg = Q_NULLPTR);     // 设置“是否已打开”状态
    bool getIsOpened();                                             // 获取“是否已打开”状态

Q_SIGNALS:
    void sigReceivedOutpatientArchive(Net::Remote::stOutpatientArchive _archive);
    void sigDevActivateStatReceived(Net::Remote::stDevActivateStat _activate_stat);

protected slots:
    void slot_webSocket_textMessageReceived(const QString &_message);

protected:

    static const QString S_SERVICE_PATH;        // 服务路径

    CWebSocket *m_webSocket;

    QString m_serviceAddr;                      // 服务地址（格式：“协议://IP:端口”）
    QString m_requestParam_DevCode;             // 请求参数 - 产品编号

};

// 【MPro系统其它接口通信】功能封装（除了通用的数据传输模块已有的功能外）
class CMProSysCommunic : public QObject
{
    Q_OBJECT
public:
    explicit CMProSysCommunic(QObject *_parent = nullptr);
    ~CMProSysCommunic();

    static void setNetworkAccessManager(QNetworkAccessManager *_net_manager);
    static void setConfigDirPath(const QString &_dir_path);                         // 设置配置目录路径

    void setServiceAddr(const QUrl &_url);                                          // 设置服务地址（格式：“协议://IP:端口”）
    void setDevCode(const QString &_dev_code);                                      // 设置请求参数 - 产品编号

    bool requestWxServiceQrCodeImage(QString &_err_msg);                            // 请求微信服务二维码图片

    bool loadWxServiceQrCodeImageFromFile();                                        // 从文件载入微信服务二维码图片
    const QPixmap &wxServiceQrCodeImage();                                          // 微信服务二维码图片（若图片 isNull()，需先调用请求且成功）
    bool invalidateWxServiceQrCodeImage();                                          // 使微信服务二维码图片失效（如设备编码修改后，当前二维码应当失效）
    const QString &wxSvcQrCodeImgFilePath();                                        // 微信服务二维码图片路径

protected:
    bool getQrCodeReplyJsonField_Msg(const QByteArray &_reply_data, QString &_msg);         // 解析二维码应答 JSON 里的字段 - "msg"

    static QNetworkAccessManager *s_netManager;

    static const QString S_SERVICE_PATH;                    // 服务路径
    static QString s_configDirPath;                         // 配置目录路径
    static const char S_WX_SVC_QR_CODE_IMG_FILE_NAME[];     // 微信服务二维码图片文件名
    static const char S_WX_SVC_QR_CODE_PARAM_FILE_NAME[];   // 微信服务二维码参数文件名

    QString m_serviceAddr;                      // 服务地址（格式：“协议://IP:端口”）
    QString m_devCode;                          // 产品编号
    QString m_devCodeOfQrCode;                  // 二维码图像的产品编号参数（从文件载入，校验过后清空）

    QPixmap m_wxServiceQrCodeImage;                 // 微信服务二维码图片

};

}   // namespace Remote
}   // namespace Net

#endif // MPRO_SYS_COMMUNIC_H
