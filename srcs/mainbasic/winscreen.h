#ifndef WINSCREEN_H
#define WINSCREEN_H

#include <QModelIndex>
#include <QTableWidgetItem>
#include <QString>
#include <QPalette>
#include <QLabel>

#include "baseform.h"
#include "mysqlitepatients.h"
#include "import.h"
#include "statusbarform.h"
#include "myeditline.h"
#include "tablemap.h"
#include "globaltypes.h"
#include "mpro-sys-communic.h"

namespace Ui {
class WinScreen;
}

// 筛查档案界面
class WinScreen : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinScreen(QWidget *parent = 0);
    ~WinScreen();

    void showLoading(bool);
    void updateTheme();
    void GetPrintPara();

    bool getNextNotMeasured(int _curr_id, CPatient &_pat, QString &_err_msg);   // 获取批量筛查名单中的下一个未测量的被测者

    static void setConfig_SortType(enSortType _sort_type);         // 设置配置：排列方式

signals:
    void sendSIGNAL(enSysSignal _sys_signal);
    void stateChanged(int);
    void doubleClicked(QModelIndex);
    void clicked(bool);
    void batchUpLoad(std::vector<CPatient>);
    void sigUpLoadData(QVector<int> _ids);
    void batchImportSig();
    void batchPrintSig(std::vector<CPatient> pats);

public slots:
    void slotUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg);
    void slot_mproSysPushSvc_ReceivedOutpatientArchive(Net::Remote::stOutpatientArchive _archive);      // 云端的档案接收事件
    void slot_batchImportFeedback(QString log);
    void slotShowWarningMsg(QString _msg);

    void slotRefresh();
    void slotEnableViewObject(bool enable);     // 设置各个控件的可用状态

    void slotPhysicButtonPressed();         // 槽函数：物理按键被点击

protected Q_SLOTS:
    void selectAll(int _state);
    void onRowCheckBoxStateChanged(int _state);
    void print_clicked();
    void port_clicked();
    void delete_clicked();
    void uploadSelectedRows();                  // 上传已选行
    void barcodeHandle();

protected:
    static const char * const S_CLASS_NAME;     // 本类的类名

    void keyPressEvent(QKeyEvent *event);
    void showEvent(QShowEvent*);
    void hideEvent(QHideEvent*);

    MySQLitePatients *mysql = Q_NULLPTR;
    std::vector<CPatient> mLk;      // 数据集（包含所有分页的数据）       // TODO: 不应该一次从数据库载入所有数据到内存？
    QTimer enterToSearch;
    QString strOutput;
    tableMap batchMap;
    QList<int> tableData;
    QString barcodeData;
    bool barcodeMode;
    QTimer readBarcode;
    QLabel *loading;
    QMovie *mMovie;
    QVector<int> idSelected;
    QIcon iconUpload, iconUploaded;

    enSortType sortType = enSortType::ByTimeDsc;    // 排序方式（有效值与排序下拉选项的索引号一致，且与 MySQLitePatients 类的所有包含 _sort_type 参数的查询函数的定义一致）

    bool commandShow(QString cmd);
    void messagedelete();
    void showMessage();
    int loadDataToTable();
    QPushButton *getUploadButton(int _row);
    void showCurrentPageToTable();          // 显示当前分页的数据到表格
    void deleteFile();

    void uploadRow(int _row);                       // 点击表格第 _row 行的 “上传” 按钮后的处理过程
    void showTableButtons(int _show_rows);          // 显示指定行数的表格按钮
    void clearUploadButtons();

    void languagechange();

    void doSearch();

    void setSortType(enSortType _sort_type, bool _need_reload = true);      // 设置排列方式

private slots:
    void on_pushButton_Back_clicked();
    void on_btnPrevious_clicked();
    void on_btnNext_clicked();
    void on_tableWidget_clicked(const QModelIndex &_index);
    void on_pushButton_print_clicked();
    void on_pushButton_port_clicked();
    void on_pushButton_delete_clicked();
    void on_btnSearch_clicked();
    void on_edtSearch_textChanged(const QString &_arg1);
    void on_firstPageButton_clicked();
    void on_lastPageButton_clicked();
    void on_pushButton_upLoad_clicked();
    void on_btnBatchImport_clicked();
    void on_Allselection_stateChanged(int arg1);
    void on_cmbSortType_currentIndexChanged(int _index);
    void on_btnUpload_0_clicked();
    void on_btnUpload_1_clicked();
    void on_btnUpload_2_clicked();
    void on_btnUpload_3_clicked();
    void on_btnUpload_4_clicked();
private:
    Ui::WinScreen *ui;
};

/// ==========================================================
/// 提供给外部调用所的变量



#endif // WINSCREEN_H
