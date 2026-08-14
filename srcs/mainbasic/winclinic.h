#ifndef WINCLINIC_H
#define WINCLINIC_H

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
#include "myeditline.h"
#include "tablemap.h"
#include "globaltypes.h"
#include "mpro-sys-communic.h"

using namespace std;

namespace Ui {
class WinClinic;
}

// 门诊档案界面
class WinClinic : public CBaseWidget        // TODO: “门诊”的英文术语不应该用 Clinic（诊所/门诊部）而应该用 Outpatient（门诊病人/门诊服务）？
{
    Q_OBJECT

public:
    explicit WinClinic(QWidget *parent = 0);
    ~WinClinic();

    static CPatient *getPatientByRow(int _row, QList<int> &_page_data, std::vector<CPatient> &_obj_list, QString *_msg = Q_NULLPTR);    // 获取表格行所对应的数据对象
    static void deleteFile(std::vector<CPatient> _pats);

    static bool checkUploadCondition(bool _is_batch);

    // 获取上传反馈信息
    static bool getUploadFeedbackMsg(const int _count_upload, const int _count_upload_final, const int _count_succ, const QString &_msg_upload_err, QString &_msg_tip);
    // 显示上传反馈信息
    static void showUploadFeedbackMsg(const int _count_upload, const int _count_upload_final, const int _count_succ, const QString &_msg);

    static void setConfig_SortType(enSortType _sort_type);         // 设置配置：排列方式

signals:
    void sendSIGNAL(enSysSignal _sys_signal);
    void stateChanged(int);
    void doubleClicked(QModelIndex);
    void clicked(bool);
    void sigUpLoadData(QVector<int> _ids);
    void batchPrintSig(std::vector<CPatient> pats);

public slots:
    void slotUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg);
    void slot_mproSysPushSvc_ReceivedOutpatientArchive(Net::Remote::stOutpatientArchive _archive);      // 云端的档案接收事件
    void slotPhysicButtonPressed();         // 槽函数：物理按键被点击

    void ref();

protected slots:
    void selectAll(int _state);
    void onRowCheckBoxStateChanged(int _state);
    void print_clicked();
    void export_clicked();
    void uploadSelectedRows();          // 上传已选行

protected:
    static const char * const S_CLASS_NAME;     // 本类的类名

    void showEvent(QShowEvent*);
    void hideEvent(QHideEvent *);

    MySQLitePatients *mysql = Q_NULLPTR;
    tableMap hisMap;
    QList<int> tableData;           // 当前页的数据对象列表（每一项的值是数据集的索引号）
    std::vector<CPatient> mLk;      // 数据集（包含所有分页的数据）       // TODO: 不应该一次从数据库载入所有数据到内存？
    QThread *work = Q_NULLPTR;
    QString strOutput;
    QMovie *uMovie = Q_NULLPTR;
    QLabel *upLoading = Q_NULLPTR;
    QVector<QString> patientidSelected;

    enSortType sortType = enSortType::ByTimeDsc;   // 排序方式（有效值与排序下拉选项的索引号一致，且与 MySQLitePatients 类的所有包含 _sort_type 参数的查询函数的定义一致）
    bool isEditing = false;     // 是否正在编辑

    void deleteSelection();
    void updateTheme();
    void GetPrintPara();
    int getDataSize();

    bool commandShow(QString cmd);
    int loadDataToTable();
    void showCurrentPageToTable();                  // 显示当前分页的数据到表格

    void measureRow(int _row);                      // 点击表格第 _row 行的 “测量” 按钮后的处理过程
    void editRow(int _row);                         // 点击表格第 _row 行的 “编辑” 按钮后的处理过程
    void showTableButtons(int _show_rows);          // 显示指定行数的表格按钮

    void languagechange();

    void doSearch();

    void setSortType(enSortType _sort_type, bool _need_reload = true);      // 设置排列方式

private slots:
    void on_btnBack_clicked();
    void on_tableWidget_clicked(const QModelIndex &_index);
    void on_btnPrevious_clicked();
    void on_btnNext_clicked();
    void on_pushButton_print_clicked();
    void on_pushButton_export_clicked();
    void on_pushButton_delete_clicked();
    void on_btnSearch_clicked();
    void on_edtSearch_textChanged(const QString &_arg1);
    void on_firstPageButton_clicked();
    void on_lastPageButton_clicked();
    void on_pushButton_upLoad_clicked();
    void on_Allselection_stateChanged(int arg1);
    void on_cmbSortType_currentIndexChanged(int _index);
    void on_btnMeasure_0_clicked();
    void on_btnMeasure_1_clicked();
    void on_btnMeasure_2_clicked();
    void on_btnMeasure_3_clicked();
    void on_btnMeasure_4_clicked();
    void on_btnEdit_0_clicked();
    void on_btnEdit_1_clicked();
    void on_btnEdit_2_clicked();
    void on_btnEdit_3_clicked();
    void on_btnEdit_4_clicked();
    void on_btnAdd_clicked();
private:
    Ui::WinClinic *ui;
};

#endif // WINCLINIC_H
