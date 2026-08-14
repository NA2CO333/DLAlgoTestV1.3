//历史记录（门诊记录）
#include "winclinic.h"
#include "ui_winclinic.h"

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
const char * const WinClinic::S_CLASS_NAME = WinClinic::staticMetaObject.className();

WinClinic::WinClinic(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::WinClinic)
{
    ui->setupUi(this);

    isShowStatusBar = true;

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
    double width_ratio_0 = 0.25, width_ratio_1 = 0.2, width_ratio_2 = 0.13, width_ratio_3 = 0.16;
    ui->tableWidget->setColumnWidth(0, col_width_total * width_ratio_0);
    ui->tableWidget->setColumnWidth(1, col_width_total * width_ratio_1);
    ui->tableWidget->setColumnWidth(2, col_width_total * width_ratio_2);
    ui->tableWidget->setColumnWidth(3, col_width_total * width_ratio_3);
    ui->tableWidget->setColumnWidth(4, col_width_total * (1 - width_ratio_0 - width_ratio_1 - width_ratio_2 - width_ratio_3));
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

    int btn_measure_x = ui->btnMeasure_0->x();
    int y_dif = (ui->checkBox_0->height() - ui->btnMeasure_0->height()) / 2;            // TODO: 通过 QTableWidget 的 setCellWidget 方法嵌入小部件，是否更好？
    ui->btnMeasure_0->move(btn_measure_x, ui->checkBox_0->y() + y_dif);
    ui->btnMeasure_1->move(btn_measure_x, ui->checkBox_1->y() + y_dif);
    ui->btnMeasure_2->move(btn_measure_x, ui->checkBox_2->y() + y_dif);
    ui->btnMeasure_3->move(btn_measure_x, ui->checkBox_3->y() + y_dif);
    ui->btnMeasure_4->move(btn_measure_x, ui->checkBox_4->y() + y_dif);

    int btn_edit_x = ui->btnEdit_0->x();
    ui->btnEdit_0->move(btn_edit_x, ui->checkBox_0->y() + y_dif);
    ui->btnEdit_1->move(btn_edit_x, ui->checkBox_1->y() + y_dif);
    ui->btnEdit_2->move(btn_edit_x, ui->checkBox_2->y() + y_dif);
    ui->btnEdit_3->move(btn_edit_x, ui->checkBox_3->y() + y_dif);
    ui->btnEdit_4->move(btn_edit_x, ui->checkBox_4->y() + y_dif);

    //QObject::connect(ui->tableWidget, &QTableWidget::doubleClicked, this, &WinClinic::on_tableWidget_clicked);
    //QObject::connect(ui->Allselection, SIGNAL(stateChanged(int)), this, SLOT(onStateChanged(int)));                 //zy
    QObject::connect(ui->checkBox_0, &QCheckBox::stateChanged, this, &WinClinic::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_1, &QCheckBox::stateChanged, this, &WinClinic::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_2, &QCheckBox::stateChanged, this, &WinClinic::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_3, &QCheckBox::stateChanged, this, &WinClinic::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_4, &QCheckBox::stateChanged, this, &WinClinic::onRowCheckBoxStateChanged);
    //zy
    //QObject::connect(ui->pushButton_delete,SIGNAL(clicked(bool)),this,SLOT(deleteLater()));  //删除

    this->sortType = (enSortType)(appSetting::value("ui/SortTypeClinic", (int)this->sortType).toInt());

    mysql = MySQLitePatients::getInstance();

    ui->btnSearch->raise();

    uMovie = new QMovie(":/resource/uploading.gif");
    const int MOVIE_WIDTH = 32;
    upLoading = new QLabel(this);
    upLoading->setMovie(uMovie);
    upLoading->setGeometry((SCREEN_WIDTH - MOVIE_WIDTH) / 2, (SCREEN_HEIGHT - MOVIE_WIDTH) / 2, MOVIE_WIDTH, MOVIE_WIDTH);
    upLoading->hide();
    //upLoading->show();
    //uMovie->start();

    ui->lblPageNum->setText("0 / 0");

}

WinClinic::~WinClinic()
{
//    input->close();
    delete ui;
}

void WinClinic::updateTheme()
{
    //
    static QString style_sheet_black;
    static bool style_black_readed = false;
    if (!style_black_readed) {
        Util::readFileToQStr(":/resource/qss/winclinic.qss", style_sheet_black);
        style_black_readed = true;
    }

    //
    //QPalette palette;
    if (themeType_Black == getSysThemeType()) {
        this->setStyleSheet(style_sheet_black);

        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));

        //
        ui->btnAdd->setIcon(QIcon(":/resource/black_theme/add_small_b.png"));
        ui->pushButton_print->setIcon(QIcon(":/resource/black_theme/print-ticket_small_b.png"));
        ui->pushButton_export->setIcon(QIcon(":/resource/black_theme/export_small_b.png"));
        ui->pushButton_delete->setIcon(QIcon(":/resource/black_theme/delete_small_b.png"));
        ui->pushButton_upLoad->setIcon(QIcon(":/resource/black_theme/batch-upload_small_b.png"));
        ui->btnBack->setIcon(QIcon(":/resource/black_theme/back_b.png"));

    }
    else {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        //
        ui->btnAdd->setIcon(QIcon(":/resource/black_theme/add-clinic_small_w.png"));
        ui->pushButton_print->setIcon(QIcon(":/resource/white_theme/print_small_w.png"));
        ui->pushButton_export->setIcon(QIcon(":/resource/white_theme/export_w.png"));
        ui->pushButton_delete->setIcon(QIcon(":/resource/white_theme/rmove_w.png"));
        ui->pushButton_upLoad->setIcon(QIcon(":/resource/white_theme/uploading_w.png"));
        ui->btnBack->setIcon(QIcon(":/resource/white_theme/back_w.png"));

        ui->lblAdd->setStyleSheet("color:rgb(1,1,1);");
        ui->lblTicketPrint->setStyleSheet("color:rgb(1,1,1);");
        ui->bench_label->setStyleSheet("color:rgb(1,1,1);");
        ui->tool_label->setStyleSheet("color:rgb(1,1,1);");
        ui->upLoad_label->setStyleSheet("color:rgb(1,1,1);");
        ui->home_label->setStyleSheet("color:rgb(1,1,1);");

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
        ui->firstPageButton->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
        ui->lastPageButton->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
        ui->btnSearch->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
        ui->edtSearch->setStyleSheet("QLineEdit{background-color:rgb(225,225,230); color:rgb(140,140,145); border-radius:5px;}");
        ui->lblPageNum->setStyleSheet("color:rgb(1,1,1);");

        ui->cmbSortType->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
    }
    //this->setPalette(palette);
    //this->setAutoFillBackground(true);

}

void WinClinic::showEvent(QShowEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {               // TODO: 这个已经没用了？
        return;
    }

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 更新控件的样式和图标
    updateTheme();
    qDebug() << "paint WinClinic:" << getSysThemeType();

    // 更新语言
    languagechange();

    //
    ui->cmbSortType->blockSignals(true);
    ui->cmbSortType->setCurrentIndex((int)sortType);
    ui->cmbSortType->blockSignals(false);

    // 载入数据到表格
    loadDataToTable();

    // 注册键盘侦听（用于扫码）
    globalService()->regKbReader(this);

}

void WinClinic::hideEvent(QHideEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    //
    patientidSelected.clear();
    ui->edtSearch->setText("");
    ui->btnSearch->setIcon(QIcon(":/resource/fangdajing.png"));

    selectAll(Qt::Unchecked);

    // 反注册键盘侦听（用于扫码）
    globalService()->unregKbReader(this);

}

int WinClinic::loadDataToTable()
{
    //
    mLk.clear();
    mLk = mysql->getInfoForClinic(sortType);

    qDebug() << "record size = " << mLk.size();

    hisMap.clear();
    hisMap.loadTableData(mLk.size());
    tableData.clear();
    tableData = hisMap.currentPage();

    //
    showCurrentPageToTable();

    //
    return mLk.size();
}

void WinClinic::on_btnBack_clicked()   //返回主界面
{
    getWinManage()->showWindowByType(WIN_HOME);
}

void WinClinic::on_tableWidget_clicked(const QModelIndex &_index)
{
    qDebug() << "-----on_tableWidget_clicked---";

    if (!this->isVisible()) {
        return;
    }

    int row = _index.row();

    CPatient *pat = getPatientByRow(row, tableData, mLk);
    if (!pat) {
        //getWinManage()->showSuspensionPrompt(msg);
        return;
    }

    // 显示“个人记录”页面
    WinPersonalRecord *win_record = getWinManage()->getWindow<WinPersonalRecord>(WIN_PER_REC);
    if (win_record) {
        win_record->setPatient(*pat);

        getWinManage()->showWindow(win_record);
    } else {
        getWinManage()->showSuspensionPrompt(tr("程序错误：Win PersonalRecord object not found!"));  // "ProgramError：Win PersonalRecord object not found!"
    }

}

void WinClinic::selectAll(int _state)
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
            if(!patientidSelected.contains(mLk.at(i).patientid))
                patientidSelected.push_back(mLk.at(i).patientid);
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
        patientidSelected.clear();
    }
}

void WinClinic::on_Allselection_stateChanged(int arg1)
{
    selectAll(arg1);
}

void WinClinic::onRowCheckBoxStateChanged(int _state)
{
    // 若被取消非选中状态，则取消“全选”按钮的选中状态
    if (Qt::Checked != _state) {
        QSignalBlocker blocker_sel_all(ui->Allselection);
        ui->Allselection->setChecked(false);
    }

    // 得到 patientid
    QString obj_name = sender()->objectName();
    int idx = obj_name.mid(obj_name.lastIndexOf("_") + 1).toUInt();
    QString patientid;
    if (ui->tableWidget->item(idx, 0)) {
        patientid = ui->tableWidget->item(idx, 0)->text();
        qDebug() << "patientid = " << patientid;
    } else {
        logWarning(QString("%1: logic error: getting patientid of row %2 failed, clicking failed").arg(__PRETTY_FUNCTION__).arg(idx));
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
        if (!patientidSelected.contains(patientid)) {
            patientidSelected.push_back(patientid);
            qDebug() << "selected: = " << patientid;
        }
    } else {
        if (patientidSelected.contains(patientid)) {
            patientidSelected.removeAt(patientidSelected.indexOf(patientid));
            qDebug() << "unselected: id = " << patientid;
        }
    }
}

void WinClinic::showCurrentPageToTable()
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

        //
        if (mLk.size() == 0) {
            break;
        }

        //
        if (batchhistory == 1) {
            tableData.clear();
            tableData = hisMap.currentPage();
        } else if (batchhistory == 3) {
            tableData.clear();
            tableData = hisMap.lastPage();
        }

        batchhistory = 0;

        //
        for (int i = 0;i < tableData.size(); i++) {
            if (i >= 5)
                break;

            int m = tableData.at(i);
            CPatient pat = mLk.at(m);

            QString patientid = pat.patientid;
            QString name = pat.patientname;
            QString sex_desc = pat.getSexDiscAbbr();
            QString birthdate = pat.getBirthDate().toString("yy/MM/dd");

            this->ui->tableWidget->setItem(m % 5, 0, new QTableWidgetItem(patientid));
            this->ui->tableWidget->setItem(m % 5, 1, new QTableWidgetItem(name));
            this->ui->tableWidget->setItem(m % 5, 2, new QTableWidgetItem(sex_desc));
            this->ui->tableWidget->setItem(m % 5, 3, new QTableWidgetItem(birthdate));
            //this->ui->tableWidget->setItem(m % 5, 4, new QTableWidgetItem(""));

            this->ui->tableWidget->item(m % 5, 0)->setTextAlignment(Qt::AlignCenter);       // 【诊疗号】居中
            this->ui->tableWidget->item(m % 5, 1)->setTextAlignment(Qt::AlignCenter);       // 【姓名】居中
            this->ui->tableWidget->item(m % 5, 2)->setTextAlignment(Qt::AlignCenter);       // 【性别】居中
            this->ui->tableWidget->item(m % 5, 3)->setTextAlignment(Qt::AlignCenter);       // 【出生日期】居中

            bool checkboxState = patientidSelected.contains(patientid);

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

            qDebug() << QString("-read show: %1, patientid=%2").arg(i).arg(patientid);
        }

    } while (false);

    // 页码
    ui->lblPageNum->setText(QString("%1 / %2").arg(hisMap.getCurrentPageNum()).arg(hisMap.getTotalPageNum()));

    // 表格按钮行数同步
    showTableButtons(tableData.size());

    //
    //qDebug() << "leave history " << __FUNCTION__ << "()";
}

void WinClinic::on_btnPrevious_clicked()   //上翻页
{
    tableData.clear();
    tableData = hisMap.previousPage();
    showCurrentPageToTable();
}

void WinClinic::on_btnNext_clicked()     //下翻页
{
    tableData.clear();
    tableData = hisMap.nextPage();
    showCurrentPageToTable();
}

void WinClinic::deleteFile(std::vector<CPatient> _pats)
{
    // 逐个处理
    for (size_t i = 0; i < _pats.size(); i++) {
        const CPatient &pat = _pats.at(i);

        if (!pat.isTest) {
            continue;
        }

        // 删除当前数据对象对应的 pdf 文件
        UtilApp::deletePdfFilesOfPatient(pat);

        // 删除当前数据对象对应的 预览 文件
        UtilApp::deletePreviewImagesOfPatient(pat);
    }

    // TODO: 还有 photo 目录没有删

}

CPatient *WinClinic::getPatientByRow(int _row, QList<int> &_page_data, std::vector<CPatient> &_obj_list, QString *_msg)
{
    if (_row > _page_data.count() - 1) {
        return Q_NULLPTR;
    }

    int idx_data = _page_data.at(_row);
    if (idx_data > (int)_obj_list.size() - 1) {
        if (_msg) {
            *_msg = (tr("内部错误：获取数据对象失败！")); // "Internal Error: get data object failed!"
        }
        return Q_NULLPTR;
    }
    CPatient &patient = _obj_list.at(idx_data);
    //qDebug() << "idx_data = " << idx_data << ", patient.id = " << patient.id << ", patient.patientid = " << patient.patientid;
    return &patient;
}

void WinClinic::deleteSelection()
{
    if (patientidSelected.size() == 0) {
        getWinManage()->showMsgWin(tr("请先选择需要操作的行"));   // "Please select rows need to be operated first"
        return;
    }

    if (mLk.size() == 0) {
        return;
    }

    QString text_confirm = tr("确定要删除？");    // "Sure want to delete?"
    bool ret = getWinManage()->showNoticeWin(text_confirm);
    if(!ret) {
        return;
    }

    QString text = tr("正在删除...");   // "Deleting data..."
    getWinManage()->showMsgWin(text,false);

    qApp->processEvents();
    qDebug()<<"start delete...";

    // 记录下要删除的行
    std::vector<CPatient> pats = mysql->findRecordByPatientidList(patientidSelected);

    // 删除记录
    mysql->TableDeleteByNum(patientidSelected);

    // 删除文件
    deleteFile(pats);

    //
    getWinManage()->hideMsgWin();

    //
    patientidSelected.clear();

    selectAll(Qt::Unchecked);
    loadDataToTable();

    // 如果历史记录已删空，提示清理存储
    //const QString &key_word = ui->edtSearch->text();
    //if ((key_word.isEmpty()) && (0 == (int)mLk.size())) {
    //    text = tr("历史记录数量为0，但可能还有一些垃圾数据。可点击 “设置” -> “系统状态” -> “清理存储” 按钮彻底清理。");
    //                      // "History is empty, but some garbage data may exists. Click button 'Settings' -> 'SystemState' -> 'CleanStorage' to completely clean."
    //    getWinManage()->showMsgWin(text);
    //}

}

void WinClinic::export_clicked()
{
    //
    if (patientidSelected.size() == 0) {
        getWinManage()->showMsgWin(tr("请先选择需要操作的行"));   // "Please select rows need to be operated first"
        return;
    }

    //
    QString udisk_path = Util::CUDisk::getPath();
    bool isshow = (udisk_path.length() > 0);

    if (isshow) {
        // 由编号获取 id
        QVector<int> id_list;
        bool is_succ_ids = mysql->getIdsByNums(patientidSelected, id_list, 1);
        if (is_succ_ids) {
            if (id_list.size() > 0) {
                import win_import(this, id_list, udisk_path, false);
                win_import.setModal(true);
                win_import.exec();

                selectAll(Qt::Unchecked);
            } else {
                getWinManage()->showSuspensionPrompt(tr("操作失败：没有数据"));  // "operation failed: no data"
            }
        } else {
            getWinManage()->showSuspensionPrompt(tr("程序错误：获取 id 列表失败！"));   // "Program Error: get id list failed!"
        }
    } else {
        QString text = tr("未检测到U盘!");   // "U disk was not detected!"
        getWinManage()->showMsgWin(text);
    }

    //clearCheckboxes();
    qDebug() << "export history result end";
}

//导出数据
void WinClinic::on_pushButton_export_clicked()
{
    export_clicked();
}


bool WinClinic::commandShow(QString cmd)
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

void WinClinic::ref()
{
    qDebug() << __PRETTY_FUNCTION__;
    loadDataToTable();
}

void WinClinic::slotPhysicButtonPressed()
{
    // 若本窗体不是当前窗体，则不处理此信号
    if (this != getWinManage()->getCurrentWin()) {
        return;
    }

    /* 按下物理按键，等效于在主页按下物理按键 */

    //
    g_WinMeasure->continueMeasuring();

}

void WinClinic::print_clicked()
{
    ui->pushButton_print->setEnabled(false);

    if(patientidSelected.size()==0){
        QString text = tr("请选择打印记录");   // "Please select print records"
        getWinManage()->showMsgWin(text);
        ui->pushButton_print->setEnabled(true);
        return;
    }
    if (ticketPrintConnType_BT == CGlobal::ticketPrintConnType)  //蓝牙打印
    {
        if(!g_Bluetooth->getBtPrinter()->getIsConnected())
        {
            qDebug()<<"bluetooth is disconnect!";
            QString text = tr("打印机未连接！");   // "Printer not connected!"
            getWinManage()->showMsgWin(text);
            ui->pushButton_print->setEnabled(true);
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
            ui->pushButton_print->setEnabled(true);
            return;
        }
        else if(printerTransmit::checkWifiPrinterConnect() == 2)
        {
            qDebug()<<"Abnormal network connection!";
            text = tr("打印机连接失败！");  // "Failed to connect to printer!"
            getWinManage()->showMsgWin(text);
            ui->pushButton_print->setEnabled(true);
            return;
        }
        GetPrintPara(); //获取将要打印的数据
    }
}

void WinClinic::GetPrintPara()
{
    std::sort(patientidSelected.begin(),patientidSelected.end());

    std::vector<CPatient> pats = mysql->findRecordByPatientidList(patientidSelected, 1);

    WinMeasure::setOperationMode(operationMode_HistoryRecord);
    emit batchPrintSig(pats);

    selectAll(Qt::Unchecked);
    patientidSelected.clear();
    ui->pushButton_print->setEnabled(true);
}

void WinClinic::on_pushButton_print_clicked()
{
    print_clicked();
}

void WinClinic::on_pushButton_delete_clicked()
{
    deleteSelection();
}

void WinClinic::on_btnSearch_clicked()
{
    // 若搜索文本不为空，则清空
    const QString &key_word = ui->edtSearch->text();
    if (!key_word.isEmpty()) {
        ui->edtSearch->setText("");
    }
}

void WinClinic::on_edtSearch_textChanged(const QString &_arg1)
{
    // 搜索按钮的状态同步
    if (!_arg1.isEmpty()) {
        ui->btnSearch->setIconSize(QSize(22,22));
        ui->btnSearch->setIcon(QIcon(":/resource/X_3.png"));
    } else {
        ui->edtSearch->setText("");
        ui->btnSearch->setIcon(QIcon(":/resource/fangdajing.png"));
    }

    // 执行搜索
    if (this->isVisible())
    {
        doSearch();
    }
}

void WinClinic::doSearch()
{
    const QString &key_word = ui->edtSearch->text();
    if (!key_word.isEmpty()) {
       mLk.clear();
       mLk = mysql->getInfoForClinic(sortType, key_word);

       if(mLk.size() == 0 || key_word == "")
       {
           MessageWin msg;
           msg.setWindowModality(Qt::ApplicationModal);        // 阻塞msg以外的所以窗体
           msg.setContent(tr("未搜索到任何记录！!"));   // "No records found!"
           msg.exec();

           //
           ui->edtSearch->setText("");
       }
       else
       {
           //
           hisMap.clear();
           hisMap.loadTableData(mLk.size());
           tableData.clear();
           tableData = hisMap.firstPage();
       }

       selectAll(Qt::Unchecked);
       showCurrentPageToTable();
    }
    else{
        loadDataToTable();
    }
}

void WinClinic::setSortType(enSortType _sort_type, bool _need_reload)
{
    //
    sortType = _sort_type;
    setConfig_SortType(sortType);

    // 排序方法改变之后，重新载入一次数据
    if (_need_reload) {
        const QString &key_word = ui->edtSearch->text();
        if (key_word.isEmpty()) {
            loadDataToTable();
        } else {
            doSearch();
        }
    }
}

void WinClinic::languagechange()
{
    qDebug() << "enter " << __PRETTY_FUNCTION__;

    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("门诊记录"));    // "Clinic Records"

    //
    QStringList list_header;
    list_header << tr("诊疗号") << tr("姓名") << tr("性别") << tr("出生日期") << tr("操作"); // << "ClinicNo" << "Name" << "Sex" << "BirthDate" << "Operation"
    ui->tableWidget->setHorizontalHeaderLabels(list_header);
    ui->cmbSortType->setItemText(0, tr("按时间升序"));   // "Ascending by Time"
    ui->cmbSortType->setItemText(1, tr("按时间降序"));   // "Descending by Time"
    ui->cmbSortType->setItemText(2, tr("按编号升序"));   // "Ascending by Num"
    ui->cmbSortType->setItemText(3, tr("按编号降序"));   // "Descending by Num"
    ui->edtSearch->setPlaceholderText(tr("搜索"));    // "Search"
    ui->Allselection->setText(tr("全选"));    // "All"
    ui->home_label->setText(tr("返回"));  // "Back"

    ui->lblTicketPrint->setText(tr("小票打印"));    // "TicketPrint"
    ui->bench_label->setText(tr("导出"));     // "Export"
    ui->tool_label->setText(tr("删除"));      // "Delete"
    ui->upLoad_label->setText(tr("批量上传"));  // "BatchUpload"
    ui->lblAdd->setText(tr("新建"));  // "New"

    QFont font;
    if (G_LANGUAGE_CHINESE == CGlobal::language) {
        font.setPointSize(10);
    } else {
        font.setPointSize(9);
    }

    ui->lblTicketPrint->setFont(font);
    ui->bench_label->setFont(font);
    ui->tool_label->setFont(font);
    ui->upLoad_label->setFont(font);
    ui->lblAdd->setFont(font);

    ui->btnMeasure_0->setText(tr("测量"));    // "Measure"
    ui->btnMeasure_1->setText(tr("测量"));    // "Measure"
    ui->btnMeasure_2->setText(tr("测量"));    // "Measure"
    ui->btnMeasure_3->setText(tr("测量"));    // "Measure"
    ui->btnMeasure_4->setText(tr("测量"));    // "Measure"

    ui->btnEdit_0->setText(tr("编辑"));   // "Edit"
    ui->btnEdit_1->setText(tr("编辑"));   // "Edit"
    ui->btnEdit_2->setText(tr("编辑"));   // "Edit"
    ui->btnEdit_3->setText(tr("编辑"));   // "Edit"
    ui->btnEdit_4->setText(tr("编辑"));   // "Edit"

    qDebug()<<"leave WinClinic::languagechange()";
}

int WinClinic::getDataSize()
{
    return mLk.size();
}

void WinClinic::on_firstPageButton_clicked()
{
    tableData.clear();
    tableData = hisMap.firstPage();
    showCurrentPageToTable();
}

void WinClinic::on_lastPageButton_clicked()
{
    tableData.clear();
    tableData = hisMap.lastPage();
    showCurrentPageToTable();
}

bool WinClinic::checkUploadCondition(bool _is_batch)
{
    if (connMode_Http == DataTransmiter::ConnMode) {
        //
        if(!g_WifiIntf->getIsConnected()){
            qDebug()<<"network is disconnect!";

            QString text = tr("网络未连接!");    // "Network is disconnect!"
            getWinManage()->showMsgWin(text);

            return false;
        }

        //
        if(DataTransmiter::ReceiverAddr.length() < 3){
            qDebug()<<"ReceiverAddr is invalid!";

            QString text = tr("请设置正确的接收地址!");   // : "Please set correct IP!" ;
            getWinManage()->showMsgWin(text);

            return false;
        }
    } else if (connMode_Bluetooth == DataTransmiter::ConnMode) {
        if (!g_Bluetooth->getBtDatatrans()->getIsConnected()) {
            qDebug()<<"bluetooth not connected";

            QString text = tr("数据传输的蓝牙未连接");    // : "bluetooth for datatrans not connected" ;
            getWinManage()->showMsgWin(text);

            return false;
        }
    }

    return true;
}

void WinClinic::uploadSelectedRows()
{
    qDebug()<<"upload clicked";

    // 检查上传的条件
    if (!WinClinic::checkUploadCondition(false)) {
        return;
    }

    //
    if(patientidSelected.size()==0){
        QString text = tr("请选择上传的记录");  // : "Please select records";
        getWinManage()->showMsgWin(text);
    } else {
        // 由编号获取 id
        QVector<int> id_list;
        bool is_succ_ids = mysql->getIdsByNums(patientidSelected, id_list, 1);
        if (is_succ_ids) {
            if (id_list.size() > 0) {
                QString text = tr("正在上传...");   // : "Uploading...";
                getWinManage()->showMsgWin(text, false);

                //
                emit sigUpLoadData(id_list);        // TODO: 如果后面逻辑不严谨没有信号回传，这里将不可恢复，看起来像是卡死？
            } else {
                getWinManage()->showSuspensionPrompt(tr("操作失败：没有可上传的数据"));  // "operation failed: no data to upload"
            }
        } else {
            getWinManage()->showSuspensionPrompt(tr("程序错误：获取 id 列表失败！"));   // "Program Error: get id list failed!"
        }
    }

    //
    patientidSelected.clear();
    selectAll(Qt::Unchecked);
    //showCurrentPageToTable();
}
void WinClinic::on_pushButton_upLoad_clicked()
{
    uploadSelectedRows();
}

void WinClinic::slotUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg)
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

bool WinClinic::getUploadFeedbackMsg(const int _count_upload, const int _count_upload_final, const int _count_succ, const QString &_msg_upload_err, QString &_msg_tip)
{
    // 构造提示信息的内容
    int count_filtered = _count_upload - _count_upload_final;
    bool is_succ = false;
    _msg_tip = "";
    if (connMode_Bluetooth != DataTransmiter::ConnMode) {
        if (_count_upload_final > 0) {
            if (_count_succ > 0) {
                _msg_tip += tr("上传成功 %1 条").arg(_count_succ);    // "Upload %1 succeeded"
            }
            int count_failed = (_count_upload_final - _count_succ);
            if (count_failed > 0) {
                if (_msg_tip.length() > 0) {
                    _msg_tip += tr("，");   // ", "
                }
                _msg_tip += tr("上传失败 %1 条。").arg(count_failed);  // "Upload %1 failed."
            } else {
                is_succ = true;
                _msg_tip += tr("。"); // "."
            }
        } else {
            _msg_tip += tr("上传失败!"); // : "Upload failed!";
        }
    } else {                                                            // 蓝牙上传大多未答复，不能知道对方是否接收成功
        is_succ = true;
        _msg_tip += tr("已上传 %1 条记录").arg(_count_upload_final);    // "%1 record uploaded."
    }

    if (count_filtered > 0) {
        _msg_tip += tr("\n%1 条被跳过。").arg(count_filtered);    // "\n%1 skipped."
    }

    if (_msg_upload_err.length() > 0) {
        _msg_tip += ("\n" + tr("错误：") + _msg_upload_err);   // "Error: "
    }

    //
    return is_succ;
}

void WinClinic::showUploadFeedbackMsg(const int _count_upload, const int _count_upload_final, const int _count_succ, const QString &_msg)
{
    // 获取提示消息
    QString msg_tip;
    bool is_succ =  WinClinic::getUploadFeedbackMsg(_count_upload, _count_upload_final, _count_succ, _msg, msg_tip);

    // 弹出提示消息
    bool is_timer_hide_msg = is_succ;                           // 是否定时隐藏消息
    getWinManage()->showMsgWin(msg_tip, !is_timer_hide_msg);

    if (is_timer_hide_msg) {
        QTime _time = QTime::currentTime().addMSecs(1500);
        while(QTime::currentTime() < _time)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1500);
        }
        getWinManage()->hideMsgWin();
    }
}

void WinClinic::setConfig_SortType(enSortType _sort_type)
{
    appSetting::setValue("ui/SortTypeClinic", (int)_sort_type);
}

void WinClinic::slot_mproSysPushSvc_ReceivedOutpatientArchive(Net::Remote::stOutpatientArchive _archive)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    QString msg = tr("接收到一个云端门诊档案消息：");    // "Received a cloud outpatient record notification: "

    // 数据合法性检查
    QString msg_err;
    if (_archive.treatmentNumber.isEmpty()) {               // 诊疗号不可为空
        Util::appendLine(msg_err, tr("错误：") + tr("诊疗号为空！"));    // "Error: ", "OutpatientNumber is empty!"
    }

    if (!msg_err.isEmpty()) {
        msg += '\n' + msg_err;
        msg += '\n' + tr("门诊档案添加失败！");     // "Failed to append outpatient record!"
        getWinManage()->showSuspensionPrompt(msg, -1);
        return;
    }

    // 检查编号是否存在
    std::vector<CPatient> pats = mysql->findRecordByPatientid(_archive.treatmentNumber);
    bool is_exist = (pats.size() > 0);
    if (is_exist) {
        QString msg_question = msg + '\n' + tr("诊疗号\"%1\"已存在！是否更新？")     // "OutpatientNumber \"%1\" already exists! Does it need an update?"
                .arg(_archive.treatmentNumber);
        bool ret = getWinManage()->showNoticeWin(msg_question, tr("是"), tr("否"));    // "Yes", "No"
        if (!ret) {
            logWarning(QString("%1: OutpatientNumber \"%2\"already exists, and user chooses to ignore!").arg(__PRETTY_FUNCTION__).arg(_archive.treatmentNumber));
            return;
        }
    }

    //
    if (!is_exist) {
        // 插入新记录
        CPatient pat;
        pat.reset();

        pat.patientid       = _archive.treatmentNumber;
        pat.patientname     = _archive.name;
        pat.patientsex      = _archive.gender;
        pat.setBirthDate(Util::strToDate(_archive.birthdate));
        pat.patientPhone    = _archive.phone;

        pat.creattime = QDateTime::currentDateTime().toString(CPatient::dateTimeFormat());

        pat.isBatch = false;

        mysql->insertHistory(pat, false);
    } else {
        // 更新旧记录
        for (CPatient &pat : pats) {
            // 须是门诊记录
            if (pat.isBatch) {
                msg_err = tr("现有记录为学校记录，不可修改为门诊记录。") + "\n"
                        // "The existing record is a school record and cannot be modified to become an outpatient record."
                        + tr("若要替换，请先手动删除！");   // "If you want to replace, please manually delete first!"
                getWinManage()->showMsgWin(msg_err);
                return;
            }

            //
            pat.patientid       = _archive.treatmentNumber;
            pat.patientname     = _archive.name;
            pat.patientsex      = _archive.gender;
            pat.setBirthDate(Util::strToDate(_archive.birthdate));
            pat.patientPhone    = _archive.phone;

            pat.creattime = QDateTime::currentDateTime().toString(CPatient::dateTimeFormat());      // NOTE: 档案的创建时间也更新，使档案最前显示
        }
        mysql->TableModify(pats);
    }

    // 提示
    if (!is_exist){
        msg += '\n' + tr("门诊档案 \"%1\" 已添加。").arg(_archive.treatmentNumber);    // "Outpatient record \"%1\" has been appended."
    } else {
        msg = tr("门诊档案 \"%1\" 已更新。").arg(_archive.treatmentNumber);   // "Outpatient record \"%1\" has been updated."
    }
    getWinManage()->showSuspensionPrompt(msg, -1);

    // 更改排序方法，按创建时间倒序排列
    setSortType(enSortType::ByTimeDsc, false);

    ui->cmbSortType->blockSignals(true);
    ui->cmbSortType->setCurrentIndex((int)sortType);
    ui->cmbSortType->blockSignals(false);

    // 刷新数据
    if (this->isVisible()) {
        loadDataToTable();
    }
}

void WinClinic::on_cmbSortType_currentIndexChanged(int _index)
{
    enSortType sort_type_new = (enSortType)_index;
    if (sort_type_new != sortType) {
        //
        setSortType(sort_type_new);

        //
        //ui->cmbSortType->clearFocus();
    }
}

void WinClinic::on_btnMeasure_0_clicked()
{
    measureRow(0);
}

void WinClinic::on_btnMeasure_1_clicked()
{
    measureRow(1);
}

void WinClinic::on_btnMeasure_2_clicked()
{
    measureRow(2);
}

void WinClinic::on_btnMeasure_3_clicked()
{
    measureRow(3);
}

void WinClinic::on_btnMeasure_4_clicked()
{
    measureRow(4);
}

void WinClinic::on_btnEdit_0_clicked()
{
    editRow(0);
}

void WinClinic::on_btnEdit_1_clicked()
{
    editRow(1);
}

void WinClinic::on_btnEdit_2_clicked()
{
    editRow(2);
}

void WinClinic::on_btnEdit_3_clicked()
{
    editRow(3);
}

void WinClinic::on_btnEdit_4_clicked()
{
    editRow(4);
}

void WinClinic::on_btnAdd_clicked()
{
    //
    CPatient pat;
    pat.reset();
    pat.patientid = CWinManage::getNewClinicNum();

    PersonalInfos::showPersonalInfo(PersonalInfos::modeFlag_New, patientSource_Manual, &pat, "");
}

void WinClinic::measureRow(int _row)
{
    CPatient *pat_curr = getPatientByRow(_row, tableData, mLk);
    if (!pat_curr) {
        //getWinManage()->showSuspensionPrompt(msg);
        return;
    }

    // 根据编号查询完整信息
    std::vector<CPatient> pats = mysql->findRecordByPatientid(pat_curr->patientid, -1, enSortType::Unknown, 1);

    // 新增测量
    if (pats.size() > 0) {
        CPatient pat_info = pats.at(0);

        CPatient pat_new;
        pat_new.cloneFrom(*pat_curr);

        pat_new.patientid       = pat_info.patientid       ;
        pat_new.patientname     = pat_info.patientname     ;
        pat_new.patientsex      = pat_info.patientsex      ;
        pat_new.setBirthDate(pat_info.getBirthDate());
        pat_new.patientstuclass = pat_info.patientstuclass ;
        pat_new.patientPhone    = pat_info.patientPhone    ;
        pat_new.patientWechat   = pat_info.patientWechat   ;
        pat_new.patientAddress  = pat_info.patientAddress  ;
        pat_new.setAgeRange(pat_info.getAgeRange());

        pat_new.id = 0;
        pat_new.patienttesttime = "";
        pat_new.creattime = "";

        WinMeasure::setOperationMode(operationMode_NormalMeasure);

        getWinManage()->openMeasureWin(pat_new, patientSource_Database);
    } else {
        getWinManage()->showSuspensionPrompt(tr("内部错误：查找数据失败"));    // "Internal error: Failed to find data"
    }
}

void WinClinic::editRow(int _row)
{
    //
    CPatient *patient_row = getPatientByRow(_row, tableData, mLk);
    if (!patient_row) {
        //getWinManage()->showSuspensionPrompt(msg);
        return;
    }

    //
    std::vector<CPatient> pats = mysql->findRecordByPatientid(patient_row->patientid);
    if (pats.size() > 0) {
        CPatient &pat = pats.at(0);

        bool is_editing = PersonalInfos::showPersonalInfo(PersonalInfos::modeFlag_EditAndSave, patientSource_Database, &pat, "");
        isEditing = is_editing;
    } else {
        logWarning(QString("Error: find record of current row failed!"), CGlobal::LOG_TEMP);
    }
}

void WinClinic::showTableButtons(int _show_rows)
{
    ui->btnMeasure_0->setVisible(_show_rows > 0);
    ui->btnMeasure_1->setVisible(_show_rows > 1);
    ui->btnMeasure_2->setVisible(_show_rows > 2);
    ui->btnMeasure_3->setVisible(_show_rows > 3);
    ui->btnMeasure_4->setVisible(_show_rows > 4);

    ui->btnEdit_0->setVisible(_show_rows > 0);
    ui->btnEdit_1->setVisible(_show_rows > 1);
    ui->btnEdit_2->setVisible(_show_rows > 2);
    ui->btnEdit_3->setVisible(_show_rows > 3);
    ui->btnEdit_4->setVisible(_show_rows > 4);

}
