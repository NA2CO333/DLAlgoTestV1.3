#include "form-dev-activate.h"
#include "ui_form-dev-activate.h"

FormDevActivate *FormDevActivate::s_instance {nullptr};

FormDevActivate::FormDevActivate(QWidget *_parent) :
    CBaseWidget(_parent),
    ui(new Ui::FormDevActivate)
{
    ui->setupUi(this);

    //
    ui->wgtQrCode->setStyleSheet("QFrame#wgtQrCode { border: 1px solid rgb(61, 62, 64); } QLabel { color:rgb(204,204,204); }");

}

void FormDevActivate::showEvent(QShowEvent *_evt)
{
    //
    CBaseWidget::showEvent(_evt);

    //

}

FormDevActivate *FormDevActivate::instance()
{
    if (!s_instance) {
        s_instance = new FormDevActivate();
    }
    return s_instance;
}

FormDevActivate::~FormDevActivate()
{
    delete ui;
}

void FormDevActivate::on_btnBack_clicked()
{
    this->hide();
}
