#include "nettools.h"

#include <QProcess>
#include <QDebug>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QApplication>

#include "logger.h"

//
namespace Common {
namespace Net {

QNetworkAccessManager *g_netManager {nullptr};

void setNetworkAccessManager(QNetworkAccessManager *_net_manager)
{
    g_netManager = _net_manager;
}

QString getBroadcastAddr(QString _ip)
{
#if (OS_TYPE == 2)
    static const QString KEY_BEGIN = "broadcast ";
    static const QString KEY_END = "\n";
#elif (OS_TYPE == 3)
    static const QString KEY_BEGIN = "Bcast:";
    static const QString KEY_END = " ";
#else
    static const QString KEY_BEGIN = "Bcast:";
    static const QString KEY_END = " ";
#endif

    //
    Q_UNUSED(_ip)

    //
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.start("ifconfig");
    process.waitForFinished(3000);

    QString data = process.readAll();
    int idx_begin = data.indexOf(KEY_BEGIN);
    if (idx_begin > 0) {
        int idx_end = data.indexOf(KEY_END, idx_begin);
        int key_len = KEY_BEGIN.length();
        QString addr = data.mid(idx_begin + key_len, idx_end - idx_begin - key_len);
        return addr;
    } else {
        return "";
    }
}

QString getDefaultGateway()
{
    QProcess proc;
    proc.start("route -n");
    proc.setReadChannel(QProcess::StandardOutput);
    bool is_finished = proc.waitForFinished();
    if (is_finished) {
        if (QProcess::NormalExit == proc.exitStatus()) {
            if (EXIT_SUCCESS == proc.exitCode()) {
                QString output = QString::fromUtf8(proc.readAll());
                QStringList lines = output.split("\n");
                /* 数据样例：
Kernel IP routing table
Destination     Gateway         Genmask         Flags Metric Ref    Use Iface
0.0.0.0         192.168.10.1    0.0.0.0         UG    302    0        0 wlan0
192.168.10.0    0.0.0.0         255.255.255.0   U     302    0        0 wlan0
                 */
                if (lines.size() >= 3) {
                    QString line_str = lines[2];        // 取第 3 行
                    int old_len, new_len;
                    int count = line_str.count(' ');
                    for (int i = 0; i < count; i++) {   // 去掉多余的空格
                        old_len = line_str.length();
                        line_str.replace("  ", " ");
                        new_len = line_str.length();
                        if (new_len == old_len) {
                            break;
                        }
                    }
                    lines = line_str.split(' ');        // 根据空格分割
                    if (lines.size() > 2) {
                        QString gateway = lines[1];     // 取第 2 行
                        return gateway;
                    } else {
                        qDebug() << __PRETTY_FUNCTION__ << ": process output rows less then 3 ";
                        return "";
                    }
                } else {
                    qDebug() << __PRETTY_FUNCTION__ << ": process output rows less then 3 ";
                    return "";
                }
            } else {
                qDebug() << __PRETTY_FUNCTION__ << ": process exit code not success";
                return "";
            }
        } else {
            qDebug() << __PRETTY_FUNCTION__ << ": process not exit normally";
            return "";
        }
    } else {
        qDebug() << __PRETTY_FUNCTION__ << ": process can't finish";
        return "";
    }
}

bool isLanIP(QString _ip)
{
    /*
    // 10.x.x.x 网段
    QRegularExpression re_10("^10(?:(?:\\.1[0-9][0-9])|(?:\\.2[0-4][0-9])|(?:\\.25[0-5])|(?:\\.[1-9][0-9])|(?:\\.[0-9])){3}$");

    // 172.16.0.0—172.31.255.254 网段
    QRegularExpression re_172("^172(?:\\.(?:1[6-9])|(?:2[0-9])|(?:3[0-1]))(?:(?:\\.1[0-9][0-9])|(?:\\.2[0-4][0-9])|(?:\\.25[0-5])|(?:\\.[1-9][0-9])|(?:\\.[0-9])){2}$");

    // 192.168.x.x 网段
    QRegularExpression re_192("^192\\.168(?:(?:\\.1[0-9][0-9])|(?:\\.2[0-4][0-9])|(?:\\.25[0-5])|(?:\\.[1-9][0-9])|(?:\\.[0-9])){2}$");
    */

    // 局域网 IP 地址包含 3 段： “192.168.*.*”，“10.*.*.*”，“172.16.*.* ~ 172.31.*.*”
    QRegularExpression regex("(192\\.168\\.[0-9]{1,3}\\.[0-9]{1,3})|(10\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})|(172\\.(1[6-9]|2[0-9]|3[0-1])\\.[0-9]{1,3}\\.[0-9]{1,3})");
    return regex.match(_ip).hasMatch();
}

bool isAccessToInternet()
{
    /* 实现方法：设定一个能长期稳定 ping 的 IP 或域名列表，若随机两个都 ping 失败，则判定为无法访问互联网，若有一个 ping 成功，则判定为能访问互联网。
     * 中国电信的公共 DNS 服务器 IP 地址：101.226.4.6
     * 百度（baidu.com）、阿里巴巴（alibaba.com）、腾讯（qq.com）、网易（163.com）
     * 阿里云（aliyun.com）、腾讯云（cloud.tencent.com）、华为云（huaweicloud.com）
     */


    // TODO:
    return false;
}

bool sendHttpRequest(const QUrl &_url, const bool _is_post, const QByteArray *_post_data, const QString *_post_content_type,
                                  QByteArray * const _reply_data, QString * const _reply_content_type, int *_http_reply_stat,
                                  QString * const _err_msg, QNetworkAccessManager *_net_access_mgr)
{
    logDebug(QString("%1: entered ... url = \"%2\"").arg(__PRETTY_FUNCTION__).arg(_url.toString()));

    //
    if (_err_msg) {
        _err_msg->clear();
    }

    //
    if (!_url.isValid()) {
        if (_err_msg) {
            *_err_msg = "ParamError: URL is not valid!";
        }
        return false;
    }

    //
    if (!_net_access_mgr) {
        if (!g_netManager) {
            logWarning(QString("%1: QNetworkAccessManager not been setted! Created internal!").arg(__PRETTY_FUNCTION__));
            g_netManager = new QNetworkAccessManager();
        }
        _net_access_mgr = g_netManager;
    }

    //
    QNetworkReply::NetworkError err_reply = (QNetworkReply::NetworkError)(-100);    // 错误码初始值（初始化为小于 0 的未有规定其意义的任意值）
    int http_stat = -1;                                                             // http 状态码（初始化为小于 0 的未有规定其意义的任意值）

    QNetworkReply *reply = nullptr;

    //
    QNetworkRequest request(_url);

    // 发送请求
    if (!_is_post) {
        reply = _net_access_mgr->get(request);
    } else {
        if (_post_data && !_post_data->isEmpty()) {
            if (_post_content_type && !_post_content_type->isEmpty()) {
                request.setHeader(QNetworkRequest::ContentTypeHeader, *_post_content_type);
            }
            reply = _net_access_mgr->post(request, *_post_data);
        } else {
            if (_err_msg) {
                *_err_msg = "ParamError: data to be post is empty!";
            }
            return false;
        }
    }

    //
    QElapsedTimer elapsed_timer;                                 // 超时的计时
    elapsed_timer.start();
    QObject::connect(reply, &QNetworkReply::uploadProgress, [&](qint64 _bytes_sent, qint64 _bytes_total) {     // 通过上传进度信号检测超时
        Q_UNUSED(_bytes_total)
        if (_bytes_sent > 0) {                                  // 如果有数据传输，重置计时器
            elapsed_timer.restart();
        }
    });

    // 创建事件循环
    QEventLoop event_loop;
    QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);

    // 通过定时器定时检查是否超时
    bool is_timeout = false;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&elapsed_timer, &is_timeout, &event_loop] () {
        // 检查是否超时
        static constexpr int TIMEOUT_RESPONSE = 20 * 1000;              // 超时时长（毫秒），指连续无数据传输的时长
        if (elapsed_timer.elapsed() > TIMEOUT_RESPONSE) {
            is_timeout = true;
            event_loop.quit();
        }
    });
    timer.start(1000);

    // 启动事件循环来阻塞等待应答的结束
    event_loop.exec(QEventLoop::ExcludeUserInputEvents);

    //
    do {
        // 若超时，中断请求
        if (is_timeout) {
            // abort() 后无法再读取，所以先读取
            if (_reply_data) {
                *_reply_data = reply->readAll();
            }

            //
            reply->abort();

            //
            err_reply = (QNetworkReply::NetworkError)(-1);
            *_err_msg = "http request timeout";
            logCritical(QString("%1: ").arg(__PRETTY_FUNCTION__) + *_err_msg);
        } else {
            // 读取 http 应答数据
            if (reply->isOpen()) {
                if (_reply_data) {
                    *_reply_data = reply->readAll();
                }
            } else {
                *_err_msg = "request connection is closed unexpected!";
                logCritical(QString("%1: ").arg(__PRETTY_FUNCTION__) + *_err_msg);
            }

            // http 应答状态
            err_reply = reply->error();
            if (QNetworkReply::NoError == err_reply) {
                // http 内容类型
                if (_reply_content_type) {
                    *_reply_content_type = reply->header(QNetworkRequest::ContentTypeHeader).toString();
                }

                // http 状态码
                http_stat = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            } else {
                //
                *_err_msg = "http reply error: " + QVariant::fromValue(err_reply).toString();
                logCritical(QString("%1: ").arg(__PRETTY_FUNCTION__) + *_err_msg);
            }
        }

        // for test
        static bool is_test = false;
        if (is_test) {
            QFile file(qApp->applicationDirPath() + QDir::separator() + "http-reply-data");
            bool succ_open_file = file.open(QFile::OpenModeFlag::WriteOnly | QFile::OpenModeFlag::Truncate);
            if (succ_open_file) {
                file.write(*_reply_data);
                file.flush();
                file.close();
            }
        }

        //
        if (_http_reply_stat) {
            *_http_reply_stat = http_stat;
        }
    } while (false);

    //
    if (reply) {
        reply->deleteLater();       // NOTE: QNetworkReply 须由调用者释放
    }

    //
    bool is_succ = (QNetworkReply::NetworkError::NoError == err_reply);     // NOTE: 返回值的定义是“通信是否正常”，而不是是否有错

    //
    return is_succ;
}

}   // namespace Net
}   // namespace Common
