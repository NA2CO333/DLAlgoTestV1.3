#include "dialoglistselect.h"
#include "ui_dialoglistselect.h"

#include <QListWidget>
#include <QCheckBox>
#include <QPainter>

#include "mainwindow.h"
#include "global.h"

CDialogListSelect::CDialogListSelect(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::CDialogListSelect)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setGeometry(100, 40, 600, 400);   // TODO: 自动设置函数

    ui->btnCancel->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);color:black;");
    ui->btnOK->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);color:black;");

    ui->lstFiles->setStyleSheet("background-color:rgb(128,128,128)");

    ui->btnCancel->hide();

}

CDialogListSelect::~CDialogListSelect()
{
    delete ui;
}

void CDialogListSelect::showEvent(QShowEvent *)
{
    CBaseFormIntf::centerWidget(this);

}

void CDialogListSelect::paintEvent(QPaintEvent *event)
{
    //make stylesheet working when setting background transparent
    QStyleOption opt;
    opt.init(this);
    QPainter pt(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &pt, this);

    pt.setRenderHint(QPainter::Antialiasing);  // 反锯齿;
    pt.setBrush(QBrush(QColor(64, 64, 64, 200)));
    pt.setPen(Qt::transparent);
    QRect rect = this->rect();
    rect.setWidth(rect.width() - 1);
    rect.setHeight(rect.height() - 1);
    pt.drawRoundedRect(rect, 15, 15);

    QWidget::paintEvent(event);
}

void CDialogListSelect::insertRow(QString _str, bool _checked)
{
    logDebug("CDialogListSelect::insertRow() _str=" + _str);
    QListWidgetItem * lwi = new QListWidgetItem();
    lwi->setSizeHint(QSize(500, 46));
    QCheckBox * ckb = new QCheckBox(_str);
    ckb->setStyleSheet("margin:2px 14px;background-color:rgb(128,128,128);color:white;");
    ckb->setChecked(_checked);
    ui->lstFiles->addItem(lwi);
    ui->lstFiles->setItemWidget(lwi, ckb);
}

void CDialogListSelect::on_btnCancel_clicked()
{
    this->reject();
}

void CDialogListSelect::on_btnClose_clicked()
{
    this->reject();
}

void CDialogListSelect::on_btnOK_clicked()
{
    for (int i = 0; i < ui->lstFiles->count(); i++)
    {
        QListWidgetItem * item = ui->lstFiles->item(i);

        QWidget * widget = ui->lstFiles->itemWidget(item);
        QCheckBox * box = (QCheckBox*)widget;
        if (box->isChecked())
            this->fileList.append(box->text());
    }

    this->accept();
}

