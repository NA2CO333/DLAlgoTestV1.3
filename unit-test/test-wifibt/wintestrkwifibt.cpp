#include "wintestrkwifibt.h"
#include "ui_wintestrkwifibt.h"

#include <QDebug>
#include <QMessageBox>
#include <QMetaType>

#include "global.h"

//
CWinTestRkWifiBt::CWinTestRkWifiBt(QWidget *parent, CBluetoothIntf *_bt_intf) :
    QMainWindow(parent),
    ui(new Ui::WinTestRkWifiBt)
{
    ui->setupUi(this);

    this->setWindowFlag(Qt::FramelessWindowHint, true);

    //
    logDebug((QString("\n") + QString(__PRETTY_FUNCTION__) + ": currentThreadId: %1").arg((qintptr)QThread::currentThreadId()), CGlobal::LOG_BLUETOOTH);

    //
    btRkTest = new CBtRkTest(0, _bt_intf);

    QObject::connect(this, &CWinTestRkWifiBt::sig_initBt         , btRkTest, &CBtRkTest::slot_initBt         , Qt::QueuedConnection);
    QObject::connect(this, &CWinTestRkWifiBt::sig_uninitBt       , btRkTest, &CBtRkTest::slot_uninitBt       , Qt::QueuedConnection);
    QObject::connect(this, &CWinTestRkWifiBt::sig_openSpp        , btRkTest, &CBtRkTest::slot_openSpp        , Qt::QueuedConnection);
    QObject::connect(this, &CWinTestRkWifiBt::sig_closeSpp       , btRkTest, &CBtRkTest::slot_closeSpp       , Qt::QueuedConnection);
    QObject::connect(this, &CWinTestRkWifiBt::sig_openBleServer  , btRkTest, &CBtRkTest::slot_openBleServer  , Qt::QueuedConnection);
    QObject::connect(this, &CWinTestRkWifiBt::sig_closeBleServer , btRkTest, &CBtRkTest::slot_closeBleServer , Qt::QueuedConnection);
    QObject::connect(this, &CWinTestRkWifiBt::sig_openBleClient  , btRkTest, &CBtRkTest::slot_openBleClient  , Qt::QueuedConnection);
    QObject::connect(this, &CWinTestRkWifiBt::sig_closeBleClient , btRkTest, &CBtRkTest::slot_closeBleClient , Qt::QueuedConnection);

    //
    btRk = btRkTest->getBtRk();
    btRk->setName("test-hhy");

    btDatatrans = btRk->getBtDatatrans();
    QObject::connect(btDatatrans, &CBtConnection::sigReceivedData, this, &CWinTestRkWifiBt::slot_this_BleReceivedData, Qt::QueuedConnection);

    //
    //workThread = new QThread;
    //btRk->moveToThread(workThread);
    //btRkTest->moveToThread(workThread);
    //workThread->start();      // TODO: 这两个类在构造时 this->moveToThread() 是否正确？

}

CWinTestRkWifiBt::~CWinTestRkWifiBt()
{
    delete ui;
}

void CWinTestRkWifiBt::on_btnClose_clicked()
{
    this->hide();
}

void CWinTestRkWifiBt::on_btnBtInit_clicked()
{
    emit sig_initBt();
}

void CWinTestRkWifiBt::on_btnOpenSpp_clicked()
{
    emit sig_openSpp();
}

void CWinTestRkWifiBt::on_btnCloseSpp_clicked()
{
    emit sig_closeSpp();
}

void CWinTestRkWifiBt::on_btnOpenBleServer_clicked()
{
    emit sig_openBleServer();
}

void CWinTestRkWifiBt::on_btnCloseBleServer_clicked()
{
    emit sig_closeBleServer();
}

void CWinTestRkWifiBt::on_btnBtUninit_clicked()
{
    emit sig_uninitBt();
}

void CWinTestRkWifiBt::on_btnScan_clicked()
{
    QString msg;
    btRk->searchDevices(msg);
    if (msg.length() > 0) {
        QMessageBox::information(this, "msg", msg);
    }
}

void CWinTestRkWifiBt::on_btnSppConnect_clicked()
{
    QString msg;
    btRk->connectDevice(ui->edtAddr->text(), msg);
    if (msg.length() > 0) {
        QMessageBox::information(this, "msg", msg);
    }
}

void CWinTestRkWifiBt::on_btnGetIsOpened_clicked()
{
    bool is_opened = btRk->getIsOpened();
    QMessageBox::information(this, "msg", (is_opened ? "is opened" : "not opened"));
}

void CWinTestRkWifiBt::on_btnGetIsConnected_clicked()
{
    bool is_opened = btRk->getIsConnected();
    QMessageBox::information(this, "msg", (is_opened ? "is connected" : "not connected"));
}

void CWinTestRkWifiBt::on_btnGetAddr_clicked()
{
    QString addr = btRkTest->getAddrByApi();
    QMessageBox::information(this, "msg", addr);
}

void CWinTestRkWifiBt::on_btnGetBleSvrAddr_clicked()
{
    QString addr = btRkTest->getAddrBleServerByApi();
    QMessageBox::information(this, "msg", addr);
}

void CWinTestRkWifiBt::on_btnBleSend_clicked()
{
    btRk->getBtDatatrans()->sendData(ui->edtBleSend->text().toLatin1());
}

void CWinTestRkWifiBt::slot_this_BleReceivedData(QByteArray _data)
{
    ui->txtBleReceived->setText(ui->txtBleReceived->toPlainText() + _data);
}

void CWinTestRkWifiBt::on_btnSetName_clicked()
{
    btRk->setName(ui->edtBtName->text());
}
