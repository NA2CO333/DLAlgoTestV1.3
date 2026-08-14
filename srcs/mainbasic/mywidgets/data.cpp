#include "data.h"

#include <QDateTime>

#include "global.h"

// 出生日期的格式
const static QString BIRTH_DATE_FORMAT = QStringLiteral("yyyy-MM-dd");

// 日期时间的格式
const static QString DATE_TIME_FORMAT = QStringLiteral("yyyy-MM-dd HH:mm:ss");

//
const QString &CPatient::birthDateFormat()
{
    return BIRTH_DATE_FORMAT;
}

const QString &CPatient::dateTimeFormat()
{
    return DATE_TIME_FORMAT;
}

QString CPatient::getImgDirName() const
{
    /* v1.5.9 之前，该名称为 patientid （一般情况下，本次升级应该递增次版本号，但是由于网络更新的的路径与版本号挂钩，且版本选择机制未完善，不便修改次版本号，所以暂不递增次版本号） */
    // TODO: 应该把存图路径保存到数据库，否则版本兼容问题比较麻烦

    //
    QString num = this->patientid;
    QDateTime date_time;

    if (this->patienttesttime.length() > 0) {
        date_time = QDateTime::fromString(this->patienttesttime, DATE_TIME_FORMAT);
    }

    if (date_time.isValid()) {                          // TODO: 这里须确保测量日期合法！若测量日期为空或不合法，有没有更好的处理办法？
        num += QString("_%1").arg(date_time.toString("yyyyMMddHHmmss"));
    } else {
        logCritical(QString("%1: measure time = '%2'(%3), not valid!").arg(__PRETTY_FUNCTION__).arg(this->patienttesttime).arg(this->patientid));
        //return false;
        num += QString("_%1").arg(this->patienttesttime);
    }

    return num;
}

QString CPatient::getSexDisc() const
{
    return ("M" == this->patientsex ?
                QCoreApplication::translate("data.cpp", "男") :
                ("F" == this->patientsex ? QCoreApplication::translate("data.cpp", "女") : "")
                );    // "Male" "Female"
}

void CPatient::setSexFromDisc(QString _disc)
{
    if (_disc == "男" || _disc.compare("Male", Qt::CaseInsensitive) == 0)
        patientsex = "M";
    else if(_disc == "女" || _disc.compare("Female", Qt::CaseInsensitive) == 0)
        patientsex = "F";
    else
        patientsex = "";
}

QString CPatient::getSexDiscAbbr() const
{
    return ("M" == this->patientsex ?
                QCoreApplication::translate("data.cpp", "男", "abbr") :
                ("F" == this->patientsex ? QCoreApplication::translate("data.cpp", "女", "abbr") : "")
                );    // "M" "F"
}

QString CPatient::getBirthDateStr() const
{
    return patientdate;
}

void CPatient::setBirthDateStr(const QString &_birth_date_str)
{
    patientdate = "";
    if (_birth_date_str.length() > 0) {
        QDate date_birth = QDate::fromString(_birth_date_str, BIRTH_DATE_FORMAT);
        if (date_birth.isValid()) {
            patientdate = date_birth.toString(BIRTH_DATE_FORMAT);
        }
    }
}

QDate CPatient::getBirthDate() const
{
    QDate date_birth;
    if (patientdate.length() > 0) {
        date_birth = QDate::fromString(patientdate, BIRTH_DATE_FORMAT);
    }
    return date_birth;
}

void CPatient::setBirthDate(const QDate &_birth_date)
{
    patientdate = (_birth_date.isValid() ? _birth_date.toString(BIRTH_DATE_FORMAT) : "");
}

enAgeRange CPatient::getAgeRange() const
{
    enAgeRange ret = ageRange_Invalid;
    bool is_from_date = false;

    // 若出生日期和测量日期都存在且都合法，则即时计算年龄段
    QDate date_birth = QDate::fromString(patientdate, BIRTH_DATE_FORMAT);
    if (date_birth.isValid()) {
        QDateTime date_time_measure = QDateTime::fromString(patienttesttime, DATE_TIME_FORMAT);
        if (date_time_measure.isValid()) {
            QDate date_measure = date_time_measure.date();
            ret = CAgeRange::getAgeRangeFromBirthdate(date_birth, &date_measure);
            is_from_date = true;

            // 若算得的年龄段和当前值不符，？  // TODO:
            //if (ret != patientagerange) {
            //    patientagerange = ret;
            //
            //    // TODO:这个逻辑？
            //
            //}
        }
    }

    // 否则取“年龄段”的值
    if (!is_from_date) {
        ret = patientagerange;
    }

    //
    return ret;
}

void CPatient::setAgeRange(enAgeRange _age_range)
{
    // 若出生日期存在且合法，则不应设置
    //QDate date = getBirthDate();
    //if (date.isValid()) {
    //    return;
    //}
    // TODO: 这个的逻辑？

    // 若值超出枚举值的范围，则设为 Invalid
    if (!(_age_range >= ageRange_Min && _age_range <= ageRange_Max)) {
        _age_range = ageRange_Invalid;
    }

    //
    patientagerange = _age_range;

}

QString CPatient::getEyePositionDisc(int _hs, int _vs)
{
    QString posi_str = "";
    if (_hs != 0) {
        posi_str.append((_hs > 0 ? QCoreApplication::translate("data.cpp", "颞侧") : QCoreApplication::translate("data.cpp", "鼻侧")) + " " + QString::number(abs(_hs)) + "° ");   // "bitemporal "  "nasal "
    }
    if (_vs != 0) {
        posi_str.append((_vs > 0 ? QCoreApplication::translate("data.cpp", "偏上") : QCoreApplication::translate("data.cpp", "偏下")) + " " + QString::number(abs(_vs)) + "°"); // "up "  "down "
    }
    if (posi_str.length() == 0) {
        posi_str = "0°";
    }
    return posi_str;
}

QString CPatient::getEyePositionDiscR()
{
    return CPatient::getEyePositionDisc(patientrighths.toInt(), patientrightvs.toInt());
}

QString CPatient::getEyePositionDiscL()
{
    return CPatient::getEyePositionDisc(patientlefths.toInt(), patientleftvs.toInt());
}

void CPatient::cloneFrom(const CPatient &_dest_obj)
{
    this->id                    = _dest_obj.id;
    this->patientid             = _dest_obj.patientid;
    this->patientname           = _dest_obj.patientname;
    this->patientagerange       = _dest_obj.patientagerange;
    this->patientsex            = _dest_obj.patientsex;
    this->patientdate           = _dest_obj.patientdate;
    this->patientlefteyesph     = _dest_obj.patientlefteyesph;
    this->patientlefteyecyl     = _dest_obj.patientlefteyecyl;
    this->patientlefteyeax      = _dest_obj.patientlefteyeax;
    this->patientleftse         = _dest_obj.patientleftse;
    this->patientleftpd         = _dest_obj.patientleftpd;
    this->patientleftptosis     = _dest_obj.patientleftptosis;
    this->patientlefths         = _dest_obj.patientlefths;
    this->patientleftvs         = _dest_obj.patientleftvs;
    this->patientrighteyesph    = _dest_obj.patientrighteyesph;
    this->patientrighteyecyl    = _dest_obj.patientrighteyecyl;
    this->patientrighteyeax     = _dest_obj.patientrighteyeax;
    this->patientrightse        = _dest_obj.patientrightse;
    this->patientrightpd        = _dest_obj.patientrightpd;
    this->patientrightptosis    = _dest_obj.patientrightptosis;
    this->patientrighths        = _dest_obj.patientrighths;
    this->patientrightvs        = _dest_obj.patientrightvs;
    this->patientpd             = _dest_obj.patientpd;
    //this->grade                 = _dest_obj.grade;
    this->patientstuclass       = _dest_obj.patientstuclass;
    this->patienttesttime       = _dest_obj.patienttesttime;
    this->patientPhone          = _dest_obj.patientPhone;
    this->patientAddress        = _dest_obj.patientAddress;
    this->patientWechat         = _dest_obj.patientWechat;
    this->barcodeData           = _dest_obj.barcodeData;
    this->batchNo               = _dest_obj.batchNo;
    this->comment1              = _dest_obj.comment1;
    this->Comment2              = _dest_obj.Comment2;
    this->isTest                = _dest_obj.isTest;
    this->isBatch               = _dest_obj.isBatch;
    this->isNeedUpload          = _dest_obj.isNeedUpload;
    this->isUploaded            = _dest_obj.isUploaded;
    this->isNeedImage           = _dest_obj.isNeedImage;
    this->isUploadedImage       = _dest_obj.isUploadedImage;
    this->creattime             = _dest_obj.creattime;

    this->IS_MULTI              = _dest_obj.IS_MULTI        ;

    this->RESULT_1_R_SPH        = _dest_obj.RESULT_1_R_SPH  ;
    this->RESULT_1_R_CYL        = _dest_obj.RESULT_1_R_CYL  ;
    this->RESULT_1_R_AX         = _dest_obj.RESULT_1_R_AX   ;
    this->RESULT_1_L_SPH        = _dest_obj.RESULT_1_L_SPH  ;
    this->RESULT_1_L_CYL        = _dest_obj.RESULT_1_L_CYL  ;
    this->RESULT_1_L_AX         = _dest_obj.RESULT_1_L_AX   ;

    this->RESULT_2_R_SPH        = _dest_obj.RESULT_2_R_SPH  ;
    this->RESULT_2_R_CYL        = _dest_obj.RESULT_2_R_CYL  ;
    this->RESULT_2_R_AX         = _dest_obj.RESULT_2_R_AX   ;
    this->RESULT_2_L_SPH        = _dest_obj.RESULT_2_L_SPH  ;
    this->RESULT_2_L_CYL        = _dest_obj.RESULT_2_L_CYL  ;
    this->RESULT_2_L_AX         = _dest_obj.RESULT_2_L_AX   ;

    this->RESULT_3_R_SPH        = _dest_obj.RESULT_3_R_SPH  ;
    this->RESULT_3_R_CYL        = _dest_obj.RESULT_3_R_CYL  ;
    this->RESULT_3_R_AX         = _dest_obj.RESULT_3_R_AX   ;
    this->RESULT_3_L_SPH        = _dest_obj.RESULT_3_L_SPH  ;
    this->RESULT_3_L_CYL        = _dest_obj.RESULT_3_L_CYL  ;
    this->RESULT_3_L_AX         = _dest_obj.RESULT_3_L_AX   ;
}

void CPatient::reset(){
    this->id                    = 0;
    this->patientid             = "";
    this->patientname           = "";
    this->patientagerange       = ageRange_Invalid;
    this->patientsex            = "";
    this->patientdate           = "";
    this->patientlefteyesph     = "";
    this->patientlefteyecyl     = "";
    this->patientlefteyeax      = "";
    this->patientleftse         = "";
    this->patientleftpd         = "";
    this->patientleftptosis     = false;
    this->patientlefths         = "";
    this->patientleftvs         = "";
    this->patientrighteyesph    = "";
    this->patientrighteyecyl    = "";
    this->patientrighteyeax     = "";
    this->patientrightse        = "";
    this->patientrightpd        = "";
    this->patientrightptosis    = false;
    this->patientrighths        = "";
    this->patientrightvs        = "";
    this->patientpd             = "";
    //this->grade                 = "";
    this->patientstuclass       = "";
    this->patienttesttime       = "";
    this->patientPhone          = "";
    this->patientAddress        = "";
    this->patientWechat         = "";
    this->barcodeData           = "";
    this->batchNo               = "";
    this->comment1              = "";
    this->Comment2              = "";
    this->isTest                = false;
    this->isBatch               = false;
    this->isNeedUpload          = false;
    this->isUploaded            = false;
    this->isNeedImage           = false;
    this->isUploadedImage       = false;
    this->creattime             = "";

    this->IS_MULTI              = false;

    this->RESULT_1_R_SPH        = 0;
    this->RESULT_1_R_CYL        = 0;
    this->RESULT_1_R_AX         = 0;
    this->RESULT_1_L_SPH        = 0;
    this->RESULT_1_L_CYL        = 0;
    this->RESULT_1_L_AX         = 0;

    this->RESULT_2_R_SPH        = 0;
    this->RESULT_2_R_CYL        = 0;
    this->RESULT_2_R_AX         = 0;
    this->RESULT_2_L_SPH        = 0;
    this->RESULT_2_L_CYL        = 0;
    this->RESULT_2_L_AX         = 0;

    this->RESULT_3_R_SPH        = 0;
    this->RESULT_3_R_CYL        = 0;
    this->RESULT_3_R_AX         = 0;
    this->RESULT_3_L_SPH        = 0;
    this->RESULT_3_L_CYL        = 0;
    this->RESULT_3_L_AX         = 0;
}

bool CPatient::isBasicInfoSame(const CPatient &_other) const
{
    bool is_same = (true
                    && _other.patientid         == this->patientid
                    && _other.patientname       == this->patientname
                    && _other.patientsex        == this->patientsex

                    && _other.patientdate       == this->patientdate
                    //&& _other.patientagerange   == this->patientagerange  // NOTE: “年龄段”不属于基本信息，因为可能在测量过程中自动设置

                    && _other.patientstuclass   == this->patientstuclass
                    && _other.patientPhone      == this->patientPhone
                    && _other.patientWechat     == this->patientWechat
                    && _other.patientAddress    == this->patientAddress
                    );
    return is_same;
}

void CPatient::cloneBasicInfoFrom(const CPatient &_other)
{
    this->patientid         = _other.patientid          ;
    this->patientname       = _other.patientname        ;
    this->patientsex        = _other.patientsex         ;

    this->patientdate       = _other.patientdate        ;
    this->patientagerange   = _other.patientagerange    ;

    this->patientstuclass   = _other.patientstuclass    ;
    this->patientWechat     = _other.patientWechat      ;
    this->patientPhone      = _other.patientPhone       ;
    this->patientAddress    = _other.patientAddress     ;
}
