#include "enhancementimag.h"

#include <QDebug>
#include <QTime>

#include "camerainit.h"

//
IplImage *enhancementImag::srcImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U,1);
IplImage *enhancementImag::destImg = cvCreateImage(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U,1);

//
enhancementImag::enhancementImag(QObject *parent) : QThread(parent)
{
    isReady = false;
    qDebug()<<"new enhancementImag threads"<<this->currentThreadId();
}

void enhancementImag::run()
{
    forever
    {
        if(!runState)
            break;
        if(isReady)
        {
            cvEqualizeHist(srcImg,destImg);
            sendImgBack((uchar*)(destImg->imageData));
            isReady = false;
        }

        msleep(2);
    }

    qDebug()<<"exit enhancementImag thread ****************"<<this->currentThreadId();
}

void enhancementImag::enhancementSlot(unsigned char *frameBuffer)
{
    if(frameBuffer==NULL)
    {
        qDebug()<<"frameBuffer==NULL";
        return;
    }
    cvSetData(srcImg,frameBuffer, IMG_WIDTH);
    isReady = true;
}

void enhancementImag::setRunState(bool state)
{
    runState = state;
}
