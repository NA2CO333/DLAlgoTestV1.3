#include "winledposidetect.h"
#include "ui_winledposidetect.h"

CWinLedPosiDetect::CWinLedPosiDetect(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CWinLedPosiDetect)
{
    ui->setupUi(this);
}

CWinLedPosiDetect::~CWinLedPosiDetect()
{
    delete ui;
}
