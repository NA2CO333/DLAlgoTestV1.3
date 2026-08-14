#include "circlshowthread.h"

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "QDebug"
#include "QTime"

#include "camerainit.h"

//
bool pyrState = false;
CvPoint3D32f pyrInfo[2];

static IplImage *srcImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U,1);
static IplImage *destImg = cvCreateImage(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U,1);

CirclShowThread::CirclShowThread(QObject *parent):
    QThread(parent)
{
    qDebug()<<"new CirclShowThread++++++++++++"<<this->currentThreadId();
}


void CirclShowThread::complcircl(unsigned char *frameBuffer)
{
//    qDebug()<<"CirclShowThread::complcircl+++++++++++++++++++"<<this->currentThreadId();

    if(frameBuffer==NULL){
        qDebug()<<"frameBuffer==NULL";
        return;
    }
//    qDebug()<< "enter complcicle+++++++++";
    QTime complcirclTime = QTime::currentTime();
    unsigned char* resultBYTE = findPyr(frameBuffer);
    QTime finishcomplcirclTime = QTime::currentTime();
    int caneraolaytime = complcirclTime.msecsTo(finishcomplcirclTime);

//        qDebug()<<"**************** complcircl time = "<<caneraolaytime;

    
    if(resultBYTE){
        emit resultcircl(resultBYTE);
//        qDebug()<< "send resultcircle+++++++++++++++";
    }
    else{
        qDebug()<< "reslutBYTE is NULL!";

    }

}

void CirclShowThread::pyrLocate(CvPoint3D32f pyr[2])
{
    pyrInfo[0].x = pyr[0].x;
    pyrInfo[0].y = pyr[0].y;
    pyrInfo[0].z = pyr[0].z;

    pyrInfo[1].x = pyr[1].x;
    pyrInfo[1].y = pyr[1].y;
    pyrInfo[1].z = pyr[1].z;

    pyrState = true;
    qDebug()<<"CirclShowThread::pyrLocate recived:"<<QTime::currentTime();
}



unsigned char *CirclShowThread::findPyr(unsigned char *frameBuffer)
{
//    qDebug()<< "enter findPyr+++++++++";

//    IplImage *srcImg;
//    IplImage *destImg;
//    srcImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT),IPL_DEPTH_8U,1);
//    destImg = cvCreateImage(cvSize(IMG_WIDTH, IMG_HEIGHT),IPL_DEPTH_8U,1);
    cvSetData(srcImg,frameBuffer,IMG_WIDTH);
//    cvSetZero(destImg);
//    cvCopy(srcImg,destImg);

    cvEqualizeHist(srcImg,destImg);

    //描圆

//    if(pyrState){

//        cvCircle(destImg, cvPoint(cvRound(pyrInfo[0].x),cvRound(pyrInfo[0].y)), cvRound(pyrInfo[0].z), CV_RGB(255,255,255), 2, 8, 0 );
//        cvCircle(destImg, cvPoint(cvRound(pyrInfo[1].x),cvRound(pyrInfo[1].y)), cvRound(pyrInfo[1].z), CV_RGB(255,255,255), 2, 8, 0 );

//        pyrState = false;
//    }


    return (unsigned char*)destImg->imageData;

}
