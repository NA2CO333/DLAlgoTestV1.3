#include "blockmousepress.h"
#include "ui_blockmousepress.h"

#include <QMouseEvent>

blockMousePress::blockMousePress(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::blockMousePress)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    this->setAttribute(Qt::WA_TranslucentBackground);

}

blockMousePress::~blockMousePress()
{
    delete ui;
}

void blockMousePress::mousePressEvent(QMouseEvent*e)
{
    this->close();
    qDebug("open backlight!!");

    e->accept();
    return;
}

void blockMousePress::keyPressEvent(QKeyEvent*)
{
    this->close();
    qDebug("open backlight!!");
    return;
}
