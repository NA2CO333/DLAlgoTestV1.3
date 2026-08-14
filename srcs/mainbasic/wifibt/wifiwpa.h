#ifndef CWIFIWPA_H
#define CWIFIWPA_H

#include <QObject>
#include <QTimer>

#include "wifibt/wifiintf.h"

//
class CWifiWpa : public CWifiIntf
{
    Q_OBJECT
public:
    bool setIsOpened(bool _is_opened) override;
    bool getIsOpened() override;

    bool getIsConnected() override;

    const stWifiInfo *getConnectedInfo() override {
        static stWifiInfo *wifi_info = Q_NULLPTR;
        if (!wifi_info) {
            wifi_info = new stWifiInfo;
            //memset(wifi_info, 0, sizeof (stWifiInfo));    /* 包含字符串的结构体，不能这样置零 */
        }
        return wifi_info;
    }

    void scan() override {}
    void connectTo(QString _ssid, QString _pwd) override {}
    void discWifi() override {}

    int getDevCount() override { return 0; }
    bool getConnInfo(stWifiInfo &_connected_info) override { return false; }

    void forgetWifi(QString _ssid) override {}

};

////
//#include <QWidget>
//#include <QFile>
//#include <QProgressDialog>
//#include <QMessageBox>
//#include <QTimer>
//#include <QTableWidget>
//#include <QSettings>
//#include <QProcess>

//#include "baseform.h"
//#include "includes.h"
//#include "wifibt/wpa/wpacommit.h"
//#include "functions.h"
//#include "wifidetails.h"
//#include "inputwifipwd.h"
//#include "wifibt/wifiintf.h"

//// /* 拷贝自 iMX6Q 版软件的 UI 和逻辑代码混在一起的模块，需要剥离 UI 部分 */
//class CWifiWpa : public CWifiIntf
//{
//    Q_OBJECT
//public:
//    explicit CWifiWpa(QObject *parent = nullptr);
//    ~CWifiWpa();
//    QTabWidget *tabWidget;
//    NetworkInfo *network;

//    static int WifiSignalItem;
//    static QString currentIP;
//    bool manualConnect;
//    int WifiUpdateCount;

//    bool getIsConnected();

//public slots:
//    void slot_RecvSysSignal(enSysSignal _sys_signal);
//    void slot_close();

//private slots:
//    void pbConnectClicked();
//    void slotAfterInputPassword(QString);
//    void slot_open();
//    void slot_read_wifilist();
//    void slot_wifiState(bool);
//    void pbCloseClicked();
//    void slot_timeout();
//    void slot_wifiStatus();
//    void slot_next();
//    void slot_back();
//    void slot_scanning();
//    void slot_open_clicked();
//    void getIPSlot(int);
//    void processTimeout();
//    void slot_itemChanged(QTableWidgetItem*current, QTableWidgetItem*previous);
//    void CancelEnterPwd(bool);
//    void UpdateWifiListName();

//signals:
//    void sendSIGNAL(enSysSignal _sys_signal);
//    void wpaSig(int cmd);
//    void sendAboutsingal(WIFI,QString);
//    void sigWrite(const uchar *_data, qint64 _size, int _time_interval = 0);
//    void signal_pwd_text_clear();

//private slots:
//    void slot_currentChanged(int index);

//    void on_pushButton_Home_clicked();

//    void on_pushButton_Back_clicked();

//protected:
//    static bool isConnect;

//    void showEvent(QShowEvent *event);
//    void hideEvent(QHideEvent *event);

//private:
//    //QNetConfigure *netConfigure;

//    CWifiIntf *wifiIntf = Q_NULLPTR;

//    QInputFileName *m_ifn;
//    QString currentSSID;
//    QString currentPWD;
//    QString ReceivePWD;
//    int conCnt;
//    int conTime;
//    bool wifi_open;

//    QStringList getKO();
//    void getWiFiInfo();
//    void getWiName();
//    bool explainWiFiInfo(QString);
//    void addTabWidgetItem();
//    void connectToWiFi(QString ssid,QString pwd);
//    void getIP();
//    bool checkNetwork();
//    bool isConnectWlan();
//    void setButtonShow();
//    bool SortByM1(WIFI &v1, WIFI &v2)//注意：本函数的参数的类型一定要与vector中元素的类型一致
//    {
//        return (v1.getStrength() > v2.getStrength()) ? true : false;//降序排列
//    }

//    void sort();
//    void autoConnect();

//    WpaCommit *wpa;
//    QVector<WIFI> m_wifi,temp_wifi;

//    QTimer *qt;
//    QTimer *wifiStatus;
//    QTimer *wifiListName;
//    int m_posPage;
//    int m_allPage;
//    bool isScanning;
//    bool readWifiListState;

//    static QProcess *process;
//    QTimer *processTimer;
//    wifidetails *aboutwifi;
//    inputWiFipwd *inputPwd;

//};

#endif // CWIFIWPA_H
