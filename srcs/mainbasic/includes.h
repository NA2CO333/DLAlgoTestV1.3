#ifndef INCLUDES_H
#define INCLUDES_H

#include <QObject>
#include "globelwireless.h"

struct Configure{
    QString modular;
    QString path;
    QString platform;
    Wireless wireless;
};

extern Configure configure;

#endif // INCLUDES_H
