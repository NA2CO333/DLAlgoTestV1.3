#include "log-appender.h"

#include <iostream>

#include <QDateTime>
#include <QTimer>
#include <QApplication>
#include <QDir>
#include <QStorageInfo>
#include <QFileInfo>
#include <QStringList>

#include "log-layout.h"

// 日志文件名中的日期时间格式 - 每小时一个文件
#define LOG_FILE_NAME_DATE_FORMAT_PER_HOUR  "yyyyMMdd_hh"

// 日志文件名中的日期时间格式 - 每天一个文件
#define LOG_FILE_NAME_DATE_FORMAT_PER_DAY   "yyyyMMdd"

// 日志文件的扩展名
#define LOG_FILE_EXT_NAME           "txt"

///=============================================================================================================
/// 前置声明

// 判断字符串是否为空
bool isStrEmpty(const char *_str);

// 判断两个字符串是否相等（都为空的时候，判断为不等）
bool isStrsEqual(const char *_str1, const char *_str2);

// 创建文件夹（若不存在时）
void makeDir(const QString &_dir_path);

///=============================================================================================================
/// class CLogAppender

//
CLogAppender::CLogAppender(QObject *_parent) : QObject(_parent)
{

}

///=============================================================================================================
/// class CLogAsyncAppender

CLogAsyncAppender::CLogAsyncAppender(QObject *_parent) : CLogAppender(_parent)
{

    thread = new QThread();
    m_buffer = new QList<stLogMsg>();
    m_mutexBuffer = new QMutex();

    this->moveToThread(thread);
    thread->start();

    QObject::connect(this, &CLogAsyncAppender::sigGotMsg, this, &CLogAsyncAppender::slot_this_GotMsg, Qt::QueuedConnection);

}

CLogAsyncAppender::~CLogAsyncAppender()
{

    thread->exit();
    thread->wait(3000);
    //if (thread->isRunning()) {
    //    thread->terminate();      // TODO: 这个不安全？
    //    thread->wait(3000);
    //}
    //delete thread;
    thread->deleteLater();
    thread = nullptr;

    delete m_buffer;
    m_buffer = nullptr;

    delete m_mutexBuffer;
    m_mutexBuffer = nullptr;

}

void CLogAsyncAppender::appendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag)
{
    //
    pushBuffer(_log_level, _msg, _tag);

    //
    emit sigGotMsg();
}

void CLogAsyncAppender::slot_this_GotMsg()
{
    processBuff();
}

void CLogAsyncAppender::pushBuffer(const enLogLevel &_log_level, const QString &_msg, const char * const _tag)
{
    QMutexLocker locker(m_mutexBuffer);
    try {
        //
        stLogMsg log_msg{_log_level, _msg, _tag};
        m_buffer->append(log_msg);

    } catch (...) {
        // TODO: ？
    }
}

int CLogAsyncAppender::popBuffer(enLogLevel &_log_level, QString &_msg, const char *&_tag)
{
    int buffer_size = -1;

    //
    QMutexLocker locker(m_mutexBuffer);
    try {
        //
        if (m_buffer->size() > 0) {
            stLogMsg log_msg = m_buffer->takeFirst();

            _log_level = log_msg.logLevel;
            _msg = log_msg.msg;
            _tag = log_msg.tag;

            buffer_size = m_buffer->size();
        } else {
            buffer_size = 0;
        }
    } catch (...) {
        // TODO: ？
    }

    //
    return buffer_size;
}

void CLogAsyncAppender::processBuff()
{
    int buffer_size = 0;
    do {
        enLogLevel log_level = logLevel_Min;
        QString msg = "";
        const char *tag = nullptr;

        buffer_size = popBuffer(log_level, msg, tag);

        doAppendLog(log_level, msg, tag);

    } while (buffer_size > 0);
}

///=============================================================================================================
/// class CLogConsoleAppender

CLogConsoleAppender::CLogConsoleAppender(QObject *_parent) : CLogAppender(_parent)
{
    this->setObjectName(LOG_APPENDER_CLASS_NAME__CONSOLE);

    m_logLayout = new CLogTextLayout();

}

void CLogConsoleAppender::appendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag)
{
    // 当前日期时间
    QDateTime *date_time_ptr = nullptr;

    QDateTime date_time = QDateTime::currentDateTime();
    date_time_ptr = &date_time;

    // 日志布局（格式）
    QString msg_formatted = m_logLayout->formatLog(_log_level, _msg, _tag, date_time_ptr);

    //
    switch (_log_level) {
    case logLevel_Debug:
    case logLevel_Info:
    case logLevel_Warning:
        std::cout << msg_formatted.toLocal8Bit().constData() << std::endl;      // TODO: 字符编码是否应该固定为 UTF-8 ？
        break;
    case logLevel_Critical:
    case logLevel_Fatal:
        std::cerr << msg_formatted.toLocal8Bit().constData() << std::endl;
        break;
    default:
        std::cout << msg_formatted.toLocal8Bit().constData() << std::endl;
    }
}

///=============================================================================================================
/// class CLogFileAppender

CLogFileAppender::CLogFileAppender(QObject *_parent) : CLogAsyncAppender(_parent)
{
    this->setObjectName(LOG_APPENDER_CLASS_NAME__FILE);

    m_logLayout = new CLogTextLayout();

    //
    separatedFileTags = new QVector<const char *>();

    openedFiles = new QMap<QString, stLogFileInfo *>();

    // 设置默认日志文件根目录
    logRootDir = qApp->applicationDirPath() + QDir::separator() + LOG_DIR_NAME;

    // 内部信号槽连接
    QObject::connect(this, &CLogFileAppender::sigCleanLogFiles, this, &CLogFileAppender::slotCleanLogFiles, Qt::QueuedConnection);

    // 程序启动时，执行一次日志文件清理
    emit sigCleanLogFiles();

}

CLogFileAppender::~CLogFileAppender()
{
    // 关闭已打开的文家
    for (auto it = openedFiles->begin(); it != openedFiles->end(); it++) {
        stLogFileInfo *file_info = it.value();
        closeLogFile(file_info);
        delete file_info;
        file_info = nullptr;
    }
    openedFiles->clear();

}

void CLogFileAppender::setRootDirPath(const QString &_dir_path)
{
    logRootDir = _dir_path;
}

QString CLogFileAppender::getRootDirPath()
{
    return logRootDir;
}

void CLogFileAppender::AddSeparatedTag(const char *_tag)
{
    // 检查是否有重复
    bool is_repeat = false;
    for (auto tag : *separatedFileTags) {
        if (strcmp(tag, _tag) == 0) {
            is_repeat = true;
            break;
        }
    }

    //
    if (!is_repeat) {
        separatedFileTags->append(_tag);
    }
}

void CLogFileAppender::setOneFilePerDay(bool _yes_or_no)
{
    isOneFilePerDay = _yes_or_no;
}

void CLogFileAppender::doAppendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag)
{
    // 当前日期时间
    QDateTime *date_time_ptr = nullptr;

    QDateTime date_time = QDateTime::currentDateTime();
    date_time_ptr = &date_time;

    // 日志布局（格式）
    QString msg_formatted = m_logLayout->formatLog(_log_level, _msg, _tag, date_time_ptr);

    //
    stLogFileInfo *log_file_info = getLogFileInfoByTag(_tag, date_time);
    if (log_file_info) {
        QTextStream *log_stream = log_file_info->stream;
        if (log_stream) {
            (*log_stream) << msg_formatted << "\n";

            //
            log_stream->flush();        // TODO: 程序崩溃之后，这里往往也会发生异常？

            // TODO: 每次都同步到文件，会否太频繁？每隔一段时间同步一次？但是如果本模块运行不是独立运行的，可能崩溃而导致部分消息未保存？应将日志模块运行在独立的程序中？


        } else {
            std::cout << "Logger model critical error: get log stream failed!" << std::endl;
        }
    } else {
        std::cout << "Logger model critical error: get log file info failed!" << std::endl;
    }
}

CLogFileAppender::stLogFileInfo *CLogFileAppender::getLogFileInfoByTag(const char *_tag, const QDateTime &_date_time)
{
    /* 基本逻辑设计：
     * 1、日志文件的目录结构：因为保留的文件较少，所以全部放到日志文件的根目录里。
     * 2、写入 log 消息时，因为部分 tag 须保存到独立文件，所以会同时打开多个文件。以 tag 作为 key 来映射对应的 log 文件，若非独立保存的文件，则 key 为 null。
     */

    // 判断是否写入独立的文件
    bool is_separate_file = false;
    for (int i = separatedFileTags->size() - 1; i >= 0; i--) {
        const char *tag = separatedFileTags->at(i);
        if (isStrsEqual(tag, _tag)) {
            is_separate_file = true;
            break;
        }
    }

    // 得到映射到对应 log 文件的 key
    QString file_key = (is_separate_file ? QString(_tag) : "");

    //
    stLogFileInfo *file_info = getLogFileInfoByKey(file_key, _date_time);

    //
    return file_info;
}

CLogFileAppender::stLogFileInfo *CLogFileAppender::getLogFileInfoByKey(const QString &_file_key, const QDateTime &_date_time)
{
    //
    stLogFileInfo *file_info = nullptr;

    // 得到 log 文件名
    QString file_path = getRootDirPath() + QDir::separator() + getLogFileName(_file_key, _date_time);

    // 若找不到 log 文件信息，则创建该 key 所对应的文件信息
    if (!openedFiles->contains(_file_key)) {
        // 打开新文件，并添加新文件信息
        file_info = openLogFile(file_path, _date_time);
        if (file_info) {
            openedFiles->insert(_file_key, file_info);
        } else {
            std::cout << "Logger model critical error: open new log file failed(1)!" << std::endl;
            return nullptr;
        }
    } else {
        //
        file_info = openedFiles->value(_file_key);

        // 判断是否需要创建新的文件（比如文件名的时间与当前时间不匹配）
        bool is_need_new_file = (file_path != file_info->filePath);

        // 替换该 key 所对应的文件信息
        if (is_need_new_file) {
            // 关闭旧文件，并移除旧文件信息
            openedFiles->remove(_file_key);

            closeLogFile(file_info);
            delete file_info;
            file_info = nullptr;

            // 打开新文件，并添加新文件信息
            file_info = openLogFile(file_path, _date_time);
            if (file_info) {
                openedFiles->insert(_file_key, file_info);
            } else {
                std::cout << "Logger model critical error: open new log file failed(2)!" << std::endl;
                return nullptr;
            }
        }
    }

    //
    return file_info;
}

QString CLogFileAppender::getLogFileName(const QString &_file_key, const QDateTime &_date_time)
{
    QString date_time_format = (isOneFilePerDay ? LOG_FILE_NAME_DATE_FORMAT_PER_DAY : LOG_FILE_NAME_DATE_FORMAT_PER_HOUR);
    QString file_name = QString("log_%1_%2.%3")
            .arg(_file_key)
            .arg(_date_time.toString(date_time_format))
            .arg(LOG_FILE_EXT_NAME);
    return file_name;
}

QDateTime CLogFileAppender::getLogFileDateFromFilePath(const QString &_file_path)
{
    QDateTime date_time;

    //
    int idx_0 = _file_path.lastIndexOf(QDir::separator());
    int idx_1 = -1, idx_2 = -1, idx_3 = -1;
    idx_1 = _file_path.indexOf("_", idx_0 + 1);
    if (idx_1 >= 0) {
        idx_2 = _file_path.indexOf("_", idx_1 + 1);
    }
    if (idx_2 >= 0) {
        idx_3 = _file_path.indexOf(".", idx_2 + 1);
    }
    if (idx_3 >= 0) {
        QString date_str = _file_path.mid(idx_2 + 1, idx_3 - idx_2 - 1);
        QString date_time_format = (isOneFilePerDay ? LOG_FILE_NAME_DATE_FORMAT_PER_DAY : LOG_FILE_NAME_DATE_FORMAT_PER_HOUR);
        if (!date_str.isEmpty()) {
            date_time = QDateTime::fromString(date_str, date_time_format);
        }
    }

    //
    return date_time;
}

CLogFileAppender::stLogFileInfo *CLogFileAppender::openLogFile(const QString &_file_path, const QDateTime &_date_time)
{
    //
    stLogFileInfo *file_info = nullptr;

    // 确保文件夹存在
    makeDir(logRootDir);

    // 打开新文件
    QFile *file = new QFile(_file_path);
    bool is_succ_open = file->open(QIODevice::WriteOnly | QIODevice::Append);
    if (!is_succ_open) {
        std::cout << __PRETTY_FUNCTION__ << ": open file '" << _file_path.toLocal8Bit().data() << "' failed!" << std::endl;

        // TODO: ？

        //
        return nullptr;
    }

    // 打开新的流
    QTextStream *stream = new QTextStream(file);
    stream->setCodec("UTF-8");

    //
    file_info = new stLogFileInfo();

    file_info->file = file;
    file_info->stream = stream;
    file_info->filePath = _file_path;
    file_info->fileTime = _date_time;

    // 新增日志文件时，执行一次日志文件清理
    emit sigCleanLogFiles();

    //
    return file_info;
}

void CLogFileAppender::closeLogFile(CLogFileAppender::stLogFileInfo *_file_info)
{
    _file_info->stream->flush();
    _file_info->file->flush();
    _file_info->file->close();

    delete _file_info->stream;
    _file_info->stream = nullptr;
    delete _file_info->file;
    _file_info->file = nullptr;

    _file_info->filePath = "";
    //_file_info->fileTime.set;      // TODO: 使日期文件无效？

}

void CLogFileAppender::slotCleanLogFiles()
{
    // TODO: 清理频度限制？记录上次执行时间，若间隔过短，忽略请求？


    //
    cleanLogFiles();
}

void CLogFileAppender::setFileKeepDays(int _keep_days)
{
    fileKeepDays = _keep_days;
}

void CLogFileAppender::setMinAvailSpace(int _min_space)
{
    minAvailSpace = _min_space;
}

void CLogFileAppender::cleanLogFiles()
{
    /* 文件清理的需求：
     * 1、仅保留设定天数的日志文件。
     * 2、最低可用空间低于设定值时，仅保留当天的日志文件。
     */

    //
    static QMutex mutex;

    //
    if (!mutex.tryLock()) {     // 防止重复调用
        std::cout << "Repeated calls cleanLogFiles()!!!" << std::endl;
        return;
    }

    //
    QString root_dir_path = getRootDirPath();
    if (!QFile::exists(root_dir_path)) {
        return;
    }

    //
    bool is_space_enough = true;

    // 得到系统当前可用空间(MB)
    QStorageInfo storage_info;
    storage_info.setPath(root_dir_path);
    storage_info.refresh();
    //int space_free = storage_info.bytesFree() / 1024 / 1024;
    int space_avail = storage_info.bytesAvailable() / 1024 / 1024;

    is_space_enough = (space_avail > minAvailSpace);

    // 得到保留的文件的最小日期
    QDate min_date = (is_space_enough ? QDate::currentDate().addDays(-(fileKeepDays - 1)) : QDate::currentDate());

    // 遍历文件目录，删掉文件日期小于最小日期的文件（以文件名识别，识别失败的也清掉）
    QDir dir(root_dir_path);
    QFileInfoList file_info_list = dir.entryInfoList(QDir::Files);
    QString file_path;
    QDate file_date;
    for (int i = file_info_list.size() - 1; i >= 0; i--) {
        const QFileInfo &file_info = file_info_list.at(i);

        file_path = file_info.absoluteFilePath();
        file_date = getLogFileDateFromFilePath(file_path).date();

        if (file_date.isValid()) {
            if (file_date < min_date) {
                QFile::remove(file_path);
            }
        } else {
            std::cout << "Logger model critical error: getLogFileDateFromFilePath('" << file_path.toLatin1().data() << "') failed!" << std::endl;
            QFile::remove(file_path);
        }
    }

    //
    mutex.unlock();
}

///=============================================================================================================
/// class CLogFaultAppender

CLogFaultAppender::CLogFaultAppender(QObject *_parent) : CLogAppender(_parent)
{
    this->setObjectName(LOG_APPENDER_CLASS_NAME__FAULT);

    m_logLayout = new CLogTextLayout();
    m_logs = new QStringList();
    m_mutexLogs = new QMutex();

}

CLogFaultAppender::~CLogFaultAppender()
{

}

void CLogFaultAppender::appendLog(const enLogLevel &_log_level, const QString &_msg, const char * const _tag)
{
    // 限制 log 级别
    if (_log_level < logLevel_Critical) {
        return;
    }

    // 当前日期时间
    QDateTime *date_time_ptr = nullptr;

    QDateTime date_time = QDateTime::currentDateTime();
    date_time_ptr = &date_time;

    // 日志布局（格式）
    QString msg_formatted = m_logLayout->formatLog(_log_level, _msg, _tag, date_time_ptr);

    //
    QMutexLocker locker(m_mutexLogs);

    // 添加 log
    m_logs->append(msg_formatted);

    // 限制 log 条数
    if (m_logs->size() >= MAX_COUNT) {
        for (int i = m_logs->size() - 1; i >= 0; i--) {
            m_logs->removeFirst();
        }
    }
}

int CLogFaultAppender::getCount()
{
    QMutexLocker locker(m_mutexLogs);
    return m_logs->size();
}

const QString &CLogFaultAppender::getLog(int _idx)
{
    QMutexLocker locker(m_mutexLogs);
    if (_idx >= 0 && _idx < m_logs->size()) {
        return m_logs->at(_idx);
    } else {
        static const QString STR_EMPTY = "";
        return STR_EMPTY;
    }
}

///=============================================================================================================
/// 其它工具函数

bool isStrEmpty(const char *_str)
{
    return ((!_str) || (strlen(_str) == 0));
}

bool isStrsEqual(const char *_str1, const char *_str2)
{
    return (!isStrEmpty(_str1)) && (!isStrEmpty(_str2)) && (0 == strcmp(_str1, _str2));
}

void makeDir(const QString &_dir_path)
{
    if (QFile::exists(_dir_path)) {
        if (!QFileInfo(_dir_path).isDir()) {
            QFile::remove(_dir_path);
        }
    } else {
        QDir log_dir(_dir_path);
        log_dir.mkpath(_dir_path);
    }
}
