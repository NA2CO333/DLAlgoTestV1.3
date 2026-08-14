#ifndef CMODALWIN_H
#define CMODALWIN_H

#include "mylabel.h"

class CModalWin : public CWidgetClickable
{
    Q_OBJECT
public:
    explicit CModalWin(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *) override;

    void slot_this_clicked();

};

#endif // CMODALWIN_H
