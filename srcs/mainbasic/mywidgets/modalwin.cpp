#include "modalwin.h"

CModalWin::CModalWin(QWidget *parent) : CWidgetClickable(parent)
{
    this->setVisible(false);
    this->setStyleSheet("QWidget { background: transparent; }");

    QObject::connect(this, &CModalWin::clicked, this, &CModalWin::slot_this_clicked, Qt::QueuedConnection);

}

void CModalWin::showEvent(QShowEvent *)
{
    this->raise();

    QWidget *parent = dynamic_cast<QWidget *>(this->parent());
    if (parent) {
        this->setGeometry(0, 0, parent->width(), parent->height());
    }
}

void CModalWin::slot_this_clicked()
{
    this->setVisible(false);
}
