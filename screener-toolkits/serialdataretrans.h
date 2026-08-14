#ifndef CSERIALDATARETRANS_H
#define CSERIALDATARETRANS_H

#include <QObject>
#include <QSerialPort>

class CSerialDataRetrans : public QObject
{
    Q_OBJECT
public:
    explicit CSerialDataRetrans(QObject *parent = 0);

    bool setIsActive(bool _is_active, QString *_msg = Q_NULLPTR);

signals:

public slots:

protected:
    QSerialPort *com1;
    QSerialPort *com2;

    bool isChinese = true;

    bool setComIsOpened(QSerialPort *_serial_port, bool _is_active, QString *_path = Q_NULLPTR, QString *_msg = Q_NULLPTR);

private slots:
    void slot_com1_readyRead();
    void slot_com2_readyRead();

};

#endif // CSERIALDATARETRANS_H
