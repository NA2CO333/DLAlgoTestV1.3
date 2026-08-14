#include "doublespinbox.h"

//
CDoubleSpinBox::CDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
{
    // 不获取输入焦点
    setFocusPolicy(Qt::NoFocus);

}

void CDoubleSpinBox::setAutoSelectDisabled(bool _is_disabled)
{
    if (_is_disabled) {
        if (!isAutoSelectDisabled) {
            QObject::connect(this, QOverload<double>::of(&CDoubleSpinBox::valueChanged), this, &CDoubleSpinBox::slot_this_valueChanged, Qt::QueuedConnection);
            isAutoSelectDisabled = true;
        }
    } else {
        if (isAutoSelectDisabled) {
            QObject::disconnect(this, QOverload<double>::of(&CDoubleSpinBox::valueChanged), this, &CDoubleSpinBox::slot_this_valueChanged);
            isAutoSelectDisabled = false;
        }
    }
}

void CDoubleSpinBox::slot_this_valueChanged(double _value)
{
    Q_UNUSED(_value)

    this->lineEdit()->deselect();
}

void CDoubleSpinBox::mouseReleaseEvent(QMouseEvent *_event)
{
    //
    int extreme_type = 0;
    if (qFuzzyIsNull(this->value() - this->maximum())) {
        extreme_type = 1;
    } else if (qFuzzyIsNull(this->value() - this->minimum())) {
        extreme_type = -1;
    }

    if (0 != extreme_type) {
        emit sigReachedExtremeValue(extreme_type);
    }

    //
    QDoubleSpinBox::mouseReleaseEvent(_event);
}
