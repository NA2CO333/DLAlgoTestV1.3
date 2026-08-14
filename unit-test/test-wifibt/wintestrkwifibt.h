#ifndef WINTESTRKWIFIBT_H
#define WINTESTRKWIFIBT_H

#include <QMainWindow>

#include <QThread>

#include "bluetoothrk.h"

namespace Ui {
class WinTestRkWifiBt;
}

//
class CWinTestRkWifiBt : public QMainWindow
{
    Q_OBJECT

public:
    explicit CWinTestRkWifiBt(QWidget *parent = nullptr, CBluetoothIntf *_bt_intf = 0);
    ~CWinTestRkWifiBt();

signals:
    void sigHidden();

    void sig_initBt         ();
    void sig_uninitBt       ();
    bool sig_openSpp        ();
    void sig_closeSpp       ();
    bool sig_openBleServer  ();
    void sig_closeBleServer ();
    bool sig_openBleClient  ();
    void sig_closeBleClient ();

private slots:
    void on_btnClose_clicked();

    void on_btnBtInit_clicked();
    void on_btnOpenSpp_clicked();
    void on_btnCloseSpp_clicked();
    void on_btnOpenBleServer_clicked();
    void on_btnCloseBleServer_clicked();
    void on_btnBtUninit_clicked();

    void on_btnScan_clicked();
    void on_btnSppConnect_clicked();
    void on_btnGetIsOpened_clicked();
    void on_btnGetIsConnected_clicked();

    void on_btnGetAddr_clicked();
    void on_btnGetBleSvrAddr_clicked();
    void on_btnBleSend_clicked();

    void slot_this_BleReceivedData(QByteArray _data);

    void on_btnSetName_clicked();

private:
    Ui::WinTestRkWifiBt *ui;

    CBtRkTest *btRkTest = Q_NULLPTR;
    CBluetoothRk *btRk = Q_NULLPTR;
    QThread *workThread = Q_NULLPTR;

    CBtConnection *btDatatrans = Q_NULLPTR;

};

#endif // WINTESTRKWIFIBT_H
