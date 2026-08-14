#include "combobox.h"

#include <QScroller>

CComboBox::CComboBox(QWidget *parent) : QComboBox(parent)
{
    listView = new QListView();
    this->setView(listView);

}

CComboBox::~CComboBox()
{
    listView->deleteLater();
    listView = Q_NULLPTR;
}

void CComboBox::enableGrabGesture()
{
    QScroller::grabGesture(listView->viewport(), QScroller::LeftMouseButtonGesture);

}

void CComboBox::showEvent(QShowEvent *)
{
    if (!isListViewInit) {
        listView->setFont(this->font());
        isListViewInit = true;
    }
}
