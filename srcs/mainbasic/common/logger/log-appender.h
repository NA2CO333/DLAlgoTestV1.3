#ifndef CLOGAPPENDER_H
#define CLOGAPPENDER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QDateTime>

#include "logger.h"

// 类型预声明
class CLogTextLayout;

///=============================================================================================================
/// class CLogAppender

// 日志添加器（虚类，负责将日志输出到不同的目标，如：控制台、文件、socket、等，不同的类对应不同的输出目标。）     /* 注意：这是线程同步的，若要异步，须用 CLogAsyncAppender */
class CLogAppender : public QObject
{
    Q_OBJECT
public:
    explicit CLogAppender(QObject *_parent = nullptr);

    // 追加 log 消息
    virtual void appendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag) = 0;

};

///=============================================================================================================
/// class CLogAsyncAppender

// 日志添加器（虚类）：实现异步添加
class CLogAsyncAppender : public CLogAppender
{
    Q_OBJECT
public:
    explicit CLogAsyncAppender(QObject *_parent = nullptr);
    ~CLogAsyncAppender();

    void appendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag) override;

Q_SIGNALS:
    void sigGotMsg();       // 信号：得到 log 消息

protected Q_SLOTS:
    void slot_this_GotMsg();

protected:
    // log 消息
    struct stLogMsg {
        enLogLevel logLevel;
        QString msg;
        const char *tag;        /* 注意：tag 的长度将被限制为 LOG_TAG_LEN_MAX。 */
    };

    QThread *thread = nullptr;
    QMutex *m_mutexBuffer = nullptr;
    QList<stLogMsg> *m_buffer = nullptr;

    void pushBuffer(const enLogLevel &_log_level, const QString &_msg, const char * const _tag);    // 添加 log 到缓冲区
    int popBuffer(enLogLevel &_log_level, QString &_msg, const char *&_tag);                        // 从缓冲区移出 log，返回缓冲区的当前尺寸
    void processBuff();                                                                             // 处理缓冲区数据

    virtual void doAppendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag) = 0;

};

///=============================================================================================================
/// class CLogConsoleAppender

// Appender 缺省对象名（可通过对象名查找该对象）：输出到 控制台
#define LOG_APPENDER_CLASS_NAME__CONSOLE  "CLogConsoleAppender"

// 日志添加器：输出到控制台         // NOTE: 在嵌入式Linux平台，输出到控制台操作需要等待IO队列，高频执行时会显著降低调用线程的实时性，因此需异步操作
class CLogConsoleAppender : public CLogAppender  // TODO: 还要清理 std::cout 和 std::cerr 操作，避免 log 顺序错位？
{
    Q_OBJECT
public:
    explicit CLogConsoleAppender(QObject *_parent = nullptr);

    void appendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag) override;

protected:
    //void doAppendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag) override;

    CLogTextLayout *m_logLayout = nullptr;

};

///=============================================================================================================
/// class CLogFileAppender

// Appender 缺省对象名（可通过对象名查找该对象）：输出到 文件
#define LOG_APPENDER_CLASS_NAME__FILE   "CLogFileAppender"

// 缺省的日志文件保存目录名称
#define LOG_DIR_NAME    "log"

// 缺省的日志文件保留天数
#define LOG_DEFAULT_FILE_KEEP_DAYS      7

// 缺省的最小系统可用空间(MB)
#define LOG_DEFAULT_MIN_AVAIL_SPACE     3072

// 日志添加器：输出到 文件
class CLogFileAppender : public CLogAsyncAppender
{
    Q_OBJECT
public:
    explicit CLogFileAppender(QObject *_parent = nullptr);
    ~CLogFileAppender();

    void setRootDirPath(const QString &_dir_path);                                  // 设置文件的根目录绝对路径
    QString getRootDirPath();

    void setFileKeepDays(int _keep_days);                                           // 设置文件保留天数
    void setMinAvailSpace(int _min_space);                                          // 设置最小可用空间（系统可用空间低于该值时仅保留当天日志）

    void AddSeparatedTag(const char * _tag);                                        // 添加独立标签（此标签的消息将被保存到独立文件）

    void setOneFilePerDay(bool _yes_or_no);                                         // 设置是否每天一个文件（否则每小时一个文件）

protected:
    /* log 文件的相关信息 */
    struct stLogFileInfo {
        QFile *file = nullptr;                  // 文件对象
        QTextStream *stream = nullptr;          // 文件对应的文本流
        QString filePath;                       // 文件路径
        QDateTime fileTime;                     // 文件的时间（与确定文件名所用的时间一致）
    };

    CLogTextLayout *m_logLayout = nullptr;

    QString logRootDir;
    QVector<const char *> *separatedFileTags = nullptr;

    QMap<QString, stLogFileInfo *> *openedFiles = nullptr;                          // 已打开的文件

    int fileKeepDays = LOG_DEFAULT_FILE_KEEP_DAYS;
    int minAvailSpace = LOG_DEFAULT_MIN_AVAIL_SPACE;

    bool isOneFilePerDay = false;               // 默认每小时一个文件

    void doAppendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag) override;

    stLogFileInfo *getLogFileInfoByTag(const char *_tag, const QDateTime &_date_time);          // 根据 log tag 获取对应的 log file info
    stLogFileInfo *getLogFileInfoByKey(const QString &_file_key, const QDateTime &_date_time);  // 根据 映射的 key 获取对应的 log file info
    QString getLogFileName(const QString &_file_key, const QDateTime &_date_time);              // 根据相关要素构造 log 文件的文件名（用这个函数统一文件名的命名格式）
    QDateTime getLogFileDateFromFilePath(const QString &_file_path);                            // 根据文件路径解析文件日期
    stLogFileInfo *openLogFile(const QString &_file_path, const QDateTime &_date_time);         // 打开新日志文件
    void closeLogFile(stLogFileInfo *_file_info);                                               // 关闭日志文件
    void cleanLogFiles();                                                                       // 清理日志文件

Q_SIGNALS:
    void sigCleanLogFiles();

protected Q_SLOTS:
    void slotCleanLogFiles();

};

///=============================================================================================================
/// class CLogFaultAppender

// Appender 缺省对象名（可通过对象名查找该对象）：故障记录
#define LOG_APPENDER_CLASS_NAME__FAULT   "CLogFaultAppender"

// 日志添加器：输出到 故障记录（仅接收 critical 和 fatal 等最重要的错误）
class CLogFaultAppender : public CLogAppender
{
    Q_OBJECT
public:
    explicit CLogFaultAppender(QObject *_parent = nullptr);
    ~CLogFaultAppender();

    void appendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag) override;

    int getCount();                         // 获取当前接收到的消息的个数
    const QString &getLog(int _idx);        // 获取指定索引号（0~getCount()）的 log 消息

protected:
    const int MAX_COUNT = 512;

    CLogTextLayout *m_logLayout = nullptr;
    QStringList *m_logs = nullptr;
    QMutex *m_mutexLogs = nullptr;

};

#endif // CLOGAPPENDER_H
