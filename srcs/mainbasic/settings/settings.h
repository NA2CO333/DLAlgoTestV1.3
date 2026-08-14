#ifndef SETTINGS_H
#define SETTINGS_H

#include <QWidget>
#include <QDialog>
#include <QSettings>
#include <QPushButton>
#include <QDebug>

#include "baseform.h"
#include "myeditline.h"
#include "statusbarform.h"
#include "globaltypes.h"
#include "globalclass.h"
#include "algo-invoker.h"
#include "data.h"

//
namespace Ui {
class settings;
}

// 年龄段
enum enAgeRange;

// “筛查超时”
enum enScreenTimeout;

// “无操作关机时间”
enum enShutdownNoOperation;

// "版本类型"
// 光路类型（对应 v1.3、1.4 旧代码的"版本类型"）
enum enOpticalPathType;

// 参考视力的显示类型（视力记录法）
enum enVisionNotation;

// 屏幕亮度（百分值）
enum enScreenBrightness;


// 业务数据：设置页面
class CBusiDataSetting : public CBusiData {
public:
    bool isEnableDistance;                          // 是否启用测距
    bool isSavePreviewImage;                        // 是否保存预览图（经过直方图均衡化处理，用于 A4 报表修饰）
    bool needLogin;                                 // 是否需要用户密码
    enAgeRange defaultAgeRange;                     // 默认年龄段
    CScreenTimeout screenTimeout;                   // 筛查超时
    CShutdownNoOperation shutdownSecs;              // 自动关机时间
    COpticalPathType opticalPathType;               // 版本类型（光路类型）
    CVisionNotation visionNotation;                 // 参考视力记录方法
    CScreenBrightness screenBrightness;             // 屏幕亮度（百分数的分子）
    enAutoScreenOff autoScreenOff;                  // 自动息屏
    enAlgoMode algoMode;                            // 【普通/专业】模式
    QString version;                                // 版本号
    bool isMultiMeasure;                            // 是否多次测量

    // 重置（还原）
    void reset() override;

    // 比较
    bool isEqualTo(const CBusiDataSetting &_busi_data) const;

};

// 【设置】业务视图
class settings : public CBaseWidget
{
    Q_OBJECT

public:
    explicit settings(QWidget *parent = 0);
    ~settings();

    //
    static int getScreenTimeoutSecs();                          // 获取【筛查超时】（秒）
    static int getPoweroffTimeSecs();                           // 获取【自动关机时间】（秒）

    static bool getCfg_IsEnableDistance();                      // 获取【是否允许测距】的配置值
    static void setCfg_IsEnableDistance(bool _is_enabled);      // 设置【是否允许测距】的配置值

    static enScreenTimeout getCfg_ScreenTimeout();              // 获取【筛查超时】的配置值
    static enShutdownNoOperation getCfg_PoweroffTime();         // 获取【自动关机时间】的配置值
    static enOpticalPathType getCfg_OpticalPathType();          // 获取【光路类型】的配置值

private slots:
    void on_pushButton_Save_clicked();
    void on_pushButton_Back_clicked();
    void on_pushButtonPassWord_clicked();
    void on_pushButton_Home_clicked();
    void on_pushButton_restore_clicked();
    void on_comboBox_Screen_Mode1_clicked(bool checked);
    void on_comboBox_Screen_Mode2_clicked(bool checked);

protected:
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    //void paintEvent(QPaintEvent *event);

    //
    void updateTheme(enThemeType _theme);                                   // 更新主题
    void updateLanguage();                                                  // 更新语言

    void configToBusiData(CBusiDataSetting &_busi_data);                    // 将配置文件里的配置设置到业务实体对象
    void saveBusiData(const CBusiDataSetting &_busi_data);                  // 保存业务数据

    void busiDataToUi(const CBusiDataSetting &_busi_data);                  // 从 数据对象 设置到 UI
    void uiToBusiData(CBusiDataSetting &_busi_data);                        // 从 UI 取值到 数据

    //QString checkValues(const CBusiDataDataTrans &_busi_data);              // 检查各个值是否合法

    void askAndSave(const CBusiDataSetting &_busi_data);                    // 询问用户是否需要保存，若需要则保存

    //
    CBusiDataSetting busiDataOrigin;            // 原始业务数据（与系统的当前值一致，但与 UI 不一定一致，因为用户可能修改了）

    QList<enAutoScreenOff> m_optionsAutoScreenOff;          // 【自动息屏】选项列表

private:
    Ui::settings *ui;
};

/// ==================================================================================

//
extern enOpticalPathType g_opticalPathType;    // 光路类型      // TODO: 清理这些全局变量

#endif // SETTINGS_H
