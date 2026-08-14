#include <QVector>
#include <QDebug>

#include "myeditline.h"

#include "windowsmanager.h"

//
myEditLine::myEditLine(QWidget *parent) : QLineEdit(parent)
{
    //ui = new QLineEdit;
    //ui->resize(200,50);
    //ui->setFocusPolicy(Qt::NoFocus);
    //this->setStyleSheet("border:1px solid rgb(200,200,200);border-radius:5px;padding:2px 4px;");
    this->setObjectName("m_edit");
    preEdit = NULL;
    nextEdit = NULL;

}

myEditLine::~myEditLine()
{
    qDebug()<<"myEditLine::~myEditLine()";
    //delete ui;
}

void myEditLine::keyPressEvent(QKeyEvent *event)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
#if (OS_TYPE != 2)
    QWidget::keyPressEvent(event);  // 屏蔽 QLineEdit 对键盘事件的处理
#else
    QLineEdit::keyPressEvent(event);
#endif
}

void myEditLine::mousePressEvent(QMouseEvent *)
{
    qDebug()<<"myEditLine mousePressEvent:"<<this;

    if (this->isReadOnly() || !this->isEnabled()) {
        return;
    }

    isKeyboardUpdated = false;
    getWinManage()->showKeyboard(this, Q_NULLPTR);
}

void myEditLine::updateText(QString _text)
{
    const QString text_old = this->text();
    if (text_old != _text) {
        qDebug() << "sender: " << sender() << ", updateText: " << _text;

        this->setText(_text);
        this->update();

        emit textEdited(_text);
    }

    isKeyboardUpdated = true;

    getWinManage()->hideKeyboard();
}

void myEditLine::setEditName(QString str)
{
    editName = str;

    //
    //this->setPlaceholderText(editName);
}

QString myEditLine::getEditName()
{
    return editName;
}

void myEditLine::setPreNext(myEditLine *_preEdit, myEditLine *_nextEdit)
{
    if(_preEdit != NULL)
        preEdit = _preEdit;
    if(_nextEdit != NULL)
        nextEdit = _nextEdit;
}

myEditLine* myEditLine::getPreEdit()
{
    if (preEdit != NULL) {
        return preEdit;
    } else {
        return Q_NULLPTR;
    }
}

myEditLine* myEditLine::getNextEdit()
{
    if (nextEdit != NULL) {
        return nextEdit;
    } else {
        return Q_NULLPTR;
    }
}

bool myEditLine::getIsKeyboardUpdated()
{
#if (OS_TYPE != 2)
    return isKeyboardUpdated;
#else
    return true;
#endif
}

///=================================================================================================
/// class mySpinBox

mySpinBox::mySpinBox(QWidget *parent) : QSpinBox(parent)
{

}

///=================================================================================================
/// class myComboBox

myComboBox::myComboBox(QWidget *parent) : QComboBox(parent)
{
    myEditLine *edit = new myEditLine(this);
    this->setLineEdit(edit);
}
