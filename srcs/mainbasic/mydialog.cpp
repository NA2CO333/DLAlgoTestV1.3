#include "mydialog.h"
#include "ui_mydialog.h"
#include <QPushButton>

myDialog::myDialog(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::myDialog)
{
    ui->setupUi(this);
    this->setGeometry(240,140,320,200);
    ui->label->setText(tr("confirm to poweroff ?"));
}

myDialog::~myDialog()
{
    delete ui;
}

void myDialog::setContent(const char *str)
{
    ui->label->setText(str);
}

void myDialog::setButtonText(QString button1, QString button2)
{
    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    QPushButton *cancelButton = ui->buttonBox->button(QDialogButtonBox::Cancel);

    okButton->setText(button1);
    cancelButton->setText(button2);
}
