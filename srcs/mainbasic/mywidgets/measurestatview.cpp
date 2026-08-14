#include "measurestatview.h"

#include <QPainter>

// 常量
const int WIDGET_WIDTH  = 70;   // 总宽度
const int WIDGET_HEIGHT = 16;   // 总高度

//const QColor COLOR_BACK     = QColor(127, 127, 127);    // 背景色
//const QColor COLOR_NOT_OK   = QColor(243, 196, 50);     // 未 OK（橙色）
const QColor COLOR_NOT_OK   = QColor(204, 204, 204);     // 未 OK（灰）
const QColor COLOR_OK       = QColor(0, 176, 0);        // OK（绿色）
const QColor COLOR_FIXED    = QColor(255, 0, 0);        // 固定曝光（红色）

//
CMeasureStatView::CMeasureStatView(QWidget *_parent, CMeasureCtrl *_measureCtrl) : QLabel(_parent), measureCtrl(_measureCtrl)
{
    setGeometry(0, 0, WIDGET_WIDTH, WIDGET_HEIGHT);
    clear();

    //
    reset();
}

// 重设
void CMeasureStatView::reset()
{
    expoTime = -1;
    isExposureOk = false;

}

void CMeasureStatView::paintEvent(QPaintEvent *_event)
{
    Q_UNUSED(_event)

    if (expoTime <= 0) {
        return;
    }

    QPainter pt(this);

    //pt.fillRect(0, 0, WIDGET_WIDTH, WIDGET_HEIGHT, COLOR_BACK);

    pt.setPen(m_isExposureFixed ? COLOR_FIXED : (isExposureOk ? COLOR_OK : COLOR_NOT_OK));
    pt.drawText(2, WIDGET_HEIGHT - 2, QString("%1 us").arg(expoTime));

}

// 更新测量状态显示部件
void CMeasureStatView::updateMeasureStat()
{
    expoTime = measureCtrl->getExposureTime();
    isExposureOk = measureCtrl->getIsExposureOk();

    this->update();
}
