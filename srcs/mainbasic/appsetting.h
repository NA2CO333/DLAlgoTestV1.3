#ifndef APPSETTING_H
#define APPSETTING_H

#include <QSettings>
#include <QMutex>

class appSetting
{
public:
    static void init(QString _file_name = "");
    static QVariant value(const QString &key, QVariant _default = QVariant());
    static void setValue(const QString &key, const QVariant &value);
    static void sync();
private:
    static QSettings *iniSetting;
    static QMutex   *mLock;
};

#endif // APPSETTING_H
