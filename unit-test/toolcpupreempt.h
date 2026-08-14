#ifndef CTOOLCPUPREEMPT_H
#define CTOOLCPUPREEMPT_H

#include <QThreadPool>
#include <QRunnable>
#include <QVector>

// 前置声明
class CTask;

// 抢占 CPU 资源，形成一个 CPU 资源缺乏的环境
class CToolCpuPreempt
{
public:
    CToolCpuPreempt();

    void start();
    void stop();

protected:
    QThreadPool *pool = Q_NULLPTR;

    QVector<CTask *> *listTask = Q_NULLPTR;

};

//
class CTask : public QRunnable
{
public:
    void run() override;

    void reset();
    void setStopFlag();

protected:
    bool flagStop = false;


};


#endif // CTOOLCPUPREEMPT_H
