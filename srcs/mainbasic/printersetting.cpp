//打印设置
#include "printersetting.h"
#include "ui_printersetting.h"

#include "QCheckBox"
#include "QVBoxLayout"
#include "QHBoxLayout"
#include "QDebug"

#include "noticewin.h"
#include "windowsmanager.h"
#include "appsetting.h"
#include "global.h"
#include "sysinfo.h"

using Common::stPrinterInfo;

/// ====================================================================================================
/// class CBusiDataPrintSetting
///

void CBusiDataPrintSetting::reset()
{
    // TODO:

}

bool CBusiDataPrintSetting::isEqualTo(const CBusiDataPrintSetting &_busi_data) const
{
    return (true
            && _busi_data.wifiPrinterIP         == this->wifiPrinterIP
            && _busi_data.wifiPrinterPort       == this->wifiPrinterPort
            && _busi_data.isAutoPrintTicket     == this->isAutoPrintTicket
            && _busi_data.ticketPrintConnType   == this->ticketPrintConnType
            && _busi_data.organizationName      == this->organizationName
            && _busi_data.orgNameA4             == this->orgNameA4
            && _busi_data.operatorName          == this->operatorName
            );
}

/// ====================================================================================================
/// class printerSetting
///

//
printerSetting::printerSetting(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::printerSetting)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    //this->setWindowModality(Qt::WindowModal);
    ipEdit = new myEditLine(this);      //IP地址
    //ipEdit->setMinimumSize(200,40);
    ipEdit->setStyleSheet("border:1px solid rgb(200,200,200);border-radius:5px;padding:2px 4px;");
    ui->horizontalLayout_IP->addWidget(ipEdit);
    portEdit = new myEditLine(this);    //端口
    //portEdit->setMinimumSize(150,40);
    portEdit->setStyleSheet("border:1px solid rgb(200,200,200);border-radius:5px;padding:2px 4px;");
    ui->horizontalLayout_Port->addWidget(portEdit);

    // 机构名称（小票）
    edtOrganizationName = new myEditLine((QWidget*)ui->edtOrganizationNameT->parent());
    edtOrganizationName->setGeometry(ui->edtOrganizationNameT->geometry());
    ui->edtOrganizationNameT->setVisible(false);

    // 机构名称（A4报告）
    edtOrgNameA4 = new myEditLine((QWidget*)ui->edtOrgNameA4T->parent());
    edtOrgNameA4->setGeometry(ui->edtOrgNameA4T->geometry());
    ui->edtOrgNameA4T->setVisible(false);

    // 操作者
    edtOperator = new myEditLine((QWidget*)ui->edtOperatorT->parent());
    edtOperator->setGeometry(ui->edtOperatorT->geometry());
    ui->edtOperatorT->setVisible(false);

    //
    ui->tabWidget->setCurrentIndex(0);

    // 等待动画
    lblWaiting = new CWaitingMovie((QWidget *)(ui->btnFindPrinterList->parent()), ":/resource/uploading.gif", 32);
    lblWaiting->move(ui->btnFindPrinterList->x() + (ui->btnFindPrinterList->width() - lblWaiting->width()) / 2,
                     ui->btnFindPrinterList->y() + (ui->btnFindPrinterList->height() - lblWaiting->height()) / 2
                     );

    // 事件连接
    QObject::connect(g_printIntf, &Common::CPrintIntf::sigSearchFinished, this, &printerSetting::slot_printIntf_SearchFinished, Qt::QueuedConnection);

    QObject::connect(&timerRefreshJobCount, &QTimer::timeout, this, &printerSetting::slot_timerRefreshJobCount_timeout, Qt::QueuedConnection);

    // 窗口视图设计期间的临时设置恢复
    ui->cbbPrinterList->clear();

    // 预添加“未选择”打印机选项
    ui->cbbPrinterList->addItem(tr("（未选择）"));   // "(not selected)"

    // 若操作系统未支持 A4 直连打印，则隐藏相关控件
    if (!CSysInfo::cups()) {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabDirectPrint));
    }

    // 设置上次已连接的打印机
    //QString last_conn_printer_uri   = appSetting::value("/cups/lastPrinterUri"  , "").toString();
    //QString last_conn_printer_name  = appSetting::value("/cups/lastPrinterName" , "").toString();
    //g_printIntf->setDefaultPrinter(last_conn_printer_uri, last_conn_printer_name);

    // 未完成，先隐藏服务器打印
    if (!CGlobal::isDebugMode) {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabServerPrinter));
    }

    // tabWidget 的 tabBar 背景色尺寸重调
    QRect rect_tab_back = ui->frmTabBackground->geometry();
    rect_tab_back.setWidth(160 * ui->tabWidget->count() - 20);
    ui->frmTabBackground->setGeometry(rect_tab_back);

}

printerSetting::~printerSetting()
{
    delete ui;
}

void printerSetting::showEvent(QShowEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 获得业务数据
    configToBusiData(busiDataOrigin);

    // 将业务数据设置到 UI
    busiDataToUi(busiDataOrigin);

    // 更新主题
    updateTheme(getSysThemeType());

    // A4 直连打印相关处理
    if (CSysInfo::cups()) {
        // 开始刷新打印任务数
        startRefreshJobCount(2000);
    }

}

void printerSetting::updateTheme(enThemeType _theme)        // TODO: 直接对整个窗体设置样式表，不必逐个部件设置
{
    //QPalette palette;
    if (themeType_Black == _theme) {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        ui->ckbIsAutoPrintTicket->setStyleSheet("QCheckBox{background-color:transparent; color:rgb(250,250,252);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png);}");
        ui->checkBox_WifiPrint->setStyleSheet(  "QCheckBox{background-color:transparent; color:rgb(250,250,252);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png);}");
        ui->checkBox_BtPrint->setStyleSheet(    "QCheckBox{background-color:transparent; color:rgb(250,250,252);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png);}");

        ui->btnBtConn->setStyleSheet("QPushButton {background-color: rgb(28,28,30); color: rgb(250,250,252); border-radius:5px;}");

        ui->label_IP->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->label_Port->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->lblOrganizationName->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");

        ipEdit->setStyleSheet(              "QLineEdit {border-radius: 5px; background-color: rgb(11,15,22); color: rgb(204,204,204);} ");
        portEdit->setStyleSheet(            "QLineEdit {border-radius: 5px; background-color: rgb(11,15,22); color: rgb(204,204,204);} ");
        edtOrganizationName->setStyleSheet( "QLineEdit {border-radius: 5px; background-color: rgb(11,15,22); color: rgb(204,204,204);} ");
        edtOrgNameA4->setStyleSheet(        "QLineEdit {border-radius: 5px; background-color: rgb(11,15,22); color: rgb(204,204,204);} ");
        edtOperator->setStyleSheet(         "QLineEdit {border-radius: 5px; background-color: rgb(11,15,22); color: rgb(204,204,204);} ");

        ui->label_Home->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Save->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");
        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/black_theme/save_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));

        ui->frmTabBackground->setStyleSheet("background-color:rgb(28,28,30);");

        ui->tabWidget->setStyleSheet(R"(
QTabWidget {background-color: transparent; color: rgb(250,250,252);}
QTabWidget::pane {border: none; border-top: 1px solid rgb(46,57,70); border-bottom: 1px solid rgb(46,57,70);}
QTabBar::tab {width: 160; height: 36; background-color: rgb(28,28,30); color: rgb(250,250,252); border-radius: 5px;}
QTabBar::tab:selected {width: 160; height: 36; background-color: rgb(90,90,95); color: rgb(250,250,252);}
                                     )");

        ui->lblPrinterList->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->btnFindPrinterList->setStyleSheet("QPushButton {background-color: rgb(28,28,30); color: rgb(250,250,252); border-radius:5px;}");
        ui->lblJobCountTitle->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->btnCancelAllJobs->setStyleSheet("QPushButton {background-color: rgb(28,28,30); color: rgb(250,250,252); border-radius:5px;}");
        ui->lblOrgNameA4->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->lblOperator->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->btnUpdateImgs->setStyleSheet("QPushButton {background-color: rgb(28,28,30); color: rgb(250,250,252); border-radius:5px;}");
        ui->btnDiagnosticStandard->setStyleSheet("QPushButton {background-color: rgb(28,28,30); color: rgb(250,250,252); border-radius:5px;}");
        ui->btnDiagnosisSuggestion->setStyleSheet("QPushButton {background-color: rgb(28,28,30); color: rgb(250,250,252); border-radius:5px;}");
        ui->lblImgLogoTitle->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->lblImgLogoTip->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->lblImgQrCodeTitle->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");
        ui->lblImgQrCodeTip->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252);}");

        QString style_combobox = R"(
QComboBox{ background-color: rgb(51,56,62); color: rgb(204,204,204); }
QComboBox::drop-down { image: url(:/resource/black_theme/combo-arrow-down_b.png); }
QComboBox QAbstractItemView { background-color: rgb(51,56,62); color: rgb(204,204,204); border: 2px solid rgb(149,149,149); }
QComboBox QAbstractItemView::item { min-height: 40px; }
QComboBox QAbstractItemView::item:selected { background-color: rgb(48,140,198); color: rgb(255,255,255); }
                )";

        ui->cbbPrinterList->setStyleSheet(style_combobox);

        ui->lblJobCount->setStyleSheet("QLabel {background-color:transparent; color:rgb(250,250,252); border:none; border-bottom: 1px solid rgb(250,250,252);}");
        ui->frmImgLogo->setStyleSheet(".QFrame {border: 1px dashed rgb(250,250,252);}");
        ui->frmImgQrCode->setStyleSheet(".QFrame {border: 1px dashed rgb(250,250,252);}");

    }
    else{
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));     //设置白色主题

        ui->ckbIsAutoPrintTicket->setStyleSheet("QCheckBox{background-color:rgb(242,242,247);} QCheckBox{color:rgb(51,51,51);} QCheckBox::indicator {width: 20px; height: 20px;}QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        ui->checkBox_WifiPrint->setStyleSheet("QCheckBox{background-color:rgb(242,242,247);} QCheckBox{color:rgb(51,51,51);} QCheckBox::indicator {width: 20px; height: 20px;}QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        ui->checkBox_BtPrint->setStyleSheet("QCheckBox{background-color:rgb(242,242,247);} QCheckBox{color:rgb(51,51,51);} QCheckBox::indicator {width: 20px; height: 20px;}QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        ui->label_IP->setStyleSheet("QLabel{background-color:rgb(242,242,247); color:rgb(51,51,51);}");
        ui->label_Port->setStyleSheet("QLabel{background-color:rgb(242,242,247); color:rgb(51,51,51);}");
        ipEdit->setStyleSheet("QLineEdit{border-radius:5px; background-color:rgb(227,227,232); color:rgb(51,51,51);}");
        portEdit->setStyleSheet("QLineEdit{border-radius:5px; background-color:rgb(227,227,232); color:rgb(51,51,51);}");

        ui->lblOrganizationName->setStyleSheet("QLabel{background-color:rgb(242,242,247); color:rgb(51,51,51);}");
        edtOrganizationName->setStyleSheet("myEditLine {border-radius: 5px; background-color: rgb(227, 227, 232); color: rgb(51, 51, 51);}");

        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Save->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");
        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/white_theme/save_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
    }
    //this->setPalette(palette);

#if (OS_TYPE == 1)
    if (g_hasBluetooth) {
        ui->checkBox_BtPrint->show();       //显示蓝牙打印复选框
    } else {
        ui->checkBox_BtPrint->hide();       //影藏蓝牙打印复选框
    }
#endif

}

void printerSetting::updateLanguage()
{
    //if (language) {
    //    ui->ckbIsAutoPrintTicket->setText("自动打印小票");
    //    ui->checkBox_WifiPrint->setText("WiFi打印");
    //    ui->checkBox_BtPrint->setText("蓝牙打印");
    //    ui->label_IP->setText("IP：");
    //    ui->label_Port->setText("端口：");
    //    ui->label_Home->setText("主页");
    //    ui->label_Save->setText("保存");
    //    ui->label_Back->setText("返回");
    //    ui->btnBtConn->setText("蓝牙连接...");
    //    ui->lblOrganizationName->setText("机构名称：");
    //
    //    ui->tabWidget->setTabText(0, "小票打印");
    //    ui->tabWidget->setTabText(1, "A4 直连打印");
    //    ui->tabWidget->setTabText(2, "通过服务器打印");
    //
    //    ui->lblPrinterList->setText("打印机列表：");
    //    ui->btnFindPrinterList->setText("查找");
    //    ui->lblJobCountTitle->setText("打印队列：");
    //    ui->btnCancelAllJobs->setText("取消所有");
    //    ui->lblOrgNameA4->setText("机构名称：");
    //    ui->lblOperator->setText("操作者：");
    //    ui->btnUpdateImgs->setText("从U盘更新Logo和二维码");
    //    ui->btnDiagnosticStandard->setText("随访诊治标准...");
    //    ui->btnDiagnosisSuggestion->setText("诊治建议...");
    //    ui->lblImgLogoTitle->setText("Logo图片");
    //    ui->lblImgLogoTip->setText("文件名：logo.png");
    //    ui->lblImgQrCodeTitle->setText("二维码图片(250*250)");
    //    ui->lblImgQrCodeTip->setText("文件名：qr-code.png");
    //} else {
    //    ui->ckbIsAutoPrintTicket->setText("Auto print ticket");
    //    ui->checkBox_WifiPrint->setText("WiFi printing");
    //    ui->checkBox_BtPrint->setText("Bluetooth printing");
    //    ui->label_IP->setText("IP: ");
    //    ui->label_Port->setText("Port: ");
    //    ui->label_Home->setText("Home");
    //    ui->label_Save->setText("Save");
    //    ui->label_Back->setText("Back");
    //    ui->btnBtConn->setText("Bluetooth Connection...");
    //    ui->lblOrganizationName->setText("OrganizationName: ");
    //
    //    ui->tabWidget->setTabText(0, "Ticket Print");
    //    ui->tabWidget->setTabText(1, "A4 Direct Print");
    //    ui->tabWidget->setTabText(2, "Print Through Server");
    //
    //    ui->lblPrinterList->setText("Printer List: ");
    //    ui->btnFindPrinterList->setText("Find");
    //    ui->lblJobCountTitle->setText("Print Queues: ");
    //    ui->btnCancelAllJobs->setText("Cancel All");
    //    ui->lblOrgNameA4->setText("Organization: ");
    //    ui->lblOperator->setText("Operator: ");
    //    ui->btnUpdateImgs->setText("Update Pictures from U-Disk");
    //    ui->btnDiagnosticStandard->setText("DiagnosticCriterion...");
    //    ui->btnDiagnosisSuggestion->setText("DiagnosisSuggestion...");
    //    ui->lblImgLogoTitle->setText("Logo Picture");
    //    ui->lblImgLogoTip->setText("FileName: logo.png");
    //    ui->lblImgQrCodeTitle->setText("QrCode Picture(250*250)");
    //    ui->lblImgQrCodeTip->setText("FileName: qr-code.png");
    //}

    getWinManage()->updateWindowTitle(this, tr("打印设置"));    // "Print setup"

    //
    QFont font;
    font.setPointSize(16);
    ipEdit->setFont(font);
    portEdit->setFont(font);
    edtOrganizationName->setFont(font);

}

QString printerSetting::getCfg_WifiPrinterIP()
{
    return appSetting::value("/printerip").toString();
}

int printerSetting::getCfg_WifiPrinterPort()
{
    return appSetting::value("/printerport").toInt();
}

void printerSetting::configToBusiData(CBusiDataPrintSetting &_busi_data)
{
    // 业务数据对象置零
    _busi_data.reset();

    // 是否自动打印
    _busi_data.isAutoPrintTicket = CGlobal::isAutoPrintTicket;

    // 小票打印连接方式
    _busi_data.ticketPrintConnType = CGlobal::ticketPrintConnType;

    // WiFi 小票打印机 IP
    _busi_data.wifiPrinterIP = getCfg_WifiPrinterIP();

    // WiFi 小票打印机 端口
    _busi_data.wifiPrinterPort = getCfg_WifiPrinterPort();

    // 机构名称（小票的）
    _busi_data.organizationName = CGlobal::organizationName;

    // 机构名称（A4报告的）
    _busi_data.orgNameA4 = CGlobal::orgNameA4;

    // 操作者
    _busi_data.operatorName = CGlobal::operatorName;

}

void printerSetting::saveBusiData(const CBusiDataPrintSetting &_busi_data)
{
    //
    appSetting::setValue("/printerip", _busi_data.wifiPrinterIP);
    appSetting::setValue("/printerport", _busi_data.wifiPrinterPort);

    //
    CGlobal::isAutoPrintTicket      = _busi_data.isAutoPrintTicket;
    CGlobal::ticketPrintConnType    = _busi_data.ticketPrintConnType;
    CGlobal::organizationName       = _busi_data.organizationName;
    CGlobal::orgNameA4              = _busi_data.orgNameA4;
    CGlobal::operatorName           = _busi_data.operatorName;
    CGlobal::saveConfs();

    //
    busiDataOrigin = _busi_data;

}

void printerSetting::busiDataToUi(const CBusiDataPrintSetting &_busi_data)
{
    ui->ckbIsAutoPrintTicket->setChecked(_busi_data.isAutoPrintTicket);

    ui->checkBox_WifiPrint->setChecked(ticketPrintConnType_WiFi == _busi_data.ticketPrintConnType);
    ui->checkBox_BtPrint->setChecked(ticketPrintConnType_BT == _busi_data.ticketPrintConnType);

    ipEdit->setText(_busi_data.wifiPrinterIP);
    portEdit->setText(QString::number(_busi_data.wifiPrinterPort));

    edtOrganizationName->setText(_busi_data.organizationName);
    edtOrgNameA4->setText(_busi_data.orgNameA4);
    edtOperator->setText(_busi_data.operatorName);

    //
    ui->btnBtConn->setVisible(ticketPrintConnType_BT == _busi_data.ticketPrintConnType);
}

void printerSetting::uiToBusiData(CBusiDataPrintSetting &_busi_data)
{
    _busi_data.isAutoPrintTicket    = ui->ckbIsAutoPrintTicket->isChecked();
    _busi_data.ticketPrintConnType  = (ui->checkBox_WifiPrint->isChecked() ? ticketPrintConnType_WiFi : ticketPrintConnType_BT);
    _busi_data.wifiPrinterIP        = ipEdit->text();
    _busi_data.wifiPrinterPort      = portEdit->text().toInt();
    _busi_data.organizationName     = edtOrganizationName->text();
    _busi_data.orgNameA4            = edtOrgNameA4->text();
    _busi_data.operatorName         = edtOperator->text();
}

void printerSetting::askAndSave(const CBusiDataPrintSetting &_busi_data)
{
    QString text = tr("是否保存修改?");   // "Save the modifications?"
    bool ret = getWinManage()->showNoticeWin(text);
    if (ret) {
        saveBusiData(_busi_data);
    }
}

void printerSetting::startRefreshJobCount(int _interval)
{
    timerRefreshJobCount.start(_interval);
}

void printerSetting::slot_timerRefreshJobCount_timeout()
{
    int job_count = g_printIntf->getJobsCount();
    ui->lblJobCount->setText(QString::number(job_count));

    if (0 == job_count) {
        timerRefreshJobCount.stop();
    }
}

void printerSetting::on_checkBox_WifiPrint_clicked(bool checked)
{
    qDebug()<<"-----WiFiPrintState:"<<checked;
    ui->checkBox_WifiPrint->setChecked(checked);
    ui->checkBox_BtPrint->setChecked(!checked);
}

void printerSetting::on_checkBox_BtPrint_clicked(bool checked)
{
    qDebug()<<"-----BtPrintState:"<<checked;
    ui->checkBox_BtPrint->setChecked(checked);
    ui->checkBox_WifiPrint->setChecked(!checked);

    ui->btnBtConn->setVisible(ticketPrintConnType_BT == CGlobal::ticketPrintConnType);
}

void printerSetting::on_pushButton_Save_clicked()
{
    CBusiDataPrintSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    } else {
        getWinManage()->showSuspensionPrompt(tr("数据未被修改")); // "Data not modified"
    }
}

void printerSetting::on_pushButton_Back_clicked()
{
    CBusiDataPrintSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    //getWinManage()->backToLastWidget();
    getWinManage()->showWindowByType(WIN_TOOL);
}

void printerSetting::on_pushButton_Home_clicked()
{
    CBusiDataPrintSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    getWinManage()->showWindowByType(WIN_HOME);
}

void printerSetting::on_btnBtConn_clicked()
{
    CBusiDataPrintSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    getWinManage()->showWindowByType(WIN_BT);
}

void printerSetting::on_btnDiagnosticStandard_clicked()
{
    // 检查保存
    CBusiDataPrintSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    // 转到“随访诊治标准”页面
    getWinManage()->showWindowByType(WIN_DIAGNOSTIC);
}

void printerSetting::on_btnDiagnosisSuggestion_clicked()
{
    // 检查保存
    CBusiDataPrintSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    // 转到“诊治建议”页面
    getWinManage()->showWindowByType(WIN_SUGGESTION);
}

void printerSetting::on_btnFindPrinterList_clicked()
{
    // 检查 WiFi 是否连接
    //if (!g_WifiIntf->getIsConnected()) {
    //    getWinManage()->showSuspensionPrompt(tr("网络未连接"));  // "network not connected"
    //    return;
    //}

    // 暂时禁用部件
    ui->btnFindPrinterList->setEnabled(false);
    ui->cbbPrinterList->setEnabled(false);
    if (ui->cbbPrinterList->count() > 0) {
        ui->cbbPrinterList->setCurrentIndex(0);
    } else {
        //log "logic error"
    }

    // 播放等待动画
    lblWaiting->setIsPlaying(true);

    //
    g_printIntf->beginSearch();
}

void printerSetting::slot_printIntf_SearchFinished()
{
    // 获取打印机列表
    listPrinterInfo = g_printIntf->getPrinterList();

    // 提示
    getWinManage()->showSuspensionPrompt(tr("搜到 %1 台打印机").arg(listPrinterInfo.count()));    // "Found %1 printers"

    // 先清空现有列表
    ui->cbbPrinterList->clear();

    // 添加“未选择”选项
    ui->cbbPrinterList->addItem(tr("（未选择）"));   // "(not selected)"

    // 从接口拷贝打印机列表到下拉列表框
    for (int i = 0; i < listPrinterInfo.count(); i++) {
        ui->cbbPrinterList->addItem(listPrinterInfo[i].name);
    }

    // 停止等待动画
    lblWaiting->setIsPlaying(false);

    // 重新启用部件
    ui->btnFindPrinterList->setEnabled(true);
    ui->cbbPrinterList->setEnabled(true);

    // 如果只搜到一台打印机，自动选择？
    // TODO: ？

}

void printerSetting::on_cbbPrinterList_activated(int index)
{
    if (index == 0) {       // 若是选择了“未选择”行，则不需处理，否则程序会崩溃
        return;
    }

    stPrinterInfo printer_info = listPrinterInfo[index - 1];

    bool succ = g_printIntf->setDefaultPrinter(printer_info.uri);
    if (succ) {
        getWinManage()->showSuspensionPrompt(tr("已设置 “%1” 为默认打印机").arg(printer_info.name)); // "'%1' has been set as the default printer"

        //
        appSetting::setValue("/cups/lastPrinterUri" , printer_info.uri);
        appSetting::setValue("/cups/lastPrinterName", printer_info.name);
    } else {
        getWinManage()->showSuspensionPrompt(tr("设置默认打印机失败！").arg(printer_info.name)); // "Failed to set default printer!"
    }
}

void printerSetting::on_btnCancelAllJobs_clicked()
{
    g_printIntf->cancelAllJobs();
    startRefreshJobCount(1000);
}

bool printerSetting::importImgFromUdisk(const QString &_udisk_path, const QString &_file_name, QString &_msg)
{
    // 从 U 盘导入 A4 报告图像文件
    QString file_path_src = _udisk_path + QDir::separator() + _file_name;
    if (QFile::exists(file_path_src)) {
        if (!QFile::exists(REPORT_CONFIG_DIR)) {
            QDir dir(REPORT_CONFIG_DIR);
            dir.mkpath(REPORT_CONFIG_DIR);
        }

        QString file_path_dst = QString(REPORT_CONFIG_DIR) + QDir::separator() + _file_name;

        if (QFile::exists(file_path_dst)) {
            QFile::remove(file_path_dst);
        }

        bool is_copy_succ = QFile::copy(file_path_src, file_path_dst);
        if (is_copy_succ) {
            _msg = tr("文件 %1 导入成功。").arg(_file_name);   // "file %1 successfully imported."
        } else {
            _msg = tr("文件 %1 复制失败。").arg(_file_name);   // "file %1 copy failed."
        }

        return is_copy_succ;
    } else {
        _msg = tr("文件 %1 未找到。").arg(_file_name);    // "file %1 not found."
    }
    return false;
}

void printerSetting::on_btnUpdateImgs_clicked()
{
    // 检查 U 盘是否存在
    QString udisk_path = Util::CUDisk::getPath();
    bool is_udisk_exists = (udisk_path.length() > 0);
    if (!is_udisk_exists) {
        getWinManage()->showSuspensionPrompt(tr("未找到 U 盘"));    // "U-Disk not found"
        return;
    }

    // 导入组织Logo图像到配置文件
    QString msg_logo;
    bool is_succ_logo = importImgFromUdisk(udisk_path, QString(REPORT_ORG_LOGO_IMG_FILE_NAME), msg_logo);
    if (is_succ_logo) {
        // 图像处理
        QString file_path = QString(REPORT_CONFIG_DIR) + QDir::separator() + REPORT_ORG_LOGO_IMG_FILE_NAME;
        QPixmap img;
        img.load(file_path);
        CReport::processOrganizationLogo(img);
        is_succ_logo = img.save(file_path);
    }

    // 导入二维码图像到配置文件
    QString msg_qrcode;
    bool is_succ_qrcode = importImgFromUdisk(udisk_path, QString(REPORT_QR_CODE_IMG_FILE_NAME), msg_qrcode);
    if (is_succ_qrcode) {
        // 图像处理
        QString file_path = QString(REPORT_CONFIG_DIR) + QDir::separator() + REPORT_QR_CODE_IMG_FILE_NAME;
        QPixmap img;
        img.load(file_path);
        CReport::processQrCodeImg(img);
        is_succ_qrcode = img.save(file_path);
    }

    // U盘同步     // TODO：从U盘读入时这个应该不需要？
    Util::CUDisk::sync();

    // 使内存对象重新载入新的配置图像，并刷新界面图像
    if (is_succ_logo) {
        CReport::getOrganizationLogo(true);
        loadOrgLogoImg();
    }
    if (is_succ_qrcode) {
        CReport::getQrCodeImg(true);
        loadQrCodeImg();
    }

    //
    getWinManage()->showSuspensionPrompt(msg_logo + "\n" + msg_qrcode);

}

void printerSetting::loadOrgLogoImg()
{
    const QPixmap &img = CReport::getOrganizationLogo();
    ui->lblImgLogo->setPixmap(img);

    QRect rect_parent = ui->frmImgLogo->geometry();
    QRect rect_child = img.rect();
    Util::rectAdaptParent(rect_child, rect_parent, 5);

    ui->lblImgLogo->setGeometry(rect_child);
}

void printerSetting::loadQrCodeImg()
{
    const QPixmap img = CReport::getQrCodeImg();
    ui->lblImgQrCode->setPixmap(img);

    QRect rect_parent = ui->frmImgQrCode->geometry();
    QRect rect_child = img.rect();
    Util::rectAdaptParent(rect_child, rect_parent, 5);

    ui->lblImgQrCode->setGeometry(rect_child);
}

void printerSetting::on_tabWidget_currentChanged(int index)
{
    /* 因为第一次显示时布局未完成，得不到准确的尺寸，所以调整图像尺寸的操作需要确保在 showEvent() 之后， 且布局完成之后才能执行。
     * 而 Qt 只有在切换到这一页时，布局才真正完成，并不是 showEvent() 就会执行布局。
     */
    if (ui->tabWidget->indexOf(ui->tabDirectPrint) == index) {
        loadOrgLogoImg();
        loadQrCodeImg();
    }
}
