#include "qrcodeinput.h"
#include "ui_qrcodeinput.h"

#include <QDebug>

#include "global.h"

//
qrcodeInput::qrcodeInput(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::qrcodeInput)
{
    ui->setupUi(this);
    barcodeMode = false;
    connect(&readBarcode,SIGNAL(timeout()),this,SLOT(readBarcodeSLot()));

    //if (language)
    //    this->ui->displayContain_label->setText("请使用扫码枪扫码");
    //else
    //    this->ui->displayContain_label->setText("Please scan the code");

}

qrcodeInput::~qrcodeInput()
{
    delete ui;
    qDebug()<<"~qrcodeInput()";
}

void qrcodeInput::on_pushButton_back_clicked()
{
    this->reject();
    //this->deleteLater();
}

void qrcodeInput::readBarcodeSLot()
{
    static int len_last = 0;

    qDebug()<<"send barcodedata:"<<barcodeData;
    int len_curr = barcodeData.length();
    if (len_curr == len_last) {
        len_last = 0;

        emit sendQRcodedata(barcodeData);
        readBarcode.stop();
        this->accept();
        //this->deleteLater();
    }
    len_last = len_curr;
}

void qrcodeInput::slotKbReaderGetline(QByteArray _line_bytes)
{
    barcodeData = QString::fromUtf8(_line_bytes.data());
    barcodeData.replace("\r", "");
    barcodeData.replace("\n", "");

    readBarcodeSLot();
}

void qrcodeInput::keyPressEvent(QKeyEvent *e)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
#if (OS_TYPE == 2)
    if (!CGlobal::isReadBarcodeByQt) {
        return;
    }

    if (e->text() != ""){
        qDebug() << e->text();
        barcodeData.append(e->text());
        if(!barcodeMode){
            barcodeMode = true;
            qDebug()<<"barcodeMode = true";

            readBarcode.start(500);
        }
    }
#endif
}

void qrcodeInput::showEvent(QShowEvent *)
{
    if (!CGlobal::isReadBarcodeByQt) {
        QObject::connect(kbReader(), &Util::CBarcodeDataDecoder::sigGetLine, this, &qrcodeInput::slotKbReaderGetline, Qt::QueuedConnection);
        kbReader()->RegListener(this);
    }

    CBaseFormIntf::centerWidget(this);

}

void qrcodeInput::hideEvent(QHideEvent *)
{
    if (!CGlobal::isReadBarcodeByQt) {
        QObject::disconnect(kbReader(), &Util::CBarcodeDataDecoder::sigGetLine, this, &qrcodeInput::slotKbReaderGetline);
        kbReader()->UnregListener(this);
    }

}
