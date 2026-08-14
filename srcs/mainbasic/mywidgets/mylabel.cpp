#include "mylabel.h"

#include <QMouseEvent>

///=============================================================================================================
/// class CMyLabel

CMyLabel::CMyLabel(QWidget *_parent) : QLabel(_parent)
{

}

CMyLabel::~CMyLabel()
{

}

void CMyLabel::mouseReleaseEvent(QMouseEvent *_evt)
{
    if (_evt->button() == Qt::LeftButton) {
        emit clicked();
    }
}

///=============================================================================================================
/// class CWidgetClickable

CWidgetClickable::CWidgetClickable(QWidget *_parent) : QWidget(_parent)
{

}

void CWidgetClickable::mouseReleaseEvent(QMouseEvent *_evt)
{
    if (_evt->button() == Qt::LeftButton) {
        emit clicked();
    }
}

///=============================================================================================================
/// class CSwitchButton

CSwitchButton::CSwitchButton(QWidget *_parent) : CMyLabel(_parent)
{
    this->setAlignment(Qt::AlignCenter);

    pixmapOn.load(":/resource/black_theme/switch-on_b.png");
    pixmapOff.load(":/resource/black_theme/switch-off_b.png");

    isOn = false;
    this->setPixmap(pixmapOff);

}

CSwitchButton::~CSwitchButton()
{

}

void CSwitchButton::setIsOn(bool _is_on)
{
    if (_is_on != isOn) {
        if (_is_on) {
            this->setPixmap(pixmapOn);
        } else {
            this->setPixmap(pixmapOff);
        }

        //
        isOn = _is_on;
    }
}

bool CSwitchButton::getIsOn()
{
    return isOn;
}

void CSwitchButton::mouseReleaseEvent(QMouseEvent *_ev)
{
    if (_ev->button() == Qt::LeftButton) {
        setIsOn(!isOn);

        emit clicked();
    }
}
