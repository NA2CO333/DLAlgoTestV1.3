#ifndef WINGUANXINTESTEEQUERY_H
#define WINGUANXINTESTEEQUERY_H

#include <QDialog>
#include <QVector>

#include "baseform.h"
#include "data-intf-guanxin.h"

namespace Ui {
class WinGuanXinTesteeQuery;
}

class WinGuanXinTesteeQuery : public CBaseDialog
{
    Q_OBJECT

public:
    explicit WinGuanXinTesteeQuery(QWidget *_parent = nullptr);
    ~WinGuanXinTesteeQuery();

    // 获取窗体当前数据
    void getUiData(Entity::ETesteeQueryRequest &_entity);

protected:
    void showEvent(QShowEvent *) override;

    void reloadAreaList(int _curr_idx);
    bool checkValues(QString &_err_msg);

private slots:
    void on_btnCancel_clicked();
    void on_btnOK_clicked();
    void on_btnEditArea_clicked();
    void on_btnAdd_clicked();
private:
    Ui::WinGuanXinTesteeQuery *ui;
};

#endif // WINGUANXINTESTEEQUERY_H
