#include "noticewin.h"
#include "ui_noticewin.h"

#include <QPainter>
#include <QPushButton>
#include <QApplication>
#include <QWidget>
#include <QDir>
#include <QDebug>
#include <QPixmap>
#include <QScreen>
#include <QStyleOption>

#include "baseform.h"
#include "global.h"

//
NoticeWin::NoticeWin(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::NoticeWin)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowFlag(Qt::FramelessWindowHint, true);
    this->setGeometry(200,110,400,260);
    ui->content->setWordWrap(true);

    //
    yesButton =  ui->buttonBox->button(QDialogButtonBox::Yes);
    noButton = ui->buttonBox->button(QDialogButtonBox::No);

    yesButton->setMinimumSize(110,45);
    noButton->setMinimumSize(110,45);

    //
    timerAutoSelect = new QTimer();
    connect(timerAutoSelect, &QTimer::timeout, this, &NoticeWin::slot_timerAutoSelect_timeout, Qt::QueuedConnection);

    //
    //m_sizeInit = ui->content->size();     /* 这里不能得到部件的真实尺寸 */
    m_fontSizeInit = ui->content->font().pointSize();

    //
    ui->lblCountDown->setVisible(false);

}

NoticeWin::~NoticeWin()
{
    delete ui;
}

void NoticeWin::keyPressEvent(QKeyEvent *)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
//    QDir dir("/media/cut");
//    if(!dir.exists()){
//        dir.mkdir("/media/cut");
//    }

//    QScreen *screen = QGuiApplication::primaryScreen();
//    QPixmap pixmap = screen->grabWindow(0);
//    QString filePathName = "/media/cut/cut-";
//    filePathName += this->objectName();
//    filePathName += ".png";

//    if(!pixmap.save(filePathName,"png"))
//    {
//        qDebug()<<"cut save png failed"<<endl;
    //    }
}

void NoticeWin::showEvent(QShowEvent *)
{
    //
    CBaseFormIntf::centerWidget(this);

    //
    enThemeType theme = getSysThemeType();
    if (themeType_Black == theme) {
        yesButton->setStyleSheet("QPushButton {border-radius:6px; padding:2px 4px; background-color:rgb(250,250,250); color:rgb(0,0,0);}");
        noButton->setStyleSheet("QPushButton {border-radius:6px; padding:2px 4px; background-color:rgb(250,250,250); color:rgb(0,0,0);}");
    } else {
        // TODO:
    }

    // 获得内容标签的初始尺寸
    if (m_sizeInit.width() <= 0) {
        m_sizeInit = ui->content->size();
    }

    // 恢复设计期间的尺寸
    QFont font = ui->content->font();
    font.setPointSize(m_fontSizeInit);
    ui->content->setFont(font);

    // 若文本高度超出容器，则调小字体
    QSize size_hint = ui->content->sizeHint();
    //qDebug() << "ui->content->sizeHint() = " << size_hint;
    if (size_hint.height() > m_sizeInit.height()) {
        do {
            font = ui->content->font();
            font.setPointSize(font.pointSize() - 1);
            ui->content->setFont(font);
            size_hint = ui->content->sizeHint();
        } while (size_hint.height() > m_sizeInit.height() && font.pointSize() >= 11);
    }

    //
    ui->lblCountDown->setVisible(timeout > 0);

    if (timeout > 0) {
        int left = ui->buttonBox->x() + (defaultSelect ? ui->buttonBox->buttons().at(0)->x() : ui->buttonBox->buttons().at(1)->x());
        ui->lblCountDown->setGeometry(left, ui->lblCountDown->y(), ui->lblCountDown->width(), ui->lblCountDown->height());

        countSec = timeout;
        ui->lblCountDown->setText(QString::number(countSec));

        timerAutoSelect->start(1000);
    }

    //
    this->setResult(defaultSelect ? QDialog::Accepted : QDialog::Rejected);

}

void NoticeWin::slot_timerAutoSelect_timeout()
{
    if (countSec > 0) {
        countSec--;
        ui->lblCountDown->setText(QString::number(countSec));
    } else {
        timerAutoSelect->stop();

        if (defaultSelect)
            accept();
        else
            reject();
    }
}

void NoticeWin::paintEvent(QPaintEvent *event)
{
    //make stylesheet working when setting background transparent
    QStyleOption opt;
    opt.init(this);
    QPainter pt(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &pt, this);

    pt.setRenderHint(QPainter::Antialiasing);  // 反锯齿;
    pt.setBrush(QBrush(QColor(60, 60, 60, 220)));
    pt.setPen(Qt::transparent);
    QRect rect = this->rect();
    rect.setWidth(rect.width() - 1);
    rect.setHeight(rect.height() - 1);
    pt.drawRoundedRect(rect, 15, 15);

    QWidget::paintEvent(event);
}


void NoticeWin::setContent(QString str)
{
    ui->content->setText(str);
}

void NoticeWin::setButtonText(QString button_Yes, QString button_No)
{
    QPushButton *yesButton = ui->buttonBox->button(QDialogButtonBox::Yes);
    QPushButton *noButton = ui->buttonBox->button(QDialogButtonBox::No);

    yesButton->setText(button_Yes);
    noButton->setText(button_No);
}
