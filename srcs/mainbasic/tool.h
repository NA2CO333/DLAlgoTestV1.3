#pragma once
#ifndef TOOL_H
#define TOOL_H

#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QEvent>
#include <QMouseEvent>

#include "baseform.h"
#include "statusbarform.h"
#include "engineermode/engineerpassword.h"
#include "musicsetting.h"
#include "settings/settings.h"

//
extern bool g_MinResolution;        // 视力度数的分辨率：true: 0.01D, false: 0.25D
extern bool g_isHmMode;             // 是否高度数模式

//extern bool g_isSavePreviewImage;   // 是否保存预览图（经过直方图均衡化处理，用于 A4 报表修饰）
extern bool g_isSaveSampleImage;    // 是否保存转灯图抽样（目前为 12、18 帧，未经处理的原图）

//
class appSetting;

//
namespace Ui {
class Tool;
}

class Tool : public CBaseWidget
{
    Q_OBJECT

public:
    explicit Tool(QWidget *parent = 0);
    ~Tool();

    void refreshText();     // 刷新按钮文本
    void refreshIcon();     // 刷新按钮图标（需要根据当前语言切换的）

    void QPropertyAnimationMoveLeft();
    void QPropertyAnimationMoveRight();
    void ReleaseMoveLeftSlide(int duration);
    void ReleaseMoveRightSlide(int duration);
    void ReleaseMoveLeftSlideReset(int duration);
    void ReleaseMoveRightSlideReset(int duration);

    void debugMouseClick(int _x, int _y);               // 鼠标点击事件时，处理连续点3下显示工程调试界面的逻辑    // TODO: 待优化

private slots:
    void on_pushButton_back_clicked();
    void on_btnWiFi_clicked();
    void on_btnStandard_clicked();
    void on_btnSingleDualEye_clicked();
    void on_btnResolution_clicked();
    void on_btnPrintSetting_clicked();
    void on_btnDateTime_clicked();
    void on_btnLanguage_clicked();
    void on_btnAbout_clicked();
    void resetMouseClicked();
    void InquireWidgetLocation();
    void on_btnHmMode_clicked();
    void on_btnDataTrans_clicked();
    void on_btnMusic_clicked();
    void on_pushButtonPrevPage_clicked();
    void on_pushButtonNextPage_clicked();
    void on_btnUpdate_clicked();
    void on_btnSettings_clicked();
    void on_btnTheme_clicked();
    void on_btnStorageStat_clicked();
    void on_btnBluetooth_clicked();
    void on_btnDistCalibrate_clicked();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void showEvent(QShowEvent *);      //add for engineerModel by sun 20180826
    void hideEvent(QHideEvent *);      //add for engineerModel by sun 20180826

    static void initConfigs();

    void initButtonsPosition();         // 初始化各个功能按钮的位置和状态（与 Form 设计图一致）
    void setFixedButtonsPosition();     // 固定不变的按钮位置和状态
    void setDynamicButtonsPosition();   // 动态设置的按钮位置和状态

    /**
     * @brief 按页、行、列参数设置工具按钮的位置
     * @param _name_key 名称关键字（除了前缀外的其余部分）
     * @param _page     页索引号，以0开始。若是隐藏，则不需此参数。
     * @param _row      行号，以0开始。若是隐藏，则不需此参数。
     * @param _col      列号，以0开始。若是隐藏，则不需此参数。
     * @param _is_visible   是否可见，若否，则隐藏
     */
    void positionButton(const QString &_name_key, int _page, int _row, int _col, bool _is_visible = true);
    void positionButton(const QWidget *_wgt, int _page, int _row, int _col, bool _is_visible = true);

private:
    Ui::Tool *ui;
    QTimer clickTimer;
    QTimer *InquireLocation;

    int countMouseClicked;
    int cnt;
    int countMouseClicked2;
    QPropertyAnimation *m_leftAnimation;
    QPropertyAnimation *m_rightAnimation;
    bool m_bRight;          //是否向右移动
    QPoint m_startPos;      //鼠标按下坐标
    bool m_Oncepress;       //每当鼠标按下时进行界面索引更新
    QPoint Page1_LR_Pos;    //第1页左右移动坐标
    QPoint Page2_L_Pos;     //第2页往左移动坐标
    QPoint Page2_R_Pos;     //第2页往右移动坐标
    int press_x = 0;        //鼠标按下时的x轴位置
    int relea_x = 0;        //鼠标释放时的y轴位置
    int SlidePage;          //页滑动标志
    int CurrentPage;        //当前显示页

signals:
    void sendSIGNAL(enSysSignal _sys_signal);

};

#endif // TOOL_H
