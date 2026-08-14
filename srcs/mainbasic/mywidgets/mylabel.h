#ifndef CMYLABEL_H
#define CMYLABEL_H

#include <QWidget>
#include <QLabel>

// 自定义 QLabel 控件
/* 特性：
 * 1、响应鼠标点击事件。
 */
class CMyLabel : public QLabel
{
    Q_OBJECT
public:
    explicit CMyLabel(QWidget *_parent = 0);
    ~CMyLabel();

protected:
    void mouseReleaseEvent(QMouseEvent *_evt) override;

signals:
    void clicked();

};

// 支持点击事件的 Widget 部件
class CWidgetClickable : public QWidget
{
    Q_OBJECT
public:
    explicit CWidgetClickable(QWidget *_parent = 0);

protected:
    void mouseReleaseEvent(QMouseEvent *_evt) override;

signals:
    void clicked();

};

// 开关按钮
class CSwitchButton : public CMyLabel
{
    Q_OBJECT

public:
    explicit CSwitchButton(QWidget *_parent = 0);
    ~CSwitchButton();

    void setIsOn(bool _is_on);
    bool getIsOn();

protected:
    void mouseReleaseEvent(QMouseEvent *_ev) override;

    bool isOn = false;
    QPixmap pixmapOn, pixmapOff;

};

#endif // CMYLABEL_H
