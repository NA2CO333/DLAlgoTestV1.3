#include "initthread.h"
#include "camera.h"
#include <QDebug>

extern Camera Camerareal;


InitThread::CaptureThread(QObject *parent) :
    QThread(parent)
{

}

void InitThread::run()
{
    Camerareal.init_SDK();
}
