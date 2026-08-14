#ifndef CDATAINTFGUANXIN_H
#define CDATAINTFGUANXIN_H

#include <QObject>
#include <QList>

#include "qserializer.h"

// 前置声明
class QNetworkAccessManager;

// 新疆冠新接口配置
struct stGuanXinIntfCfg {
    /* 新疆冠新测试服务器 */
    QString ip          = "202.100.182.100";                                    // IP 或 域
    int     port        = 10099;                                                // 端口
    QString pathUpload  = "/dataadapterline/outEqpt/publicEqpinfo";             // 结果上传路径
    QString pathQuery   = "/dataadapterline/outEqpt/findPercheckinfoMsg";       // 名单查询路径

    bool    isHttps     = false;    // 是否使用 https

    bool isEqualTo(const stGuanXinIntfCfg &_other) const {
        return (true &&
                this->ip        == _other.ip        &&
                this->port      == _other.port      &&
                true
                );
    }
};

// 区域信息
struct stAreaInfo {
    QString code;       // 编号
    QString name;       // 名称

    stAreaInfo(const QString &_code, const QString &_name)  : code(_code), name(_name) {}
    stAreaInfo(const char *_code, const char *_name)        : code(_code), name(_name) {}
};

// 结果项的名称列表（通过静态常量字符串来定义各个结果项的名称）
class CResultItemNames
{
public:
    static constexpr char TongJu            []{"TongJu"         };          // 瞳距，单位：mm
    static constexpr char Ltongkongdaxiao   []{"Ltongkongdaxiao"};          // 左瞳孔大小，单位：mm
    static constexpr char Rtongkongdaxiao   []{"Rtongkongdaxiao"};          // 右瞳孔大小，单位：mm
    static constexpr char Laxialview        []{"Laxialview"     };          // 左轴位，单位：°
    static constexpr char Raxialview        []{"Raxialview"     };          // 右轴位，单位：°
    static constexpr char LZhuJing          []{"LZhuJing"       };          // 左柱镜，单位：D
    static constexpr char RZhuJing          []{"RZhuJing"       };          // 右柱镜，单位：D
    static constexpr char LYanQiuJing       []{"LYanQiuJing"    };          // 左眼球镜，单位：D
    static constexpr char RYanQiuJing       []{"RYanQiuJing"    };          // 右眼球镜，单位：D
    static constexpr char ChuShaiZhenDuan   []{"ChuShaiZhenDuan"};          // 初筛诊断，例：左眼正常，右眼正常
};

namespace Entity {

// 结果项
class EResultItem : public QSerializer
{
    Q_GADGET
    QS_SERIALIZABLE

    QS_FIELD(QString, name  , "");
    QS_FIELD(QString, value , "");
    QS_FIELD(QString, range , "");
};

// 结果上传请求
class EResultRequest : public QSerializer
{
    Q_GADGET
    QS_SERIALIZABLE

    QS_FIELD(QString, testno, "");                      // 学生的体检号
    QS_COLLECTION_OBJECTS(QList, EResultItem, result);  // 学生的筛查结果（数组）

public:
    void addResultItem(const QString &_name, const QString &_value, const QString &_range = "")
    {
        EResultItem item;
        item.name = _name;
        item.value = _value;
        item.range = _range;
        result.append(item);
    }
};

// 结果上传应答
class EResultResponse : public QSerializer {
    Q_GADGET
    QS_SERIALIZABLE

    QS_FIELD(QString, data      , ""    );              // 消息
    QS_FIELD(QString, status    , ""    );              // 状态码

    //bool isSucc() { return isStatCodeSucc(status); }
};

// 名单查询请求
class ETesteeQueryRequest : public QSerializer
{
    Q_GADGET
    QS_SERIALIZABLE

    QS_FIELD(QString, areaCode  , ""    );              // eg."5bf7078e-1986-1c02-e053-1164010a1796",
    QS_FIELD(QString, beginTime , ""    );              // eg."2025-04-01",
    QS_FIELD(QString, endTime   , ""    );              // eg."2025-04-22",
    QS_FIELD(QString, memo      , "SLSC");              // eg."SLSC"
};

// 被测者信息实体
class ETesteeInfo : public QSerializer
{
    Q_GADGET
    QS_SERIALIZABLE

    QS_FIELD(QString, Office    , ""    );              //  eg. ""
    QS_FIELD(QString, Sex       , ""    );              //  eg. "男"
    QS_FIELD(QString, Birthday  , ""    );              //  eg. "1956-10-06"
    QS_FIELD(QString, Time      , ""    );              //  eg. "2025-04-03 09:04:59"
    QS_FIELD(QString, Smoke     , ""    );              //  eg. ""
    QS_FIELD(QString, Weight    , ""    );              //  eg. ""
    QS_FIELD(QString, Name      , ""    );              //  eg. "李四"
    QS_FIELD(QString, PatientID , ""    );              //  eg. "14521"
    QS_FIELD(qint64 , RECORDTIME, 0     );              //  eg. 1743642539000
    QS_FIELD(QString, Height    , ""    );              //  eg. ""
    QS_FIELD(QString, id        , ""    );              //  eg. "7882c853-f18e-4a75-8927-09ec7678c6a6"
    QS_FIELD(QString, HISCODE   , ""    );              //  eg. "5bf7078e-1986-1c02-e053-1164010a1796"
    QS_FIELD(int    , Age       , 0     );              //  eg. 69
    QS_FIELD(QString, MEMO      , ""    );              //  eg. "SLSC"
};

// 名单查询应答
class ETesteeQueryResponse : public QSerializer
{
    Q_GADGET
    QS_SERIALIZABLE

    QS_COLLECTION_OBJECTS(QList, ETesteeInfo, data);    // 名单数组
    QS_FIELD(QString, message   , ""    );              // 消息
    QS_FIELD(QString, status    , ""    );              // 状态码

    //bool isSucc() { return isStatCodeSucc(status); }
};

}   // namespace Entity

// 冠新数据接口（参见 "20250508_视筛2.0需求_崔继友/冠新第三方外部仪器接口文档20250513.docx"）
class CDataIntfGuanXin : public QObject
{
    Q_OBJECT
public:
    explicit CDataIntfGuanXin(QNetworkAccessManager *_net_manager, QObject *_parent = nullptr);
    ~CDataIntfGuanXin();

    static bool init();

    static bool loadConfig();
    static bool saveConfig();

    static const QString DATE_FORMAT;               // 日期格式

    static QVector<stAreaInfo> &areaList();         // 地区列表

    //
    void setConfig(const stGuanXinIntfCfg &_cfg);                           // 接口配置

    bool queryTesteeList(const Entity::ETesteeQueryRequest &_request,
                         Entity::ETesteeQueryResponse &_response, QString &_err_msg);           // 查询名单
    // NOTE: 不提交名单的已接收消息，避免用户误删后难处理
    bool uploadResult(const QString &_area_code, const Entity::EResultRequest &_request,
                      Entity::EResultResponse &_response, QString &_err_msg);                   // 上传结果


protected:
    static QString PATH_AREAS_CFG;                  // 地区代码配置文件路径
    static QVector<stAreaInfo> s_areaList;          // 地区代码配置列表

    stGuanXinIntfCfg m_cfg;
    QNetworkAccessManager *m_netManager{nullptr};

};

#endif // CDATAINTFGUANXIN_H
