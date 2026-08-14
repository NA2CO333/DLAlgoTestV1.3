#include "logger.h"

#include <iostream>

#include <QDir>
#include <QMutex>
#include <QDateTime>
#include <QApplication>

#include "log-appender.h"

//
CLogger::CLogger()
{
    //
    appenderList = new QVector<CLogAppender *>();

}

CLogger::~CLogger()
{
    // 释放 Appender
    for (int i = 0; i < appenderList->size(); i++) {
        CLogAppender *appender = appenderList->at(i);
        delete appender;
    }
    appenderList->clear();

    //
    delete appenderList;
    appenderList = nullptr;

}

bool CLogger::getIsEnabled()
{
    return isEnabled;
}

void CLogger::setIsEnabled(bool _is_enabled)
{
    isEnabled = _is_enabled;

    //
    setQDebugHandler(_is_enabled);
}

enLogLevel CLogger::getLogLevel()
{
    return logLevel;
}

void CLogger::setLogLevel(enLogLevel _log_level)
{
    logLevel = _log_level;
}

void CLogger::addAppender(CLogAppender *_appender)
{
    bool is_repeat = false;
    for (int i = 0; i < appenderList->size(); i++) {
        const CLogAppender *appender = appenderList->at(i);
        if (appender == _appender) {
            is_repeat = true;
            break;
        }
    }
    if (!is_repeat) {
        appenderList->append(_appender);
    }
}

bool CLogger::removeAppender(CLogAppender *_appender)
{
    int idx = -1;
    for (int i = 0; i < appenderList->size(); i++) {
        const CLogAppender *appender = appenderList->at(i);
        if (appender == _appender) {
            idx = i;
            break;
        }
    }
    if (idx >= 0) {
        appenderList->removeAt(idx);
        return true;
    }
    return (idx >= 0);
}

const QVector<CLogAppender *> *CLogger::getAppenderList()
{
    return appenderList;
}

CLogAppender *CLogger::getAppender(const QString &_name)
{
    CLogAppender *appender = nullptr;
    for (int i = appenderList->size() - 1; i >= 0; i--) {
        if (appenderList->at(i)->objectName() == _name) {       // TODO: 如果是多个 File Appender 的情况？
            appender = appenderList->at(i);
            break;
        }
    }
    return appender;
}

void CLogger::addFilterTag(QString _filter)
{
    // 若标签名包含下划线，则替换为横杆（因为log文件名可能用到标签名，而且log文件名需依赖下划线分隔文件名的各个部分）
    if (_filter.contains("_")) {
        _filter.replace("_", "-");
    }

    //
    if ((_filter.length() > 0) && (!filterTags.contains(_filter))) {
        filterTags.append(_filter);
    }
}

const QStringList &CLogger::getFilterTags()
{
    return filterTags;
}

void CLogger::clearFilterTags()
{
    filterTags.clear();
}

bool CLogger::isFiltered(char const *_tag, const enLogLevel &_log_level)
{
    if (_log_level >= logLevel_Critical) {          // 日志类型大于等于“严重”的，不可过滤掉
        return false;
    }
    if (filterTags.size() == 0) {                   // 若过滤标签列表为空，则表示无需过滤
        return false;
    } else {
        if ((!_tag) || (strlen(_tag) == 0)) {       // 若需要过滤，且传入的标签为空，则判断为不存在于过滤标签列表中，需过滤掉（所以，不支持 0 长度的过滤标签）
            return true;
        } else {
            bool is_contains = filterTags.contains(QString::fromLatin1(_tag));
            return (!is_contains);
        }
    }
}

void CLogger::log_out(const enLogLevel &_log_level, const QString &_msg, const char * const _tag)
{
    if (!isEnabled) {
        return;
    }

    // 类型过滤
    if (_log_level < logLevel) {
        return;
    }

    // 标签过滤
    if (isFiltered(_tag, _log_level)) {
        return;
    }

    // 输出到各个目标
    for (int i = 0; i < appenderList->size(); i++) {
        appenderList->at(i)->appendLog(_log_level, _msg, _tag);
    }

}

void CLogger::debug(const QString &_msg, const char * const _tag)
{
    log_out(logLevel_Debug, _msg, _tag);
}

void CLogger::info(const QString &_msg, const char * const _tag)
{
    log_out(logLevel_Info, _msg, _tag);
}

void CLogger::warning(const QString &_msg, const char * const _tag)
{
    log_out(logLevel_Warning, _msg, _tag);
}

void CLogger::critical(const QString &_msg, const char * const _tag)       // TODO: 严重错误，记录到另外的文件？
{
    log_out(logLevel_Critical, _msg, _tag);
}

void CLogger::fatal(const QString &_msg, const char * const _tag)
{
    log_out(logLevel_Fatal, _msg, _tag);
}

void CLogger::qDebugMsgHandler(QtMsgType _type, const QMessageLogContext &_context, const QString &_msg)
{
    Q_UNUSED(_context)

    switch(_type)
    {
    case QtDebugMsg:
        logDebug(_msg);
        //
        break;
    case QtInfoMsg:
        logInfo(_msg);
        //
        break;
    case QtWarningMsg:
        logWarning(_msg);
        //
        break;
    case QtCriticalMsg:
        logCritical(_msg);
        //
        break;
    case QtFatalMsg:
        logFatal(_msg);
        //
        break;
    default:
        logDebug(_msg);
        //
        break;
    }
}

void CLogger::setQDebugHandler(bool _is_install)
{
    if (_is_install) {
        qInstallMessageHandler(qDebugMsgHandler);
    } else {
        qInstallMessageHandler(0);
    }
}

/// ================================================================================================================
/// class CLoggerHelper
///

QVector<CLogAppender *> *CLoggerHelper::disabledAppenders = nullptr;

void CLoggerHelper::setFileAppenderEnabled(bool _enabled)
{
    if (!disabledAppenders) {
        disabledAppenders = new QVector<CLogAppender *>;
    }

    if (_enabled) {
        if (disabledAppenders->size() > 0) {
            CLogFileAppender *file_appender = dynamic_cast<CLogFileAppender *>(disabledAppenders->at(0));   // TODO: 如果是多个 File Appender 的情况？
            if (file_appender) {
                logger()->addAppender(file_appender);
                disabledAppenders->removeAt(0);
            } else {
                std::cout << __PRETTY_FUNCTION__ << ": no file appender disabled!" << std::endl;
            }
        } else {
            std::cout << __PRETTY_FUNCTION__ << ": no appender disabled!" << std::endl;
        }
    } else {
        CLogFileAppender *file_appender = dynamic_cast<CLogFileAppender *>(logger()->getAppender(QString(LOG_APPENDER_CLASS_NAME__FILE)));
        if (file_appender) {
            logger()->removeAppender(file_appender);
            disabledAppenders->append(file_appender);
        } else {
            std::cout << __PRETTY_FUNCTION__ << ": file appender not found!" << std::endl;
        }
    }
}

void CLoggerHelper::releaseDisabledAppenders()
{
    if (disabledAppenders) {
        for (int i = 0; i < disabledAppenders->size(); i++) {
            CLogAppender *appender = disabledAppenders->at(i);
            delete appender;
        }
        disabledAppenders->clear();

        delete disabledAppenders;
        disabledAppenders = nullptr;
    }
}

int CLoggerHelper::copyLogToDir(QString _dest_dir, int *_count_fail)
{
    _count_fail = 0;

    // 检查确保目标文件夹存在
    if (!QFile::exists(_dest_dir)) {
        QDir dir;
        bool succ_mkpath = dir.mkpath(_dest_dir);
        if (!succ_mkpath) {
            std::cout << __PRETTY_FUNCTION__ << ": make dest path failed!" << std::endl;
            return -1;
        }
    }

    // 得到 log 文件的根文件夹
    QString src_dir = "";
    CLogFileAppender *file_appender = dynamic_cast<CLogFileAppender *>(logger()->getAppender(QString(LOG_APPENDER_CLASS_NAME__FILE)));
    if (file_appender) {
        src_dir = file_appender->getRootDirPath();
    } else {
        std::cout << __PRETTY_FUNCTION__ << ": get file_appender failed!" << std::endl;
        return -1;
    }

    // 若获取 log 根文件夹失败，则拷贝失败
    if (src_dir.length() == 0) {
        std::cout << __PRETTY_FUNCTION__ << ": get log root path failed!" << std::endl;
        return -1;
    }

    // 文件遍历及拷贝
    int count_succ = 0;
    int count_fail = 0;
    QDir dir(src_dir);
    QFileInfoList file_list = dir.entryInfoList(QDir::Files);
    for (int i = 0; i < file_list.length(); i++) {
        QFileInfo file_info = file_list.at(i);
        if (file_info.isFile()) {
            QString file_name = file_info.fileName();
            QString src_path = src_dir + "/" + file_name;
            QString dest_path = _dest_dir + "/" + file_name;
            if (QFile::exists(src_path)){
                if (QFile::copy(src_path, dest_path)){
                    count_succ++;
                } else {
                    count_fail++;
                }
            }
        }
    }

    // 输出失败文件个数
    if (_count_fail) {
        *_count_fail = count_fail;
    }

    // 返回成功文件个数
    return count_succ;
}

/// ================================================================================================================
/// 全局对象
///

CLogger *loggerPtr = nullptr;

CLogger *logger()
{
    if (!loggerPtr) {
        loggerPtr = new CLogger();
    }
    return loggerPtr;
}

void releaseLogger()
{
    if (loggerPtr) {
        // 反注册 QDebug 的消息处理函数
        loggerPtr->setQDebugHandler(false);

        //
        delete loggerPtr;
        loggerPtr = nullptr;

        //
        CLoggerHelper::releaseDisabledAppenders();

    }
}
