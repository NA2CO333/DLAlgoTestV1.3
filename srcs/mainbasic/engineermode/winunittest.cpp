#include "winunittest.h"
#include "ui_winunittest.h"

#include <QMessageBox>

#include "windowsmanager.h"
#include "serialportdatatest.h"

#ifdef TEST_RKWIFIBT
#include "wintestrkwifibt.h"
#endif

//
SerialPortDataTest *winBaseboardSerail = Q_NULLPTR;

#ifdef TEST_RKWIFIBT
CWinTestRkWifiBt *winTestRkWifiBt = Q_NULLPTR;
#endif

//
CWinUnitTest::CWinUnitTest(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WinUnitTest)
{
    ui->setupUi(this);

    this->setWindowFlag(Qt::FramelessWindowHint, true);

}

CWinUnitTest::~CWinUnitTest()
{
    delete ui;
}

void CWinUnitTest::on_btnClose_clicked()
{
    this->hide();
}

// 显示底板串口调试窗口
void CWinUnitTest::on_btnTestBaseboardSerial_clicked()
{
    if (!winBaseboardSerail) {
        winBaseboardSerail = new SerialPortDataTest;
    }
    winBaseboardSerail->show();
}

void CWinUnitTest::on_btnTestRkWifiBt_clicked()
{
#ifdef TEST_RKWIFIBT
    if (!winTestRkWifiBt) {
        winTestRkWifiBt = new CWinTestRkWifiBt;
    }
    winTestRkWifiBt->show();
#else
    QMessageBox::information(this, "msg", "macro TEST_RKWIFIBT not defined");
#endif
}
