#include "engineerpassword.h"
#include "ui_engineerpassword.h"

#include "windowsmanager.h"
#include "appsetting.h"
#include "baseform.h"
#include "global.h"

//
QElapsedTimer engineerpassword::s_elapsedChecked;

engineerpassword::engineerpassword(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::engineerpassword)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_TranslucentBackground);
    rootPwd = new myEditLine;
    engineerPwd = new myEditLine;
    rootPwd->setMinimumSize(230,40);
    rootPwd->setStyleSheet("border:1px solid rgb(200,200,200);border-radius:5px;padding:2px 4px;");
    engineerPwd->setMinimumSize(230,40);
    engineerPwd->setStyleSheet("border:1px solid rgb(200,200,200);border-radius:5px;padding:2px 4px;");
    ui->horizontalLayout->addWidget(rootPwd);
    ui->horizontalLayout_2->addWidget(engineerPwd);
    ui->pushButton_Ok->setStyleSheet("border-radius:5px;padding:2px 4px;background-color:rgb(250,250,250);");
    ui->pushButton_Alter->setStyleSheet("border-radius:5px;padding:2px 4px;background-color:rgb(250,250,250);");
    ui->pushButton_Cancel->setStyleSheet("border-radius:5px;padding:2px 4px;;background-color:rgb(250,250,250);");


    ui->label_Name->hide();
    rootPwd->hide();
    showPwdEdit = false;
}

engineerpassword::~engineerpassword()
{
    delete ui;
}

qint64 engineerpassword::elapsedChecked()
{
    if (s_elapsedChecked.isValid()) {
        return s_elapsedChecked.elapsed();
    } else {
        return -1;
    }
}

void engineerpassword::paintEvent(QPaintEvent *event)
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

void engineerpassword::showEvent(QShowEvent *event)
{
    //
    CBaseFormIntf::centerWidget(this);

    //
    if(themeType_Black == getSysThemeType())
        ui->pushButton_Alter->setStyleSheet("QPushButton{border-radius:5px; background-color:rgb(60,60,60,220); color:rgb(100,100,100);}");
    else
        ui->pushButton_Alter->setStyleSheet("QPushButton{border-radius:5px; background-color:rgb(180,180,180,220); color:rgb(100,100,100);}");

    //
    //if (language) {
    //    ui->label_Name->setText("二级密码:");
    //    ui->label_Pwd->setText("密        码:");
    //    ui->pushButton_Ok->setText("确认");
    //    ui->pushButton_Alter->setText("修改密码");
    //    ui->pushButton_Cancel->setText("取消");
    //} else {
    //    ui->label_Name->setText("Secondary Password:");
    //    ui->label_Pwd->setText("Password:");
    //    ui->pushButton_Alter->setText("Change Password");
    //    ui->pushButton_Ok->setText("OK");
    //    ui->pushButton_Cancel->setText("Cancel");
    //}

    //
    this->setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    showPwdEdit = false;

}

void engineerpassword::keyPressEvent(QKeyEvent *_evt)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
    CBaseWidget::keyPressEvent(_evt);

    //
    if (_evt->key() == Qt::Key::Key_Enter || _evt->key() == Qt::Key::Key_Return) {
        on_pushButton_Ok_clicked();
    }
}

void engineerpassword::on_pushButton_Ok_clicked()
{
    QString ePassword = appSetting::value("/tool/enginPassword").toString();
    qDebug()<<"-----"<<ePassword;
    QString message,buttonText;
    if(engineerPwd->text() == ePassword){
        this->close();
        s_elapsedChecked.start();
        getWinManage()->showWindowByType(WIN_ENGIN);
    }
    else if(engineerPwd->text() == ""){
        message = tr("请输入密码");  // "Please enter password?"
        buttonText = tr("确认");  // "OK"

        MessageWin mess;
        mess.setContent(message);
        mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
        mess.setButtonText(buttonText);
        if (mess.exec() == QDialog::Accepted) {
            //
        }
    }
    else{
        message = tr("密码错误!");  // "Password error"
        buttonText = tr("确认");  // "OK"

        MessageWin mess;
        mess.setContent(message);
        mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
        mess.setButtonText(buttonText);
        if(mess.exec() == QDialog::Accepted){}
    }
}

void engineerpassword::on_pushButton_Alter_clicked()
{
    ui->label_Name->show();
    rootPwd->show();
    if(showPwdEdit){
        QString rPassword = appSetting::value("/tool/rootPassword").toString();
        qDebug()<<"-----"<<rPassword;
        QString message,buttonText;
        if(rootPwd->text() == rPassword){
            if(engineerPwd->text() == ""){
                message = tr("请输入密码");  // "Please enter password"
                buttonText = tr("确认");  // "OK"

                MessageWin mess;
                mess.setContent(message);
                mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
                mess.setButtonText(buttonText);
                if(mess.exec() == QDialog::Accepted){}
            }
            else{
                message = tr("密码修改成功\n新密码:") + engineerPwd->text(); // "Password changed successfully\nnew password:"
                buttonText = tr("确认");  // "OK"

                MessageWin mess;
                mess.setContent(message);
                mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
                mess.setButtonText(buttonText);
                if(mess.exec() == QDialog::Accepted)
                    appSetting::setValue("/tool/enginPassword",engineerPwd->text());
            }
        }
        else if (rootPwd->text() == "") {
            message = tr("请输入二级密码");    // "Please enter the Secondary password"
            buttonText = tr("确认");  // "OK"

            MessageWin mess;
            mess.setContent(message);
            mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
            mess.setButtonText(buttonText);
            if(mess.exec() == QDialog::Accepted){}
        }
        else{
            message = tr("二级密码错误!");    // "Administrator password error"
            buttonText = tr("确认");  // "OK"

            MessageWin mess;
            mess.setContent(message);
            mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
            mess.setButtonText(buttonText);
            if(mess.exec() == QDialog::Accepted){}
        }
    }
    showPwdEdit = true;
}

void engineerpassword::on_pushButton_Cancel_clicked()
{
    rootPwd->clear();
    engineerPwd->clear();
    this->close();

}
