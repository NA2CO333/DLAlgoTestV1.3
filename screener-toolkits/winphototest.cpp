#include "winphototest.h"
#include "ui_winphototest.h"

#include "global.h"

WinPhotoTest::WinPhotoTest(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WinPhotoTest)
{
    ui->setupUi(this);
}

WinPhotoTest::~WinPhotoTest()
{
    delete ui;
}

void WinPhotoTest::on_btnStart_clicked()
{
    static QString STR_START    = "开始";
    static QString STR_STOP     = "结束";

    bool is_start = (ui->btnStart->text() == STR_START);
    if (is_start) {


        //
        ui->btnStart->setText(STR_STOP);
    } else {


        //
        ui->btnStart->setText(STR_START);
    }
}

void WinPhotoTest::on_btnPause_clicked()
{
    static QString STR_PAUSE    = "暂停";
    static QString STR_CONTINUE = "继续";

    bool is_pause = (ui->btnStart->text() == STR_PAUSE);
    if (is_pause) {


        //
        ui->btnStart->setText(STR_CONTINUE);
    } else {


        //
        ui->btnStart->setText(STR_PAUSE);
    }

}

void WinPhotoTest::on_btnGoBack_clicked()
{
    global->showWinHome();
}
