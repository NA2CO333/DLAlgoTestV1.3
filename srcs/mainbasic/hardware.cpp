#include "hardware.h"

#include "global.h"

//
bool CHardware::isCameraPowerOn = true;

//
CHardware::CHardware()
{

}

// 相机断电（前后未加延时，需要调用者酌情添加）
void CHardware::cameraPowerOff()
{
    logDebug("CHardware::cameraPowerOff(): into ...", CGlobal::LOG_CAPTURE);

    if (!isCameraPowerOn) {
        logWarning("CHardware::cameraPowerOff(): camera power is already off!", CGlobal::LOG_CAPTURE);
        return;
    }

#if (3 == OS_TYPE)
    cameraPowerOff_rk3568();
#else
    logDebug("CHardware::cameraPowerOff(): not implemented!", CGlobal::LOG_CAPTURE);
    // TODO:

#endif

    isCameraPowerOn = false;
}

// 相机上电（前后未加延时，需要调用者酌情添加）
void CHardware::cameraPowerOn()
{
    logDebug("CHardware::cameraPowerOn(): into ...", CGlobal::LOG_CAPTURE);

    if (isCameraPowerOn) {
        logWarning("CHardware::cameraPowerOn(): camera power is already on!", CGlobal::LOG_CAPTURE);
        return;
    }

#if (3 == OS_TYPE)
    cameraPowerOn_rk3568();
#else
    logDebug("CHardware::cameraPowerOn(): not implemented!", CGlobal::LOG_CAPTURE);
    // TODO:

#endif

    isCameraPowerOn = true;
}

bool CHardware::getIsCameraPowerOn()
{
    return isCameraPowerOn;
}

//
static const char *CMD_WL02_SWITCH_TO_HOST_1    = "echo HOST > /dev/otg_mode";
static const char *CMD_WL02_SWITCH_TO_HOST_2    = "echo host > /sys/devices/platform/fe8a0000.usb2-phy/otg_mode";
static const char *CMD_WL02_SWITCH_TO_DEVICE_1  = "echo peripheral > /sys/devices/platform/fe8a0000.usb2-phy/otg_mode";
static const char *CMD_WL02_SWITCH_TO_DEVICE_2  = "echo DEVICE > /dev/otg_mode";

static const char *CMD_WL04_USB_PWR_ON          = "echo 1 > /sys/class/leds/usb3_host2_pwr/brightness";
static const char *CMD_WL04_USB_PWR_OFF         = "echo 0 > /sys/class/leds/usb3_host2_pwr/brightness";

// 相机断电_RK3568
void CHardware::cameraPowerOff_rk3568()
{
    logDebug("CHardware::cameraPowerOff_rk3568(): into ...", CGlobal::LOG_CAPTURE);

    // 区分主板版本 wl02 和 wl04
    if (QFile::exists("/dev/otg_mode")) {       // wl02 版主板
        logDebug("is wl02", CGlobal::LOG_CAPTURE);

        system(CMD_WL02_SWITCH_TO_DEVICE_1);
        Util::waitMs(200);
        system(CMD_WL02_SWITCH_TO_DEVICE_2);
    } else {                                    // wl04 版主板
        logDebug("is wl04", CGlobal::LOG_CAPTURE);

        system(CMD_WL04_USB_PWR_OFF);
    }

}

// 相机上电_RK3568
void CHardware::cameraPowerOn_rk3568()
{
    logDebug("CHardware::cameraPowerOn_rk3568(): into ...", CGlobal::LOG_CAPTURE);

    // 区分主板版本 wl02 和 wl04
    if (QFile::exists("/dev/otg_mode")) {       // wl02 版主板
        logDebug("is wl02", CGlobal::LOG_CAPTURE);

        system(CMD_WL02_SWITCH_TO_HOST_1);
        Util::waitMs(200);
        system(CMD_WL02_SWITCH_TO_HOST_2);
    } else {                                    // wl04 版主板
        logDebug("is wl04", CGlobal::LOG_CAPTURE);

        // 上电
        system(CMD_WL04_USB_PWR_ON);
    }
}

// 设置机器的日期时间（包括操作系统和硬件时间）
void CHardware::setMachineDateTime(QDateTime _date_time)
{
    qDebug() << "CHardware::setMachineDateTime(): into ... date_time = " << _date_time.toString();

    setSysDateTime(_date_time);
    syncHardwareDateTime(true);

    qDebug() << "CHardware::setMachineDateTime(): ended";
}

// 设置系统时间
void CHardware::setSysDateTime(QDateTime _date_time)
{
    QString cmd_date = QString("date -s ") + _date_time.toString("yyyy-MM-dd");
    system(cmd_date.toLatin1().data());
    qDebug() << "CHardware::setSysDateTime(): cmd = \"" << cmd_date << "\"";

    QString cmd_time = QString("date -s ") + _date_time.toString("hh:mm:ss");
    system(cmd_time.toLatin1().data());
    qDebug() << "CHardware::setSysDateTime(): cmd = \"" << cmd_time << "\"";
}

// 同步系统时钟。
// @param _is_sys_to_hw : 同步方向是系统到硬件，若否，则是从硬件时钟同步到操作系统
void CHardware::syncHardwareDateTime(bool _is_sys_to_hw)
{
    QString cmd = QString("hwclock ") + (_is_sys_to_hw ? " -w" : " -s");
    system(cmd.toLatin1().data());
    qDebug() << "CHardware::setHardwareDateTime(): cmd = \"" << cmd << "\"";
}

// 读取硬件时间（若失败，则 QDateTime is not isValid() ）
QDateTime CHardware::getHardwareDateTime()
{
    QDateTime date_time;

    QString cmd = "hwclock -r";
    QString response_str;
    Util::executeLinuxCmd(cmd, &response_str);
    qDebug() << cmd << " :-> " << response_str;

    int idx = 0;
    for (int i = 0; i < 5; i++) {               // 查找第五个空格位置（hwclock -r 输出时间格式：Thu Mar 23 11:11:55 2023  0.000000 seconds）
        idx = response_str.indexOf(' ', idx);
        if (idx < 0) {
            break;
        }
    }
    if (idx < 0) {
        qWarning() << "get datetime from \"" << response_str << "\" failed?";
        idx = response_str.length() - 1;
    }

    QString date_time_str = response_str.left(idx);             // TODO: 此格式只适合 rk3568 主板当前版本的 buildroot 系统，更多的日期格式兼容？
    qDebug() << "got hardware datetime: " << date_time_str;
    date_time = QDateTime::fromString(date_time_str);

    return date_time;
}

