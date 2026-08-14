#ifndef LOGINWIN_H
#define LOGINWIN_H

#include <QWidget>
#include <QSettings>
#include <QTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "baseform.h"
#include "myeditline.h"
#include "messagewin.h"
#include "statusbarform.h"
#include "globaltypes.h"

namespace Ui {
class loginWin;
}

//
class loginWin : public CBaseWidget
{
    Q_OBJECT

public:
    explicit loginWin(QWidget *parent = 0);
    ~loginWin();

    enum workMode{
        login = 0,
        oldPassWord,
        newPassWord,
        confirmPassWord,
    };

    void setMode(workMode);

private slots:
    void on_pushButton_confirm_clicked();
    void on_pushButton_Back_clicked();
    void on_pushButton_Home_clicked();

signals:
    void showMainwindow(enSysSignal _sys_signal);

protected:
    void showEvent(QShowEvent *);

private:
    Ui::loginWin *ui;
    myEditLine *UsrName;
    myEditLine *PassWord;
//    QSettings *mSetting;
    int missCnt;
    QTime missBeginTime;
    QGridLayout *gLayout;
    QHBoxLayout *hLayout;
    QVBoxLayout *vLayout;

    workMode mode;
    QString tempPassword,tempUsrName;
    QMovie *mMovie;
};

#endif // LOGINWIN_H
