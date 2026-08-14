#ifndef CDOUBLESPINBOX_H
#define CDOUBLESPINBOX_H

#include <QSpinBox>
#include <QLineEdit>

// 自定义 QDoubleSpinBox 控件
/* 特性：
 * 1、取消文本的自动全选。
 * 2、无焦点。
 * 3、增加“达到极值”事件。
 */
class CDoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    explicit CDoubleSpinBox(QWidget *parent = nullptr);

    //
    void setAutoSelectDisabled(bool _is_disabled);

signals:
    void sigReachedExtremeValue(int _extreme_type);             // “达到极值”事件。参数： -1 表示达到最小值，1 表示最大值。

protected slots:
    void slot_this_valueChanged(double _value);

protected:
    void mouseReleaseEvent(QMouseEvent *_event) override;

    bool isAutoSelectDisabled = false;

};

#endif // CDOUBLESPINBOX_H
