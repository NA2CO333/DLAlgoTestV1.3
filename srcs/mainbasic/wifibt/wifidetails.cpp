#include "wifidetails.h"
#include "ui_wifidetails.h"

#include <QMessageBox>
#include <QDebug>

#include "windowsmanager.h"
#include "winwifi.h"
#include "appsetting.h"
#include "global.h"
#include "utilui.h"

//
stWifiInfo g_wifiInfo;

// 关于wifi的详细信息
wifidetails::wifidetails(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::wifidetails)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_TranslucentBackground);

    //
    Util::Ui::clearStyleSheet(this);

}

wifidetails::~wifidetails()
{
    delete ui;
}

void wifidetails::showEvent(QShowEvent *)
{
    //
    ui->pushButton_Cancel->setFocusPolicy(Qt::StrongFocus);
    if(themeType_Black == getSysThemeType())
        ui->pushButton_Cancel->setStyleSheet("QPushButton:focus; background-color:rgb(40,40,40); color:rgb(255,255,255); border-radius:5px; padding:2px 4px;");
    else
        ui->pushButton_Cancel->setStyleSheet("QPushButton:focus; background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");

    //设置忘记网络按钮颜色
    bool is_recorded = false;
    const QList<stWifiHistPwd> &wifi_records = WinWifi::getWifiHistPwds();
    for (int i=0; i < wifi_records.size(); i++){
        const stWifiHistPwd record = wifi_records.at(i);
        if (record.ssid == g_wifiInfo.ssid){
            qDebug() << "find the same ssid:" << g_wifiInfo.ssid;
            is_recorded = true;
            break;
        }
    }
    if (is_recorded) {
        if (themeType_Black == getSysThemeType()) {
            ui->pushButton_Forget->setStyleSheet("background-color:rgb(40,40,40); color:rgb(255,255,255); border-radius:5px; padding:2px 4px;");
        } else {
            ui->pushButton_Forget->setStyleSheet("background-color:rgb(200,200,202); color:rgb(1,1,1); border-radius:5px; padding:2px 4px;");
        }
        ui->pushButton_Forget->setEnabled(true);
    } else {
        if (themeType_Black == getSysThemeType()) {
            ui->pushButton_Forget->setStyleSheet("background-color:rgb(40,40,40); color:rgb(100,100,102); border-radius:5px; padding:2px 4px;");
        } else {
            ui->pushButton_Forget->setStyleSheet("background-color:rgb(200,200,202); color:rgb(130,130,132); border-radius:5px; padding:2px 4px;");
        }
        ui->pushButton_Forget->setEnabled(false);
    }

    //if (language) {
    //    ui->label_Status->setGeometry(70,30,75,17);
    //    ui->label_Status_Text->setGeometry(150,30,200,17);
    //    ui->label_Status->setText("状态信息:");
    //
    //    ui->label_Signal->setGeometry(70,60,75,17);
    //    ui->label_Signal_Text->setGeometry(150,60,200,17);
    //    ui->label_Signal->setText("信号强度:");
    //
    //    ui->label_Encrypt->setGeometry(70,90,75,17);
    //    ui->label_Encrypt_Text->setGeometry(150,90,200,17);
    //    ui->label_Encrypt->setText("加密方式:");
    //
    //    ui->label_MAC->setGeometry(70,120,75,17);
    //    ui->label_MAC_Text->setGeometry(150,120,200,17);
    //    ui->label_MAC->setText("MAC地址:");
    //
    //    ui->label_IP->setGeometry(70,150,75,17);
    //    ui->label_IP_Text->setGeometry(150,150,200,17);
    //    ui->label_IP->setText("IP地址     :");
    //
    //    ui->pushButton_Forget->setText("忘记网络");
    //    ui->pushButton_Cancel->setText("取消");
    //} else {
    //    ui->label_Status->setGeometry(70,30,147,17);
    //    ui->label_Status_Text->setGeometry(222,30,200,17);
    //    ui->label_Status->setText("Status messages:");
    //
    //    ui->label_Signal->setGeometry(70,60,147,17);
    //    ui->label_Signal_Text->setGeometry(222,60,200,17);
    //    ui->label_Signal->setText("Signal intensity:");
    //
    //    ui->label_Encrypt->setGeometry(70,90,147,17);
    //    ui->label_Encrypt_Text->setGeometry(222,90,200,17);
    //    ui->label_Encrypt->setText("Way of encryption:");
    //
    //    ui->label_MAC->setGeometry(70,120,147,17);
    //    ui->label_MAC_Text->setGeometry(222,120,200,17);
    //    ui->label_MAC->setText("MAC address:");
    //
    //    ui->label_IP->setGeometry(70,150,147,17);
    //    ui->label_IP_Text->setGeometry(222,150,200,17);
    //    ui->label_IP->setText("IP address:");
    //
    //    ui->pushButton_Forget->setText("Forget");
    //    ui->pushButton_Cancel->setText("Cancel");
    //}
}

void wifidetails::paintEvent(QPaintEvent *event)
{
    //make stylesheet working when setting background transparent
    QStyleOption opt;
    opt.init(this);
    QPainter pt(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &pt, this);

    pt.setRenderHint(QPainter::Antialiasing);  // 反锯齿;
    if(themeType_Black == getSysThemeType())
        pt.setBrush(QBrush(QColor(60, 60, 60, 220)));
    else
        pt.setBrush(QBrush(QColor(180, 180, 180, 220)));
    pt.setPen(Qt::transparent);
    QRect rect = this->rect();
    rect.setWidth(rect.width() - 1);
    rect.setHeight(rect.height() - 1);
    pt.drawRoundedRect(rect, 15, 15);

    QWidget::paintEvent(event);
}

void wifidetails::slotShowWifiInfo(stWifiInfo _wifi_info, QString _ssid_connected, QString _local_mac, QString _local_ip)
{
    qDebug()<<"-----_wifi_info.ssid:"<<_wifi_info.ssid<<"  _ssid_connected:"<<_ssid_connected;
    if(this->focusWidget())
        qDebug() << this->focusWidget()->objectName();

    //
    g_wifiInfo = _wifi_info;

    //连接状态
    if(_wifi_info.ssid == _ssid_connected){
        ui->label_Status_Text->setText(tr("已连接"));  // "connected"
        ui->label_MAC_Text->setText(_local_mac.toUpper());      //MAC地址
        ui->label_IP_Text->setText(_local_ip);                  //IP地址
    }
    else{
        ui->label_Status_Text->setText(tr("未连接"));  // "disconnected"
        ui->label_MAC_Text->setText(tr("无"));    // MAC地址   // "no"
        ui->label_IP_Text->setText(tr("无"));     // IP地址    // "no"
    }

    //信号强度
    enWiFiStrengthLevel strength_level = CWifiIntf::rssiToStrengthLevel(_wifi_info.rssi);

    if (wiFiStrengthLevel_4 == strength_level)
        ui->label_Signal_Text->setText(tr("强"));    // "strong"
    else if (wiFiStrengthLevel_3 == strength_level)
        ui->label_Signal_Text->setText(tr("中"));    // "general"
    else if (wiFiStrengthLevel_2 == strength_level || wiFiStrengthLevel_1 == strength_level)
        ui->label_Signal_Text->setText(tr("弱"));    // "weak"

    ui->label_Encrypt_Text->setText(_wifi_info.key_mgmt);    //加密方式

    this->show();
    this->move ((SCREEN_WIDTH - this->width())/2,(SCREEN_HEIGHT - this->height())/2);
    this->setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
}

void wifidetails::on_pushButton_Forget_clicked()
{
    emit sigForgetWifi(g_wifiInfo.ssid);

    qDebug()<<"forget record: "<<g_wifiInfo.ssid;

    this->close();
}

void wifidetails::on_pushButton_Cancel_clicked()
{
    this->close();
}

