#ifndef TEST_H
#define TEST_H

#include <QWidget>
#include <QSettings>

class MySettings : public QSettings
{
public:
    MySettings(const QString &fileName, Format format, QObject *parent = 0);

public:
    void setValueSync(const QString &key, const QVariant &value);
};

class SystemSettings : public QWidget
{
    Q_OBJECT

public:
    static SystemSettings *instance();

signals:
    void themeChanged(QString theme);

protected:
    SystemSettings(QWidget* parent = 0);

};

#endif // TEST_H
