#ifndef CUSBDRIVEMONITOR_H
#define CUSBDRIVEMONITOR_H

#include <atomic>

#include <QThread>

#include "util-common.h"

// U 盘插拔侦听
class CUsbDriveMonitor : public QThread
{
    Q_OBJECT
public:
    static CUsbDriveMonitor *instance();

    void setIsStarted(bool _is_started) { m_isStarted = _is_started; }

signals:
    void sigUsbDriveOnlineChanged(bool _is_on);     // 【U盘在线】改变信号

protected:
    explicit CUsbDriveMonitor(QObject *_parent = nullptr);
    ~CUsbDriveMonitor();
    inline static CUsbDriveMonitor *s_instance {nullptr};

    void run() override;

    bool checkIfOnline();                       // 检查 U 盘是否在线

    std::atomic<bool> m_isStarted {false};
    bool m_lastUsbDriveOnline {false};

};

#endif // CUSBDRIVEMONITOR_H
