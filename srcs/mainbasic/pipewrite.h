#ifndef PIPEWRITE_H
#define PIPEWRITE_H

#include <QObject>
#include <QThread>

//
class PipeWrite : public QThread
{
    Q_OBJECT

public:
    PipeWrite(QObject *parent=0);
    ~PipeWrite();

    void run();

private:
    int pipeWrite;
    QByteArray msg;
};

#endif // PIPEWRITE_H
