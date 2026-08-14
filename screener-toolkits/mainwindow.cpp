#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QDir>
#include <QFile>

#include "global.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

//#ifndef DESKTOP
//    ui->btnQuit->setVisible(false);
//#endif

    serialDataRetrans = new CSerialDataRetrans;

    ui->lblVersion->setText(global->version);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slotAboutToExit()
{
#ifndef DESKTOP
    //system("echo 0 > /sys/class/backlight/pwm-backlight/brightness");
#endif

}

void MainWindow::on_btnQuit_clicked()
{
    this->slotAboutToExit();

    QApplication::exit(EXIT_SUCCESS);
}

void MainWindow::on_btnQuitToScreener_clicked()
{
#if (1 == OS_TYPE || 2 == OS_TYPE)
    QString app_name = "main";
#else
    QString app_name = "screener";
#endif
    QString name_sub = "_desktop";

    if (qAppName().endsWith(name_sub))
        app_name += name_sub;
    QString file_path = qApp->applicationDirPath() + QDir::separator() + app_name;

    bool is_exit = true;
    bool is_run = true;
    if (!QFile::exists(file_path)) {
        is_run = false;
        QMessageBox::StandardButton button = QMessageBox::question(this, "Question", "File not found!\nExit?");
        if (QMessageBox::No == button)
            is_exit = false;
    }

    if (is_exit) {
        this->slotAboutToExit();
        QApplication::exit(EXIT_SUCCESS);
    }

    if (is_run) {
        system((file_path + " &").toLocal8Bit().data());
    }
}

void MainWindow::on_btnPhotoTest_clicked()
{
    global->showWinPhotoTest();
}

void MainWindow::on_ckbDebugComRetrans_clicked(bool checked)
{
    QString msg;
    bool succ = serialDataRetrans->setIsActive(checked, &msg);
    if (!succ) {
        QMessageBox::critical(this, "error", msg);
    }
}

void MainWindow::on_btnMkRk3568UsbMountPath_clicked()
{
    // 检查创建 /media/usb0 ~ /media/usb7 路径，解决 rk3568 平台 screener_1.5.0_20230111_35 及之前版本软件在卸载 U 盘后删除 U 盘路径导致 U 盘自动挂载失败的问题
    for (int i = 0; i < 8; i++) {
        QString path = QString::asprintf("/media/usb%d", i);
        if (!QFile::exists(path)) {
            std::system((QString("mkdir ") + path).toLocal8Bit().data());
        }
    }
    QMessageBox::information(this, "message", "finished");
}

void MainWindow::on_btnReboot_clicked()
{
#if (2 != OS_TYPE)
   std::system("reboot");
#endif
}

