#include <QApplication>

#include "mainwindow.h"

#include "logger.h"
#include "algo.h"

//
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#ifndef RUN_IN_DESKTOP
    system("echo 75 > /sys/class/backlight/pwm-backlight/brightness");
#endif

    MainWindow w;
    QObject::connect(&a, &QApplication::aboutToQuit, &w, &MainWindow::slotAboutToExit);

    logger()->setIsEnabled(true);
    logger()->setFilter(LOG_TAG);

    w.show();

    return a.exec();
}
