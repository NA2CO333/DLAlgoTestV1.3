#ifndef DETECTBARCODE_H
#define DETECTBARCODE_H

#include "opencv2/imgproc/imgproc.hpp"

#if (OS_TYPE != 2)
#  include "BarcodeDecoder.h"
#else
#  include "testdesktop.h"
#endif

//
class detectBarcode : public QObject
{
    Q_OBJECT
public:
    explicit detectBarcode(QObject *parent = 0);

public slots:
    void slotDetectBarcode(unsigned char *_img_data);

signals:
    void sigDetectionResult(bool _is_succ, QString _decode_data);

private:
    BarcodeDecoder barcode;

};

#endif // DETECTBARCODE_H
