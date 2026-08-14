#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    edtImeTest = new QLineEdit(this);
    edtImeTest->setGeometry(10, 10, 200, 35);

}

Widget::~Widget()
{
    delete ui;
}

void Widget::showEvent(QShowEvent *_event)
{

}
