#include "usb-drive-monitor.h"

#include <QFile>
#include <QDebug>

//
CUsbDriveMonitor *CUsbDriveMonitor::instance()
{
    if (!s_instance) {
        s_instance = new CUsbDriveMonitor();
    }
    return s_instance;
}

CUsbDriveMonitor::CUsbDriveMonitor(QObject *_parent) : QThread(_parent)
{

}

CUsbDriveMonitor::~CUsbDriveMonitor()
{
    m_isStarted = false;
}

void CUsbDriveMonitor::run()
{
    //
    m_isStarted = true;

    //
    do {
        //
        bool is_on_line = checkIfOnline();
        if (is_on_line != m_lastUsbDriveOnline) {
            m_lastUsbDriveOnline = is_on_line;
            emit sigUsbDriveOnlineChanged(is_on_line);
        }

        //
        msleep(400);
    } while (m_isStarted);

    //
    qDebug() << "CUsbDriveMonitor::run() exited";
}

bool CUsbDriveMonitor::checkIfOnline()
{
    // 1.分区设备是否存在
    bool devExist = QFile::exists("/dev/sda1");
    if (!devExist)
        return false;

    // 2.进一步确认已经挂载到 /media/usb0
#if (OS_TYPE != 2)
    static constexpr char MOUNT_PATH[] = " /media/usb0 ";
#else
    static constexpr char MOUNT_PATH[] = " /media/henry/";
#endif
    QFile f("/proc/mounts");
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.readAll().contains(MOUNT_PATH);
}
