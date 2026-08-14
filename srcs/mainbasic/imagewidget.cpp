//实现图片缩放,图片移动,以及图片翻页功能
#include "imagewidget.h"
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QPointF>
#include <QGraphicsSceneDragDropEvent>
#include <QDrag>
#include <math.h>

#include "util-common.h"

ImageWidget::ImageWidget(QObject *parent):QObject(parent)
{
    //m_pix = *pixmap;
    setAcceptDrops(true);//如果启用为真，此项目将接受悬停事件;否则，它将忽略它们。默认情况下，项目不接受悬停事件
    m_scaleValue = 0;
    m_scaleDafault = 0;
    m_isMove = false;
}

QRectF ImageWidget::boundingRect() const
{
    return QRectF(-m_pix.width() / 2, -m_pix.height() / 2,
                  m_pix.width(), m_pix.height());
}

void ImageWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                    QWidget *)
{
    painter->drawPixmap(-m_pix.width() / 2, -m_pix.height() / 2, m_pix);
}

void ImageWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (Util::compDouble(m_scaleValue, m_scaleDafault) == 0){
        press_x = event->pos().x();
        press_y = event->pos().y();
    }
    if(event->button()== Qt::LeftButton)
    {
        qDebug()<<"Press:"<<m_isMove<<" pos"<<event->pos();
        m_startPos = event->pos();//鼠标左击时，获取当前鼠标在图片中的坐标，
        m_isMove = true;//标记鼠标左键被按下
    }
    else if(event->button() == Qt::RightButton)
    {
        ResetItemPos();//右击鼠标重置大小
    }

}

void ImageWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    //qDebug()<<"move pos:"<<event->pos();
    if (m_isMove && Util::compDouble(m_scaleValue, m_scaleDafault) > 0)
    {
        QPointF point = (event->pos() - m_startPos)*m_scaleValue;
        moveBy(point.x(), point.y());
    }
}

void ImageWidget::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (Util::compDouble(m_scaleValue, m_scaleDafault) == 0){
        relea_x = event->pos().x();
        relea_y = event->pos().y();
        //此处屏蔽图片翻页功能(如果要打开该功能,mouseMoveEvent函数中的if条件也要开放)
        if((relea_x - press_x)>50 && qAbs(relea_y-press_y)<100)    //x轴滑动距离大于50像素,y轴滑动距离小于100像素
            emit sigUserTurnPage(false);    // 右滑动
        if((press_x - relea_x)>50 && qAbs(relea_y-press_y)<100)
            emit sigUserTurnPage(true);     // 左滑动

    }
    m_isMove = false;//标记鼠标左键已经抬起
    if(event->pos().x()<=750 && event->pos().y()<=450)
        ReleasePos = event->pos();
    qDebug()<<"ReleasePos x:"<<ReleasePos.x()<<" y:"<<ReleasePos.y();
}

void ImageWidget::slotChangeZoom(int _zoom)
{
    if((_zoom > 0)&&(m_scaleValue >= 50))//最大放大到原始图像的50倍
    {
        return;
    }
    else if((_zoom < 0)&&(m_scaleValue <= m_scaleDafault))//图像缩小到自适应大小之后就不继续缩小
    {
        ResetItemPos();//重置图片大小和位置，使之自适应控件窗口大小
    }
    else
    {
        qreal qrealOriginScale = m_scaleValue;
        if(_zoom > 0)//鼠标滚轮向前滚动
        {
            m_scaleValue*=1.1;//每次放大10%
        }
        else
        {
            m_scaleValue*=0.9;//每次缩小10%
        }
        setScale(m_scaleValue);
        if(_zoom > 0)
            moveBy(-ReleasePos.x()*qrealOriginScale*0.1, -ReleasePos.y()*qrealOriginScale*0.1);//使图片缩放的效果看起来像是以鼠标所在点为中心进行缩放的
        else
            moveBy(ReleasePos.x()*qrealOriginScale*0.1, ReleasePos.y()*qrealOriginScale*0.1);//使图片缩放的效果看起来像是以鼠标所在点为中心进行缩放的
    }
}

void ImageWidget::setQGraphicsViewWH(int nwidth, int nheight)//将主界面的控件QGraphicsView的width和height传进本类中，并根据图像的长宽和控件的长宽的比例来使图片缩放到适合控件的大小
{
    int nImgWidth = m_pix.width();
    int nImgHeight = m_pix.height();
    qreal temp1 = nwidth*1.0/nImgWidth;
    qreal temp2 = nheight*1.0/nImgHeight;
    if(temp1>temp2)
    {
        m_scaleDafault = temp2;
    }
    else
    {
        m_scaleDafault = temp1;
    }
    setScale(m_scaleDafault);
    m_scaleValue = m_scaleDafault;
}

void ImageWidget::ResetItemPos()//重置图片位置
{
    m_scaleValue = m_scaleDafault;//缩放比例回到一开始的自适应比例
    qDebug()<<"m_scaleValue:"<<m_scaleValue;
    setScale(m_scaleDafault);//缩放到一开始的自适应大小
    setPos(0,0);
}

qreal ImageWidget::getScaleValue() const
{
    return m_scaleValue;
}

void ImageWidget::setPixmap(QPixmap *pixmap)
{
    m_pix = *pixmap;
}
