//批量筛查界面
#include "winscreen.h"
#include "ui_winscreen.h"

#include <QTableWidget>
#include <QDebug>
#include <QModelIndex>
#include <QTableWidgetItem>
#include <QScrollBar>
#include <vector>
#include <QVector>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QMovie>
#include <QCheckBox>
#include <QSignalBlocker>

#include <string>

#include "mainwindow.h"
#include "mysqlitepatients.h"
#include "personalinfos.h"
#include "import.h"
#include "winclinic.h"
#include "noticewin.h"
#include "messagewin.h"
#include "batchuploadlist.h"
#include "windowsmanager.h"
#include "printertransmit.h"
#include "mainwindow.h"
#include "global.h"
#include "util-app.h"
#include "win-guanxin-testee-query.h"
#include "utilui.h"
#include "data-intf-guanxin.h"

using namespace std;

using namespace DataTrans;

//
const char * const WinScreen::S_CLASS_NAME = WinScreen::staticMetaObject.className();

WinScreen::WinScreen(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::WinScreen)
{
    ui->setupUi(this);

    QFont font = qApp->font();

    //QList<QLabel *> qlabel = this->findChildren<QLabel *>();
    //foreach (QLabel *ql, qlabel) {
    //     ql->setFont(font);
    //}

    font.setPointSize(12);
    this->ui->tableWidget->setFont(font);

    isShowStatusBar = true;

    this->ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    //this->ui->tableWidget->setSelectionMode(QAbstractItemView::MultiSelection);     //设置可选中多行
    this->ui->tableWidget->setSelectionMode(QAbstractItemView::NoSelection);    // 设置表格不可选择
    this->ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);      //设置每行内容不可更改
    this->ui->tableWidget->setFocusPolicy(Qt::NoFocus);
    this->ui->tableWidget->horizontalHeader()->setSectionsClickable(false);         //水平方向的头不可点击
    //this->ui->tableWidget->setAlternatingRowColors(true);                         //设置隔一行变一颜色，即：一灰一白
    this->ui->tableWidget->verticalScrollBar()->setHidden(true);
    this->ui->tableWidget->horizontalScrollBar()->setHidden(true);
    this->ui->tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);    //影藏水平滚动条

    ui->pushButton_print->setIconSize(QSize(42,42));

    int col_width_total = ui->tableWidget->width();
    double width_ratio_1 = 0.13, width_ratio_2 = 0.20, width_ratio_3 = 0.13,
           width_ratio_4 = 0.13, width_ratio_5 = 0.15, width_ratio_6 = 0.18;
    ui->tableWidget->setColumnWidth(0, 0);
    ui->tableWidget->setColumnWidth(1, col_width_total * width_ratio_1);
    ui->tableWidget->setColumnWidth(2, col_width_total * width_ratio_2);
    ui->tableWidget->setColumnWidth(3, col_width_total * width_ratio_3);
    ui->tableWidget->setColumnWidth(4, col_width_total * width_ratio_4);
    ui->tableWidget->setColumnWidth(5, col_width_total * width_ratio_5);
    ui->tableWidget->setColumnWidth(6, col_width_total * width_ratio_6);
    ui->tableWidget->setColumnWidth(7, col_width_total * (1 - width_ratio_1 - width_ratio_2 - width_ratio_3 - width_ratio_4 - width_ratio_5 - width_ratio_6));
    /* 不可设置 horizontalHeaderStretchLastSection ，因为这样之后第一列不可隐藏 */

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

    int btn_upload_x = ui->btnUpload_0->x();
    int y_dif = (ui->checkBox_0->height() - ui->btnUpload_0->height()) / 2;
    ui->btnUpload_0->move(btn_upload_x, ui->checkBox_0->y() + y_dif);
    ui->btnUpload_1->move(btn_upload_x, ui->checkBox_1->y() + y_dif);
    ui->btnUpload_2->move(btn_upload_x, ui->checkBox_2->y() + y_dif);
    ui->btnUpload_3->move(btn_upload_x, ui->checkBox_3->y() + y_dif);
    ui->btnUpload_4->move(btn_upload_x, ui->checkBox_4->y() + y_dif);

    //QObject::connect(ui->tableWidget, &QTableWidget::doubleClicked, this, &WinScreen::on_tableWidget_clicked);
    //QObject::connect(ui->Allselection, SIGNAL(stateChanged(int)), this, SLOT(onStateChanged(int)));                 //zy
    QObject::connect(ui->checkBox_0, &QCheckBox::stateChanged, this, &WinScreen::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_1, &QCheckBox::stateChanged, this, &WinScreen::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_2, &QCheckBox::stateChanged, this, &WinScreen::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_3, &QCheckBox::stateChanged, this, &WinScreen::onRowCheckBoxStateChanged);
    QObject::connect(ui->checkBox_4, &QCheckBox::stateChanged, this, &WinScreen::onRowCheckBoxStateChanged);          //zy
    //QObject::connect(ui->pushButton_photo, SIGNAL(sendSIGNAL(int)), this, SLOT(signalHandler(int)));          //tupian

    iconUpload = QIcon(":/resource/black_theme/cell-icon-upload_b.png");
    iconUploaded = QIcon(":/resource/black_theme/cell-icon-uploaded_b.png");

    this->sortType = (enSortType)(appSetting::value("ui/SortTypeScreen", (int)this->sortType).toInt());

    mysql = MySQLitePatients::getInstance();

    ui->btnSearch->raise();

    barcodeMode = false;

    if (CGlobal::isReadBarcodeByQt)
        QObject::connect(&readBarcode,SIGNAL(timeout()),this,SLOT(barcodeHandle()));

    mMovie = new QMovie(":/resource/loading.gif");
    const int MOVIE_WIDTH = 128;
    loading = new QLabel(this);
    loading->setMovie(mMovie);
    loading->setGeometry((SCREEN_WIDTH - MOVIE_WIDTH) / 2, (SCREEN_HEIGHT - MOVIE_WIDTH) / 2, MOVIE_WIDTH, MOVIE_WIDTH);
    loading->hide();

    ui->lblPageNum->setText("0 / 0");

    //add end
}

WinScreen::~WinScreen()
{
//    input->close();
    delete ui;
}

void WinScreen::updateTheme()
{
    //
    static QString style_sheet_black;
    static bool style_black_readed = false;
    if (!style_black_readed) {
        Util::readFileToQStr(":/resource/qss/winscreen.qss", style_sheet_black);
        style_black_readed = true;
    }

    //
    //QPalette palette;
    if(themeType_Black == getSysThemeType()){
        this->setStyleSheet(style_sheet_black);

        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));

        //
        ui->pushButton_print->setIcon(QIcon(":/resource/black_theme/print-ticket_small_b.png"));
        ui->pushButton_port->setIcon(QIcon(":/resource/black_theme/import-export_small_b.png"));
        ui->pushButton_delete->setIcon(QIcon(":/resource/black_theme/delete_small_b.png"));
        ui->pushButton_upLoad->setIcon(QIcon(":/resource/black_theme/batch-upload_small_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));

    }
    else{
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        //
        ui->pushButton_print->setIcon(QIcon(":/resource/white_theme/print_small_w.png"));
        ui->pushButton_port->setIcon(QIcon(":/resource/white_theme/export_w.png"));
        ui->pushButton_delete->setIcon(QIcon(":/resource/white_theme/rmove_w.png"));
        ui->pushButton_upLoad->setIcon(QIcon(":/resource/white_theme/uploading_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));

        ui->history_label->setStyleSheet("color:rgb(1,1,1);");
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
        ui->btnBatchImport->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
        ui->btnSearch->setStyleSheet("QPushButton{background-color:rgb(225,225,230); color:rgb(1,1,1); border-radius:5px;}");
        ui->edtSearch->setStyleSheet("QLineEdit{background-color:rgb(225,225,230); color:rgb(140,140,145); border-radius:5px;}");
        ui->lblPageNum->setStyleSheet("color:rgb(1,1,1);");
    }
    //this->setPalette(palette);
    //this->setAutoFillBackground(true);

}

void WinScreen::showEvent(QShowEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {               // TODO: 这个已经没用了？
        return;
    }

    // 注册键盘侦听（用于扫码）
    globalService()->regKbReader(this);

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 更新控件的样式和图标
    updateTheme();
    qDebug() << "paint WinScreen:" << getSysThemeType();

    // 更新语言
    languagechange();

    //
    ui->cmbSortType->blockSignals(true);
    ui->cmbSortType->setCurrentIndex((int)sortType);
    ui->cmbSortType->blockSignals(false);

    // 载入数据到表格
    loadDataToTable();

    // 启用各个按钮
    slotEnableViewObject(true);

}

void WinScreen::hideEvent(QHideEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 反注册键盘侦听（用于扫码）
    globalService()->unregKbReader(this);

    //
    idSelected.clear();
    ui->edtSearch->setText("");
    ui->btnSearch->setIcon(QIcon(":/resource/fangdajing.png"));

    selectAll(Qt::Unchecked);
    slotEnableViewObject(true);
}

void WinScreen::slotShowWarningMsg(QString _msg)
{
    getWinManage()->showMsgWin(_msg);

    return;
}

void WinScreen::on_pushButton_Back_clicked()
{
    getWinManage()->showWindowByType(WIN_HOME);
}

void WinScreen::on_tableWidget_clicked(const QModelIndex &_index)
{
    qDebug()<<"-----on_tableWidget_clicked---";

    //
    if (!this->isVisible()) {
        return;
    }

    // 显示测量结果
    int currentRow = _index.row();

    if (currentRow > tableData.count() - 1) {
        return;
    }

    QTableWidgetItem *item_patientid = ui->tableWidget->item(currentRow, 1);
    QTableWidgetItem *item_age_range = ui->tableWidget->item(currentRow, 5);

    int dataset_row = tableData.at(currentRow);
    if (dataset_row > (int)mLk.size() - 1) {
        return;
    }

    CPatient pat = mLk.at(dataset_row);
    qDebug()<<"achievesqlrow = "<<dataset_row;
    qDebug()<<"patient.ID ="<<pat.id;
    bool istest = pat.isTest;

    if (item_patientid != 0 && item_age_range != 0)
    {
        if (istest)//如果已测，则隐藏批量筛查界面打开结果界面
        {            
            WinMeasure::setOperationMode(operationMode_BatchRecord);

            Result *win_result =  globalService()->getResultWin();

            win_result->setIsNeedSave(false);
            win_result->setHistoryListPtr(&mLk);

            win_result->setPatient(pat);
            //win_result->setModeFlags();
            win_result->setIsNeedSave(false);
            getWinManage()->showWindow(win_result);
            //emit sendSIGNAL(sysSignal_18);
            //this->hide();
        }
        else      //如果未测隐藏批量筛查界面打开被测者信息界面
        {
            WinMeasure::setOperationMode(operationMode_BatchScreen);             // 根据数据对象显示被测者信息    // TODO: 封装成共用函数？

            PersonalInfos::showPersonalInfo(PersonalInfos::modeFlag_EditAndTest, patientSource_Database, &pat, "");
        }
    }
}

void WinScreen::slotEnableViewObject(bool enable)
{
    ui->pushButton_print->setEnabled(enable);
    ui->pushButton_port->setEnabled(enable);
    ui->pushButton_delete->setEnabled(enable);
    ui->pushButton_upLoad->setEnabled(enable);
    ui->tableWidget->setEnabled(enable);
}

void WinScreen::slotPhysicButtonPressed()
{
    // 若本窗体不是当前窗体，则不处理此信号
    if (this != getWinManage()->getCurrentWin()) {
        return;
    }

    /* 按下物理按键，等效于点击当前页首行开始之后的第一个未测行 */

    // 得到首行的数据集索引号
    int first_dataset_row = -1;
    if (tableData.size() > 0) {
        first_dataset_row = tableData.at(0);
        if (first_dataset_row > (int)mLk.size() - 1) {
            return;
        }
    }
    if (first_dataset_row < 0) {
        return;
    }

    // 在数据集中往下查找第一个未测对象
    int idx_fist_not_test = -1;
    for (size_t i = first_dataset_row; i < mLk.size(); i++) {
        CPatient &pat = mLk.at(i);
        if (!pat.isTest) {
            idx_fist_not_test = i;
            break;
        }
    }
    if (idx_fist_not_test < 0) {
        return;
    }

    // 表格翻页到该对象
    tableData.clear();
    bool succ_page_turning = batchMap.gotoPageByIndex(idx_fist_not_test, tableData);
    if (succ_page_turning) {
        showCurrentPageToTable();
    } else {
#if (OS_TYPE == 2)
        getWinManage()->showSuspensionPrompt(QString("Internal err: page turning by data index %1 failed").arg(idx_fist_not_test));
#endif
        return;
    }

    // 跳转到该对象的被测者信息页面
    CPatient &pat_not_test = mLk.at(idx_fist_not_test);

    WinMeasure::setOperationMode(operationMode_BatchScreen);             // 根据数据对象显示被测者信息    // TODO: 封装成共用函数？

    PersonalInfos::showPersonalInfo(PersonalInfos::modeFlag_EditAndTest, patientSource_Database, &pat_not_test, "");
}

void WinScreen::on_btnPrevious_clicked()
{
    tableData.clear();
    tableData = batchMap.previousPage();
    showCurrentPageToTable();
}

void WinScreen::on_btnNext_clicked()
{
    tableData.clear();
    tableData = batchMap.nextPage();
    showCurrentPageToTable();
}

void WinScreen::delete_clicked()   //删除
{
    QString text = tr("是否删除？");     // "Delete data?"
    bool ret = getWinManage()->showNoticeWin(text);
    if(ret)
        messagedelete();
}

void WinScreen::on_pushButton_delete_clicked()
{
    delete_clicked();
}

void WinScreen::deleteFile()
{
    if(idSelected.size() == 0)
        return;

    QDir pdfDir(PDF_REPORT_DIR);        // TODO: 使用 UtilApp::deletexxx() ？参考 winclinic.cpp
    if(pdfDir.exists())
    {
        for(int i=0;i<idSelected.size();i++)
        {
            int id = idSelected.at(i);
            const CPatient *pat = MySQLitePatients::getPatientFromListById(mLk, id);
            if (pat && pat->isTest) {
                UtilApp::deletePdfFilesOfPatient(*pat);
            } else {
                logWarning(QString("get data obj of id %1 failed!, delete pdf file failed!").arg(pat->id).arg(pat->patientid));
            }
        }
    }
    else
        pdfDir.mkdir(pdfDir.path());

    QDir imgDir("/media/pdfPreviewImg/");
    if(imgDir.exists())
    {
        for(int i=0;i<idSelected.size();i++)
        {
            int id = idSelected.at(i);
            const CPatient *pat = MySQLitePatients::getPatientFromListById(mLk, id);
            if (pat && pat->isTest) {
                UtilApp::deletePreviewImagesOfPatient(*pat);
            } else {
                logWarning(QString("get data obj of id %1 failed!, delete preview image failed!").arg(pat->id).arg(pat->patientid));
            }
        }
    }
    else
        pdfDir.mkdir("/media/pdfPreviewImg/");

    // TODO: 还有 photo 目录没有删

}

void WinScreen::uploadRow(int _row)
{
#if OS_TYPE != 2
    // 检查上传的条件
    if (!WinClinic::checkUploadCondition(true)) {
        return;
    }
#endif

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
    emit sigUpLoadData(id_list);                        // TODO: 如果后面逻辑不严谨没有信号回传，这里将不可恢复，看起来像是卡死？

    //
    //qDebug() << __PRETTY_FUNCTION__ << ": sleeping in thread " << reinterpret_cast<quintptr>(QThread::currentThreadId());
    //QThread::msleep(10000);

}

void WinScreen::showTableButtons(int _show_rows)
{
}

void WinScreen::clearUploadButtons()
{
    ui->btnUpload_0->setVisible(false);
    ui->btnUpload_1->setVisible(false);
    ui->btnUpload_2->setVisible(false);
    ui->btnUpload_3->setVisible(false);
    ui->btnUpload_4->setVisible(false);
}

void WinScreen::messagedelete()
{
    if (mLk.size() == 0) {
        return;
    }

    QString text = tr("正在删除...");   // "Deleting data..."
    getWinManage()->showMsgWin(text,false);

    qDebug()<<"start delete...";
    qApp->processEvents();

    mysql->TableDelete(idSelected);
    deleteFile();       //删除对应图像和个人报告

    getWinManage()->hideMsgWin();

    idSelected.clear();
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

void WinScreen::showCurrentPageToTable()
{
    //qDebug() << __PRETTY_FUNCTION__;

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
        if (batchhistory == 2) {
            //qDebug()<<"=========batchhistory == 2";
            tableData.clear();
            tableData = batchMap.currentPage();
        } else if (batchhistory == 4) {
            tableData.clear();
            tableData = batchMap.lastPage();
        }

        batchhistory = 0;

        //qDebug()<<"--tableData:"<<tableData;
        if (tableData.size()==0) {
            break;
        }

        //
        for (int i = 0; i < tableData.size(); i++) {
            if (i > 5)
                break;

            int m = tableData.at(i);
            CPatient pat = mLk.at(m);
            int id = pat.id;
            QString patientid = pat.patientid;
            QString name = pat.patientname;
            QString banji = pat.patientstuclass;
            QString testtime = QDateTime::fromString(pat.patienttesttime, CPatient::dateTimeFormat()).toString("yy/MM/dd HH:mm");
            QString age_desc = CAgeRange::getAgeRangeDesc(pat.getAgeRange());
            QString sex = pat.getSexDiscAbbr();

            this->ui->tableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(id)));
            this->ui->tableWidget->setItem(i, 1, new QTableWidgetItem(patientid));
            this->ui->tableWidget->setItem(i, 2, new QTableWidgetItem(name));
            this->ui->tableWidget->setItem(i, 3, new QTableWidgetItem(sex));
            this->ui->tableWidget->setItem(i, 4, new QTableWidgetItem(banji));
            this->ui->tableWidget->setItem(i, 5, new QTableWidgetItem(age_desc));
            this->ui->tableWidget->setItem(i, 6, new QTableWidgetItem(testtime));

            QPushButton *btn = getUploadButton(i);
            if (btn) {
                btn->setVisible(pat.isTest);
                if (pat.isUploaded) {
                    //this->ui->tableWidget->setItem(i, 7, new QTableWidgetItem(iconUploaded, ""));
                    btn->setIcon(iconUploaded);
                } else {
                    //this->ui->tableWidget->setItem(i, 7, new QTableWidgetItem(iconUpload, ""));
                    btn->setIcon(iconUpload);
                }
            } else {
                // logCritical();
            }

            ui->tableWidget->item(i,1)->setTextAlignment(Qt::AlignCenter);      // 【筛查号】居中
            ui->tableWidget->item(i,2)->setTextAlignment(Qt::AlignCenter);      // 【姓名】居中
            ui->tableWidget->item(i,3)->setTextAlignment(Qt::AlignCenter);      // 【性别】居中
            ui->tableWidget->item(i,4)->setTextAlignment(Qt::AlignCenter);      // 【班级】居中
            ui->tableWidget->item(i,5)->setTextAlignment(Qt::AlignCenter);      // 【年龄段】居中
            ui->tableWidget->item(i,6)->setTextAlignment(Qt::AlignCenter);      // 【测量时间】居中
            //ui->tableWidget->item(i,7)->setTextAlignment(Qt::AlignCenter);      // 【上传状态】居中

            bool checkboxState =  idSelected.contains(id);
            switch (i) {
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
        }

    } while (false);

    // 页码
    QString page_num_str = QString("%1 / %2").arg(batchMap.getCurrentPageNum()).arg(batchMap.getTotalPageNum());
    ui->lblPageNum->setText(page_num_str);

    // 表格按钮行数同步
    showTableButtons(tableData.size());

    //
    //qDebug() << "leave " << __FUNCTION__ << "()";
}

QPushButton *WinScreen::getUploadButton(int _row)
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

void WinScreen::onRowCheckBoxStateChanged(int _state)
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

void WinScreen::selectAll(int _state)
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

void WinScreen::on_Allselection_stateChanged(int arg1)
{
    selectAll(arg1);
}

void WinScreen::port_clicked()
{
    QString udisk_path = Util::CUDisk::getPath();
    bool isshow = (udisk_path.length() > 0);
    qDebug()<<"find upan,isshow ="<<isshow;

//    for (int i = 0; i < 4; i++) {
//        if (commandShow(command[i])) {
//            isshow = true;
//            break;
//        }
//    }
    if (isshow) {
        import win_import(this, idSelected, udisk_path, true);
        win_import.setModal(true);
 
        slotEnableViewObject(false);
        win_import.exec();
        slotEnableViewObject(true);

        selectAll(Qt::Unchecked);
    } else {
        showMessage();
    }
    //selectAll(Qt::Unchecked);
}

void WinScreen::on_pushButton_port_clicked()
{
    port_clicked();
}

bool WinScreen::commandShow(QString cmd)
{
    QProcess *p = new QProcess;
    p->start(cmd);
    p->waitForFinished(3000);
    QByteArray out = p->readAllStandardError();

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

void WinScreen::showMessage()
{
    QString text = tr("未检测到U盘!");   // "U disk was not detected!"
    getWinManage()->showMsgWin(text);
}

void WinScreen::slotRefresh()
{
    loadDataToTable();
}

void WinScreen::print_clicked()
{
    ui->pushButton_print->setEnabled(false);
    if(idSelected.size()==0){
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
            QString text = tr("打印机未连接！");   // "Printer disconnect!"
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

void WinScreen::GetPrintPara()
{
    std::sort(idSelected.begin(), idSelected.end());

    std::vector<CPatient> pats = mysql->findRecordByIdList(idSelected);

    WinMeasure::setOperationMode(operationMode_BatchRecord);
    emit batchPrintSig(pats);

    selectAll(Qt::Unchecked);
    idSelected.clear();
    ui->pushButton_print->setEnabled(true);
}

bool WinScreen::getNextNotMeasured(int _curr_id, CPatient &_pat, QString &_err_msg)
{
    int idx_curr = -1;
    for (uint i = 0; i < mLk.size(); i++) {
        if (mLk.at(i).id == _curr_id) {
            idx_curr = i;
            break;
        }
    }
    if (idx_curr < 0) {
        // 找不到
        _err_msg = tr("逻辑错误：找不到当前实体");  // "Logic error: Unable to find the current entity"
    } else {
        if (idx_curr == (int)mLk.size() - 1) {
            // 是最后一个
            _err_msg = tr("已经是最后一个");   // "It's already the last one"
        } else {
            // 在数据集中往下查找第一个未测对象
            int idx_fist_not_test = -1;
            for (size_t i = idx_curr + 1; i < mLk.size(); i++) {
                CPatient &pat = mLk.at(i);
                if (!pat.isTest) {
                    idx_fist_not_test = i;

                    _pat.cloneFrom(pat);

                    return true;
                }
            }

            if (idx_fist_not_test < 0) {
                _err_msg = tr("找不到下一个未测者"); // "Unable to find the next unmeasured person"
            }
        }
    }

    return false;
}

void WinScreen::setConfig_SortType(enSortType _sort_type)
{
    appSetting::setValue("ui/SortTypeScreen", (int)_sort_type);
}

void WinScreen::on_pushButton_print_clicked()
{
    print_clicked();
}

int WinScreen::loadDataToTable()
{
    //
    mLk.clear();
    mLk = mysql->getInfoForBatch(true, sortType);
    qDebug() << "--WinScreen:mLk.size() = " << mLk.size();

    //
    batchMap.clear();
    batchMap.loadTableData(mLk.size());
    tableData.clear();
    tableData = batchMap.currentPage();

    //
    showCurrentPageToTable();

    //
    return mLk.size();
}

void WinScreen::on_btnSearch_clicked()
{
    // 若搜索文本不为空，则清空
    const QString &key_word = ui->edtSearch->text();
    if (!key_word.isEmpty()) {
        ui->edtSearch->setText("");
    }
}

void WinScreen::on_edtSearch_textChanged(const QString &_arg1)
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

void WinScreen::doSearch()
{
    const QString &key_word = ui->edtSearch->text();
    if (!key_word.isEmpty()) {
        mLk.clear();
        mLk = mysql->findTableInfo(1, key_word, sortType);
        if (mLk.size() == 0 || key_word == "")
        {
            MessageWin msg;
            msg.setWindowModality(Qt::ApplicationModal);        // 阻塞msg以外的所以窗体
            msg.setContent(tr("未搜索到任何记录！!"));  // "No records found!"
            msg.exec();

            //
            ui->edtSearch->setText("");
        }
        else
        {
            //
            batchMap.clear();
            batchMap.loadTableData(mLk.size());
            tableData.clear();
            tableData = batchMap.firstPage();
        }

        selectAll(Qt::Unchecked);
        showCurrentPageToTable();
    }
    else{
        loadDataToTable();
    }
}

void WinScreen::setSortType(enSortType _sort_type, bool _need_reload)
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

void WinScreen::languagechange()
{
    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("批量筛查"));    // "Screen Records"

    //
    //if (language) {
    //    ui->tableWidget->setHorizontalHeaderLabels(QStringList()<<"id"<<"编号"<<"姓名"<<"性别"<<"班级"<<"年龄段"<<"测量时间"<<"上传");
    //    ui->cmbSortType->setItemText(0, "按时间升序");
    //    ui->cmbSortType->setItemText(1, "按时间降序");
    //    ui->cmbSortType->setItemText(2, "按编号升序");
    //    ui->cmbSortType->setItemText(3, "按编号降序");
    //    ui->edtSearch->setPlaceholderText("搜索");
    //    ui->Allselection->setText("全  选");
    //    ui->home_label->setText("返回");
    //    ui->history_label->setText("小票打印");
    //    ui->bench_label->setText("导入/导出");
    //    ui->tool_label->setText("删除");
    //    ui->upLoad_label->setText("批量上传");
    //    ui->btnBatchImport->setText("名单获取");
    //
    //} else {
    //    ui->tableWidget->setHorizontalHeaderLabels(QStringList()<<"id"<<"No."<<"Name"<<"Sex"<<"Class"<<"AgeRange"<<"TestTime"<<"Upload");
    //    ui->cmbSortType->setItemText(0, "Ascending by Time");
    //    ui->cmbSortType->setItemText(1, "Descending by Time");
    //    ui->cmbSortType->setItemText(2, "Ascending by ID");
    //    ui->cmbSortType->setItemText(3, "Descending by ID");
    //    ui->edtSearch->setPlaceholderText("Search");
    //    ui->Allselection->setText("All  ");
    //    ui->home_label->setText("Back");
    //    ui->history_label->setText("TicketPrint");
    //    ui->bench_label->setText("Export\nImport");
    //    ui->tool_label->setText("Delete");
    //    ui->upLoad_label->setText("BatchUpload");
    //    ui->btnBatchImport->setText("ListFetch");
    //}

    //
    if (dataInterfaceCfg_GuanXin != WinDataTrans::getCfg_intfType()) {
        ui->btnBatchImport->setText(tr("批量导入").replace(' ', '\n'));        // "Batch Import"        // NOTE: QPushButton 不像 QLabel 那样能自动换行，只能这样添加换行
    } else {
        ui->btnBatchImport->setText(tr("名单获取").replace(' ', '\n'));        // "List Fetch"          // NOTE: QPushButton 不像 QLabel 那样能自动换行，只能这样添加换行
    }

    //
    QFont font;
    if (G_LANGUAGE_CHINESE == CGlobal::language) {
        font.setPointSize(10);
    } else {
        font.setPointSize(9);
    }

    ui->history_label->setFont(font);
    ui->bench_label->setFont(font);
    ui->tool_label->setFont(font);
    ui->upLoad_label->setFont(font);

}

void WinScreen::on_firstPageButton_clicked()
{
    tableData.clear();
    tableData = batchMap.firstPage();
    showCurrentPageToTable();
}

void WinScreen::on_lastPageButton_clicked()
{

    tableData.clear();
    tableData = batchMap.lastPage();

    showCurrentPageToTable();
}

void WinScreen::keyPressEvent(QKeyEvent *event)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
    if (CGlobal::isReadBarcodeByQt) {
        if(event->key()==Qt::Key_Enter || event->key()==Qt::Key_Return){
            qDebug()<<"--get Key_Enter or Key_Return!";
            readBarcode.stop();
            barcodeHandle();
        }
        else if(event->text()!=""){
            //qDebug()<<event->text();
            barcodeData.append(event->text());
            if(!barcodeMode){
                barcodeMode = true;
                qDebug()<<"barcodeMode = true";
                readBarcode.start(800);
                showLoading(true);
            }
        }
    }

/*
    QDir dir("/media/cut");
    if(!dir.exists()){
        dir.mkdir("/media/cut");
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow(0);
    QString filePathName = "/media/cut/cut-";
    filePathName += QTime::currentTime().toString(Qt::ISODate);
    filePathName += ".png";

    if(!pixmap.save(filePathName,"png"))
    {
        qDebug()<<"cut save png failed"<<endl;
    }
    */
}

void WinScreen::showLoading(bool state)
{
    if(state){
        loading->show();
        mMovie->start();
    }
    else{
        mMovie->stop();
        loading->hide();
    }

}


void WinScreen::barcodeHandle()
{
    qDebug()<<"barcode read:"<<barcodeData;

    //
    if (CGlobal::isReadBarcodeByQt) {
        showLoading(false);
        readBarcode.stop();
    }

    // 条码数据的读取和解析
    const int MIN_BARCODE_LEN = 2;
    bool is_valid  = (barcodeData.length() > MIN_BARCODE_LEN
            //&& (barcodeData[barcodeData.length() - 1] == QChar('\r') || barcodeData[barcodeData.length() - 1] == QChar('\n'))
            );
    if (is_valid) {
        globalService()->doOn_QrCode_ReceivedCode(barcodeData.toUtf8());
    } else {
        getWinManage()->showSuspensionPrompt(tr("二维码内容为空！"));    // "The QR code content is empty!"
    }

    // 使用完后重置条码数据缓冲区
    barcodeData.clear();
    barcodeMode = false;
}

void WinScreen::uploadSelectedRows()
{
#if OS_TYPE != 2
    // 检查上传的条件
    if (!WinClinic::checkUploadCondition(true)) {
        return;
    }
#endif

    //
    if(idSelected.size()==0){
        QString text = tr("请选择上传的记录");  // "Please select records"
        getWinManage()->showMsgWin(text);
    } else {
        QString text = tr("正在上传...");   // "Uploading..."
        getWinManage()->showMsgWin(text, false);

        //
        emit sigUpLoadData(idSelected);

        //
        idSelected.clear();
        selectAll(Qt::Unchecked);
    }

    /*
    batchUploadList batchUpload(this);
    if(QDialog::Accepted==batchUpload.exec()){
        qDebug()<<"QDialog::Accepted==batchUpload.exec()";
        int size = batchUploadList::batchList.size();
        if(size>0){
            qDebug()<<"batchList.size()"<<size;
            for(int i=0;i<size;i++){
                QString str = batchUploadList::batchList.at(i);
                qDebug()<<"-----findTableInfobyBatchNo:"<<str;
                if(str=="无批号"||str=="No batchNo"){
                    qDebug()<<"str==无批号,set to null";
                    str = "";
                }

                mLk.clear();
                mLk = mysql->findTableInfobyBatchNo(str);
                if(mLk.size()>0)
                    batchUpLoad(mLk);
                else
                    qDebug()<<"findTableInfobyBatchNo size="<<mLk.size();
            }
        }
    }
    batchUploadList::clearList();
    */
}

void WinScreen::on_pushButton_upLoad_clicked()
{
    uploadSelectedRows();
}

void WinScreen::slotUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg)
{
    if (WinMeasure::isOpened())
        return;
    if (!this->isVisible())
        return;

    // 显示上传反馈消息
    WinClinic::showUploadFeedbackMsg(_count_upload, _count_upload_final, _count_succ, _msg);

    //
    batchhistory = 2;
    loadDataToTable();
}

void WinScreen::slot_mproSysPushSvc_ReceivedOutpatientArchive(Net::Remote::stOutpatientArchive _archive)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    QString msg = tr("接收到一个云端学校档案消息：");    // "Received a notification of school record from the cloud: "

    // 数据合法性检查
    QString msg_err;
    if (_archive.treatmentNumber.isEmpty()) {               // 编号不可为空
        Util::appendLine(msg_err, tr("错误：") + tr("编号为空！"));    // "Error: ", "Number is empty!"
    }

    if (!msg_err.isEmpty()) {
        msg += '\n' + msg_err;
        msg += '\n' + tr("学校档案添加失败！");     // "Failed to append school record!"
        getWinManage()->showSuspensionPrompt(msg, -1);
        return;
    }

    // 检查编号是否存在
    std::vector<CPatient> pats = mysql->findRecordByPatientid(_archive.treatmentNumber);
    bool is_exist = (pats.size() > 0);
    if (is_exist) {
        QString msg_question = msg + '\n' + tr("编号\"%1\"已存在！是否更新？")     // "Number \"%1\" already exists! Does it need an update?"
                .arg(_archive.treatmentNumber);
        bool ret = getWinManage()->showNoticeWin(msg_question, tr("是"), tr("否"));    // "Yes", "No"
        if (!ret) {
            logWarning(QString("%1: Number \"%2\"already exists, and user chooses to ignore!").arg(__PRETTY_FUNCTION__).arg(_archive.treatmentNumber));
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

        pat.isBatch = true;

        mysql->insertHistory(pat, false);
    } else {
        // 更新旧记录
        for (CPatient &pat : pats) {
            // 须是筛查记录
            if (!pat.isBatch) {
                msg_err = tr("现有记录为门诊记录，不可修改为学校记录。") + "\n"
                        // "The existing record is a outpatient record and cannot be modified to become an school record."
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
        msg += '\n' + tr("学校档案 \"%1\" 已添加。").arg(_archive.treatmentNumber);    // "School record \"%1\" has been appended."
    } else {
        msg = tr("学校档案 \"%1\" 已更新。").arg(_archive.treatmentNumber);   // "School record \"%1\" has been updated."
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

void WinScreen::on_btnBatchImport_clicked()
{
    //
    if (connMode_Http == DataTransmiter::ConnMode) {
        if(!g_WifiIntf->getIsConnected()){
            qDebug()<<"network is disconnect!";

            QString text = tr("网络未连接!");    // "Network is disconnect!"
            getWinManage()->showMsgWin(text);

            return;
        }
    }

    //
    if (dataInterfaceCfg_GuanXin == WinDataTrans::getCfg_intfType()) {
        //
        static WinGuanXinTesteeQuery *win = nullptr;
        if (!win) {
            win = new WinGuanXinTesteeQuery(this);
            win->setParent(this);
            Util::Ui::centerWidget(win);
        }

        // 弹出名单查询窗口
        int ret = win->exec();
        if (QDialog::Accepted == ret) {
            //
            Entity::ETesteeQueryRequest testee_request;
            win->getUiData(testee_request);

            // 数据查询
            CDataIntfGuanXin data_intf(networkManager(), nullptr);
            stGuanXinIntfCfg cfg;
            WinDataTrans::getGuanXinIntfCfg(cfg);
            data_intf.setConfig(cfg);
            QString err_msg;

            Entity::ETesteeQueryResponse testee_response;
            bool succ = data_intf.queryTesteeList(testee_request, testee_response, err_msg);
            if (succ) {
                // 保存到数据库
                std::vector<CPatient> pats;
                for (int i = 0; i < testee_response.data.size(); i++) {
                    const Entity::ETesteeInfo testee = testee_response.data.at(i);

                    CPatient pat;
                    pat.patientid = testee.PatientID ;
                    pat.patientname = testee.Name;
                    pat.setSexFromDisc(testee.Sex);
                    pat.setBirthDateStr(Util::strToDate(testee.Birthday).toString(CPatient::birthDateFormat()));
                    pat.creattime = QDateTime::fromString(testee.Time, "yyyy-MM-dd HH:mm:ss").toString(CPatient::dateTimeFormat());
                    pat.comment1 = testee_request.areaCode; // NOTE: = testee.HISCODE ?
                    pat.Comment2 = testee.id;

                    pat.isBatch = true;

                    pats.push_back(pat);
                }

                int count_repeated = 0;
                if (pats.size() > 0) {
                    MySQLitePatients *mysql = MySQLitePatients::getInstance();
                    mysql->TableBatchAdd(pats, &count_repeated);
                }

                //
                QString msg = tr("获取到 %1 个被测者信息。").arg(testee_response.data.size());    // "%1 testees information fetched."
                if (count_repeated > 0) {
                    msg += "\n" + tr("%1 个已存在。").arg(count_repeated);       // "%1 is exists."
                }
                getWinManage()->showMsgWin(msg);

                // 列表刷新
                loadDataToTable();
            } else {
                QString msg = tr("获取名单失败！");    // "Failed to fetch testee list!"
                msg = msg + "\nError: " + err_msg;
                getWinManage()->showMsgWin(msg);
            }
        }
    } else {
        //
        emit batchImportSig();

        //
        QString text = tr("正在导入...");   // "Importing..."
        getWinManage()->showMsgWin(text, true, "Cancel", -1, true);

    }
}

void WinScreen::slot_batchImportFeedback(QString log)
{
    qDebug()<<"slot_batchImportFeedback:"<<log;
    qDebug()<<"Clients::mFlen:"<<Clients::mFlen;
    if (log == "succ") {
        QString text = tr("导入已完成！");    // "Import complete!"
        getWinManage()->showMsgWin(text);

        loadDataToTable();
    } else if(log == "fail"){
        QString text = tr("导入失败："); // "Import failed: "
        text.append(log);
        getWinManage()->showMsgWin(text);
    } else {
        getWinManage()->showMsgWin(log);
    }

}

void WinScreen::on_cmbSortType_currentIndexChanged(int _index)
{
    enSortType sort_type_new = (enSortType)_index;
    if (sort_type_new != sortType) {
        //
        setSortType(sort_type_new);

        //ui->cmbSortType->clearFocus();
    }
}

void WinScreen::on_btnUpload_0_clicked()
{
    uploadRow(0);
}

void WinScreen::on_btnUpload_1_clicked()
{
    uploadRow(1);
}

void WinScreen::on_btnUpload_2_clicked()
{
    uploadRow(2);
}

void WinScreen::on_btnUpload_3_clicked()
{
    uploadRow(3);
}

void WinScreen::on_btnUpload_4_clicked()
{
    uploadRow(4);
}
