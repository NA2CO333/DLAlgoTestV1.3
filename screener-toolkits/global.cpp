#include "global.h"

CGlobal *global = Q_NULLPTR;

QString CGlobal::version = "v1.1";

CGlobal::CGlobal(QObject *parent) : QObject(parent)
{

}

CGlobal::~CGlobal()
{

}

void CGlobal::showWinHome()
{
    showWin<MainWindow>();
}

void CGlobal::showWinPhotoTest()
{
    showWin<WinPhotoTest>();
}
