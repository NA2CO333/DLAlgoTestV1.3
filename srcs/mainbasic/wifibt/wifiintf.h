#ifndef WIFIINTF_H
#define WIFIINTF_H

#include <QObject>
#include <QVector>

// 信号强度等级（等级越高越强）
enum enWiFiStrengthLevel {
    wiFiStrengthLevel_4,
    wiFiStrengthLevel_3,
    wiFiStrengthLevel_2,
    wiFiStrengthLevel_1,
};

// WiFi 信息
struct stWifiInfo{
    QString id;
    QString ssid;
    QString pwd;
    QString localMac;   // 本地 MAC
    QString localIp;    // 本地 IP
    int rssi {-999};
    QString mode;
    QString proto;
    QString key_mgmt;
    QString pairwise;
    bool encry {true};

    //
    static const stWifiInfo DEFAULT;

    //
    stWifiInfo(){
        rssi = 0;
        encry = false;
        key_mgmt = "WPA-PSK";
    }

    //
    void reset() {
        *this = DEFAULT;
    }

    //
    QString getEncry(){
        if(encry)
            return key_mgmt;
        else
            return "ON";
        //return (encry ? "on" : "off");
    }

    //
    int getRssi(){
        return rssi;
    }
};

// 连接状态
enum enWifiConnState {
    wifiConnState_Unknown           = -1,
    wifiConnState_Idle              ,
    wifiConnState_Opened            ,
    wifiConnState_Closed            ,
    wifiConnState_ScanResults       ,
    wifiConnState_Connecting        ,
    wifiConnState_Connected         ,
    wifiConnState_DhcpOk            ,
    wifiConnState_Disconnected      ,
    wifiConnState_PassWordErr       ,
    wifiConnState_ConnectingFailed  ,
};

//
class CWifiIntf : public QObject
{
    Q_OBJECT
public:
    ~CWifiIntf();
    static CWifiIntf *instance();

    const QVector<stWifiInfo> &getWifiList();

    virtual bool setIsOpened(bool _is_opened) = 0;              // 开关 WiFi
    virtual bool getIsOpened() = 0;

    virtual bool getIsConnected() = 0;
    virtual const stWifiInfo *getConnectedInfo() = 0;           // 当前连接的 WiFi 信息    //NOTE: 注意：若无连接，返回空指针

    virtual void scan() = 0;                                    // 扫描网络
    virtual void connectTo(QString _ssid, QString _pwd) = 0;    //
    virtual void discWifi() = 0;

    virtual int getDevCount() = 0;                              // 搜索到的 WiFi 接入点个数
    virtual bool getConnInfo(stWifiInfo &_connected_info) = 0;

    virtual void forgetWifi(QString _ssid) = 0;                 // 忘记网络

    bool isWifiDirect();                                        // 当前连接是否 WiFi 直连

    static enWiFiStrengthLevel rssiToStrengthLevel(int _rssi);

#if (OS_TYPE == 2)
    void emitConnectedStateChanged(bool _is_connected);
#endif

signals:
    void sigOpenedStateChanged(bool _is_opened);                // “是否已打开”状态改变信号
    void sigConnectedStateChanged(bool _is_connected);          // “是否已连接”状态改变信号

    void sigRecvStatus(enWifiConnState _status);
    void sigWifiListChanged();
    void sigNotice(QString _msg);       // TODO: 整理完善
    void sigMessage(QString _msg);

protected:
    explicit CWifiIntf(QObject *parent = nullptr);

    static CWifiIntf *s_instance;

    QVector<stWifiInfo> m_wifiList;

    bool m_isOpened = false;
    bool m_isConnected = false;

};

#endif // WIFIINTF_H
