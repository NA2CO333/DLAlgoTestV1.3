#include "mainwindow.h"
#include <QApplication>

#include "global.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    global = new CGlobal;

#if (1 == OS_TYPE)
    system("echo 50 > /sys/class/backlight/pwm-backlight/brightness");
#endif

    global->showWinHome();

    return a.exec();
}
