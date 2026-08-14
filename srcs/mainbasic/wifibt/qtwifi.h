#ifndef QTWIFI_H
#define QTWIFI_H

#include <QDebug>
#include <QProcess>
#include <QThread>
#include <QTimer>

//#include "wificonnection.h"

struct WiFiConfig   // TODO: 改用 stWifiInfo 类型？
{
    QString bssid;
    QString ssid;
    QString psk;
    QString key_mgmt;
    int signal;
};

extern "C" {

#include "RkWifiBt/Rk_wifi.h"
#include "RkWifiBt/Rk_softap.h"

}

class qtWifi : public QObject
{
    Q_OBJECT

public:
    ~qtWifi();

    static qtWifi* getInstance()
    {
        if (!_instance) {
            _instance = new qtWifi(nullptr, false);
        }
        return _instance;
    }

    QVector<WiFiConfig> confs;
    RK_WIFI_INFO_Connection_s info_wifi_con;        // 当前连接信息

    bool isOn();
    void turnOn();
    void turnOff();

    static int wifi_callback(RK_WIFI_RUNNING_State_e state,
                             RK_WIFI_INFO_Connection_s *info);

//    void emit_recvDev(QVector<WiFiConfig> confs);
    void emit_recvDev();
    void emit_recvStatus(int status);
private:
    static qtWifi* _instance;
    qtWifi(QWidget *parent = nullptr, bool on = false);

    QString ssid;
signals:
    //void sendDev(QVector<WiFiConfig> confs);
    void sendDev();
    void sendStatus(int Status);

public slots:


};

#endif /* QTWIFI_H */
