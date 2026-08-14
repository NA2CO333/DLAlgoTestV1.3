#include "wifiintf.h"

#if (WIFI_TYPE == 1)
#  include "wifibt/wifiwpa.h"
#else
#  include "wifibt/wifirk.h"
#endif

//
const stWifiInfo stWifiInfo::DEFAULT = stWifiInfo {};

//
CWifiIntf *CWifiIntf::s_instance = Q_NULLPTR;

//
CWifiIntf *CWifiIntf::instance()
{
    if (!s_instance) {
#if (WIFI_TYPE == 1)
        s_instance = new CWifiWpa();
#else
        s_instance = new CWifiRk();
#endif
    }
    return s_instance;
}

CWifiIntf::CWifiIntf(QObject *parent) : QObject(parent)
{
    //
    static bool is_reg = false;
    if (!is_reg) {
        //
        qRegisterMetaType<enWifiConnState>("enWifiConnState");

        //
        is_reg = true;
    }
}

CWifiIntf::~CWifiIntf()
{

}

const QVector<stWifiInfo> &CWifiIntf::getWifiList()
{
    return m_wifiList;
}

bool CWifiIntf::isWifiDirect()
{
    const static QString WIFI_DIRECT_SSID_PREFIX = "DIRECT";        // WiFi 直连时的 SSID 前缀

    const stWifiInfo *wifi_conn_info = getConnectedInfo();

    bool is_wifi_direct;
    if (wifi_conn_info) {
        QString ssid = wifi_conn_info->ssid;
        is_wifi_direct = ssid.startsWith(WIFI_DIRECT_SSID_PREFIX);
    } else {
        is_wifi_direct = false;
    }

    return is_wifi_direct;
}

enWiFiStrengthLevel CWifiIntf::rssiToStrengthLevel(int _rssi)
{
    enWiFiStrengthLevel level = wiFiStrengthLevel_1;
    if (_rssi < 0) {
        if (_rssi >= -60) {
            level = wiFiStrengthLevel_4;
        } else if (_rssi >= -80) {
            level = wiFiStrengthLevel_3;
        } else if (_rssi >= -90) {
            level = wiFiStrengthLevel_2;
        } else /*if (_rssi >= -100)*/ {
            level = wiFiStrengthLevel_1;
        }
    }
    return level;
}

#if (OS_TYPE == 2)
void CWifiIntf::emitConnectedStateChanged(bool _is_connected)
{
    emit sigConnectedStateChanged(_is_connected);
}
#endif
