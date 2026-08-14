#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QRectF>
#include <QMouseEvent>
#include <QPointF>
#include <QDragEnterEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsItem>
#include <QPoint>

enum Enum_ZoomState{
    NO_STATE,
    RESET,
    ZOOM_IN,
    ZOOM_OUT
};

class ImageWidget : public QObject,public QGraphicsItem
{
    Q_OBJECT
public:
    explicit ImageWidget(QObject *parent = 0);

    void ResetItemPos();
    qreal getScaleValue() const;
    void setQGraphicsViewWH(int nwidth,int nheight);

    void setPixmap(QPixmap *pixmap);
    QPixmap *pixmap() { return &m_pix; };

public slots:
    void slotChangeZoom(int _zoom);

signals:
    void sigUserTurnPage(bool _is_toward_left);             // 用户翻页事件（待程序执行），_is_toward_left: 是否向左翻页（即页数+1）

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QRectF boundingRect() const override;

private:
    int     m_zoomState;
    bool    m_isMove;
    qreal   m_scaleValue;
    qreal   m_scaleDafault;
    QPixmap m_pix;
    QPointF m_startPos;
    QPointF ReleasePos;
    int press_x = 0;  //鼠标按下时的位置
    int press_y = 0;
    int relea_x = 0;  //鼠标释放时的位置
    int relea_y = 0;
};

#endif // IMAGEWIDGET_H
