//主题切换
#include "themebackground.h"
#include "ui_themebackground.h"

#include <QDebug>

#include "windowsmanager.h"
#include "appsetting.h"
#include "global.h"

//
themebackground::themebackground(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::themebackground)
{
    ui->setupUi(this);

    isShowStatusBar = true;

}

themebackground::~themebackground()
{
    delete ui;
}

void themebackground::updateTheme(enThemeType _theme_type)
{
    //更新主题
    //QPalette palette;
    if (themeType_Black == _theme_type) {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        //设置所有QRadioButton样式
        QList<QRadioButton *> list_CheckBox = findChildren<QRadioButton *>();
        foreach(QRadioButton *p,list_CheckBox)
        {
            p->setStyleSheet("QRadioButton{background-color:rgb(1,1,1,1); color:rgb(204,204,204);}\
                                               QRadioButton::indicator:unchecked {image:url(:/resource/gray.png);width:25px;height:25px;border-radius:10px;}\
                                               QRadioButton::indicator:checked {image:url(:/resource/green.png);width:25px;height:25px;border-radius:10px;}");
            //p->setStyleSheet("QRadioButton{background-color:rgb(1,1,1,1); color:rgb(204,204,204);}");
        }
        //设置QLabel样式
        ui->label_Home->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Save->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");

        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/black_theme/save_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
        ui->radioButton_White->setChecked(false);
        ui->radioButton_Black->setChecked(true);
    }
    else if (themeType_White == _theme_type) {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        //设置所有QRadioButton样式
        QList<QRadioButton *> list_CheckBox = findChildren<QRadioButton *>();
        foreach(QRadioButton *p,list_CheckBox)
        {
            p->setStyleSheet("QRadioButton{background-color:rgb(255,255,255,1); color:rgb(1,1,1);}\
                                               QRadioButton::indicator:unchecked {image:url(:/resource/gray.png);width:25px;height:25px;border-radius:10px;}\
                                               QRadioButton::indicator:checked {image:url(:/resource/green.png);width:25px;height:25px;border-radius:10px;}");

            //p->setStyleSheet("QRadioButton{background-color:rgb(255,255,255,1); color:rgb(1,1,1);}");
        }

        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Save->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");

        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/white_theme/save_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
        ui->radioButton_White->setChecked(true);
        ui->radioButton_Black->setChecked(false);
    }
    //this->setPalette(palette);

    // 更新标题栏的主题
    WgtStatusBar::instance()->setCurrentThemeType(_theme_type);

}

void themebackground::showEvent(QShowEvent *)
{
    updateTheme(getSysThemeType());

    // 更新语言
    //if (language) {
    //    getWinManage()->updateWindowTitle(this, "主题");
    //
    //    ui->radioButton_Black->setText("黑色主题");
    //    ui->radioButton_White->setText("白色主题");
    //    ui->label_Home->setText("主页");
    //    ui->label_Save->setText("保存");
    //    ui->label_Back->setText("返回");
    //} else {
    //    getWinManage()->updateWindowTitle(this, "Theme");
    //
    //    ui->radioButton_Black->setText("Black Theme");
    //    ui->radioButton_White->setText("White Theme");
    //    ui->label_Home->setText("Home");
    //    ui->label_Save->setText("Save");
    //    ui->label_Back->setText("Back");
    //}
}

void themebackground::Save_prompt_dialog()
{
    /*
    int ret = QMessageBox::question(this,"question",text,QMessageBox::Yes,QMessageBox::No);
    if(ret == QMessageBox::Yes)
    {
        if(ui->radioButton_Black->isChecked())
            theme = 1;
        else if(ui->radioButton_White->isChecked())
            theme = 2;
    }
    else{}
    */
    QString text = tr("是否保存设置？");   // "Do you want to save Settings?"
    NoticeWin msg;
    msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    msg.setContent(text);
    if(msg.exec() == QDialog::Accepted)
    {
        if(ui->radioButton_Black->isChecked()){
            setSysThemeType(themeType_Black, true);
        }
        else if(ui->radioButton_White->isChecked()){
            setSysThemeType(themeType_White, true);
        }
        CBaseWindow::getInstance()->setTheme(getSysThemeType());
    }
    else{}
}
void themebackground::on_pushButton_Save_clicked()
{
    qDebug() << "---Set theme :" << getSysThemeType();
    Save_prompt_dialog();
}

void themebackground::on_pushButton_Home_clicked()
{
    if(themeType_Black == getSysThemeType()){
        theme_w_flag = false;
        theme_b_flag = true;
    }
    else{
        theme_w_flag = true;
        theme_b_flag = false;
    }

    if(theme_w_flag==ui->radioButton_White->isChecked() && theme_b_flag==ui->radioButton_Black->isChecked())
        getWinManage()->showWindowByType(WIN_HOME);
    else
    {
        Save_prompt_dialog();
        getWinManage()->showWindowByType(WIN_HOME);
    }
}

void themebackground::on_pushButton_Back_clicked()
{
    if(themeType_Black == getSysThemeType()){
        theme_w_flag = false;
        theme_b_flag = true;
    }
    else{
        theme_w_flag = true;
        theme_b_flag = false;
    }
    //this->reject();     //发送Cancel返回状态
    //getWinManage()->showWindowByType(WIN_TOOL);
    if(theme_w_flag==ui->radioButton_White->isChecked() && theme_b_flag==ui->radioButton_Black->isChecked())
        getWinManage()->backToLastWidget();
    else
    {
        Save_prompt_dialog();
        getWinManage()->backToLastWidget();
    }
}

void themebackground::on_radioButton_White_clicked()
{
    //
    updateTheme(themeType_White);

    //
    ui->radioButton_White->setChecked(true);
    ui->radioButton_Black->setChecked(false);
}

void themebackground::on_radioButton_Black_clicked()
{
    //
    updateTheme(themeType_Black);

    //
    ui->radioButton_White->setChecked(false);
    ui->radioButton_Black->setChecked(true);
}
