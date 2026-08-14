#include "pipewrite.h"

#include <QFile>
#include <QTime>
#include <QDebug>
#include <QCoreApplication>
#include <QApplication>

#include <unistd.h>
//#include <signal.h>

//#include "capturethread.h"
//#include "messagewin.h"

// add by wim
//extern uchar *g_readBuf;
//extern CaptureThread *g_m_thread;

//void catchSignal(int sig)
//{
//    qDebug() << "catch signal " << sig;

//    if(QWiFiList::hasWifiOpened)
//    {
//        // show warning
//        MessageWin mess;
//        mess.setContent("更新成功正在重新启动");
//        mess.show();
//        qApp->processEvents();

//        QThread::sleep(2);
//        globalManager()->reboot();
//    }
//    signal(SIGTERM, SIG_IGN);
//    signal(SIGKILL, SIG_IGN);
//    signal(SIGINT, SIG_IGN);
//    signal(SIGSEGV, SIG_IGN);

//    if(gCameraHandle > 0)
//        CameraSetTriggerMode(gCameraHandle, cameraTriggerMode_Auto);

//    if(g_m_thread)
//    {
//        g_m_thread->stop();
//        g_m_thread->wait(10000);
//    }
//    if(g_readBuf!=NULL)
//    {
//        free(g_readBuf);
//        g_readBuf=NULL;
//    }

//    if(gCameraHandle > 0)
//    {
//        //相机反初始化。释放资源。
//        qDebug() << "camera stop: " << CameraStop(gCameraHandle);
//        qDebug() << "camera uninit: " << CameraUnInit(gCameraHandle);
//        gCameraHandle=-1;
//    }
//    else
//        qDebug() << "gCameraHandle = " << gCameraHandle;

//    qApp->exit(-1);
//}

PipeWrite::PipeWrite(QObject *parent) : QThread(parent)
{
    if(qApp->arguments().size() < 2)
        return;

    msg = "I am alive";

    pipeWrite = qApp->arguments().at(1).toInt();
}

PipeWrite::~PipeWrite()
{
}

void PipeWrite::run()
{
//    qDebug() << "register signal handler\n\n\n\n\n";
//    signal(SIGTERM, catchSignal);
//    signal(SIGKILL, catchSignal);
//    signal(SIGINT, catchSignal);
//    signal(SIGSEGV, catchSignal);

    if (qApp->arguments().size() < 2) {     // TODO: app 的 arguments 不应在 main() 函数之外使用，不熟悉的人维护可能出问题
        return;
    }

    while(1)
    {
        QTime time = QTime::currentTime();
        qDebug() << "write pipe(" << time.toString("hh:mm:ss") << ")";
        write(pipeWrite, (void *)msg.data(), msg.size());
        QThread::msleep(500);

    }
}
