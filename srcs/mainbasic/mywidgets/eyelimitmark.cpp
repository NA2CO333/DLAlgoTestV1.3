#include "eyelimitmark.h"

#include "camerainit.h"

#include <QPainter>

//
CEyeLimitMark::CEyeLimitMark(QWidget *parent, int _left, int _top, int _width, int _height) : QLabel(parent)
{
    canvasRect = QRect(0, 0, _width, _height);

    this->setGeometry(_left, _top, canvasRect.width(), canvasRect.height());

    //pointCenter.setX(SCREEN_WIDTH / 2 - _left);
    //pointCenter.setY(SCREEN_HEIGHT / 2 - _top);
    pointCenter.setX(_width / 2);
    pointCenter.setY(_height / 2);

}

void CEyeLimitMark::setIsFocusMode(bool _is_focus_mode)
{
    isFocusMode = _is_focus_mode;
}

void CEyeLimitMark::paintEvent(QPaintEvent *)
{
    QPainter pt(this);
    if (!isFocusMode) {
        paintEyeLimitMark(pt);
    } else {
        paintCenterCross(pt);
    }
}

void CEyeLimitMark::paintEyeLimitMark(QPainter &_pt)
{
    //
    const int BORDER_MARGIN_X   = 39;       // 边框的左外边距
    const int BORDER_MARGIN_Y   = 30;       // 边框的上外边距
    const int BORDER_LEN        = 40;       // 边框的线段长度
    const int BORDER_WIDTH      = 11;       // 边框宽度
    const int CENTER_MARGIN     = 36;       // 中部位标的外边距
    const int CENTER_LEN        = 33;       // 中部位标的线段长度
    const int CENTER_WIDTH      = 8;        // 中部位标

    const QColor MARK_COLOR = QColor(48, 48, 48, 200);

    // 左上角
    _pt.fillRect(canvasRect.left() + BORDER_MARGIN_X, canvasRect.top() + BORDER_MARGIN_Y,
                BORDER_LEN, BORDER_WIDTH, MARK_COLOR);
    _pt.fillRect(canvasRect.left() + BORDER_MARGIN_X, canvasRect.top() + BORDER_MARGIN_Y + BORDER_WIDTH,
                BORDER_WIDTH, BORDER_LEN - BORDER_WIDTH, MARK_COLOR);

    // 右上角
    _pt.fillRect(canvasRect.right() - BORDER_MARGIN_X - BORDER_LEN, canvasRect.top() + BORDER_MARGIN_Y,
                BORDER_LEN, BORDER_WIDTH, MARK_COLOR);
    _pt.fillRect(canvasRect.right() - BORDER_MARGIN_X - BORDER_WIDTH, canvasRect.top() + BORDER_MARGIN_Y + BORDER_WIDTH,
                BORDER_WIDTH, BORDER_LEN - BORDER_WIDTH, MARK_COLOR);

    // 左下角
    _pt.fillRect(canvasRect.left() + BORDER_MARGIN_X, canvasRect.bottom() - BORDER_MARGIN_Y - BORDER_WIDTH,
                BORDER_LEN, BORDER_WIDTH, MARK_COLOR);
    _pt.fillRect(canvasRect.left() + BORDER_MARGIN_X, canvasRect.bottom() - BORDER_MARGIN_Y - BORDER_LEN,
                BORDER_WIDTH, BORDER_LEN - BORDER_WIDTH, MARK_COLOR);

    // 右下角
    _pt.fillRect(canvasRect.right() - BORDER_MARGIN_X - BORDER_LEN, canvasRect.bottom() - BORDER_MARGIN_Y - BORDER_WIDTH,
                BORDER_LEN, BORDER_WIDTH, MARK_COLOR);
    _pt.fillRect(canvasRect.right() - BORDER_MARGIN_X - BORDER_WIDTH, canvasRect.bottom() - BORDER_MARGIN_Y - BORDER_LEN,
                BORDER_WIDTH, BORDER_LEN - BORDER_WIDTH, MARK_COLOR);

    // 上中部位标
    _pt.fillRect(canvasRect.left() + canvasRect.width() / 2 - CENTER_LEN, canvasRect.top() + CENTER_MARGIN,
                CENTER_LEN * 2, CENTER_WIDTH, MARK_COLOR);
    _pt.fillRect(canvasRect.left() + canvasRect.width() / 2 - CENTER_WIDTH / 2, canvasRect.top() + CENTER_MARGIN + CENTER_WIDTH,
                CENTER_WIDTH, CENTER_LEN - CENTER_WIDTH, MARK_COLOR);

    // 下中部位标
    _pt.fillRect(canvasRect.left() + canvasRect.width() / 2 - CENTER_LEN, canvasRect.bottom() - CENTER_MARGIN,
                CENTER_LEN * 2, CENTER_WIDTH, MARK_COLOR);
    _pt.fillRect(canvasRect.left() + canvasRect.width() / 2 - CENTER_WIDTH / 2, canvasRect.bottom() - CENTER_MARGIN - CENTER_LEN + CENTER_WIDTH,
                 CENTER_WIDTH, CENTER_LEN - CENTER_WIDTH, MARK_COLOR);
}

void CEyeLimitMark::paintCenterCross(QPainter &_pt)
{
    const QColor MARK_COLOR_FRONT = QColor(130, 255, 60, 255);        // 前景色
    //const QColor MARK_COLOR_BACK = QColor(255, 255, 255, 255);      // 背景色

    const int LINE_LEN      = 50;   // 线长
    const int LINE_WIDTH    = 4;    // 线宽
    //const int BORDER        = 1;    // 边框厚度

    int left, top, width, height;

    // 横线
    left = pointCenter.x() - LINE_LEN / 2;
    top = pointCenter.y() - LINE_WIDTH / 2;
    width = LINE_LEN;
    height = LINE_WIDTH;

    //_pt.fillRect(left - BORDER, top - BORDER, width + BORDER * 2, height + BORDER * 2, MARK_COLOR_BACK);
    _pt.fillRect(left, top, width, height, MARK_COLOR_FRONT);

    // 竖线
    left = pointCenter.x() - LINE_WIDTH / 2;
    top = pointCenter.y() - LINE_LEN / 2;
    width = LINE_WIDTH;
    height = LINE_LEN;

    //_pt.fillRect(left - BORDER, top - BORDER, width + BORDER * 2, height + BORDER * 2, MARK_COLOR_BACK);
    _pt.fillRect(left, top, width, height, MARK_COLOR_FRONT);

}

