#pragma once
#ifndef MYEDITLINE_H
#define MYEDITLINE_H
//written by sun

#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QCoreApplication>
#include <QSpinBox>
#include <QComboBox>

// 支持屏幕键盘的 QLineEdit 控件
class myEditLine : public QLineEdit
{
    Q_OBJECT

public:
    myEditLine(QWidget *parent = 0);
    ~myEditLine();
    void setEditName(QString);
    QString getEditName();
    void setPreNext(myEditLine  *_preEdit,myEditLine *_nextEdit);
    myEditLine* getPreEdit();
    myEditLine* getNextEdit();
    bool getIsKeyboardUpdated();

public slots:
    void updateText(QString _text);

protected:
    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QMouseEvent *);    //鼠标按下事件

private:
    //QLineEdit* ui;
    QString editName;
    myEditLine  *preEdit,*nextEdit;
    bool isKeyboardUpdated = false;
};

// 支持屏幕键盘的 QSpinox 控件        // TODO: 这个没用，只要 QAbstractSpinBox::setLineEdit(myEditLine) 即可实现屏幕键盘的支持
class mySpinBox : public QSpinBox
{
    Q_OBJECT

public:
    explicit mySpinBox(QWidget *parent = 0);

};

// 支持屏幕键盘的 ComboBox
class myComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit myComboBox(QWidget *parent = 0);

};

#endif // MYEDITLINE_H
