#include "updatedialog.h"
#include "ui_updatedialog.h"

#include <QSettings>

#include "windowsmanager.h"
#include "appsetting.h"
#include "global.h"

//
extern bool update_flag;

// 检查更新弹框
UpdateDialog::UpdateDialog(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::UpdateDialog)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setGeometry(100,90,600,300);

    ui->pushButtonUpdate->setMinimumSize(110,45);
    ui->pushButtonUpdate->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);");

    ui->pushButtonRemind->setMinimumSize(110,45);
    ui->pushButtonRemind->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);");

    ui->pushButtonIgnore->setMinimumSize(110,45);
    ui->pushButtonIgnore->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);");
}

UpdateDialog::~UpdateDialog()
{
    delete ui;
}

void UpdateDialog::paintEvent(QPaintEvent *event)
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

void UpdateDialog::showEvent(QShowEvent *)
{
    ui->labelContext->setText(tr("发现新版本,是否更新?"));   // "Discover new version, update?"
    ui->pushButtonUpdate->setText(tr("现在更新"));  // "Update now"
    ui->pushButtonRemind->setText(tr("下次提醒"));  // "Remind later"
    ui->pushButtonIgnore->setText(tr("不再提醒"));  // "Ignore"

}

void UpdateDialog::on_pushButtonUpdate_clicked()
{
    done(Update);
    update_flag = false;
}

void UpdateDialog::on_pushButtonRemind_clicked()
{
    done(Remind);
    update_flag = false;

    //WinUpdateSetup::setCfg_autoCheckUpdate(true);
}

void UpdateDialog::on_pushButtonIgnore_clicked()
{
    done(Ignore);
    update_flag = false;

    WinUpdateSetup::setCfg_autoCheckUpdate(false);
}
