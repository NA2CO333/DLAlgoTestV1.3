//标题栏,显示日期时间,wifi以及电池图标
#include "statusbarform.h"
#include "ui_statusbarform.h"

#include <QMutex>
#include "QDebug"

//#include "winmeasure.h"
//#include "blockmousepress.h"
//#include "myserialport.h"
#include "windowsmanager.h"
//#include "appsetting.h"
#include "global.h"
#include "utilui.h"
#include "logger.h"
#include "winmanage.h"

//
#define BATVAL_0        0
#define BATVAL_1        1
#define BATVAL_2        2
#define BATVAL_3        3
#define BATVAL_4        4

namespace  {
constexpr int C_TIMER_INDETERMINATE_PROGRESS_INTERVAL_MS = 500;      // 电量“不确定进度条”定时器的触发间隔（ms）
}

//
WgtStatusBar *WgtStatusBar::s_instance = Q_NULLPTR;

WgtStatusBar *WgtStatusBar::instance()
{
    if (!s_instance) {
        s_instance = new WgtStatusBar();
    }
    return s_instance;
}

WgtStatusBar::WgtStatusBar(QWidget *_parent) :
    QWidget(_parent),
    ui(new Ui::WgtStatusBar)
{
    ui->setupUi(this);

    //
    Util::Ui::clearStyleSheet(this);

    // 窗体尺寸
#if (SCREEN_SIZE_TYPE == 2)
    QRect rect_self = this->geometry();
    this->setGeometry(rect_self.left(), rect_self.top(), SCREEN_WIDTH, STATUSBAR_HEIGHT);
#endif
//    qDebug() << "STATUSBAR_HEIGHT = " << STATUSBAR_HEIGHT << ", this->height() = " << this->height();

    //
    ui->lblTitle->setText("");
    setStatusBarProperty();

    m_timerUpdateTime = new QTimer();
    connect(m_timerUpdateTime,SIGNAL(timeout()),this,SLOT(slot_timerUpdateTime_timeout()));
    m_timerUpdateTime->start(1000);

    this->setAttribute(Qt::WA_TranslucentBackground,true);

    this->ui->lblBattery->setVisible(false);

    //
    m_timerIndeterminateLinearProgress = new QTimer();
    m_timerIndeterminateLinearProgress->setInterval(C_TIMER_INDETERMINATE_PROGRESS_INTERVAL_MS);
    QObject::connect(m_timerIndeterminateLinearProgress, &QTimer::timeout, this, &WgtStatusBar::slot_timerIndeterminateLinearProgress_timeout, Qt::QueuedConnection);

    //
    ui->lblUSBDrive->setVisible(false);

}

WgtStatusBar::~WgtStatusBar()
{
    delete ui;
}

void WgtStatusBar::showEvent(QShowEvent *_event)
{
    Q_UNUSED(_event)

    //qDebug() << "STATUSBAR_HEIGHT = " << STATUSBAR_HEIGHT << ", this->height() = " << this->height();
    if (CGlobal::isDebugMode) {
        assert(STATUSBAR_HEIGHT == this->height());
    }

    setCurrentThemeType(getSysThemeType());

}

void WgtStatusBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    if(themeType_Black == getSysThemeType())
        painter.setBrush(QColor(80,80,80,100));
    else
        painter.setBrush(QColor(200,200,200,100));
    QPen pen = painter.pen();
    pen.setStyle(Qt::NoPen);        //无边框
    painter.setPen(pen);
    painter.drawRect(rect());       //颜色填充
}

void WgtStatusBar::hideEvent(QHideEvent *)
{
    //logDebug("WgtStatusBar::hideEvent() called", CGlobal::LOG_SYS);
}

void WgtStatusBar::mouseReleaseEvent(QMouseEvent *_e)
{
    static QString obj_name_tool;
    static bool is_name_got = false;
    if (!is_name_got) {
        obj_name_tool = getWinManage()->getWindowNameByType(WIN_TOOL);              // TODO: 这样修改后，使用起来比以前麻烦了？
        is_name_got = true;
    }

    // 传到工具界面
    QWidget *curr_widget = getWinManage()->getCurrentWin();
    if (curr_widget) {
        if (curr_widget->objectName() == obj_name_tool) {
            globalService()->getToolWin()->debugMouseClick(_e->pos().x(), _e->pos().y());
        }
    }

}

void WgtStatusBar::setCurrParent(QWidget *_w)
{
    if (!_w) {
        return;
    }

    CBaseFormIntf *form_intf = dynamic_cast<CBaseFormIntf *>(_w);
    if (!form_intf) {
        QString msg = QString(__PRETTY_FUNCTION__) + ": " + _w->objectName() + "is not a CBaseFormIntf";
        logWarning(msg);
        if (CGlobal::isDebugMode) {
            getWinManage()->showSuspensionPrompt(msg);
        }
        return;
    }

    WgtStatusBar *widg_status = instance();
    if (form_intf->isShowStatusBar) {
        widg_status->setParent(_w);         // TODO: 这里切换 parent 时，总会被隐藏？
        //status_win->setTitle(_w->windowTitle());     // TODO: 需将所有窗体对 WgtStatusBar 的标题的修改改为对自身标题的修改，且连接父窗体和状态栏的标题改变信号，才能执行这个语句

        widg_status->raise();
        if (!widg_status->isVisible() /*&& _w->isVisible()*/) {
            widg_status->show();
        }
    } else {
        widg_status->hide();
    }
}

void WgtStatusBar::setCurrentThemeType(enThemeType _theme_type)
{
    //设置标题栏样式
    if(themeType_Black == _theme_type){
        this->setStyleSheet("QWidget{background-color:rgb(20,23,31);}");

        ui->lblTitle->setStyleSheet("QLabel{background-color:transparent; color:rgb(204,204,204);}");
        ui->lblDateTime->setStyleSheet("QLabel{background-color:transparent; color:rgb(204,204,204);}");
        ui->lblBT->setStyleSheet("QLabel{background-color:transparent; color:rgb(204,204,204);}");
        //ui->lblBattery->setStyleSheet("QLabel{background-color:transparent; color:rgb(204,204,204);}");

        ui->lblDevActivate->setStyleSheet("QLabel{ color: rgb(200, 65, 65); background-color: transparent; }");
        ui->lblUSBDrive->setStyleSheet("QLabel { image: url(:/resource/black_theme/icon_usb-drive_b.png); background-color:transparent; }");
    }
    else{
        ui->lblTitle->setStyleSheet("QLabel{background-color:transparent; color:rgb(1,1,1);}");
        ui->lblDateTime->setStyleSheet("QLabel{background-color:transparent; color:rgb(1,1,1);}");
        ui->lblBT->setStyleSheet("QLabel{background-color:transparent; color:rgb(1,1,1);}");
        //ui->lblBattery->setStyleSheet("QLabel{background-color:transparent; color:rgb(1,1,1);}");

        ui->lblDevActivate->setStyleSheet("QLabel{ color: rgb(200, 65, 65); background-color: transparent; }");
        ui->lblUSBDrive->setStyleSheet("QLabel { image: url(:/resource/black_theme/icon_usb-drive_b.png); background-color:transparent; }");
    }

    //
    updateTitle();
    updateWifiIcon();
    updateBlueToothIcon();
    updateBatteryIcon();
    updateIsUsbDrivePlugged();
}

void WgtStatusBar::setStatusBarProperty()
{
    m_palette = this->palette();
    m_palette.setColor(QPalette::WindowText,Qt::white);
    ui->lblDateTime->setPalette(m_palette);
    ui->lblTitle->setPalette(m_palette);
    //ui->batteryValue->setPalette(mPalette);
    this->setWindowFlag(Qt::FramelessWindowHint, true);
}

void WgtStatusBar::updateTitle()
{
    // NOTE: 防止漏设置，所以这里每次都获取一次          // TODO: 逻辑优化？
    m_trialDesc = CWinManage::getTrialDesc();

    //
    QString title_str = m_title
            + (!m_titleSub.isEmpty() ? "    " + m_titleSub : "")
            + (!m_trialDesc.isEmpty() ? "  " + m_trialDesc : "");
    ui->lblTitle->setText(title_str);

    //
    bool not_activated = !CGlobal::isDevActivated;
    ui->lblDevActivate->setVisible(not_activated);
}

void WgtStatusBar::setTitle(const QString &_title)
{
    m_title = _title;

    m_titleSub.clear();     // NOTE: 兼容旧代码，同时清空副标题      // TODO: 需求梳理及逻辑优化

    updateTitle();
}

void WgtStatusBar::setTitleSub(const QString &_title_sub)
{
    m_titleSub = _title_sub;

    updateTitle();
}

//void WgtStatusBar::setTrialDesc(const QString &_desc)
//{
//    m_trialDesc = _desc;
//
//    updateTitle();
//}

void WgtStatusBar::setIsWifiOpened(bool _is_opened)
{
    m_isWifiOpened = _is_opened;

    updateWifiIcon();
}

void WgtStatusBar::setIsWifiConnected(bool _is_connnected)
{
    m_isWifiConnected = _is_connnected;

    updateWifiIcon();
}

void WgtStatusBar::setIsBtOpened(bool _is_opened)
{
    m_isBtOpened = _is_opened;

    updateBlueToothIcon();
}

void WgtStatusBar::setIsBtConnected(bool _is_connnected)
{
    m_isBtConnected = _is_connnected;

    updateBlueToothIcon();
}

void WgtStatusBar::setBatteryStat(int _batt_level, bool _is_charging, bool _is_full)
{
    m_batteryLevel  = _batt_level;
    m_isCharging    = _is_charging;
    m_isFull        = _is_full;

    updateBatteryIcon();
}

void WgtStatusBar::setIsUsbDrivePlugged(bool _is_plugged)
{
    m_IsUsbDrivePlugged = _is_plugged;

    updateIsUsbDrivePlugged();
}

void WgtStatusBar::slot_BatteryChanged(int _batt_ad, bool _is_charging, bool _is_full)
{
    setBatteryStat(_batt_ad, _is_charging, _is_full);
}

void WgtStatusBar::slot_timerUpdateTime_timeout()
{
    // 刷新状态栏时间
    m_dateTime = QDateTime::currentDateTime();
    QString time_str = m_dateTime.toString(DATE_TIME_FORMAT);
    ui->lblDateTime->setText(time_str);

    // 若太长时间未刷新电量，则隐藏电量图标
    if (!BatteryMonitor::instance()->isBattValueValid()) {
        if (ui->lblBattery->isVisible()) {
            ui->lblBattery->setVisible(false);
        }
        if (ui->lblCharging->isVisible()) {
            ui->lblCharging->setVisible(false);
        }
    }
}

void WgtStatusBar::slot_timerIndeterminateLinearProgress_timeout()
{
    //
    m_indeterminateLinearProgressValue++;
    if (m_indeterminateLinearProgressValue > BATVAL_4) {
        m_indeterminateLinearProgressValue = BATVAL_1;
    }

    //
    updateBatteryLevel(m_indeterminateLinearProgressValue, true);
}

void WgtStatusBar::updateWifiIcon()
{
    if (m_isWifiOpened) {
        //
        if (!ui->lblWiFi->isVisible()) {
            ui->lblWiFi->setVisible(true);
        }

        //
        if (m_isWifiConnected) {
            //
            enWiFiStrengthLevel wifi_level = wiFiStrengthLevel_1;
            const stWifiInfo *wifi_conn_info = g_WifiIntf->getConnectedInfo();
            if (wifi_conn_info) {
                wifi_level = CWifiIntf::rssiToStrengthLevel((wifi_conn_info->rssi));
            }

            //
            if (themeType_Black == getSysThemeType()) {
                if(!wifi_conn_info) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/black_theme/wifi_disconnect_b.png);background-color:transparent;");
                } else if (wiFiStrengthLevel_1 == wifi_level) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/black_theme/wifi_signal1_b.png);background-color:transparent;");
                } else if (wiFiStrengthLevel_2 == wifi_level) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/black_theme/wifi_signal2_b.png);background-color:transparent;");
                } else if (wiFiStrengthLevel_3 == wifi_level) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/black_theme/wifi_signal3_b.png);background-color:transparent;");
                } else if (wiFiStrengthLevel_4 == wifi_level) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/black_theme/wifi_signal4_b.png);background-color:transparent;");
                }
            } else {
                if (!wifi_conn_info) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/white_theme/wifi_disconnect_w.png);background-color:transparent;");
                } else if (wiFiStrengthLevel_1 == wifi_level) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/white_theme/wifi_signal1_w.png);background-color:transparent;");
                } else if (wiFiStrengthLevel_2 == wifi_level) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/white_theme/wifi_signal2_w.png);background-color:transparent;");
                } else if (wiFiStrengthLevel_3 == wifi_level) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/white_theme/wifi_signal3_w.png);background-color:transparent;");
                } else if (wiFiStrengthLevel_4 == wifi_level) {
                    ui->lblWiFi->setStyleSheet("background-image: url(:/resource/white_theme/wifi_signal4_w.png);background-color:transparent;");
                }
            }
        } else {
            if (themeType_Black == getSysThemeType()) {
                ui->lblWiFi->setStyleSheet("background-image: url(:/resource/black_theme/wifi_disconnect_b.png);background-color:transparent;");
            } else {
                ui->lblWiFi->setStyleSheet("background-image: url(:/resource/white_theme/wifi_disconnect_w.png);background-color:transparent;");
            }
        }
    } else {
        if (ui->lblWiFi->isVisible()) {
            ui->lblWiFi->setVisible(false);
        }
    }
}

void WgtStatusBar::updateBlueToothIcon()
{
    if (m_isBtOpened) {
        //
        if (!ui->lblBT->isVisible()) {
            ui->lblBT->setVisible(true);
        }

        //
        if (m_isBtConnected) {
            if (themeType_Black == getSysThemeType()) {
                ui->lblBT->setStyleSheet("background-image: url(:/resource/bluetooth/bt_connect_b.png);background-color:transparent;");
            } else {
                ui->lblBT->setStyleSheet("background-image: url(:/resource/bluetooth/bt_connect_w.png);background-color:transparent;");
            }
        } else {
            if (themeType_Black == getSysThemeType()) {
                ui->lblBT->setStyleSheet("background-image: url(:/resource/bluetooth/bt_disconnect_b.png);background-color:transparent;");
            } else {
                ui->lblBT->setStyleSheet("background-image: url(:/resource/bluetooth/bt_disconnect_w.png);background-color:transparent;");
            }
        }
    } else {
        if (ui->lblBT->isVisible()) {
            ui->lblBT->setVisible(false);
        }
    }
}

void WgtStatusBar::updateBatteryIcon()
{
    if (BatteryMonitor::isBattValueGot() && BatteryMonitor::instance()->isBattValueValid()) {
        if (!ui->lblBattery->isVisible()) {
            ui->lblBattery->setVisible(true);
        }

        if (BatteryMonitor::getIsCharging()) {
            if (!ui->lblCharging->isVisible()) {
                ui->lblCharging->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_charge.png); background-color:transparent;} ");
                ui->lblCharging->setVisible(true);
            }
        } else {
            if (ui->lblCharging->isVisible()) {
                ui->lblCharging->setVisible(false);
            }
        }

        // 电量显示
        int batt_level = BatteryMonitor::getBattLevel();     // TODO: 充电和电量状态值应该归为一类？
        if (!BatteryMonitor::getIsCharging()) {     // 若是非充电状态
            // 停止电量“不确定进度条”动画
            setBattetryIndeterminateLinearProgressStarted(false);

            //
            updateBatteryLevel(batt_level, false);                          // TODO: 旧协议，一直不会显示满电？
        } else {                                    // 若是正在充电状态
            if (!MySerialPort::isProtocalSupportChargedFull()) {        // 旧协议的充电状态电量显示
                // 停止电量“不确定进度条”动画
                setBattetryIndeterminateLinearProgressStarted(false);

                // 显示程序计算后的电量等级
                updateBatteryLevel(batt_level, false);
            } else {                                                    // 新协议的充电状态电量显示
                // 若未充满，则电量滚动
                if (!BatteryMonitor::getIsChargedFull()) {
                    // 开始电量“不确定进度条”动画
                    setBattetryIndeterminateLinearProgressStarted(true);
                } else {
                    // 停止电量“不确定进度条”动画
                    setBattetryIndeterminateLinearProgressStarted(false);

                    // 满电图标
                    ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_full.png); background-color:transparent;}");
                }
            }
        }
    } else {
        if (ui->lblBattery->isVisible()) {
            ui->lblBattery->setVisible(false);
        }
        if (ui->lblCharging->isVisible()) {
            ui->lblCharging->setVisible(false);
        }
    }
}

void WgtStatusBar::updateIsUsbDrivePlugged()
{
    ui->lblUSBDrive->setVisible(m_IsUsbDrivePlugged);
}

void WgtStatusBar::setBattetryIndeterminateLinearProgressStarted(bool _is_start)
{
    if (_is_start) {
        if (!m_timerIndeterminateLinearProgress->isActive()) {
            m_timerIndeterminateLinearProgress->start();
        }
    } else {
        if (m_timerIndeterminateLinearProgress->isActive()) {
            m_timerIndeterminateLinearProgress->stop();
        }
    }
}

void WgtStatusBar::updateBatteryLevel(int _batt_level, bool _is_charging)
{
    //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << " : level = " << _batt_level;

    switch (_batt_level) {
    case BATVAL_0:
        if (!_is_charging) {
            ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_0.png); background-color:transparent;}");
        } else {
            ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_0_green.png); background-color:transparent;}");
        }
        break;
    case BATVAL_1:
        if (!_is_charging) {
            ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_1.png); background-color:transparent;}");
        } else {
            ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_1_green.png); background-color:transparent;}");
        }
        break;
    case BATVAL_2:
        if (!_is_charging) {
            ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_2.png); background-color:transparent;}");
        } else {
            ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_2_green.png); background-color:transparent;}");
        }
        break;
    case BATVAL_3:
        ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_3.png); background-color:transparent;}");
        break;
    case BATVAL_4:
        ui->lblBattery->setStyleSheet("QLabel {image: url(:/resource/battery/battery_b_4.png); background-color:transparent;}");
        break;
    default:
        ui->lblBattery->setStyleSheet("QLabel {image: url(); background-color:transparent;}");
        break;
    }
}

void WgtStatusBar::on_lblDevActivate_clicked()
{
    // 显示设备激活二维码
    globalService()->showDevActivateDialog();
}
