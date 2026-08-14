#ifndef CHARDWARE_H
#define CHARDWARE_H

#include <QDateTime>

// 对硬件的零散操作的集合
class CHardware
{
public:
    CHardware();

    static void cameraPowerOff();                                   // 相机断电
    static void cameraPowerOn();                                    // 相机上电
    static bool getIsCameraPowerOn();

    static void setMachineDateTime(QDateTime _date_time);           // 设置机器的日期时间（即 设置系统时间 + 时间同步到硬件）

    static void setSysDateTime(QDateTime _date_time);               // 设置系统时间
    static void syncHardwareDateTime(bool _is_sys_to_hw);           // 将系统时间同步到硬件
    static QDateTime getHardwareDateTime();                         // 读取硬件时间（若失败，则 QDateTime is not isValid() ）

protected:
    static bool isCameraPowerOn;

    static void cameraPowerOff_rk3568();
    static void cameraPowerOn_rk3568();

};

#endif // CHARDWARE_H
