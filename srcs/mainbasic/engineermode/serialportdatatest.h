#ifndef SERIALPORTDATATEST_H
#define SERIALPORTDATATEST_H

#include <QWidget>
#include <QThread>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QVBoxLayout>

#include "baseform.h"
#include "myserialport.h"
#include "myeditline.h"
#include "qrcodeinput.h"

namespace Ui {
class SerialPortDataTest;
}

class SerialPortDataTest : public CBaseWidget
{
    Q_OBJECT

public:
    explicit SerialPortDataTest(QWidget *parent = 0);
    ~SerialPortDataTest();

    bool stat_stop;
    bool test_power;
    bool infrared;
    bool beep;
    bool wifi_flag;
    bool usb_port_flag;
    bool camera_flag;

public slots:
    void serailportValue(int _cmd_id, QByteArray _pkg_data);

protected:
    void showEvent(QShowEvent *_evt) override;
    void hideEvent(QHideEvent *_evt) override;

    myEditLine *lineEdit_Send;
    QVBoxLayout *vLayout;
    QVBoxLayout *vLayoutLine1_Line2;
    QHBoxLayout *hLayoutmA;
    QHBoxLayout *hLayout1;
    QHBoxLayout *hLayout2;
    QHBoxLayout *hLayout3;
    QDateTime  *strtime;
    QTimer *timer;

protected slots:
    void slot_timerTimeout();
    //void Recv_Date();

private slots:
    void on_pushButton_Send_clicked();
    void on_pushButton_Back_clicked();
    void on_pushButton_Recv_Stop_clicked();
    void on_pushButton_Clear_clicked();
    void on_pushButton_Export_clicked();
    void on_pushButton_Save_clicked();
    void on_pushButton_Delete_clicked();
    void on_pushButton_1_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();
    void on_pushButton_8_clicked();
    void on_pushButton_6_clicked();
    void on_pushButton_7_clicked();
private:
    Ui::SerialPortDataTest *ui;
};

#endif // SERIALPORTDATETEST_H
