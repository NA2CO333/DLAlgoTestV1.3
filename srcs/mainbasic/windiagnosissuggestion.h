#ifndef WINDIAGNOSISSUGGESTION_H
#define WINDIAGNOSISSUGGESTION_H

#include <QWidget>

#include "baseform.h"
#include "data.h"

namespace Ui {
class WinDiagnosisSuggestion;
}

// 业务数据
class CBusiDataSuggestion : public CBusiData
{
public:
    QString suggestion;

    // 重置（还原）
    void reset() override;

    // 比较
    bool isEqualTo(const CBusiDataSuggestion &_busi_data) const;

    // 拷贝
    void copyFrom(CBusiDataSuggestion &_busi_data);

};

//
class WinDiagnosisSuggestion : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinDiagnosisSuggestion(QWidget *parent = nullptr);
    ~WinDiagnosisSuggestion();

protected:
    void showEvent(QShowEvent *) override;

    CBusiDataSuggestion busiDataOrigin;                     // 原始业务数据

    //
    void updateTheme(enThemeType _theme);                                   // 更新主题
    void updateLanguage();                                                  // 更新语言

    void configToBusiData(CBusiDataSuggestion &_busi_data);                 // 将配置文件里的配置设置到业务实体对象
    void saveBusiData(CBusiDataSuggestion &_busi_data);                     // 保存业务数据

    void busiDataToUi(const CBusiDataSuggestion &_busi_data);               // 将 数据 设置到 UI
    void uiToBusiData(CBusiDataSuggestion &_busi_data);                     // 从 UI 取值到 数据

    //QString checkValues(const CBusiDataDataTrans &_busi_data);              // 检查各个值是否合法

    void askAndSave(CBusiDataSuggestion &_busi_data);                       // 询问用户是否需要保存，若需要则保存

private slots:
    void on_btnHome_clicked();
    void on_btnSave_clicked();
    void on_btnBack_clicked();

    void on_btnLoad_clicked();

private:
    Ui::WinDiagnosisSuggestion *ui;
};

#endif // WINDIAGNOSISSUGGESTION_H
