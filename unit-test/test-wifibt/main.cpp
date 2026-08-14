#include "wintestrkwifibt.h"
#include <QApplication>

#include "global.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

#if (1 == OS_TYPE)
    system("echo 75 > /sys/class/backlight/pwm-backlight/brightness");
#endif

    CWinTestRkWifiBt w;
    QObject::connect(&w, &CWinTestRkWifiBt::sigHidden, &a, &QApplication::quit);

    CGlobal::isDebugMode = true;

    logger()->setIsEnabled(true);
    logger()->setFilter(CGlobal::LOG_BLUETOOTH);

    w.show();

    return a.exec();
}
