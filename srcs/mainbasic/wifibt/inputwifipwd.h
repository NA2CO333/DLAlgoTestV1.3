#ifndef INPUTWIFIPWD_H
#define INPUTWIFIPWD_H

#include <QWidget>
#include <QSettings>
#include "myeditline.h"

namespace Ui {
class inputWiFipwd;
}

//
class inputWiFipwd : public QWidget
{
    Q_OBJECT

public:
    explicit inputWiFipwd(QWidget *parent);
    ~inputWiFipwd();

    void EnterPwdText(const QString &_ssid, const QString &_pwd);

signals:
    QString sigGotPwd(QString _ssid, QString _password);
    QString sendCancelFlag(bool);

private slots:
    void on_pushButton_Ok_clicked();
    void on_pushButton_Cancel_clicked();

protected:
    void paintEvent(QPaintEvent *event);
    void showEvent(QShowEvent *);

private:
    Ui::inputWiFipwd *ui;
};

#endif // INPUTWIFIPWD_H
