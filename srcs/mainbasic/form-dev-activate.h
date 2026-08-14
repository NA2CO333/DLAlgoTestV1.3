#ifndef FORMDEVACTIVATE_H
#define FORMDEVACTIVATE_H

#include <QWidget>

#include "baseform.h"

namespace Ui {
class FormDevActivate;
}

class FormDevActivate : public CBaseWidget
{
    Q_OBJECT

public:
    static FormDevActivate *instance();
    ~FormDevActivate();

protected:
    explicit FormDevActivate(QWidget *_parent = nullptr);
    static FormDevActivate *s_instance;

    void showEvent(QShowEvent *_evt) override;

private slots:
    void on_btnBack_clicked();
private:
    Ui::FormDevActivate *ui;
};

#endif // FORMDEVACTIVATE_H
