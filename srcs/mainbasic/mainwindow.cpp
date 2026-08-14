//主页
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QPalette>
#include <QBrush>
#include <QPixmap>
#include <QStatusBar>
#include <QtCore>
#include <QMovie>

#include "winmeasure.h"
#include "windowsmanager.h"
#include "personalinfos.h"
#include "global.h"

using namespace std;

int batchhistory = 0;
int beginreplace = 0;
int endreplace = 0;
int p=0;

//
MainWindow::MainWindow(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 窗体尺寸
#if (SCREEN_SIZE_TYPE == 2)
    QRect rect_self = this->geometry();
    this->setGeometry(rect_self.left(), rect_self.top(), SCREEN_WIDTH, SCREEN_HEIGHT);
#endif
    //qDebug() << "SCREEN_WIDTH = " << SCREEN_WIDTH << ", this->width() = " << this->width();

    /*
    QWidget *myWidget = ui->centralWidget;
    myWidget->setAutoFillBackground(true);
    */
    QFont font = qApp->font();
    QList<QLabel *> qlabel = this->findChildren<QLabel *>();
    foreach (QLabel *ql, qlabel) {
         ql->setFont(font);
    }

    isShowStatusBar = true;

    barcodeMode = false;

    if (CGlobal::isReadBarcodeByQt)
        connect(&readBarcode,SIGNAL(timeout()),this,SLOT(barcodeHandle()));

    mMovie = new QMovie(":/resource/loading.gif");
    loading = new QLabel(this);
    loading->setMovie(mMovie);
    loading->setGeometry(336,176,128,128);
    loading->hide();

    //
    qDebug() << "MainWindow() ended";
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showEvent(QShowEvent *)
{
    qDebug()<< "paint MainWindow";

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 样式刷新
    QPalette palette = this->palette();
    if(themeType_Black == getSysThemeType()){
        palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_b.png"));   //默认黑色背景图

        ui->pushButton_historyrecord->setIcon(QIcon(":/resource/black_theme/clinic_records_b.png"));
        ui->pushButton_piliang->setIcon(QIcon(":/resource/black_theme/Mass_screening_b.png"));
        ui->pushButton_tool->setIcon(QIcon(":/resource/black_theme/tool_b.png"));
        //ui->pushButton_6_12->setStyleSheet("QPushButton{background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px;}");
        //ui->pushButton_12_36->setStyleSheet("QPushButton{background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px;}");
        //ui->pushButton_3_6->setStyleSheet("QPushButton{background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px;}");
        //ui->pushButton_6_20->setStyleSheet("QPushButton{background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px;}");
        //ui->pushButton_20_100->setStyleSheet("QPushButton{background-color:rgb(51,56,62); color:rgb(204,204,204); border-radius:5px;}");
        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p,list_Label)
        {
            QString  qstr = p->objectName();
            //qDebug()<<"------strstr1------"<<p->objectName();
            if(qstr.contains("label_",Qt::CaseSensitive))   //成功返回true 第二个参数表示是否大小写敏感
            {
                p->setStyleSheet("QLabel{background-color:rgb(204,204,204); border-radius:5px;}");
                p->raise();
            }
        }
        ui->history_label->setStyleSheet("QLabel{background-color:transparent; color:rgb(204,204,204);}");
        ui->tool_label->setStyleSheet("QLabel{background-color:transparent; color:rgb(204,204,204);}");
        ui->bench_label->setStyleSheet("QLabel{background-color:transparent; color:rgb(204,204,204);}");

        ui->ckbIsSimulatedEye->setStyleSheet("QCheckBox {background-color:rgb(1,1,1); color:rgb(204,204,204);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png);} ");
        ui->ckbIsDebugMode->setStyleSheet("QCheckBox {background-color:rgb(1,1,1); color:rgb(204,204,204);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png);} ");

        qDebug()<< "paint black background!";
    }
    else{
        palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_w.png"));  //白色背景图

        ui->pushButton_historyrecord->setIcon(QIcon(":/resource/white_theme/clinic_records_w.png"));
        ui->pushButton_piliang->setIcon(QIcon(":/resource/white_theme/Mass_screening_w.png"));
        ui->pushButton_tool->setIcon(QIcon(":/resource/white_theme/tool_w.png"));
        //ui->pushButton_6_12->setStyleSheet("QPushButton{background-color:rgb(227,227,227); color:rgb(1,1,1); border-radius:5px;}");
        //ui->pushButton_12_36->setStyleSheet("QPushButton{background-color:rgb(227,227,227); color:rgb(1,1,1); border-radius:5px;}");
        //ui->pushButton_3_6->setStyleSheet("QPushButton{background-color:rgb(227,227,227); color:rgb(1,1,1); border-radius:5px;}");
        //ui->pushButton_6_20->setStyleSheet("QPushButton{background-color:rgb(227,227,227); color:rgb(1,1,1); border-radius:5px;}");
        //ui->pushButton_20_100->setStyleSheet("QPushButton{background-color:rgb(227,227,227); color:rgb(1,1,1); border-radius:5px;}");
        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p,list_Label)
        {
            QString  qstr = p->objectName();
            //qDebug()<<"------strstr1------"<<p->objectName();
            if(qstr.contains("label_",Qt::CaseSensitive))   //成功返回true 第二个参数表示是否大小写敏感
            {
                p->setStyleSheet("QLabel{background-color:rgb(172,172,172); border-radius:5px;}");
                p->raise();
            }
        }
        ui->history_label->setStyleSheet("QLabel{background-color:transparent; color:rgb(1,1,1);}");
        ui->tool_label->setStyleSheet("QLabel{background-color:transparent; color:rgb(1,1,1);}");
        ui->bench_label->setStyleSheet("QLabel{background-color:transparent; color:rgb(1,1,1);}");

        //ui->ckbIsSimulatedEye->setStyleSheet();
        //ui->ckbIsDebugMode->setStyleSheet();

        qDebug()<< "paint blue background!";
    }
    this->setPalette(palette);
    this->setAutoFillBackground(true);

    // 标题
    getWinManage()->updateWindowTitle(this, tr("主页"));

    //
    //if(language){  //显示中文字体
    //getWinManage()->updateWindowTitle(this, tr("主页"));
    //
    //    //ui->home_label->setText("主页");
    //    ui->history_label->setText("门诊记录");
    //    ui->bench_label->setText("批量筛查");
    //    ui->tool_label->setText("工具");
    //
    //    ui->pushButton_6_12->setText("06-12 个月");
    //    ui->pushButton_12_36->setText("12-36 个月");
    //    ui->pushButton_3_6->setText("03-06 周岁");
    //    ui->pushButton_6_20->setText("06-20 周岁");
    //    ui->pushButton_20_100->setText("20-100 周岁");
    //}
    //else    //显示英文字体
    //{
    //    getWinManage()->updateWindowTitle(this, "Home");
    //
    //    //ui->home_label->setText("Home");
    //    ui->history_label->setText("ClinicRecords");
    //    ui->bench_label->setText("ScreenRecords");
    //    ui->tool_label->setText("Tools");
    //
    //    ui->pushButton_6_12->setText("06-12\nMonths");
    //    ui->pushButton_12_36->setText("12-36\nMonths");
    //    ui->pushButton_3_6->setText("03-06\nYears");
    //    ui->pushButton_6_20->setText("06-20\nYears");
    //    ui->pushButton_20_100->setText("20-100\nYears");
    //}

    barcodeMode = false;
    WinMeasure::setOperationMode(operationMode_NormalMeasure);

    // 注册键盘侦听（用于扫码）
    globalService()->regKbReader(this);

    //
    ui->ckbIsSimulatedEye->setVisible(CGlobal::isDebugMode || CGlobal::isSimulatedEye);
    ui->ckbIsSimulatedEye->setChecked(CGlobal::isSimulatedEye);

#if (2 != OS_TYPE)
    ui->ckbIsDebugMode->setVisible(false);
#endif
    if (CGlobal::isDebugMode) {
        ui->ckbIsDebugMode->setVisible(true);
    }
    ui->ckbIsDebugMode->setChecked(CGlobal::isDebugMode);

}

void MainWindow::hideEvent(QHideEvent *)
{
    // FIXME: (2025-02-27) 调试发现程序关闭时，本窗体的隐藏事件被调用了两次？


    // 反注册键盘侦听（用于扫码）
    globalService()->unregKbReader(this);
}

void MainWindow::slotPhysicButtonPressed()
{
    // 若本窗体不是当前窗体，则不处理此信号
    if (this != getWinManage()->getCurrentWin()) {
        return;
    }

    // 在主页点击物理按键后，启动测量界面
    g_WinMeasure->continueMeasuring();

}

void MainWindow::on_pushButton_historyrecord_clicked()  //门诊记录
{
    getWinManage()->showWindowByType(WIN_CLINIC);

}

void MainWindow::on_pushButton_tool_clicked()   //工具
{
    //
    g_elapsedTimer.start();
    qDebug() << __PRETTY_FUNCTION__  << ": g_elapsedTimer started ...";

    //
    getWinManage()->showWindowByType(WIN_TOOL);

    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() = " << g_elapsedTimer.elapsed();

}

void MainWindow::on_pushButton_6_12_clicked()  //6-12个月
{
    WinMeasure::setOperationMode(operationMode_NormalMeasure);
    getWinManage()->openClinicMeasure(ageRange_0_06_12_MONTH);
}

void MainWindow::on_pushButton_12_36_clicked()  //12-36个月
{
    WinMeasure::setOperationMode(operationMode_NormalMeasure);
    getWinManage()->openClinicMeasure(ageRange_1_01_03_YEAR);
}

void MainWindow::on_pushButton_3_6_clicked()    //3-6岁
{
    WinMeasure::setOperationMode(operationMode_NormalMeasure);
    getWinManage()->openClinicMeasure(ageRange_2_03_06_YEAR);
}

void MainWindow::on_pushButton_6_20_clicked()   //6-20岁
{
    WinMeasure::setOperationMode(operationMode_NormalMeasure);
    getWinManage()->openClinicMeasure(ageRange_3_06_20_YEAR);
}

void MainWindow::on_pushButton_20_100_clicked() //20-100岁
{
    WinMeasure::setOperationMode(operationMode_NormalMeasure);
    getWinManage()->openClinicMeasure(ageRange_4_20_100_YEAE);
}

void MainWindow::on_pushButton_piliang_clicked()    //批量筛查
{
    getWinManage()->showWindowByType(WIN_SCREEN);
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //按拍摄按键进入测量
//    if(e->key()==Qt::Key_Escape)              /* Qt::Key_Escape 按键消息已统一在 WindowsManagers::eventFilter() 处理。 */
//    {
//        //qDebug()<<"press Key_Escape,enter camera";
//        //continueMeasuring();
//    }

    //
    if (CGlobal::isReadBarcodeByQt) {
        //获取扫码抢信息
        barcodeData.append(e->text());

        if(!barcodeMode){
            barcodeMode = true;
            qDebug()<<"barcodeMode = true";
            readBarcode.start(1000);
            showLoading(true);
        }

        if(e->key()==Qt::Key_Enter || e->key()==Qt::Key_Return)  /* Qt::Key_Enter 是数字键盘的Enter键 */
        {
            qDebug()<<"--get Key_Enter or Key_Return!";
            barcodeHandle();
        }

//        if(barcodeData.endsWith("/n")){
//            qDebug()<<"--read barcode ends!";
//            readBarcode.stop();
//            barcodeData.chop(2);
//            barcodeHandle();
//        }
    }
}

// TODO: 目前支持条码的窗口有主页、批量筛查和结果页，扫码开始和中途的处理是 keyPressEvent()，结束的处理是 barcodeHandle()，抽象为通用模块？
void MainWindow::barcodeHandle()    // TODO: 检查缓冲区的内容是否是回车结尾，且长度大于最小值，否则忽略数据。另外两个支持扫码的窗体也一样。
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

//void MainWindow::mousePressEvent(QMouseEvent *e)
//{
//}

void MainWindow::showLoading(bool state)
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

void MainWindow::on_ckbIsDebugMode_clicked(bool checked)
{
    CGlobal::isDebugMode = checked;
}

