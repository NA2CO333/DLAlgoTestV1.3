#ifndef CMEASURESTATVIEW_H
#define CMEASURESTATVIEW_H

#include <QLabel>

#include "measurectrl.h"

// 测量状态显示部件（当前曝光时间、调光状态等）
class CMeasureStatView : public QLabel
{
    Q_OBJECT
public:
    explicit CMeasureStatView(QWidget *_parent, CMeasureCtrl *_measureCtrl);

    void reset();
    void updateMeasureStat();

    void setIsExposureFixed(bool _is_fixed) { m_isExposureFixed = _is_fixed; }          // 是否固定曝光时间

signals:

public slots:

protected:
    void paintEvent(QPaintEvent *_event);

    CMeasureCtrl *measureCtrl = Q_NULLPTR;

    int expoTime = -1;
    bool isExposureOk = false;
    bool m_isExposureFixed {false};             // 是否固定曝光时间

};

#endif // CMEASURESTATVIEW_H
