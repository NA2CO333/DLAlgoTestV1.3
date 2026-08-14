#ifndef WPACOMMIT_H
#define WPACOMMIT_H

//初始化wifi
#define WPA_INIT                1
//连接wifi
#define WPA_CONNECT             2
//连接状态
#define CTRL_REQUEST_STATUS     3
//断开wifi
#define CTRL_CMD_DISCON         4
//搜索wifi
#define CTRL_CMD_SCAN           5
//关闭wifi
#define CTRL_CMD_CLOSE          6
//获取wifi列表
#define GET_WIFILIST            7

#include <QObject>
#include "wifibt/wpa/includes.h"
#include "wifibt/wpa/wpa_ctrl.h"
#include <QSocketNotifier>
#include <QVector>

struct WIFI{
    QString id;
    QString ssid;
    QString pwd;
    QString mac;
    QString strength;
    QString mode;
    QString proto;
    QString key_mgmt;
    QString pairwise;
    bool encry;

    WIFI(){
        encry = false;
        key_mgmt = "WPA-PSK";
    }

    QString getEncry(){
        if(encry)
            return key_mgmt;
        else
            return "ON";
        //return (encry ? "on" : "off");
    }

    int getStrength(){
        return strength.toInt();
    }
};

class WpaCommit : public QObject
{
    Q_OBJECT
public:
    explicit WpaCommit(QObject *parent = 0);
    bool WPA_init();
    QVector<WIFI> getWifiList(){
        return m_wifilist;
    }
    static QVector<WIFI> m_wifilist;
    static WIFI currentWIFI;

    bool wpa_connect_wifi(WIFI wifi);

    bool wpa_ctrl_cmd(const char *cmd){
        char reply[10];
        size_t reply_len = sizeof(reply);
       if(ctrlRequest(cmd,  reply, &reply_len) == 0)
           return true;
       return false;
    }

    int ctrlRequest(const char *cmd, char *buf, size_t *buflen);
    bool checkConnect();

signals:
    void signal_read_wifilist();
    void signal_wifiState(bool);

public slots:
    void receiveMsgs();
    void processMsg(char *msg);
    void ctrlCmdHandler(int);


private:

    char *ctrl_iface;
    char* ctrl_iface_dir;
    struct wpa_ctrl *ctrl_conn;
    struct wpa_ctrl *monitor_conn;
    QSocketNotifier *msgNotifier;
    int pingsToStatusUpdate;
    bool networkMayHaveChanged;

    int openCtrlConnection(const char *ifname);
    //int ctrlRequest(const char *cmd, char *buf, size_t *buflen);
    void updateResults();
    static int str_match(const char *a, const char *b);
};

#endif // WPACOMMIT_H
