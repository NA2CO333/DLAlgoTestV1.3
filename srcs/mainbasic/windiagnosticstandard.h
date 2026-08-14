#ifndef WINDIAGNOSTICSTANDARD_H
#define WINDIAGNOSTICSTANDARD_H

#include <QWidget>
#include <QSpinBox>

#include "baseform.h"
#include "data.h"
#include "algointf.h"
#include "doublespinbox.h"

namespace Ui {
class WinDiagnosticStandard;
}

// 默认配置值：散光随访阈值（下标 0~4 年龄段，与 enAgeRange 一致） /* 配置项个数变动的可能性比年龄段个数变动的可能性大得多，这个数据结构，配置项增加后，前后版本可兼容 */
constexpr double DEFAULT_ASTIGMATISM_FOLLOWUP[5]    = {+1.50, +1.25, +1.00, +1.00, +1.00};

// 默认配置值：散光诊治阈值
constexpr double DEFAULT_ASTIGMATISM_DIAGNOSE[5]    = {+2.25, +2.00, +2.00, +1.75, +1.75};

// 默认配置值：近视随访阈值
constexpr double DEFAULT_MYOPIA_FOLLOWUP[5]         = {+1.25, +0.75, +0.75, -0.75, -0.25};

// 默认配置值：近视诊治阈值
constexpr double DEFAULT_MYOPIA_DIAGNOSE[5]         = {+0.25, +0.00, -0.25, -1.00, -0.75};

// 默认配置值：远视随访阈值
constexpr double DEFAULT_HYPEROPIA_FOLLOWUP[5]      = {+2.50, +2.00, +2.00, +1.50, +1.25};

// 默认配置值：远视诊治阈值
constexpr double DEFAULT_HYPEROPIA_DIAGNOSE[5]      = {+3.50, +3.25, +3.00, +2.75, +1.75};

// 随访诊治标准（一个年龄段的）
struct stDiagnosticStandard {
    double astigmatismFollowUp;         // 散光随访阈值（0~4 年龄段，与 enAgeRange 一致）
    double astigmatismDiagnose;         // 散光诊治阈值
    double myopiaFollowUp;              // 近视随访阈值
    double myopiaDiagnose;              // 近视诊治阈值
    double hyperopiaFollowUp;           // 远视随访阈值
    double hyperopiaDiagnose;           // 远视诊治阈值

    // 设置为指定年龄段的默认值
    void setToDefault(enAgeRange _age_range)
    {
        if (_age_range >= ageRange_Min && _age_range <= ageRange_Max) {
            astigmatismFollowUp = DEFAULT_ASTIGMATISM_FOLLOWUP[_age_range];
            astigmatismDiagnose = DEFAULT_ASTIGMATISM_DIAGNOSE[_age_range];
            myopiaFollowUp      = DEFAULT_MYOPIA_FOLLOWUP[_age_range];
            myopiaDiagnose      = DEFAULT_MYOPIA_DIAGNOSE[_age_range];
            hyperopiaFollowUp   = DEFAULT_HYPEROPIA_FOLLOWUP[_age_range];
            hyperopiaDiagnose   = DEFAULT_HYPEROPIA_DIAGNOSE[_age_range];
        }
    }

};


// 业务数据
class CBusiDataDiagnostic : public CBusiData
{
public:
    stDiagnosticStandard diagnosticStandards[5];    // 5 个年龄段的随访诊治标准

    // 重置（还原）
    void reset() override;

    // 比较
    bool isEqualTo(const CBusiDataDiagnostic &_busi_data) const;

};

// “诊治随访标准设置”视图
class WinDiagnosticStandard : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinDiagnosticStandard(QWidget *parent = nullptr);
    ~WinDiagnosticStandard();

    //
    static QString getCfg_astigmatismFollowUp();        // 获得 散光随访阈值 的配置值（5个年龄段）
    static QString getCfg_astigmatismDiagnose();        // 获得 散光诊治阈值 的配置值（5个年龄段）
    static QString getCfg_myopiaFollowUp();             // 获得 近视随访阈值 的配置值（5个年龄段）
    static QString getCfg_myopiaDiagnose();             // 获得 近视诊治阈值 的配置值（5个年龄段）
    static QString getCfg_hyperopiaFollowUp();          // 获得 远视随访阈值 的配置值（5个年龄段）
    static QString getCfg_hyperopiaDiagnose();          // 获得 远视诊治阈值 的配置值（5个年龄段）

    //
    static bool getDiagnostic(const CPatient &_patient,
                              bool _has_right, double &_astigmatism_r, double &_myopia_r, double &_hyperopia_r,
                              bool _has_left, double &_astigmatism_l, double &_myopia_l, double &_hyperopia_l);

protected:
    void showEvent(QShowEvent *) override;

    static CBusiDataDiagnostic busiDataOrigin;                 // 原始业务数据

    stDiagnosticStandard tabWidgetData[5];              // tabWidget 的数据（换页时将当前页数据设置到编辑框）
    int currentTabIndex;

    //
    void updateTheme(enThemeType _theme);                                   // 更新主题

    void configToBusiData(CBusiDataDiagnostic &_busi_data);                 // 将配置文件里的配置设置到业务实体对象
    void saveBusiData(const CBusiDataDiagnostic &_busi_data);               // 保存业务数据

    void busiDataToUi(const CBusiDataDiagnostic &_busi_data);               // 将 数据 设置到 UI
    void uiToBusiData(CBusiDataDiagnostic &_busi_data);                     // 从 UI 取值到 数据

    //QString checkValues(const CBusiDataDataTrans &_busi_data);              // 检查各个值是否合法

    void askAndSave(const CBusiDataDiagnostic &_busi_data);                 // 询问用户是否需要保存，若需要则保存

    //
    void tabWidgetDataToEdits(const int _idx_tab);
    void editsToTabWidgetData(const int _idx_tab);

    void setSpinBoxValue(CDoubleSpinBox *_spin_box, double _value);     // 设置 SpinBox 的值（支持显示"+"号）

    void updateLanguage();
protected slots:
    void slot_CDoubleSpinBox_valueChanged(double _value);
    void slot_CDoubleSpinBox_ReachedExtremeValue(int _extreme_type);

private slots:
    void on_btnHome_clicked();
    void on_btnSave_clicked();
    void on_btnBack_clicked();
    void on_tabWidget_currentChanged(int index);
    void on_btnRestore_clicked();

private:
    Ui::WinDiagnosticStandard *ui;
};

#endif // WINDIAGNOSTICSTANDARD_H
