#include "setting.h"


setting::setting(QObject *parent) : QObject(parent)
{

}
void setting::get_bool(QSettings *ini,QString obj,bool *val){
    QString res = ini->value(obj).toString();
    *val = (res == "true")?true:false;
}
void setting::set_bool(QSettings *ini,QString obj,bool *val){
    ini->setValue(obj,(*val == true)?"true":"false");
}
void setting::get_int(QSettings *ini,QString obj,int *val){
    QString res = ini->value(obj).toString();
    *val = res.toInt();
}
void setting::set_int(QSettings *ini,QString obj,int *val){
    ini->setValue(obj,QString::number(*val));
}

void setting::get_string(QSettings *ini,QString obj,QString *val){
    *val = ini->value(obj).toString();
}
void setting::set_string(QSettings *ini,QString obj,QString *val){
    ini->setValue(obj,*val);
}


