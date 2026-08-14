#ifndef DIALOGLISTSELECT_H
#define DIALOGLISTSELECT_H

#include "baseform.h"

namespace Ui {
class CDialogListSelect;
}

class CDialogListSelect : public CBaseDialog
{
    Q_OBJECT

public:
    explicit CDialogListSelect(QWidget *parent = 0);
    ~CDialogListSelect();

    QStringList fileList;

    void insertRow(QString _str, bool _checked = false);

protected:
    void showEvent(QShowEvent *);
    void paintEvent(QPaintEvent *event);

private slots:
    void on_btnCancel_clicked();
    void on_btnOK_clicked();
    void on_btnClose_clicked();

private:
    Ui::CDialogListSelect *ui;
};

#endif // DIALOGLISTSELECT_H
