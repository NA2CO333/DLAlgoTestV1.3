#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QThread>
#include <QUrl>
#include <QNetworkReply>
#include <QFile>

#include "remoteservicedefs.h"

// 前置声明
class QNetworkAccessManager;

namespace Net {
namespace Remote {

// 前置声明
class CRemoteService;

///=============================================================================================================
/// 数据类

const QString STAT_SUCC = QStringLiteral("succ");
const QString STAT_FAIL = QStringLiteral("fail");

// 数据类基类
class CBusiData     // TODO: 实现自动 JSON 序列化和反序列化
{
protected:

};

// “通信消息”类
class CCommunicMessage : public CBusiData
{
public:
    QString command;        // 指令编码
    QString stat;           // 状态码（见名称以“STAT_”开头的字符串常量）
    QString data;           // 被消息所携带的数据对象序列化后得到的JSON字符串
    QString msg;            // 消息。如错误消息，提示消息等，用于提示用户
    QString version;        // 数据格式/协议版本号（待扩展，暂留空，可省略）
    QString sender;         // 消息发送者。如果发送者是设备端，则为设备编号，否则可为空

    bool fromJson(const QString &_json_str);
    QString toJson();
};

// “目录查询请求”数据类
class CReqDirList : public CBusiData
{
public:
    QString dirPath;        // 想要查询的目标文件夹的路径（相对于顶层目录的路径）
    QString currPath;       // 当前目录的路径，可为空,若目标目录为".."，则必须要有

    bool fromJson(const QString &_json_str);
};

// “文件信息”数据类
class CFileInfo : public CBusiData
{
public:
    QString name;           // 文件名（不包含路径）
    bool isDir;             // 是否文件夹
    QString size;           // 文件大小
    QString date;           // 文件日期

    QString toJson();
    void toJsonObj(QJsonObject &_json_obj) const;
};

// “目录查询应答”数据类
class CRespDirList : public CBusiData
{
public:
    QString dirPath;                // 当前目录相对于顶层目录的路径
    int count = 0;                  // “文件信息”数组的元素个数
    QVector<CFileInfo> fileList;    // “文件信息”数组

    QString toJson();
};

// “文件上传请求”数据类
class CReqUploadFile : public CBusiData
{
public:
    QString dirPath;    // 想要上传的文件所在的文件夹的路径（相对于顶层目录的路径）
    QString name;       // 文件名（不包含路径）
    int bytesPSeg;      // 每段字节数(bytes per segmentation)，默认 100k(1024*100) bytes
    int segBegin;       // 起始分段的索引号（默认为 0）
    int byteBegin;      // 起始字节的索引号（从起始分段的第0字节算起，默认为 0）
    int timeout;        // 超时时长（单位：秒）
    bool compress;      // 是否需要压缩（默认不压缩）
    QString format;     // 压缩格式（支持 ".gz", ".zip"，默认".gz"）
    bool needCheck;     // 是否需要校验（默认false）
    QString verifiAlgo; // 校验算法（支持："crc32"，"md5"，默认"crc32"）

    bool fromJson(const QString &_json_str);
};

// “文件上传开始”数据类
class CPostFileInfo : public CBusiData
{
public:
    QString dirPath;    // 文件所在的路径（相对路径，同上）
    QString name;       // 文件名（不包含路径）
    int size = 0;       // 文件大小（单位：字节）

    QString toJson();
};

// “文件上传结束”数据类
class CPostFileEnd : public CBusiData
{
public:
    QString dirPath;    // 文件所在的路径（相对路径，同上）
    QString name;       // 文件名（不含路径，和请求的文件名不一定一致）
    int size;           // 已上传字节数
    bool isSucc;        // 是否上传成功（指设备端是否处理成功）
    QString checksum;   // 文件的校验值（上传成功时才有，以无分隔符的十六进制文本表示）

    QString toJson();
};

///=============================================================================================================
/// class CCommandHandler

// 指令处理函数（一个指令一般只有一个处理器，但是一个处理器通常会支持多个指令，即指令和处理器一般是多对一关系）
using funcCmdPrecess = std::function<bool(const QString &, QString &)>;

// 指令处理者
class CCommandHandler : public QObject
{
    Q_OBJECT
public:
    explicit CCommandHandler(CRemoteService *_service, QObject *_parent = nullptr);

    /**
     * @brief 获得本指令处理者支持的指令列表
     * @param _cmd_list 【输出参数】本指令处理者支持的指令列表
     */
    void getSupportedCmds(QStringList &_cmd_list);

    /**
     * @brief 处理指定的指令
     * @param _cmd              指令编号
     * @param _data_json_str    业务数据对象序列化之后得到的 JSON 字符串
     * @param _err_msg          【输出参数】错误消息。若处理失败，则由此参数返回错误消息
     * @return  是否成功
     */
    bool processCmd(const QString &_cmd, const QString &_data_json_str, QString &_err_msg);

protected:
    CRemoteService *service = Q_NULLPTR;
    QMap<QString, funcCmdPrecess> *cmdProcessFuncs = Q_NULLPTR;         // “指令-处理函数” 映射表

};

///=============================================================================================================
/// class CDirListHandler

// “目录浏览及下载”指令处理者的类名
extern const QString TYPE_NAME_DIR_LIST;

//
class CFileUpload;

// “目录浏览及下载”指令处理者
class CDirListHandler : public CCommandHandler
{
    Q_OBJECT
public:
    explicit CDirListHandler(CRemoteService *_service, QObject *_parent = nullptr);

    /**
     * @brief 添加顶层目录路径（支持添加多个路径，且各个路径的文件夹层数可以不同）
     * @param _dir_path 目录的绝对路径
     */
    void addTopDir(QString _dir_path);

    void setUploadSvcUrl(QUrl _url);                        // 设置文件上传服务路径
    QUrl getUploadSvcUrl();

signals:
    /* 私有 */
    void sigUploadFile(CReqUploadFile _req_upload_file);            // 上传文件

protected slots:
    void slot_fileUpload_UploadError(QString _err_msg);
    void slot_fileUpload_Message(Net::Remote::enLogType _log_type, QString _msg);

protected:
    QStringList topDirs;        // 顶层目录列表（路径都以 '/' 结尾）

    CFileUpload *fileUpload = Q_NULLPTR;
    QThread *threadUploadFile = Q_NULLPTR;

    bool processCmd_DirList(const QString &_data_json_str, QString &_err_msg);
    bool processCmd_UploadFile(const QString &_data_json_str, QString &_err_msg);

    bool isInTopDirs(const QString &_dst_path);                 // 检查指定的路径是否在顶层目录内

    void uploadFile(const CReqUploadFile &_req_upload_file);            // 上传文件（原协议，分段通过 websocket 的 binary 消息上传）

};

///=============================================================================================================
/// class CFileUpload

// 文件上传类
class CFileUpload : public QObject
{
    Q_OBJECT

public:
    friend class CDirListHandler;

    explicit CFileUpload(CRemoteService *_service, QObject *_parent = nullptr);

    static void setNetworkAccessManager(QNetworkAccessManager *_net_manager);

signals:
    void sigUploadAborted(QString _err_msg);                            // “上传中断”信号
    void sigMessage(Net::Remote::enLogType _log_type, QString _msg);    // “消息”信号     /* 不可直接调用 service->emitLog()，因为线程不同 */

public slots:
    void slotUploadFile(CReqUploadFile _req_upload_file);

protected:
    CRemoteService *service = Q_NULLPTR;

    static QNetworkAccessManager *s_netManager;

    QUrl uploadSvcUrl;

    /**
     * @brief 打包文件夹
     * @param _dir_path 文件夹路径
     * @param _tar_path 【输出参数】打包后产生的文件路径
     * @param _err_msg  【输出参数】错误消息
     * @return          是否成功
     */
    static bool tarDir(const QString &_dir_path, QString &_tar_path, QString &_err_msg);

    /**
     * @brief 将文件或文件夹压缩为 zip 文件
     * @param _file_path
     * @param _zip_path
     * @param _err_msg
     * @return
     */
    static bool zipFileOrDir(QString _file_path, QString &_zip_path, QString &_err_msg);

    void uploadFile(const CReqUploadFile &_req_upload_file);           // 上传文件（暂弃用原协议，改为通过 http 文件上传方法上传到通用的文件上传接口）

    /**
     * @brief 上传文件到万灵云端的文件上传接口
     * @param _url
     * @param _file_name
     * @param _file
     * @param _http_stat    【输出参数】http 应答状态码
     * @param _body         【输出参数】http 应答报文的 body
     * @param _bytes_sent   【输出参数】已上传字节数
     * @param _bytes_total  【输出参数】总共要上传的字节数
     * @return              网络错误。若小于 0，则为自定义的值。-1：超时；其它值：未有规定其意义（表明程序逻辑有漏洞）；
     */
    QNetworkReply::NetworkError uploadHttpMultipartFile(const QUrl &_url, const QString &_file_path, int &_http_stat, QByteArray &_body, qint64 &_bytes_sent, qint64 &_bytes_total);

};

}   // namespace Remote
}   // namespace Net

#endif // COMMANDHANDLER_H
