//TCP发送打印数据
#include "printertransmit.h"
#include <QtNetwork>
#include "windowsmanager.h"

#include "util-common.h"
#include "global.h"
#include "screener-report-receipt.h"

//
printerTransmit::printerTransmit(QObject *parent) : QObject(parent)
{

}

printerTransmit::~printerTransmit()
{

}

void printerTransmit::slotPrintTicket(CPatient _pat, QString _judgement_desc)
{
    //
    enSingleDualEyeMode single_dual_eye = Result::judgeSingleDualEyeMode(_pat);

    //
    CScreenerReportReceipt report;

    report.setData(_pat, _judgement_desc, single_dual_eye, CGlobal::visionNotation != visionNotation_None, Result::isCylNegative());
    report.updateData();

    //
    QString text, err_msg;
    bool succ = report.getText(text, err_msg);
    if (succ) {
        std::cout << "receipt report text: ======================\n" << text.toStdString() << std::endl;
    } else {
        logCritical("Failed to printing receipt: " + err_msg);
        emit sigDataSendFinished(false, err_msg);
        return;
    }

    //
    if (ticketPrintConnType_WiFi == CGlobal::ticketPrintConnType) {     // wifi 打印
        sendToPrint(text);
    } if (ticketPrintConnType_BT == CGlobal::ticketPrintConnType) {     // 蓝牙打印
        QTextCodec *pCodec = QTextCodec::codecForName("gb2312");
        QByteArray arrData = pCodec->fromUnicode(text);

        //emit sendToBtPrint(arrData);
        btConnection->pushSendingData(arrData);
    }

    //
    emit sigDataSendFinished(succ);
}

//
QTcpSocket tSocket;     //创建TCP套接字
//
void printerTransmit::sendToPrint(QString result)
{
    QString printerIP = appSetting::value("/printerip").toString();
    int printerPORT = appSetting::value("/printerport").toInt();

//    while(tSocket.state()!=QAbstractSocket::ConnectingState
//          && tSocket.state()!=QAbstractSocket::ConnectedState){
//        qDebug()<<"tSocket.state()="<<tSocket.state();
//        qDebug()<<"connect to printer...";
//        tSocket.connectToHost(printerIP,printerPORT,QTcpSocket::WriteOnly);
//        QThread::msleep(100);
//    }
    tSocket.connectToHost(printerIP, printerPORT, QTcpSocket::WriteOnly);   //连接打印机IP及端口
    if(tSocket.waitForConnected(2000))
    {
        qDebug() << "connect sucess----tSocket.state()=" << tSocket.state();

        QTextCodec *pCodec = QTextCodec::codecForName("GB18030");
        QByteArray arrData = pCodec->fromUnicode(result);
        tSocket.write(arrData);
        tSocket.waitForBytesWritten(500);
        tSocket.disconnectFromHost();
    }
    else
    {
        qDebug() << "connect failed";
    }
}

int printerTransmit::checkWifiPrinterConnect()
{

    QString printerIP = appSetting::value("/printerip").toString();
    int printerPORT = appSetting::value("/printerport").toInt();

    QTcpSocket testSock;
    testSock.connectToHost(printerIP, printerPORT, QTcpSocket::WriteOnly);
    if(g_WifiIntf->getIsConnected())
    {
        if(testSock.waitForConnected(1000))
        {
            qDebug()<<"打印机已连接："<<printerIP;
            testSock.disconnectFromHost();
            return 0;
        }
        else
        {
            qDebug()<<"打印机连接失败："<<printerIP;
            return 2;
        }
    }
    else            //网络未连接
        return 1;
}

void printerTransmit::setBtConnection(CBtConnection *_bt_conn)
{
    btConnection = _bt_conn;
}
