#ifndef CUTILUI_H
#define CUTILUI_H

#include <QList>
#include <QString>
#include <QComboBox>

namespace Util {
namespace Ui {

// 设置 QComboBox 的下拉列表框（一般用在控件初始化及语言切换时，比如年龄段）
void setComboBoxItemsText(QComboBox * const _combo_box, const QList<QString> &_items_text);

// 在指定控件的旁边加上星号符
void addAsteriskToSideOfWidget(const QWidget *_widget, bool _is_left_side);

// 使窗口位于父窗口中心
void centerWidget(QWidget *_wgt);

void clearStyleSheet(QWidget *_widget);                                     // 清理 QWidget 及其所有子部件的样式

void hideAllChildren(QWidget *_parent_wgt);                                 // 隐藏所有子窗口

}   // namespace Ui
}   // namespace Util

#endif // CUTILUI_H
