#ifndef CDATAINTFANHUISCREEN_H
#define CDATAINTFANHUISCREEN_H

#include <QObject>

#include "data.h"

// 数据接口 - “安徽筛查系统”
class CDataIntfAnHuiScreen : public QObject
{
    Q_OBJECT
public:
    explicit CDataIntfAnHuiScreen(QObject *_parent = nullptr);

    // 判断指定的二维码数据是否“安徽筛查系统”定义的格式
    static bool isMyQrCode(const QByteArray &_line_bytes);

    // 解析二维码内容，并赋值给 CPatient 对象
    static bool qrCodeToPatient(const QByteArray &_line_bytes, CPatient &_pat, QString &_err_msg);

signals:

};

#endif // CDATAINTFANHUISCREEN_H
