#ifndef PRINTERTRANSMIT_H
#define PRINTERTRANSMIT_H

#include <QObject>
#include <QSettings>

#include "mysqlitepatients.h"
#include "bluetoothintf.h"
#include "report-text.h"

//
class printerTransmit : public QObject
{
    Q_OBJECT
public:
    explicit printerTransmit(QObject *parent = 0);
    ~printerTransmit();

    void sendToPrint(QString);
    static int checkWifiPrinterConnect();   // 检查 WiFi 小票打印机连接状态。返回：0:打印机已连接，1:网络未连接，2:打印机连接失败
    void setBtConnection(CBtConnection *_bt_conn);

signals:
    void sendToBtPrint(QByteArray);
    void sigDataSendFinished(bool _is_succ, QString _err_msg = "");     // 事件：数据发送完成

public slots:
    void slotPrintTicket(CPatient _pat, QString _judgement_desc);       // 打印小票

protected:
    CBtConnection *btConnection = Q_NULLPTR;

};

#endif // PRINTERTRANSMIT_H
