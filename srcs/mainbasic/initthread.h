#ifndef INITTHREAD
#define INITTHREAD

#include <QThread>
#include <QImage>


class InitThread : public QThread
{
    Q_OBJECT
public:
    explicit InitThread(QObject *parent = 0);

public:
    void run();



private:


public slots:

};


#endif // INITTHREAD

