#include "wifirk.h"

#include <QDebug>

#include "util-common.h"
#include "global.h"

//
static const int INTERVAL_SECS_OF_SCANNING  = 30;    // 定时扫描间隔（秒）

// stWifiInfo 从 SDK 的类型拷贝
void wifiInfoFromSdkType(const RK_WIFI_INFO_Connection_s &_sdk_wifi_info, stWifiInfo &_wifi_info)
{
    _wifi_info.reset();

    _wifi_info.ssid     = _sdk_wifi_info.ssid;
    _wifi_info.localMac = _sdk_wifi_info.mac_address;
    _wifi_info.localIp  = _sdk_wifi_info.ip_address;
}

//
CWifiRk::CWifiRk(QObject *parent) : CWifiIntf(parent)
{
    m_AW_wifi = qtWifi::getInstance();
    m_workThread = new QThread();
    connect(m_AW_wifi, &qtWifi::sendStatus, this, &CWifiRk::slotRecvStatus, Qt::QueuedConnection);
    connect(m_AW_wifi, &qtWifi::sendDev, this, &CWifiRk::slotRecvDev, Qt::QueuedConnection);
    m_AW_wifi->moveToThread(m_workThread);
    m_workThread->start();

}

// 开关 WiFi
bool CWifiRk::setIsOpened(bool _is_opened)
{
    logDebug((QString(__PRETTY_FUNCTION__) + ": into, _is_open = %1").arg(Util::bool2str(_is_opened)), CGlobal::LOG_WIFI);

    // 还原 wpa 配置        // TODO: 这是临时解决方法，待优化
    /* 因为实测（2023/08/25）连接打印机的 WiFi 直连提供的接入点后，且未调用 forget，重启后 WiFi 打开时会出错：
     * “RTW: ERROR rtw_p2p_enable, iface_id:0 is not P2P interface!”。
     * 为解决此问题，这里采取每次打开 WiFi 前都恢复 wpa 配置文件的办法。这样做之后，底层的自动重连就失效，须自己实现重连。
     */
    // TODO: 改为在初始化脚本里执行 "cp /etc/wpa_supplicant.conf /userdata/cfg/wpa_supplicant.conf" ？
    if (_is_opened) {
        static bool is_wpa_cfg_restored = false;
        if (!is_wpa_cfg_restored) {
            restoreWpaConfig();
            is_wpa_cfg_restored = true;
        }
    }

    /* 实测，若是 WiFi 打开状态下，程序崩溃后自动重新执行，那么存在以下问题：
     * 1、通过 API 函数获取的 WiFi 状态是打开的，但是，若不重新执行一次打开，则无法接收到扫描结果。
     * 2、这种状态下，关闭 WiFi 时，接收不到关闭事件。
     *
     * 因此：这里取消了禁止重复开关操作的限制。
     */

    //
    bool is_current_open = false;
    bool is_succ = this->getLastState(&is_current_open, Q_NULLPTR, true);       /* 这里通过 API 重新获取状态，确保状态值准确性 */
    if (is_succ) {
        if (is_current_open == _is_opened) {
            qDebug() << QString("%1: is already %2 !").arg(__PRETTY_FUNCTION__).arg(_is_opened ? "opened" : "closed");
            //return true;
        }
    } else {
        qDebug() << QString("%1: get current state failed!").arg(__PRETTY_FUNCTION__);
        return false;
    }

    if (_is_opened) {
        //
        logDebug(QString(__PRETTY_FUNCTION__) + ": opening wifi ...", CGlobal::LOG_WIFI);
        m_AW_wifi->turnOn();
    } else {
        if (getIsConnected()) {
            discWifi();
        }

        logDebug(QString(__PRETTY_FUNCTION__) + ": closing wifi ...", CGlobal::LOG_WIFI);
        m_AW_wifi->turnOff();
    }

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_WIFI);
    return true;
}

bool CWifiRk::getIsOpened()
{
    //logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_WIFI);

    //
    bool is_open = false;
    bool is_succ = this->getLastState(&is_open, Q_NULLPTR);

    if (is_succ) {
        return is_open;
    } else {
        return false;
    }

    //logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_WIFI);
}

void CWifiRk::restoreWpaConfig()
{
    static constexpr char PATH_WPA_CONF[] = "/userdata/cfg/wpa_supplicant.conf";
    static constexpr char TEXT_WPA_CONF[] =
        "ctrl_interface=/var/run/wpa_supplicant\n"
        "update_config=1\n"
        "\n"
        "network={\n"
        "        ssid=\"SSID\"\n"
        "        psk=\"PASSWORD\"\n"
        "        key_mgmt=WPA-PSK\n"
        "        priority=1\n"
        "}\n";

    QFile file(PATH_WPA_CONF);
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        file.write(TEXT_WPA_CONF);
        file.flush();
        file.close();

        // 磁盘同步，避免意外断电而导致文件数据丢失         // TODO: 改为解析 "/userdata/cfg/wpa_supplicant.conf" 来判断上次连接的是否 wifi-direct ，然后调用 api 来 forget ，避免直接访问这个文件？
        Util::CUDisk::sync();
    } else {
        qWarning() << __PRETTY_FUNCTION__ << ": file '" << PATH_WPA_CONF << "' open failed! reset wpa failed!";
    }

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_WIFI);
}

bool CWifiRk::getIsConnected()
{
    //logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_WIFI);

    //
    bool is_conn = false;
    bool is_succ = this->getLastState(Q_NULLPTR, &is_conn);

    //logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_WIFI);
    if (is_succ) {
        return is_conn;
    } else {
        return false;
    }
}

/**
 * @brief 获取最后的 WiFi 状态
 * @param _is_opened        : 若不为空，则获取【是否打开】状态值
 * @param _is_connected     : 若不为空，则获取【是否连接】状态值
 * @param is_need_re_get    : 是否需要重新获取
 * @return 返回 是否成功
 */
bool CWifiRk::getLastState(bool *_is_opened, bool *_is_connected, bool is_need_re_get)
{
    static constexpr int ESCAPE_TIMES = 30;         /* 状态栏 0.5 秒更新一次，每次调用两次，2 秒重新通过 API 获取最新值（设为0时每次都重新获取） */   // TODO: 这个常量的确定？
    static int count = 0;

    int ret = -1;

    // 若最后值有效且访问次数
    if (m_lastState < 0 || count >= ESCAPE_TIMES) {       // TODO: 这个 ESCAPE_TIMES 减少查询次数的处理是否有必要，如果 RK_wifi_running_getState() 较慢，是有必要的
        //logDebug((QString(__PRETTY_FUNCTION__) + ": need re-get (m_lastState = %1)").arg((int)m_lastState), CGlobal::LOG_WIFI);

        is_need_re_get = true;
    }
    count ++;

    // 若最后状态值不是可直接判断本函数所须的状态的值而是其它中间状态值，则重新查询当前状态值      // TODO: 注意：如果 m_lastState 稳定的不在这几个值范围内，则每次都会重新获取，可能导致程序卡顿等问题！
    if (!(RK_WIFI_State_IDLE == m_lastState ||
          RK_WIFI_State_OFF == m_lastState ||
          RK_WIFI_State_CONNECTED == m_lastState ||
          RK_WIFI_State_DISCONNECTED == m_lastState ||
          RK_WIFI_State_DHCP_OK == m_lastState))
    {
        //logDebug((QString(__PRETTY_FUNCTION__) + ": m_lastState = %1, seem not valid, so need re-get").arg((int)m_lastState), CGlobal::LOG_WIFI);

        is_need_re_get = true;
    }

    // 如果 WiFi 未打开，不需查询
    //if (RK_WIFI_State_DISCONNECTED == m_lastState) {
    //    // logDebug();
    //
    //    is_need_re_get = false;
    //}

    //
    if (is_need_re_get) {
        /* RK_wifi_running_getState() 函数得到的状态值情况实测：
         * 刚开始，未开启 wifi：    RK_WIFI_State_IDLE (0)
         * 开启 wifi 后：           RK_WIFI_State_DISCONNECTED (5)
         * 连接后：                 RK_WIFI_State_CONNECTED (4)
         * 分配 IP 后：             RK_WIFI_State_DHCP_OK (9)
         * 断开连接后：               RK_WIFI_State_DISCONNECTED (5)
         * 关闭 wifi 后：           RK_WIFI_State_OFF (7)
         *
         * 特殊情况 1：如果 wpa_supplicant.conf 文件为空，那么打开 wifi 会失败，且关闭也失败，而状态值一直是 RK_WIFI_State_DISCONNECTED(5) ？
         *
         */
        ret = RK_wifi_running_getState(&m_lastState);

        /* 经测试，进入 runningstatus.cpp 页面时，WiFi 打开后未连接，间歇性地出现 RK_wifi_running_getState() 返回 RK_WIFI_State_IDLE 的情况；
         * 连接后，间歇性地出现返回 RK_WIFI_State_? 的情况。但并不是所有机器，任何时候都这样，同一台机器同一个版本程序，有时能复现有时不能复现。
         * 原因未明，可能和该窗体的某些代码有关，所以这里重试获取。 */
        if (0 == ret && RK_WIFI_State_IDLE == m_lastState) {
            ret = RK_wifi_running_getState(&m_lastState);
        }

        //logDebug((QString(__PRETTY_FUNCTION__) + ": retrieve new state = %1, ret = %2").arg((int)m_lastState).arg(ret), CGlobal::LOG_WIFI);

        count = 0;
    } else {
        ret = 0;

        //logDebug((QString(__PRETTY_FUNCTION__) + ": return last state = %1").arg((int)m_lastState), CGlobal::LOG_WIFI);
    }

    if (_is_opened) {
        *_is_opened = !(RK_WIFI_State_IDLE == m_lastState || RK_WIFI_State_OFF == m_lastState);
    }
    if (_is_connected) {
        *_is_connected = (RK_WIFI_State_CONNECTED == m_lastState || RK_WIFI_State_DHCP_OK == m_lastState);
    }

    return (0 == ret);
}

const stWifiInfo *CWifiRk::getConnectedInfo()
{
    //logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_WIFI);

    stWifiInfo *wifi_info = Q_NULLPTR;

    //logDebug(QString("idx connected = %1, list count = %2").arg(m_idxConnected).arg(m_wifiList.count()), CGlobal::LOG_WIFI);
    if (m_idxConnected >= 0) {
        if (m_idxConnected < m_wifiList.count()) {
            wifi_info = &(m_wifiList[m_idxConnected]);
        } else {
            logCritical(QString(__PRETTY_FUNCTION__) + ": idx out of bound!", CGlobal::LOG_WIFI);
        }
    }

    //logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_WIFI);
    return wifi_info;
}

void CWifiRk::scan()
{
    int ret = RK_wifi_scan();       // TODO: 调用这个函数后，会不停地扫描。怎么停止？
    if (ret < 0) {
        logWarning(QString(__PRETTY_FUNCTION__) + ": failed to scan!", CGlobal::LOG_WIFI);
    }
}

void CWifiRk::connectTo(QString _ssid, QString _pwd)
{
    qDebug() << "CWifiRk::connectTo(): _ssid = " << _ssid << ", _pwd = " << _pwd;

    if (getIsConnected()) {
        discWifi();
    }

    //
    m_idxConnecting = getIdxOfWifiBySsid(_ssid);
    //if (m_idxConnecting < 0) {
    //    logCritical(QString(__PRETTY_FUNCTION__) + ": logic err! ssid not found!", CGlobal::LOG_WIFI);
    //    emit sigNotice(tr("连接失败！"));       // "Connecting failed!"
    //    return;
    //}

    //
    m_lastSsid = _ssid;
    m_lastPwd = _pwd;
    RK_wifi_connect(_ssid.toLocal8Bit().data(), _pwd.toLocal8Bit().data());
}

void CWifiRk::discWifi()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_WIFI);

    if (!getIsConnected()) {
        return;
    }

    RK_wifi_disconnect_network();

    RK_wifi_forget_with_ssid(m_AW_wifi->info_wifi_con.ssid);
    qDebug() << "CWifiRk::discWifi(): ssid = " << m_AW_wifi->info_wifi_con.ssid;

    m_idxConnected = -1;
    m_idxConnecting = -1;

    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_WIFI);
}

int CWifiRk::getDevCount()
{
    return m_AW_wifi->confs.size();
}

bool CWifiRk::getConnInfo(stWifiInfo &_connected_info)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_WIFI);

    //_ssid       = m_AW_wifi->info_wifi_con.ssid;
    //_mac_local  = m_AW_wifi->info_wifi_con.mac_address;
    //_ip_local   = m_AW_wifi->info_wifi_con.ip_address;                                  // TODO: 空的？
    //logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_WIFI);
    //return true;

    int ret = RK_wifi_running_getConnectionInfo(&m_AW_wifi->info_wifi_con);
    if (0 == ret) {
        wifiInfoFromSdkType(m_AW_wifi->info_wifi_con, _connected_info);
    }
    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_WIFI);
    return (0 == ret);
}

void CWifiRk::forgetWifi(QString _ssid)
{
    if (getIsConnected() && (QString(m_AW_wifi->info_wifi_con.ssid) == _ssid)) {
        discWifi();
    }

    RK_wifi_forget_with_ssid(m_AW_wifi->info_wifi_con.ssid);
}

void CWifiRk::slotRecvStatus(int _status)
{
    logDebug(QString("CWifiRk::slotRecvStatus(): into ... _status = %1").arg(_status), CGlobal::LOG_WIFI);

    // 旧的连接状态
    static bool is_conn_last = false;   /* 默认为未连接 */

    //
    m_lastState = (RK_WIFI_RUNNING_State_e)_status;

    //
    enWifiConnState wifi_state = rkStateToIntfState((RK_WIFI_RUNNING_State_e)_status);
    if (wifiConnState_DhcpOk == wifi_state) {
        // 得到当前连接
        //int rssi = RK_wifi_get_connected_ap_rssi();           // TODO: 获取不了？
        //logDebug(QString("connected rssi = %1").arg(rssi), CGlobal::LOG_WIFI);

        stWifiInfo connected_info;
        if (!getConnInfo(connected_info)) {
            QString msg = "Failed to get connected info!";
            qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): " << msg;
            emit sigMessage(msg);
        }

        // 确保当前连接信息在 m_wifiList 中存在且前置
        int idx_conn = -1;
        if (!connected_info.ssid.isEmpty()) {
            idx_conn = getIdxOfWifiBySsid(connected_info.ssid);
            if (idx_conn >= 0) {
                // 在 m_wifiList 中找到，将其前置
                if (idx_conn > 0) {
                    //
                    m_wifiList.move(idx_conn, 0);

                    //
                    emit sigWifiListChanged();
                }
            } else {
                // 在 m_wifiList 中找不到，添加至首位
                m_wifiList.prepend(connected_info);

                //
                emit sigWifiListChanged();
            }

            //
            idx_conn = 0;
        } else {
            QString msg = "LogicError: ssid from getConnInfo() is empty!";
            qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): " << msg;
            emit sigMessage(msg);
        }

        //
        if (m_idxConnecting >= 0 && m_idxConnecting < m_wifiList.count()) {
            m_idxConnected = m_idxConnecting;
            m_idxConnecting = -1;
        } else {
        }

        // 变量设置或重置
        m_idxConnected = idx_conn;
        m_idxConnecting = -1;

        //
        emit sigConnectedStateChanged(true);        /* DHCP 成功才算已连接，否则解析不了域名，大部分网络访问可能失败 */
    } else if (wifiConnState_Disconnected == wifi_state) {
        emit sigConnectedStateChanged(false);
    }

    // 检查“是否已开启”状态的改变
    if (wifiConnState_Opened == wifi_state || wifiConnState_Closed == wifi_state) {
        const bool is_opened = (wifiConnState_Opened == wifi_state);
        if (is_opened != m_isOpened) {
            //
            m_isOpened = is_opened;

            //
            emit sigOpenedStateChanged(m_isOpened);
        }
    }

    // 检查“是否已连接”状态的改变
    bool is_conn_new = getIsConnected();
    if (is_conn_new != is_conn_last) {
        if (!is_conn_new || (is_conn_new && wifiConnState_DhcpOk == wifi_state)) {      /* 连接时，只有到 DhcpOk 后才触发连接改变事件 */
            is_conn_last = is_conn_new;
            logDebug(QString("emitting sigConnectedStateChanged(), stat = %1").arg(is_conn_new ? "connected" : "disconnected"));
            emit sigConnectedStateChanged(is_conn_new);     // 发射“是否已连接”状态改变信号
        }
    }

    //
    emit sigRecvStatus(wifi_state);
}

enWifiConnState CWifiRk::rkStateToIntfState(RK_WIFI_RUNNING_State_e _rk_state)
{
    enWifiConnState wifi_state = wifiConnState_Unknown;
    switch ((RK_WIFI_RUNNING_State_e)_rk_state) {                        /* 见 qtWifi::wifi_callback() */
    case RK_WIFI_State_IDLE: {
        wifi_state = wifiConnState_Idle;
        //
        break;
    }
    case RK_WIFI_State_CONNECTING: {
        wifi_state = wifiConnState_Connecting;
        //
        break;
    }
    case RK_WIFI_State_CONNECTFAILED: {
        wifi_state = wifiConnState_ConnectingFailed;
        //
        break;
    }
    case RK_WIFI_State_CONNECTFAILED_WRONG_KEY: {
        wifi_state = wifiConnState_PassWordErr;
        //
        break;
    }
    case RK_WIFI_State_CONNECTED: {
        wifi_state = wifiConnState_Connected;
        //
        break;
    }
    case RK_WIFI_State_DISCONNECTED: {
        wifi_state = wifiConnState_Disconnected;
        //
        break;
    }
    case RK_WIFI_State_OPEN: {
        wifi_state = wifiConnState_Opened;
        //
        break;
    }
    case RK_WIFI_State_OFF: {
        wifi_state = wifiConnState_Closed;
        //
        break;
    }
    case RK_WIFI_State_SCAN_RESULTS: {
        wifi_state = wifiConnState_ScanResults;
        //
        break;
    }
    case RK_WIFI_State_DHCP_OK: {
        wifi_state = wifiConnState_DhcpOk;
        //
        break;
    }
    default:
        //
        break;
    }
    return wifi_state;
}

void CWifiRk::reset()
{
    m_idxConnected = -1;
    m_idxConnecting = -1;

    m_lastState = (RK_WIFI_RUNNING_State_e)-1;

}

void CWifiRk::slotRecvDev()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_WIFI);

    /* 应确保当前连接点的信息存在于列表头部：
     * 先暂存当前连接点的信息，再添加搜索结果列表，再检查当前连接点是否存在，若存在则移到列表头部，否则添加到列表头部。
     */

    //
    stWifiInfo curr_wifi_info;
    if (m_idxConnected >= 0) {
        if (m_idxConnected < m_wifiList.size()) {
            curr_wifi_info = m_wifiList[m_idxConnected];
        } else {
            QString msg = QString("LogicError: m_idxConnected is out of bound[0~%1)!").arg(m_wifiList.size());
            qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): " << msg;
            emit sigMessage(msg);
            m_idxConnected = -1;
        }
    }
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): ssid_connected = " << curr_wifi_info.ssid;

    m_wifiList.clear();           // TODO: 访问锁？若存在跨线程访问，必须注意线程安全

    //
    const int count = m_AW_wifi->confs.size();
    qDebug() << "WiFi scan result count = " << count;

    for (int i = 0; i < count; i++) {
        const WiFiConfig &wifi_cfg = m_AW_wifi->confs.at(i);

        qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): rssi of \"" << wifi_cfg.ssid << "\" = " << wifi_cfg.signal;

        if (wifi_cfg.ssid == curr_wifi_info.ssid) {
            curr_wifi_info.rssi = wifi_cfg.signal;
        } else {
            stWifiInfo wifi_info;

            wifi_info.ssid      = wifi_cfg.ssid;
            wifi_info.rssi      = wifi_cfg.signal;
            wifi_info.key_mgmt  = wifi_cfg.key_mgmt;
            wifi_info.encry     = !wifi_cfg.key_mgmt.isEmpty();

            m_wifiList.append(wifi_info);
        }
    }
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_wifiList.size() = " << m_wifiList.size() << " after ScanResult updating.";

    // 按信号强度降序排列
    std::sort(m_wifiList.begin(), m_wifiList.end(), [](const stWifiInfo &_a, const stWifiInfo &_b) { return _a.rssi > _b.rssi; });

    //
    if (m_idxConnected >= 0) {
        // 插入当前连接点的信息到头部
        m_wifiList.prepend(curr_wifi_info);

        //
        m_idxConnected = 0;
    }

    //
    emit sigWifiListChanged();

    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_WIFI);
}

int CWifiRk::getIdxOfWifiBySsid(QString _ssid)
{
    int idx = -1;
    for (int i = 0; i < m_wifiList.count(); i++) {
        if (_ssid == m_wifiList[i].ssid) {
            idx = i;
            break;
        }
    }
    return idx;
}

