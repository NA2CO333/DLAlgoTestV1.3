#ifndef LOGGER_H
#define LOGGER_H

/* 日志模块：
 * 1、提供日志写入接口。
 * 2、须确保线程安全。
 * 3、可支持多种的日志输出目标（Appender）：如 Console、文件，等。其中，日志文件的自动清理，由相应的 Appender 在其内部实现。
 * 4、可支持多种的日志布局(Layout，即格式)：如 纯文本，html，等。
 * 5、可将 QDEBUG 模块的输出转接入本模块。（这样之后，应用程序代码中就只有 std::cout 和 printf() 等语句的log输出未接入本模块。因此对于旧代码，需将其查找替换为由本 log 模块 或 QDebug() 输出。）
 */

// 模块版本号
#define LOG_MODULE_VER  1.0.0
// 最后编辑日期
#define LOG_LAST_EDIT   "2023-10-11"

//
// TODO: 本模块的类型放到独立的命名空间 “log”

//
#include <QString>
#include <QMutex>
#include <QList>

// TODO: 命名空间（待规划）

// 类型预声明
/* 本类是模块的最外层接口类，而内部的 Appender 和 Layout 类都引用了本头文件，所以本头文件不能引用模块内其它类的头文件，须采用预声明。 */
class CLogAppender;
class CLogLayout;

// log 等级
enum enLogLevel {
    logLevel_Debug = 0,                     // 调试（用于辅助调试）
    logLevel_Info,                          // 信息（有助于系统监控、故障分析的重要信息）
    logLevel_Warning,                       // 警告（与预期不一致，有导致故障的可能性，但程序仍能基本正常工作；边沿功能的异常；等）
    logLevel_Critical,                      // 严重（核心功能部分丧失；次要功能完全丧失；等）
    logLevel_Fatal,                         // 致命（程序崩溃；核心功能完全丧失；等）

    logLevel_Min = logLevel_Debug,
    logLevel_Max = logLevel_Fatal,
};

// log tag 的最大长度
#define LOG_TAG_LEN_MAX 16

// 日志接口类
class CLogger           // TODO: 需要获得程序崩溃前的所有 log，需要独立进程？
{
public:
    explicit CLogger();
    ~CLogger();

    void setIsEnabled(bool _is_enabled);                                // 设置本模块是否启用
    bool getIsEnabled();

    void setLogLevel(enLogLevel _log_level);                            // 设置 日志等级（即低于此等级的日志消息将被忽略）     // TODO: 不同的 输出目标 可独立设置日志等级？
    enLogLevel getLogLevel();                                           // 获取 日志等级

    void addAppender(CLogAppender *_appender);                          // 添加 Appender
    bool removeAppender(CLogAppender *_appender);                       // 移除 Appender（未释放被移除的 Appender，需由调用方释放）
    const QVector<CLogAppender *> *getAppenderList();                   // 获得 Appender 列表
    CLogAppender *getAppender(const QString &_name);                    // 获得指定 名称 的 Appender

    void addFilterTag(QString _tag);                                    // 增加一个过滤标签（若添加过，则只有添加了的标签所对应的日志才会被保留，其余的日志会被丢弃）
    const QStringList &getFilterTags();                                 // 获取 过滤标签列表
    void clearFilterTags();                                             // 清除 过滤标签列表

    void setQDebugHandler(bool _is_install);                            // 设置 QDebug 的处理函数  /* 注意：启用后，log 模块内部不可再调用 qDebug()，否则会形成死循环。 */

    // TODO: registerTag(const char *_tag);     // 注册 log 标签（使 log 模块知道将被使用的标签有哪些）

    /* 注意: 建议不要直接引用这几个函数，而是使用本头文件下方定义的宏函数，这样在需要重构时不需修改所有文件。 */
    void debug(const QString &_msg, const char * const _tag = NULL);
    void info(const QString &_msg, const char * const _tag = NULL);
    void warning(const QString &_msg, const char * const _tag = NULL);
    void critical(const QString &_msg, const char * const _tag = NULL);
    void fatal(const QString &_msg, const char * const _tag = NULL);

protected:
    bool isEnabled = true;
    enLogLevel logLevel = logLevel_Warning;
    QVector<CLogAppender *> *appenderList = nullptr;
    QStringList filterTags;                                 // 过滤标签列表

    void log_out(const enLogLevel &_log_level, const QString &_msg, const char * const _tag = NULL);

    bool isFiltered(const char *_tag, const enLogLevel &_log_level);    // 判断指定标签是否被过滤掉

    static void qDebugMsgHandler(QtMsgType _type, const QMessageLogContext &_context, const QString &_msg);     // QDebug 的消息处理函数

};

// log 模块的辅助类
class CLoggerHelper
{
public:

    static void setFileAppenderEnabled(bool _enabled);
    static void releaseDisabledAppenders();

    /**
     * @brief 拷贝 log 文件到指定目录
     * @param _dest_dir     文件拷贝的目标文件夹的绝对路径
     * @param _count_fail   【输出参数】拷贝时出错的文件个数
     * @return 返回成功拷贝的文件个数，返回 -1 表示拷贝失败
     */
    static int copyLogToDir(QString _dest_dir, int* _count_fail);

protected:
    static QVector<CLogAppender *> *disabledAppenders;

};

/// ================================================================================================================
/// 全局对象
///

// 获取全局 log 单例对象，若未创建，则自动创建
CLogger *logger();
// 释放全局 log 单例对象
void releaseLogger();

// 日志输出宏
/* 注意：
 * 参数 _tag 的类型为 const char * 类型，，且长度不可超过 LOG_TAG_LEN_MAX。建议传入的 tag 是全局的常量。
 */
#define logDebug(msg, ...)      logger()->debug(msg, ##__VA_ARGS__)
#define logInfo(msg, ...)       logger()->info(msg, ##__VA_ARGS__)
#define logWarning(msg, ...)    logger()->warning(msg, ##__VA_ARGS__)
#define logCritical(msg, ...)   logger()->critical(msg, ##__VA_ARGS__)
#define logFatal(msg, ...)      logger()->fatal(msg, ##__VA_ARGS__)

// 日志输出宏（扩展，增加了当前类名和函数名的输出）
#define logWarningEx(msg, ...)  { QString s = QString() + __PRETTY_FUNCTION__ + ": " + msg; logger()->warning(s, ##__VA_ARGS__); }
#define logCriticalEx(msg, ...) { QString s = QString() + __PRETTY_FUNCTION__ + ": " + msg; logger()->critical(s, ##__VA_ARGS__); }
#define logFatalEx(msg, ...)    { QString s = QString() + __PRETTY_FUNCTION__ + ": " + msg; logger()->fatal(s, ##__VA_ARGS__); }

#endif // LOGGER_H
