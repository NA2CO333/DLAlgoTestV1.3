#ifndef CDATA_H
#define CDATA_H

#include <QString>
#include <QDate>

#include "globaltypes.h"

/* 2023-10-20 需求变更:
 * 需求提出：崔继友
 * 需求：支持门诊需求，一个被测者可以有多条测量记录共存。
 *
 * 当前系统的不足：
 * CPatient::patientid 字段（被测者编号），和“诊疗号”同义，但是该字段的值在批量筛查页面中显示为“编号”，而此页面的编号，应为“筛查号”。
 * 而且现有对接的客户中，至少有部分客户将此字段作为“筛查号”来用，比如万灵云端，但是也可能有部分客户不是。
 *
 * 所以，数据结构做如下变更：
 * 1、仪器中，以及数据传输过程中，“诊疗号”和“筛查号”共用一个字段。
 * 仪器：根据不同的模块入口决定此字段的意义。
 * 云端：根据不同的服务接口决定此字段的意义。
 * 再增加 bool MeasureResult::IsClinic 字段，表示“是否门诊记录”，若否，则为筛查记录。
 *
 * 此外：
 * 旧代码中，将被测者编号当作是不重复的，一些查询都以此为键值，这是不合理的，须改为以测量结果表的 id 为主键。
 *
 */

//
struct CPatient {
    int  id = 0;                        // 数据库id主键
    QString patientid;                  // 被测者编号 或 筛查号（若 isBatch，则为筛查号，否则为诊疗号）
    QString patientname;                // 姓名
    QString patientsex;                 // 性别（'M' 或 'F' 或 空字符串）
    QString patientlefteyesph;          // 左眼球镜(屈光)
    QString patientlefteyecyl;          // 左眼柱镜(散光)
    QString patientlefteyeax;           // 左眼轴位角
    QString patientleftse;              // 左眼等效球镜度
    QString patientleftpd;              // 左眼瞳孔直径
    bool    patientleftptosis = false;  // 左眼上睑下垂
    QString patientlefths;              // 左眼斜视 horizen
    QString patientleftvs;              // 左眼斜视 vertical
    QString patientrighteyesph;         // 右眼球镜(屈光)
    QString patientrighteyecyl;         // 右眼柱镜(散光)
    QString patientrighteyeax;          // 右眼轴位角
    QString patientrightse;             // 右眼等效球镜度
    QString patientrightpd;             // 右眼瞳孔直径
    bool    patientrightptosis = false; // 右眼上睑下垂
    QString patientrighths;             // 右眼斜视 horizen
    QString patientrightvs;             // 右眼斜视 vertical
    QString patientpd;                  // 瞳距
    //QString grade;                      // 年级
    QString patientstuclass;            // 班级       -> （2023-10-26）若是门诊档案，则为“籍贯”
    QString patienttesttime;            // 测试时间         /* 注意：因为存图等文件名和测量记录的匹配需要用到这个时间，所以这个时间在一启动测量时就要设置，而且在之后的业务过程中不可修改。 */
    QString patientPhone;               // 电话
    QString patientAddress;             // 地址
    QString patientWechat;              // 微信       -> （2023-10-31）若是门诊档案，则为“民族”
    QString barcodeData;                // 扫码数据
    QString batchNo;                    // 批次编号
    QString comment1;                   // 外部编号
    QString Comment2;                   // 是否标准数据
    bool    isTest = false;             // 是否已测，0 未测，1 已测
    bool    isBatch = false;            // 是否批量数据，0 门诊，1 筛查     // TODO: 用枚举？ clinic-门诊， batch-筛查   // TODO: “门诊”的英文术语不应该用 Clinic（诊所/门诊部）而应该用 Outpatient（门诊病人/门诊服务）？
    bool    isNeedUpload = false;       // 是否需要上传       // TODO: 这个字段好像没有意义？清掉？
    bool    isUploaded = false;         // 是否已上传
    bool    isNeedImage = false;        // 是否需要上传图像
    bool    isUploadedImage = false;    // 是否已上传图像
    QString creattime;                  // 创建时间

    bool    IS_MULTI        = false ;   // 是否多次测量

    double  RESULT_1_R_SPH  = 0     ;   // 第一组结果，右眼，球镜度
    double  RESULT_1_R_CYL  = 0     ;   // 第一组结果，右眼，柱镜度
    double  RESULT_1_R_AX   = 0     ;   // 第一组结果，右眼，轴位
    double  RESULT_1_L_SPH  = 0     ;   // 第一组结果，左眼，球镜度
    double  RESULT_1_L_CYL  = 0     ;   // 第一组结果，左眼，柱镜度
    double  RESULT_1_L_AX   = 0     ;   // 第一组结果，左眼，轴位

    double  RESULT_2_R_SPH  = 0     ;   // 第二组结果，右眼，球镜度
    double  RESULT_2_R_CYL  = 0     ;   // 第二组结果，右眼，柱镜度
    double  RESULT_2_R_AX   = 0     ;   // 第二组结果，右眼，轴位
    double  RESULT_2_L_SPH  = 0     ;   // 第二组结果，左眼，球镜度
    double  RESULT_2_L_CYL  = 0     ;   // 第二组结果，左眼，柱镜度
    double  RESULT_2_L_AX   = 0     ;   // 第二组结果，左眼，轴位

    double  RESULT_3_R_SPH  = 0     ;   // 第三组结果，右眼，球镜度
    double  RESULT_3_R_CYL  = 0     ;   // 第三组结果，右眼，柱镜度
    double  RESULT_3_R_AX   = 0     ;   // 第三组结果，右眼，轴位
    double  RESULT_3_L_SPH  = 0     ;   // 第三组结果，左眼，球镜度
    double  RESULT_3_L_CYL  = 0     ;   // 第三组结果，左眼，柱镜度
    double  RESULT_3_L_AX   = 0     ;   // 第三组结果，左眼，轴位

    /* “偏差”字段的格式：
     * 1、字符串，第一层为分号分隔的三组数据，每组数据（第二层）由逗号分隔。
     * 2、第二层各字段分别是：
     *    右眼 sph 的偏差
     *    右眼 cyl 的偏差
     *    右眼 ax  的偏差
     *    左眼 sph 的偏差
     *    左眼 cyl 的偏差
     *    左眼 ax  的偏差
     */
    struct stDeviations {

    };

    // 解析 deviations 字符串
    void parseDeviationsStr(const QString &_str, stDeviations &_devistions);

    /* 函数成员 */
    void cloneFrom(const CPatient &_dest_obj);      // 对象拷贝     // TODO: 这个类的成员的数据类型，没有必要手写拷贝函数？
    void reset();                                   // 重置对象（对象的初始值由本函数决定，所以创建对象后，一般应当调用此函数）

    bool isBasicInfoSame(const CPatient &_other) const;         // 基本信息是否一致
    void cloneBasicInfoFrom(const CPatient &_other);            // 拷贝基本信息

    static const QString &birthDateFormat();        // 出生日期的格式（遵循 QDate::toString() 的语法）
    static const QString &dateTimeFormat();         // 日期时间的格式

    // 获取本测量记录的存图目录名（具有不重复性，存图的目录名以词命名，预览图像文件、报表文件的文件名以此词区分不同的测量记录）
    QString getImgDirName() const;
    // TODO: 应该把存图路径保存到数据库，否则版本兼容问题比较麻烦？

    QString getSexDisc() const;             // 获取性别的描述文字（“男”或“女”，"Male"或"Female"）
    QString getSexDiscAbbr() const;         // 获取性别的描述文字缩写（“男”或“女”，"M"或"F"）
    void setSexFromDisc(QString _disc);

    // begin: 【出生日期】和【年龄段】  ========================================================================

    /* 出生日期和年龄段的业务逻辑规则：
     * 1、由 生日 + 测量时间 -> 得出 年龄段，所以若是有生日且合法，则生日优先，而年龄段则由生日 + 测量时间实时计算。若无有效的生日，才取“年龄段”字段的值。
     * 2、在被测者档案中，严格来说，年龄段是没有意义的，因为只有确定了测量时间，年龄段才能由生日推得。所以，生日属于患者档案，而年龄段属于测量档案。
     * 3、从系统外部获取生日和年龄段时，应优先设置生日？（没必要？）
     *
     */

    QString getBirthDateStr() const;                                // 获取【出生日期】字符串
    void setBirthDateStr(const QString &_birth_date_str);

    QDate getBirthDate() const;                                     // 获取【出生日期】
    void setBirthDate(const QDate &_birth_date);

    enAgeRange getAgeRange() const;                                 // 获取【年龄段】字段
    void setAgeRange(enAgeRange _age_range);

    /* TODO: (2026-07-13)年龄段和生日两个字段保持一致的严谨方案：
     * 1、去掉数据库的 patientagerange 字段，年龄段由生日计算。
     * 2、另加一字段字段 "IS_BIRTHDATE_EXACT" 表明该生日是否准确的。
     * 3、当不知道生日只知道年龄段时，用该年龄段对应的日期区间内的中值作为生日，且将 "IS_BIRTHDATE_EXACT" 设为 false。
     */
    // NOTE: 此方案的实施，需要移除和新增字段，所以需要先实现自动数据库迁移。

    // end: 【出生日期】和【年龄段】  ==========================================================================

    QDateTime getTestTime() { return QDateTime::fromString(patienttesttime, CPatient::dateTimeFormat()); }

    static QString getEyePositionDisc(int _hs, int _vs);    // 获取眼位描述

    QString getEyePositionDiscR();      // 获取眼位描述：右眼
    QString getEyePositionDiscL();      // 获取眼位描述：左眼

protected:
    // begin: 【出生日期】和【年龄段】  ========================================================================
    enAgeRange patientagerange = ageRange_Invalid;          // 年龄段
    QString patientdate;                                    // 出生日期
    // end: 【出生日期】和【年龄段】  ==========================================================================

};

// 业务数据类的基类
class CBusiData
{
public:
    // 重置（还原）
    virtual void reset() = 0;

    // 比较
    //virtual bool isEqualTo(void) const = 0;

};

#endif // CDATA_H
