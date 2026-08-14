#include "unittestintf.h"

MySettings *appSettings = new MySettings(QString("./settings.ini"), MySettings::IniFormat);
QString theme;

MySettings::MySettings(const QString &fileName, Format format, QObject *parent) : QSettings(fileName, format, parent)
{

}

void MySettings::setValueSync(const QString &key, const QVariant &value)
{
    setValue(key, value);
    sync();
}

SystemSettings::SystemSettings(QWidget* parent) : QWidget(parent)
{

}

SystemSettings *SystemSettings::instance()
{
    return new SystemSettings();
}
