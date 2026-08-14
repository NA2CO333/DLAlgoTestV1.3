#include "detectbarcode.h"

#include <QTextCodec>
#include <QTime>

#include "mysqlitepatients.h"
#include "personalinfos.h"
#include "windowsmanager.h"

//
#define SCAN_WIDTH 340

//
static IplImage *tempBarcodeImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
static IplImage *calImg         = cvCreateImage(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
static IplImage *selectImg      = cvCreateImage(cvSize(SCAN_WIDTH, SCAN_WIDTH), IPL_DEPTH_8U, 1);

//
detectBarcode::detectBarcode(QObject *parent):
    QObject(parent)
{

}

//
void detectBarcode::slotDetectBarcode(unsigned char *_img_data)
{
    static int _scan_count = 0;

    //
    //QTextCodec *codec = QTextCodec::codecForName("GBK");
    //QTextCodec::setCodecForLocale(codec);

    cvZero(calImg);
    cvZero(selectImg);

    if(_img_data != NULL){
        cvSetData(tempBarcodeImg, _img_data, IMG_WIDTH);
        cvSetImageROI(tempBarcodeImg,cvRect(230,70,SCAN_WIDTH,SCAN_WIDTH));
        cvCopy(tempBarcodeImg,selectImg);
        cvResetImageROI(tempBarcodeImg);
    }
    else{
        qDebug()<<"detectBarcode recieve img error!";
        return;
    }


    std::string codeString;
    std::string tName;

    int ret = 0;

    ret = barcode.ScanImage(*selectImg,codeString,tName);

    if (ret == 0) {
        qDebug()<<"scanImage sucess****************";
        QString data = QString::fromStdString(codeString);
        QString decodeData  = QString::fromUtf8(QByteArray::fromPercentEncoding(data.toLatin1()));
        qDebug()<<"get data:"<<decodeData<<"\n******************************"<<_scan_count;

        emit sigDetectionResult(true, decodeData);
    }
    else
    {
        qDebug()<<"scanImage failed:"<<_scan_count;
        emit sigDetectionResult(false, "");
    }
}
