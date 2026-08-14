#ifndef STATUSBARFORM_H
#define STATUSBARFORM_H

#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QElapsedTimer>

#include "messagewin.h"
#include "baseform.h"

//
namespace Ui {
class WgtStatusBar;
}

// 状态栏信息
class WgtStatusBar : public QWidget
{
    Q_OBJECT

public:
    static WgtStatusBar *instance();
    ~WgtStatusBar();

    void setCurrParent(QWidget *_w);

    void setCurrentThemeType(enThemeType _theme_type);

    /* NOTE: 先设置内部变量，再刷新 UI，这样内部也可自我刷新（如改变样式时）
     */

    void updateTitle();                                                         // 更新标题（标题包含标题文本及附注）

    void setTitle(const QString &_title);                                       // 设置主标题
    void setTitleSub(const QString &_title_sub);                                // 设置次标题（主标题保留原值）
    //void setTrialDesc(const QString &_desc);                                    // 设置试用机描述    // NOTE: (2026-07-16)目前采用内部实时获取的方式  // TODO: 需求梳理及逻辑优化

    void setIsWifiOpened(bool _is_opened);                                      // WiFi 是否已开启
    void setIsWifiConnected(bool _is_connnected);                               // WiFi 是否已连接

    void setIsBtOpened(bool _is_opened);                                        // 蓝牙是否已开启
    void setIsBtConnected(bool _is_connnected);                                 // 蓝牙是否已连接

    void setBatteryStat(int _batt_level, bool _is_charging, bool _is_full);     // 设置电量格数、充电状态、是否充满

    void setIsUsbDrivePlugged(bool _is_plugged);                                // 设置 U 盘是否已插上

public slots:
    void slot_BatteryChanged(int _batt_ad, bool _is_charging, bool _is_full);

protected slots:
    void slot_timerUpdateTime_timeout();
    void slot_timerIndeterminateLinearProgress_timeout();

protected:
    explicit WgtStatusBar(QWidget *_parent = 0);
    static WgtStatusBar *s_instance;

    void showEvent(QShowEvent *_event) override;
    void paintEvent(QPaintEvent *) override;
    void hideEvent(QHideEvent *) override;
    void mouseReleaseEvent(QMouseEvent *_e) override;

    //
    void updateWifiIcon();
    void updateBlueToothIcon();
    void updateBatteryIcon();
    void updateIsUsbDrivePlugged();

    //
    void setBattetryIndeterminateLinearProgressStarted(bool _is_start);         // 设置电量“不确定进度条”动画的开启状态

    //
    void setStatusBarProperty();

    void updateBatteryLevel(int _batt_level, bool _is_charging);

    //
    static inline const QString DATE_TIME_FORMAT = "yyyy-MM-dd  HH:mm";

    QTimer *m_timerUpdateTime {nullptr};    // 时间更新定时器

    QPalette m_palette;

    //
    QString m_title;                        // 主标题
    QString m_titleSub;                     // 次标题
    QString m_trialDesc;                    // 试用机描述
    QDateTime m_dateTime;                   // 时间

    //
    bool m_isWifiOpened {false};            // WiFi 是否已开启
    bool m_isWifiConnected {false};         // WiFi 是否已连接

    bool m_isBtOpened {false};              // 蓝牙是否已开启
    bool m_isBtConnected {false};           // 蓝牙是否已连接

    int m_batteryLevel {0};                 // 电量格数
    bool m_isCharging {false};              // 是否正在充电
    bool m_isFull {false};                  // 是否充满

    bool m_IsUsbDrivePlugged {false};       // U 盘是否已插上

    //
    QTimer *m_timerIndeterminateLinearProgress {nullptr};   // 电量“不确定进度条”定时器
    int m_indeterminateLinearProgressValue {0};             // 电量“不确定进度条”的当前进度值

private slots:
    void on_lblDevActivate_clicked();
private:
    Ui::WgtStatusBar *ui;
};

#endif // STATUSBARFORM_H
