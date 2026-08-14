#include "appsetting.h"

#include <QDebug>
#include <QApplication>
#include <QDir>

QMutex *appSetting::mLock = NULL;     //读写锁指针(保护代码或文件)
QSettings *appSetting::iniSetting = NULL;     //ini文件指针

#define DEFAULT_FILE_NAME   "manylinks"

void appSetting::init(QString _file_name)
{
    if (_file_name.length() == 0) {
        _file_name = DEFAULT_FILE_NAME;
    }

    iniSetting  = new QSettings(qApp->applicationDirPath() + QDir::separator() + _file_name, QSettings::IniFormat);

    qDebug() << "setting file name is " << iniSetting->fileName();

    mLock       = new QMutex;
}

QVariant appSetting::value(const QString &key, QVariant _default)  //获取系统参数(.ini文件)
{
    QVariant value;
    mLock->lock();
    if (iniSetting->contains(key)) {
        value = iniSetting->value(key);    //QVariant万能的数据类型
    } else {
        iniSetting->setValue(key, _default);
        value = _default;
    }
    mLock->unlock();
    return  value;
}

void appSetting::setValue(const QString &key, const QVariant &value)    //写入参数(.ini文件)
{
    mLock->lock();
    iniSetting->setValue(key,value);
    iniSetting->sync();
    mLock->unlock();
}

void appSetting::sync()
{
    iniSetting->sync();
}

