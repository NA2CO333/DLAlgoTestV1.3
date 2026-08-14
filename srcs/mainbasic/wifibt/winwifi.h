#ifndef WINWIFI_H
#define WINWIFI_H

#include <QWidget>
#include <QFile>
#include <QMessageBox>
#include <QTableWidget>
#include <QSettings>
#include <QProcess>

#include "baseform.h"
#include "includes.h"
#include "wifidetails.h"
#include "inputwifipwd.h"
#include "wifibt/wifiintf.h"
#include "globaltypes.h"

//
namespace Ui {
class WinWifi;
}

// WiFi 历史密码（WiFi History Password，即成功连接过的 SSID 和密码，系统会把这些记录保存下来，以后连接时不必再次输入）
struct stWifiHistPwd {
    QString ssid;
    QString pwd;
};

//
class WinWifi : public CBaseWidget
{
    Q_OBJECT
public:
    explicit WinWifi(QWidget *parent, CWifiIntf *_wifi_intf);
    ~WinWifi();

    static bool getCfg_isWifiOpened(bool _is_reload = false);       // 获得配置值：WiFi 是否打开
    static void setCfg_isWifiOpened(bool _is_open);                 // 设置配置值：WiFi 是否打开

    static QString getCfg_lastWifiSsid();                           // 获得配置值：上一次成功连接的 ssid
    static void setCfg_lastWifiSsid(QString _ssid);                 // 设置配置值：上一次成功连接的 ssid

    static const QList<stWifiHistPwd> &getWifiHistPwds();           // 得到所有“WiFi历史密码”
    static int getWifiHistPwdIndex(QString _ssid);                  // 获得“WiFi历史密码”中指定 ssid 的索引号
    static void addWifiHistPwd(QString _ssid, QString _pwd);        // 添加“WiFi历史密码”
    static void removeWifiHistPwd(QString _ssid);                   // 移除“WiFi历史密码”

public slots:
    void slot_RecvSysSignal(enSysSignal _sys_signal);       // 收到系统信号的槽函数（如启动或停止 WiFi 刷新）
    void slot_ScanWifi();

signals:
    void sigSendSysSignal(enSysSignal _sys_signal);
    void sigScanWifi();
    void sigShowWifiInfo(stWifiInfo _wifi_info, QString _ssid_connected, QString _local_ip, QString _local_mac);

protected Q_SLOTS:
    //void slotSetIsOpened(bool _is_open);                    // 设置 WiFi 开关
    void slotDisConnectWifi();                              // 断开当前 WiFi 连接
    void slot_winWifiInfo_ForgetWifi(QString _ssid);        // 忘记 WiFi

    void slot_winPwdInput_GotPwd(QString _ssid, QString _password);
    void slot_tblWifiList_currentItemChanged(QTableWidgetItem*current, QTableWidgetItem*);

    void slot_wifiIntf_RecvStatus(enWifiConnState _status);
    void slot_wifiIntf_WifiListChanged();
    void slot_wifiIntf_Notice(QString _msg);
    void slot_wifiIntf_Message(QString _msg);

protected:
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

    static const char * const S_CLASS_NAME;     // 本类的类名

    static bool isConnect;

    static int WifiSignalItem;
    static QString currentIP;

    static bool isWifiOpenedCfg;        // WiFi 是否打开

    int WifiUpdateCount;

    CWifiIntf *wifiIntf = Q_NULLPTR;

    QString connectingSsid;     // 正在连接的 SSID
    QString lastPassword;       // 最后输入的密码
    int conCnt;
    int conTime;

    static void saveWifiRecords();                  // 保存成功连接过的 WiFi 密码
    static void loadWifiRecords();                  // 载入成功连接过的 WiFi 密码

    void fillWifiListToTable();
    void updatePageWidgets();               // 更新翻页部件

    void updateView_btnIsWifiOpened(bool _is_on);

    QVector<stWifiInfo> m_wifiList;         // WiFi 信息列表（与表格一一对应）

    int m_posPage;
    int m_allPage;
    bool isScanning;

    wifidetails *winWifiInfo;
    inputWiFipwd *winPwdInput;

    bool m_isOpening {false};                       // 是否正在打开
    bool m_isClosing {false};                       // 是否正在关闭

    void setIsOpened(bool _is_open);        // 开关 WiFi
    void openWifi();
    void closeWifi();

    void scanWifi();
    void connectWifi(const QString &_ssid, const bool _is_encry);
    void disconnectWifi();
    void updateWifiList();

    void nextPage();
    void prevPage();

private slots:
    void on_pushButton_Home_clicked();
    void on_pushButton_Back_clicked();
    void on_btnIsWifiOpened_clicked();
    void on_btnSearch_clicked();
    void on_btnConnect_clicked();
    void on_btnPrevPage_clicked();
    void on_btnNextPage_clicked();
    void on_btnHiddenSsid_clicked();
private:
    Ui::WinWifi *ui;
};

#endif // WINWIFI_H
