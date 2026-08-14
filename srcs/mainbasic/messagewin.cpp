#include "messagewin.h"
#include "ui_messagewin.h"

#include <QPainter>
#include <QPushButton>
#include <QStyleOption>

#include "baseform.h"
#include "global.h"
#include "camerainit.h"

//
MessageWin::MessageWin(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::MessageWin)
{
    //
    ui->setupUi(this);

    //
    this->setAttribute(Qt::WA_TranslucentBackground);           // NOTE: 背景设为透明，在 paintEvent() 里绘制背景
    this->setWindowFlag(Qt::FramelessWindowHint, true);

    ui->lblContent->setWordWrap(true);
    okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setMinimumSize(110, 40);

    //
    //m_sizeInit = ui->lblContent->size();     /* 这里不能得到部件的真实尺寸 */
    m_fontSizeInit = ui->lblContent->font().pointSize();

    //
    this->setMaximumSize(SCREEN_WIDTH, SCREEN_HEIGHT);      // TODO: 若不限制最大尺寸，文本框的文本过长会导致整个对话框的尺寸扩大，超出屏幕后，按钮将无法点击
    ui->lblContent->setMaximumSize(SCREEN_WIDTH - ui->wgtBtns->height() - 20, SCREEN_HEIGHT - 30);      // NOTE: 实测在 Ubuntu 里只需设置 this->setMaximumSize()，而 RK3568 里还需设置 lblContent->setMaximumSize()

}

MessageWin::~MessageWin()
{
    delete ui;
}

void MessageWin::setButtonEnable(bool state)
{
    if(state){
        okButton->setVisible(true);
    }
    else{
        okButton->setVisible(false);
    }
}

void MessageWin::mouseReleaseEvent(QMouseEvent *_evt)
{
    //
    CBaseDialog::mouseReleaseEvent(_evt);

    // 若两个按钮都不可见，点击后隐藏
    if ((!okButton->isVisible()) && (!okButton->isEnabled())) {
        this->hide();
    }
}

void MessageWin::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    //
    CBaseFormIntf::centerWidget(this);

    //
    enThemeType theme = getSysThemeType();
    if (themeType_Black == theme) {
        okButton->setStyleSheet("QPushButton {border-radius:6px; padding:2px 4px; background-color:rgb(250,250,250); color: rgb(0,0,0);}");
    } else {
        // TODO:
    }

    // 获得内容标签的初始尺寸
    if (m_sizeInit.width() <= 0) {
        m_sizeInit = ui->lblContent->size();
    }

    // 恢复设计期间的尺寸
    QFont font = ui->lblContent->font();
    font.setPointSize(m_fontSizeInit);
    ui->lblContent->setFont(font);

    // 若文本高度超出容器，则调小字体
    QSize size_hint = ui->lblContent->sizeHint();
    //qDebug() << "ui->lblContent->sizeHint() = " << size_hint;
    if (size_hint.height() > m_sizeInit.height()) {
        do {
            font = ui->lblContent->font();
            font.setPointSize(font.pointSize() - 1);
            ui->lblContent->setFont(font);
            size_hint = ui->lblContent->sizeHint();
        } while (size_hint.height() > m_sizeInit.height() && font.pointSize() >= 11);
    }

    // 超时自动隐藏
    if (m_timeoutSecs > 0) {
        this->startTimer(m_timeoutSecs * 1000);
    }
}

void MessageWin::timerEvent(QTimerEvent *)
{
    this->hide();
}

void MessageWin::paintEvent(QPaintEvent *event)
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


void MessageWin::setContent(QString str)
{
    ui->lblContent->setText(str);
}

void MessageWin::setButtonText(QString str)
{
    QPushButton *bt = ui->buttonBox->button(QDialogButtonBox::Ok);
    bt->setText(str);
}
