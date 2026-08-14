#include "inputwifipwd.h"
#include "ui_inputwifipwd.h"

#include <QMessageBox>
#include <QDebug>

#include "windowsmanager.h"
#include "appsetting.h"
#include "global.h"

// wifi密码输入界面
inputWiFipwd::inputWiFipwd(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::inputWiFipwd)
{
    ui->setupUi(this);

    this->setAttribute(Qt::WA_TranslucentBackground);

    //
    ui->edtWifiName->setStyleSheet("border:1px solid rgb(200,200,200);border-radius:5px;padding:2px 4px;");
    ui->edtWifiPassword->setStyleSheet("border:1px solid rgb(200,200,200);border-radius:5px;padding:2px 4px;");

    ui->pushButton_Ok->setStyleSheet("border-radius:5px;padding:2px 4px;background-color:rgb(250,250,250);");
    ui->pushButton_Cancel->setStyleSheet("border-radius:5px;padding:2px 4px;;background-color:rgb(250,250,250);");

    //
    if (parent) {
        this->setGeometry((parent->width() - this->width()) / 2, (parent->height() - this->height()) / 2, this->width(), this->height());
    }

}

inputWiFipwd::~inputWiFipwd()
{
    delete ui;
}

void inputWiFipwd::showEvent(QShowEvent *)
{
    //if (language) {
    //    ui->lblWifiName->setText("网络名称:");
    //    ui->lblWifiPassword->setText("密码:");
    //    ui->pushButton_Ok->setText("确认");
    //    ui->pushButton_Cancel->setText("取消");
    //} else {
    //    ui->lblWifiName->setText("SSID:");
    //    ui->lblWifiPassword->setText("Password:");
    //    ui->pushButton_Ok->setText("OK");
    //    ui->pushButton_Cancel->setText("Cancel");
    //}
}

void inputWiFipwd::paintEvent(QPaintEvent *event)
{
    //make stylesheet working when setting background transparent
    QStyleOption opt;
    opt.init(this);
    QPainter pt(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &pt, this);

    pt.setRenderHint(QPainter::Antialiasing);  // 反锯齿;
    if(themeType_Black == getSysThemeType())
        pt.setBrush(QBrush(QColor(60, 60, 60, 220)));
    else
        pt.setBrush(QBrush(QColor(180, 180, 180, 220)));
    pt.setPen(Qt::transparent);
    QRect rect = this->rect();
    rect.setWidth(rect.width() - 1);
    rect.setHeight(rect.height() - 1);
    pt.drawRoundedRect(rect, 15, 15);

    QWidget::paintEvent(event);
}

void inputWiFipwd::EnterPwdText(const QString &_ssid, const QString &_pwd)
{
    //
    ui->edtWifiName->setText(_ssid);
    ui->edtWifiName->setEnabled(_ssid.isEmpty());

    ui->edtWifiPassword->setText(_pwd);

    //
    this->show();
    this->move ((SCREEN_WIDTH - this->width())/2,(SCREEN_HEIGHT - this->height())/2);

    this->setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体

}

//确认密码
void inputWiFipwd::on_pushButton_Ok_clicked()
{
    qDebug()<<"--inputWiFipwd::pbConfirmClicked()";

    //
    QString ssid = ui->edtWifiName->text();
    QString password = ui->edtWifiPassword->text();

    // 检测密码长度
    //if (password.length() >= 8)
    {
        emit sigGotPwd(ssid, password);   // 验证密码
        this->close();
    }
    //else {
    //    QMessageBox::about(this, tr("提示"), tr("密码长度须大于8位"));   // "tip"  "password length must greater than 8"
    //}
}

//取消
void inputWiFipwd::on_pushButton_Cancel_clicked()
{
    sendCancelFlag(true);

    this->close();
}
