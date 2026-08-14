//历史记录（门诊记录）
#include "winpersonalrecord.h"
#include "ui_winpersonalrecord.h"

#include <QTableWidget>
#include <QDebug>
#include <QModelIndex>
#include <QTableWidgetItem>
#include <QScrollBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPainter>
#include <QHeaderView>
#include <QSqlQuery>
#include <QMovie>
#include <QSignalBlocker>

#include <string>
#include <vector>
#include <iostream>

#include "mainwindow.h"
#include "mysqlitepatients.h"
#include "threadmodel.h"
#include "import.h"
#include "winscreen.h"
#include "messagewin.h"
#include "noticewin.h"
#include "windowsmanager.h"
#include "printertransmit.h"
#include "appsetting.h"
#include "global.h"
#include "util-app.h"
#include "winpersonalrecord.h"

using namespace std;

using namespace DataTrans;

//
WinPersonalRecord::WinPersonalRecord(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::WinPersonalRecord)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    //pal.setColor(QPalette::Background,Qt::white);
    //setPalette(pal);

    QFont font = qApp->font();

    //QList<QLabel *> qlabel = this->findChildren<QLabel *>();
    //foreach (QLabel *ql, qlabel) {
    //     ql->setFont(font);
    //}

    font.setPointSize(12);
    this->ui->tableWidget->setFont(font);

//    ui->tableWidget->setStyleSheet("background-color:transparent");
//    ui->tableWidget->setStyleSheet("font:13pt '微软雅黑';color: white;");

    this->ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    //this->ui->tableWidget->setSelectionMode(QAbstractItemView::MultiSelection);  //设置可选中多个目标
    this->ui->tableWidget->setSelectionMode(QAbstractItemView::NoSelection);    // 设置表格不可选择
    this->ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);   //设置每行内容不可更改
    this->ui->tableWidget->setFocusPolicy(Qt::NoFocus);
    this->ui->tableWidget->horizontalHeader()->setSectionsClickable(false);      //水平方向的头不可点击
    //this->ui->tableWidget->setAlternatingRowColors(true);                      //设置隔一行变一颜色，即：一灰一白
    this->ui->tableWidget->verticalScrollBar()->setHidden(true);
    this->ui->tableWidget->horizontalScrollBar()->setHidden(true);
    this->ui->tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); //影藏水平滚动条
    //this->ui->tableWidget->horizontalHeader()->show();      //显示表头

    int col_width_total = ui->tableWidget->width();
    double width_ratio_1 = 0.1, width_ratio_2 = 0.35, width_ratio_3 = 0.15, width_ratio_4 = 0.15;
    ui->tableWidget->setColumnWidth(0, 0);
    ui->tableWidget->setColumnWidth(1, col_width_total * width_ratio_1);
    ui->tableWidget->setColumnWidth(2, col_width_total * width_ratio_2);
    ui->tableWidget->setColumnWidth(3, col_width_total * width_ratio_3);
    ui->tableWidget->setColumnWidth(4, col_width_total * width_ratio_4);
    ui->tableWidget->setColumnWidth(5, col_width_total * (1 - width_ratio_1 - width_ratio_2 - width_ratio_3 - width_ratio_4));
    /* 不可设置 horizontalHeaderStretchLastSection，因为这样之后第一列不可隐藏 */

    ui->tableWidget->setIconSize(QSize(48,21));
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(54);          //设置行高度

    int checkbox_x = ui->checkBox_0->x();
    int top_ckb = ui->tableWidget->y() + ui->tableWidget->horizontalHeader()->height();
    int row_height = ui->tableWidget->rowHeight(1);
    int space_checkbox = (ui->tableWidget->rowHeight(1) - ui->checkBox_0->height()) / 2;
    ui->checkBox_0->move(checkbox_x, top_ckb + row_height * 0 + space_checkbox);
    ui->checkBox_1->move(checkbox_x, top_ckb + row_height * 1 + space_checkbox);
    ui->checkBox_2->move(checkbox_x, top_ckb + row_height * 2 + space_checkbox);
    ui->checkBox_3->move(checkbox_x, top_ckb + row_height * 3 + space_checkbox);
    ui->checkBox_4->move(checkbox_x, top_ckb + row_height * 4 + space_checkbox);

    int btn_view_x = ui->btnView_0->x();
    int y_dif = (ui->checkBox_0->height() - ui->btnView_0->height()) / 2;
    ui->btnView_0->move(btn_view_x, ui->checkBox_0->y() + y_dif);
    ui->btnView_1->move(btn_view_x, ui->checkBox_1->y() + y_dif);
    ui->btnView_2->move(btn_view_x, ui->checkBox_2->y() + y_dif);
    ui->btnView_3->move(btn_view_x, ui->checkBox_3->y() + y_dif);
    ui->btnView_4->move(btn_view_x, ui->checkBox_4->y() + y_dif);

    int btn_del_x = ui->btnDelete_0->x();
    ui->btnDelete_0->move(btn_del_x, ui->checkBox_0->y() + y_dif);
    ui->btnDelete_1->move(btn_del_x, ui->checkBox_1->y() + y_dif);
    ui->btnDelete_2->move(btn_del_x, ui->checkBox_2->y() + y_dif);
    ui->btnDelete_3->move(btn_del_x, ui->checkBox_3->y() + y_dif);
    ui->btnDelete_4->move(btn_del_x, ui->checkBox_4->y() + y_dif);

    int btn_upload_x = ui->btnUpload_0->x();
    ui->btnUpload_0->move(btn_upload_x, ui->checkBox_0->y() + y_dif);
    ui->btnUpload_1->move(btn_upload_x, ui->checkBox_1->y() + y_dif);
    ui->btnUpload_2->move(btn_upload_x, ui->checkBox_2->y() + y_dif);
    ui->btnUpload_3->move(btn_upload_x, ui->checkBox_3->y() + y_dif);
    ui->btnUpload_4->move(btn_upload_x, ui->checkBox_4->y() + y_dif);

    QObject::connect(ui->checkBox_0, &QCheckBox::stateChanged, this, &WinPersonalRecord::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_1, &QCheckBox::stateChanged, this, &WinPersonalRecord::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_2, &QCheckBox::stateChanged, this, &WinPersonalRecord::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_3, &QCheckBox::stateChanged, this, &WinPersonalRecord::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_4, &QCheckBox::stateChanged, this, &WinPersonalRecord::onRowCheckBoxStateChanged);

    iconUpload = QIcon(":/resource/black_theme/cell-icon-upload_b.png");
    iconUploaded = QIcon(":/resource/black_theme/cell-icon-uploaded_b.png");

    this->sortType = (enSortType)(appSetting::value("ui/SortTypePersonRecord", (int)this->sortType).toInt());
    if (sortType > enSortType::ByTimeDsc) {
        // logCritical();
        setSortType(enSortType::ByTimeDsc, false);
    }

    mysql = MySQLitePatients::getInstance();

    QHBoxLayout mlayout;

    this->setLayout(&mlayout);

    uMovie = new QMovie(":/resource/uploading.gif");
    const int MOVIE_WIDTH = 32;
    upLoading = new QLabel(this);
    upLoading->setMovie(uMovie);
    upLoading->setGeometry((SCREEN_WIDTH - MOVIE_WIDTH) / 2, (SCREEN_HEIGHT - MOVIE_WIDTH) / 2, MOVIE_WIDTH, MOVIE_WIDTH);
    upLoading->hide();
    //upLoading->show();
    //uMovie->start();

    ui->lblPageNum->setText("0 / 0");

    // 恢复窗口设计期间的辅助特征
    ui->lblName->setFrameStyle(QFrame::NoFrame);
    ui->lblSex->setFrameStyle(QFrame::NoFrame);
    ui->lblBirthDate->setFrameStyle(QFrame::NoFrame);

}

WinPersonalRecord::~WinPersonalRecord()
{
//    input->close();
    delete ui;
}

void WinPersonalRecord::updateTheme()
{
    //
    static QString style_sheet_black;
    static bool style_black_readed = false;
    if (!style_black_readed) {
        Util::readFileToQStr(":/resource/qss/winpersonalrecord.qss", style_sheet_black);
        style_black_readed = true;
    }

    //
    //QPalette palette;
    if (themeType_Black == getSysThemeType()) {
        this->setStyleSheet(style_sheet_black);

        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));

        //
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
        ui->btnPrintTicket->setIcon(QIcon(":/resource/black_theme/print-ticket_small_b.png"));
        ui->btnPrintA4->setIcon(QIcon(":/resource/black_theme/print-a4_small_b.png"));
        ui->pushButton_upLoad->setIcon(QIcon(":/resource/black_theme/batch-upload_small_b.png"));

    }
    else {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        //
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
        ui->btnPrintTicket->setIcon(QIcon(":/resource/white_theme/print_small_w.png"));
        ui->btnPrintA4->setIcon(QIcon(":/resource/white_theme/export_w.png"));
        ui->pushButton_upLoad->setIcon(QIcon(":/resource/white_theme/uploading_w.png"));

        ui->home_label->setStyleSheet("color:rgb(1,1,1);");
        ui->lblPrintTicket->setStyleSheet("color:rgb(1,1,1);");
        ui->lblPrintA4->setStyleSheet("color:rgb(1,1,1);");
        ui->upLoad_label->setStyleSheet("color:rgb(1,1,1);");

        QList<QCheckBox *> list_CheckBox = findChildren<QCheckBox *>();
        foreach(QCheckBox *p, list_CheckBox) {
            p->setStyleSheet("QCheckBox::indicator {width: 45px;height: 50px;}\
                             QCheckBox::indicator {background-color:rgb(242,242,247);}\
                             QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png)}\
                             QCheckBox::indicator:checked{image:url(:/resource/checked.png)}");
        }
        ui->Allselection->setStyleSheet("QCheckBox{color:rgb(1,1,1);}\
                                        QCheckBox{border-radius:5px;}\
                                        QCheckBox{width: 50px;height: 50px;}\
                                        QCheckBox{background-color:rgb(242,242,247);}\
                                        QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png)}\
                                        QCheckBox::indicator:checked{image:url(:/resource/checked.png)}");
        ui->tableWidget->horizontalHeader()->setStyleSheet("QHeaderView::section{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
        ui->tableWidget->setStyleSheet("QTableWidget{background-color:rgb(225,225,230); color:rgb(50,50,50);}");
        ui->btnPrevious->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
        ui->btnNext->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
        ui->lblPageNum->setStyleSheet("color:rgb(1,1,1);");

        ui->cmbSortType->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
    }
    //this->setPalette(palette);
    //this->setAutoFillBackground(true);

}

void WinPersonalRecord::showEvent(QShowEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 每次都重新从数据库载入数据，避免数据被编辑后不同步
    reloadCurrentRecord();

    // 更新控件的样式和图标
    updateTheme();
    qDebug() << "paint WinPersonalRecord:" << getSysThemeType();

    // 更新语言
    languagechange();

    // 更新窗口头部数据
    ui->lblName->setText(patient.patientname);
    ui->lblSex->setText(patient.getSexDisc());
    ui->lblBirthDate->setText(patient.getBirthDateStr());

    //
    ui->cmbSortType->blockSignals(true);
    ui->cmbSortType->setCurrentIndex((int)sortType);
    ui->cmbSortType->blockSignals(false);

    // 载入数据到表格
    loadDataToTable();

}

void WinPersonalRecord::hideEvent(QHideEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    //
    idSelected.clear();

    selectAll(Qt::Unchecked);
}

void WinPersonalRecord::loadDataToTable()
{
    // 从数据库中查出数据集
    mLk.clear();
    mLk = mysql->findRecordByPatientid(patient.patientid, 1, sortType);       // 因为结果表和个人信息表未分开，门诊数据会多一条未测的记录，作为个人信息记录
    qDebug() << "mLk.size() = " << mLk.size();

    //
    hisMap.clear();
    hisMap.loadTableData(mLk.size());
    tableData.clear();
    tableData = hisMap.currentPage();

    //
    showCurrentPageToTable();

}

void WinPersonalRecord::on_pushButton_Back_clicked()
{
    getWinManage()->showWindowByType(WIN_CLINIC);
}

void WinPersonalRecord::on_tableWidget_clicked(const QModelIndex &_index)
{
    Q_UNUSED(_index)
    //    qDebug() << "-----on_tableWidget_clicked---";

    //
    //if (!this->isVisible()) {
    //    return;
    //}

    // 若点击其它列，
    //viewRecord(_index.row());

}

void WinPersonalRecord::selectAll(int _state)
{
    //
    QSignalBlocker blocker_sel_all(ui->Allselection);
    QSignalBlocker blocker_0(ui->checkBox_0), blocker_1(ui->checkBox_1), blocker_2(ui->checkBox_2), blocker_3(ui->checkBox_3), blocker_4(ui->checkBox_4);
    if (_state == Qt::Checked)
    {
        //
        ui->Allselection->setChecked(true);

        //
        if (ui->tableWidget->item(0, 0)) ui->checkBox_0->setChecked(true);
        if (ui->tableWidget->item(1, 0)) ui->checkBox_1->setChecked(true);
        if (ui->tableWidget->item(2, 0)) ui->checkBox_2->setChecked(true);
        if (ui->tableWidget->item(3, 0)) ui->checkBox_3->setChecked(true);
        if (ui->tableWidget->item(4, 0)) ui->checkBox_4->setChecked(true);

        //
        for (unsigned int i = 0; i  <mLk.size(); i++) {
            if(!idSelected.contains(mLk.at(i).id))
                idSelected.push_back(mLk.at(i).id);
        }
    }
    else
    {
        //
        ui->Allselection->setChecked(false);

        //
        ui->checkBox_0->setChecked(false);
        ui->checkBox_1->setChecked(false);
        ui->checkBox_2->setChecked(false);
        ui->checkBox_3->setChecked(false);
        ui->checkBox_4->setChecked(false);

        //
        idSelected.clear();
    }
}

void WinPersonalRecord::on_Allselection_stateChanged(int arg1)
{
    selectAll(arg1);
}

void WinPersonalRecord::onRowCheckBoxStateChanged(int _state)
{
    // 若被取消非选中状态，则取消“全选”按钮的选中状态
    if (Qt::Checked != _state) {
        QSignalBlocker blocker_sel_all(ui->Allselection);
        ui->Allselection->setChecked(false);
    }

    // 得到 id
    QString obj_name = sender()->objectName();
    int idx = obj_name.mid(obj_name.lastIndexOf("_") + 1).toUInt();
    int id;
    if(ui->tableWidget->item(idx, 0)){
        id = ui->tableWidget->item(idx, 0)->text().toInt();
        qDebug() << "id = " << id;
    } else {
        logWarning(QString("%1: logic error: getting id of row %2 failed, clicking failed").arg(__PRETTY_FUNCTION__).arg(idx));
        QCheckBox *check_box = dynamic_cast<QCheckBox*>(sender());
        if (check_box) {
            QSignalBlocker blocker(check_box);
            check_box->setChecked(false);
            getWinManage()->showSuspensionPrompt(tr("内部错误：获取ID失败，选择失败！"));  // "Internal error: \nFailed to obtain ID, selection failed!"
        } else {
            logWarning(QString("%1: logic error: getting QCheckBox of row %2 failed, canceling checking failed").arg(__PRETTY_FUNCTION__).arg(idx));
        }
        //
        return;
    }

    //
    if (Qt::Checked == _state) {
        if (!idSelected.contains(id)) {
            idSelected.push_back(id);
            qDebug() << "selected: = " << id;
        }
    } else {
        if (idSelected.contains(id)) {
            idSelected.removeAt(idSelected.indexOf(id));
            qDebug() << "unselected: id = " << id;
        }
    }
}

void WinPersonalRecord::showCurrentPageToTable()
{
    qDebug() << __PRETTY_FUNCTION__ << ":" << batchhistory << ",tableSize:" << tableData.size();

    //
    QSignalBlocker blocker_0(ui->checkBox_0), blocker_1(ui->checkBox_1), blocker_2(ui->checkBox_2), blocker_3(ui->checkBox_3), blocker_4(ui->checkBox_4);

    ui->checkBox_0->setChecked(false);
    ui->checkBox_1->setChecked(false);
    ui->checkBox_2->setChecked(false);
    ui->checkBox_3->setChecked(false);
    ui->checkBox_4->setChecked(false);

    //
    do {
        this->ui->tableWidget->clearContents();
        clearUploadButtons();

        //
        if (mLk.size() == 0) {
            break;
        }

        //
        if(batchhistory == 1){
            tableData.clear();
            tableData = hisMap.currentPage();
        }
        else if(batchhistory == 3)
        {
            tableData.clear();
            tableData = hisMap.lastPage();
        }

        batchhistory = 0;

        //
        for (int i = 0; i < tableData.size(); i++)
        {
            if (i >= 5)
                break;

            int m = tableData.at(i);
            CPatient pat = mLk.at(m);

            int id = pat.id;
            QString time = pat.patienttesttime;

            this->ui->tableWidget->setItem(m % 5, 0, new QTableWidgetItem(QString::number(id)));
            this->ui->tableWidget->setItem(m % 5, 1, new QTableWidgetItem(QString::number(i + 1)));
            this->ui->tableWidget->setItem(m % 5, 2, new QTableWidgetItem(time));
            //this->ui->tableWidget->setItem(m % 5, 3, new QTableWidgetItem(QString("")));

            QPushButton *btn = getUploadButton(i);
            if (btn) {
                btn->setVisible(pat.isTest);
                if (pat.isUploaded) {
                    //this->ui->tableWidget->setItem(i, 4, new QTableWidgetItem(iconUploaded, QString("")));
                    btn->setIcon(iconUploaded);
                } else {
                    //this->ui->tableWidget->setItem(i, 4, new QTableWidgetItem(iconUpload, QString("")));
                    btn->setIcon(iconUpload);
                }
            } else {
                // logCritical();
            }

            //this->ui->tableWidget->setItem(m % 5, 5, new QTableWidgetItem(QString("")));

            this->ui->tableWidget->item(m % 5, 1)->setTextAlignment(Qt::AlignCenter);       // 序号 居中
            this->ui->tableWidget->item(m % 5, 2)->setTextAlignment(Qt::AlignCenter);       // 测量时间 居中
            //this->ui->tableWidget->item(m % 5, 4)->setTextAlignment(Qt::AlignCenter);       // 上传状态 居中

            bool checkboxState = idSelected.contains(id);
            switch (m%5) {
            case 0:
                this->ui->checkBox_0->setChecked(checkboxState);
                break;
            case 1:
                this->ui->checkBox_1->setChecked(checkboxState);
                break;
            case 2:
                this->ui->checkBox_2->setChecked(checkboxState);
                break;
            case 3:
                this->ui->checkBox_3->setChecked(checkboxState);
                break;
            case 4:
                this->ui->checkBox_4->setChecked(checkboxState);
                break;
            default:
                break;
            }

            qDebug() << QString("-read show: %1, id=%2").arg(i).arg(id);
        }
    } while (false);

    // 页码
    ui->lblPageNum->setText(QString("%1 / %2").arg(hisMap.getCurrentPageNum()).arg(hisMap.getTotalPageNum()));

    // 表格按钮行数同步
    showTableButtons(tableData.size());

    //
    qDebug() << "leave history " << __FUNCTION__ << "()";
}

QPushButton *WinPersonalRecord::getUploadButton(int _row)
{
    if (0 == _row) {
        return ui->btnUpload_0;
    } else if (1 == _row) {
        return ui->btnUpload_1;
    } else if (2 == _row) {
        return ui->btnUpload_2;
    } else if (3 == _row) {
        return ui->btnUpload_3;
    } else if (4 == _row) {
        return ui->btnUpload_4;
    }
    return Q_NULLPTR;
}

void WinPersonalRecord::on_btnPrevious_clicked()   //上翻页
{
    tableData.clear();
    tableData = hisMap.previousPage();
    showCurrentPageToTable();
}

void WinPersonalRecord::on_btnNext_clicked()     //下翻页
{
    tableData.clear();
    tableData = hisMap.nextPage();
    showCurrentPageToTable();
}

bool WinPersonalRecord::commandShow(QString cmd)
{
    QProcess *p = new QProcess;
    p->start(cmd);
    p->waitForFinished(3000);
    QByteArray out = p->readAllStandardError();

    qDebug()<<"out = "<<out;
    strOutput = out;
    bool successed = false;
    bool failed;
    if (strOutput.startsWith("mount:", Qt::CaseSensitive)) {
        successed = strOutput.contains("already mounted",Qt::CaseSensitive);
        failed = strOutput.contains("not exist", Qt::CaseSensitive);
        if (successed) {
            qDebug()<<"already mounted";
            return successed;
        }
        if (failed) {
            qDebug()<<"not exist";
            return !failed;
        }
    }
    return successed;
}

void WinPersonalRecord::ref()
{
    qDebug() << __PRETTY_FUNCTION__;
    loadDataToTable();
}

void WinPersonalRecord::print_clicked()
{
    ui->btnPrintTicket->setEnabled(false);

    if(idSelected.size()==0){
        QString text = tr("请选择打印记录");   // "Please select print records"
        getWinManage()->showMsgWin(text);
        ui->btnPrintTicket->setEnabled(true);
        return;
    }
    if (ticketPrintConnType_BT == CGlobal::ticketPrintConnType)  //蓝牙打印
    {
        if(!g_Bluetooth->getBtPrinter()->getIsConnected())
        {
            qDebug()<<"bluetooth is disconnect!";
            QString text = tr("打印机未连接！");   // "Printer disconnect!"
            getWinManage()->showMsgWin(text);
            ui->btnPrintTicket->setEnabled(true);
            return;
        }
        GetPrintPara(); //获取将要打印的数据
    }
    if (ticketPrintConnType_WiFi == CGlobal::ticketPrintConnType)  //wifi打印
    {
        QString text;
        if(printerTransmit::checkWifiPrinterConnect() == 1)
        {
            qDebug()<<"Network is disconnect!";
            text = tr("网络未连接!");    // "Network is disconnect!"
            getWinManage()->showMsgWin(text);
            ui->btnPrintTicket->setEnabled(true);
            return;
        }
        else if(printerTransmit::checkWifiPrinterConnect() == 2)
        {
            qDebug()<<"Abnormal network connection!";
            text = tr("打印机连接失败！");  // "Failed to connect to printer!"
            getWinManage()->showMsgWin(text);
            ui->btnPrintTicket->setEnabled(true);
            return;
        }
        GetPrintPara(); //获取将要打印的数据
    }
}

void WinPersonalRecord::GetPrintPara()
{
    //std::sort(idSelected.begin(),idSelected.end());

    std::vector<CPatient> pats = mysql->findRecordByIdList(idSelected);

    WinMeasure::setOperationMode(operationMode_HistoryRecord);
    emit batchPrintSig(pats);

    selectAll(Qt::Unchecked);
    idSelected.clear();
    ui->btnPrintTicket->setEnabled(true);
}

void WinPersonalRecord::on_btnPrintTicket_clicked()
{
    print_clicked();
}

void WinPersonalRecord::languagechange()
{
    qDebug() << "enter " << __PRETTY_FUNCTION__;

    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("个人记录 - %1").arg(patient.patientid));    // "Personal Records - %1"

    //
    //if (language) {
    //    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << "id" << "序号" << "测量时间" << "测量结果" << "上传状态" << "删除");
    //    ui->cmbSortType->setItemText(0, "按时间升序");
    //    ui->cmbSortType->setItemText(1, "按时间降序");
    //    ui->cmbSortType->setItemText(2, "按编号升序");
    //    ui->cmbSortType->setItemText(3, "按编号降序");
    //    ui->Allselection->setText("全选");
    //    ui->home_label->setText("返回");
    //
    //    ui->lblPrintTicket->setText("小票打印");
    //    ui->lblPrintA4->setText("A4打印");
    //    ui->upLoad_label->setText("批量上传");
    //
    //    ui->lblTitle_Name->setText("姓名");
    //    ui->lblTitle_Sex->setText("性别");
    //    ui->lblTitle_BirthDate->setText("出生日期");
    //
    //    ui->btnView_0->setText("查看");
    //    ui->btnView_1->setText("查看");
    //    ui->btnView_2->setText("查看");
    //    ui->btnView_3->setText("查看");
    //    ui->btnView_4->setText("查看");
    //
    //    ui->btnDelete_0->setText("删除");
    //    ui->btnDelete_1->setText("删除");
    //    ui->btnDelete_2->setText("删除");
    //    ui->btnDelete_3->setText("删除");
    //    ui->btnDelete_4->setText("删除");
    //
    //} else {
    //    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << "id" << "Number" << "MeasureTime" << "Result" << "Upload" << "Delete");
    //    ui->cmbSortType->setItemText(0, "Ascending by Time");
    //    ui->cmbSortType->setItemText(1, "Descending by Time");
    //    ui->cmbSortType->setItemText(2, "Ascending by ID");
    //    ui->cmbSortType->setItemText(3, "Descending by ID");
    //    ui->Allselection->setText("All");
    //    ui->home_label->setText("Back");
    //
    //    ui->lblPrintTicket->setText("TicketPrint");
    //    ui->lblPrintA4->setText("A4 Print");
    //    ui->upLoad_label->setText("BatchUpload");
    //
    //    ui->lblTitle_Name->setText("Name");
    //    ui->lblTitle_Sex->setText("Sex");
    //    ui->lblTitle_BirthDate->setText("BirthDate");
    //
    //    ui->btnView_0->setText("View");
    //    ui->btnView_1->setText("View");
    //    ui->btnView_2->setText("View");
    //    ui->btnView_3->setText("View");
    //    ui->btnView_4->setText("View");
    //
    //    ui->btnDelete_0->setText("Delete");
    //    ui->btnDelete_1->setText("Delete");
    //    ui->btnDelete_2->setText("Delete");
    //    ui->btnDelete_3->setText("Delete");
    //    ui->btnDelete_4->setText("Delete");
    //}

    //
    QFont font;
    if (G_LANGUAGE_CHINESE == CGlobal::language) {
        font.setPointSize(10);
    } else {
        font.setPointSize(9);
    }

    ui->lblPrintTicket->setFont(font);
    ui->lblPrintA4->setFont(font);
    ui->upLoad_label->setFont(font);

    ui->lblTitle_Name->setFont(font);
    ui->lblTitle_Sex->setFont(font);
    ui->lblTitle_BirthDate->setFont(font);

    qDebug()<<"leave WinPersonalRecord::languagechange()";
}

void WinPersonalRecord::setSortType(enSortType _sort_type, bool _need_reload)
{
    //
    if (_sort_type > enSortType::ByTimeDsc) {
        // logCritical();
        _sort_type = enSortType::ByTimeDsc;
    }

    //
    sortType = _sort_type;
    setConfig_SortType(sortType);

    //
    if (_need_reload) {
        loadDataToTable();
    }
}

void WinPersonalRecord::setPatient(const CPatient &_pat)
{
    patient.cloneFrom(_pat);
}

void WinPersonalRecord::reloadCurrentRecord()
{
    bool succ = mysql->getInfoForClinicByPatientId(patient.patientid, patient);
    if (!succ) {
        //logCritical();
    }
}

void WinPersonalRecord::setConfig_SortType(enSortType _sort_type)
{
    appSetting::setValue("ui/SortTypePersonRecord", (int)_sort_type);
}

void WinPersonalRecord::uploadSelectedRows()
{
    qDebug()<<"upload clicked";

    // 检查上传的条件
    if (!WinClinic::checkUploadCondition(false)) {
        return;
    }

    //
    if(idSelected.size()==0){
        QString text = tr("请选择上传的记录");  // "Please select records"
        getWinManage()->showMsgWin(text);
    } else{
        QString text = tr("正在上传...");   // "Uploading..."
        getWinManage()->showMsgWin(text, false);

        //
        emit sigUpLoadData(idSelected);                     // TODO: 如果后面逻辑不严谨没有信号回传，这里将不可恢复，看起来像是卡死？
    }

    idSelected.clear();
    selectAll(Qt::Unchecked);
    //showCurrentPageToTable();
}
void WinPersonalRecord::on_pushButton_upLoad_clicked()
{
    uploadSelectedRows();
}

void WinPersonalRecord::slotUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg)
{
    if (WinMeasure::isOpened())
        return;
    if (!this->isVisible())
        return;

    // 显示上传反馈消息
    WinClinic::showUploadFeedbackMsg(_count_upload, _count_upload_final, _count_succ, _msg);

    //
    batchhistory = 1;
    loadDataToTable();
}

void WinPersonalRecord::slotPhysicButtonPressed()
{
    // 若本窗体不是当前窗体，则不处理此信号
    if (this != getWinManage()->getCurrentWin()) {
        return;
    }

    /* 按下物理按键，则开启本被测者的新测量 */

    // 根据编号查询完整信息
    std::vector<CPatient> pats = mysql->findRecordByPatientid(patient.patientid, -1, enSortType::Unknown, 1);
    if (pats.size() == 0) {
        getWinManage()->showSuspensionPrompt(tr("内部错误：查找数据失败"));    // "Internal error: Failed to find data"
    }

    //
    CPatient pat_new = pats.at(0);

    pat_new.id = 0;
    pat_new.patienttesttime = "";
    pat_new.creattime = "";


    // TODO: 这里如果本窗体是扫码显示的，不应该是普通模式，应该是批量筛查模式？


    //
    WinMeasure::setOperationMode(operationMode_NormalMeasure);
    getWinManage()->openMeasureWin(pat_new, patientSource_Manual);

}

void WinPersonalRecord::on_cmbSortType_currentIndexChanged(int _index)
{
    enSortType sort_type_new = (enSortType)_index;
    if (sort_type_new != sortType) {
        //
        setSortType(sort_type_new);

        //ui->cmbSortType->clearFocus();
    }
}

void WinPersonalRecord::on_btnView_0_clicked()
{
    viewRecord(0);
}

void WinPersonalRecord::on_btnView_1_clicked()
{
    viewRecord(1);
}

void WinPersonalRecord::on_btnView_2_clicked()
{
    viewRecord(2);
}

void WinPersonalRecord::on_btnView_3_clicked()
{
    viewRecord(3);
}

void WinPersonalRecord::on_btnView_4_clicked()
{
    viewRecord(4);
}

void WinPersonalRecord::on_btnDelete_0_clicked()
{
    deleteRow(0);
}

void WinPersonalRecord::on_btnDelete_1_clicked()
{
    deleteRow(1);
}

void WinPersonalRecord::on_btnDelete_2_clicked()
{
    deleteRow(2);
}

void WinPersonalRecord::on_btnDelete_3_clicked()
{
    deleteRow(3);
}

void WinPersonalRecord::on_btnDelete_4_clicked()
{
    deleteRow(4);
}

void WinPersonalRecord::deleteRow(int _row)
{
    QString text_confirm = tr("是否删除？"); // "Delete data?"
    bool ret = getWinManage()->showNoticeWin(text_confirm);
    if (!ret) {
        return;
    }

    QString text = tr("正在删除...");   // "Deleting data..."
    getWinManage()->showMsgWin(text,false);

    qApp->processEvents();
    qDebug()<<"start delete...";

    // 获取行数据对象
    CPatient *pat = WinClinic::getPatientByRow(_row, tableData, mLk);
    if (!pat) {
        //getWinManage()->showSuspensionPrompt(msg);
        return;
    }

    // 删除数据
    QVector<int> ids;
    ids.push_back(pat->id);
    mysql->TableDelete(ids);

    // 删除文件
    std::vector<CPatient> pats;
    pats.push_back(*pat);
    WinClinic::deleteFile(pats);

    //
    getWinManage()->hideMsgWin();

    // 重新载入数据
    loadDataToTable();

}

void WinPersonalRecord::uploadRow(int _row)
{
    // 检查上传的条件
    if (!WinClinic::checkUploadCondition(false)) {
        return;
    }

    // 获取行数据对象
    CPatient *pat = WinClinic::getPatientByRow(_row, tableData, mLk);
    if (!pat) {
        //getWinManage()->showSuspensionPrompt(msg);
        return;
    }

    // 上传
    QVector<int> id_list;
    id_list.push_back(pat->id);

    QString text = tr("正在上传...");   // "Uploading..."
    getWinManage()->showMsgWin(text, false);

    //
    emit sigUpLoadData(id_list);

}

void WinPersonalRecord::showTableButtons(int _show_rows)
{
    ui->btnView_0->setVisible(_show_rows > 0);
    ui->btnView_1->setVisible(_show_rows > 1);
    ui->btnView_2->setVisible(_show_rows > 2);
    ui->btnView_3->setVisible(_show_rows > 3);
    ui->btnView_4->setVisible(_show_rows > 4);

    ui->btnDelete_0->setVisible(_show_rows > 0);
    ui->btnDelete_1->setVisible(_show_rows > 1);
    ui->btnDelete_2->setVisible(_show_rows > 2);
    ui->btnDelete_3->setVisible(_show_rows > 3);
    ui->btnDelete_4->setVisible(_show_rows > 4);

}

void WinPersonalRecord::clearUploadButtons()
{
    ui->btnUpload_0->setVisible(false);
    ui->btnUpload_1->setVisible(false);
    ui->btnUpload_2->setVisible(false);
    ui->btnUpload_3->setVisible(false);
    ui->btnUpload_4->setVisible(false);
}

void WinPersonalRecord::viewRecord(int _row)
{
    if (_row > tableData.count() - 1) {
        return;
    }

    int idx_data = tableData.at(_row);
    if (idx_data > (int)mLk.size() - 1) {
        return;
    }

    CPatient &patient = mLk.at(idx_data);
    qDebug() << "idx_data = " << idx_data << ", patient.id = " << patient.id << ", patient.patientid = " << patient.patientid;

    // 显示“结果”页面
    Result *win_result = getWinManage()->getWindow<Result>(WIN_RESULT);
    if (win_result) {
        WinMeasure::setOperationMode(operationMode_HistoryRecord);
        win_result->setIsNeedSave(false);
        win_result->setHistoryListPtr(&mLk);
        win_result->setPatient(patient);
        getWinManage()->showWindow(win_result);
    } else {
        getWinManage()->showSuspensionPrompt(tr("程序错误：Win Result object not found!"));  // "ProgramError：Win Result object not found!"
    }

}

void WinPersonalRecord::on_btnPrintA4_clicked()
{
    //
    if (idSelected.size() == 0) {
        getWinManage()->showMsgWin(tr("请先选择需要操作的行"));   // "Please select rows need to be operated first"
        return;
    }

    // 检查当前打印机是否可用
    if (!g_printIntf->getIsPrinterReady()) {
        getWinManage()->showMsgWin(tr("当前打印机不可用！\n请先连接打印机"));   // "Current printer not available!\nSet up the printer connection first."
        return;
    }

    // 打印 A4 报表
    QVector<CPatient *> pats;
    MySQLitePatients::getPatientFromListByIdList(mLk, pats, idSelected);
    for (int i = 0; i < pats.size(); i++) {
        Result::printA4Report(*pats.at(i));
    }
}

void WinPersonalRecord::on_btnUpload_0_clicked()
{
    uploadRow(0);
}

void WinPersonalRecord::on_btnUpload_1_clicked()
{
    uploadRow(1);
}

void WinPersonalRecord::on_btnUpload_2_clicked()
{
    uploadRow(2);
}

void WinPersonalRecord::on_btnUpload_3_clicked()
{
    uploadRow(3);
}

void WinPersonalRecord::on_btnUpload_4_clicked()
{
    uploadRow(4);
}
