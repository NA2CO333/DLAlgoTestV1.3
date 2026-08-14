// by wim

#include "progresswindow.h"
#include "ui_progresswindow.h"

#include <QPainter>
#include <QStyleOption>

#include "global.h"

//
ProgressWindow::ProgressWindow(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::ProgressWindow)
{
    ui->setupUi(this);
    ui->progressBar->setValue(0);

    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setGeometry(100,120,600,240);

    //if (language) {
    //    ui->contextLabel->setText("正在导出");
    //    //ui->noticeLabel->setText("请不要进行操作\n以免中断导出过程");
    //} else {
    //    ui->contextLabel->setText("Exporting");
    //    //ui->noticeLabel->setText("Please do not operate\nto avoid interrupting the export process");
    //}

    ui->noticeLabel->setVisible(false);

}

ProgressWindow::~ProgressWindow()
{
    delete ui;
}

void ProgressWindow::setProgress(int progress)
{
    ui->progressBar->setValue(progress);
}

void ProgressWindow::setContext(QString text)
{
    ui->contextLabel->setText(text);
}

void ProgressWindow::setNotice(QString text)
{
    ui->noticeLabel->setText(text);
}

void ProgressWindow::paintEvent(QPaintEvent *event)
{
    //make stylesheet working when setting background transparent
        QStyleOption opt;
        opt.init(this);
        QPainter pt(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &pt, this);

        pt.setRenderHint(QPainter::Antialiasing);  // 反锯齿;
        pt.setBrush(QBrush(QColor(60, 60, 60, 170)));
        pt.setPen(Qt::transparent);
        QRect rect = this->rect();
        rect.setWidth(rect.width() - 1);
        rect.setHeight(rect.height() - 1);
        pt.drawRoundedRect(rect, 15, 15);

        QWidget::paintEvent(event);
}
