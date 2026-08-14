#ifndef CSUSPENSIONPROMPTBOX_H
#define CSUSPENSIONPROMPTBOX_H

#include <QWidget>
#include <QLabel>

// 悬浮提示框
class CSuspensionPromptBox : public QFrame         // TODO: 直接从 QLabel 继承效果怎样？      // TODO: 不能在 QDialog 之上显示？
{
    Q_OBJECT
public:
    static CSuspensionPromptBox *getInstance();     // 单例获取
    static void releaseInstance();                  // 单例释放

    ~CSuspensionPromptBox();

    void setScreenSize(int _width, int _height);

    bool getIsShown();      /* QWidget::isVisible() 可能被父窗体改变，所以不能用 */
    int showMessage(QWidget *_parent, QString _msg, int _msecs = 0);
    void changeParent(QWidget *_parent);
    void hideMsgWin(int _msg_id = 0);

protected:
    static CSuspensionPromptBox *instance;

    explicit CSuspensionPromptBox(QWidget *parent = 0, Qt::WindowFlags f = 0);

    void timerEvent(QTimerEvent *_timer_evt) override;
    void mouseReleaseEvent(QMouseEvent *_e) override;
    void showEvent(QShowEvent *event) override;

    int screenWidth = -1;
    int screenHeight = -1;

    QLabel *lblText = Q_NULLPTR;

    int timerId = 0;
    bool isShown = false;       // TODO: 为什么不用 QWidget::isVisible() ？
    int msgId = 0;

    void delayHide(int _msecs);
    void adjustGeometry(int _content_width, int _content_height);
    void updateTheme();

};

#endif // CSUSPENSIONPROMPTBOX_H
