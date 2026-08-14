#ifndef QRCODEINPUT_H
#define QRCODEINPUT_H

#include <QDialog>
#include <QWidget>
#include <QTimer>
#include <QKeyEvent>

#include "baseform.h"

namespace Ui {
class qrcodeInput;
}

//
class qrcodeInput : public CBaseDialog
{
    Q_OBJECT

public:
    explicit qrcodeInput(QWidget *parent = 0);
    ~qrcodeInput();

signals:
    void sendQRcodedata(QString);

protected:
    void keyPressEvent(QKeyEvent*);
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

private slots:
    void on_pushButton_back_clicked();
    void readBarcodeSLot();
    void slotKbReaderGetline(QByteArray _line_bytes);

private:
    Ui::qrcodeInput *ui;
    bool barcodeMode;
    QString barcodeData;
    QTimer readBarcode;

};

#endif // QRCODEINPUT_H
