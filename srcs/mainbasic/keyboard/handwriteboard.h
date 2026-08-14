#ifndef HANDWRITEBOARD_H
#define HANDWRITEBOARD_H

#include <QLabel>
#include <QMouseEvent>
#include <QTimer>

#include "inputmethodintf.h"

//定义画板坐标变量结构体
struct MyPoint
{
public:
    MyPoint(short x=0, short y=0)
    {
        _x = x;
        _y = y;
    }

    inline void setX(short x)
    {
        _x = x;
    }
    inline void setY(short y)
    {
        _y = y;
    }
    inline short x()
    {
        return _x;
    }
    inline short y()
    {
        return _y;
    }

private:
    short _x;
    short _y;
};

//定义画板类
class HandWriteBoard : public QLabel
{
    Q_OBJECT

public:
    enum InputLanguage {Chinese, English};
    HandWriteBoard(QWidget *parent=0, Qt::WindowFlags f=0);
    HandWriteBoard(const QString &text, QWidget *parent=0, Qt::WindowFlags f=0);
    ~HandWriteBoard();
    void init();
    void clear();
    void setLanguage(InputLanguage lang);

    void mousePressEvent(QMouseEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *);
    void paintEvent(QPaintEvent *);

    CInputMethodIntf *inputMethodIntf = Q_NULLPTR;

public slots:
    void SlotHandWritingClear();

signals:
    void recognizeResult(const QStringList &list);
    void SendClearState();

private:
    QVector<MyPoint> track;
    bool isWriting;
    InputLanguage lang;
    unsigned short mResultStr[255];
    QTimer *HandClear;
    int ClearCount;
};

#endif // HANDWRITEBOARD_H
