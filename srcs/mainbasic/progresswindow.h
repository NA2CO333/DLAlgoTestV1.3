#ifndef PROGRESSWINDOW_H
#define PROGRESSWINDOW_H

#include "baseform.h"

namespace Ui {
class ProgressWindow;
}

class ProgressWindow : public CBaseWidget
{
    Q_OBJECT

public:
    explicit ProgressWindow(QWidget *parent = 0);
    ~ProgressWindow();

    void setProgress(int progress);
    void setContext(QString text);
    void setNotice(QString text);

protected:
        void paintEvent(QPaintEvent *event);

private:
    Ui::ProgressWindow *ui;
};

#endif // PROGRESSWINDOW_H
