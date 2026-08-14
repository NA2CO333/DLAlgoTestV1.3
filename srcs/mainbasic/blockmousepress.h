#ifndef BLOCKMOUSEPRESS_H
#define BLOCKMOUSEPRESS_H

#include "baseform.h"
#include "statusbarform.h"

namespace Ui {
class blockMousePress;
}

class blockMousePress : public CBaseDialog
{
    Q_OBJECT

public:
    explicit blockMousePress(QWidget *parent = 0);
    ~blockMousePress();

protected:
    void mousePressEvent(QMouseEvent*);
    void keyPressEvent(QKeyEvent*);

private:
    Ui::blockMousePress *ui;
};

#endif // BLOCKMOUSEPRESS_H
