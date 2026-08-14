#ifndef WINPERSONALRECORD_H
#define WINPERSONALRECORD_H

#include <QModelIndex>
#include <QTableWidgetItem>
#include <QString>
#include <QPalette>
#include <QThread>
#include <QLabel>

#include "baseform.h"
#include "mysqlitepatients.h"
#include "threadmodel.h"
#include "statusbarform.h"
#include "tablemap.h"
#include "globaltypes.h"

using namespace std;

namespace Ui {
class WinPersonalRecord;
}

// 门诊档案界面
class WinPersonalRecord : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinPersonalRecord(QWidget *parent = 0);
    ~WinPersonalRecord();

    void updateTheme();
    void GetPrintPara();

    void setPatient(const CPatient &_pat);

    void reloadCurrentRecord();             // 重新从数据库载入当前记录

    static void setConfig_SortType(enSortType _sort_type);         // 设置配置：排列方式

signals:
    void stateChanged(int);
    void doubleClicked(QModelIndex);
    void clicked(bool);
    void sigUpLoadData(QVector<int> _ids);
    void batchPrintSig(std::vector<CPatient> pats);

public slots:
    void slotUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg);
    void slotPhysicButtonPressed();         // 槽函数：物理按键被点击

    void ref();

protected slots:
    void onRowCheckBoxStateChanged(int _state);

protected:
    void showEvent(QShowEvent*);
    void hideEvent(QHideEvent *);

    MySQLitePatients *mysql = Q_NULLPTR;
    tableMap hisMap;
    QList<int> tableData;
    std::vector<CPatient> mLk;      // 数据集（包含所有分页的数据）       // TODO: 不应该一次从数据库载入所有数据到内存？
    //QPalette pal;
    //ThreadModel *model;
    QThread *work = Q_NULLPTR;
    QString strOutput;
    QMovie *uMovie = Q_NULLPTR;
    QLabel *upLoading = Q_NULLPTR;
    QVector<int> idSelected;
    QIcon iconUpload, iconUploaded;

    CPatient patient;

    enSortType sortType = enSortType::ByTimeAsc;    // 排序方式，有效值与排序下拉选择框一致（0 创建时间升序，1 创建时间降序，2 编号升序，3 编号降序）

    void selectAll(int _state);
    void print_clicked();
    void uploadSelectedRows();          // 上传已选行

    bool commandShow(QString cmd);
    void loadDataToTable();
    void showCurrentPageToTable();              // 显示当前分页的数据到表格
    QPushButton *getUploadButton(int _row);

    void viewRecord(int _row);                      // 点击表格第 _row 行的 “查看” 按钮后的处理过程
    void deleteRow(int _row);                       // 点击表格第 _row 行的 “删除” 按钮后的处理过程
    void uploadRow(int _row);                       // 点击表格第 _row 行的 “上传” 按钮后的处理过程
    void showTableButtons(int _show_rows);          // 显示指定行数的表格按钮
    void clearUploadButtons();

    void languagechange();

    void setSortType(enSortType _sort_type, bool _need_reload = true);      // 设置排列方式

private slots:
    void on_pushButton_Back_clicked();
    void on_tableWidget_clicked(const QModelIndex &_index);
    void on_btnPrevious_clicked();
    void on_btnNext_clicked();
    void on_btnPrintTicket_clicked();
    void on_btnPrintA4_clicked();
    void on_pushButton_upLoad_clicked();
    void on_Allselection_stateChanged(int arg1);
    void on_cmbSortType_currentIndexChanged(int _index);

    void on_btnView_0_clicked();
    void on_btnView_1_clicked();
    void on_btnView_2_clicked();
    void on_btnView_3_clicked();
    void on_btnView_4_clicked();

    void on_btnDelete_0_clicked();
    void on_btnDelete_1_clicked();
    void on_btnDelete_2_clicked();
    void on_btnDelete_3_clicked();
    void on_btnDelete_4_clicked();

    void on_btnUpload_0_clicked();
    void on_btnUpload_1_clicked();
    void on_btnUpload_2_clicked();
    void on_btnUpload_3_clicked();
    void on_btnUpload_4_clicked();

private:
    Ui::WinPersonalRecord *ui;
};

#endif // WINPERSONALRECORD_H
