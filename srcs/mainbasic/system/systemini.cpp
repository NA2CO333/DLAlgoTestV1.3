#include "systemini.h"
#include "system/setting.h"
#include <QSettings>
#include <QDebug>
#include "includes.h"

SystemIni::SystemIni(QObject *parent) : QObject(parent)
{

}

/***********************************
函数名称：   readIni()
输入：         NULL
输出：         NULL
功能：         读取配置文件
***********************************/
void SystemIni::readIni()
{
    QSettings *ini = new QSettings("/opt/PDA/configs/EmbedSky.ini", QSettings::IniFormat);
    ini->beginGroup("SYSTEM");
    setting::get_string(ini, "platform", &configure.platform);
    ini->endGroup();
    QString type;
    ini->beginGroup("3G_4G");
    setting::get_string(ini, "type", &type);
    ini->endGroup();
    QStringList typeList = type.split(",");
    configure.wireless.wireless.clear();
    foreach(QString str, typeList){
        ts_Wireless wireless;
        wireless.type = str;
        ini->beginGroup(str);
        setting::get_string(ini, "yidong", &wireless.mobile);
        setting::get_string(ini, "dianxin", &wireless.telecom);
        setting::get_string(ini, "liantong", &wireless.unicom);
        ini->endGroup();
        configure.wireless.wireless.append(wireless);
    }
    delete ini;
    ini = nullptr;
}

/***********************************
函数名称：   writeIni()
输入：         path 保存结果文件路径
输出：         NULL
功能：         保存测试结果
***********************************/
void SystemIni::writeIni(QString path)
{
    QSettings *ini = new QSettings(path, QSettings::IniFormat);
    delete ini;
    ini = nullptr;
}

