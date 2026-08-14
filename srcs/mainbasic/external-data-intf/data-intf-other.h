#ifndef CDATAINTFOTHER_H
#define CDATAINTFOTHER_H

#include <QObject>

#include "data.h"

// 其它第三方对接功能的封装
class CDataIntfOther : public QObject
{
    Q_OBJECT
public:
    explicit CDataIntfOther(QObject *parent = nullptr);

    // 山东勤成：判断是否本系统的二维码
    static bool shanDongQinCheng_isMyQrCode(const QByteArray &_code);

    // 山东勤成：二维码解析
    static bool shanDongQinCheng_parseQrCode(const QByteArray &_code, CPatient &_pat, QString &_err_msg);

signals:

protected:
    static const char * const S_CLASS_NAME;     // 本类的类名

};

#endif // CDATAINTFOTHER_H
