#ifndef NOTICEWIN_H
#define NOTICEWIN_H

#include <QDialogButtonBox>
#include <QTimer>

#include "baseform.h"

namespace Ui {
class NoticeWin;
}

class NoticeWin : public CBaseDialog
{
    Q_OBJECT

public:
    explicit NoticeWin(QWidget *parent = 0);
    ~NoticeWin();

    int timeout = 0;        // 超时（秒）
    bool defaultSelect = true;

    void setContent(QString);
    void setButtonText(QString button_Yes, QString button_No);

protected:
    void paintEvent(QPaintEvent *event);
    void keyPressEvent(QKeyEvent*); //按键事件
    void showEvent(QShowEvent *event);

    QTimer *timerAutoSelect = Q_NULLPTR;
    int countSec = 0;

private:
    Ui::NoticeWin *ui;

    QPushButton *yesButton;
    QPushButton *noButton;

    QSize m_sizeInit = QSize(-1, -1);
    int m_fontSizeInit;

private slots:
    void slot_timerAutoSelect_timeout();

};

#endif // NOTICEWIN_H
