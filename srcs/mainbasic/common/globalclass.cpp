#include "globalclass.h"

#include <math.h>

#include <QDebug>
#include <QDate>
#include <QCoreApplication>

#include "util-common.h"
#include "global.h"

/// ================================================================================================
/// class CAgeRange

// TODO: 其它涉及计算年龄段的代码：Edit::checkAgeAndDate(), PersonalInfos::checkAgeAndDate()，重写。本函数源自 Edit::getAgeRange()，待优化
enAgeRange CAgeRange::getAgeRangeFromBirthdate(QDate _birth_date, QDate * const _measure_date)
{
    enAgeRange age_range = ageRange_Invalid;

    if (_birth_date.isValid()) {
        int year_birth = _birth_date.year();        // 生日的 年
        int month_birth = _birth_date.month();      // 生日的 月
        int day_birth = _birth_date.day();          // 生日的 日

        QDate measure_date = (_measure_date ? *_measure_date : QDate::currentDate());
        if (!measure_date.isValid()) {
            qWarning() << "measure date not valid!";
            return ageRange_Invalid;
        }

        int year_curr = measure_date.year();            // 测量时的 年
        int month_curr = measure_date.month();          // 测量时的 月
        int day_curr = measure_date.day();              // 测量时的 日

        int year_diff = 0, month_diff = 0, day_diff = 0;        // 相差年月日
        if (year_curr >= year_birth)        // 如果当前日期的年份 >= 出生日期的年份
        {
            if (day_curr >= day_birth) {
                day_diff = day_curr - day_birth;
            } else {    // 否则当前日期的年份 < 出生日期的年份
                day_diff = (day_curr + 30) - day_birth;
                month_curr--;
            }

            if (month_curr >= month_birth) {
                month_diff = month_curr - month_birth;
            } else {
                month_diff = (month_curr + 12) - month_birth;
                year_curr--;
            }

            year_diff = year_curr - year_birth;
        }
        qDebug() << QString::asprintf("CAgeRange::getAgeRangeFromBirthdateStr(): year_diff = %d, month_diff = %d, day_diff = %d", year_diff, month_diff, day_diff);

        if (year_diff < 0){
            qWarning() << "get ageRange error!";
            return ageRange_Invalid;
        }
        qDebug() << "get age2:" << year_diff;

        if (year_diff == 0 /*&& month_diff >= 6*/ && month_diff < 12) {
            age_range = ageRange_0_06_12_MONTH;
        } else if (year_diff >= 1 && year_diff < 3) {
            age_range = ageRange_1_01_03_YEAR;
        } else if (year_diff >= 3 && year_diff < 6) {
            age_range = ageRange_2_03_06_YEAR;
        } else if (year_diff >= 6 && year_diff < 20) {
            age_range = ageRange_3_06_20_YEAR;
        } else if (year_diff >= 20 /*&& year_diff <= 100*/) {
            age_range = ageRange_4_20_100_YEAE;
        } else {
            age_range = ageRange_Invalid;
        }
    }

    return age_range;
}

enAgeRange CAgeRange::getAgeRangeFromBirthdateStr(QString _birthdate_str, QDate *_measure_date)
{
    QDate birth_date = Util::strToDate(_birthdate_str);
    if (birth_date.isValid()) {
        return getAgeRangeFromBirthdate(birth_date, _measure_date);
    } else {
        return ageRange_Invalid;
    }
}

enAgeRange CAgeRange::fromAge(const int _age)
{
    if (_age < 0){
        return ageRange_Invalid;
    } else if (_age == 0) {
        return ageRange_0_06_12_MONTH;
    } else if (_age >= 1 && _age < 3) {
        return ageRange_1_01_03_YEAR;
    } else if (_age >= 3 && _age < 6) {
        return ageRange_2_03_06_YEAR;
    } else if (_age >= 6 && _age < 20) {
        return ageRange_3_06_20_YEAR;
    } else if (_age >= 20) {
        return ageRange_4_20_100_YEAE;
    } else {
        return ageRange_Invalid;
    }
}

QDate CAgeRange::getBirthDateByAgeRange(enAgeRange _age_range, double _posi, QDate *_measure_date)
{
    QDate birth_date;

    // 获取测量日期
    QDate measure_date = (_measure_date ? *_measure_date : QDate::currentDate());
    if (!measure_date.isValid()) {
        return birth_date;
    }

    // 计算出生日期到测量日期的天数
    int months_min = 0, months_max = 0;
    if (ageRange_0_06_12_MONTH == _age_range) {
        months_min = 6;
        months_max = 12;
    } else if (ageRange_1_01_03_YEAR == _age_range) {
        months_min = 1 * 12;
        months_max = 3 * 12;
    } else if (ageRange_2_03_06_YEAR == _age_range) {
        months_min = 3 * 12;
        months_max = 6 * 12;
    } else if (ageRange_3_06_20_YEAR == _age_range) {
        months_min = 6 * 12;
        months_max = 20 * 12;
    } else if (ageRange_4_20_100_YEAE == _age_range) {
        months_min = 20 * 12;
        months_max = 100 * 12;
    } else {
        return birth_date;
    }

    // 生日的最大、最小值
    QDate birth_date_min = measure_date.addMonths(-months_max);
    QDate birth_date_max = measure_date.addMonths(-months_min);

    // 最大最小生日的间隔天数
    qint64 days_diff = birth_date_min.daysTo(birth_date_max);

    // 出生日期
    birth_date = birth_date_max.addDays(-std::ceil(days_diff * _posi));     // （从后往前减）

    //
    return birth_date;
}

// 由年龄段索引得到年龄段描述
QString CAgeRange::getAgeRangeDesc(enAgeRange _age_range_idx)
{
    switch (_age_range_idx) {
    case ageRange_Invalid:          return "";
    case ageRange_0_06_12_MONTH:    return QCoreApplication::translate("globalclass.cpp", "06-12个月");   // "06-12(M)"
    case ageRange_1_01_03_YEAR:     return QCoreApplication::translate("globalclass.cpp", "12-36个月");   // "12-36(M)"
    case ageRange_2_03_06_YEAR:     return QCoreApplication::translate("globalclass.cpp", "03-06岁");     // "03-06(Y)"
    case ageRange_3_06_20_YEAR:     return QCoreApplication::translate("globalclass.cpp", "06-20岁");     // "06-20(Y)"
    case ageRange_4_20_100_YEAE:    return QCoreApplication::translate("globalclass.cpp", "20-100岁");    // "20-100(Y)"
    default:
        return "???";
    }
}

void CAgeRange::getAgeRangeDescList(QStringList &_list)
{
    _list.clear();
    _list.append(getAgeRangeDesc(ageRange_0_06_12_MONTH));
    _list.append(getAgeRangeDesc(ageRange_1_01_03_YEAR));
    _list.append(getAgeRangeDesc(ageRange_2_03_06_YEAR));
    _list.append(getAgeRangeDesc(ageRange_3_06_20_YEAR));
    _list.append(getAgeRangeDesc(ageRange_4_20_100_YEAE));
}

bool CAgeRange::isAgeRangeValid(enAgeRange _age_range)
{
    return (_age_range >= ageRange_Min && _age_range <= ageRange_Max);
}

/// ====================================================================================================
/// class CScreenTimeout
///

CScreenTimeout::CScreenTimeout() : CEnum<enScreenTimeout>()
{
    init();
}

CScreenTimeout::CScreenTimeout(enScreenTimeout _v) : CEnum<enScreenTimeout>(_v)
{
    init();
}

void CScreenTimeout::init()
{
    values().clear();
    values() << screenTimeout_30Sec << screenTimeout_1Min << screenTimeout_3Min << screenTimeout_No;
}

void CScreenTimeout::discrips(QList<QString> &_list) const
{
    _list.clear();
    _list.append(getDiscrip(screenTimeout_30Sec));
    _list.append(getDiscrip(screenTimeout_1Min));
    _list.append(getDiscrip(screenTimeout_3Min));
    _list.append(getDiscrip(screenTimeout_No));
}

QString CScreenTimeout::getDiscrip() const
{
    return CScreenTimeout::getDiscrip(value);
}

QString CScreenTimeout::getDiscrip(enScreenTimeout _v)
{
    switch (_v) {
    case screenTimeout_30Sec:   return QCoreApplication::translate("globalclass.cpp", "30秒");          // "30s"
    case screenTimeout_1Min:    return QCoreApplication::translate("globalclass.cpp", "1分钟(默认)");   // "1min(default)"
    case screenTimeout_3Min:    return QCoreApplication::translate("globalclass.cpp", "3分钟");         // "3min"
    case screenTimeout_No:      return QCoreApplication::translate("globalclass.cpp", "关闭");          // "never"
    }
    return "???";
}

/// ====================================================================================================
/// class CShutdownNoOperation
///

CShutdownNoOperation::CShutdownNoOperation() : CEnum<enShutdownNoOperation>()
{
    init();
}

CShutdownNoOperation::CShutdownNoOperation(enShutdownNoOperation _v) : CEnum<enShutdownNoOperation>(_v)
{
    init();
}

void CShutdownNoOperation::init()
{
    values() << shutdownNoOperation_No << shutdownNoOperation_15Min << shutdownNoOperation_30Min << shutdownNoOperation_1Hour << shutdownNoOperation_2Hour ;
}

void CShutdownNoOperation::discrips(QList<QString> &_list) const
{
    _list.clear();
    _list.append(getDiscrip(shutdownNoOperation_No));
    _list.append(getDiscrip(shutdownNoOperation_15Min));
    _list.append(getDiscrip(shutdownNoOperation_30Min));
    _list.append(getDiscrip(shutdownNoOperation_1Hour));
    _list.append(getDiscrip(shutdownNoOperation_2Hour));
}

QString CShutdownNoOperation::getDiscrip() const
{
    return CShutdownNoOperation::getDiscrip(value);
}

QString CShutdownNoOperation::getDiscrip(enShutdownNoOperation _v)
{
    switch (_v) {
    case shutdownNoOperation_No:    return QCoreApplication::translate("globalclass.cpp", "关闭");    // "never"
    case shutdownNoOperation_15Min: return QCoreApplication::translate("globalclass.cpp", "15分钟");  // "15 min"
    case shutdownNoOperation_30Min: return QCoreApplication::translate("globalclass.cpp", "30分钟");  // "30 min"
    case shutdownNoOperation_1Hour: return QCoreApplication::translate("globalclass.cpp", "1小时");   // "1 hour"
    case shutdownNoOperation_2Hour: return QCoreApplication::translate("globalclass.cpp", "2小时");   // "2 hour"
    }
    return "???";
}

/// ====================================================================================================
/// class CSingleDualEyeMode
///

QString enumToText_SingleDualEyeMode(enSingleDualEyeMode _mode)
{
    switch (_mode) {
    case singleDualEyeMode_Right    : return QCoreApplication::translate("globalclass.cpp", "右眼");  // "Right Eye"
    case singleDualEyeMode_Left     : return QCoreApplication::translate("globalclass.cpp", "左眼");  // "Left Eye"
    case singleDualEyeMode_Both     : return QCoreApplication::translate("globalclass.cpp", "双眼");  // "Dual Eye"
    }
    return "???";
}

/// ====================================================================================================
/// class COpticalPathType
///

COpticalPathType::COpticalPathType() : CEnum<enOpticalPathType>()
{
    init();
}

COpticalPathType::COpticalPathType(enOpticalPathType _v) : CEnum<enOpticalPathType>(_v)
{
    init();
}

void COpticalPathType::init()
{
    values() << opticalPathType_General << opticalPathType_Square << opticalPathType_LShape ;
}

void COpticalPathType::discrips(QList<QString> &_list) const
{
    _list.clear();
    _list.append(getDiscrip(opticalPathType_General));
    _list.append(getDiscrip(opticalPathType_Square));
    _list.append(getDiscrip(opticalPathType_LShape));
}

QString COpticalPathType::getDiscrip() const
{
    return COpticalPathType::getDiscrip(value);
}

QString COpticalPathType::getDiscrip(enOpticalPathType _v)
{
    switch (_v) {
    case opticalPathType_General:   return QCoreApplication::translate("globalclass.cpp", "常规");        // "General"
    case opticalPathType_Square:    return QCoreApplication::translate("globalclass.cpp", "方形视筛箱");  // "Square Box"
    case opticalPathType_LShape:    return QCoreApplication::translate("globalclass.cpp", "L形视筛箱");   // "L-Shaped Box"
    }
    return "???";
}

/// ====================================================================================================
/// class CVisionNotation
///

CVisionNotation::CVisionNotation() : CEnum<enVisionNotation>()
{
    init();
}

CVisionNotation::CVisionNotation(enVisionNotation _v) : CEnum<enVisionNotation>(_v)
{
    init();
}

void CVisionNotation::init()
{
    values() << visionNotation_None << visionNotation_FivePoint << visionNotation_Decimal ;
}

void CVisionNotation::discrips(QList<QString> &_list) const
{
    _list.clear();
    _list.append(getDiscrip(visionNotation_None));
    _list.append(getDiscrip(visionNotation_FivePoint));
    _list.append(getDiscrip(visionNotation_Decimal));
}

QString CVisionNotation::getDiscrip() const
{
    return CVisionNotation::getDiscrip(value);
}

QString CVisionNotation::getDiscrip(enVisionNotation _v)
{
    switch (_v) {
    case visionNotation_None:       return QCoreApplication::translate("globalclass.cpp", "无");         // "None"
    case visionNotation_FivePoint:  return QCoreApplication::translate("globalclass.cpp", "五分制");     // "FivePoint"
    case visionNotation_Decimal:    return QCoreApplication::translate("globalclass.cpp", "小数制");     // "Decimal"
    }
    return "???";
}

/// ====================================================================================================
/// class CScreenBrightness
///

CScreenBrightness::CScreenBrightness() : CEnum<enScreenBrightness>()
{
    init();
}

CScreenBrightness::CScreenBrightness(enScreenBrightness _v) : CEnum<enScreenBrightness>(_v)
{
    init();
}

const CScreenBrightness &CScreenBrightness::reset()
{
    value = values()[values().size() - 2];      // 屏幕亮度（百分数的分子），选倒数第二个为缺省值
    return (*this);
}

void CScreenBrightness::init()
{
    values() << screenBrightness_20 << screenBrightness_40 << screenBrightness_60 << screenBrightness_80 ;
}

void CScreenBrightness::discrips(QList<QString> &_list) const
{
    _list.clear();
    _list.append(getDiscrip(screenBrightness_20));
    _list.append(getDiscrip(screenBrightness_40));
    _list.append(getDiscrip(screenBrightness_60));
    _list.append(getDiscrip(screenBrightness_80));
}

QString CScreenBrightness::getDiscrip() const
{
    return CScreenBrightness::getDiscrip(value);
}

QString CScreenBrightness::getDiscrip(enScreenBrightness _v)
{
    switch (_v) {
    case screenBrightness_20:
        return "20%";
    case screenBrightness_40:
        return "40%";
    case screenBrightness_60:
        return "60%";
    case screenBrightness_80:
        return "80%";
    default:
        return "??";
    }
}

/// ====================================================================================================
/// class CSex
///

CSex::CSex() : CEnum<enSex>()
{
    init();
}

CSex::CSex(enSex _v) : CEnum<enSex>(_v)
{
    init();
}

const CSex &CSex::reset()
{
    value = values()[0];
    return (*this);
}

void CSex::init()
{
    values() << sex_Male << sex_Fefale ;
}

void CSex::discrips(QList<QString> &_list) const
{
    _list.clear();
    _list.append(getDiscrip(sex_Male));
    _list.append(getDiscrip(sex_Fefale));
}

QString CSex::getDiscrip() const
{
    return CSex::getDiscrip(value);
}

QString CSex::getDiscrip(enSex _v)
{
    switch (_v) {
    case sex_Male:      return QCoreApplication::translate("globalclass.cpp", "男");   // "Male"
    case sex_Fefale:    return QCoreApplication::translate("globalclass.cpp", "女");   // "Female"
    }
    return "???";
}

///=============================================================================================================
/// class stVerInfo
///

int stVerInfo::compareWith(const stVerInfo &_ver_info) const
{
    int compare_base =
            Util::compValue<int>(this->verMajor, _ver_info.verMajor) * 100 +
            Util::compValue<int>(this->verMinor, _ver_info.verMinor) * 10 +
            Util::compValue<int>(this->verPatch, _ver_info.verPatch) * 1;
    return compare_base;
}

int stVerInfoApp::compareWith(const stVerInfoApp &_ver_info, bool _compare_build) const
{
    int compare_base = stVerInfo::compareWith(_ver_info);
    int compare_date = Util::compValue<QDate>(this->verDate, _ver_info.verDate);
    int compare_build = (_compare_build ? Util::compValue<int>(this->verBuild, _ver_info.verBuild) : 0);

    /* 基本版本号的权重，应大于版本日期。比如从旧版分支出来的版本，虽然版本日期较新，但版本应认为是旧版。 */
    int compare_total = Util::compValue<int>(compare_base * 100 + compare_date * 10 + compare_build * 1, 0);
    return compare_total;
}

///=============================================================================================================
///
///

QString getProductModelStr(enProductModel _model)
{
    QString model_str;
    if (productModel_SL100S == _model) {
        model_str = "SL-100S";
    } else if (productModel_SL100 == _model) {
        model_str = "SL-100";
    } else if (productModel_SL100P == _model) {
        model_str = "SL-100P";
    }
    return model_str;
}

QString enumToText_AutoScreenOff(enAutoScreenOff _option)
{
    switch (_option) {
    case enAutoScreenOff::Unknown       : return QCoreApplication::translate("globalclass.cpp", "未知");      // "Unknown"
    case enAutoScreenOff::Never         : return QCoreApplication::translate("globalclass.cpp", "永不");      // "Never"
    case enAutoScreenOff::Duration_1    : return QCoreApplication::translate("globalclass.cpp", "10分钟");    // "10 min"
    case enAutoScreenOff::Duration_2    : return QCoreApplication::translate("globalclass.cpp", "20分钟");    // "20 min"
    case enAutoScreenOff::Duration_3    : return QCoreApplication::translate("globalclass.cpp", "30分钟");    // "30 min"
    }
    return "???";
}

int enumToInt_AutoScreenOff(enAutoScreenOff _option)
{
    switch (_option) {
    case enAutoScreenOff::Unknown       : return -1;
    case enAutoScreenOff::Never         : return 0;
    case enAutoScreenOff::Duration_1    : return 10 * 60;      // "10 min"
    case enAutoScreenOff::Duration_2    : return 20 * 60;      // "20 min"
    case enAutoScreenOff::Duration_3    : return 30 * 60;      // "30 min"
    }
    return -1;
}

const char *enumToName_QrCodeSystem(enQrCodeSystem _system)
{
    switch (_system) {
    case enQrCodeSystem::Unknown            : return "Unknown";
    case enQrCodeSystem::Manylinks          : return "Manylinks";
    case enQrCodeSystem::Huayi              : return "Huayi";
    case enQrCodeSystem::AnHuiScreen        : return "AnHuiScreen";
    case enQrCodeSystem::ShanDongQinCheng   : return "ShanDongQinCheng";
    }
    return "???";
}

QString enumToText_DistSensorType(enDistSensorType _type, bool _is_engineer)
{
    if (G_LANGUAGE_CHINESE == CGlobal::language) {
        if (_is_engineer) {
            switch (_type) {
            case enDistSensorType::Unknown      : return "未知"                     ;
            case enDistSensorType::Mb1010       : return "超声 MB1010 接底板"        ;
            //case enDistSensorType::Xkc_DYP_A06  : return "超声 XKC DYP-A06 接核心板" ;
            //case enDistSensorType::Xkc_KL200    : return "红外 XKC-KL200 接核心板"   ;
            //case enDistSensorType::TFLC02       : return "红外 TF-LC02 接核心板"     ;
            //case enDistSensorType::TFLuna       : return "红外 TF-Luna 接核心板"     ;
            //case enDistSensorType::TFminiS      : return "红外 TFmini-S 接核心板"    ;
            case enDistSensorType::SIMAN_SDM10  : return "激光 SDM10 接核心板"       ;
            default:
                return "???";
            }
        } else {
            switch (_type) {
            case enDistSensorType::Unknown      : return "未知"              ;
            case enDistSensorType::Mb1010       : return "超声_v1"           ;
            //case enDistSensorType::Xkc_DYP_A06  : return "超声_v2"           ;
            //case enDistSensorType::Xkc_KL200    : return "红外_v1"           ;
            //case enDistSensorType::TFLC02       : return "红外_v2.1"         ;
            //case enDistSensorType::TFLuna       : return "红外_v2.2"         ;
            //case enDistSensorType::TFminiS      : return "红外_v2.3"         ;
            case enDistSensorType::SIMAN_SDM10  : return "Siman-SDM10"     ;
            default:
                return "???";
            }
        }
    } else {
        switch (_type) {
        case enDistSensorType::Unknown      : return "Unknown"          ;
        case enDistSensorType::Mb1010       : return "sonar_v1"         ;
        //case enDistSensorType::Xkc_DYP_A06  : return "sonar_v2"         ;
        //case enDistSensorType::Xkc_KL200    : return "infrared_v1"      ;
        //case enDistSensorType::TFLC02       : return "infrared_v2.1"    ;
        //case enDistSensorType::TFLuna       : return "infrared_v2.2"    ;
        //case enDistSensorType::TFminiS      : return "infrared_v2.3"    ;
        case enDistSensorType::SIMAN_SDM10  : return "Siman-SDM10"      ;
        }
    }
}
