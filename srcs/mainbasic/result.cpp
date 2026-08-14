//筛查结果界面
#include "result.h"
#include "ui_result.h"

#include <algorithm>

#include <QString>
#include <QDebug>
#include <QPainter>
#include <QPoint>
#include <QtPrintSupport/QPrinter>
#include <QFile>
#include <QPdfWriter>
#include <QTextOption>
#include <QImage>
#include <QMessageBox>
#include <QPen>
#include <QColor>
#include <QLineF>
#include <QVector>
#include <QPalette>

#include "mainwindow.h"
#include "mysqlitepatients.h"
#include "windowsmanager.h"
#include "noticewin.h"
#include "windatatrans.h"
#include "DataTransmit.h"
#include "printertransmit.h"
#include "mainwindow.h"
#include "eyesightstandard.h"
#include "util-common.h"
#include "global.h"
#include "threadmodel.h"
#include "sysinfo.h"
#include "util-app.h"
#include "uploadthread.h"

using namespace DataTrans;

//
//struct CPatient a[5][6];
//extern double g_aEyesightStandard[5][9];        // TODO: 此处注释掉，用到此变量的地方改用 eyesightstandard::standardCompare()

//
int Result::ultimateDirCount = 0;
int Result::ultimateFileCount = 0;

QString PhotoPath1; //结果图片1路径
QString PhotoPath2; //结果图片2路径

///=============================================================================================================
/// struct stVisionJudgementDesc

//
QString stVisionJudgementDesc::toStr(bool _is_format)
{
    QString str;

    if (R.length() > 0) {
        str += QCoreApplication::translate("result.cpp", "右眼: ") + R;  // "Right: "
    }

    if (L.length() > 0) {
        if (str.length() > 0) {
            str += "; ";
        }
        str += QCoreApplication::translate("result.cpp", "左眼: ") + L; // "Left: "
    }

    if (Both.length() > 0) {
        if (str.length() > 0) {
            str += (_is_format ? "\n" : "; ");
        }
        str += Both;
    }

    return str;
}

///=============================================================================================================
/// class Result

//
bool Result::s_isCylNegative = true;

//
Result::Result(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::Result)
{
    ui->setupUi(this);

    x_lblSeCaptionR = ui->lblSeCaptionR->x();
    x_lblSeCaptionL = ui->lblSeCaptionL->x();
    x_lblSeR = ui->lblSeR->x();
    x_lblSeL = ui->lblSeL->x();

    isShowStatusBar = true;

    //QFont font = qApp->font();
    //QList<QLabel *> qlabel = this->findChildren<QLabel *>();
    //foreach (QLabel *ql, qlabel)
    //{
    //    ql->setFont(font);
    //}

    //font.setPointSize(15);
    //font.setBold(true);
    //ui->lblJudgementR->setFont(font);
    //ui->lblJudgementL->setFont(font);
    //ui->lblJudgementBoth->setFont(font);

    //ui->pushButton_preview->setVisible(false);
    //ui->label_preview->setVisible(false);

    QFont resultFont = qApp->font();
    resultFont.setPointSize(10);

    rUD_label = new QLabel(this);
    rLR_label = new QLabel(this);
    lUD_label = new QLabel(this);
    lLR_label = new QLabel(this);
    rUD_label->setAlignment(Qt::AlignCenter);
    rLR_label->setAlignment(Qt::AlignCenter);
    lUD_label->setAlignment(Qt::AlignCenter);
    lLR_label->setAlignment(Qt::AlignCenter);
    rUD_label->setGeometry(15, 110, 60, 20);     //右眼上下斜视坐标
    rLR_label->setGeometry(15, 130, 60, 20);     //右眼左右斜视坐标
    lUD_label->setGeometry(725, 110, 60, 20);    //左眼上下斜视坐标
    lLR_label->setGeometry(725, 130, 60, 20);    //左眼左右斜视坐标
    rUD_label->setFont(resultFont);
    rLR_label->setFont(resultFont);
    lUD_label->setFont(resultFont);
    lLR_label->setFont(resultFont);

//    iniSetting = new QSettings("manylinks", QSettings::IniFormat);

    barcodeMode = false;

    //if (CGlobal::isReadBarcodeByQt) {
    //    connect(&readBarcode, &QTimer::timeout, this, &Result::barcodeHandle, Qt::QueuedConnection);
    //}

    timerAutoTest.setSingleShot(true);
    connect(&timerAutoTest, &QTimer::timeout, this, &Result::slot_timerAutoTest_timeout, Qt::QueuedConnection);

    keypressState = true;

    // 若不支持 A4 直连打印，则隐藏相关控件
    if (!CSysInfo::cups()) {
        ui->btnPrintA4->setVisible(false);
        ui->label_PrintA4->setVisible(false);

        // 左边的控件除了 Home 之外按依次右移
        ui->pushButton_preview->setGeometry(ui->btnSwichCylSign->geometry());
        ui->label_preview->setGeometry(ui->label_SwichCylSign->geometry());

        ui->btnSwichCylSign->setGeometry(ui->pushButton_edit->geometry());
        ui->label_SwichCylSign->setGeometry(ui->label_Edit->geometry());

        ui->pushButton_edit->setGeometry(ui->btnPrintA4->geometry());
        ui->label_Edit->setGeometry(ui->label_PrintA4->geometry());
    }

    // 信号槽连接
    QObject::connect(this, &Result::sigSaveResult, this, &Result::slot_this_SaveResult, Qt::QueuedConnection);
    QObject::connect(this, &Result::sigResultAbnormal, this, &Result::slotResultAbnormal, Qt::QueuedConnection);

    //
    m_winMultiResults = new WinMultiResults(this);
    m_winMultiResults->setParent(ui->wgtMultiResults);
    m_winMultiResults->setGeometry(0, 0, ui->wgtMultiResults->width(), ui->wgtMultiResults->height());
    ui->wgtMultiResults->raise();
    ui->wgtMultiResults->setVisible(false);

    // 开发时临时配置的恢复
    ui->lblJudgementR->setFrameShape(QFrame::NoFrame);
    ui->lblJudgementL->setFrameShape(QFrame::NoFrame);
    ui->lblJudgementBoth->setFrameShape(QFrame::NoFrame);

    ui->lblBg->setVisible(false);

    ui->wgtMultiResults->setStyleSheet("");

}

Result::~Result()
{
    delete ui;
}

void Result::setIsNeedSave(bool _is_need_save)
{
    isNeedSave = _is_need_save;

    // “预览”按钮的可见性
    ui->pushButton_preview->setVisible(!isNeedSave);
    ui->label_preview->setVisible(!isNeedSave);

    // “保存”按钮和“返回”按钮的切换
    updateView_SwitchSaveAndBackButton(isNeedSave);

}

void Result::setPatient(const CPatient &_pat)
{
    patient.cloneFrom(_pat);
}

void Result::setIsReliable(bool _is_reliable)
{
    isReliable = _is_reliable;
}

bool Result::getIsReliable()
{
    return isReliable;
}

void Result::slot_timerAutoTest_timeout()
{
    //timerAutoTest.stop();     /* singleShot，不需停止 */

    // 保存结果
    QString err_msg;
    bool succ_save = saveResult(patient, err_msg);
    if (!succ_save) {
        getWinManage()->showSuspensionPrompt(err_msg, -1);
    }

    // 如果是连拍模式，继续下一次测量
    if (g_AutoTest) {
        beforeBack();
        g_WinMeasure->continueMeasuring();
    }

}

void Result::updateView_SwitchSaveAndBackButton(bool _is_show_save)
{
    // “保存”按钮
    ui->pushButton_Save->setVisible(_is_show_save);
    ui->label_Save->setVisible(_is_show_save);

    // “返回”按钮
    ui->pushButton_back->setVisible(!_is_show_save);
    ui->label_Back->setVisible(!_is_show_save);
}

void Result::paintEvent(QPaintEvent *)
{
    QPainter painter_bg(this);

    if (themeType_Black == getSysThemeType()) {
        painter_bg.drawPixmap(QRect(0, STATUSBAR_HEIGHT, imgBg.width(), imgBg.height()), imgBg);
    } else{
        QPixmap resultImg1;
        QPainter resultP1(this);
        resultImg1.load(":/resource/white_theme/synthesize_w.png");
        QPixmap tempImg = imgBg.copy(0, 25, 800, 255);
        painter_bg.drawPixmap(QRect(0, 25, tempImg.width(), tempImg.height()), tempImg);
        resultP1.drawPixmap(QRect(105, 190, 244, 93), resultImg1);
        resultP1.drawPixmap(QRect(450, 190, 244, 93), resultImg1);
    }

    if (!CGlobal::isReducedVersion) {    // 裁减版，不显示斜视提示圆
        if(mStrabismus.getEnable())
        {
            double rUDval, rLRval, lUDval, lLRval;
            bool rUD, rLR, lUD, lLR;

            mStrabismus.getValue(rUDval, rLRval, lUDval, lLRval);
            mStrabismus.getDirection(rUD, rLR, lUD, lLR);

            QPainter painter_circle(this);
            QPen pen;
            pen.setWidth(5);
            pen.setColor(QColor(255, 132, 132));
            painter_circle.setPen(pen);
            static const float width_inner = 36, width_outer = 75;
            if(mStrabismus.getRightEyeState()){                             // 凝视提醒图形
                static const float x = 209.5, y = 100;
                QRectF insideCircle1(x - width_inner / 2, y - width_inner / 2, width_inner, width_inner);
                QRectF outerCircle1(x - width_outer / 2, y - width_outer / 2, width_outer, width_outer);
                painter_circle.drawEllipse(insideCircle1);
                painter_circle.drawEllipse(outerCircle1);
            }
            if(mStrabismus.getLeftEyeState()){
                static const float x = 594.5, y = 100;
                QRectF insideCircle2(x - width_inner / 2, y - width_inner / 2, width_inner, width_inner);
                QRectF outerCircle2(x - width_outer / 2, y - width_outer / 2, width_outer, width_outer);
                painter_circle.drawEllipse(insideCircle2);
                painter_circle.drawEllipse(outerCircle2);
            }
        }
    }

    QPainter painter(this);
    if(themeType_Black == getSysThemeType())
        painter.setBrush(QColor(150,150,150));  //灰白色
    else
        painter.setBrush(QColor(200,200,200));  //灰白色

    // 判断描述区域的颜色填充，区分正常计算值、统计值、和月龄屈光值
//    QRect rect_judgement = QRect(0, 283, 800, 80);

//    if (patient.Comment2 == "1") {
//        painter.setPen(QPen(QColor(0, 0, 0), 1, Qt::SolidLine));        // 月龄屈光值：边框线比较细，颜色全黑
//        painter.drawRect(rect_judgement);
//    } else if (patient.Comment2 == "2") {
//        painter.setPen(QPen(QColor(0, 0, 0), 1, Qt::DashDotLine));        // 统计值：边框线比较细，颜色全黑，虚线
//        painter.drawRect(rect_judgement);
//    } else {
//        painter.setPen(QPen(QColor(160, 160, 160), 2, Qt::SolidLine));  // 正常计算值：边框线较粗，颜色半灰
//        painter.drawRect(rect_judgement);
//    }

    // 瞳孔直径和眼位数据的分隔线
    static const int PS_GAIN_SEP_Y = 129;
    if (!CGlobal::isReducedVersion) {    /* 裁减版，不显示 */
        if(themeType_Black == getSysThemeType()) {
            QPainter pt(this);
            pt.setPen(QPen(QColor(180, 180, 183), 1, Qt::SolidLine));
            pt.drawLine(QPoint(22,  PS_GAIN_SEP_Y), QPoint(67,  PS_GAIN_SEP_Y));        // 右眼分隔线
            pt.drawLine(QPoint(730, PS_GAIN_SEP_Y), QPoint(775, PS_GAIN_SEP_Y));      // 左眼分隔线
        }
        else {
            QPainter pt(this);
            pt.setPen(QPen(QColor(80, 80, 80), 1, Qt::SolidLine));
            pt.drawLine(QPoint(22,  PS_GAIN_SEP_Y), QPoint(67,  PS_GAIN_SEP_Y));
            pt.drawLine(QPoint(730, PS_GAIN_SEP_Y), QPoint(775, PS_GAIN_SEP_Y));
        }
    }

}

// 显示眼位
void Result::setStrabismus(const CPatient &_pat, bool _has_right, bool _has_left, stVisionJudgementRst _right_comp, stVisionJudgementRst _left_comp)
{
    qDebug() << "---patient.patientid:" <<_pat.patientid;

    // 裁减版，不显示眼位
    if (CGlobal::isReducedVersion) {
        return;
    }

    //
    int rUDval = _pat.patientrightvs.toInt();      // 右眼垂直凝视
    int rLRval = _pat.patientrighths.toInt();      // 右眼水平凝视
    int lUDval = _pat.patientleftvs.toInt();       // 左眼垂直凝视
    int lLRval = _pat.patientlefths.toInt();       // 左眼水平凝视
    qDebug() << "---rUDval:" << rUDval << "rLRval:" << rLRval << "lUDval:" << lUDval << "lLRval:" << lLRval;

    bool rUDArrow = (rUDval >= 0);
    bool rLRArrow = (rLRval >= 0);
    bool lUDArrow = (lUDval >= 0);
    bool lLRArrow = (lLRval >= 0);

    // 箭头方向： 垂直方向：正表示上，负表示下；水平方向：正表示左，负表示右
    QString rUDstr, rLRstr, lUDstr, lLRstr;
    if(rUDArrow)
        rUDstr.append("↑ ");
    else
        rUDstr.append("↓ ");
    if(rLRArrow)
        rLRstr.append("←");
    else
        rLRstr.append("→");

    if(lUDArrow)
        lUDstr.append("↑ ");
    else
        lUDstr.append("↓ ");
    if(lLRArrow)
        lLRstr.append("←");
    else
        lLRstr.append("→");

    if(rUDval == 0) rUDstr = " ";
    if(rLRval == 0) rLRstr = " ";
    if(lUDval == 0) lUDstr = "  ";
    if(lLRval == 0) lLRstr = "  ";

    if (_has_right) {
        rUDstr.append(QString::number(abs(rUDval), 10) + "°");
        rLRstr.append(QString::number(abs(rLRval), 10) + "°");
    }

    if (_has_left) {
        lUDstr.append(QString::number(abs(lUDval), 10) + "°");
        lLRstr.append(QString::number(abs(lLRval), 10) + "°");
    }

    //
    mStrabismus.clear();

    //
    if(_right_comp.verticalGaze)
    {
        rUD_label->setStyleSheet("color:red;");
    }
    else
    {
        if(themeType_Black == getSysThemeType())
            rUD_label->setStyleSheet("color:rgb(204,204,204);");
        else if(themeType_White == getSysThemeType())
            rUD_label->setStyleSheet("color:rgb(109,109,113);");
    }

    if(_right_comp.nasalGaze || _right_comp.bitemporalGaze)
    {
        rLR_label->setStyleSheet("color:red;");
    }
    else
    {
        if(themeType_Black == getSysThemeType())
            rLR_label->setStyleSheet("color:rgb(204,204,204);");
        else if(themeType_White == getSysThemeType())
            rLR_label->setStyleSheet("color:rgb(109,109,113);");
    }

    //
    if(_left_comp.verticalGaze)
    {
        lUD_label->setStyleSheet("color:red;");
    }
    else
    {
        if(themeType_Black == getSysThemeType())
            lUD_label->setStyleSheet("color:rgb(204,204,204);");
        else if(themeType_White == getSysThemeType())
            lUD_label->setStyleSheet("color:rgb(109,109,113);");
    }

    if(_left_comp.nasalGaze || _left_comp.bitemporalGaze)
    {
        lLR_label->setStyleSheet("color:red;");
    }
    else
    {
        if(themeType_Black == getSysThemeType())
            lLR_label->setStyleSheet("color:rgb(204,204,204);");
        else if(themeType_White == getSysThemeType())
            lLR_label->setStyleSheet("color:rgb(109,109,113);");
    }

    //
    int gaze_deviation_r = std::ceil(std::sqrt(rUDval * rUDval + rLRval * rLRval));
    int gaze_deviation_l = std::ceil(std::sqrt(lUDval * lUDval + lLRval * lLRval));

    if (gaze_deviation_r > CGlobal::maxGazeDeviation) {
        mStrabismus.setRightEyeState(true);
    }

    if (gaze_deviation_l > CGlobal::maxGazeDeviation) {
        mStrabismus.setLeftEyeState(true);
    }

    //
    rUD_label->setText(rUDstr);
    rLR_label->setText(rLRstr);
    lUD_label->setText(lUDstr);
    lLR_label->setText(lLRstr);

    // 记下眼位数据
    mStrabismus.setDirection(rUDArrow, rLRArrow, lUDArrow, lLRArrow);
    mStrabismus.setValue(rUDval, rLRval, lUDval, lLRval);
    mStrabismus.setEnable(true);

    //
    this->update();
}

void Result::show_theme_state()
{
    //QPalette palette;
    if(themeType_Black == getSysThemeType()){
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_preview->setIcon(QIcon(":/resource/black_theme/preview_picture_b.png"));
        ui->pushButton_edit->setIcon(QIcon(":/resource/black_theme/edit_b.png"));
        ui->btnPrintReceipt->setIcon(QIcon(":/resource/black_theme/print-ticket_b.png"));
        ui->btnPrintA4->setIcon(QIcon(":/resource/black_theme/print-a4_b.png"));
        ui->pushButton_chongxin->setIcon(QIcon(":/resource/black_theme/retest_b.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/black_theme/save_b.png"));
        ui->pushButton_back->setIcon(QIcon(":/resource/black_theme/back_b.png"));

        //设置所有Label样式
        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p,list_Label)
        {
            QString  qstr = p->objectName();
            //qDebug()<<"------strstr1------"<<p->objectName();
            if(qstr.contains("label_",Qt::CaseSensitive) /*|| qstr.startsWith("lbl")*/)   //成功返回true 第二个参数表示是否大小写敏感
            {
                p->setStyleSheet("color:rgb(250,250,252);");
            }
            if(qstr.contains("_label",Qt::CaseSensitive))
            {
                p->setStyleSheet("color:rgb(204,204,204);");
            }
        }

        //
        ui->btnShowMultiResults->setStyleSheet(
                    QString() +
                    "QPushButton { background-color: #30FFFFFF; border-radius: 7px; } " +
                    "QPushButton:checked { background-color: #90FFFFFF; border-radius: 7px; }"
                    );
    }
    else if(themeType_White == getSysThemeType()){
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_preview->setIcon(QIcon(":/resource/white_theme/preview_picture_w.png"));
        ui->pushButton_edit->setIcon(QIcon(":/resource/white_theme/edit_w.png"));
        ui->btnPrintReceipt->setIcon(QIcon(":/resource/white_theme/print_big_w.png"));
        ui->btnPrintA4->setIcon(QIcon(":/resource/black_theme/print-a4_w.png"));
        ui->pushButton_chongxin->setIcon(QIcon(":/resource/white_theme/retest_w.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/white_theme/save_w.png"));
        ui->pushButton_back->setIcon(QIcon(":/resource/white_theme/back_w.png"));

        //设置所有Label样式
        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p,list_Label)
        {
            QString  qstr = p->objectName();
            //qDebug()<<"------strstr1------"<<p->objectName();
            if(qstr.contains("label_",Qt::CaseSensitive))   //成功返回true 第二个参数表示是否大小写敏感
            {
                p->setStyleSheet("color:rgb(1,1,1);");
            }
            if(qstr.contains("_label",Qt::CaseSensitive))
            {
                p->setStyleSheet("color:rgb(109,109,113);");
            }
        }
    }
    //this->setPalette(palette);

}

void Result::showEvent(QShowEvent *)
{
    //static int old_se_l = ui->lblSeL->x();
    //static int old_se_r = ui->lblSeR->x();

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    //
    //bool is_multi = m_pat.IS_MULTI;
    bool is_multi = CGlobal::isMultiMeasure;

    //
    ui->btnShowMultiResults->setChecked(false);
    ui->btnShowMultiResults->setVisible(is_multi);

    //
    ui->lblDebugInfo->raise();
    ui->lblDebugInfo->setVisible(CGlobal::isDebugMode);

    // 切换结果记录的按钮的有效性
    ui->pushButton_PrevPage->setVisible(historyListPtr);
    ui->pushButton_NextPage->setVisible(historyListPtr);

    //
    static const QString BG_IMG_BACK_BLACK = QStringLiteral(":/resource/black_theme/result_bg.png");
    static const QString BG_IMG_BACK_WHITE = QStringLiteral(":/resource/white_theme/eye_template_w.png");
    static QString bg_img_path;
    if (themeType_Black == getSysThemeType()) {
        if (BG_IMG_BACK_BLACK != bg_img_path) {
            bg_img_path = BG_IMG_BACK_BLACK;
            imgBg.load(bg_img_path);     // 背景图
        }
    } else {
        if (BG_IMG_BACK_WHITE != bg_img_path) {
            bg_img_path = BG_IMG_BACK_WHITE;
            imgBg.load(bg_img_path); // 背景图
        }
    }

    // 非缩减版，显示瞳孔直径单位
    ui->lblPsUnitR->setVisible(!CGlobal::isReducedVersion);
    ui->lblPsUnitL->setVisible(!CGlobal::isReducedVersion);
    if (!CGlobal::isReducedVersion) {
        QFont font_thin("Noto Sans SC", 11);        // TODO: 同样字体和尺寸，为什么在窗体设计器里的设置无效，而这里设置却有效？
        ui->lblPsUnitR->setFont(font_thin);
        ui->lblPsUnitL->setFont(font_thin);
    }

    // 裁减版，不显示瞳孔尺寸及眼位
    ui->rightPd_label->setVisible(!CGlobal::isReducedVersion);
    ui->leftPd_label->setVisible(!CGlobal::isReducedVersion);

    rUD_label->setVisible(!CGlobal::isReducedVersion);
    rLR_label->setVisible(!CGlobal::isReducedVersion);
    lUD_label->setVisible(!CGlobal::isReducedVersion);
    lLR_label->setVisible(!CGlobal::isReducedVersion);

    // 语言
    languageChange(patient);

    // 刷新标题
    refreshTitle(patient);

    // 批量筛查，查找并显示下一个被测者信息到标题栏
    if (isBatchScreen()) {
        // 获取批量筛查名单中的下一个未测量的被测者
        QString err_msg;
        CPatient pat;
        bool is_found = batchscr->getNextNotMeasured(patient.id, pat, err_msg);
        if (is_found) {
            // 显示下一位        /* 这块代码须写在 refreshTitle() 之后，否则会被覆盖 */
            QString title_sub;
            if (pat.patientid.length() > 0) {
                title_sub = tr("下一位: ") + pat.patientid + " " + pat.patientname;    // "next: "
            }
            WgtStatusBar::instance()->setTitleSub(title_sub);
        } else {
            //getWinManage()->showSuspensionPrompt(err_msg);
        }
    }

    // 异常大地数值，加上 ">" 或 "<" 表示       // TODO：这个处理，应当仅用于输出的显示，而不应当存储到数据库？这个处理过后，数值将无法再用于比较等运算？
    //CAlgoInvoker::scope_limitation(patient);

    // 载入数据
    //showVisionJudgementDesc();
    loadDataToUi(patient);
    qDebug() << "paint Result:" << getSysThemeType();

    //
    show_theme_state();         //更新控件状态

    // 根据是否显示参考视力调整界面
    adjustWidgetsByVisionNotation(CGlobal::visionNotation != visionNotation_None);

    //
    if (CGlobal::visionNotation == visionNotation_None) {
        //QRect rect = ui->lblSeL->geometry();
        //rect.setLeft(old_se_l);
        //ui->lblSeL->setGeometry(rect);

        //rect = ui->lblSeR->geometry();
        //rect.setLeft(old_se_r);
        //ui->lblSeR->setGeometry(rect);
    } else {
        //QRect rect = ui->lblSeL->geometry();
        //rect.setLeft(old_se_l - 40);
        //ui->lblSeL->setGeometry(rect);

        //rect = ui->lblSeR->geometry();
        //rect.setLeft(old_se_r - 40);
        //ui->lblSeR->setGeometry(rect);

        QString refer_vision_tip = tr("*参考视力仅根据屈光数据推算，不构成主观视力的任何依据");   // "* Reference vision is calculated only from refractive data and does not constitute any basis for subjective vision"
        ui->lblReferVisionTips->setText(refer_vision_tip);
    }

    //
    QWidget::update();

    // 自动保存
    //const enOperationMode op_mode = WinMeasure::getOperationMode();
    //if (!(op_mode == historyRecord || op_mode == batchRecord)) {      // TODO: 从历史或批量界面打开，然后重测，再返回，这里无法从现有状态值判断？opMode 值梳理；本对象的状态应独立；
    if (isNeedSave) {
        if (!CGlobal::getIsExternalControl()) {
            // 如果是自动连拍模式或是蓝牙指令开始测量且设置了自动保存，则启动定时器调用保存过程             // TODO: 自动连拍的控制，应该在上层结构中处理，而不应在此处
            if (g_AutoTest || (patientSource_Scanning == m_patientSource || patientSource_Command == m_patientSource)) {
                //const int INTERVAL_MIN = 3000;
                const int INTERVAL_MIN = 100;

                int interval = INTERVAL_MIN;
                if (g_AutoTest) {
                    if (CGlobal::captureInterval > 0) {
                        interval = std::max(interval, (int)(CGlobal::captureInterval * 1000 - g_WinMeasure->elapsedMeasure()));
                    }
                } else {
                    interval = 300;
                }

                timerAutoTest.start(interval);      // TODO: 自动保存结果的过程，不应用定时器？易使程序逻辑复杂化
            } else {
                // 若结果不可信，提示是否重测
                if (!getIsReliable()) {
                    emit sigResultAbnormal();
                }
            }
        } else {        // 受控版，立即发送筛查结果
            // 通过信号槽保存，使保存过程在 showEvent() 过程之后才执行
            emit sigSaveResult();       // TODO: 受控版，不需要保存结果？但是发送时还要查询，所以这里必须要先保存。    // TODO: 传输结果时没必要再查询？
        }
    }

    // 注册键盘侦听（用于扫码）
    globalService()->regKbReader(this, Result::callback_QrCodeScanned);

}

//
void Result::hideEvent(QHideEvent *)
{
    //
    mStrabismus.setEnable(false);

    timerAutoTest.stop();

    // 反注册键盘侦听（用于扫码）
    globalService()->unregKbReader(this);

    // 取消键盘输入侦听（支持扫码）
    //if (!CGlobal::isReadBarcodeByQt) {
    //    //if (inputMeasure != WinMeasure::getOperationMode())
    //    {
    //        //kbReader.disconnectKbListener(this, &WinScreen::on_kbReader_getline);
    //        QObject::disconnect(kbReader(), &Util::CKeyboardReader::sigGetLine, this, &Result::slotKbReaderGetline);
    //        kbReader()->UnregUser((long)this);
    //    }
    //}

    //
    m_patientSource = patientSource_Unknown;

    setIsReliable(true);

}

void Result::slot_this_SaveResult()
{
    QString err_msg;
    bool succ_save = saveResult(patient, err_msg);
    if (!succ_save) {
        getWinManage()->showSuspensionPrompt(err_msg, -1);
    }
}

void Result::slot_personInfo_finished(int _dialog_code)
{
    // 获取修改后的数据
    if (QDialog::Accepted == _dialog_code) {
        // 从编辑窗体读取更新后的值
        PersonalInfos *person_info = getWinManage()->getWindow<PersonalInfos>(WIN_PER);
        if (!person_info) {
#if (OS_TYPE == 2)
            getWinManage()->showSuspensionPrompt(tr("内部错误：获取“被测者信息”窗口失败")); // "Internal error: Failed to obtain 'Testee Information' window"
#endif
            getWinManage()->showSuspensionPrompt(tr("获取编辑后的值失败"), -1);  // "Failed to obtain modified values"
            return;
        }
        const CPatient &pat = person_info->getPatient();

        // 更新正在编辑的值
        QString number_old = patient.patientid;
        bool is_changed = updateEditingValues(pat);
        if (is_changed) {
            if (patient.id > 0) {                   /* 若 id 大于 0，表示修改已有记录，须整批修改 */
                // 同步修改数据库中所有同编号的记录的被测者信息
                PersonalInfos::editTesteeInfoOfNumber(number_old, pat);
            }
        }
    }

    // 刷新标题
    refreshTitle(patient);
}

// 根据是否显示参考视力调整界面
void Result::adjustWidgetsByVisionNotation(bool _is_refer_vision_shown)
{
    ui->lblReferVisionR->setVisible(_is_refer_vision_shown);
    ui->lblReferVisionL->setVisible(_is_refer_vision_shown);
    ui->lblReferVisionTips->setVisible(_is_refer_vision_shown);

    const int OFFSET = 70;

    int diff = (_is_refer_vision_shown ? 0 : OFFSET);

    QRect rect_caption_r = ui->lblSeCaptionR->geometry();
    rect_caption_r.moveLeft(x_lblSeCaptionR + diff);
    ui->lblSeCaptionR->setGeometry(rect_caption_r);

    QRect rect_caption_l = ui->lblSeCaptionL->geometry();
    rect_caption_l.moveLeft(x_lblSeCaptionL + diff);
    ui->lblSeCaptionL->setGeometry(rect_caption_l);

    QRect rect_r = ui->lblSeR->geometry();
    rect_r.moveLeft(x_lblSeR + diff);
    ui->lblSeR->setGeometry(rect_r);

    QRect rect_l = ui->lblSeL->geometry();
    rect_l.moveLeft(x_lblSeL + diff);
    ui->lblSeL->setGeometry(rect_l);

}

bool Result::callback_QrCodeScanned()
{
    //
    Result *form = globalService()->getResultWin();
    if (!form) {
        getWinManage()->showMsgWin("ProgramError: Failed to get WinResult!");
        return false;
    }

    // 若未保存，则提示，并拦截
    if (form->isNeedSave && !form->checkIsSaved(form->patient)) {
        getWinManage()->showMsgWin("请先保存数据！");  // "Please save the data first!"
        return false;
    } else {
        return true;
    }
}

bool Result::isBatchScreen()
{
    return (WinMeasure::getOperationMode() == operationMode_BatchScreen /*|| WinMeasure::getOperationMode() == batchReTest*/);
}

bool Result::printTicket(const CPatient &_pat, bool _is_check_conn, QString *_err_msg)
{
    // 检查打印机是否已连接
    if (_is_check_conn) {
        if (ticketPrintConnType_BT == CGlobal::ticketPrintConnType) {           // 蓝牙打印

            if (!g_Bluetooth->getBtPrinter()->getIsConnected()) {
                if (_err_msg) {
                    *_err_msg = tr("蓝牙打印机未连接！");   // "Bluetooth Printer not connected!"
                }
                return false;
            }
        } else if (ticketPrintConnType_WiFi == CGlobal::ticketPrintConnType) {  // wifi 打印
            if (printerTransmit::checkWifiPrinterConnect() == 1) {
                if (_err_msg) {
                    *_err_msg = tr("网络未连接！");   // "Network not connected!"
                }
                return false;
            } else if (printerTransmit::checkWifiPrinterConnect() == 2) {
                if (_err_msg) {
                    *_err_msg = tr("WiFi 打印机未连接！");   // "WiFi Printer not connected!"
                }
                return false;
            }
        }
    }

    // 生成视力判断       // TODO: 本函数可能是外部调用的，需重新生成视力判断？
    bool has_right, has_left;
    Result::judgeSingleDualEyeMode(_pat, &has_right, &has_left);
    stVisionJudgementRst right_comp, left_comp;
    Result::getVisionJudgementRst(_pat, has_right, has_left, right_comp, left_comp);
    m_judgementDescStr = Result::getVisionJudgementDesc(_pat, has_right, has_left, right_comp, left_comp).toStr(true);

    //
    switchCylSignAndPrintTicket(_pat, m_judgementDescStr);    //发送给数据给打印机

    //
    return true;
}

void Result::beforeBack()
{
    // 重置“是否批量筛查”
    WinMeasure::setOperationMode(operationMode_Unknown);

    // 重置“历史记录列表”指针
    historyListPtr = Q_NULLPTR;

}

void Result::setHistoryListPtr(const std::vector<CPatient> *_ptr)
{
    historyListPtr = _ptr;
}

void Result::on_pushButton_back_clicked()
{
    //
    const enOperationMode op_mode = WinMeasure::getOperationMode();

    //
    saveNotice(patient);
    qDebug() << "after myQMessageShow";

    //
    if (operationMode_NormalMeasure == op_mode) {
        batchhistory = 0;
        getWinManage()->showWindowByType(WIN_HOME);
    } else if (operationMode_HistoryRecord == op_mode /*|| normalReTest == op_mode*/) {
        batchhistory = 1;
//        emit sendSIGNAL(sysSignal_11);    // WinScreen
        getWinManage()->showWindowByType(WIN_PER_REC);
    } else if (operationMode_BatchRecord == op_mode /*|| batchReTest == op_mode*/) {
        batchhistory = 2;
//        emit sendSIGNAL(sysSignal_22);    // WinScreen
        getWinManage()->showWindowByType(WIN_SCREEN);
    } else {
        batchhistory = 0;
        getWinManage()->showWindowByType(WIN_HOME);
    }

    //
    beforeBack();

}

void Result::on_pushButton_Save_clicked()
{
    // 如果已启动自动保存定时器，则先关闭
    if (timerAutoTest.isActive()) {
        timerAutoTest.stop();
    }

    //
    QString err_msg;
    bool succ_save = saveResult(patient, err_msg);
    if (!succ_save) {
        getWinManage()->showSuspensionPrompt(err_msg, -1);
    }

    //
    qDebug() << "==opMode=" << WinMeasure::getOperationMode();
    if(WinMeasure::getOperationMode() == operationMode_NormalMeasure /*|| WinMeasure::getOperationMode() == normalReTest*/)
    {
        //saveResult();
        batchhistory = 1;
//        emit sendSIGNAL(sysSignal_11);    // WinScreen
        getWinManage()->showWindowByType(WIN_CLINIC);
    }
    else if(WinMeasure::getOperationMode() == operationMode_HistoryRecord || WinMeasure::getOperationMode() == operationMode_BatchRecord)
    {

    }
    else if(isBatchScreen())
    {
        //saveResult();
        batchhistory = 2;
//        emit sendSIGNAL(sysSignal_22);    // WinScreen
        getWinManage()->showWindowByType(WIN_SCREEN);
    }
    else  //4,5
    {
        //saveResult();
        WinMeasure::setOperationMode(operationMode_NormalMeasure);
        batchhistory = 4;
//        emit sendSIGNAL(sysSignal_22);    // WinScreen
        getWinManage()->showWindowByType(WIN_SCREEN);
    }

}

void Result::on_pushButton_Home_clicked()
{
    //
    saveNotice(patient);

    //
    getWinManage()->showWindowByType(WIN_HOME);

}

void Result::refreshTitle(const CPatient &_pat)
{
    getWinManage()->updateWindowTitle(this, tr("编号:") + " " + _pat.patientid + " " + _pat.patientname);  // "No.:"
}

void Result::loadDataToUi(const CPatient &_pat)
{
    qDebug() << "enter loadDataToUi";

    // 右、左眼是否需要计算
    bool has_right, has_left;
    Result::judgeSingleDualEyeMode(_pat, &has_right, &has_left);     /* 这里不能用 g_SingleDualEye，因为可能是浏览历史结果数据，单双眼模式已变 */

    // 右眼结果
    {
        QString sph_r, cyl_r, axis_r, se_r, ps_r, se_ref_r;
        if (CGlobal::visionNotation != visionNotation_None) {
            se_ref_r = "  *" + tr("参考视力:");        // "Vision:"
        }
        if (has_right) {
            CAlgoInvoker::switchCylSign_StrAx(_pat.patientrighteyesph, _pat.patientrighteyecyl, _pat.patientrighteyeax,
                                              isCylNegative(), sph_r, cyl_r, axis_r);
            se_r = _pat.patientrightse;
            ps_r = _pat.patientrightpd;
            if (CGlobal::visionNotation != visionNotation_None) {
                se_ref_r += " " + (has_right ? CAlgoInvoker::diopterToVision(_pat.patientrighteyesph, _pat.patientrighteyecyl, CGlobal::visionNotation.getValue()) : "");
            }
        }

        // 若柱镜度为 0（精度修整后），则柱镜、轴位都显示为空（包括结果界面、小票、导出、数据上传）
        if (Util::compDouble(_pat.patientrighteyecyl.toDouble(), 0) == 0) {
            cyl_r = "";
            axis_r = "";
        }

        //
        ui->label_rightds->setText(sph_r);
        ui->label_rightdc->setText(cyl_r);
        ui->label_rightaxis->setText(axis_r);
        ui->lblSeR->setText(se_r);
        ui->rightPd_label->setText(ps_r);

        if (CGlobal::visionNotation != visionNotation_None) {
            ui->lblReferVisionR->setText(se_ref_r);
        }
    }

    // 左眼结果
    {
        QString sph_l, cyl_l, axis_l, se_l, ps_l, se_ref_l;
        if (CGlobal::visionNotation != visionNotation_None) {
            se_ref_l = "  *" + tr("参考视力:");        // "Vision:"
        }
        if (has_left) {
            CAlgoInvoker::switchCylSign_StrAx(_pat.patientlefteyesph, _pat.patientlefteyecyl, _pat.patientlefteyeax,
                                              isCylNegative(), sph_l, cyl_l, axis_l);
            se_l = _pat.patientleftse;
            ps_l = _pat.patientleftpd;
            if (CGlobal::visionNotation != visionNotation_None) {
                se_ref_l += " " + (has_left ? CAlgoInvoker::diopterToVision(_pat.patientlefteyesph, _pat.patientlefteyecyl, CGlobal::visionNotation.getValue()) : "");
            }
        }

        // 若柱镜度为 0（精度修整后），则柱镜、轴位都显示为空（包括结果界面、小票、导出、数据上传）
        if (Util::compDouble(_pat.patientlefteyecyl.toDouble(), 0) == 0) {
            cyl_l = "";
            axis_l = "";
        }

        //
        ui->label_leftds->setText(sph_l);
        ui->label_leftdc->setText(cyl_l);
        ui->label_leftaxis->setText(axis_l);
        ui->lblSeL->setText(se_l);
        ui->leftPd_label->setText(ps_l);

        if (CGlobal::visionNotation != visionNotation_None) {
            ui->lblReferVisionL->setText(se_ref_l);
        }
    }

    // 瞳距
    QString pd;
    if (has_right && has_left) {
        pd = _pat.patientpd;
    }
    ui->pd_label->setText(pd);

    // 柱镜度符号
    ui->label_CylSign->setText(isCylNegative() ? "-CYL" : "+CYL");

    // 视力标准的比较
    stVisionJudgementRst right_comp, left_comp;
    Result::getVisionJudgementRst(_pat, has_right, has_left, right_comp, left_comp);

    // 载入眼位
    setStrabismus(patient, has_right, has_left, right_comp, left_comp);

    // 显示屈光判断描述
    showVisionJudgementDesc(patient, has_right, has_left, right_comp, left_comp);

    //
    m_winMultiResults->setData(_pat, has_right, has_left, isCylNegative());

    //
    if (CGlobal::isDebugMode) {
        ui->lblDebugInfo->setText(QString("exp:%1 us, pl avg:%2").arg(patient.comment1).arg(patient.Comment2));
    }

    //
    qDebug() << "after loadDataToUi";
}

bool Result::getNextResult(int _curr_id, CPatient &_pat, QString &_err_msg)
{
    if (historyListPtr) {
        int idx_curr = -1;
        for (uint i = 0; i < historyListPtr->size(); i++) {
            if (historyListPtr->at(i).id == _curr_id) {
                idx_curr = i;
                break;
            }
        }
        if (idx_curr < 0) {
            // 找不到
            _err_msg = tr("逻辑错误：找不到当前实体"); // "Logic error: Unable to find the current entity"
        } else {
            if (idx_curr == (int)historyListPtr->size() - 1) {
                // 是最后一个
                _err_msg = tr("已经是最后一个");   // "It's already the last one"
            } else {
                int idx_found = -1;
                for (uint i = idx_curr + 1; i < historyListPtr->size(); i++) {
                    if (historyListPtr->at(i).isTest) {
                        idx_found = i;
                        break;
                    }
                }

                if (idx_found >= 0) {
                    _pat = historyListPtr->at(idx_found);
                    return true;
                } else {
                    _err_msg = tr("找不到下一个测量结果");    // "Unable to find the next measurement result"
                    return false;
                }
            }
        }
    } else {
        _err_msg = tr("逻辑错误：数据为空"); // "Logic error: Data is empty"
    }
    return false;
}

bool Result::getPrevResult(int _curr_id, CPatient &_pat, QString &_err_msg)
{
    if (historyListPtr) {
        int idx_curr = -1;
        for (int i = 0; i < (int)historyListPtr->size(); i++) {
            if (historyListPtr->at(i).id == _curr_id) {
                idx_curr = i;
                break;
            }
        }
        if (idx_curr < 0) {
            // 找不到
            _err_msg = tr("逻辑错误：找不到当前实体");  // "Logic error: Unable to find the current entity"
        } else {
            if (idx_curr == 0) {
                // 是第一个
                _err_msg = tr("已经是第一个");    // "It's already the first one"
            } else {
                int idx_found = -1;
                for (int i = idx_curr - 1; i >= 0; i--) {
                    if (historyListPtr->at(i).isTest) {
                        idx_found = i;
                        break;
                    }
                }

                if (idx_found >= 0) {
                    _pat = historyListPtr->at(idx_found);
                    return true;
                } else {
                    _err_msg = tr("找不到上一个测量结果");    // "Unable to find the previous measurement result"
                    return false;
                }
            }
        }
    } else {
        _err_msg = tr("逻辑错误：数据为空"); // "Logic error: Data is empty"
    }
    return false;
}

void Result::languageChange(const CPatient &_pat)
{
    //if (language) {
    //    ui->label_Home->setText("主页");
    //    ui->label_preview->setText("图像预览");
    //    ui->label_Edit->setText("编辑");
    //    ui->label_PrintTicket->setText("小票打印");
    //    ui->label_PrintA4->setText("A4打印");
    //    ui->label_ReTest->setText("重新测量");
    //    ui->label_Back->setText("返回");
    //    ui->label_Save->setText("保存");
    //} else {
    //    ui->label_Home->setText("Home");
    //    ui->label_preview->setText("Preview");
    //    ui->label_Edit->setText("Edit");
    //    ui->label_PrintTicket->setText("PrintTicket");
    //    ui->label_PrintA4->setText("PrintA4");
    //    ui->label_ReTest->setText("Retest");
    //    ui->label_Back->setText("Back");
    //    ui->label_Save->setText("Save");
    //}

    Q_UNUSED(_pat)

    //
    if (isCylNegative()) {
        ui->label_SwichCylSign->setText(tr("负散光"));     // "-AST"
    } else {
        ui->label_SwichCylSign->setText(tr("正散光"));     // "+AST"
    }

}

bool Result::checkIsSaved(const CPatient &_pat)
{
    bool is_saved;  // 是否已保存

    if (_pat.id > 0) {          // 若 id 大于 0，则说明数据来自数据库，或已保存，那么需比较是否已修改
        MySQLitePatients *mysql = MySQLitePatients::getInstance();
        std::vector<CPatient> modifyPat;
        std::vector<CPatient> mypats = mysql->findRecordById(_pat.id);
        int myPaySize = mypats.size();

        if (myPaySize > 0) {
            CPatient mypat = mypats.at(0);
            QString name = mypat.patientname;
            QString sex = mypat.patientsex;
            enAgeRange age_range = mypat.getAgeRange();
            QString leftsph = mypat.patientlefteyesph;
            QString rightsph = mypat.patientrighteyesph;
            QString leftcyl = mypat.patientlefteyecyl;
            QString rightcyl = mypat.patientrighteyecyl;
            QString leftax = mypat.patientlefteyeax;
            QString rightax = mypat.patientrighteyeax;
            QString rightse = mypat.patientrightse;//edit by sun 20180808
            QString leftse = mypat.patientleftse;//edit by sun 20180808
            QString phone = mypat.patientPhone;
            QString wechat = mypat.patientWechat;
            QString address = mypat.patientAddress;
            QDate date = mypat.getBirthDate();
            if (_pat.patientname == name && _pat.patientsex == sex
                    && _pat.getAgeRange() == age_range && _pat.patientlefteyesph == leftsph
                    && _pat.patientrighteyesph == rightsph && _pat.patientlefteyecyl == leftcyl
                    && _pat.patientlefteyeax == leftax && _pat.patientrighteyecyl == rightcyl
                    && _pat.patientrighteyeax == rightax && _pat.patientrightse == rightse //edit by sun 20180808
                    && _pat.patientleftse == leftse && _pat.patientPhone == phone && _pat.patientWechat == wechat
                    && _pat.patientAddress == address && _pat.getBirthDate() == date)
            {                       // 若数据相等，则判定为已保存
                is_saved = true;
            } else {                // 若数据不相等，则判定为未保存
                is_saved = false;
            }
        } else {    // 若不为空的id未查到，则提示逻辑异常，判定为已保存
            getWinManage()->showSuspensionPrompt("LogicError: id of this record not found from database!", -1);
            is_saved = true;
        }
    } else {    // 若 id 为 0，则是新记录，判定为未保存
        is_saved = false;
    }

    return is_saved;
}

void Result::saveNotice(const CPatient &_pat)
{
    //
    if (!isNeedSave) {
        return;
    }

    //
    if (checkIsSaved(_pat)) {
        qDebug() << "nothting is changed!";
        isNeedSave = false;
        return;
    }

    //
    QString text = tr("是否保存?"); // "Save data?"
    bool ret = getWinManage()->showNoticeWin(text);
    if (ret) {  // 选择是
        QString err_msg;
        bool succ_save = saveResult(patient, err_msg);
        if (!succ_save) {
            getWinManage()->showSuspensionPrompt(err_msg, -1);
        }
    } else {
        isNeedSave = false;     // 选择不保存，则置“需要保存”为false
    }

}

bool Result::saveResult(CPatient &_pat, QString &_err_msg)
{
    if (!isNeedSave) {
        logWarning("Result::saveResult(): isNeedSave = false, returned!");
        return true;
    }

    //
    qDebug() << "in saveResult 0";
    keypressState = false;
    QString text = tr("正在保存...");   // "Saving data..."
    int msg_id = getWinManage()->showMsgWin(text);

    qApp->processEvents();
    usleep(100);

    // PDF报表预览图（经过直方图均衡化处理的）的正式保存（从临时路径移动到正式路径）
    //if (g_isSavePreviewImage ||
    //        (DataTransmiter::IsUploadImage && connMode_Http == DataInterfaceCfg_to_ConnMode(WinDataTrans::getCfg_intfType()))
    //        ) {
    //    QString currentPreFile("/media/pdfPreviewImg/current_pdfPreview.jpg");
    //    QString new_img_path = UtilApp::getPreviewImgPath(_pat);
    //    QFile previewImg(currentPreFile);
    //    if(previewImg.exists())
    //    {
    //        // 转移 PDF 图像
    //        QString cmd = "mv " + currentPreFile + " " + new_img_path;
    //        system(cmd.toLatin1().data());
    //
    //        // 生成报表
    //        //Result::savePdfReport(_pat);      // NOTE: PDF 文件改为在需要时即时生成
    //     }
    //}

    //
    MySQLitePatients *mysql = MySQLitePatients::getInstance();

    //
    qDebug() << "in saveResult 1";

    bool is_need_upload = (CGlobal::getIsExternalControl() ? true : DataTransmiter::IsPostImmediately);

    _pat.isNeedUpload = is_need_upload;
    _pat.isTest = true;

    if (0 == _pat.id) {
        qDebug() << "insertHistory()!";
        bool is_succ_insert = mysql->insertHistory(_pat, true);         // 数据库记录的新增
        if (!is_succ_insert) {
            logWarning(QString("%1: insert new record failed!").arg(__PRETTY_FUNCTION__), CGlobal::LOG_MEASURE);
            _err_msg = tr("插入新记录失败！");   // "Insert new record failed!"
            getWinManage()->hideMsgWin(msg_id, true);
            return false;
        }
    } else {
        qDebug() << "modify id:" << _pat.id;
        qDebug() << "modify patientname:" << _pat.patientname << "patient batch:" << _pat.isBatch;
        std::vector<CPatient> pat_list;
        pat_list.push_back(_pat);
        mysql->TableModify(pat_list);       // 数据库记录的修改
    }

    //
    qDebug() << "in saveResult 3";
    QString err_msg;
    if (is_need_upload) {
        if (g_uploadThread->checkIsConnected(err_msg)) {
            // 得到记录新增到数据表自动生成的主键 id 值（否则后面的数据上传得不到 id 值）


            // 自动上传
            uploadResultData(_pat);
        } else {
            qDebug() << "AutoUpload skipped: " << err_msg;
        }
    }

    //
    keypressState = true;

    // 打印结果小票
    if (CGlobal::isAutoPrintTicket) {
        QString err_msg;
        bool succ = printTicket(patient, true, &err_msg);
        if (!succ) {
            //getWinManage()->showMsgWin(err_msg);
        }
    }

    //
    isNeedSave = false;

    // 保存后，将保存按钮切换为返回按钮
    updateView_SwitchSaveAndBackButton(false);

    // 每次保存结果后，刷新 存储 使用率，使该值及时更新
    RunningStatus::refreshStorageRate();

    //
    getWinManage()->hideMsgWin(msg_id, true);
    return true;
}

void Result::uploadResultData(const CPatient &_pat)
{
    //
    QVector<int> ids;
    ids.push_back(_pat.id);

//
#if (BLUETOOTH_TYPE == 2)
    /* RK BT */
    // 若需要上传，且是蓝牙方式
    if (connMode_Bluetooth == DataTransmiter::ConnMode) {
        // 显示等待提示框
        QString text = tr("正在等待蓝牙上传结束...");   // "Waiting for bluetooth upload finished..."
        m_msgIdOfWaitingBtUpload = getWinManage()->showMsgWin(text, false);
    }
#endif

    //
    emit sigUpLoadData(ids);
    qDebug() << "start upload :" << ids.at(0);

    // 等待上传结束
    Util::waitMs(3000, &m_msgIdOfWaitingBtUpload, -1);

}

void Result::upLoadCallback(string info)
{
    if(info != "")
    {
        qDebug() << "Result::call back info:" << QString::fromStdString(info);
    }
    else
    {
        qDebug() << "Result::call back info is empty!";
    }
}

void Result::on_pushButton_edit_clicked()
{
    /*
    Edit editdialog;
    editdialog.setWindowModality(Qt::WindowModal);
    if(editdialog.exec()== QDialog::Accepted){
        qDebug()<<"editdialog.exec()== QDialog::Accepted!";
        getWinManage()->updateWindowTitle(this, "编号:"+patient.patientid +" "+ patient.patientname);
        WgtStatusBar::getInstance->setTitle();
        this->update();
    }
    */

    // 如果是还未保存到数据库的结果数据，编辑前须先插入新的测量结果（原因见 PersonalInfos::saveDataToDB() 的备注）
    //if (patient.id == 0 && isNeedSave) {
    //    bool ret = getWinManage()->showNoticeWin(tr("须先保存测量结果。\n是否保存？"));   // "Measurement result must be saved first. \nSave it now?"
    //    if (!ret) {
    //        return;
    //    }
    //
    //    // 先保存新的测量结果
    //    saveResult(patient);
    //}

    // 若是旧记录，不允许在此界面编辑（因为被测者和测量记录未分表，且此界面的编辑是返回后再保存，无法批量保存）
    //bool is_enable_edit = (patient.id <= 0);
    //if (!is_enable_edit) {
    //    getWinManage()->showMsgWin(tr("只有新结果的被测者信息才允许在此界面编辑。"));        // "Only new results are allowed to edit the test subject's information on this interface."
    //    return;
    //}

    //
    PersonalInfos *person_info = PersonalInfos::getPersonalInfoWin(PersonalInfos::modeFlag_EditToEntity, m_patientSource, &patient, "");
    getWinManage()->showWidgetAsDialogProcess<Result>(person_info, this, &Result::slot_personInfo_finished);
}

void Result::receieve()
{
    qDebug() << "received:" << patient.patientlefteyeax << "," << patient.patientrighteyeax;
    ui->pd_label->setText(patient.patientpd);
    ui->lblSeL->setText(patient.patientleftse);
    ui->label_leftds->setText(patient.patientlefteyesph);
    ui->label_leftdc->setText(patient.patientlefteyecyl);
    ui->label_leftaxis->setText(patient.patientlefteyeax);
    ui->lblSeR->setText(patient.patientrightse);
    ui->label_rightds->setText(patient.patientrighteyesph);
    ui->label_rightdc->setText(patient.patientrighteyecyl);
    ui->label_rightaxis->setText(patient.patientrighteyeax);
    ui->leftPd_label->setText(patient.patientleftpd);
    ui->rightPd_label->setText(patient.patientrightpd);
}

void Result::on_btnPrintA4_clicked()
{
    // 若未保存，先提示并保存
    if (isNeedSave) {               // TODO: 这个判断方法有无逻辑漏洞？
        bool is_save = getWinManage()->showNoticeWin(tr("需先保存数据。是否继续？"));   // "Data needs to be saved first.\n Continue?"
        if (is_save) {
            QString err_msg;
            bool succ_save = saveResult(patient, err_msg);
            if (!succ_save) {
                getWinManage()->showSuspensionPrompt(err_msg, -1);
            }
        } else {
            return;
        }
    }

    // 打印 A4 报表
    printA4Report(patient);

}

void Result::on_btnPrintReceipt_clicked()
{
    qDebug() << "print " << patient.patientid;

    //
    ui->btnPrintReceipt->setEnabled(false);

    //
    if (ticketPrintConnType_BT == CGlobal::ticketPrintConnType) {           // 蓝牙打印
        if (!g_Bluetooth->getBtPrinter()->getIsConnected()) {
            qDebug()<<"bluetooth is disconnect!";
            QString text = tr("打印机未连接！");   // "Printer disconnect!"
            getWinManage()->showMsgWin(text);
            ui->btnPrintReceipt->setEnabled(true);
            return;
        }
    } else if (ticketPrintConnType_WiFi == CGlobal::ticketPrintConnType) {  // wifi打印
#if (2 != OS_TYPE)
        QString text;
        if(printerTransmit::checkWifiPrinterConnect() == 1) {
            qDebug()<<"Network is disconnect!";
            text = tr("网络未连接!");    // "Network is disconnect!"
            getWinManage()->showMsgWin(text);
            ui->btnPrintReceipt->setEnabled(true);
            return;
        } else if(printerTransmit::checkWifiPrinterConnect() == 2) {
            qDebug()<<"Abnormal network connection!";
            text = tr("打印机连接失败！");  // "Failed to connect to printer!"
            getWinManage()->showMsgWin(text);
            ui->btnPrintReceipt->setEnabled(true);
            return;
        }
#endif
    }

    //
    QString err_msg;
    bool succ = printTicket(patient, false, &err_msg);
    if (!succ) {
        getWinManage()->showMsgWin(err_msg);
    }

    //
    ui->btnPrintReceipt->setEnabled(true);
}

// 导出pdf    /* 已废弃，改用 CReport::generatePdfReport() */
//void Result::generatePdf(QString imgFilename)
//{

//    QFont font0 = qApp->font();

//    QDir pdfPath(PDF_REPORT_DIR);
//    if(!pdfPath.exists())
//    {
//        pdfPath.mkdir(PDF_REPORT_DIR);
//    }
//    QFile pdfFile(QString(PDF_REPORT_DIR) + QDir::separator() + patient.patientid + "_result.pdf");
//    pdfFile.open(QIODevice::WriteOnly);               // 打开要写入的pdf文件
//    QPdfWriter *pPdfWriter = new QPdfWriter(&pdfFile);  // 创建pdf写入器
//    pPdfWriter->setPageSize(QPagedPaintDevice::A4);     // 设置纸张为A4
//    pPdfWriter->setResolution(300);                     // 设置纸张的分辨率为300,因此其像素为3508X2479
//    QPainter painterText;
//    painterText.begin(pPdfWriter);

//    //    painterText.setPen(QColor(167,203,220,220));
//    painterText.setPen(QPen(QColor(167, 203, 220, 220), 6, Qt::SolidLine)); //draw rect
//    painterText.drawRect(100, 200, 2200, 360);

//    QPoint pointzero(170, 350);                               //VISION SCREENER
//    QString title1 = "VISION SCREENER";
//    font0.setPointSize(26);
//    //    font0.setWeight(900);
//    font0.setBold(true);
//    painterText.setPen(QPen(QColor(167, 203, 220, 220))); /*QColor(167,203,220,220)*/
//    painterText.setFont(font0);
//    painterText.drawText(pointzero, title1);
//    title1.clear();


//    QPoint pointzero1(170, 475);                               //视力筛查报告
//    QString title11 = "视力筛查报告";
//    painterText.setPen(QColor(55, 85, 125));
//    font0.setPointSize(26);
//    painterText.setFont(font0);
//    painterText.drawText(pointzero1, title11);
//    title11.clear();

//    QPoint pointone(1500, 400);                               //姓名
//    QString title2 = "姓    名: " + patient.patientname;
//    painterText.setPen(QColor(55, 85, 125));
//    QFont font1 = qApp->font();
//    int fontSize = 14;
//    font1.setPointSize(fontSize);
//    painterText.setFont(font1);
//    painterText.drawText(pointone, title2);

//    title2.clear();

//    QPoint pointone1(1500, 500);                               //测试时间
//    QString title22 = "测试时间: " + patient.patienttesttime;
//    painterText.drawText(pointone1, title22);
//    title22.clear();

//    painterText.setPen(QColor(167, 203, 220, 240));
//    QPoint point(170, 650);                               // 被测者信息
//    QString title = "被测者信息";
//    QFont font = qApp->font();
//    //    font.setFamily("simhei.ttf");
//    fontSize = 14;
//    font.setPointSize(fontSize);
//    painterText.setFont(font);
//    painterText.drawText(point, title);
//    title.clear();

//    QPoint point0(170, 750);                               //ID
//    QString ID = QString("被测者ID");
//    fontSize = 10;
//    painterText.setPen(QColor(55, 85, 125));
//    font.setPointSize(fontSize);
//    painterText.setFont(font);
//    painterText.drawText(point0, ID);
//    ID = patient.patientid;
//    font.setPointSize(14);
//    painterText.setFont(font);
//    painterText.drawText(QPoint(550, 755), ID);
//    ID.clear();

//    QPoint point1(170, 850);                               //sex
//    QString sex = "性别";
//    font.setPointSize(10);
//    painterText.setFont(font);
//    painterText.drawText(point1, sex);
//    sex = patient.getSexDisc(language);
//    font.setPointSize(14);
//    painterText.setFont(font);
//    painterText.drawText(QPoint(550, 855), sex);
//    sex.clear();

//    QPoint point2(170, 950);                               //ageRange
//    QString age_range_str = QString("年龄段");
//    fontSize = 10;
//    font.setPointSize(fontSize);
//    painterText.setFont(font);
//    painterText.drawText(point2, age_range_str);
//    age_range_str = patient.patientagerange;
//    font.setPointSize(14);
//    painterText.setFont(font);
//    painterText.drawText(QPoint(550, 955), age_range_str);
//    age_range_str.clear();

//    QPoint point3(170, 1050);                               //birth date
//    QString birthDate = QString("出生日期");
//    fontSize = 10;
//    font.setPointSize(fontSize);
//    painterText.setFont(font);
//    painterText.drawText(point3, birthDate);
//    birthDate = patient.patientdate;
//    font.setPointSize(14);
//    painterText.setFont(font);
//    painterText.drawText(QPoint(550, 1055), birthDate);
//    birthDate.clear();
//    qDebug() << "start draw pdfPreviewImg";

//    QDir previewImgDir("/media/pdfPreviewImg");
//    if(!previewImgDir.exists())
//    {
//        previewImgDir.mkdir("/media/pdfPreviewImg");
//    }
//    else
//    {
//        QFile imgFile(imgFilename);
//        if(imgFile.exists())
//        {
//            static constexpr int PDF_IMG_WIDTH = 800;
//            static constexpr int PDF_IMG_HEIGHT = 480;

//            QImage img(imgFilename);      //pdfPreview.bmp

//#if (1 == CAMERA_TYPE)
//            int left = 1500, top = 650, width = PDF_IMG_WIDTH, height = 480;
//#else
//            int left = 1500, top = 685, width = PDF_IMG_WIDTH, height = PDF_IMG_WIDTH * ((double)img.height() / img.width());
//#endif

//            top += (PDF_IMG_HEIGHT - height) / 2;

//            painterText.drawImage(QRect(left, top, width, height), img);
//            qDebug() << "imgFilename exist:" << imgFile.fileName();
//        }
//        else
//            qDebug() << "doesn't exist:" << imgFile.fileName();
//    }
//    painterText.setPen(QPen(QColor(167, 203, 220, 60), 60, Qt::SolidLine));         //draw line
//    painterText.drawLine(150, 1250, 2250, 1250);
//    qDebug() << "drawed pdfPreviewImg";

//    QString back_img_file_path = CGlobal::isReducedVersion ? ":/resource/eyes_reduced.jpg" : ":/resource/eyes.jpg";    // 裁减版，不显示瞳孔尺寸
//    QImage pp(back_img_file_path);                            //result interface
//    pp = pp.scaled(1300, 480);
//    painterText.drawImage(QRect(170, 1400, pp.width(), pp.height()), pp);
//    qDebug() << "draw eyes.jpg";

//    ////////////////// result data /////////////////////

//    painterText.setPen(QColor(167, 203, 220, 240));
//    font.setPointSize(14);
//    painterText.setFont(font);
//    painterText.drawText(QPoint(170, 1370), "筛查结果");

//    font.setPointSize(8);
//    painterText.setFont(font);
//    painterText.setPen(QColor(0, 0, 0));

//    double rAxi = patient.patientrighteyeax.toDouble();
//    int rAxiInt = rAxi;
//    QString rightAxi = QString::number(rAxiInt, 10);
//    double lAxi = patient.patientlefteyeax.toDouble();
//    int lAxiInt = lAxi;
//    QString leftAxi = QString::number(lAxiInt, 10);

//    if (!CGlobal::isReducedVersion) {    // 裁减版，不显示瞳孔尺寸
//        painterText.drawText(QPoint(220, 1470), patient.patientrightpd);
//        painterText.drawText(QPoint(1370, 1470), patient.patientleftpd);
//    }
//    painterText.drawText(QPoint(800, 1470), patient.patientpd);
//    painterText.drawText(QPoint(490, 1760), patient.patientrightse);
//    painterText.drawText(QPoint(1070, 1760), patient.patientleftse);
//    painterText.drawText(QPoint(390, 1860), patient.patientrighteyesph);
//    painterText.drawText(QPoint(490, 1860), patient.patientrighteyecyl);
//    painterText.drawText(QPoint(630, 1860), rightAxi);
//    painterText.drawText(QPoint(940, 1860), patient.patientlefteyesph);
//    painterText.drawText(QPoint(1070, 1860), patient.patientlefteyecyl);
//    painterText.drawText(QPoint(1205, 1860), leftAxi);

//    painterText.setPen(QPen(QColor(167, 203, 220, 60), 60, Qt::SolidLine));         //draw line
//    painterText.drawLine(150, 2050, 2250, 2050);

//    painterText.setPen(QPen(QColor(55, 85, 125), 3, Qt::SolidLine));            //draw line
//    painterText.drawLine(150, 3000, 1600, 3000);//x
//    painterText.drawLine(150, 3000, 150, 2200); //y

//    int age_range = patient.patientagerange.toInt();
//    font.setPointSize(8);
//    painterText.setFont(font);
//    painterText.setPen(QPen(QColor(55, 85, 125), 4, Qt::DotLine));          //draw line
//    if(age_range <= 1)
//    {
//        painterText.drawText(QPoint(1600, 2810), "在范围内");
//        painterText.drawLine(150, 2800, 1580, 2800);
//        painterText.drawLine(150, 2600, 1580, 2600);
//        painterText.drawLine(150, 2400, 1580, 2400);

//        painterText.setPen(Qt::NoPen);
//        painterText.setBrush(QColor(185, 245, 195, 100));
//        painterText.drawRoundedRect(QRect(150, 2800, 1430, 200), 25, 25);
//        painterText.setBrush(QColor(167, 203, 220, 60));
//        painterText.drawRoundedRect(QRect(150, 2600, 1430, 200), 25, 25);
//        painterText.drawRoundedRect(QRect(150, 2400, 1430, 200), 25, 25);
//        painterText.drawRoundedRect(QRect(150, 2200, 1430, 200), 25, 25);

//    }
//    else if(age_range > 1)
//    {
//        painterText.drawText(QPoint(1600, 2910), "在范围内");
//        painterText.drawLine(150, 2900, 1580, 2900);
//        painterText.drawLine(150, 2700, 1580, 2700);
//        painterText.drawLine(150, 2500, 1580, 2500);
//        painterText.drawLine(150, 2300, 1580, 2300);

//        painterText.setPen(Qt::NoPen);
//        painterText.setBrush(QColor(185, 245, 195, 100));
//        painterText.drawRoundedRect(QRect(150, 2900, 1430, 100), 25, 25);
//        painterText.setBrush(QColor(167, 203, 220, 60));
//        painterText.drawRoundedRect(QRect(150, 2700, 1430, 200), 25, 25);
//        painterText.drawRoundedRect(QRect(150, 2500, 1430, 200), 25, 25);
//        painterText.drawRoundedRect(QRect(150, 2300, 1430, 200), 25, 25);

//    }

//    painterText.setPen(QPen(QColor(55, 85, 125)));
//    painterText.drawText(QPoint(300 - 5, 3050), "远视");
//    painterText.drawText(QPoint(500 - 5, 3050), "近视");
//    painterText.drawText(QPoint(700 - 5, 3050), "散光");
//    painterText.drawText(QPoint(950 - 5, 3050), "远视");
//    painterText.drawText(QPoint(1150 - 5, 3050), "近视");
//    painterText.drawText(QPoint(1350 - 5, 3050), "散光");

//    painterText.drawText(QPoint(500 - 5, 3110), "右眼");
//    painterText.drawText(QPoint(1150 - 5, 3110), "左眼");

//    double mRatio, hRatio, cylRatio; //mRatio 近视   hRatio 远视   cylRatio 散光

//    if(age_range == 0)
//    {
//        mRatio = 200 / 2;
//        hRatio = 200 / 3.5;
//        cylRatio = 200 / 2.25;
//    }
//    else if(age_range == 1)
//    {
//        mRatio = 200 / 2;
//        hRatio = 200 / 3;
//        cylRatio = 200 / 2;
//    }
//    else if(age_range == 2)
//    {
//        mRatio = 100 / 1.25;
//        hRatio = 100 / 2.5;
//        cylRatio = 100 / 1.75;
//    }
//    else if(age_range == 3)
//    {
//        mRatio = 100 / 1;
//        hRatio = 100 / 2.5;
//        cylRatio = 100 / 1.5;
//    }
//    else if(age_range == 4)
//    {
//        mRatio = 100 / 0.75;
//        hRatio = 100 / 1.5;
//        cylRatio = 100 / 1.5;
//    }

//    painterText.setPen(Qt::NoPen);              //draw line
//    painterText.setBrush(QColor(167, 203, 220, 180));
//    double rSph = patient.patientrighteyesph.toDouble();
//    double rCyl = patient.patientrighteyecyl.toDouble();
//    //double rPd = patient.patientrightpd.toDouble();

//    double lSph = patient.patientlefteyesph.toDouble();
//    double lCyl = patient.patientlefteyecyl.toDouble();
//    //double lPd = patient.patientleftpd.toDouble();

//    painterText.setBrush(QColor(167, 203, 220, 60));
//    painterText.drawRoundedRect(QRect(250, 3070, 550, 60), 15, 15);
//    painterText.drawRoundedRect(QRect(900, 3070, 550, 60), 15, 15);

//    painterText.setBrush(QColor(55, 85, 125));
//    int rLineSph;
//    if(rSph >= 0)
//    {
//        rLineSph = rSph * hRatio;
//        painterText.drawRect(QRect(250, (3000 - rLineSph), 150, rLineSph));
//    }
//    else
//    {
//        rLineSph = (-rSph) * mRatio;
//        painterText.drawRect(QRect(450, (3000 - rLineSph), 150, rLineSph));
//    }
//    int rLineCyl = (-rCyl) * cylRatio;
//    painterText.drawRect(QRect(650, (3000 - rLineCyl), 150, rLineCyl));

//    int lLineSph;
//    if(lSph >= 0)
//    {
//        lLineSph = lSph * hRatio;
//        painterText.drawRect(QRect(900, (3000 - lLineSph), 150, lLineSph));
//    }
//    else
//    {
//        lLineSph = (-lSph) * mRatio;
//        painterText.drawRect(QRect(1100, (3000 - lLineSph), 150, lLineSph));

//    }
//    int lLineCyl = (-lCyl) * cylRatio;
//    painterText.drawRect(QRect(1300, (3000 - lLineCyl), 150, lLineCyl));

//    painterText.setPen(QColor(55, 85, 125));
//    font.setPointSize(8);
//    painterText.setFont(font);

//    painterText.drawText(QPoint(1250, 3380), "注:筛查结果不能替代专业医生或验光师的全面检查");

//    QImage logo(":/resource/logo.png");    //result
//    logo = logo.scaled(230, 190);
//    painterText.drawImage(QRect(2000, 3200, logo.width(), logo.height()), logo);
//    qDebug() << "draw logo";
//    painterText.end();

//    delete pPdfWriter;
//    pPdfWriter = nullptr;

//    pdfFile.close();


//}

void Result::on_pushButton_chongxin_clicked()
{
    //if (WinMeasure::getOperationMode() == historyRecord || WinMeasure::getOperationMode() == normalMeasure ) {
    //    //WinMeasure::setOperationMode(normalReTest); //back to history after retest
    //    WinMeasure::setOperationMode(normalMeasure);
    //} else if (WinMeasure::getOperationMode() == batchRecord || WinMeasure::getOperationMode() == batchScreen) {
    //    //WinMeasure::setOperationMode(batchReTest);
    //    WinMeasure::setOperationMode(batchScreen);
    //}
    qDebug()<<"-----模式:"<<WinMeasure::getOperationMode();
    getWinManage()->openMeasureWin(patient, m_patientSource);
}

/**
 * @brief 根据已有的测量结果数值判断单双眼模式（因为现有的数据库结果里没有记录这个值）
 * @param _pat
 * @param _right: 是否有右眼数据
 * @param _left:  是否有左眼数据
 */
enSingleDualEyeMode Result::judgeSingleDualEyeMode(CPatient _pat, bool *_has_right, bool *_has_left)
{
    //
    enSingleDualEyeMode single_dual_eye = singleDualEyeMode_Both;
    bool has_right = true;
    bool has_left = true;

    //
    if (_pat.isTest && Util::compDouble(_pat.patientpd.toDouble(), 0) == 0) {      // NOTE: 若已测，且无瞳距，则必是单眼模式的测量结果
        has_right = !_pat.patientrightpd.isEmpty();
        has_left = !_pat.patientleftpd.isEmpty();
    }

    //
    if (!has_left || !has_right) {
        if (has_left) {
            single_dual_eye = singleDualEyeMode_Left;
        } else if (has_right) {
            single_dual_eye = singleDualEyeMode_Right;
        } else {
            //logWarning("logic error: ")
        }
    }

    //
    if (_has_right) {
        *_has_right = has_right;
    }
    if (_has_left) {
        *_has_left = has_left;
    }

    return single_dual_eye;
}

// 对测量结果进行裁减（裁减版需求：屏蔽瞳孔尺寸、眼位、上睑下垂）
void Result::reduceResult(CPatient &_pat)
{
    if (CGlobal::isReducedVersion) {
        _pat.patientrightpd = "";
        _pat.patientrighths = "";
        _pat.patientrightvs = "";
        _pat.patientrightptosis = false;     // TODO: 这个没法清掉，还要单独处理？

        _pat.patientleftpd = "";
        _pat.patientlefths = "";
        _pat.patientleftvs = "";
        _pat.patientleftptosis = false;
    }
}

void Result::reduceResult(std::vector<CPatient> &_pats)
{
    for (size_t i = 0; i < _pats.size(); i++) {
        Result::reduceResult(_pats[i]);
    }
}

void Result::patientToReportData(const CPatient &_pat, const stVisionJudgementDesc &_judgement_desc, stReportData &_report_data)
{
    //
    QString str_grade, str_class;
    ThreadModel::splitGradeAndClass(_pat.patientstuclass, str_grade, str_class);

    enSex sex = (_pat.patientsex == SEX_CODE_MALE ? sex_Male : sex_Fefale);

    //
    _report_data.patientstugrade    = str_grade;
    _report_data.patientstuclass    = str_class;

    _report_data.patientid          = _pat.patientid            ;
    _report_data.patientname        = _pat.patientname          ;
    _report_data.patientsex         = CSex::getDiscrip(sex);
    _report_data.patientdate        = _pat.getBirthDateStr();
    _report_data.patientagerange    = QString::number((int)_pat.getAgeRange());
    _report_data.patienttesttime    = _pat.patienttesttime      ;

    _report_data.patientrighteyeax  = _pat.patientrighteyeax    ;
    _report_data.patientlefteyeax   = _pat.patientlefteyeax     ;

    _report_data.patientrightpd     = _pat.patientrightpd       ;
    _report_data.patientleftpd      = _pat.patientleftpd        ;

    _report_data.patientpd          = _pat.patientpd            ;

    _report_data.patientrightse     = _pat.patientrightse       ;
    _report_data.patientleftse      = _pat.patientleftse        ;

    _report_data.patientrighteyesph = _pat.patientrighteyesph   ;
    _report_data.patientlefteyesph  = _pat.patientlefteyesph    ;

    _report_data.patientrighteyecyl = _pat.patientrighteyecyl   ;
    _report_data.patientlefteyecyl  = _pat.patientlefteyecyl    ;

    _report_data.patientrighths     = _pat.patientrighths       ;
    _report_data.patientrightvs     = _pat.patientrightvs       ;

    _report_data.patientlefths      = _pat.patientlefths        ;
    _report_data.patientleftvs      = _pat.patientleftvs        ;

    _report_data.judgementRight     = _judgement_desc.R         ;
    _report_data.judgementLeft      = _judgement_desc.L         ;
    _report_data.judgementBoth      = _judgement_desc.Both      ;

    _report_data.isBatch            = _pat.isBatch              ;

}

bool Result::printA4Report(const CPatient &_pat)
{
    // 检查当前打印机是否可用
    if (!g_printIntf->getIsPrinterReady()) {
        getWinManage()->showSuspensionPrompt(tr("当前打印机不可用！\n请先连接打印机")); // "Current printer not available!\nSet up the printer connection first."
        return false;
    }

    //
    QString file_path = Result::savePdfReport(_pat);

    //
    g_printIntf->printFile(file_path);

    getWinManage()->showSuspensionPrompt(tr("打印任务已发送"));    // "Print job has been sent"

    //
    return true;
}

QString Result::savePdfReport(const CPatient &_pat)
{
    //
    //CAlgoInvoker::switchCylSign_StrAx(_pat.patientlefteyesph, _pat.patientlefteyecyl, _pat.patientlefteyeax, isCylNegative(),
    //        _pat.patientlefteyesph, _pat.patientlefteyecyl, _pat.patientlefteyeax);
    //
    //CAlgoInvoker::switchCylSign_StrAx(_pat.patientrighteyesph, _pat.patientrighteyecyl, _pat.patientrighteyeax, isCylNegative(),
    //        _pat.patientrighteyesph, _pat.patientrighteyecyl, _pat.patientrighteyeax);

    //
#if (OS_TYPE!=2)
    //static const QString PDF_DIR = QCoreApplication::applicationDirPath();
    //static const QString PDF_PATH = PDF_DIR + QDir::separator() + "pdf/temp.pdf";
    static const QString PDF_PATH = QString("/tmp/%1/%2").arg("screener-tmp").arg("temp.pdf");
#else
    static const QString PDF_PATH = QString("/dev/shm/%1/%2").arg("screener-tmp").arg("temp.pdf");
#endif

    // 报表数据
    bool has_right, has_left;
    enSingleDualEyeMode single_dual_eye = Result::judgeSingleDualEyeMode(_pat, &has_right, &has_left);
    stVisionJudgementRst right_comp, left_comp;
    Result::getVisionJudgementRst(_pat, has_right, has_left, right_comp, left_comp);
    stVisionJudgementDesc judgement_desc = Result::getVisionJudgementDesc(_pat, has_right, has_left, right_comp, left_comp);

    stReportData report_data;
    Result::patientToReportData(_pat, judgement_desc, report_data);

    report_data.singleDualEyeMode = single_dual_eye;

    double astigmatism_r = 0, myopia_r = 0, hyperopia_r = 0;    // 右眼 散光诊治，近视诊治，远视诊治
    double astigmatism_l = 0, myopia_l, hyperopia_l = 0;        // 左眼 ...
    WinDiagnosticStandard::getDiagnostic(_pat, has_right, astigmatism_r, myopia_r, hyperopia_r, has_left, astigmatism_l, myopia_l, hyperopia_l);

    report_data.astigmatismR = astigmatism_r;
    report_data.astigmatismL = astigmatism_l;
    report_data.myopiaR = myopia_r;
    report_data.myopiaL = myopia_l;
    report_data.hyperopiaR = hyperopia_r;
    report_data.hyperopiaL = hyperopia_l;

    report_data.isAnisometropic = (right_comp.anisometropia /*|| left_comp.anisometropic*/);
    report_data.isGazeR = (right_comp.verticalGaze || right_comp.nasalGaze || right_comp.bitemporalGaze);
    report_data.isGazeL = (left_comp.verticalGaze || left_comp.nasalGaze || left_comp.bitemporalGaze);
    report_data.isPtosisR = _pat.patientrightptosis;
    report_data.isPtosisL = _pat.patientleftptosis;

    report_data.imgPath = UtilApp::getPreviewImgPath(_pat);
    report_data.destPath = /*UtilApp::getPdfFilePath(_pat)*/PDF_PATH;

    // 报表其它配置
    CReport::setIsReducedVersion(CGlobal::isReducedVersion);
    CReport::setOrganizationName(CGlobal::orgNameA4);
    CReport::setOperatorName(CGlobal::operatorName);

    //
    CReport::generatePdfReport(report_data);

    //
    return PDF_PATH;
}

bool Result::isCylNegative()
{
    return Result::s_isCylNegative;
}

void Result::getVisionJudgementRst(const CPatient &_pat, bool _has_right, bool _has_left,
                                   stVisionJudgementRst & _right_comp, stVisionJudgementRst & _left_comp)
{
    eyesightstandard::standardCompare(_pat, (_has_right ? &_right_comp : Q_NULLPTR), (_has_left ? &_left_comp : Q_NULLPTR));
}

// 显示视力判断
void Result::showVisionJudgementDesc(const CPatient &_pat, bool _right, bool _left,
                                     stVisionJudgementRst _right_comp, stVisionJudgementRst _left_comp)
{
    qDebug() << "enter showVisionJudgementDesc, opMode=" << WinMeasure::getOperationMode();

    //
    stVisionJudgementDesc judgement_desc = Result::getVisionJudgementDesc(_pat, _right, _left, _right_comp, _left_comp);

    //
    m_judgementDescStr = judgement_desc.toStr(true);    // 视力判断描述文本，给自动小票打印引用

    //
    QString desc_r, desc_l;
    if (_right) {
        desc_r = tr("右眼:") + judgement_desc.R;  // "Right:"
    }
    if (_left) {
        desc_l = tr("左眼:") + judgement_desc.L;  // "Left:"
    }
    ui->lblJudgementR->setText(desc_r);
    ui->lblJudgementL->setText(desc_l);
    ui->lblJudgementBoth->setText(judgement_desc.Both);

    // 根据不同的值显示不同的样式
    if(themeType_Black == getSysThemeType()){
        if (!_right_comp.isNormal() || _right_comp.dataAbnormal) {
            ui->lblJudgementR->setStyleSheet("QLabel{color:rgb(230,0,0);}");   //字体-浅红
        } else {
            ui->lblJudgementR->setStyleSheet("QLabel{color:rgb(0,150,50);}");   //字体-春绿   //边框颜色 border:1px solid rgb(0,255,0);
        }
        if (!_left_comp.isNormal() || _left_comp.dataAbnormal) {
            ui->lblJudgementL->setStyleSheet("QLabel{color:rgb(230,0,0);}");
        } else {
            ui->lblJudgementL->setStyleSheet("QLabel{color:rgb(0,150,50);}");
        }
        if (judgement_desc.Both.length() > 0l) {
            ui->lblJudgementBoth->setStyleSheet("QLabel{color:rgb(230,0,0);}");
        } else {
            ui->lblJudgementBoth->setStyleSheet("QLabel{color:rgb(0,150,50);}");
        }
    }
    else{
        if (!_right_comp.isNormal() || _right_comp.dataAbnormal) {
            ui->lblJudgementR->setStyleSheet("QLabel{color:rgb(230,0,0);}");
        } else {
            ui->lblJudgementR->setStyleSheet("QLabel{color:rgb(0,200,70);}");
        }
    }

    //
    this->update();
}

// 对传入的检查结果做视力判断，并返回判断描述。静态公开函数，可供筛查历史及批量筛查页面打印时调用
stVisionJudgementDesc Result::getVisionJudgementDesc(const CPatient &_pat, bool _has_right, bool _has_left,
                                                     stVisionJudgementRst _right_comp, stVisionJudgementRst _left_comp)
{
    stVisionJudgementDesc judgement_desc;

    //
    if((!_has_right || _right_comp.isNormal()) && (!_has_left || _left_comp.isNormal()))
    {
        QString str = tr("正常"); // "normal"
        judgement_desc.R = str;
        judgement_desc.L = str;
        judgement_desc.Both = "";
    }
    else
    {
        bool has_str_r = false;
        if(_has_right)
        {
            if(_right_comp.myopia)
            {
                judgement_desc.R.append(tr("近视"));  // "myopia"
                has_str_r = true;
            }
            if(_right_comp.hyperopia)
            {
                judgement_desc.R.append(tr("远视"));  // "hyperopia"
                has_str_r = true;
            }
            if(_right_comp.astigmatism)
            {
                if(has_str_r)
                    judgement_desc.R.append(",");
                judgement_desc.R.append(tr("散光"));  // "astigmatism"
                has_str_r = true;
            }
            if(_right_comp.verticalGaze || _right_comp.nasalGaze || _right_comp.bitemporalGaze)
            {
                if(has_str_r)
                    judgement_desc.R.append(",");
                judgement_desc.R.append(tr("凝视"));  // "gaze"
                has_str_r = true;
            }
            if(_pat.patientrightptosis)
            {
                if(has_str_r)
                    judgement_desc.R.append(",");
                judgement_desc.R.append(tr("上睑下垂"));    // "upper eyelid ptosis"
                has_str_r = true;
            }

            if(!has_str_r)
                judgement_desc.R.append(tr("正常"));  // "normal"
        }

        bool has_str_l = false;
        if(_has_left)
        {
            if(_left_comp.myopia)
            {
                judgement_desc.L.append(tr("近视"));  // "myopia"
                has_str_l = true;
            }
            if(_left_comp.hyperopia)
            {
                judgement_desc.L.append(tr("远视"));  // "hyperopia"
                has_str_l = true;
            }
            if(_left_comp.astigmatism)
            {
                if(has_str_l)
                    judgement_desc.L.append(",");
                judgement_desc.L.append(tr("散光"));  // "astigmatism"
                has_str_l = true;
            }
            if(_left_comp.verticalGaze || _left_comp.nasalGaze || _left_comp.bitemporalGaze)
            {
                if(has_str_l)
                    judgement_desc.L.append(",");
                judgement_desc.L.append(tr("凝视"));  // "gaze"
                has_str_l = true;
            }
            if(_pat.patientleftptosis)
            {
                if(has_str_l)
                    judgement_desc.L.append(",");
                judgement_desc.L.append(tr("上睑下垂"));    // "upper eyelid ptosis"
                has_str_l = true;
            }

            if(!has_str_l)
                judgement_desc.L.append(tr("正常"));  // "normal"
        }

        bool has_str_b = false;
        if(_right_comp.anisometropia || _left_comp.anisometropia) {
            if (has_str_b)
                judgement_desc.Both.append(", ");
            judgement_desc.Both.append(tr("双眼屈光参差"));   // "binocular vision difference"
            has_str_b = true;
        }

        if(_right_comp.unequalInPupilSize || _left_comp.unequalInPupilSize) {
            if (has_str_b)
                judgement_desc.Both.append(", ");
            judgement_desc.Both.append(tr("瞳孔大小不等"));   // "unequal in pupil size"
            has_str_b = true;
        }

        if(_right_comp.gazeAsymmetry || _left_comp.gazeAsymmetry) {
            if (has_str_b)
                judgement_desc.Both.append(", ");
            judgement_desc.Both.append(tr("凝视不对称"));    // "gaze asymmetry"
            has_str_b = true;
        }

        if(_right_comp.dataAbnormal || _left_comp.dataAbnormal) {
            if (has_str_b)
                judgement_desc.Both.append("; ");
            judgement_desc.Both.append(tr("数据可能异常，建议核实"));  // "retest is recommended"
        }
    }
    return judgement_desc;
}

void Result::keyPressEvent(QKeyEvent *event)
{
    //
    if (!this->isVisible()) {
        return;
    }

    //
    if (!keypressState) {
        return;
    }

    //
    if(event->key() == Qt::Key_Escape)              /* Qt::Key_Escape 按键消息已统一在 WindowsManagers::eventFilter() 处理。 */
    {
        //saveAndMeasure(patient.isBatch);
    }
    else if(event->text() != "")
    {
        if(WinMeasure::getOperationMode() != operationMode_InputMeasure){
            //qDebug()<<"opMode ="<<WinMeasure::getOperationMode()<<",return;";
            return;
        }

        //if (CGlobal::isReadBarcodeByQt) {
        //    if(event->key()==Qt::Key_Enter || event->key()==Qt::Key_Return){
        //        qDebug()<<"--get Key_Enter or Key_Return!";
        //        readBarcode.stop();
        //        barcodeHandle();
        //    }
        //    else if(event->text()!=""){
        //        //qDebug()<<event->text();
        //        barcodeData.append(event->text());
        //        if(!barcodeMode){
        //            barcodeMode = true;
        //            qDebug()<<"barcodeMode = true";
        //            readBarcode.start(800);
        //            //showLoading(true);
        //        }
        //    }
        //}
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

//void Result::barcodeHandle()
//{
////    emit sendSIGNAL(sysSignal_10);
//    getWinManage()->showWindowByType(WIN_HOME);

//    qDebug() << "barcode read:" << barcodeData;
//    saveResult();

//    //
//    //showLoading(false);
//    readBarcode.stop();

//    //
//    patient.reset();
//    PersonalInfos::barcodeMode = true;
//    PersonalInfos::barcodeData = barcodeData;

//    getWinManage()->showWindowByType(WIN_PER);
////    qrInfo.getData(barcodeData);
//    WinMeasure::setOperationMode(inputMeasure);

//    //
//    barcodeData.clear();
//    barcodeMode = false;
//}

//void Result::slotKbReaderGetline(string _line_str)
//{
//    //showLoading(true);

//    logDebug(_line_str.data());

//    if (!barcodeMode)
//        barcodeMode = true;

//    barcodeData = _line_str.data();
//    barcodeData.replace("\r", "");
//    barcodeData.replace("\n", "");

//    barcodeHandle();
//}

//void Result::mousePressEvent(QMouseEvent *event)
//{
//}

void Result::batchPrint(std::vector<CPatient> pats)
{
    QString text = tr("正在打印...");   // "Printing..."
    QString buttonText;
    if(pats.size() == 1)
        buttonText = tr("确定");  // "Ok"
    else
        buttonText = tr("取消");  // "Cancel"

    getWinManage()->showMsgWin(text,true,buttonText);

    for (size_t i = 0; i < pats.size(); i++)
    {
//        if(!msg->isVisible())
//            break;
        if(!getWinManage()->isMsgShow())
            break;
        else
            getWinManage()->hideMsgWin();

        qApp->processEvents();

        patient = pats.at(i);
        QString curText = tr("正在打印: "); // "Printing: "
        curText.append(patient.patientid);
        QRegExp valueRegExp(QString("(%1)").arg(patient.patientid));
        curText = curText.replace(valueRegExp, "<font style='font-size:20px;'>\\1</font>");

//        msg->setContent(curText);
        getWinManage()->showMsgWin(curText,true,buttonText);

        printTicket(patient);

        QTime _time = QTime::currentTime().addMSecs(3000);
        while(QTime::currentTime() < _time)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 3000);
        }
    }

    getWinManage()->hideMsgWin();
}

void Result::slot_printTrans_DataSendFinished(bool _is_succ, QString _err_msg)
{
    if (!_is_succ) {
        getWinManage()->showSuspensionPrompt(tr("小票打印失败：") + _err_msg, 5000);    // "Printing receipt failed: "
    }
}

void Strabismus::clear()
{
    rightUpDown = false;
    rightLeftRight = false;
    leftUpDown = false;
    leftLeftRight = false;
    rightUpDownVal = 0;
    rightLeftRightVal = 0;
    leftUpDownVal = 0;
    leftLeftRightVal = 0;
    rightEyeState = false;
    leftEyeState = false;
}

void Strabismus::setDirection(bool _rightUpDown, bool _rightLeftRight, bool _leftUpDown, bool _leftLeftRight)
{
    rightUpDown = _rightUpDown ;
    rightLeftRight = _rightLeftRight ;
    leftUpDown = _leftUpDown ;
    leftLeftRight = _leftLeftRight ;

}

void Strabismus::setValue(double _rightUpDown, double _rightLeftRight, double _leftUpDown, double _leftLeftRight)
{
    rightUpDownVal = _rightUpDown ;
    rightLeftRightVal = _rightLeftRight ;
    leftUpDownVal = _leftUpDown ;
    leftLeftRightVal = _leftLeftRight ;
}

void Strabismus::getDirection(bool &_rightUpDown, bool &_rightLeftRight, bool &_leftUpDown, bool &_leftLeftRight)
{
    _rightUpDown = rightUpDown;
    _rightLeftRight = rightLeftRight;
    _leftUpDown = leftUpDown;
    _leftLeftRight = leftLeftRight;
}

void Strabismus::getValue(double &_rightUpDown, double &_rightLeftRight, double &_leftUpDown, double &_leftLeftRight)
{
    _rightUpDown = rightUpDownVal;
    _rightLeftRight = rightLeftRightVal;
    _leftUpDown = leftUpDownVal;
    _leftLeftRight = leftLeftRightVal;
}

void Strabismus::setEnable(bool state)
{
    enable = state;
}

bool Strabismus::getEnable()
{
    return enable;
}

void Strabismus::setRightEyeState(bool state)
{
    rightEyeState = state;
}

bool Strabismus::getRightEyeState()
{
    return rightEyeState;
}

void Strabismus::setLeftEyeState(bool state)
{
    leftEyeState = state;
}

bool Strabismus::getLeftEyeState()
{
    return leftEyeState;
}

//获取目录
bool Result::GetDirList()
{
    QString img_dir_name = patient.getImgDirName();
    QString ImagePath = QString("/media/photo/%1").arg(img_dir_name);    //查找编号命名的目录
    QDir myDir(ImagePath);
    if(!myDir.exists()){
        ShowMessageWin();
        return false;
    }

    QFileInfo file_info(ImagePath);
    if (file_info.isDir()) {
        ultimateDir = ImagePath;
        ultimateDirCount = 1;
    } else {
        ultimateDir = "";
        ultimateDirCount = 0;
    }

    return true;
}

// 获取目录文件
int Result::GetDirFile()
{
    //获取存图的两张图片(未处理过的)
    QDir dir(ultimateDir);
    QStringList filter;
    QStringList fileList = dir.entryList(filter,QDir::Files | QDir::Readable,QDir::Name);
    if (fileList.size() <= 0) {  //一张图片也没有
        return 0;
    }
    PhotoPath1 = ultimateDir+"/"+fileList[0];
    PhotoPath2 = ultimateDir+"/"+fileList[1];
    qDebug()<<"-----file1:"<<PhotoPath1;
    qDebug()<<"-----file2:"<<PhotoPath2;
    ultimateFileCount = 0;
    if (QFile::exists(PhotoPath1)) {
        ultimateFileCount++;
    }
    if (QFile::exists(PhotoPath2)) {
        ultimateFileCount++;
    }
    return ultimateFileCount;
}

bool Result::updateEditingValues(const CPatient &_pat)
{
    // 检查是否有修改
    bool is_edited = PersonalInfos::checkIsDataChanged(patient, _pat);
    if (is_edited) {
        // 拷贝修改后的值
        PersonalInfos::cloneEditingValues(_pat, patient);

        // 同步到上一级数据源（因为本窗体的“上一条”和“下一条”功能需要从那里读数据）
        // TODO:

    }

    //
    return is_edited;
}

void Result::on_pushButton_preview_clicked()
{
    if (!GetDirList()) {
        return;
    }
    int img_count = GetDirFile();
    if (img_count == 0 || img_count == 1) {     // TODO: 未勾上“保存预览图”，但勾上了“上传图像”，实际有预览图，但提示没有？
        ShowMessageWin();
        return;
    }
    getWinManage()->showWindowByType(WIN_IMAGE);
}

void Result::ShowMessageWin()
{
    QString message = tr("未找到图片。请确认是否已勾选“保存预览图”。");  // "Picture not Found.\nPlease confirm 'Save Preview Image' has been checked."
    QString buttonText = tr("确认");  // "OK"

    MessageWin mess;
    mess.setContent(message);
    mess.setWindowModality(Qt::ApplicationModal);        //阻塞mess以外的所以窗体
    mess.setButtonText(buttonText);
    if(mess.exec() == QDialog::Accepted)
        return;
}

//上一个病例
void Result::on_pushButton_PrevPage_clicked()
{
    QString err_msg;
    CPatient pat;
    bool is_found = getPrevResult(patient.id, pat, err_msg);
    if (is_found) {
        //MySQLitePatients::getInstance()->getPatientById(pat);           // 每次都从数据库重新载入记录，避免数据被编辑后不同步

        patient.cloneFrom(pat);
        qDebug()<<"-----patient.id:"<<patient.id;
        refreshTitle(patient);      //这里主要是改变编号用到
        loadDataToUi(patient);      //更新部分控件参数(有控件被改变了会调用paintEvent事件)
        show_theme_state();
    } else {
        getWinManage()->showSuspensionPrompt(err_msg);
    }
}

//下一个病例
void Result::on_pushButton_NextPage_clicked()
{
    QString err_msg;
    CPatient pat;
    bool is_found = getNextResult(patient.id, pat, err_msg);
    if (is_found) {
        //MySQLitePatients::getInstance()->getPatientById(pat);           // 每次都从数据库重新载入记录，避免数据被编辑后不同步

        patient.cloneFrom(pat);
        qDebug()<<"-----patient.id:"<<patient.id;
        refreshTitle(patient);
        loadDataToUi(patient);
        show_theme_state();
    } else {
        getWinManage()->showSuspensionPrompt(err_msg);
    }
}

void Result::slotResultAbnormal()
{
    if (!this->isVisible()) {
        qDebug() << "Result::slotResultAbnormal(): this not visible, waiting ...";
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 1500) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 300);
            if (this->isVisible()) {
                break;
            }
        }
    }
    if (!this->isVisible()) {
        qCritical() << "Result::slotResultAbnormal(): this not visible!";
        return;
    }

    bool ret = getWinManage()->showNoticeWin(tr("本次测量结果偏离正常值较大，建议重新测量。"),    // "This measurement result deviates significantly from the normal value. It is recommended to re measure."
                                             tr("忽略提示"), tr("重新测量"),    // "Ignore prompt", "Re measure"
                                             0, true, true);
    if (ret)
    {//结果异常，但是正常保存数据
        //on_pushButton_Save_clicked();
    }
    else
    {//结果异常重新测量
        on_pushButton_chongxin_clicked();
        qDebug()<<"on_pushButton_chongxin_clicked" << ret;
    }
    qDebug()<<"SlotResultAbnormalHandle" << ret;
}

void Result::slotPhysicButtonPressed()
{
    // 若本窗体不是当前窗体，则不处理此信号
    if (this != getWinManage()->getCurrentWin()) {
        return;
    }

    // 门诊记录和批量记录界面按键不起作用
    if (WinMeasure::getOperationMode() == operationMode_HistoryRecord || WinMeasure::getOperationMode() == operationMode_BatchRecord) {   // TODO: 检查梳理？
        return;
    }

    // 保存结果
    QString err_msg;
    bool succ_save = saveResult(patient, err_msg);
    if (!succ_save) {
        getWinManage()->showSuspensionPrompt(err_msg, -1);
    }

    // 若是批量模式，测量下一位未测量的
    qDebug() << "opMode = " << WinMeasure::getOperationMode();
    if (isBatchScreen()) {
        QString err_msg;
        CPatient pat;
        bool is_found = batchscr->getNextNotMeasured(patient.id, pat, err_msg);
        if (is_found) {
            getWinManage()->openMeasureWin(pat, patientSource_Database);
        } else {
            getWinManage()->showSuspensionPrompt(err_msg);
        }
    } else {                                                        // 否则，继续门诊模式的下一次测量
        // 继续下一次测量
        beforeBack();
        g_WinMeasure->continueMeasuring();
    }
}

void Result::slotUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg)
{
    //
    qDebug() << __PRETTY_FUNCTION__ << ": entered ...";

    //
    if (!this->isVisible()) {
        return;
    }

    // 隐藏等待提示框
    getWinManage()->hideMsgWin(m_msgIdOfWaitingBtUpload);
    m_msgIdOfWaitingBtUpload = -1;

    // 获取提示消息
    QString msg_tip;
    bool is_succ = WinClinic::getUploadFeedbackMsg(_count_upload, _count_upload_final, _count_succ, _msg, msg_tip);
    if (is_succ) {
        msg_tip = tr("测量结果上传成功！");      // "Measurement result upload succeeded!"
    } else {
        msg_tip = tr("测量结果上传失败！");      // "Measurement result upload failed!"
    }
    getWinManage()->showSuspensionPrompt(msg_tip);

}

void Result::on_btnSwichCylSign_clicked()
{
    //
    if (isCylNegative()) {
        s_isCylNegative = false;
        ui->btnSwichCylSign->setIcon(QIcon(":/resource/black_theme/cyl_positive.png"));
        ui->label_SwichCylSign->setText(tr("正散光")); // "+AST"
    } else {
        s_isCylNegative = true;
        ui->btnSwichCylSign->setIcon(QIcon(":/resource/black_theme/cyl_negative.png"));
        ui->label_SwichCylSign->setText(tr("负散光")); // "-AST"
    }

    //
    loadDataToUi(patient);

    //
    this->update();
}

void Result::switchCylSignAndPrintTicket(CPatient _pat, QString _judgement_desc)
{
    //CAlgoInvoker::switchCylSign_StrAx(_pat.patientlefteyesph, _pat.patientlefteyecyl, _pat.patientlefteyeax, isCylNegative(),
    //        _pat.patientlefteyesph, _pat.patientlefteyecyl, _pat.patientlefteyeax);
    //
    //CAlgoInvoker::switchCylSign_StrAx(_pat.patientrighteyesph, _pat.patientrighteyecyl, _pat.patientrighteyeax, isCylNegative(),
    //        _pat.patientrighteyesph, _pat.patientrighteyecyl, _pat.patientrighteyeax);
    // NOTE: 小票打印模块已做散光符号处理，此处不能重复做，且此处做得不完全，还有多次测量的结果未处理

    emit printSig(_pat, _judgement_desc);
}

void Result::on_btnShowMultiResults_toggled(bool _checked)
{
    ui->wgtMultiResults->setVisible(_checked);
}
