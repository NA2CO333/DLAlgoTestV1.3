#ifndef NETTOOLS_H
#define NETTOOLS_H

#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>

namespace Common {
namespace Net {

// 设置全局 Qt 网络管理者对象
void setNetworkAccessManager(QNetworkAccessManager *_net_manager);

/**
 * @brief 得到广播地址（使用 RK 平台）
 * @param _ip
 * @return 若传入 IP，则找该 IP 所对应的广播地址，否则找第一个发现的广播地址
 */
QString getBroadcastAddr(QString _ip = "");

// 得到默认网关
QString getDefaultGateway();

// 判断指定 IP 是否局域网 IP
bool isLanIP(QString _ip);

// 判断是否能访问互联网
bool isAccessToInternet();

/**
 * @brief 发送 http 请求（仅支持 GET 和 POST 类型），返回字节应答中的序列。
 * @param _url
 * @param _is_post              请求的类型是否为 POST。若否，则为 GET，即仅支持 GET 和 POST 两种 http 请求类型
 * @param _post_data            待 POST 的数据。若是 GET 请求，可为空
 * @param _post_content_type    待 POST 的数据的类型。若是 GET 请求，可为空
 * @param _reply_data
 * @param _reply_content_type
 * @param _http_reply_stat      应答的 http 状态码（初始值为-1）
 * @param _err_msg
 * @param _net_access_mgr       Qt 的 QNetworkAccessManager 对象，若传入空指针，则使用本模块内部的对象
 * @return 通信是否正常（注意：这里仅检查请求是否顺利完成，而服务端返回的错误，如http状态码，这里不作判定，由调用方判定）
 * @note 调用示例：CMProSysCommunic::requestWxServiceQrCodeImage()
 */
bool sendHttpRequest(const QUrl &_url, const bool _is_post, const QByteArray *_post_data = nullptr, const QString *_post_content_type = nullptr,
                     QByteArray * const _reply_data = nullptr, QString * const _reply_content_type = nullptr, int *_http_reply_stat = nullptr,
                     QString * const _err_msg = nullptr, QNetworkAccessManager *_net_access_mgr = nullptr);

}   // namespace Net
}   // namespace Common

#endif // NETTOOLS_H
