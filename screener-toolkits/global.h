#ifndef CGLOBAL_H
#define CGLOBAL_H

#include <QObject>

#include "mainwindow.h"
#include "winphototest.h"

class CGlobal : public QObject
{
    Q_OBJECT
public:
    explicit CGlobal(QObject *parent = 0);
    ~CGlobal();

public:
    void showWinHome();
    void showWinPhotoTest();

    static QString version;

protected:
    struct stWinInfo {
        QString className;
        QWidget *win;
    };

    template <typename T>
    void showWin() {
        QWidget *win = Q_NULLPTR;
        QString class_name = T::staticMetaObject.className();
        foreach (stWinInfo win_info, winInfos) {
            if (class_name == win_info.className) {
                win = win_info.win;
                break;
            }
        }

        if (!win) {
            win = new T;
            stWinInfo win_info;
            win_info.className = class_name;
            win_info.win = win;
            winInfos.append(win_info);
        }

        if (currentWin && currentWin != win) {
            currentWin->hide();
        }

        currentWin = win;
        currentWin->show();
    }

    QVector<stWinInfo> winInfos;
    QWidget *currentWin = Q_NULLPTR;

    MainWindow *winHome = Q_NULLPTR;
    WinPhotoTest *winPhotoTest = Q_NULLPTR;

signals:

public slots:

};

extern CGlobal *global;

#endif // CGLOBAL_H
