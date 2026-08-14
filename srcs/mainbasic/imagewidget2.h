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
// class ImageWidget :public QObject, QGraphicsItem
class ImageWidget :public QGraphicsItem
{
    //Q_OBJECT
public:
    ImageWidget(QPixmap *pixmap);
    QRectF  boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    void ResetItemPos();
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    qreal getScaleValue() const;
    void setQGraphicsViewWH(int nwidth,int nheight);


private slots:
    //void ZoomPictures(int value);

private:
    qreal   m_scaleValue;
    qreal   m_scaleDafault;
    QPixmap m_pix;
    int     m_zoomState;
    bool    m_isMove;
    QPointF m_startPos;
    QPoint  ReleasePos;

};
#endif // IMAGEWIDGET_H
