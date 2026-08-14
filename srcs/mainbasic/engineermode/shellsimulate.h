#ifndef SHELLSIMULATE_H
#define SHELLSIMULATE_H

#include "baseform.h"

namespace Ui {
class shellsimulate;
}

class shellsimulate : public CBaseDialog
{
    Q_OBJECT

public:
    explicit shellsimulate(QWidget *parent = 0);
    ~shellsimulate();

protected:
    void keyPressEvent(QKeyEvent *e);
    void showEvent(QShowEvent *);

private:
    Ui::shellsimulate *ui;

private slots:
    void on_btnClear_clicked();
    void on_btnGoBack_clicked();
};

#endif // SHELLSIMULATE_H
