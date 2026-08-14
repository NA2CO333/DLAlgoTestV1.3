#include "utilui.h"

#include <QLabel>

namespace Util {
namespace Ui {

//
void setComboBoxItemsText(QComboBox * const _combo_box, const QList<QString> &_items_text)
{
    for (int i = _combo_box->count() - 1; i >= 0; i--) {
        if (i < _items_text.size()) {
            _combo_box->setItemText(i, _items_text[i]);
        }
    }
}

void addAsteriskToSideOfWidget(const QWidget *_widget, bool _is_left_side)
{
    static QMap<QString, QLabel *> map;

    // 创建 Label 控件
    QLabel *label = Q_NULLPTR;
    auto it_existed = map.find(_widget->objectName());
    if (map.end() == it_existed) {
        label = new QLabel;

        label->setText("*");
        label->setObjectName("_asterisk_" + _widget->objectName());
        label->setAlignment(Qt::AlignCenter);
        label->setFont(_widget->font());
        label->setStyleSheet("QLabel { color: rgb(164, 0, 0); }");

        label->setParent(_widget->parentWidget());

        //
        map.insert(_widget->objectName(), label);
    } else {
        label = it_existed.value();
    }

    // 设置 Label 控件的位置
    int h = _widget->height();
    int w = h;
    int x = (_is_left_side ? _widget->x() - w : _widget->x() + _widget->width());
    int y = _widget->y();

    label->setGeometry(x, y, w, h);

}

void centerWidget(QWidget *_wgt)
{
    if (!_wgt) {
        return;
    }

    QWidget *parent_wgt = _wgt->parentWidget();
    if (!parent_wgt) {
        return;
    }

    _wgt->move((parent_wgt->width() - _wgt->width()) / 2, (parent_wgt->height() - _wgt->height()) / 2);
}

void clearStyleSheet(QWidget *_widget)
{
    //
    if (_widget->styleSheet().length() > 0) {
        _widget->setStyleSheet("");
    }

    //
    QList<QWidget *> list = _widget->findChildren<QWidget *>();
    for (QWidget * child : list) {
        if (child->styleSheet().length() > 0) {
            child->setStyleSheet("");
        }
    }
}

void hideAllChildren(QWidget *_parent_wgt)
{
    QList<QWidget *> childen = _parent_wgt->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : childen) {
        child->hide();
    }
}

}   // namespace Ui
}   // namespace Util
