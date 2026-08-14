#include "suspensionpromptbox.h"

#include <limits.h>

#include <QDebug>

// 静态成员
CSuspensionPromptBox *CSuspensionPromptBox::instance = Q_NULLPTR;

// 单例获取
CSuspensionPromptBox *CSuspensionPromptBox::getInstance()
{
    if (!instance) {
        instance = new CSuspensionPromptBox();
    }
    return instance;
}

// 单例释放
void CSuspensionPromptBox::releaseInstance()
{
    if (instance) {
        delete instance;
        instance = Q_NULLPTR;
    }
}

//
CSuspensionPromptBox::CSuspensionPromptBox(QWidget *parent, Qt::WindowFlags f) : QFrame(parent, f)
{
    lblText = new QLabel(this);
    lblText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

}

CSuspensionPromptBox::~CSuspensionPromptBox()
{

}

void CSuspensionPromptBox::setScreenSize(int _width, int _height)
{
    this->screenWidth = _width;
    this->screenHeight = _height;
}

// 是否已显示
bool CSuspensionPromptBox::getIsShown()
{
    return isShown;
}

// 在 @param _parent_win 内显示信息 @param _msg 并延时 @param _msecs 毫秒后隐藏。若 _msecs = 0 则表示默认显示时间，若 _msecs < 0，则一直显示，直到被点击。
int CSuspensionPromptBox::showMessage(QWidget *_parent, QString _msg, int _msecs)
{
    const int DEFAULT_MSECS     = 3000;         // 默认显示时间（毫秒）
    const int MIN_MSECS         = 1000;         // 最短显示时间（毫秒）

    //
    assert(screenWidth > 0 && screenHeight > 0);

    // 延时量检查修正
    if (_msecs > 0) {               // 超时 > 0 时，不能低于最小值
        if (_msecs < MIN_MSECS) {
            _msecs = MIN_MSECS;
        }
    } else if (0 == _msecs) {       // 超时 = 0 时，表示默认超时
        _msecs = DEFAULT_MSECS;
    } else if (_msecs < 0) {        // 超时 < 0 时，表示永不超时
        //_msecs = -1;
    }

    // 父窗口切换
    changeParent(_parent);
    this->raise();

    // 显示，及设置“已显示”标志
    this->setVisible(true);

    isShown = true;

    // 更新样式
    updateTheme();

    // 文字设置
    lblText->setText(_msg);

    // 尺寸调整
    //qDebug() << lblText->width() << ", " << lblText->height();
    lblText->adjustSize();
    //qDebug() << lblText->width() << ", " << lblText->height();
    adjustGeometry(lblText->width(), lblText->height());

    // 计时隐藏
    delayHide(_msecs);

    // 消息 ID
    msgId ++;
    if (msgId >= INT_MAX) {
        msgId = 1;
    }

    //
    return msgId;
}

void CSuspensionPromptBox::hideMsgWin(int _msg_id)
{
    /* 消息 ID 处理逻辑：若为默认值0，则一律隐藏，否则只有等于当前 ID 才隐藏 */

    // 若传入的
    if (_msg_id != 0) {
        if (_msg_id != msgId) {
            return;
        }
    }

    //
    if (timerId > 0) {
        this->killTimer(timerId);
        timerId = 0;
    }

    isShown = false;
    msgId = 0;
    this->setVisible(false);
}

// 根据内容的尺寸调整本窗口的位置和尺寸
void CSuspensionPromptBox::adjustGeometry(int _content_width, int _content_height)  // TODO: 第一次弹出时尺寸偏小，最后一个字只显示一半？
{
    static const int MAX_WIDTH  = screenWidth * 0.8;
    static const int MAX_HEIGHT = screenHeight * 0.6;
    //static const int MIN_WIDTH  = screenWidth * 0.4;
    //static const int MIN_HEIGHT = screenHeight * 0.2;

    static const int PADDING_HORI = 10;
    static const int PADDING_VERT = 5;

    int width_win = std::min(_content_width + PADDING_HORI * 2, MAX_WIDTH);
    //width_win = std::max(width_win, MIN_WIDTH);
    int height_win = std::min(_content_height + PADDING_VERT * 2, MAX_HEIGHT);
    //height_win = std::max(height_win, MIN_HEIGHT);

    int x_win = (screenWidth - width_win) / 2;
    int y_win = (screenHeight - height_win) / 2;

    this->setGeometry(QRect(x_win, y_win, width_win, height_win));

    lblText->setGeometry((width_win - _content_width) / 2, (height_win - _content_height) / 2, _content_width, _content_height);

}

void CSuspensionPromptBox::updateTheme()
{
    this->setStyleSheet("CSuspensionPromptBox {background-color: rgb(204, 204, 204); border: 2px solid rgb(180, 180, 180); border-radius: 8px;}");
    //this->setAutoFillBackground(true);        /* 如果设 autoFillBackground 为 ture，圆角外会有背景色 */

    lblText->setStyleSheet("QLabel {background-color: rgb(204, 204, 204); color: rgb(28, 28, 30);}");   /* 如果不设置背景色和前景色，此部件的颜色会与 this->parent() 的颜色相同 */
    //lblText->setAutoFillBackground(true);
    /* 注意：这里的样式有可能会被主窗体的 QList<QLabel *> xxx = findChildren<QLabel *>() 然后循环 setStyleSheet 模式的代码覆盖！因此上述循环代码应根据 objectName().startWith() 过滤 */

    //
    this->update();

}

//
void CSuspensionPromptBox::changeParent(QWidget *_parent)
{
    if (isShown) {
        this->setVisible(false);
    }

    if (this->parent() != _parent) {
        this->setParent(_parent);
        this->raise();
        if (isShown) {
            this->setVisible(true);
        }
    }
}

void CSuspensionPromptBox::timerEvent(QTimerEvent *_timer_evt)
{
    Q_UNUSED(_timer_evt)

    // 隐藏消息（定时器会在该函数内杀掉）
    hideMsgWin();
}

void CSuspensionPromptBox::mouseReleaseEvent(QMouseEvent *_e)
{
    Q_UNUSED(_e)

    this->setVisible(false);
    isShown = false;
}

void CSuspensionPromptBox::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

}

void CSuspensionPromptBox::delayHide(int _msecs)
{
    // 若定时器已存在，杀掉
    if (timerId > 0) {
        this->killTimer(timerId);
        timerId = 0;
    }

    // 若延时大于 0，设置定时器
    if (_msecs > 0) {
        timerId = this->startTimer(_msecs);
    }
}

