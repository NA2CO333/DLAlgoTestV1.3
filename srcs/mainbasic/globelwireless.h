#ifndef GLOBELWIRELESS_H
#define GLOBELWIRELESS_H

#include <QObject>
#include <QVector>

struct ts_Wireless{
    QString type;
    QString mobile;
    QString unicom;
    QString telecom;
};

struct Wireless{
    QVector<ts_Wireless> wireless;

    QStringList getType(){
        QStringList res;
        foreach(ts_Wireless ts, wireless){
            res.append(ts.type);
        }
        return res;
    }
};

#endif // GLOBELWIRELESS_H
