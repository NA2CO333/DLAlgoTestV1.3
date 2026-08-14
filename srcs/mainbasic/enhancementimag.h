#ifndef ENHANCEMENTIMAG_H
#define ENHANCEMENTIMAG_H

#include <QObject>
#include <QThread>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"

class enhancementImag : public QThread      // TODO: 未用到，丢弃？
{
    Q_OBJECT
public:
    explicit enhancementImag(QObject *parent = 0);
    void run();
    void setRunState(bool);

signals:
    void sendImgBack(unsigned char *frameBuffer);

public slots:
    void enhancementSlot(unsigned char *frameBuffer);

private:
    static IplImage *srcImg,*destImg;
    bool isReady;
    bool runState;

};

#endif // ENHANCEMENTIMAG_H
