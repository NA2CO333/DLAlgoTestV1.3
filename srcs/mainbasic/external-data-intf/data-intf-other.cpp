#include "data-intf-other.h"

#include <QTextCodec>
#include <QStringList>

#include "logger.h"
#include "globalclass.h"

//
const char * const CDataIntfOther::S_CLASS_NAME = CDataIntfOther::staticMetaObject.className();

CDataIntfOther::CDataIntfOther(QObject *parent) : QObject(parent)
{

}

bool CDataIntfOther::shanDongQinCheng_isMyQrCode(const QByteArray &_code)
{
    /* 文档：《山东勤成二维码/二维码解码.txt》
     */

    if (!_code.isEmpty()) {
        static constexpr char SUFFIX = '@';             // 后缀

        int idx = _code.lastIndexOf('\n');
        if (idx >= 0) {
            idx -= 1;
            if (idx < 0) {
                return false;
            }
        } else {
            idx = _code.size() - 1;
        }

        return (_code.at(idx) == SUFFIX);
    } else {
        return false;
    }
}

bool CDataIntfOther::shanDongQinCheng_parseQrCode(const QByteArray &_code, CPatient &_pat, QString &_err_msg)
{
    /* 文档：《山东勤成二维码/二维码解码.txt》
     */

    /* 逗号分隔字符串的字段序：
     * Name = 字符串[0],//姓名
     * Sex = 字符串[1],//性别（“男”/“女”）
     * Age = 字符串[2],//年龄
     * IdCardNo =字符串[3]//身份证号
     *
     * （字段个数不止4个）
     */

    //
    _err_msg.clear();

    // 去掉最后的 '@'
    int idx = _code.lastIndexOf('@');
    if (idx != _code.size() - 2) {
        _err_msg = tr("格式错误") + tr("：") + tr("最后一个字符须是'@'！");     // "FormatError: the last char mast be '@'!"
        return false;
    }

    QByteArray code = _code.left(idx);

    // 将经过 Base64 编码的字符串反编码为字节数组
    QByteArray code_bytes = QByteArray::fromBase64(code);

    // 以 GB18030 编码将字节数组转换为字符串
    static constexpr char CODEC[] = "GB18030";
    QTextCodec *codec = QTextCodec::codecForName(CODEC);
    if (!codec) {
        _err_msg = tr("系统未支持 '%1' 编码！").arg(CODEC);    // "The system does not support '%1' encoding!";
        logWarning(QString("%1::%2(): %1").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(_err_msg));
        return false;
    }

    QString code_str = codec->toUnicode(code_bytes);

    // 解析逗号分隔的字符串
    static constexpr int COUNT_FIELD_MIN = 4;       // 字段个数的最小值

    QStringList value_strs = code_str.split(',');
    if (value_strs.size() >= COUNT_FIELD_MIN) {
        //
        QString name = value_strs.at(0);
        QString sex_str = (value_strs.at(1) == "男" ? "M" : (value_strs.at(1) == "女" ? "F" : ""));
        int age = value_strs.at(2).toInt();
        QString number = value_strs.at(3);

        // 必有字段的检查
        if (number.isEmpty()) {
            _err_msg = tr("格式错误") + tr("：") + tr("身份证号不可为空！");  // "FormatError", ": ", "The ID number cannot be empty!"
            return false;
        }

        //
        _pat.patientid      = number;
        _pat.patientname    = name;
        _pat.patientsex     = sex_str;
        _pat.setAgeRange(CAgeRange::fromAge(age));
    } else {
        _err_msg = tr("格式错误") + tr("：") + tr("字段个数须大于等于%1！").arg(COUNT_FIELD_MIN);
        // "FormatError", ": ", "The number of fields must be greater than or equal to %1!"
        return false;
    }

    //
    return true;
}
