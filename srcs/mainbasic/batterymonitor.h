#ifndef BATTERYMONITOR_H
#define BATTERYMONITOR_H

#include <QObject>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>

//
class CBatteryAdCalc;

//
class BatteryMonitor : public QObject
{    
    Q_OBJECT

public:
    static BatteryMonitor *instance();
    ~BatteryMonitor();

    void setBatteryAD(int _batt_ad, bool _is_charging, bool _is_full);             // TODO: 这些不应是静态？

    static bool isBattValueGot();       // 是否已收到电量      // TODO: 没必要？梳理代码，将未设置的电量设为 -1
    static float getVoltage();
    static int getBattLevel();
    static bool getIsCharging();

    static bool getIsChargedFull();

    bool isBattValueValid();                // 电量值是否有效（太长时间未更新时，判定无效）

signals:
    void sigBatteryChanged(int _batt_ad, bool _is_charging, bool _is_full);

protected slots:
    void slot_timer_timeout();
    void slot_powerOffTimer_timeout();

protected:
    explicit BatteryMonitor(QObject *_parent = nullptr);
    static inline BatteryMonitor *s_instance {nullptr};

    static int s_batteryAD;             // 电量 AD 值（下位机上传的原值，未经计算转换）
    static double s_voltage;            // 电压
    static int s_battLevel;             // 几格电（0~4）     // TODO: 是否充电状态等，也从 statusbarform 移到这里
    static bool s_isCharging;           // 是否正在充电
    static inline bool s_isChargedFull {false};     // 是否已充满

    QElapsedTimer m_elapsedSetAD;         // 上次设置 AD 计时

    static void setIsCharging(bool _is_charging);
    static void setIsChargedFull(bool _is_full);

    static bool adToLevel_Old(int _batt_ad, int &_batt_level, double &_voltage);
    static bool adToLevel_21700(int _batt_ad, int &_batt_level, double &_voltage);

    static bool s_batteryLow;
    static int s_countBattValueGot;         // 电量值获取次数

    QTimer *m_timer;
    QTimer *m_powerOffTimer;
    int m_powerOffCount;
    int m_changedDelay;
    int m_batteryLowCount = 0;

    static CBatteryAdCalc *s_batteryCalc;

};

/// =========================================================================================================
/// class CBatteryAdCalc

#include <QElapsedTimer>

// 电池电量（AD 值）计算
/* 只有非充电状态下的电量才有效，而充电状态下的电量，则根据累计充电时间计算。
 */
class CBatteryAdCalc
{
public:
    explicit CBatteryAdCalc();

    void setChargingStat(bool _is_charging);            // 设置充电状态
    void setAdValue(int _ad);                           // 设置 AD 值（非充电状态下才有效）

    int getAdCalc();                                    // 获取经过计算的 AD 值（可能是未知值）

protected:
    enum class enChargingStat {
        Unknown,
        Yes,
        No,
    };

    int m_batteryAd = -1;                                         // 电池 AD 值
    enChargingStat m_chargingStat = enChargingStat::Unknown;      // 充电状态
    bool m_isAdTrusty = false;                                    // 电池 AD 值是否可信（比如在程序启动前就一直处于直充状态，那么程序就一直得不到可信的电量值）

    QElapsedTimer m_elapsedTimer;

};

#endif // BATTERYMONITOR_H
