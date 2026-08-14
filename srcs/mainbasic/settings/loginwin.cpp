#include "loginwin.h"
#include "ui_loginwin.h"

#include <QMovie>

#include "windowsmanager.h"
#include "appsetting.h"
#include "global.h"

//
extern bool loginState;

//
loginWin::loginWin(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::loginWin)
{
    ui->setupUi(this);

    gLayout = new QGridLayout;
    vLayout = new QVBoxLayout;
    hLayout = new QHBoxLayout;

    UsrName = new myEditLine;
    PassWord = new myEditLine;

    UsrName->setVisible(true);
    PassWord->setVisible(true);

    gLayout->addWidget(ui->label_usr,0,0,1,1);
    gLayout->addWidget(UsrName,0,1,1,1);
    gLayout->addWidget(ui->label_password,1,0,1,1);
    gLayout->addWidget(PassWord,1,1,1,1);

    vLayout->addStretch(1);
    vLayout->addLayout(gLayout);
    vLayout->addStretch(1);
    hLayout->addStretch(1);
    hLayout->addLayout(vLayout);
    hLayout->addStretch(1);
    this->setLayout(hLayout);

//    mMovie = new QMovie(":/resource/evin.gif");
//    mMovie->setScaledSize(QSize(SCREEN_WIDTH, SCREEN_HEIGHT));
//    ui->label_2->setMovie(mMovie);

    //connect(UsrName,SIGNAL(textChanged(QString)),this,SLOT(slot_UsrName(QString)));       //(屏蔽)2020.7.9
    //connect(PassWord,SIGNAL(textChanged(QString)),this,SLOT(slot_PassWord(QString)));     //(屏蔽)2020.7.9

//    mSetting = new QSettings("manylinks",QSettings::IniFormat);
    QString _UsrName = appSetting::value("/login/usrname").toString();
    if(_UsrName != ""){
        UsrName->setText(_UsrName);
    }
    else{
        _UsrName = "admin";
        UsrName->setText(_UsrName);
        appSetting::setValue("/login/usrname",_UsrName);
        appSetting::setValue("/login/password","123456");
    }

    missCnt = appSetting::value("/login/misscnt").toInt();
    missBeginTime = appSetting::value("/login/missbegintime").toTime();

}

loginWin::~loginWin()
{
    delete ui;
}

void loginWin::showEvent(QShowEvent *)
{
    qDebug()<<"show loginWin";

    //更新语言
    //if (language) {
    //    ui->label_usr->setText("用户名：");
    //    ui->label_password->setText("密码：");
    //    ui->pushButton_confirm->setText("确认");
    //    ui->label_Home->setText("主页");
    //    ui->label_Back->setText("返回");
    //} else {
    //    ui->label_usr->setText("UserName:");
    //    ui->label_password->setText("Password:");
    //    ui->pushButton_confirm->setText("OK");
    //    ui->label_Home->setText("Home");
    //    ui->label_Back->setText("Back");
    //}

    QString strText;
    if(mode == login){
        strText = tr("手持视力筛查仪");    // "Handheld vision screener"
        ui->label->setText(strText);
        ui->label->adjustSize();
    }
    else if(mode == oldPassWord){
        strText = tr("请输入旧密码"); // "Please enter the old password"
        ui->label->setText(strText);
        ui->label->adjustSize();
    }
    else if(mode == newPassWord){
        strText = tr("请输入新密码"); // "Please enter a new password"
        ui->label->setText(strText);
        ui->label->adjustSize();
    }
    else if(mode == confirmPassWord){
        strText = tr("请再输入一次"); // "Please enter again"
        ui->label->setText(strText);
        ui->label->adjustSize();
    }

    ui->pushButton_Home->setVisible(login != mode);
    ui->label_Home->setVisible(login != mode);
    ui->pushButton_Back->setVisible(login != mode);
    ui->label_Back->setVisible(login != mode);

    //更新主题
    //QPalette palette;
    qDebug() << "=========LOGIN==========" << getSysThemeType();
    if(themeType_Black == getSysThemeType()){
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        this->setStyleSheet("QWidget {background-color: transparent; } ");

        //设置QLabel样式
        ui->label->setStyleSheet("color:rgb(204,204,204);");
        ui->label_usr->setStyleSheet("color:rgb(204,204,204);");
        ui->label_password->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Home->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");
        QFont font;
        font.setPointSize(15);
        UsrName->setFont(font);
        PassWord->setFont(font);
        UsrName->setStyleSheet("QLineEdit{height:30px; border-radius:3px; background-color:rgb(28,28,30); color:rgb(241,241,242);}");
        PassWord->setStyleSheet("QLineEdit{height:30px; border-radius:3px; background-color:rgb(28,28,30); color:rgb(250,250,252);}");
        ui->pushButton_confirm->setStyleSheet("QPushButton{border-radius:3px; background-color:rgb(51,56,62); color:rgb(241,241,242);}");
        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
    }
    else{
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        ui->label->setStyleSheet("color:rgb(1,1,1);");
        ui->label_usr->setStyleSheet("color:rgb(1,1,1);");
        ui->label_password->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");
        QFont font;
        font.setPointSize(15);
        UsrName->setFont(font);
        PassWord->setFont(font);
        UsrName->setStyleSheet("QLineEdit{height:30px; border-radius:3px; backgroudd-color:rgb(227,227,232); color:rgb(1,1,1);}");
        PassWord->setStyleSheet("QLineEdit{height:30px; border-radius:3px; backgroudd-color:rgb(227,227,232); color:rgb(1,1,1);}");
        ui->pushButton_confirm->setStyleSheet("QPushButton{border-radius:3px; background-color:rgb(200,200,200); color:rgb(1,1,1);}");
        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
    }
    //this->setPalette(palette);
}

void loginWin::setMode(workMode _mode)
{
    mode = _mode;
}

void loginWin::on_pushButton_confirm_clicked()
{
    QString strText;
    if(mode == login || mode == oldPassWord){
        QString _UsrNameText = UsrName->text();
                QString _UsrName = appSetting::value("/login/usrname").toString();

                if(_UsrNameText == _UsrName){
                    qDebug()<<"correct usrname!";
                }
                else{
                    MessageWin  msg;
                    strText = tr("请输入正确用户名!");  // "Please enter the correct username!"
                    msg.setContent(strText);
                    msg.exec();
                }


                QString _PassWordText = PassWord->text();
                if(missCnt >= 5){
                    int timeDiff = missBeginTime.secsTo(QTime::currentTime());
                    int minLeft = 10 - (timeDiff / 60);

                    if(minLeft > 0){
                        MessageWin  msg;
                        QString text = tr("密码错误次数超5次\n请 %1 分钟后再试！").arg(minLeft);   // "Enter the wrong password more than 5 times\nPlease try again after %1 minutes!"

                        msg.setContent(text);
                        msg.exec();
                        return;
                    }
                    else{
                        missCnt = 0;
                        appSetting::setValue("/login/misscnt",missCnt);
                    }

                }

                QString _PassWord = appSetting::value("/login/password").toString();

                if(_PassWordText == _PassWord){
                    qDebug()<<"password correct!";
                    missCnt = 0;
                    appSetting::setValue("/login/misscnt",missCnt);

                    if(mode == login){
                        getWinManage()->showWindowByType(WIN_HOME);
                    }else if(mode == oldPassWord){
                        mode = newPassWord;
                        strText = tr("请输入新密码"); // "Please enter a new password"
                        ui->label->setText(strText);
                        ui->label->adjustSize();
                    }

                }
                else{
                    MessageWin  msg;
                    strText = tr("密码错误");   // "Wrong password"
                    msg.setContent(strText);
                    msg.exec();
                    missCnt++;
                    appSetting::setValue("/login/misscnt",missCnt);

                    if(missCnt >= 5){
                        missBeginTime = QTime::currentTime();
                        appSetting::setValue("/login/missbegintime",missBeginTime);
                    }
                }

                PassWord->clear();
                if(mode == oldPassWord){
                    mode == newPassWord;
                    this->update();
                }
    }
    else if(mode == newPassWord){
        strText = tr("请再输入一次"); // "Please enter again"
        ui->label->setText(strText);
        ui->label->adjustSize();
        QString _UsrName = UsrName->text();
        QString _Password = PassWord->text();

        if(_UsrName.length() < 2){
            MessageWin msg;
            strText = tr("请输入合法用户名");   // "Please enter a valid username"
            msg.setContent(strText);
            msg.exec();
        }
        if(_Password.length() < 2){
            MessageWin msg;
            strText = tr("请输入至少3位密码");  // "Please enter at least 3 digits password"
            msg.setContent(strText);
            msg.exec();
        }

        tempPassword = _Password;
        tempUsrName = _UsrName;

        mode = confirmPassWord;
        PassWord->clear();
    }
    else if(mode == confirmPassWord){
        QString _UsrName = UsrName->text();
        QString _Password = PassWord->text();

        if(_UsrName.length() < 2){
            MessageWin msg;
            strText = tr("请输入合法用户名");   // "Please enter a valid username"
            msg.setContent(strText);
            msg.exec();
        }
        if(_Password.length() < 2){
            MessageWin msg;
            strText = tr("请输入至少3位密码");  // "Please enter at least 3 digits password"
            msg.setContent(strText);
            msg.exec();
        }

        appSetting::setValue("/login/usrname",_UsrName);
        appSetting::setValue("/login/password",_Password);

        MessageWin msg;
        strText = tr("修改成功!");  // "Successfully modified!"
        msg.setContent(strText);
        msg.exec();

        PassWord->clear();
        getWinManage()->showWindowByType(WIN_SET);
    }
}

void loginWin::on_pushButton_Back_clicked()
{
    //this->close();
    getWinManage()->showWindowByType(WIN_SET);
}

void loginWin::on_pushButton_Home_clicked()
{
    //
}
