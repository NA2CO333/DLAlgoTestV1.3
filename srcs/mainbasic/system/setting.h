#ifndef SETTING_H
#define SETTING_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QProcess>
#include <stdio.h>

//
class setting : public QObject
{
    Q_OBJECT
public:
    explicit setting(QObject *parent = 0);

public:
    //_ts_gunSetting gun[sysconfig_chargegun_MAX];

    // QString Num2Str(int val);
    static void get_bool(QSettings *ini,QString obj,bool *val);
    static void set_bool(QSettings *ini,QString obj,bool *val);
    static void get_int(QSettings *ini,QString obj,int *val);
    static void set_int(QSettings *ini,QString obj,int *val);
    static void get_string(QSettings *ini,QString obj,QString *val);
    static void set_string(QSettings *ini,QString obj,QString *val);

    static int myexec(const char *cmd){
        FILE *pp = popen(cmd, "r"); //建立管道
        if (!pp) {
            return -1;
        }
        char tmp[1024]; //设置一个合适的长度，以存储每一行输出
        while (fgets(tmp, sizeof(tmp), pp) != NULL) {
            if (tmp[strlen(tmp) - 1] == '\n') {
                tmp[strlen(tmp) - 1] = '\0'; //去除换行符
            }
        }
        pclose(pp); //关闭管道
        return 0;
    }
signals:

public slots:
};

#endif // SETTING_H
