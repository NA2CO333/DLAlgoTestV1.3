#ifndef CIRCLSHOWTHREAD_H
#define CIRCLSHOWTHREAD_H

#include <QThread>

#include "opencv2/imgproc/imgproc.hpp"


class CirclShowThread : public QThread
{
    Q_OBJECT
public:
    explicit CirclShowThread(QObject *parent = 0);
    unsigned char* findPyr(unsigned char *frameBuffer);

signals:
    void resultcircl(unsigned char* frameBuffer);

public slots:
    void complcircl(unsigned char *frameBuffer);
    void pyrLocate(CvPoint3D32f[]);
};

#endif // CIRCLSHOWTHREAD_H
