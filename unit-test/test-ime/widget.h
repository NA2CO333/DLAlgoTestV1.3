#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QLineEdit>

#include "keyboard.h"

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

protected:
    void showEvent(QShowEvent *_event);

private:
    Ui::Widget *ui;

    QLineEdit *edtImeTest;

};

#endif // WIDGET_H
