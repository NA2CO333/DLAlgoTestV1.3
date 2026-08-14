#include "import.h"
#include "ui_import.h"

#include <QThread>
#include <QDebug>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QCheckBox>
//#include <QFileDialog>

#include "mainwindow.h"
#include "mysqlitepatients.h"
#include "winscreen.h"
#include "threadmodel.h"
#include "winclinic.h"
#include "messagewin.h"
#include "windowsmanager.h"
#include "global.h"
#include "dialoglistselect.h"

import::import(QWidget *parent, QVector<int> _selectedIds, QString _udisk_path, bool _is_batch) :
    CBaseDialog(parent),
    ui(new Ui::import)
{
    qDebug()<<"enter export mode";

    ui->setupUi(this);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setGeometry(150,90,500,300);

    //
    selectedIds = _selectedIds;
    udiskPath = _udisk_path;

    //
    ui->btnImportBatch->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);color:black;");
    ui->btnExportBatch->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);color:black;");
    ui->btnExportHistory->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);color:black;");
    ui->btnSelectExport->setStyleSheet("border-radius:6px;padding:2px 4px;background-color:rgb(250,250,250);color:black;");

    //if (language) {
    //     ui->btnSelectExport->setText("导出...");
    //     ui->btnImportBatch->setText("导入...");
    //     ui->btnExportBatch->setText("导出");
    //     ui->btnExportHistory->setText("导出");
    //     ui->contextLabel->setText("请选择需要的操作：");
    //     ui->sheetCheckBox->setText("筛查结果");
    //     ui->pdfCheckBox->setText("个人报告");
    //     ui->templateCheckBox->setText("表格模板");
    //     //result->setText("筛查结果");
    //     //pdfFile->setText("个人报告");
    //} else {
    //    ui->btnSelectExport->setText("Export...");
    //    ui->btnImportBatch->setText("Import...");
    //    ui->btnExportBatch->setText("Export");
    //    ui->btnExportHistory->setText("Export");
    //    ui->contextLabel->setText("Please select your action:");
    //    ui->sheetCheckBox->setText("Results");
    //    ui->pdfCheckBox->setText("Reports");
    //    ui->templateCheckBox->setText("Template");
    //    //result->setText("Results");
    //    //pdfFile->setText("Reports");
    //}

    if (_is_batch)//batch
    {
        ui->btnExportHistory->hide();
        ui->btnExportBatch->hide();

        ui->sheetCheckBox->hide();
        ui->pdfCheckBox->hide();
        ui->templateCheckBox->hide();
    }
    else //history export
    {
        ui->btnImportBatch->hide();
        ui->btnExportBatch->hide();
        ui->btnSelectExport->hide();
//        gLayout->addLayout(checkboxLayout,0,0,1,1);
//        gLayout->addLayout(exportLayout,1,0,1,1);
//        this->setLayout(gLayout);
    }

    //
    model = new ThreadModel;
    hard = new QThread();
    model->moveToThread(hard);

    //
    //connect(model, &ThreadModel::ppp()), historyWin, &History::Deltwo()));
    connect(model, &ThreadModel::sigRefresh, batchscr, &WinScreen::slotRefresh);              // TODO: 不只是批量筛查窗口使用导入模块？   // TODO: 这些信号和槽的命名、逻辑都比较乱，待整理！
    //connect(model, &ThreadModel::sigProcessEnd, historyWin, &History::ref);
    connect(model, &ThreadModel::sigWarningMsg, batchscr, &WinScreen::slotShowWarningMsg);      //add by sun for showing msgWin
    connect(model, &ThreadModel::sigEnableViewObject, batchscr, &WinScreen::slotEnableViewObject);
    connect(this, &import::sigImportBatch, model, &ThreadModel::slotImportBatch);
    connect(this, &import::sigExport, model, &ThreadModel::slotExportData);

    connect(model, &ThreadModel::progressSig, getWinManage(), &CWinManage::showProgress);
    connect(model, &ThreadModel::sigProcessEnd, this, &import::slotProcessEnd);

    //
    hard->start();

    //
    ui->sheetCheckBox->setStyleSheet(   "QCheckBox{ color:white; background:transparent;} QCheckBox::indicator {width: 16px; height: 16px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked {image: url(:/resource/unchecked.png);}");
    ui->pdfCheckBox->setStyleSheet(     "QCheckBox{ color:white; background:transparent;} QCheckBox::indicator {width: 16px; height: 16px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked {image: url(:/resource/unchecked.png);}");
    ui->templateCheckBox->setStyleSheet("QCheckBox{ color:white; background:transparent;} QCheckBox::indicator {width: 16px; height: 16px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked {image: url(:/resource/unchecked.png);}");

}

import::~import()
{
    hard->exit();
    hard->wait(10000);
    if (hard->isRunning()) {
        hard->terminate();
        hard->wait(10000);
    }

    model->deleteLater();
    model = nullptr;
    hard->deleteLater();
    hard = nullptr;

    delete ui;
}

void import::showEvent(QShowEvent *)
{
    //
    CBaseFormIntf::centerWidget(this);

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

}

//
void import::paintEvent(QPaintEvent *event)
{
    //make stylesheet working when setting background transparent
        QStyleOption opt;
        opt.init(this);
        QPainter pt(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &pt, this);

        pt.setRenderHint(QPainter::Antialiasing);  // 反锯齿;
        pt.setBrush(QBrush(QColor(60, 60, 60, 200)));
        pt.setPen(Qt::transparent);
        QRect rect = this->rect();
        rect.setWidth(rect.width() - 1);
        rect.setHeight(rect.height() - 1);
        pt.drawRoundedRect(rect, 15, 15);

        QWidget::paintEvent(event);
}

void import::keyPressEvent(QKeyEvent *)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
    /*
    QDir dir("/media/cut");
    if(!dir.exists()){
        dir.mkdir("/media/cut");
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow(0);
    QString filePathName = "/media/cut/cut-";
    filePathName += this->objectName();
    filePathName += ".png";

    if(!pixmap.save(filePathName,"png"))
    {
        qDebug()<<"cut save png failed"<<endl;
    }
    */
}

void import::exportScreenData(bool _is_batch)
{
    bool is_sheet = ui->sheetCheckBox->isChecked();
    bool is_pdf = ui->pdfCheckBox->isChecked();
    bool is_template = ui->templateCheckBox->isChecked();

    if (!is_template) {
        if (selectedIds.size() == 0) {
            QString text = tr("请选择导出记录");   // "Please select records"
            getWinManage()->showMsgWin(text);
            this->reject();
            return;
        }
    }

    if (!is_sheet && !is_pdf && !is_template)
    {       
        QString text = tr("请选择导出文件类型"); // "Please select file type"
        getWinManage()->showMsgWin(text);
        this->reject();
        return;
    }

    emit sigExport(selectedIds, udiskPath, _is_batch, is_sheet, is_pdf, is_template);

}

void import::slotProcessEnd()
{
    this->accept();
}

void import::on_back_pushButton_clicked()
{
    this->close();
}

// “导出...”按钮点击：选择导出选项
void import::on_btnSelectExport_clicked()
{
    qDebug()<<"select export";
    ui->btnExportBatch->show();
    ui->pdfCheckBox->show();
    ui->templateCheckBox->show();
    ui->sheetCheckBox->show();

    ui->btnSelectExport->hide();
    ui->btnImportBatch->hide();
}

// “导入...”按钮点击（只对批量筛查有效）
void import::on_btnImportBatch_clicked()
{
    qDebug()<<"select import";

    /*
    // 显示文件选择对话框
    QFileDialog file_dlg(this);
    file_dlg.setWindowTitle(language ? "选择 *.xlsx 文件" : "select *.xlsx files");
    file_dlg.setDirectory(udiskPath);
    file_dlg.setNameFilter(language ? "(文件 *.xlsx)" : "File (*.xlsx)");
    file_dlg.setFileMode(QFileDialog::ExistingFiles);
    file_dlg.setViewMode(QFileDialog::List);
    if (file_dlg.exec()) {
        QStringList file_list = file_dlg.selectedFiles();
        //this->hide();
        if (file_list.length() > 0) {
            logDebug(QString::asprintf("import::on_btnImportBatch_clicked(): selected files: %s", file_list.join(",").toLocal8Bit().data()));
            emit sigImportBatch(file_list);
        }
    }
    */  // TODO: 不能用这种方式，因为会把系统的文件都显示出来，不安全？

    // 显示文件选择对话框
    //this->hide();

    CDialogListSelect dlg(this->parentWidget());
    dlg.setModal(true);

    Util::CUDisk::remount();     // TODO: 这是临时解决方法，待完善

    //QTextCodec *codec_old = QTextCodec::codecForLocale();
    //QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF8"));

    QDir dir(udiskPath);
    QFileInfoList info_list = dir.entryInfoList(QDir::Files, QDir::Name);

    QStringList file_name_list;
    foreach (QFileInfo file_info, info_list)
    {
        if (file_info.fileName().endsWith(".xlsx"))
            file_name_list.append(file_info.fileName());
    }
    //Util::sortStringList(file_name_list);   // TODO: QDir::entryInfoList(, QDir::SortFlag) 怎么实现的？

    foreach (QString str, file_name_list) {
        dlg.insertRow(str);
    }

    //QTextCodec::setCodecForLocale(codec_old);

    if (dlg.exec() && dlg.fileList.length() > 0)
    {
        QStringList file_list;
        for (int i = 0; i < dlg.fileList.length(); i++)
            file_list.append(udiskPath + QDir::separator() + dlg.fileList.at(i));
        logDebug("import::on_btnImportBatch_clicked(): selected files: " + file_list.join(","));
        emit sigImportBatch(file_list);
    }
}

// “导出”按钮点击（只对门诊记录有效）
void import::on_btnExportHistory_clicked()
{
    exportScreenData(false);
}

// “导出”按钮点击（只对批量筛查有效）
void import::on_btnExportBatch_clicked()
{
    exportScreenData(true);
}

