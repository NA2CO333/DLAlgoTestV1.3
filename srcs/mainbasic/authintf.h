#ifndef CAUTHINTF_H
#define CAUTHINTF_H

#include <QObject>
#include <QDate>

/*
 * 设备激活状态和云端的同步机制：
 * 通过两个途径从云端获取当前设备的激活状态：
 * 1、通过档案推送服务。可实时接收云端推送的设备激活状态。云端的设备激活状态的推送时机，由云端后端服务程序决定。
 * 2、通过试用机状态查询接口。查询时机：联网后延时10秒开始，若本地状态为未激活，则每隔 60 秒查询一次。
 *
 * 试用机状态和云端的同步机制：
 * 通过试用机状态查询接口。
 * 查询的触发时机：
 * 1、由“WiFi已连接”事件触发，延时10秒后，查询一次。
 * 2、若本地状态为试用机，且查询失败，则定时 10 分钟查询一次。
 * 3、若本地状态为试用机，且距离上次成功查询的时间超过了一天，则再查询一次。        // TODO: 试用机没必要定时查询？
 *
 * 另：试用机须禁止修改系统时间。
 *
 */

// 前置声明
class QNetworkAccessManager;

// 机器授权接口封装
class CAuthIntf : public QObject
{
    Q_OBJECT
public:
    explicit CAuthIntf(QObject *_parent = nullptr);
    ~CAuthIntf();

    static void setNetworkAccessManager(QNetworkAccessManager *_net_manager);

    // 机器授权类型
    enum enAuthType {
        authType_NotSet         = 0,        // 未设置
        authType_Trial          = 1,        // 试用
        authType_Permanent      = 2,        // 永久
    };
    /* “试用机状态”业务规则：
     * 1、若是试用机，若超出试用期限，则禁止使用测量功能。
     * 2、每次查询试用机状态时，服务端应返回当前日期，客户端应校正当前日期。
     */

    // 授权信息（授权状态）
    struct stAuthInfo {
        enAuthType authType;
        QDate today;                // 今天日期
        QDate expiryDate;           // 过期日（即从这天开始过期）
        bool isDevActivated;        // 是否设备已激活
    };

    // 设备类别
    enum enAuthDevType {
        authDevType_Screener    = 1,    // 视筛
        authDevType_IolMaster   = 2,    // 生测
    };

    // 授权接口的错误类型
    enum enAuthIntfErrType {
        authIntfErrType_Succ            = 0,    // 成功
        authIntfErrType_Fail,                   // 失败（原因未知）
        authIntfErrType_QueryFail,              // HTTP 查询失败
        authIntfErrType_RespToJsonFail,         // HTTP 应答数据转 JSON 失败
        authIntfErrType_JsonInvalid,            // JSON 格式非法
        authIntfErrType_DataValueInvalid,       // 数据值非法
    };

    void setDevType(enAuthDevType _dev_type);               // 设置【产品类别】
    void setDevNum(QString _dev_num);                       // 设置【产品编号】

    void setIsUseTestEnv(bool _is_produce);                 // 设置【是否使用测试环境】

    const stAuthInfo *getAuthInfo();                // 获取上次授权查询得到的应答信息。注意：若查询失败，会返回 NULL，所以访问前要做空指针检查

signals:
    void sigQueryAuthInfoFinished(CAuthIntf::enAuthIntfErrType _err_type, QString _err_msg);    // 【授权信息查询】已完成（此后，调用者可以通过本接口得到授权信息）

public slots:
    void slot__QueryAuthInfo();                     // 【授权信息查询】的信号接收槽

protected:
    enAuthIntfErrType queryAuthInfo(stAuthInfo *&_auth_info);

    static const QString S_SERVICE_PATH;            // 服务路径

    static QNetworkAccessManager *s_netManager;
    stAuthInfo *m_lastAuthInfo {nullptr};           // 上次（从程序运行后开始）查询得到的授权信息

    enAuthDevType devType = authDevType_Screener;
    QString devNum;
    bool isUseTestEnv = false;                      // 是否使用测试环境
    QString lastErr;                                // 上次查询的错误信息

};

#endif // CAUTHINTF_H
