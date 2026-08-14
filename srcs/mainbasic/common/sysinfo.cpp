#include "sysinfo.h"

#include <QProcess>
#include <QDateTime>

#include "util-common.h"

// TODO: 注意：这个文件放在根目录时只能读，写入时失败？
#define FILE_PATH_SYS_INFO "/sys-info.ini"

//
CSysInfo *CSysInfo::instance = Q_NULLPTR;

//
CSysInfo::CSysInfo()
{
    iniConfig = new QSettings(FILE_PATH_SYS_INFO, QSettings::IniFormat);
    //mutex = new QMutex;
}

CSysInfo::~CSysInfo()
{

}

CSysInfo *CSysInfo::getInstance()
{
    if (!instance) {
        instance = new CSysInfo;
    }
    return instance;
}

QString CSysInfo::firmwareVersion()
{
    //getInstance()->mutex->lock();
    QString val = getInstance()->iniConfig->value("/system/firmwareVersion", "").toString();
    //getInstance()->mutex->unlock();
    return val;
}

QString CSysInfo::rootfsVersion()
{
    //getInstance()->mutex->lock();
    QString val = getInstance()->iniConfig->value("/system/rootfsVersion", "").toString();
    //getInstance()->mutex->unlock();
    return val;
}

QString CSysInfo::getAppVerOrigin()
{
    //getInstance()->mutex->lock();
    QString val = getInstance()->iniConfig->value("/system/mainVersion", "").toString();
    //getInstance()->mutex->unlock();
    return val;
}

QString CSysInfo::osVersion()   // TODO: 去掉后面的日期？uname -r 即可？uname -rv ？检查对比几个固件版本信息
{
    QProcess p;
    p.start("uname -a");
    p.waitForFinished(10000);
    p.setReadChannel(QProcess::StandardOutput);
    QString out_str = p.readAllStandardOutput();
    QStringList list_str;
    Util::splitStrToFields(out_str, list_str, ' ');     /* eg. "Linux RK356X 4.19.219 #578 SMP Thu Apr 13 10:35:38 CST 2023 aarch64 GNU/Linux" */
    QString ver_str;
    if (list_str.size() > 10) {
        QString date_str = /*list_str[5] + " " +*/ list_str[6] + " " + list_str[7] + " " + list_str[8] /*+ " " + list_str[9]*/ + " " + list_str[10];
        QDateTime date_time = QLocale(QLocale::English).toDateTime(date_str, "MMM d HH:mm:ss yyyy");
        ver_str = list_str[2] + " " + list_str[3] + " " + date_time.toString("yyyy-MM-dd");
    }
    return ver_str;
}

bool CSysInfo::cups()
{
    //getInstance()->mutex->lock();
    bool val = getInstance()->iniConfig->value("/system/cups", false).toBool();
    //getInstance()->mutex->unlock();
    return val;

    // TODO: 改为通过系统文件的某些特征来判断？比如是否存在“/etc/ppd/*.ppd”，这样更直接。

}
