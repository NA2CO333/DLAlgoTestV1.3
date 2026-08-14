#ifndef MYDIALOG_H
#define MYDIALOG_H

#include "baseform.h"

namespace Ui {
class myDialog;
}

class myDialog : public CBaseDialog
{
    Q_OBJECT

public:
    explicit myDialog(QWidget *parent = 0);
    ~myDialog();
    void setContent(const char*);
    void setButtonText(QString,QString);

protected:


private:
    Ui::myDialog *ui;
};

#endif // MYDIALOG_H
