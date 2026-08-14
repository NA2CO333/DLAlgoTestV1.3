#ifndef CWIFIRK_H
#define CWIFIRK_H

#include <QObject>
#include <QThread>
#include <QTimer>

#include "wifibt/wifiintf.h"
#include "wifibt/qtwifi.h"

class CWifiRk : public CWifiIntf
{
    Q_OBJECT
public:
    bool setIsOpened(bool _is_opened) override;
    bool getIsOpened() override;

    bool getIsConnected() override;
    const stWifiInfo *getConnectedInfo() override;

    void scan() override;
    void connectTo(QString _ssid, QString _pwd) override;
    void discWifi() override;

    int getDevCount() override;
    bool getConnInfo(stWifiInfo &_connected_info) override;

    void forgetWifi(QString _ssid) override;

private slots:
    void slotRecvStatus(int _status);
    void slotRecvDev();

protected:
    friend class CWifiIntf;
    explicit CWifiRk(QObject *parent = nullptr);
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    enWifiConnState rkStateToIntfState(RK_WIFI_RUNNING_State_e _rk_state);      // Rk3568库定义的状态值转通用接口定义的变量值
    void reset();
    int getIdxOfWifiBySsid(QString _ssid);
    bool getLastState(bool *_is_opened, bool *_is_connected, bool is_need_re_get = false);
    void restoreWpaConfig();

    //
    qtWifi *m_AW_wifi = Q_NULLPTR;

    //
    QThread *m_workThread = Q_NULLPTR;
    int m_idxConnecting = -1;
    int m_idxConnected = -1;             // 当前连接在 wifiList 中的索引号
    QString m_lastSsid;
    QString m_lastPwd;
    RK_WIFI_RUNNING_State_e m_lastState = (RK_WIFI_RUNNING_State_e)-1;

};

#endif // CWIFIRK_H
