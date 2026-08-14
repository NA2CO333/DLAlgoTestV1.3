#include "winlog.h"
#include "ui_winlog.h"

//#include <QScroller>

//#include "remoteservice.h"

//
WinLog::WinLog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WinLog)
{
    ui->setupUi(this);

    //
    //QScroller::grabGesture(ui->txtLog->viewport(), QScroller::LeftMouseButtonGesture);        // TODO: 为什么会滑动太快？
    ui->txtLog->setReadOnly(true);

}

WinLog::~WinLog()
{
    delete ui;
}

void WinLog::showEvent(QShowEvent *)
{
    //
    this->setAutoFillBackground(true);


}

void WinLog::hideEvent(QHideEvent *)
{

}

void WinLog::slot_remoteService_Log(Net::Remote::enLogType _log_type, QString _log_msg)
{
    if (Net::Remote::logType_error == _log_type) {
        _log_msg = "===============错误：\n" + _log_msg;
    }

    ui->txtLog->appendPlainText(_log_msg);
    ui->txtLog->ensureCursorVisible();
}

void WinLog::on_btnClose_clicked()
{
    this->hide();
}

void WinLog::on_btnClear_clicked()
{
    ui->txtLog->clear();
}
