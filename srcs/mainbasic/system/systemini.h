#ifndef SYSTEMINI_H
#define SYSTEMINI_H

#include <QObject>

class SystemIni : public QObject
{
    Q_OBJECT
public:
    explicit SystemIni(QObject *parent = 0);

    static void readIni();
    static void writeIni(QString path);
signals:

public slots:
};

#endif // SYSTEMINI_H
