//万年历界面,日期时间设置以及获取网络时间
#include "datepage.h"
#include "ui_datepage.h"

#include <time.h>

#include <QDateTimeEdit>
#include <QDateTime>
#include <QApplication>
#include <QProcess>
#include <QDebug>
#include <QCalendarWidget>
#include <QToolButton>
#include <QPainter>
#include <QDebug>
#include <QPen>
#include <QStringList>

#include "mainwindow.h"
#include "windowsmanager.h"
#include "global.h"
#include "hardware.h"

//
int Timer_Count=0;
extern bool Get_Network_Time_flag;
bool Starting_Up_Get_Time_Count = true;     //开机只获取一次网络时间

//
datePage::datePage(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::datePage)
{
    ui->setupUi(this);

    isShowStatusBar = true;

//    this->ui->timeEdit->setDateTime(QDateTime::currentDateTime());
//    this->ui->timeEdit->setDisplayFormat("HH:mm");

//    calendarWidget->setLocale(QLocale(QLocale::Chinese,QLocale::China));

    // 小时下拉列表的初始化
    QStringList hours;
    for (int i = 0; i < 24; i++) {
        if(i < 10)
            hours.append("0" + QString::number(i, 10));
        else
            hours.append(QString::number(i, 10));
    }
    ui->cbbHour->addItems(hours);

    // 分钟下拉列表的初始化
    QStringList mins;
    for (int i = 0; i < 60; i++) {
        if (i < 10)
            mins.append("0"+ QString::number(i, 10));
        else
            mins.append(QString::number(i, 10));
    }
    ui->cbbMinute->addItems(mins);

    // 月份下拉列表的初始化
    QStringList months = QStringList() << "01" << "02" << "03" << "04" << "05" << "06" << "07" << "08" << "09" << "10" << "11" << "12";
    ui->cbbMonth->clear();
    ui->cbbMonth->addItems(months);

    // 年份下拉列表的初始化
    QStringList years;
    for (int i = 2016; i <= 2050; i++) {
        years.append(QString::number(i));
    }
    ui->cbbYear->clear();
    ui->cbbYear->addItems(years);

    //
    ui->cbbYear->enableGrabGesture();
    ui->cbbMonth->enableGrabGesture();
    ui->cbbHour->enableGrabGesture();
    ui->cbbMinute->enableGrabGesture();

}

datePage::~datePage()
{
    delete ui;
}

// 设置 业务数据 到 UI 部件
void datePage::setBusiDataToUi(QDateTime _date_time)
{
    dateTimeEditing = _date_time;

    //
    int year    = dateTimeEditing.date().year();
    int month   = dateTimeEditing.date().month();
    int day     = dateTimeEditing.date().day();
    int hour    = dateTimeEditing.time().hour();
    int minute  = dateTimeEditing.time().minute();

    setCbbYearValue(year);
    setCbbMonthValue(month);

    setCbbHourValue(hour);
    setCbbMinuteValue(minute);

    ui->calendarWidget->setSelectedDate(QDate(year, month, day));

}

void datePage::setCbbYearValue(int _year)
{
    ui->cbbYear->setCurrentIndex(ui->cbbYear->findText(QString("%1").arg(_year, 4, 10, QLatin1Char('0'))));
}

void datePage::setCbbMonthValue(int _month)
{
    ui->cbbMonth->setCurrentIndex(ui->cbbMonth->findText(QString("%1").arg(_month, 2, 10, QLatin1Char('0'))));
}

void datePage::setCbbHourValue(int _hour)
{
    ui->cbbHour->setCurrentIndex(ui->cbbHour->findText(QString("%1").arg(_hour, 2, 10, QLatin1Char('0'))));
}

void datePage::setCbbMinuteValue(int _minute)
{
    ui->cbbMinute->setCurrentIndex(ui->cbbMinute->findText(QString("%1").arg(_minute, 2, 10, QLatin1Char('0'))));
}

// 检查是否需要保存
bool datePage::checkIsNeedSave()
{
    bool is_same = false;

    //
    int year    = dateTimeEditing.date().year();
    int month   = dateTimeEditing.date().month();
    int day     = dateTimeEditing.date().day();
    int hour    = dateTimeEditing.time().hour();
    int minute  = dateTimeEditing.time().minute();

    //
    do {
        if (QDate(year, month, day) != ui->calendarWidget->selectedDate())
            break;

        if (ui->cbbHour->currentText().toInt() != hour)
            break;

        if (ui->cbbMinute->currentText().toInt() != minute)
            break;

        //
        is_same = true;
    } while (false);

    return (!is_same);
}

void datePage::showEvent(QShowEvent *)
{
    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    //更新主题
    //QPalette palette;
    if (themeType_Black == getSysThemeType()) {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        ui->calendarWidget->setStyleSheet("QCalendarWidget QWidget {background-color: rgb(128, 128, 128); alternate-background-color: rgb(96,96,96);}");

        QString style_combobox = R"(
QComboBox{ background-color: rgb(51,56,62); color: rgb(204,204,204); }
QComboBox::drop-down { image: url(:/resource/black_theme/combo-arrow-down_b.png); }
QComboBox QAbstractItemView { background-color: rgb(51,56,62); color: rgb(204,204,204); border: 1px solid rgb(149,149,149); }
QComboBox QAbstractItemView::item { min-height: 40px; }
QComboBox QAbstractItemView::item:selected { background-color: rgb(48,140,198); color: rgb(255,255,255); }
                )";

        ui->cbbYear->setStyleSheet(style_combobox);
        ui->cbbHour->setStyleSheet(style_combobox);
        ui->cbbMonth->setStyleSheet(style_combobox);
        ui->cbbMinute->setStyleSheet(style_combobox);

        //设置QLabel样式
        ui->label_Home->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Save->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");
        ui->label_year->setStyleSheet("color:rgb(204,204,204);");
        ui->label_month->setStyleSheet("color:rgb(204,204,204);");
        ui->label_hour->setStyleSheet("color:rgb(204,204,204);");
        ui->label_min->setStyleSheet("color:rgb(204,204,204);");

        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/black_theme/save_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
    }
    else{
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        ui->calendarWidget->setStyleSheet("QCalendarWidget{background-color:rgb(249,249,251); color:rgb(185,111,111);}");
        ui->cbbYear->setStyleSheet("QComboBox{border-radius:5px; background-color:rgb(227,227,232); color:rgb(1,1,1);}");
        ui->cbbHour->setStyleSheet("QComboBox{border-radius:5px; background-color:rgb(227,227,232); color:rgb(1,1,1);}");
//        if(language){
//            ui->cbbMonth->setMinimumWidth(90);
//            ui->cbbMonth->setMaximumWidth(90);
//        }
//        else{
//            ui->cbbMonth->setMinimumWidth(135);
//            ui->cbbMonth->setMaximumWidth(135);
//        }
        ui->cbbMonth->setStyleSheet("QComboBox{border-radius:5px; background-color:rgb(227,227,232); color:rgb(1,1,1);}");
        ui->cbbMinute->setStyleSheet("QComboBox{border-radius:5px; background-color:rgb(227,227,232); color:rgb(1,1,1);}");

        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Save->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");
        ui->label_year->setStyleSheet("color:rgb(1,1,1);");
        ui->label_month->setStyleSheet("color:rgb(1,1,1);");
        ui->label_hour->setStyleSheet("color:rgb(1,1,1);");
        ui->label_min->setStyleSheet("color:rgb(1,1,1);");

        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/white_theme/save_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
    }
    //this->setPalette(palette);

    //
    getWinManage()->updateWindowTitle(this, tr("日期"));  // "Date"

    if (G_LANGUAGE_CHINESE == CGlobal::language) {
        ui->calendarWidget->setLocale(QLocale::Chinese);
    } else {
        ui->calendarWidget->setLocale(QLocale::English);
    }

    //更新语言
    //if (language) {
    //    //ui->radioButton_Black->setText("黑色主题");
    //    //ui->radioButton_White->setText("白色主题");
    //    ui->label_year->setText("年");
    //    //ui->label_month->show();
    //    ui->label_month->setText("月");
    //    ui->label_hour->setText("时");
    //    ui->label_min->setText("分");
    //    ui->label_Home->setText("主页");
    //    ui->label_Save->setText("保存");
    //    ui->label_Back->setText("返回");
    //} else {
    //    //ui->radioButton_Black->setText("Black Theme");
    //    //ui->radioButton_White->setText("White Theme");
    //    ui->label_year->setText("Year");
    //    ui->label_month->setText("Month");
    //    ui->label_hour->setText("Hour");
    //    ui->label_min->setText("Min");
    //    ui->label_Home->setText("Home");
    //    ui->label_Save->setText("Save");
    //    ui->label_Back->setText("Back");
    //}

    //
    ui->calendarWidget->setFirstDayOfWeek(Qt::Monday);  //第一列显示星期一

    //
    setBusiDataToUi(QDateTime::currentDateTime());

}

void datePage::paintEvent(QPaintEvent *)
{
    /*
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing,true);
    painter.setPen(QPen(QColor(7,232,250),2));
    painter.setBrush(QColor(7,232,250));
    painter.drawRect(20, 30, 760, 40);
    */
}

// 保存的提示及处理
void datePage::Save_prompt_dialog()
{
    QString text = tr("是否保存设置？");   // "Do you want to save Settings?"
    NoticeWin msg;
    msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    msg.setContent(text);
    if(msg.exec() == QDialog::Accepted)
    {
        qDebug() << "saving edited date time";
        QDate date = ui->calendarWidget->selectedDate();
        QTime time(ui->cbbHour->currentText().toInt(), ui->cbbMinute->currentText().toInt());

        QDateTime date_time(date, time);
        qDebug() << "datePage::Save_prompt_dialog(): got new time = " << date_time.toString();
        CHardware::setMachineDateTime(date_time);
    }
}

//
void datePage::on_pushButton_Save_clicked()
{
    Save_prompt_dialog();
}

// 年份下拉框当前项改变事件
void datePage::on_cbbYear_activated(int _idx)
{
    Q_UNUSED(_idx)
    doAfterYearMonthChanged();
}

// 月份下拉框当前项改变事件
void datePage::on_cbbMonth_activated(int _idx)
{
    Q_UNUSED(_idx)
    doAfterYearMonthChanged();
}

void datePage::doAfterYearMonthChanged()
{
    int year = ui->cbbYear->currentText().toInt();
    int month = ui->cbbMonth->currentText().toInt();
    int day = ui->calendarWidget->selectedDate().day();

    QDate date(year, month, day);
    ui->calendarWidget->setSelectedDate(date);
    ui->calendarWidget->showSelectedDate();

    logDebug(QString("set calendar to ") + ui->calendarWidget->selectedDate().toString(), CGlobal::LOG_SYS);
}

void datePage::on_calendarWidget_clicked(const QDate &_date)
{
    setCbbYearValue(_date.year());
    setCbbMonthValue(_date.month());
}

void datePage::on_pushButton_Back_clicked()
{
    if (!checkIsNeedSave()) {
        getWinManage()->showWindowByType(WIN_TOOL);
    } else {
        Save_prompt_dialog();
        getWinManage()->showWindowByType(WIN_TOOL);
    }
}

void datePage::on_pushButton_Home_clicked()
{
    if (!checkIsNeedSave()) {
        getWinManage()->showWindowByType(WIN_HOME);
    } else {
        Save_prompt_dialog();
        getWinManage()->showWindowByType(WIN_HOME);
    }
}

/********************* 以下为网络时间的获取 *********************/

#if (1 == OS_TYPE)
void datePage::slot_updateNetworkTime()                             // TODO: 这功能不属于这个模块，不应该放这里
{
#if (OS_TYPE == 2)
    return;
#endif

    if(Starting_Up_Get_Time_Count && g_WifiIntf->getIsConnected()){
        qDebug()<<"get time...";
        udpsocket = new QUdpSocket(this);
        connect(udpsocket,SIGNAL(connected()),this,SLOT(connectsucess()));
        connect(udpsocket,SIGNAL(readyRead()),this,SLOT(readingDataGrams()));
        //udpsocket->connectToHost("time.windows.com",123);
        udpsocket->connectToHost("120.25.108.11",123); //阿里云的节点

        Timer_Count++;
        if(Timer_Count > 1){  //开机联网情况下获取2次网络时间
            Timer_Count = 0;
            Starting_Up_Get_Time_Count = false;
            Get_Network_Time_flag = false;
        }
    }
}

void datePage::connectsucess()
{
    qint8 LI=0;  //无预告
    qint8 VN=3;  //版本号
    qint8 MODE=3; //客户端几
    qint8 STRATUM=0;//表示本地时钟层次水平
    qint8 POLL=4; //测试间隔
    qint8 PREC=-6; //表示本地时钟精度
    QDateTime Epoch(QDate(1900, 1, 1));
    qint32 second=quint32(Epoch.secsTo(QDateTime::currentDateTime()));
    qDebug()<<"Connected...";
    qint32 temp=0;
    QByteArray timeRequest(48, 0);
    timeRequest[0]=(LI <<6) | (VN <<3) | (MODE);
    timeRequest[1]=STRATUM;
    timeRequest[2]=POLL;
    timeRequest[3]=PREC & 0xff;
    timeRequest[5]=1;
    timeRequest[9]=1;

    //40到43字节保存原始时间戳，即客户端发送的时间
    timeRequest[40]=(temp=(second&0xff000000)>>24);
    temp=0;
    timeRequest[41]=(temp=(second&0x00ff0000)>>16);
    temp=0;
    timeRequest[42]=(temp=(second&0x0000ff00)>>8);
    temp=0;
    timeRequest[43]=((second&0x000000ff));
    udpsocket->flush();
    udpsocket->write(timeRequest);
    udpsocket->flush();
}

void datePage::readingDataGrams()       // TODO: 代码在 rk3568 下执行，出了异常，本函数未执行到最后？
{
    qDebug()<<"Reading...";
    QByteArray newTime;
    QDateTime Epoch(QDate(1900, 1, 1));
    QDateTime unixStart(QDate(1970, 1, 1));
    do
    {
        newTime.resize(udpsocket->pendingDatagramSize());
        udpsocket->read(newTime.data(), newTime.size());
    }while(udpsocket->hasPendingDatagrams());

    QByteArray TransmitTimeStamp;
    //qDebug() << "newTime:" << newTime.toHex() << "size: " << newTime.size();
    TransmitTimeStamp = newTime.right(8);
    //qDebug() << "TransmitTimeStamp" << TransmitTimeStamp.toHex();
    quint32 seconds = TransmitTimeStamp[0];
    //qDebug () << "seconds: " << seconds;
    quint8 temp = 0;
    for(int j = 1;j <= 3;j++)
    {
        seconds = seconds<<8;
        temp = TransmitTimeStamp[j];
        seconds = seconds+temp;
    }
    quint32 mseconds = TransmitTimeStamp[4];
    quint8 temp1 = 0;
    for(int j = 5;j <= 7;j++)
    {
        mseconds = mseconds<<8;
        temp1 = TransmitTimeStamp[j];
        mseconds = mseconds+temp1;
    }

    quint32 x = mseconds;
    mseconds = (((x) >> 12) - 759 * ((((x) >> 10) + 32768) >> 16));
    //qDebug() << "mseconds:" << mseconds  << "        " << "seconds:" << seconds;
    //seconds = seconds * 1000 + mseconds;
    QDateTime date_time;
    date_time.setTime_t(seconds-Epoch.secsTo(unixStart));

    this->udpsocket->disconnectFromHost();
    this->udpsocket->close();

    date_time = date_time.addSecs(8 * 60 * 60);     // TODO: “因为移植到板子上时间少了8小时(时间差问题)，所以这里要加上8小时”？

    qDebug() << "datePage::readingDataGrams(): get new time = " << date_time.toString();
    CHardware::setMachineDateTime(date_time);

}
#endif
