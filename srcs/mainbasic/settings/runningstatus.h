#ifndef RUNNINGSTATUS_H
#define RUNNINGSTATUS_H

#include <QWidget>
#include <QObject>
#include <QMutex>
#include <QLabel>
#include <QProcess>
#include <QTableWidget>
#include <QStringListModel>
#include <QDate>

#include "baseform.h"
#include "statusbarform.h"
#include "mysqlitepatients.h"

namespace Ui {
class RunningStatus;
}

// 系统存储、CPU 占用率等状态的显示i
class RunningStatus : public CBaseWidget
{
    Q_OBJECT

public:
    static RunningStatus *getInstance(QWidget *_parent = 0);
    ~RunningStatus();

    static int getCurrCpuRate();                            // 得到 当前CPU使用率（百分数）
    static int getCurrMemRate();                            // 得到 当前内存使用率（百分数）
    static bool getIsStorageFull();                         // 得到 是否存储空间已满  /* 注意：这里得到的是最后一次读取得而得的值，而不是实时值 */   // TODO: 和 WindowsManagers::isStorageSpaceEnough() 合并？

    static void refreshStorageRate();                       // 刷新存储空间使用率

    void startTimerRefreshCpuRate(int _interval, bool _is_immediately = false);         // 开始定时刷新CPU使用率（interval <= 0 表示停止）
    void startTimerRefreshMemRate(int _interval, bool _is_immediately = false);         // 开始定时刷新内存使用率（interval <= 0 表示停止）
    void startTimerRefreshStorageRate(int _interval, bool _is_immediately = false);     // 开始定时刷新存储使用率（interval <= 0 表示停止）

    void stopAllTimerRefresh();                             // 停止所有的定时刷新

signals:
    void sigRefreshStorageRate();
    void sigCleanStorage(QDate _date_earliest_keep);

private slots:
    void on_pushButton_Back_clicked();
    void on_pushButton_Home_clicked();
    void on_btnCleanStorage_clicked();
    void on_lvCleanPeriod_clicked(const QModelIndex &_index);

    void slot_timerCPU_timeout();
    void slot_timerMemory_timeout();
    void slot_timerStorage_timeout();

    void slot_RefreshStorageRate();
    void slotCleanStorage(QDate _date_earliest_keep);

protected:
    explicit RunningStatus(QWidget *parent = 0);

    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

    //
    static RunningStatus *instance;

    // 清理周期
    enum enCleanPeriod {
        cleanPeriod_oneWeekAgo      = 0,                            // 一个星期之前的
        cleanPeriod_twoWeeksAgo     ,                               // 两个星期之前的
        cleanPeriod_oneMonthAgo     ,                               // 一个月之前的
        cleanPeriod_threeMonthsAgo  ,                               // 三个月之前的
        cleanPeriod_halfYearAgo     ,                               // 半年之前的
        cleanPeriod_oneYearAgo      ,                               // 一年之前的
        cleanPeriod_all             ,                               // 全部
    };

    //
    static int currCpuRate;         // 上次读取得到的 CPU 使用率
    static int currMemRate;         // 上次读取得到的 内存 使用率
    static bool isStorageFull;

    //
    QTimer *timerCPU;       //定时器获取CPU使用率信息
    QTimer *timerMemory;    //定时器获取内存使用率信息
    QTimer *timerStorage;   //定时器获取存储使用率信息

    //
    QStringListModel *modelCleanPeriods = Q_NULLPTR;

    //
    void emitRefreshStorageRate();

    //
    void readCpuRate();                 // 读取 CPU 使用率
    void readMemRate();                 // 读取 内存 使用率
    void readStorageRate();             // 读取 存储 使用率

    //
    void updateUi_CpuMemInfo(const int *_cpu_percent, const int *_mem_percent, const int *_mem_used, const int *_mem_all);

    //
    void insertRowToTableWidget(int _row, QString _dev_name, QString _used, QString _free, QString _total, int _percent);   // 插入行到表格
    void warnStorageIsTooLow();         // 警告存储空间过低

    bool cleanAllStorage(QString &_err_msg);                                                    // 清理所有的用户存储数据
    void cleanStorage(const QDate &_date_earliest_keep);                                        // 清理 指定日期之前（包含该日期）的 用户存储数据
    int cleanFilesBeforeDate(const QString &_dir_path, const QDate &_date_earliest_keep, bool _remove_if_empty);    // 清理指定目录的 指定日期之前（包含该日期）的 文件

private:
    Ui::RunningStatus *ui;

};

#endif // RUNNINGSTATUS_H
