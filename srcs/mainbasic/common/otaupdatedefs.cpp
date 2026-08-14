#include "otaupdatedefs.h"

#include <QProcess>
#include <QSettings>
#include <QDebug>

// 备份文件前缀
static QString g_backupFilePrefix = "DataBackup";
// 语言是否为中文
static bool g_isChinese = true;

//
#define CONFIG_PATH_BACKUP_FILE_PREFIX  "/backup/backupFilePrefix"

//
QSettings *g_OtaUpdateConfig = Q_NULLPTR;
QSettings *getOtaUpdateConfig()
{
    if (!g_OtaUpdateConfig) {
        QString file_name = QString("%1/%2").arg(OTA_APP_DIR).arg(OTA_STAT_FILE_NAME);
        g_OtaUpdateConfig = new QSettings(file_name, QSettings::IniFormat);
        //qDebug() << g_OtaUpdateConfig->fileName();
    }
    return g_OtaUpdateConfig;
}

//
bool OtaUpdate::isNeedOtaRestore()
{
    /* 判断逻辑：
     * ota 升级程序须存在（过滤掉旧固件的情况）；
     * ota 第一次运行标识不存在；
     */
    QString path_app = QString(OTA_APP_DIR) + QDir::separator() + OTA_APP_NAME;
    QString path_tag = QString(OTA_APP_DIR) + QDir::separator() + OTA_STAT_FILE_NAME;
    bool is_first = (QFile::exists(path_app) && !QFile::exists(path_tag));

    //
    return is_first;
}

bool OtaUpdate::runOtaUpdate()
{
    QString path_app = QString(OTA_APP_DIR) + QDir::separator() + OTA_APP_NAME;
    QString cmd = path_app + " " + OTA_PARAM_OTA_UPDATE + (g_isChinese ? QString("") : (QString(" ") + OTA_PARAM_LANGUAGE + "=e")) + " &";
    if (QFile::exists(path_app)) {
        int exit_code = system(cmd.toLatin1().data());
        return (-1 != exit_code && 127 != exit_code);
    } else {
        return false;
    }
}

bool OtaUpdate::runOtaRestore()
{
    QString path_app = QString(OTA_APP_DIR) + QDir::separator() + OTA_APP_NAME;
    QString cmd = path_app + " " + OTA_PARAM_OTA_RESTORE + (g_isChinese ? QString("") : (QString(" ") + OTA_PARAM_LANGUAGE + "=e")) + " &";
    if (QFile::exists(path_app)) {
        int exit_code = system(cmd.toLatin1().data());
        return (-1 != exit_code && 127 != exit_code);
    } else {
        return false;
    }
}

bool OtaUpdate::runUserBackup()
{
    QString path_app = QString(OTA_APP_DIR) + QDir::separator() + OTA_APP_NAME;
    QString cmd = path_app + " " + OTA_PARAM_USER_BACKUP + (g_isChinese ? QString("") : (QString(" ") + OTA_PARAM_LANGUAGE + "=e")) + " &";
    if (QFile::exists(path_app)) {
        int exit_code = system(cmd.toLatin1().data());
        return (-1 != exit_code && 127 != exit_code);
    } else {
        return false;
    }
}

bool OtaUpdate::runUserRestore()
{
    QString path_app = QString(OTA_APP_DIR) + QDir::separator() + OTA_APP_NAME;
    QString cmd = path_app + " " + OTA_PARAM_USER_RESTORE + (g_isChinese ? QString("") : (QString(" ") + OTA_PARAM_LANGUAGE + "=e")) + " &";
    if (QFile::exists(path_app)) {
        int exit_code = system(cmd.toLatin1().data());
        return (-1 != exit_code && 127 != exit_code);
    } else {
        return false;
    }
}

void OtaUpdate::setBackupFilePrefix(const QString &_prefix)
{
    getOtaUpdateConfig()->setValue(CONFIG_PATH_BACKUP_FILE_PREFIX, _prefix);
    getOtaUpdateConfig()->sync();
}

QString OtaUpdate::getBackupFilePrefix()
{
    QString prefix = getOtaUpdateConfig()->value(CONFIG_PATH_BACKUP_FILE_PREFIX).toString();
    if (prefix.length() == 0) {
        prefix = g_backupFilePrefix;
    }
    return prefix;
}

void OtaUpdate::setIsChinese(bool _is_chinese)
{
    g_isChinese = _is_chinese;
}

float OtaUpdate::getDiskRemainSpace(QString _udisk_path)
{
    QString cmd = QString("df -h %1").arg(_udisk_path);

    QProcess proc_df;
    proc_df.start(cmd);
    bool is_succ = proc_df.waitForFinished(-1);
    if (is_succ) {
        QString output = proc_df.readAllStandardOutput();
        int idx = output.lastIndexOf('\n', output.length() - 2);        // 找到最后一行
        if (idx >= 0) {
            QString line_str = output.mid(idx + 1);
            int count = line_str.count(' ');
            int len_old, len_new;
            for (int i = 0; i < count; i++) {               // 循环去掉多余空格，转为只有一个空格分隔的多字段拼接字符串
                len_old = line_str.length();
                len_new = line_str.replace("  ", " ").length();
                if (len_new == len_old) {
                    break;
                }
            }
            QStringList list_field = line_str.split(' ');
            QString field = list_field[3];
            QChar unit = field[field.length() - 1];
            float remain = field.left(field.length() - 1).toDouble();           // 得到剩余空间所在字段
            if (unit.toUpper() == 'M') {
                remain /= 1024;
            }
            return remain;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}

float OtaUpdate::getBackupRequireSpace()
{
    constexpr float SPACE_DISK          = 14.0;         // 磁盘总空间
    constexpr float SPACE_SYS_ORIGIN    = 1.0;          // 系统初始占用空间

    // 得到当前磁盘剩余空间
    float remain = getDiskRemainSpace("/");

    //
    float ret = -1;

    if (remain >= 0) {
        float used = SPACE_DISK - SPACE_SYS_ORIGIN - remain;        // 得到历史数据占用空间
        ret = used * 1.5;
    }

    return ret;
}

void OtaUpdate::markOtaRestoreExecuted()
{
    static const QString path_stat_file = QString(OTA_APP_DIR) + QDir::separator() + OTA_STAT_FILE_NAME;

    if (!QFile::exists(path_stat_file)) {
        QString cmd = QString("echo '' > %1").arg(path_stat_file);
        system(cmd.toLatin1().data());
    }
}
