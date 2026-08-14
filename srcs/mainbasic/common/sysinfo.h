#ifndef SYSINFO_H
#define SYSINFO_H

#include <QString>
#include <QSettings>
#include <QMutex>

// 系统信息
class CSysInfo
{
public:
    static QString firmwareVersion();                           // 获得 完整固件版本号
    static QString rootfsVersion();                             // 获得 rootfs 镜像版本号
    static QString osVersion();                                 // 获得 操作系统版本
    static bool cups();                                         // 获得 是否支持 CUPS

    static QString getAppVerOrigin();                           // 获得 主程序原始版本号，即固件镜像包中的主程序版本号，并不一定等于当前程序版本号

protected:
    CSysInfo();
    ~CSysInfo();

    static CSysInfo *getInstance();

    static CSysInfo *instance;

    QSettings *iniConfig = Q_NULLPTR;
    //QMutex *mutex = Q_NULLPTR;

};

#endif // SYSINFO_H
