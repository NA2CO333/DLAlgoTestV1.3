#ifndef GLOBALCLASS_H
#define GLOBALCLASS_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QDate>

#include "globaltypes.h"

// 模板：枚举类型逻辑封装类的基类
template <typename T>
class CEnum {
public:
    CEnum()                                             // 构造函数
    {
        value = (T)0;
    }
    CEnum(T _v)                                         // 构造函数
    {
        value = _v;
    }

    bool &isInited() const
    {
        static bool s_isInited = false;
        return s_isInited;
    }

    QList<T> &values() const                            // 值列表
    {
        static QList<T> s_listValue;
        return s_listValue;
    }

    int currentIndex() const                            // 获得当前值在值列表中的索引号
    {
        return values().indexOf(value);
    }
    bool setCurrentIndex(int _idx)                      // 设置当前值在值列表中的索引号
    {
        if (_idx >= 0 && _idx < values().size()) {
            value = values()[_idx];
        } else {
            return false;
        }
        return true;
    }

    virtual QString getDiscrip() const = 0;                     // 指定项的描述字符串
    virtual void discrips(QList<QString> &_list) const = 0;     // 枚举值描述列表

    T getValue() const                                  // 获取对象的枚举值
    {
        return value;
    }
    bool setValue(const T &_v)                          // 设置对象的枚举值
    {
        if (values().contains(_v)) {
            value = _v;
            return true;
        } else {
            return false;
        }
    }
    bool setValue(const CEnum<T> &_v)                   // 从另一个对象设置本对象的值（值拷贝）
    {
        return setValue(_v.getValue());
    }

    int toInt() const                                   // 转为 int
    {
        int v = (int)getValue();
        return v;
    }
    virtual const CEnum<T> &reset()                     // 重置为默认值
    {
        if (values().size() > 0) {
            value = values()[0];
        }
        return (*this);
    }

    bool operator==(const T &_v) const                  // 相等判断
    {
        return (_v == value);
    }
    bool operator==(const CEnum<T> &_v) const
    {
        return (_v.value == value);
    }
    bool operator!=(const T &_v) const
    {
        return !(*this == _v);
    }
    bool operator!=(const CEnum<T> &_v) const
    {
        return !(*this == _v);
    }

protected:
    T value = (T)0;

};

// “年龄段”枚举值逻辑封装类
class CAgeRange /*: public CEnum<enAgeRange>*/
{
public:
    CAgeRange() {}

    // 根据生日获得年龄段
    static enAgeRange getAgeRangeFromBirthdate(QDate _birth_date, QDate * const _measure_date = Q_NULLPTR);
    // 根据生日字符串获得年龄段
    static enAgeRange getAgeRangeFromBirthdateStr(QString _birthdate_str, QDate *_measure_date = Q_NULLPTR);

    // 由年龄得到年龄段
    static enAgeRange fromAge(const int _age);

    // 由年龄段得到可用的生日（_posi 为可用生日区间的位置比例）
    static QDate getBirthDateByAgeRange(enAgeRange _age_range, double _posi = 0.01, QDate *_measure_date = Q_NULLPTR);

    // 获取年龄段的描述字符串
    static QString getAgeRangeDesc(enAgeRange _age_range_idx);
    // 获取所有年龄段的描述字符串列表
    static void getAgeRangeDescList(QStringList &_list);

    // 判断年龄段是否有效
    static bool isAgeRangeValid(enAgeRange _age_range);

protected:


};

// “筛查超时”的值（单位：秒）       // TODO: 进一步封装为类（而不是替换）？
enum enScreenTimeout {
    screenTimeout_No        = 0,        // 0 表示无超时
    screenTimeout_30Sec     = 30,       // 30 秒             // TODO: 枚举值还是应该按照从 0 递增的方法定义，秒数另外换算？否则无法使用 min, max 值，无法自动遍历
    screenTimeout_1Min      = 60,       // 1 分钟
    screenTimeout_3Min      = 60 * 3,   // 3 分钟

    screenTimeout_Default = screenTimeout_1Min,
};

// “筛查超时”枚举值逻辑封装类
class CScreenTimeout : public CEnum<enScreenTimeout>
{
public:
    CScreenTimeout();
    CScreenTimeout(enScreenTimeout _v);
    void discrips(QList<QString> &_list) const override;    // 枚举值描述列表
    QString getDiscrip() const override;                    // 指定项的描述字符串
    static QString getDiscrip(enScreenTimeout _v);          // 指定项的描述字符串
protected:
    void init();
};

// “无操作关机时间”的值（单位：秒）
enum enShutdownNoOperation  {
    shutdownNoOperation_No      = 0,            // 0 表示无超时
    shutdownNoOperation_15Min   = 15 * 60,      // 15 分钟
    shutdownNoOperation_30Min   = 30 * 60,      // 30 分钟
    shutdownNoOperation_1Hour   = 60 * 60,      // 1 小时
    shutdownNoOperation_2Hour   = 60 * 120,     // 2 小时

    shutdownNoOperation_Default = shutdownNoOperation_15Min,
};

// “无操作关机时间”枚举值逻辑封装类
class CShutdownNoOperation : public CEnum<enShutdownNoOperation>
{
public:
    CShutdownNoOperation();
    CShutdownNoOperation(enShutdownNoOperation _v);
    void discrips(QList<QString> &_list) const override;    // 枚举值描述列表
    QString getDiscrip() const override;                    // 指定项的描述字符串
    static QString getDiscrip(enShutdownNoOperation _v);    // 指定项的描述字符串
protected:
    void init();
};

// 单双眼模式
//class CSingleDualEyeMode : public CEnum<enSingleDualEyeMode>
//{
//public:
//    CSingleDualEyeMode();
//    CSingleDualEyeMode(enSingleDualEyeMode _v);
//    void discrips(QList<QString> &_list) override;              // 枚举值描述列表
//    QString getDiscrip(enSingleDualEyeMode _v) override;        // 指定项的描述字符串
//protected:
//    void init();
//};
QString enumToText_SingleDualEyeMode(enSingleDualEyeMode _mode);     // 枚举值转文本 - 单双眼

// “光路类型”（对应、替换 v1.3、1.4 旧代码的"版本类型"）枚举值逻辑封装类
class COpticalPathType : public CEnum<enOpticalPathType>
{
public:
    COpticalPathType();
    COpticalPathType(enOpticalPathType _v);
    void discrips(QList<QString> &_list) const override;        // 枚举值描述列表
    QString getDiscrip() const override;                        // 指定项的描述字符串
    static QString getDiscrip(enOpticalPathType _v);            // 指定项的描述字符串
protected:
    void init();
};

// 参考视力的显示类型（视力记录法）
class CVisionNotation : public CEnum<enVisionNotation>
{
public:
    CVisionNotation();
    CVisionNotation(enVisionNotation _v);
    void discrips(QList<QString> &_list) const override;    // 枚举值描述列表
    QString getDiscrip() const override;                    // 指定项的描述字符串
    static QString getDiscrip(enVisionNotation _v);         // 指定项的描述字符串
protected:
    void init();
};

// 屏幕亮度（百分数的分子）
enum enScreenBrightness {
    screenBrightness_20     = 20,
    screenBrightness_40     = 40,
    screenBrightness_60     = 60,
    screenBrightness_80     = 80,

    screenBrightness_Default = screenBrightness_60,
};

// “屏幕亮度（百分数的分子）”枚举值逻辑封装类
class CScreenBrightness : public CEnum<enScreenBrightness>
{
public:
    CScreenBrightness();
    CScreenBrightness(enScreenBrightness _v);
    const CScreenBrightness &reset() override;
    void discrips(QList<QString> &_list) const override;    // 枚举值描述列表
    QString getDiscrip() const override;                    // 指定项的描述字符串
    static QString getDiscrip(enScreenBrightness _v);       // 指定项的描述字符串
protected:
    void init();
};

// 性别
class CSex : public CEnum<enSex>
{
public:
    CSex();
    CSex(enSex _v);
    const CSex &reset() override;
    void discrips(QList<QString> &_list) const override;    // 枚举值描述列表
    QString getDiscrip() const override;                    // 指定项的描述字符串
    static QString getDiscrip(enSex _v);                    // 指定项的描述字符串
protected:
    void init();
};

// 版本信息
struct stVerInfo
{
    int verMajor {-1};
    int verMinor {-1};
    int verPatch {-1};

    // 是否有效
    bool isNull() const
    { return verMajor < 0; }

    // 比较两个版本号，-1 表示前者小于后者，0 表示相等，1 表示前者大于后者
    int compareWith(const stVerInfo &_ver_info) const;
};

// 版本信息
struct stVerInfoApp : stVerInfo
{
    QDate verDate;      // 版本日期
    int verBuild {-1};       // 版本编译号

    // 是否有效
    bool isNull() const
    { return stVerInfo::isNull(); }

    // 比较两个版本号，-1 表示前者小于后者，0 表示相等，1 表示前者大于后者
    int compareWith(const stVerInfoApp &_ver_info, bool _compare_build = false) const;

};

// 距离单位
enum enDistanceUnit {
    distanceUnit_cm         = 0,        // 厘米
    distanceUnit_mm         = 1,        // 毫米
};

// 由“产品型号”枚举值得到型号字符串
QString getProductModelStr(enProductModel _model);

// 自动息屏（背光超时，Backlight Timeout）
enum class enAutoScreenOff {
    Unknown         = -1,       // 未知
    Never           = 0,        // 从不       // NOTE: UI的选项列表里此项一般排最后，与本枚举类型的定义不同，所以需另外构造
    Duration_1      ,           // 时长1
    Duration_2      ,           // 时长2
    Duration_3      ,           // 时长3

    Default         = Duration_1,
};
QString enumToText_AutoScreenOff(enAutoScreenOff _option);      // 枚举转文本（含翻译）
int enumToInt_AutoScreenOff(enAutoScreenOff _option);           // 枚举转整型（单位为秒数，0 表示无限，-1 表示未知）

// 二维码所属系统
enum class enQrCodeSystem {
    Unknown             ,       // 未知
    Manylinks           ,       // 万灵帮桥
    Huayi               ,       // 华谊
    AnHuiScreen         ,       // 安徽筛查系统
    ShanDongQinCheng    ,       // 山东勤成
};
const char *enumToName_QrCodeSystem(enQrCodeSystem _system);

// 距离传感器类型
enum class enDistSensorType {
    Unknown         = -1,           // 未知
    Mb1010          = 00,           // 超声 MB1010，连接底板
    //Xkc_DYP_A06     = 01,           // 超声 XKC DYP-A06，连接核心板
    //Xkc_KL200       = 11,           // 红外 XKC-KL200，连接核心板
    //TFLC02          = 12,           // 红外 TF-LC02，连接核心板
    //TFLuna          = 13,           // 红外 TF-Luna，连接核心板
    //TFminiS         = 14,           // 红外 TFmini-S，连接核心板
    SIMAN_SDM10     ,

    Min = Mb1010,
    Max = SIMAN_SDM10,
};
QString enumToText_DistSensorType(enDistSensorType _type, bool _is_engineer = false);   // 枚举转文本 - 测距模块类型

#endif // GLOBALCLASS_H
