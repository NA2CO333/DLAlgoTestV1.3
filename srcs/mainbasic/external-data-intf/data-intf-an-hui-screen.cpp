#include "data-intf-an-hui-screen.h"

#include "globalclass.h"

//
namespace  {
    constexpr int C_COUNT_COMMA_MIN = 4;    // 最小逗号个数
}

//
CDataIntfAnHuiScreen::CDataIntfAnHuiScreen(QObject *_parent) : QObject(_parent)
{

}

bool CDataIntfAnHuiScreen::isMyQrCode(const QByteArray &_line_bytes)
{
    // NOTE： 文档《20260407_安徽筛查系统对接-视筛 20260407_崔继友.docx》
    /* 二维码内容格式：姓名,唯一标识,性别（1男2女）,年龄,
     * eg.:
     * 扫码后得到： "测试1,2603161404581308,1,0,\n"
     */
    // NOTE: Upload Url: "http://yj.ahyshr.cn:9004/common/screening/upIoadScreeningData"

    // 根据逗号个数来判断
    const int count_comma = _line_bytes.count(',');
    return (count_comma >= C_COUNT_COMMA_MIN);
}

bool CDataIntfAnHuiScreen::qrCodeToPatient(const QByteArray &_line_bytes, CPatient &_pat, QString &_err_msg)
{
    //
    _err_msg.clear();

    //
    QString qr_code = QString::fromUtf8(_line_bytes);
    qr_code.replace("\r", "");
    qr_code.replace("\n", "");

    //
    const QStringList fields = qr_code.split(',');
    static const int COUNT_FIELD_MIN = C_COUNT_COMMA_MIN + 1;       // 最小字段个数
    if (fields.size() >= COUNT_FIELD_MIN) {
        //
        const int gender = fields.at(2).toInt();
        const int age = fields.at(3).toInt();

        //
        _pat.patientname    = fields.at(0);
        _pat.patientid      = fields.at(1);
        _pat.patientsex     = (gender == 1 ? "M" : (gender == 2 ? "F" : ""));
        _pat.setAgeRange(CAgeRange::fromAge(age));

        return true;
    } else {
        _err_msg = tr("格式错误") + tr("：") + tr("字段个数须大于等于%1！").arg(COUNT_FIELD_MIN);
        // "FormatError", ": ", "The number of fields must be greater than or equal to %1!"
        return false;
    }
}
