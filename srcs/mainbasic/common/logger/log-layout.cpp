#include "log-layout.h"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

#include <QMutex>
#include <QDateTime>

//
CLogLayout::CLogLayout(QObject *_parent) : QObject(_parent)
{

}

QString CLogTextLayout::formatLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag, const QDateTime * const _date_time)
{
    //
    QString msg_formatted;      // 格式化好的日志消息

    // 时间
    if (_date_time) {
        msg_formatted += (*_date_time).toString("hh:mm:ss.zzz");
    }

    //{
    //    auto now = std::chrono::system_clock::now();
    //    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    //
    //    std::time_t time = std::chrono::system_clock::to_time_t(now);
    //    char time_str[24];
    //    //std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&time));      /* 19 bytes */
    //    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", std::localtime(&time));       /* 8 bytes */
    //
    //    std::ostringstream oss;
    //    oss << time_str << '.' << std::setw(3) << std::setfill('0') << ms.count();    // 加上毫秒数
    //
    //    msg_formatted = msg_formatted + oss.str().c_str();
    //}

    // 日志等级
    QString type_str = "";
    switch(_log_level)
    {
    case logLevel_Debug:
        type_str += "[D]";
        break;
    case logLevel_Info:
        type_str += "[I]";
        break;
    case logLevel_Warning:
        type_str += "[W]";
        break;
    case logLevel_Critical:
        type_str += "[C]";
        break;
    case logLevel_Fatal:
        type_str += "[F]";
        break;
    default:
        type_str += "[U]";      /* U == "unknown" */
        break;
    }
    msg_formatted = msg_formatted + " " + type_str;

    // 日志标签
    if (_tag) {
        int len_tag = strlen(_tag);
        if (len_tag > LOG_TAG_LEN_MAX) {
            len_tag = LOG_TAG_LEN_MAX;
        }
        QString tag_str = QString::fromLatin1(_tag, len_tag);
        if (tag_str.length() > 0) {
            tag_str.prepend('[');
            tag_str.append(']');
        }
        msg_formatted = msg_formatted + " " + tag_str;
    }

    // 日志消息
    msg_formatted = msg_formatted + " " + _msg;

    //
    return msg_formatted;
}
