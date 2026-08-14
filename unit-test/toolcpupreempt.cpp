#include "toolcpupreempt.h"

#include <iostream>
#include <cmath>
#include <limits>

//
CToolCpuPreempt::CToolCpuPreempt()
{
    pool = QThreadPool::globalInstance();
    pool->setMaxThreadCount(100);

    listTask = new QVector<CTask *>;

    CTask *task = Q_NULLPTR;
    for (int i = 0; i < 100; i++) {
        task = new CTask;
        listTask->push_back(task);
    }

}

void CToolCpuPreempt::start()
{
    CTask *task = Q_NULLPTR;
    for (int i = listTask->size() - 1; i >= 0; i--) {
        task = listTask->at(i);
        task->reset();
        pool->start(task);
    }

}

void CToolCpuPreempt::stop()
{
    if (pool->activeThreadCount() == 0) {
        return;
    }

    CTask *task = Q_NULLPTR;
    for (int i = listTask->size() - 1; i >= 0; i--) {
        task = listTask->at(i);
        task->setStopFlag();
    }

    pool->waitForDone();

}

/// ============================================================================================
/// class CTask

void CTask::run()
{
    double a = 1;
    double b = 2;
    double c;
    while (!flagStop) {
        if (a >= std::sqrt(INT_MAX / 8)) {
            a = 1;
        }
        a ++;
        if (b >= std::sqrt(INT_MAX / 8)) {
            b = 1;
        }
        b ++;

        c = std::sqrt(std::pow(a, 2) + std::pow(b, 2));
        if (c < -1.0) {
            std::cout << c << std::endl;
        }

    }

}

void CTask::reset()
{
    flagStop = false;
}

void CTask::setStopFlag()
{
    flagStop = true;
}
