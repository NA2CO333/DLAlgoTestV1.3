#ifndef MESSAGEWIN_H
#define MESSAGEWIN_H

#include "baseform.h"

namespace Ui {
class MessageWin;
}

// TODO: 本窗体最前显示时，若之后还有主窗体显示，则本窗体最前显示但没有焦点，导致整个 UI 无法操作？
/* 好像不是，貌似是 Qt Forms 的 bug ？因为在 PC 里调试时，本对话框会抢占最前显示，即点击其它窗体后，先是其它窗体最前，然后最前位置立即被本对话框抢回。
 * 而且点击本对话框上的按钮，会有焦点虚框显示，但是无法触发按钮的点击事件。
 *
 * 调试发现：若本窗体在非主线程里 show，则无法响应鼠标事件，若本窗体还设为 Qt::ApplicationModal，则会导致 UI 线程 show 的窗体也失去鼠标事件响应。
 * 就算本窗体是在主线程 show 的，如果在它之后又有窗体 show 出来，在嵌入式平台里还是会被后面的窗体挡住，导致整个程序无法操作，而 PC 里提示框却可以再次抢占最前显示，使操作能正常进行。
 *
 */

//
class MessageWin : public CBaseDialog
{
    Q_OBJECT

public:
    explicit MessageWin(QWidget *parent = 0);
    ~MessageWin();

    void setContent(QString);
    void setButtonEnable(bool);
    void setButtonText(QString);
    void setTimeout(int _timeout_secs) { m_timeoutSecs = _timeout_secs; }       // 设置超时（秒数）自动隐藏，小于等于0表示无超时

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *_evt) override;
    void showEvent(QShowEvent *event) override;
    void timerEvent(QTimerEvent *) override;

    QSize m_sizeInit = QSize(-1, -1);
    int m_fontSizeInit;

    int m_timeoutSecs = -1;     // 超时（秒数）自动隐藏，小于等于0表示无超时

private:
    Ui::MessageWin *ui;
    QPushButton* okButton;
};

#endif // MESSAGEWIN_H
