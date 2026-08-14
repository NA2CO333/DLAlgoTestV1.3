#include "formmsgviewer.h"
#include "ui_formmsgviewer.h"

//
FormMsgViewer *g_MsgViewer = Q_NULLPTR;

FormMsgViewer *getMsgViewer()
{
    if (!g_MsgViewer) {
        g_MsgViewer = new FormMsgViewer();
    }
    return g_MsgViewer;
}

//
FormMsgViewer::FormMsgViewer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormMsgViewer)
{
    ui->setupUi(this);
}

FormMsgViewer::~FormMsgViewer()
{
    delete ui;
}

void FormMsgViewer::setText(QString &_txt)
{
    ui->txtMsgs->setText(_txt);
}

void FormMsgViewer::setText(QStringList &_list_str)
{
    QString txt;
    for (int i = 0; i < _list_str.length(); i++) {
        txt += _list_str[i] + "\n";
    }
    setText(txt);
}

void FormMsgViewer::showEvent(QShowEvent *_event)
{
    this->setWindowState(Qt::WindowMaximized);
}

