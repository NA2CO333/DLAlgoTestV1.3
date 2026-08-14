#ifndef CEYELIMITMARK_H
#define CEYELIMITMARK_H

#include <QLabel>
#include <QPixmap>
#include <QPoint>

// 眼部限位标记部件（取景框）
class CEyeLimitMark : public QLabel
{
    Q_OBJECT
public:
    explicit CEyeLimitMark(QWidget *parent, int _left, int _top, int _width, int _height);

    void setIsFocusMode(bool _is_focus_mode);   // 是否“调焦模式”（用于生产调校，调焦模式时中心显示十字标）

signals:

public slots:

protected:
    void paintEvent(QPaintEvent *);

    QRect canvasRect;
    bool isFocusMode = false;
    QPoint pointCenter;

    void paintEyeLimitMark(QPainter &_pt);      // 绘制双眼限位标志
    void paintCenterCross(QPainter &_pt);       // 绘制中心十字

};

#endif // CEYELIMITMARK_H
