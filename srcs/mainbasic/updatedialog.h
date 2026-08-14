#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>
#include <QPainter>
#include <QShowEvent>

#include "baseform.h"

namespace Ui {
class UpdateDialog;
}

class UpdateDialog : public CBaseDialog
{
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget *parent = 0);
    ~UpdateDialog();

    enum Result {Update,Remind,Ignore};

protected:
    void paintEvent(QPaintEvent *event);
    void showEvent(QShowEvent *);

private slots:
    void on_pushButtonUpdate_clicked();
    void on_pushButtonRemind_clicked();
    void on_pushButtonIgnore_clicked();

signals:

private:
    Ui::UpdateDialog *ui;
};

#endif // UPDATEDIALOG_H
