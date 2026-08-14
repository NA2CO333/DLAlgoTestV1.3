#include "shellsimulate.h"
#include "ui_shellsimulate.h"

#include <QPlainTextEdit>

#include "util-common.h"

shellsimulate::shellsimulate(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::shellsimulate)
{
    ui->setupUi(this);

//if (OS_TYPE == 2)
//    this->setWindowFlag(Qt::WindowStaysOnTopHint, true);
//#else
//    this->setWindowFlag(Qt::X11BypassWindowManagerHint, true);
//#endif

    //this->setStyleSheet("background-color:rgb(250,250,250);");

}

shellsimulate::~shellsimulate()
{
    delete ui;
}

void shellsimulate::keyPressEvent(QKeyEvent *e)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
    if (e->key() == Qt::Key_Return) {
        QString in_str = ui->edtInput->text();
        QString out_str = "";
        out_str += "> " + in_str + "\n";

        QString std_out;
        Util::executeLinuxCmd(in_str, &std_out);

        out_str += std_out + "\n";
        ui->txtOutput->insertPlainText(out_str);
        ui->txtOutput->ensureCursorVisible();
    }
}

void shellsimulate::showEvent(QShowEvent *)
{
    QApplication::setActiveWindow(this);    // TODO: 如果不加这个，第二次 show 的时候，焦点仍在 CEngineerMode （父窗口）？
    ui->edtInput->setFocus();
}

void shellsimulate::on_btnClear_clicked()
{
    ui->txtOutput->clear();
    //ui->edtInput->clear();
    ui->edtInput->setFocus();
}

void shellsimulate::on_btnGoBack_clicked()
{
    this->accept();
    //this->hide();
}
