#ifndef WIFIDETAILS_H
#define WIFIDETAILS_H

#include <QWidget>
#include <QSettings>

#include "baseform.h"
#include "myeditline.h"
#include "wifibt/wifiintf.h"

//
namespace Ui {
class wifidetails;
}

//
class WinWifi;

//
class wifidetails : public CBaseWidget
{
    Q_OBJECT

public:
    explicit wifidetails(QWidget *parent);
    ~wifidetails();

signals:
    void sigForgetWifi(QString _ssid);

public slots:
    void slotShowWifiInfo(stWifiInfo _wifi_info, QString _ssid_connected, QString _local_mac, QString _local_ip);

private slots:
    void on_pushButton_Forget_clicked();
    void on_pushButton_Cancel_clicked();

protected:
    void paintEvent(QPaintEvent *event);
    void showEvent(QShowEvent *);

private:
    Ui::wifidetails *ui;
};

#endif // wifidetails_H
