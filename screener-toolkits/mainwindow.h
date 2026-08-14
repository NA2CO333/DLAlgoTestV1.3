#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "serialdataretrans.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void slotAboutToExit();

private slots:
    void on_btnQuit_clicked();
    void on_btnQuitToScreener_clicked();
    void on_btnPhotoTest_clicked();
    void on_ckbDebugComRetrans_clicked(bool checked);
    void on_btnMkRk3568UsbMountPath_clicked();

    void on_btnReboot_clicked();

private:
    Ui::MainWindow *ui;

    CSerialDataRetrans *serialDataRetrans;

};

#endif // MAINWINDOW_H
