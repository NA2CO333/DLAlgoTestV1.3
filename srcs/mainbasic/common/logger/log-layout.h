#ifndef CLOGLAYOUT_H
#define CLOGLAYOUT_H

#include <QObject>

#include "logger.h"

// 日志布局类（负责将日志消息的格式化，如：纯文本，html，等。每一种日志输出目标，都可以有自己的输出格式。）
class CLogLayout : public QObject
{
public:
    explicit CLogLayout(QObject *_parent = nullptr);

    // 格式化日志消息
    virtual QString formatLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag, const QDateTime * const _date_time) = 0;

};

// 纯文本日志布局类
class CLogTextLayout : public CLogLayout
{
public:
    QString formatLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag, const QDateTime * const _date_time) override;

};

#endif // CLOGLAYOUT_H
