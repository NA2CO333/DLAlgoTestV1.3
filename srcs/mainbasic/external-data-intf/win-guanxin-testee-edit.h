#ifndef WINGUANXINTESTEEEDIT_H
#define WINGUANXINTESTEEEDIT_H

#include <QDialog>

#include "baseform.h"
#include "data-intf-guanxin.h"

namespace Ui {
class WinGuanXinTesteeEdit;
}

class WinGuanXinTesteeEdit : public CBaseDialog
{
    Q_OBJECT

public:
    explicit WinGuanXinTesteeEdit(QWidget *_parent = nullptr);
    ~WinGuanXinTesteeEdit();

    // 工作模式
    enum class enWorkMode {
        edit,       // 编辑
        add,        // 新增（不显示列表框和操作按钮组）
    };

    //
    void setData(QVector<stAreaInfo> &_area_list, int _idx);        // NOTE: “编辑”模式时调用
    void setData();                                                 // NOTE: “新增”模式时调用

    bool isChanged() { return m_isChanged; }

    int currentIndex();                 // 当前选项
    QString currentName();
    QString currentCode();

protected:
    void showEvent(QShowEvent *) override;

    void reloadAreaList(int _curr_idx);
    void showAreaInfoByIndex(int _idx);

    bool checkDataValid(QString &_err_msg);

    QVector<stAreaInfo> *m_areaList {nullptr};
    int m_idx {-1};

    bool m_isChanged {false};
    enWorkMode m_workMode;

private slots:
    void on_cbbAreaCode_activated(int _index);
    void on_edtCode_textEdited(const QString &_arg1);
    void on_edtName_textEdited(const QString &_arg1);
    void on_btnDel_clicked();
    void on_btnOK_clicked();
    void on_btnCancel_clicked();
private:
    Ui::WinGuanXinTesteeEdit *ui;
};

#endif // WINGUANXINTESTEEEDIT_H
