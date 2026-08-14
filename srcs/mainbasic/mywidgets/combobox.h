#ifndef CCOMBOBOX_H
#define CCOMBOBOX_H

#include <QComboBox>
#include <QListView>

// 自定义 ComboBox 控件
/* 特性：
 * 支持 QSS 行高
 */
class CComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit CComboBox(QWidget *parent = nullptr);
    ~CComboBox();

    void enableGrabGesture();

protected:
    void showEvent(QShowEvent *) override;

    QListView *listView = Q_NULLPTR;
    bool isListViewInit = false;

};

#endif // CCOMBOBOX_H
