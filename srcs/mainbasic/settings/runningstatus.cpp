#include "runningstatus.h"
#include "ui_runningstatus.h"

#include <QProcess>
#include <QTableWidget>
#include <QFileInfo>
#include <QHeaderView>
#include <QDir>
#include <QProgressBar>
#include <QTimer>
#include <QDebug>
#include <QSqlQuery>
#include <QFileInfoList>
#include <QSqlError>

#include "windowsmanager.h"
#include "global.h"
#include "global_intf.h"

//
#define GB (1024 * 1024 * 1024)
#define MB (1024 * 1024)
#define KB (1024)

// 最大存储空间使用率（百分数）
#define MAX_STORAGE_USED 95

//
static constexpr int INTERVAL_CPU_FAST      = 1000;         // CPU 使用率快速刷新的间隔（本窗体可见时，用较快的频率刷新）
static constexpr int INTERVAL_MEM_FAST      = 3000;         // 内存 使用率快速刷新的间隔
static constexpr int INTERVAL_STORAGE_FAST  = 5000;         // 存储 使用率快速刷新的间隔
//static constexpr int INTERVAL_STORAGE_SLOW  = 60000;        // 存储 使用率慢速刷新的间隔（本窗体不可见时，用较慢的频率刷新）

static const QDate DATE_CLEAN_ALL = QDate(1, 1, 1);         // 清除所有数据时的日期标志

//
RunningStatus *RunningStatus::instance = nullptr;

int RunningStatus::currCpuRate = 0;
int RunningStatus::currMemRate = 0;

bool RunningStatus::isStorageFull = false;

//
RunningStatus *RunningStatus::getInstance(QWidget *_parent)
{
    if (!instance) {
        instance = new RunningStatus(_parent);
    }
    return instance;
}

//
RunningStatus::RunningStatus(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::RunningStatus)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    // 设置列数和列宽
    ui->tableWidget->clear();
    ui->tableWidget->setColumnCount(5);
    ui->tableWidget->setColumnWidth(0, 120);
    ui->tableWidget->setColumnWidth(1, 90);
    ui->tableWidget->setColumnWidth(2, 90);
    ui->tableWidget->setColumnWidth(3, 90);
    ui->tableWidget->setColumnWidth(4, 200);
    ui->tableWidget->setStyleSheet("QTableWidget::item{padding:0px;}");
    ui->tableWidget->horizontalHeader()->setSectionsClickable(false);           //水平方向的头不可点击
    ui->tableWidget->verticalHeader()->setSectionsClickable(false);             //垂直方向的头不可点击
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);       //设置选择行为，以行为单位
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);        //设置编辑模式,不可编辑
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);      //设置选择模式，选择单行
    ui->tableWidget->verticalHeader()->setVisible(true);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

    //
    timerCPU = new QTimer(this);
    QObject::connect(timerCPU, &QTimer::timeout, this, &RunningStatus::slot_timerCPU_timeout);

    timerMemory = new QTimer(this);
    QObject::connect(timerMemory, &QTimer::timeout, this, &RunningStatus::slot_timerMemory_timeout);

    timerStorage = new QTimer(this);
    QObject::connect(timerStorage, &QTimer::timeout, this, &RunningStatus::slot_timerStorage_timeout);

    //
    QObject::connect(this, &RunningStatus::sigRefreshStorageRate, this, &RunningStatus::slot_RefreshStorageRate, Qt::QueuedConnection);
    QObject::connect(this, &RunningStatus::sigCleanStorage, this, &RunningStatus::slotCleanStorage, Qt::QueuedConnection);

    //
    modelCleanPeriods = new QStringListModel(ui->lvCleanPeriod);
    ui->lvCleanPeriod->setModel(modelCleanPeriods);
    ui->lvCleanPeriod->setResizeMode(QListView::Adjust);
    ui->lvCleanPeriod->setFrameStyle(QFrame::StyledPanel);

}

RunningStatus::~RunningStatus()
{
    delete ui;
}

void RunningStatus::showEvent(QShowEvent *)
{
    //
    //QPalette palette;
    if(themeType_Black == getSysThemeType()){
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));

        ui->labCPUMemory->setStyleSheet("QLabel{border-radius:5px;color:rgb(204,204,204);}");
        ui->tableWidget->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(50,50,50); color:rgb(204,204,204);}");
        ui->tableWidget->verticalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(50,50,50); color:rgb(204,204,204);}");
        ui->tableWidget->setStyleSheet("QTableWidget{background-color:rgb(20,20,20); color:rgb(255,255,255);} QTableCornerButton::section{background-color:rgb(170,170,170);}");
        ui->label_vertical->show();
        ui->label_vertical->setStyleSheet("background-color:rgb(20,20,20,100);");
        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
        ui->label_Home->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");
        ui->btnCleanStorage->setStyleSheet("QPushButton {background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px;}");
    }
    else{
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        ui->labCPUMemory->setStyleSheet("QLabel{border-radius:5px;color:rgb(1,1,1);}");
        ui->tableWidget->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
        ui->tableWidget->verticalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(225,225,230); color:rgb(50,50,50);} ");
        ui->tableWidget->setStyleSheet("QTableWidget{background-color:rgb(242,242,247); color:rgb(50,50,50);} QTableCornerButton::section{background-color:rgb(230,230,230);}");
        ui->label_vertical->hide();
        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");
        ui->btnCleanStorage->setStyleSheet("");
    }
    //this->setPalette(palette);

    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("系统状态"));    // "System State"

    //
    QStringList table_header;
    table_header << tr("盘符") << tr("已用空间") << tr("可用空间") << tr("总大小") << tr("已用百分比");   // << "Drive" << "Used" << "Usable" << "Total" << "Percentage used"
    ui->tableWidget->setHorizontalHeaderLabels(table_header);

    QStringList list_clean_periods;
    list_clean_periods << tr("一个星期之前的") << tr("两个星期之前的") << tr("一个月之前的") << tr("三个月之前的") << tr("半年之前的") << tr("一年之前的") << tr("全部");
    //  << "one week ago" << "two weeks ago" << "one month ago" << "three months ago" << "half year ago" << "one year ago" << "all";
    for (int i = 0; i < list_clean_periods.count(); i++) {
        list_clean_periods[i] = tr("清理 【%1】 历史数据").arg(list_clean_periods.at(i));  // "clean data of [%1]"
    }
    modelCleanPeriods->setStringList(list_clean_periods);

    //
    //if (language) {
    //    ui->label_Home->setText("主页");
    //    ui->label_Back->setText("返回");
    //    ui->btnCleanStorage->setText("清理存储");
    //} else {
    //    ui->label_Home->setText("Home");
    //    ui->label_Back->setText("Back");
    //    ui->btnCleanStorage->setText("CleanStorage");
    //}

    // 开始定时刷新：CPU、内存、存储使用率
    startTimerRefreshCpuRate(INTERVAL_CPU_FAST, true);
    startTimerRefreshMemRate(INTERVAL_MEM_FAST, true);
    startTimerRefreshStorageRate(INTERVAL_STORAGE_FAST, true);

}

void RunningStatus::hideEvent(QHideEvent *)
{
    // 停止定时刷新 CPU、内存 使用率
    startTimerRefreshCpuRate(0);
    startTimerRefreshMemRate(0);

    // 停止定时刷新 存储 使用率
    startTimerRefreshStorageRate(0);

    // 存储使用率的刷新改为慢速     /* 没有必要慢速定时刷新，改为在结果页面保存数据后通过信号槽触发刷新一次即可 */
    //startTimerRefreshStorageRate(INTERVAL_STORAGE_SLOW);

}

void RunningStatus::on_pushButton_Back_clicked()
{
    //
    getWinManage()->showWindowByType(WIN_TOOL);
}

void RunningStatus::on_pushButton_Home_clicked()
{
    //
    getWinManage()->showWindowByType(WIN_HOME);
}

void RunningStatus::on_btnCleanStorage_clicked()
{
    //
    ui->winCleanPeriod->setVisible(true);

    //
    ui->lvCleanPeriod->clearSelection();

    int border_width = ui->lvCleanPeriod->frameWidth();     // QFrame::StyledPanel 的边框线的宽度
    int width = ui->lvCleanPeriod->sizeHintForColumn(0) + border_width * 2;
    int height = ui->lvCleanPeriod->sizeHintForRow(0) * modelCleanPeriods->rowCount() + border_width * 2;
    ui->lvCleanPeriod->setGeometry(ui->btnCleanStorage->x() + ui->btnCleanStorage->width() - width, ui->btnCleanStorage->y(), width, height);

}

int RunningStatus::getCurrCpuRate()
{
    return RunningStatus::currCpuRate;
}

int RunningStatus::getCurrMemRate()
{
    return RunningStatus::currMemRate;
}

bool RunningStatus::getIsStorageFull()
{
    return RunningStatus::isStorageFull;
}

void RunningStatus::startTimerRefreshCpuRate(int _interval, bool _is_immediately)
{
    if (_interval > 0) {
        timerCPU->start(_interval);

        if (_is_immediately) {
            readCpuRate();
        }
    } else {
        timerCPU->stop();
    }
}

void RunningStatus::startTimerRefreshMemRate(int _interval, bool _is_immediately)
{
    if (_interval > 0) {
        timerMemory->start(_interval);

        if (_is_immediately) {
            readMemRate();
        }
    } else {
        timerMemory->stop();
    }
}

void RunningStatus::startTimerRefreshStorageRate(int _interval, bool _is_immediately)
{
    if (_interval > 0) {
        timerStorage->start(_interval);

        if (_is_immediately) {
            readStorageRate();
        }
    } else {
        timerStorage->stop();
    }
}

void RunningStatus::stopAllTimerRefresh()
{
    startTimerRefreshCpuRate(0);
    startTimerRefreshMemRate(0);
    startTimerRefreshStorageRate(0);
}

void RunningStatus::refreshStorageRate()
{
    RunningStatus::getInstance()->emitRefreshStorageRate();
}

// 获取 CPU 运行状态
void RunningStatus::slot_timerCPU_timeout()
{
    readCpuRate();
}

// 获取 内存 使用状态
void RunningStatus::slot_timerMemory_timeout()
{
    readMemRate();
}

// 获取 存储 使用状态
void RunningStatus::slot_timerStorage_timeout()
{
    readStorageRate();
}

void RunningStatus::emitRefreshStorageRate()
{
    emit sigRefreshStorageRate();
}

void RunningStatus::slot_RefreshStorageRate()
{
    if (timerStorage->isActive()) {
        return;
    }

    //
    readStorageRate();
}

void RunningStatus::updateUi_CpuMemInfo(const int *_cpu_percent, const int *_mem_percent, const int *_mem_used, const int *_mem_all)
{
    static int cpu_percent  = 0;
    static int mem_percent  = 0;
    static int mem_all      = 0;
    static int mem_used     = 0;

    //
    if (_cpu_percent)   cpu_percent = *_cpu_percent;
    if (_mem_percent)   mem_percent = *_mem_percent;
    if (_mem_used)      mem_used    = *_mem_used;
    if (_mem_all)       mem_all     = *_mem_all;

    //
    QString info_str = tr("CPU : %1%  内存 : %2% ( 已用 %3 MB / 共 %4 MB )").arg(cpu_percent).arg(mem_percent).arg(mem_used).arg(mem_all);   // "CPU : %1%  Memory : %2% ( Used %3 MB / All %4 MB )"
    ui->labCPUMemory->setText(info_str);

}

void RunningStatus::readCpuRate()
{
    //
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.start("cat /proc/stat");                    // 查询 CPU 使用状态
    process.waitForFinished(5000);

    //
    static int total_old = 0, idle_old = 0;     // TODO: 这是？

    int cpu_percent = 0;
    int total_new = 0, idle_new = 0;

    //
    while(!process.atEnd()){
        QString str_line = QString::fromUtf8(process.readLine());
        if (str_line.startsWith("cpu "))
        {
            QStringList list = str_line.split(" ");
            idle_new = list.at(5).toInt();
            foreach(QString value, list) {
                total_new += value.toInt();
            }

            int total = total_new - total_old;
            int idle = idle_new - idle_old;
            total_old = total_new;
            idle_old = idle_new;

            //
            cpu_percent = 100 * (total - idle) / total;
            RunningStatus::currCpuRate = cpu_percent;

            //
            break;
        }
    }

    //
    updateUi_CpuMemInfo(&cpu_percent, nullptr, nullptr, nullptr);

}

void RunningStatus::readMemRate()
{
    //
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.start("cat /proc/meminfo");                     // 查询内存信息
    process.waitForFinished(5000);

    //
    int mem_percent = 0;
    int mem_all = 0;
    int mem_used = 0;

    int memory_free = 0;

    //
    while(!process.atEnd()){
        QString str_line = QString::fromUtf8(process.readLine());
        if (str_line.startsWith("MemTotal"))
        {
            str_line = str_line.replace(" ", "");
            str_line = str_line.split(":").at(1);
            mem_all = str_line.left(str_line.length() - 3).toInt() / KB;
        }
        else if (str_line.startsWith("MemFree"))
        {
            str_line = str_line.replace(" ", "");
            str_line = str_line.split(":").at(1);
            memory_free = str_line.left(str_line.length() - 3).toInt() / KB;
        }
        else if (str_line.startsWith("Buffers"))
        {
            str_line = str_line.replace(" ", "");
            str_line = str_line.split(":").at(1);
            memory_free += str_line.left(str_line.length() - 3).toInt() / KB;
        }
        else if (str_line.startsWith("Cached"))
        {
            str_line = str_line.replace(" ", "");
            str_line = str_line.split(":").at(1);
            memory_free += str_line.left(str_line.length() - 3).toInt() / KB;

            //
            mem_used = mem_all - memory_free;           // TODO: 这个算得好像有点偏差？
            mem_percent = 100 * mem_used / mem_all;
            RunningStatus::currMemRate = mem_percent;

            //
            break;
        }
    }

    //
    updateUi_CpuMemInfo(nullptr, &mem_percent, &mem_used, &mem_all);

}

void RunningStatus::readStorageRate()
{
    static bool is_storage_not_enough_warned = false;       // 是否已警告空间不足（开机后只提示一次）

    //
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.start("df -h");                                 // 查询磁盘使用信息
    process.waitForFinished(5000);

    // 清空表格中的原有数据
    ui->tableWidget->clearContents();       /* clearContents() 只是清除数据，并未清除行 */

    //
    int row = 0;
    while (!process.atEnd()) {
        QString str_line = QString::fromUtf8(process.readLine());

        //
        QString dev_name;
        if (str_line.startsWith("/dev/root")) {
            dev_name = tr("本地磁盘");  // "Local disk"
        //} else if (result.startsWith("/dev/mmcblk1p1")) {
        //    name = (language ? "本地磁盘(分区1)" : );
        } else if (str_line.startsWith("/dev/sda") || str_line.startsWith("/dev/sdb")) {
            dev_name = tr("可移动磁盘"); // "Mobile disk"
        }

        // 跳过未识别的行
        if (dev_name.length() == 0) {
            continue;
        }

        // 去掉多余的空格
        int len_old = -1, len_new = -1;
        do {
            len_old = str_line.length();
            len_new = str_line.replace("  ", " ").length();
        } while (len_new >= 0 && len_new != len_old);

        //
        QString total, used, free;
        int percent = 0;
        QStringList list = str_line.split(" ");

        total = list[1];
        used = list[2];
        free = list[3];
        percent = list[4].left(list[4].length() - 1).toInt();
        //dev_name += "(" + list[5].trimmed() + ")";

        // 插入行到表格
        insertRowToTableWidget(row, dev_name, used, free, total, percent);
        row++;

        // 低存储空间警告
        if (((dev_name == "本地磁盘") || (dev_name == "Local disk"))) {
            if (percent > MAX_STORAGE_USED) {
                isStorageFull = true;

                if (!is_storage_not_enough_warned) {
                    is_storage_not_enough_warned = true;
                    warnStorageIsTooLow();
                }
            } else {
                isStorageFull = false;
            }
        }

    }

}

void RunningStatus::insertRowToTableWidget(int _row, QString _dev_name, QString _used, QString _free, QString _total, int _percent)
{
    //
    int row_count = ui->tableWidget->rowCount();
    if (_row > row_count - 1) {
        for (int i = row_count; i <= _row; i++) {
            ui->tableWidget->insertRow(i);
        }
    }

    //
    ui->tableWidget->setRowHeight(_row, 40);

    QTableWidgetItem *itemname = new QTableWidgetItem(_dev_name);
    QTableWidgetItem *itemuse = new QTableWidgetItem(_used);
    itemuse->setTextAlignment(Qt::AlignCenter);
    QTableWidgetItem *itemfree = new QTableWidgetItem(_free);
    itemfree->setTextAlignment(Qt::AlignCenter);
    QTableWidgetItem *itemall = new QTableWidgetItem(_total);
    itemall->setTextAlignment(Qt::AlignCenter);

    ui->tableWidget->setItem(_row, 0, itemname);
    ui->tableWidget->setItem(_row, 1, itemuse);
    ui->tableWidget->setItem(_row, 2, itemfree);
    ui->tableWidget->setItem(_row, 3, itemall);

    QProgressBar *bar = new QProgressBar;
    bar->setRange(0, 100);
    bar->setValue(_percent);

    QString qss;
    if(_percent < 50){
        qss = "QProgressBar{text-align:center;border-width:0px;border-radius:0px;color:#000000;}"
              "QProgressBar::chunk{background:rgb(60, 140, 220);}";
    }
    else if(_percent < 90){
        qss = "QProgressBar{text-align:center;border-width:0px;border-radius:0px;color:#FFFFFF;}"
              "QProgressBar::chunk{background:rgb(60, 140, 220);}";
    }
    else{
        qss = "QProgressBar{text-align:center;border-width:0px;border-radius:0px;color:#FFFFFF;}"
              "QProgressBar::chunk{background:rgb(255, 30, 30);}";
    }

    bar->setStyleSheet(qss);
    ui->tableWidget->setCellWidget(_row, 4, bar);

}

void RunningStatus::warnStorageIsTooLow()
{
    QString message = "内存容量不足!";    // "Out of memory!"
    QString buttonText = "确认";  // "OK"

    MessageWin mess;
    mess.setContent(message);
    //mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    mess.setButtonText(buttonText);
    if(mess.exec() == QDialog::Accepted){}
    else{}
}

bool RunningStatus::cleanAllStorage(QString &_err_msg)
{
    // 删除数据表
    {
        //QSqlQuery query(MySQLite::m_dbconn);
        //
        //QString sql_drop_table_histroy ="DROP TABLE HistroyPatients";       // 这是旧的数据表名，单词 History 写错了
        //if (query.exec(sql_drop_table_histroy)) {
        //    qDebug() << "drop table HistroyPatients!";
        //} else {
        //    qDebug() << "drop table HistroyPatients failed!";
        //}
        //
        //QString sql_drop_table_history = "DROP TABLE " + TABLE_NAME_HISTORY;
        //if (query.exec(sql_drop_table_history)) {
        //    qDebug() << "drop table " << TABLE_NAME_HISTORY << "!";
        //} else {
        //    qDebug() << "drop table " << TABLE_NAME_HISTORY << " failed!";
        //}
    }

    // 删除数据库文件      // NOTE: 删除文件后再重建，可作为数据库文件损坏后修复的途径，因为数据库文件损坏后，删表重建表并不能修复
    {
        // 关闭数据库
        bool succ_close = MySQLitePatients::getInstance()->closeDatabase(_err_msg);
        if (!succ_close) {
            _err_msg = tr("关闭数据库失败") + ": " + _err_msg;
            return false;
        }

        // 删除文件
        const QString file_path = MySQLitePatients::getInstance()->databaseFilePath();
        QFile file(file_path);
        bool succ_del = file.remove();
        if (!succ_del) {
            _err_msg = tr("删除数据库文件失败") + ": " + file.errorString();
            return false;
        }

        // 重新打开数据库
        bool succ_open = MySQLitePatients::getInstance()->openDatabase(_err_msg);
        if (!succ_open) {
            _err_msg = tr("重新关闭数据库失败") + ": " + _err_msg;
            return false;
        }
    }

    // 重建数据表
    MySQLitePatients::getInstance()->initDatabase();        // 初始化数据库

    // 重置当前 ID 记录文件
    //getWinManage()->resetCurrentIdFile();
    //qDebug() << "reset current Num completed !";

    //
    qDebug() << "start clear k.value.txt and photo ...";

    // 删除 "/media/log" 目录并重建                // TODO: 这些路径应该用常量定义
    QDir deleteDir("/media/log");
    if(deleteDir.exists()){
        deleteDir.removeRecursively();
        qDebug() << "remove log folder!";
    }
    if(deleteDir.mkpath("/media/log")){
        qDebug() << "create log folder!";
    }

    // 删除 "/media/algoLog" 目录并重建
    QDir algo_log_dir("/media/algoLog");
    if(algo_log_dir.exists()){
        algo_log_dir.removeRecursively();
        qDebug() << "remove log folder!";
    }
    if(algo_log_dir.mkpath("/media/algoLog")){
        qDebug() << "create log folder!";
    }

    // 删除 "/media/photo" 目录并重建
    QDir photoDir("/media/photo");
    if(photoDir.exists()){
        photoDir.removeRecursively();
        qDebug() << "remove photo folder!";
    }
    if(photoDir.mkpath("/media/photo")){
        qDebug() << "create photo folder!";
    }

    qDebug() << "clear clear k.value.txt and photo complete!";

    // 删除 "/media/pdfPreviewImg" 目录并重建
    QDir preViewImg("/media/pdfPreviewImg");
    if(preViewImg.exists()){
        preViewImg.removeRecursively();
        qDebug()<<"remove preViewImg sucess!";
    }
    if(preViewImg.mkpath("/media/pdfPreviewImg")){
        qDebug()<<"creat preViewImg";
    }

    // 删除 PDF 报告 目录并重建
    QDir pdfFile(PDF_REPORT_DIR);
    if (pdfFile.exists()) {
        pdfFile.removeRecursively();
        qDebug()<<"remove pdfFile sucess!";
    }
    if(pdfFile.mkpath(PDF_REPORT_DIR)){
        qDebug() << "creat " << PDF_REPORT_DIR;
    }

    //
    return true;
}

int RunningStatus::cleanFilesBeforeDate(const QString &_dir_path, const QDate &_date_earliest_keep, bool _remove_if_empty)
{
    //
    QDir dir(_dir_path);
    if (!dir.exists()) {
        //logDebug("");
        return 0;
    }

    //
    QFileInfoList list_file_info = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    int count_escape = 0;
    QDateTime file_time;
    QDate file_date;
    for (int i = 0; i < list_file_info.size(); i++) {
        const QFileInfo &file_info = list_file_info.at(i);
        file_time = file_info.fileTime(QFileDevice::FileMetadataChangeTime);
        file_date = file_time.date();

        if (!file_info.isDir()) {
            if (file_date.isValid()) {
                if (file_date < _date_earliest_keep) {
                    bool succ_del = QFile::remove(file_info.absoluteFilePath());
                    if (!succ_del) {
                        //logWarning();
                    }
                } else {
                    count_escape++;
                }
            } else {
                //logCritical("文件日期无效");
                count_escape++;
            }
        } else {
            int count = cleanFilesBeforeDate(file_info.absoluteFilePath(), _date_earliest_keep, true);
            if (count > 0) {
                count_escape++;
            }
        }
    }

    // 若文件夹内的文件已清空，则删除文件夹
    if (_remove_if_empty && 0 == count_escape) {
        dir.removeRecursively();
    }

    //
    return count_escape;
}

// 清空记录
void RunningStatus::cleanStorage(const QDate &_date_earliest_keep)
{
    QString text = tr("清理存储后数据将不可恢复！若未备份切勿选择 Yes，是否继续？");   // "Data cleaned could not be restored! Don't choose Yes if has not backed up, Continue?"
    bool nRet = getWinManage()->showNoticeWin(text);
    if (nRet) {
        qDebug() << "start clear history record ...";

        bool succ = true;
        if (DATE_CLEAN_ALL == _date_earliest_keep) {
            // 清除所有用户数据
            QString err_msg;
            bool succ = cleanAllStorage(err_msg);
            if (!succ) {
                getWinManage()->showMsgWin(tr("清理所有历史数据失败") + ": " + err_msg);
            }
        } else {
            // 显示等待提示框
            int msg_id = getWinManage()->showMsgWin(tr("正在清理用户数据，请稍侯..."), false);  // "Cleaning user data, please wait..."

            qApp->processEvents();

            // 清理数据表的记录
            MySQLitePatients::getInstance()->deleteBeforeDate(_date_earliest_keep);

            // 清理 "/media/log" 目录的文件
            cleanFilesBeforeDate("/media/log", _date_earliest_keep, false);

            // 清理 "/media/algoLog" 目录的文件
            cleanFilesBeforeDate("/media/algoLog", _date_earliest_keep, false);

            // 清理 "/media/photo" 目录的文件
            cleanFilesBeforeDate("/media/photo", _date_earliest_keep, false);

            // 清理 "/media/pdfPreviewImg" 目录的文件
            cleanFilesBeforeDate("/media/pdfPreviewImg", _date_earliest_keep, false);

            // 清理 PDF 报告
            cleanFilesBeforeDate(PDF_REPORT_DIR, _date_earliest_keep, false);

            // 隐藏等待提示框
            getWinManage()->hideMsgWin(msg_id);
        }

        // 磁盘同步
        //sync();           // TODO: 这个 C 函数是非阻塞的？所以返回时，缓冲未必已经写入？
        system("sync");

        // 刷新 存储 使用率
        RunningStatus::refreshStorageRate();

        //
        if (succ) {
            globalIntf()->asyncSuspensionPrompt(tr("存储清理完毕。"));  // "Storage cleaning completed."
        }
    }
}

void RunningStatus::on_lvCleanPeriod_clicked(const QModelIndex &_index)
{
    //
    ui->winCleanPeriod->setVisible(false);

    // 计算最大的日志日期
    QDate date_max;
    enCleanPeriod clean_period = (enCleanPeriod)_index.row();
    switch (clean_period) {
    case cleanPeriod_oneWeekAgo:
        date_max = QDate::currentDate().addDays(-7);
        break;
    case cleanPeriod_twoWeeksAgo:
        date_max = QDate::currentDate().addDays(-14);
        break;
    case cleanPeriod_oneMonthAgo:
        date_max = QDate::currentDate().addMonths(-1);
        break;
    case cleanPeriod_threeMonthsAgo:
        date_max = QDate::currentDate().addMonths(-3);
        break;
    case cleanPeriod_halfYearAgo:
        date_max = QDate::currentDate().addMonths(-6);
        break;
    case cleanPeriod_oneYearAgo:
        date_max = QDate::currentDate().addYears(-1);
        break;
    case cleanPeriod_all:
        date_max = DATE_CLEAN_ALL;
        break;
    default:
        date_max = QDate::currentDate().addMonths(-3);
        break;
    }
    //qDebug() << date_max;

    //
    if (cleanPeriod_all != clean_period) {
        date_max = date_max.addDays(1);
    }

    //
    emit sigCleanStorage(date_max);

}

void RunningStatus::slotCleanStorage(QDate _date_earliest_keep)
{
    cleanStorage(_date_earliest_keep);
}
