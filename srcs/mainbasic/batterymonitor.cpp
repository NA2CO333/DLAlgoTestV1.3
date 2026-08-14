#include "batterymonitor.h"

#include "statusbarform.h"
#include "messagewin.h"
#include "winmeasure.h"
#include "windowsmanager.h"
#include "global.h"
#include "logger.h"

// 自动关机电压
#define SHUTDOWN_VOLTAGE    6.50

namespace  {
//constexpr double VOLTAGE_MAX = 8.6;     // 最大电压（达到则判定为已充满）
}

//
int BatteryMonitor::s_batteryAD = -1;       // 电池电压对应的 AD 值（下位机上传的原值，未经计算转换）
double BatteryMonitor::s_voltage = -1;      // 电池电压（v）
int BatteryMonitor::s_battLevel = -1;       // 电量格数

bool BatteryMonitor::s_isCharging = false;

int BatteryMonitor::s_countBattValueGot = 0;
bool BatteryMonitor::s_batteryLow = false;
CBatteryAdCalc *BatteryMonitor::s_batteryCalc = Q_NULLPTR;

//
BatteryMonitor::BatteryMonitor(QObject *_parent) : QObject(_parent)
{
    m_timer = new QTimer;
    connect(m_timer, SIGNAL(timeout()), this, SLOT(slot_timer_timeout()));  //定时器查询电池状态
    m_timer->start(5000);

    m_powerOffTimer = new QTimer;
    m_powerOffTimer->setSingleShot(true);
    connect(m_powerOffTimer, SIGNAL(timeout()), this, SLOT(slot_powerOffTimer_timeout()));

    m_powerOffCount = 0;
    m_changedDelay = 0;
    m_batteryLowCount = 0;

    //
    s_batteryCalc = new CBatteryAdCalc;

}

BatteryMonitor *BatteryMonitor::instance()
{
    if (!s_instance) {
        s_instance = new BatteryMonitor();
    }
    return s_instance;
}

BatteryMonitor::~BatteryMonitor()
{
    m_timer->stop();
    delete m_timer;
    m_timer = nullptr;
    delete m_powerOffTimer;
    m_powerOffTimer = nullptr;
}

float BatteryMonitor::getVoltage()
{
    return (s_voltage >= 0 ? s_voltage : 0);
}

int BatteryMonitor::getBattLevel()
{
    return (s_battLevel >= 0 ? s_battLevel : 0);
}

bool BatteryMonitor::getIsCharging()
{
    return s_isCharging;
}

bool BatteryMonitor::getIsChargedFull()
{
    //return (s_voltage > VOLTAGE_MAX);     // 最高电压，应考虑电池衰老后的情况？     // TODO: 目前实测，一插直充，电压就升到 8.4，无法作为充满的依据？

    return s_isChargedFull;
}

bool BatteryMonitor::isBattValueValid()
{
    bool is_valid = (m_elapsedSetAD.isValid() && m_elapsedSetAD.elapsed() < 1000 * 10);
    return is_valid;
}

void BatteryMonitor::setIsChargedFull(bool _is_full)
{
    //s_voltage = VOLTAGE_MAX + 0.1;

    s_isChargedFull = _is_full;
}

// 电压 AD 值转电量格数（旧电池）
bool BatteryMonitor::adToLevel_Old(int _batt_ad, int &_batt_level, double &_voltage)
{
    // 电压（旧代码的电压计算貌似有问题，这是拷贝 adToLevel_21700() 的）
    _voltage = _batt_ad / 461.073066667 + 0.4;

    //
    bool is_succ = false;
    float batt_num = (((float)_batt_ad - 2550) / 100.0);
    if (batt_num > 5.0) {
        _batt_level = 4;
    } else if (batt_num > 3.0 && batt_num <= 5.0) {
        _batt_level = 3;
    } else if (batt_num > 1.0 && batt_num <= 3.0) {
        _batt_level = 2;
    } else if (batt_num > 0 && batt_num <= 1.0) {     //1格电量相机不可用,可以设短一点
        _batt_level = 1;
    } else if (batt_num <= 0) {
        _batt_level = 0;
    }

    //
    return is_succ;
}

// 电压 AD 值转电量格数（21700电池）
bool BatteryMonitor::adToLevel_21700(int _batt_ad, int &_batt_level, double &_voltage)
{
    /* 文档：《20220916_王洪勇_2170新电池剩余电量和电压对应关系（测试）-测试结果及更改意见.xls》
     *
     * AD值 = (电池电压 - 0.5) * (0.3377 * 4096 / 3) = (电池电压 - 0.5) * 461.073066667
     * 电压 = AD值 / 461.073066667 + 0.5
     *
     * >=7.40v          : 4格
     * >=7.12v, <7.40v  : 3格
     * >=6.90v, <7.12v  : 2格
     * >=6.60v, <6.90v  : 1格
     * <6.60v           : 0格
     *
     */
    /*
     * 自动关机电压：6.50v
     * 不能拍摄电压：0格
     */

    //
    _voltage = _batt_ad / 461.073066667 + 0.5;

    //
    bool is_succ = false;
    if (Util::compDouble(_voltage, 7.40) >= 0) {
        _batt_level = 4;
        is_succ = true;
    } else if (Util::compDouble(_voltage, 7.12) >= 0 && Util::compDouble(_voltage, 7.40) < 0) {
        _batt_level = 3;
        is_succ = true;
    } else if (Util::compDouble(_voltage, 6.90) >= 0 && Util::compDouble(_voltage, 7.12) < 0) {
        _batt_level = 2;
        is_succ = true;
    } else if (Util::compDouble(_voltage, 6.60) >= 0 && Util::compDouble(_voltage, 6.90) < 0) {
        _batt_level = 1;
        is_succ = true;
    } else if (_voltage > 0 && Util::compDouble(_voltage, 6.60) < 0) {
        _batt_level = 0;
        is_succ = true;
    }

    //
    return is_succ;
}

void BatteryMonitor::setBatteryAD(int _batt_ad, bool _is_charging, bool _is_full)
{
    //
    bool is_valid_old = isBattValueValid();

    // 程序启动完成事件
    if (-1 == s_batteryAD) {
        globalService()->checkStartupEvent(2);
    }

    //
    const int   old_level       = getBattLevel();
    const bool  old_is_charging = getIsCharging();
    const bool  old_is_full     = getIsChargedFull();

    // 更新本模块的 充电状态                  // NOTE: 充电状态要在 AD 之前设置，因为设置 CBatteryAdCalc::setAdValue() 时需用到充电状态
    setIsCharging(_is_charging);

    // 更新电量计算模块的 AD 值
    s_batteryCalc->setAdValue(_batt_ad);

    // 从【电量计算模块】获取计算后的 AD 值
    int batt_ad = s_batteryCalc->getAdCalc();

    // 更新本模块的电量
    int batt_level;
    double volt;
    //bool is_succ = adToLevel_Old(batt_ad, batt_level, volt);
    bool is_succ = adToLevel_21700(batt_ad, batt_level, volt);
    if (is_succ) {
        s_batteryAD = batt_ad;
        s_battLevel = batt_level;
        s_voltage = volt;

        // 收到电量次数计数
        if (0 == s_countBattValueGot) {
            s_countBattValueGot++;        // TODO: 一段时间内未收到电量值，置零？
        }
    } else {
        qDebug() << "ADC value error!";
    }

    //
    setIsChargedFull(_is_full);

    //
    m_elapsedSetAD.start();

    //
    static bool is_first_set = true;
    bool is_changed = (false
                       || old_level         != getBattLevel()
                       || old_is_charging   != getIsCharging()
                       || old_is_full       != getIsChargedFull());
    if (is_changed || is_first_set || !is_valid_old) {
        emit sigBatteryChanged(getBattLevel(), getIsCharging(), getIsChargedFull());
        is_first_set = false;
    }

    //
    logDebug(QString("set battery ad = %1, charging = %2, is_full = %3, batt_level = %4")
             .arg(_batt_ad).arg(Util::bool2str(_is_charging)).arg(Util::bool2str(_is_full)).arg(s_battLevel), CGlobal::LOG_SYS);
}

void BatteryMonitor::setIsCharging(bool _is_charging)
{
    if (_is_charging != s_isCharging) {
        //
        s_isCharging = _is_charging;

        //
        if (_is_charging && s_batteryLow) {
            s_batteryLow = false;
        }

        //
        s_batteryCalc->setChargingStat(_is_charging);
    }
}

bool BatteryMonitor::isBattValueGot()
{
    bool ret = (s_countBattValueGot > 0);
    //s_countBattValueGot--;
    return ret;
}

void BatteryMonitor::slot_timer_timeout()
{
    if(!s_batteryLow && !s_isCharging)    //电池电量低且非充电状态
    {
        if(s_battLevel > 1){   //大于1格电
            m_batteryLowCount=0;
            s_batteryLow = false;
            return;
        }
        else{
#if OS_TYPE != 2
            m_batteryLowCount++;      //电量过低次数累加
#endif
            if(m_batteryLowCount>=5){
                m_batteryLowCount=5;  //固定变量值,防止无限自加
                qDebug()<<"--电量过低次数累加:"<<m_batteryLowCount;
                s_batteryLow = true;

                // open backlight if nessesary
                PowerControl::getlInstance()->reset();

                // notice voice
                g_SoundIntf->playPrompt(soundPrompt_LowBattery);

                QString text = tr("电量过低!"); // "Low battery!"
                getWinManage()->showMsgWin(text);

                QElapsedTimer timer;
                timer.start();
                while (g_SoundIntf->isPlaying() && timer.elapsed() < 3000) {
                    Util::waitMs(300);
                }
            }
        }
    }

    if (Util::compDouble(s_voltage, SHUTDOWN_VOLTAGE) <= 0 && !s_isCharging)
    {
#if OS_TYPE != 2
        m_powerOffCount++;
#endif
        if(m_powerOffCount >= 2)
        {
            // open backlight if nessesary
            PowerControl::getlInstance()->reset();

            // notice voice
            g_SoundIntf->playPrompt(soundPrompt_Shutdown);

            // 低电量LED闪烁
            MySerialPort::instance()->write(lowBatteryFlash);

            // auto power off 5 seconds later without any operator
            m_powerOffTimer->start(5000);

            MessageWin win_poweroff_warning;
            win_poweroff_warning.setContent(tr("电量过低,正在关机..."));    // "Low battery,shutting down..."
            win_poweroff_warning.exec();

            QElapsedTimer timer;
            timer.start();
            while (g_SoundIntf->isPlaying() && timer.elapsed() < 3000) {
                Util::waitMs(300);
            }

            // 执行关机后，停止定时器，以免每隔几秒就弹出提示框，方便调试
            m_timer->stop();

            // 关机
            globalService()->powerOff();

        }
    }
    else
    {
        m_powerOffCount = 0;
    }
}

void BatteryMonitor::slot_powerOffTimer_timeout()
{
    globalService()->powerOff();
}

/// =========================================================================================================
/// class CBatteryAdCalc

// 停止充电后多久，电量才可信（单位：毫秒）
static const int AD_TRUSTY_AFTER_NONCHARGING_MS     = 0;    // TODO: 关于这个，最开始测试时，好像有这种现象，但是后来又没有啦？

// 充电状态下 每小时 AD 的增加量
/* 据《20220916_王洪勇_2170新电池剩余电量和电压对应关系（测试）-测试结果及更改意见.xls》，放电时 5 小时大约降 1.5v。
 * 据说充电时反过来的时间关系也差不多，再保守点，算 1.35v 。
 * 据 adToLevel_21700() 的算式，AD 增量是 电压增量的 461.073066667 倍。
 */
static const double AD_INCREASE_PER_HOUR            = 1.35 * 461.073066667 / 5;

//
CBatteryAdCalc::CBatteryAdCalc()
{
    m_elapsedTimer.start();
}

void CBatteryAdCalc::setChargingStat(bool _is_charging)
{
    enChargingStat charging_stat = (_is_charging ? enChargingStat::Yes : enChargingStat::No);
    if (charging_stat != m_chargingStat) {
        m_chargingStat = charging_stat;
        m_elapsedTimer.start();
    }
}

void CBatteryAdCalc::setAdValue(int _ad)
{
    // 更新 AD 值      /* 若历史 AD 值是可信的，则忽略不可信的 AD 值，否则，记录不可信的 AD 值  */
    bool is_trusty_new = false;
    if (enChargingStat::No == m_chargingStat) {
        if (m_elapsedTimer.elapsed() > AD_TRUSTY_AFTER_NONCHARGING_MS) {          // 停止充电后，须过一段时间，电量才可信
            is_trusty_new = true;
        }
    }
    bool is_trysty_old = m_isAdTrusty;

    if (is_trusty_new) {
        m_batteryAd = _ad;
        if (!m_isAdTrusty) {
            m_isAdTrusty = true;
        }
    } else {
        if (!is_trysty_old) {
            m_batteryAd = _ad;
        }
    }
}

int CBatteryAdCalc::getAdCalc()
{
    if (enChargingStat::No == m_chargingStat) {
        return m_batteryAd;
    } else if (enChargingStat::Yes == m_chargingStat) {
        if (m_isAdTrusty) {
            // 充电状态，根据累积时间，计算当前电量
            return (m_batteryAd + ((double)m_elapsedTimer.elapsed() / 1000 / 3600 * AD_INCREASE_PER_HOUR));
        } else {
            return m_batteryAd;
        }
    } else {
        return m_batteryAd;
    }
}
