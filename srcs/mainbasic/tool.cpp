//工具菜单
#include "tool.h"
#include "ui_tool.h"

#include <QPushButton>
#include <QIcon>
#include <QPixmap>
#include <datepage.h>
#include <QDir>
#include <QMessageBox>

#include "eyesightstandard.h"
#include "windatatrans.h"
#include "printersetting.h"
#include "windatatrans.h"
#include "algointf.h"
#include "musicsetting.h"
#include "appsetting.h"
#include "global.h"
#include "windowsmanager.h"
#include "dialoglanguage.h"

using namespace DataTrans;

//
bool g_isHmMode = false;            // 是否高度数模式
bool g_MinResolution = false;       // 是否小分辨率

//bool g_isSavePreviewImage = false;      // 是否保存预览图（经过直方图均衡化处理，用于 A4 报表修饰）
bool g_isSaveSampleImage = false;       // 是否保存转灯图抽样（目前为 12、18 帧，未经处理的原图）

//
Tool::Tool(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::Tool)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_SYS);

    ui->setupUi(this);
    //this->setStyleSheet("background-color:rgb(1,1,1);");//2020.4.24
    //this->setStyleSheet("color: rgb(204, 204, 204);");//2020.4.26
    //QFont font("Courier",10,QFont::Normal,false);
    //QApplication::setFont(font);

    isShowStatusBar = true;

    countMouseClicked = 0;
    countMouseClicked2 = 0;
    cnt = 0;
    SlidePage = 1;
    CurrentPage = 1;

    //iniSetting = new QSettings("manylinks",QSettings::IniFormat);

    CAlgoInvoker::setCurve(appSetting::value("/tool/curve").toInt());
    if((CAlgoInvoker::getCurve()) < 1 || (CAlgoInvoker::getCurve()) > 3)
    {
        CAlgoInvoker::setCurve(2);
    }
    appSetting::setValue("/tool/curve", CAlgoInvoker::getCurve());
    m_leftAnimation = new QPropertyAnimation(ui->widget, "pos");
    m_rightAnimation  = new QPropertyAnimation(ui->widget_2, "pos");

    //先设置第二页位置,不然两页重叠会有重影
    //m_rightAnimation->setDuration(0);
    //m_rightAnimation->setStartValue(QPoint(0,0));
    //m_rightAnimation->setEndValue(QPoint(800,0));
    //m_rightAnimation->start();                        // TODO: 这个在构造方法里无效？（RK3568 平台）

    connect(&clickTimer, SIGNAL(timeout()), this, SLOT(resetMouseClicked())); //add for engineerModel by sun 20180826

    InquireLocation = new QTimer;
    connect(InquireLocation, SIGNAL(timeout()), this, SLOT(InquireWidgetLocation()));
    InquireLocation->stop();

    initConfigs();

    // wim
    //ui->groupBox_page1->setStyleSheet("QGroupBox{border:none}");
    //ui->groupBox_page2->setStyleSheet("QGroupBox{border:none}");
    //ui->groupBox_page2->setVisible(false);
    ui->pushButtonPrevPage->setVisible(false);

    //setting = new SettingsDialog(this);
    //connect(setting, SIGNAL(setNewServer(QString)), this, SIGNAL(setNewServer(QString)));
    //getWinManage()->addWidget(setting);

    // 修正窗体设计期间可能存在的布局问题
    ui->widget->setParent(this);
    ui->widget->move(0, 0);
    ui->widget->setAutoFillBackground(false);

    ui->widget_2->setParent(this);
    ui->widget_2->move(0, 0);
    ui->widget_2->setAutoFillBackground(false);

    ui->widget->lower();
    ui->widget_2->lower();

    //
    ui->widget_2->move(800, 0);

    //
    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_SYS);
}

Tool::~Tool()
{
    delete ui;
}

void Tool::showEvent(QShowEvent *)
{
    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() 1 = " << g_elapsedTimer.elapsed();

    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    //
    static bool is_first = true;

    // 调整按钮位置及显示状态
    if (is_first) {
        initButtonsPosition();
        setFixedButtonsPosition();
    }
    setDynamicButtonsPosition();

    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() 2 = " << g_elapsedTimer.elapsed();

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() 3 = " << g_elapsedTimer.elapsed();

    // 更新文本和图标
    refreshText();
    refreshIcon();

    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() 4 = " << g_elapsedTimer.elapsed();

    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("工具"));  // "Tool"

    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() 5 = " << g_elapsedTimer.elapsed();

    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() 6 = " << g_elapsedTimer.elapsed();

    //
    clickTimer.start(2000);
    qDebug() << "start clickTimer";

    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() 7 = " << g_elapsedTimer.elapsed();

    //
    qDebug() << "paint tool";

    //QPalette palette;
    if (themeType_Black == getSysThemeType()) {     //黑色主题
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));

        ui->btnDataTrans->setIcon(QIcon(":/resource/black_theme/data_transmission_b.png"));
        ui->btnWiFi->setIcon(QIcon(":/resource/black_theme/wifi_b.png"));
        ui->btnDateTime->setIcon(QIcon(":/resource/black_theme/date_b.png"));
        ui->btnPrintSetting->setIcon(QIcon(":/resource/black_theme/print_setup_b.png"));
        ui->btnStandard->setIcon(QIcon(":/resource/black_theme/eyesight_standard_b.png"));
        ui->btnMusic->setIcon(QIcon(":/resource/black_theme/music_b.png"));
        ui->btnAbout->setIcon(QIcon(":/resource/black_theme/aubot_b.png"));
        ui->btnUpdate->setIcon(QIcon(":/resource/black_theme/check_update_b.png"));
        ui->btnSettings->setIcon(QIcon(":/resource/black_theme/settings_b.png"));
        ui->btnTheme->setIcon(QIcon(":/resource/black_theme/theme_b.png"));
        ui->btnBluetooth->setIcon(QIcon(":/resource/black_theme/bluetooth_b.png"));
        ui->pushButton_back->setIcon(QIcon(":/resource/black_theme/back_b.png"));

        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p,list_Label)
        {
            QString  qstr = p->objectName();
            //qDebug()<<"------strstr1------"<<p->objectName();
            if (qstr.contains("label_", Qt::CaseSensitive) || qstr.startsWith("lbl", Qt::CaseSensitive))   //成功返回true 第二个参数表示是否大小写敏感
                p->setStyleSheet("color:rgb(204,204,204);");
        }

        qDebug() << "paint black background!";
    }
    else  //白色主题
    {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        ui->btnDataTrans->setIcon(QIcon(":/resource/white_theme/data_transmission_w.png"));
        ui->btnWiFi->setIcon(QIcon(":/resource/white_theme/wifi_w.png"));
        ui->btnDateTime->setIcon(QIcon(":/resource/white_theme/date_w.png"));
        ui->btnPrintSetting->setIcon(QIcon(":/resource/white_theme/print_setup_w.png"));
        ui->btnStandard->setIcon(QIcon(":/resource/white_theme/eyesight_standard_w.png"));
        ui->btnMusic->setIcon(QIcon(":/resource/white_theme/music_w.png"));
        ui->btnAbout->setIcon(QIcon(":/resource/white_theme/aubot_w.png"));
        ui->btnUpdate->setIcon(QIcon(":/resource/white_theme/check_update_w.png"));
        ui->btnSettings->setIcon(QIcon(":/resource/white_theme/settings_w.png"));
        ui->btnTheme->setIcon(QIcon(":/resource/white_theme/theme_w.png"));
        ui->btnBluetooth->setIcon(QIcon(":/resource/white_theme/bluetooth_w.png"));
        ui->pushButton_back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p,list_Label)
        {
            QString  qstr = p->objectName();
            //qDebug()<<"------strstr2------"<<p->objectName();
            if (qstr.contains("label_", Qt::CaseSensitive) || qstr.startsWith("lbl", Qt::CaseSensitive))   //成功返回true 第二个参数表示是否大小写敏感
                p->setStyleSheet("color:rgb(1,1,1);");
        }
        qDebug() << "paint white background!";
    }
    //this->setPalette(palette);

    //
    qDebug() << __PRETTY_FUNCTION__ << ": g_elapsedTimer.elapsed() 8 = " << g_elapsedTimer.elapsed();

    //
    is_first = false;
}

void Tool::hideEvent(QHideEvent *)
{
    qDebug() << "WinTool hiding";

    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    //
    cnt = 0;
    clickTimer.stop();
}

void Tool::initButtonsPosition()
{
    // 初始位置（与 Form 设计一致）
    positionButton(ui->btnSingleDualEye     , 0, 0, 0);
    positionButton(ui->btnHmMode            , 0, 0, 1);
    positionButton(ui->btnMusic             , 0, 0, 2);
    positionButton(ui->btnLanguage          , 0, 0, 3);

    positionButton(ui->btnDataTrans         , 0, 1, 0);
    positionButton(ui->btnWiFi              , 0, 1, 1);
    positionButton(ui->btnDateTime          , 0, 1, 2);
    positionButton(ui->btnPrintSetting      , 0, 1, 3);

    positionButton(ui->btnSettings          , 0, 2, 0);
    positionButton(ui->btnUpdate            , 0, 2, 1);
    positionButton(ui->btnBluetooth         , 0, 2, 2);

    positionButton(ui->btnStorageStat       , 1, 0, 0);
    positionButton(ui->btnResolution        , 1, 0, 1);
    positionButton(ui->btnStandard          , 1, 0, 2);
    positionButton(ui->btnTheme             , 1, 0, 3);

    positionButton(ui->btnAbout             , 1, 1, 0);
    positionButton(ui->btnDistCalibrate     , 1, 1, 1);
}

void Tool::setFixedButtonsPosition()
{
    // 隐藏“距离校准”按钮
    positionButton(ui->btnDistCalibrate, 0, 0, 0, false);

    // 隐藏“主题”按钮，且将“关于”按钮移到原“主题”按钮位置                 // TODO: 目前只支持一种 theme，待完善
    positionButton(ui->btnTheme, 0, 0, 0, false);
    positionButton(ui->btnAbout, 1, 0, 3);

    // 显示蓝牙图标
#if (OS_TYPE == 1)
    if (!g_hasBluetooth) {
        positionButton(ui->btnBluetooth, 0, 0, 0, false);
    }
#endif

}

void Tool::setDynamicButtonsPosition()
{
    // 若是试用版，隐藏“日期”按钮，“打印设置”按钮左移
    bool is_trial = (CAuthIntf::authType_Trial == CGlobal::authType);
    positionButton(ui->btnDateTime, 0, 1, 2, !is_trial);

    if (is_trial) {
        positionButton(ui->btnPrintSetting, 0, 1, 2);
    } else {
        positionButton(ui->btnPrintSetting, 0, 1, 3);
    }

    // 根据型号设置音乐是否可用
    bool is_music_enabled = CGlobal::getIsMusicEnabled();
    positionButton(ui->btnMusic, 0, 0, 2, is_music_enabled);

    if (!is_music_enabled) {        // 若音乐图标不可见，则将下一页的“存储”前移
        positionButton(ui->btnStorageStat   , 0, 0, 2);

        positionButton(ui->btnResolution    , 1, 0, 0);
        positionButton(ui->btnStandard      , 1, 0, 1);
        positionButton(ui->btnAbout         , 1, 0, 2);
    } else {
        positionButton(ui->btnStorageStat   , 1, 0, 0);

        positionButton(ui->btnResolution    , 1, 0, 1);
        positionButton(ui->btnStandard      , 1, 0, 2);
        positionButton(ui->btnAbout         , 1, 0, 3);
    }

}

void Tool::positionButton(const QString &_name_key, int _page, int _row, int _col, bool _is_visible)
{
    // TODO: 自动设置 btn 和 lbl 的尺寸？

    //
    QPushButton *btn = nullptr;
    QLabel *lbl = nullptr;

    QList<QWidget*> wgt_list = this->findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
    foreach (QWidget *wgt, wgt_list) {
        if (wgt->objectName() == "btn" + _name_key) {
            btn = dynamic_cast<QPushButton*>(wgt);
        } else if (wgt->objectName() == "lbl" + _name_key) {
            lbl = dynamic_cast<QLabel*>(wgt);
        }
    }

    if (btn && lbl) {
        if (_is_visible) {
            //
            QWidget *parent_new = nullptr;
            if (0 == _page) {
                parent_new = ui->widget;
            } else if (1 == _page) {
                parent_new = ui->widget_2;
            } else {
                logCritical(QString("%1->%2(): page index \"%3\" not valid!").arg(__FILE__).arg(__FUNCTION__).arg(_page));
                return;
            }

            //
            bool curr_visible = btn->isVisible();

            //
            QWidget *parent_old = btn->parentWidget();
            if (parent_old != parent_new) {
                btn->setParent(parent_new);
                lbl->setParent(parent_new);
                curr_visible = false;           // NOTE: 切换 parent 后，部件会被隐藏
            }

            //
            if (!curr_visible) {
                btn->show();
                lbl->show();
            }

            //
            const int LEFT = 30;                                        // 左侧的空隙
            const int TOP = WgtStatusBar::instance()->height();      // 顶部的空隙
            const int PADDING_BOTTOM = 30;                              // 底部的空隙

            const QSize SIZE_WGT = QSize(ui->widget->width() - LEFT * 2, ui->widget->height() - TOP - PADDING_BOTTOM);  // 容器尺寸
            const QSize SIZE_CELL = QSize(SIZE_WGT.width() / 4, SIZE_WGT.height() / 3);                                 // 单元格尺寸

            const QSize SIZE_BTN = ui->btnAbout->size();                // 按钮尺寸
            const QSize SIZE_LBL = ui->lblAbout->size();                // 标签尺寸

            const int SPACE_BTN_LBL = 11;       // 按钮和标签的空隙

            int btn_x = LEFT + SIZE_CELL.width() * _col + (SIZE_CELL.width() - SIZE_BTN.width()) / 2;
            int btn_y = TOP + SIZE_CELL.height() * _row + (SIZE_CELL.height() - SIZE_BTN.height() - SPACE_BTN_LBL - SIZE_LBL.height()) / 2;
            btn->move(btn_x, btn_y);

            int lbl_x = LEFT + SIZE_CELL.width() * _col + (SIZE_CELL.width() - SIZE_LBL.width()) / 2;
            int lbl_y = btn_y + SIZE_BTN.height() + SPACE_BTN_LBL;
            lbl->move(lbl_x, lbl_y);
        } else {
            btn->hide();
            lbl->hide();
        }
    } else {
        logCritical(QString("%1->%2(): Widget name \"%3\" not found!").arg(__FILE__).arg(__FUNCTION__).arg(_name_key));
        return;
    }
}

void Tool::positionButton(const QWidget *_wgt, int _page, int _row, int _col, bool _is_visible)
{
    positionButton(_wgt->objectName().mid(3), _page, _row, _col, _is_visible);
}

//***********************************************************add for engineerModel by sun 20180826
//if countMouseClicked >= 3 ,engineerModel will show
//if more than 3 seconds no mousePress happens ,countMouseClicked would be reseted
void  Tool::mousePressEvent(QMouseEvent *event)
{
    m_Oncepress = true;
    m_startPos = event->pos();
    press_x = event->globalX();

    debugMouseClick(event->pos().x(), event->pos().y());

}

//
void Tool::debugMouseClick(int _x, int _y)
{
    //qDebug() << "Tool::debugClick() into, 1 = " << countMouseClicked << ", 2 = " << countMouseClicked2;

    cnt = 0;
    if (_y <= 80) {           //点击上方区域调出工程界面
        if (_x >= 700)
            countMouseClicked2++;
        else
            countMouseClicked++;
        qDebug() << "clicking valid : 1 = " << countMouseClicked << ", 2 = " << countMouseClicked2;
    }

    if (countMouseClicked >= 3)
    {
        //
        countMouseClicked = 0;

        //
#if OS_TYPE != 2
        bool enginPwdOffFlag = CEngineerMode::isPasswordEnabled();
#else
        //bool enginPwdOffFlag = CEngineerMode::isPasswordEnabled();
        bool enginPwdOffFlag = false;
#endif
        qint64 elapsed_checked = engineerpassword::elapsedChecked();
        bool is_checked = (elapsed_checked > 0 && elapsed_checked < 1 * 60 * 60 * 1000);
        if (enginPwdOffFlag && !is_checked) {
            qDebug() << "show engineerpassword";
            engineerpassword *engineerPwdWin = new engineerpassword(this);
            engineerPwdWin->show();
        }
        else{
            qDebug() << "show engineerModel";
            getWinManage()->showWindowByType(WIN_ENGIN);
        }
    }

    if (countMouseClicked2 >= 3) {
        countMouseClicked2 = 0;
        positionButton(ui->btnDistCalibrate, 1, 1, 1, !ui->btnDistCalibrate->isVisible());
    }
}

void Tool::mouseMoveEvent(QMouseEvent *event)
{
    QPoint point = event->pos() - m_startPos;
    Page1_LR_Pos = QPoint(point.x(),0);
    Page2_L_Pos = QPoint(800+point.x(),0);
    Page2_R_Pos = QPoint(point.x()-800,0);

    if (/*m_Oncepress &&*/ abs(point.x()) >= 1)
    {
        m_Oncepress = false;
        if (point.x() < 0)  //
            m_bRight = false;
        else if(point.x() > 0)
            m_bRight = true;
    }
    if (m_bRight)   //向右移动
    {
        if (point.x() < 0)  //向右移动过程中突然又向左移动
            m_bRight = false;
        if(SlidePage == 2)   //当前为第2页
            QPropertyAnimationMoveRight();
    }

    if (!m_bRight)  //向左移动
    {
        if (point.x() > 0)  //向左移动过程中突然又向右移动
            m_bRight = true;
        if(SlidePage == 1)   //当前为第1页
            QPropertyAnimationMoveLeft();
    }
}

void Tool::mouseReleaseEvent(QMouseEvent *event)
{
    //if (press_x == relea_x) {
    //    return;
    //}

    //
    int duration = 300;     //持续时间
    relea_x = event->globalX();
    if(!m_bRight && SlidePage==1 && (press_x - relea_x)>50)         //判断滑动方向（左滑）
    {
        ui->pushButtonNextPage->setVisible(false);
        ui->pushButtonPrevPage->setVisible(true);
        ReleaseMoveLeftSlide(duration);
    }
    else if(m_bRight && SlidePage==2 && (relea_x - press_x)>50)     //判断滑动方向（右滑）
    {
        ui->pushButtonNextPage->setVisible(true);
        ui->pushButtonPrevPage->setVisible(false);
        ReleaseMoveRightSlide(duration);
    }
    else if(!m_bRight && SlidePage!=2 && (press_x - relea_x)<=50 && press_x != relea_x)   //左滑,x轴滑动距离小于等于50像素
        ReleaseMoveLeftSlideReset(duration);
    else if(m_bRight && SlidePage!=1 &&  (relea_x - press_x)<=50 && press_x != relea_x)   //右滑,x轴滑动距离小于等于50像素
        ReleaseMoveRightSlideReset(duration);
    else
        InquireLocation->start(1000);
}

void Tool::resetMouseClicked()
{
    cnt++;
    if(cnt >= 1)
    {
        countMouseClicked = 0;
        countMouseClicked2 = 0;
    }
    //qDebug() << "reset mouseclick";
}

//**************************************************************add end
void Tool::on_btnWiFi_clicked()
{
    //emit sendSIGNAL(sysSignal_12);
    getWinManage()->showWindowByType(WIN_WIFI);
}


void Tool::on_btnStandard_clicked()
{
    //eyesightstandard eyesightstandard1;
    //eyesightstandard1.setModal(true);
    //eyesightstandard1.exec();
    getWinManage()->showWindowByType(WIN_EYESIGHT);
}

void Tool::on_btnSingleDualEye_clicked()
{
    // 单双眼模式切换
    g_SingleDualEye = (enSingleDualEyeMode)((int)g_SingleDualEye + 1);
    if (g_SingleDualEye > singleDualEyeMode_Max) {
        g_SingleDualEye = singleDualEyeMode_Min;
    };

    // 按钮更新
    if (singleDualEyeMode_Both == g_SingleDualEye) {
        if(themeType_Black == getSysThemeType())
            ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/single-dual-eye_both_b.png"));
        else
            ui->btnSingleDualEye->setIcon(QIcon(":/resource/white_theme/single-dual-eye_both_w.png"));

        this->ui->lblSingleDualEye->setText(tr("双眼模式"));  // "Binocular pattern"
    } else if (singleDualEyeMode_Left == g_SingleDualEye) {
        if(themeType_Black == getSysThemeType())
            this->ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/single-dual-eye_left_b.png"));
        else
            this->ui->btnSingleDualEye->setIcon(QIcon(":/resource/white_theme/single-dual-eye_left_w.png"));

        this->ui->lblSingleDualEye->setText(tr("左眼模式"));  // "Left"
    } else if (singleDualEyeMode_Right == g_SingleDualEye) {
        if(themeType_Black == getSysThemeType())
            this->ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/single-dual-eye_right_b.png"));
        else
            this->ui->btnSingleDualEye->setIcon(QIcon(":/resource/white_theme/single-dual-eye_right_w.png"));

        this->ui->lblSingleDualEye->setText(tr("右眼模式"));  // "Right"
    }
}

void Tool::on_btnResolution_clicked()
{
    if (g_MinResolution) {
        g_MinResolution = false;
        if (themeType_Black == getSysThemeType())
            ui->btnResolution->setIcon(QIcon(":/resource/black_theme/resolution0.25_b.png"));
        else
            ui->btnResolution->setIcon(QIcon(":/resource/white_theme/resolution0.25_w.png"));

        //this->ui->lblResolution->setText(tr("分辨率0.25D"));   // "Resolution 0.25D"
    } else {
        g_MinResolution = true;
        if (themeType_Black == getSysThemeType())
            ui->btnResolution->setIcon(QIcon(":/resource/black_theme/resolution0.01_b.png"));
        else
            ui->btnResolution->setIcon(QIcon(":/resource/white_theme/resolution0.01_w.png"));

        //this->ui->lblResolution->setText(tr("分辨率0.01D"));   // "Resolution 0.01D"
    }
}

void Tool::on_btnLanguage_clicked()
{
    // 弹出语言选择窗口
    DialogLanguage *dialog = new DialogLanguage();
    dialog->setLanguage(CGlobal::language);
    CBaseWindow *win_base = getWinBase();
    //dialog->setParent(win_base);
    CBaseFormIntf::centerWidget(dialog, win_base);
    int ret = dialog->exec();
    if (QDialog::Rejected == ret) {
        return;
    }

    // “语言”全局变量的切换
    CGlobal::language = dialog->getLanguage();

    // 保存新的语言值
    appSetting::setValue("global/language", CGlobal::language);
    appSetting::sync();

    //// 翻译器设置
    //CWinManage::setTranslator(CGlobal::language);
    //
    //// 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    //ui->retranslateUi(this);
    //
    //// 重新加载窗体的文本和图标
    //refreshText();
    //refreshIcon();
    //
    //// 标题刷新
    //getWinManage()->updateWindowTitle(this, tr("工具"));  // "Tool"

    // 自动重启
    globalService()->reboot();

}
void Tool::on_pushButton_back_clicked()
{
    getWinManage()->showWindowByType(WIN_HOME);
}

void Tool::on_btnPrintSetting_clicked()
{
    //printerSetting *pSetting = new printerSetting(this);
    //pSetting->show();
    getWinManage()->showWindowByType(WIN_PRINT);
}

void Tool::on_btnDateTime_clicked()
{
    //datePage datedemo;
    getWinManage()->showWindowByType(WIN_DATE);
    //datedemo.setModal(true);
    //datedemo.exec();
}

void Tool::refreshText()
{
    //
    //if (language) {
    //    ui->label_Back->setText("返回");
    //    ui->lblStandard->setText("视力标准");
    //    ui->lblLanguage->setText("语言");
    //    ui->lblDataTrans->setText("数据传输");
    //    ui->lblDateTime->setText("日期");
    //    ui->lblPrintSetting->setText("打印设置");
    //    ui->lblAbout->setText("关于");
    //    ui->lblMusic->setText("音乐");
    //    ui->lblUpdate->setText("软件更新");
    //    ui->lblSettings->setText("设置");
    //    ui->lblTheme->setText("主题");
    //    ui->lblBluetooth->setText("蓝牙");
    //    ui->lblHmMode->setText("高度数/正常模式");
    //    ui->lblStorageStat->setText("存储");
    //    ui->lblResolution->setText("分辨率");
    //    ui->lblDistCalibrate->setText("距离校准");
    //} else {
    //    ui->label_Back->setText("Back");
    //    ui->lblStandard->setText("Eyesight Criteria");
    //    ui->lblLanguage->setText("Language");
    //    ui->lblDataTrans->setText("Data Transmission");
    //    ui->lblDateTime->setText("Date");
    //    ui->lblPrintSetting->setText("Print Setup");
    //    ui->lblAbout->setText("About");
    //    ui->lblMusic->setText("Music");
    //    ui->lblUpdate->setText("Software Update");
    //    ui->lblSettings->setText("Settings");
    //    ui->lblTheme->setText("Theme");
    //    ui->lblBluetooth->setText("Bluetooth");
    //    ui->lblHmMode->setText("High Degree/Normal");
    //    ui->lblStorageStat->setText("Storage");
    //    ui->lblResolution->setText("Resolution");
    //    ui->lblDistCalibrate->setText("Distance Calibration");
    //}

    //
    if (singleDualEyeMode_Both == g_SingleDualEye) {
        ui->lblSingleDualEye->setText(tr("双眼模式"));    // "Binocular Pattern"
    } else if (singleDualEyeMode_Left == g_SingleDualEye) {
        ui->lblSingleDualEye->setText(tr("左眼模式"));    // "Left Eye Mode"
    } else if (singleDualEyeMode_Right == g_SingleDualEye) {
        ui->lblSingleDualEye->setText(tr("右眼模式"));    // "Right Eye Mode"
    }

}

void Tool::refreshIcon()
{
    //
    if (g_MinResolution) {      // 分辨率
        if (themeType_Black == getSysThemeType())
            ui->btnResolution->setIcon(QIcon(":/resource/black_theme/resolution0.01_b.png"));
        else
            ui->btnResolution->setIcon(QIcon(":/resource/white_theme/resolution0.01_w.png"));
    } else {
        if (themeType_Black == getSysThemeType())
            ui->btnResolution->setIcon(QIcon(":/resource/black_theme/resolution0.25_b.png"));
        else
            ui->btnResolution->setIcon(QIcon(":/resource/white_theme/resolution0.25_w.png"));
    }

    if (singleDualEyeMode_Both == g_SingleDualEye) {    // 左右眼模式
        if (themeType_Black == getSysThemeType())
            ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/single-dual-eye_both_b.png"));
        else
            ui->btnSingleDualEye->setIcon(QIcon(":/resource/white_theme/single-dual-eye_both_w.png"));
    } else if (singleDualEyeMode_Left == g_SingleDualEye) {
        if (themeType_Black == getSysThemeType())
            ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/single-dual-eye_left_b.png"));
        else
            ui->btnSingleDualEye->setIcon(QIcon(":/resource/white_theme/single-dual-eye_left_w.png"));
    } else if (singleDualEyeMode_Right == g_SingleDualEye) {
        if (themeType_Black == getSysThemeType())
            ui->btnSingleDualEye->setIcon(QIcon(":/resource/black_theme/single-dual-eye_right_b.png"));
        else
            ui->btnSingleDualEye->setIcon(QIcon(":/resource/white_theme/single-dual-eye_right_w.png"));
    }

    //
    if (themeType_Black == getSysThemeType())   // 语言
        ui->btnLanguage->setIcon(QIcon(QString(":/resource/black_theme/language_b_%1.png").arg(CGlobal::language)));
    else
        ui->btnLanguage->setIcon(QIcon(":/resource/white_theme/chinese_w.png"));

    if (g_isHmMode) {       // 高度数
        if (themeType_Black == getSysThemeType())
            ui->btnHmMode->setIcon(QIcon(QString(":/resource/black_theme/degree_high_b_%1.png").arg(CGlobal::language)));
        else
            ui->btnHmMode->setIcon(QIcon(":/resource/white_theme/c_high_degree_w.png"));
    } else {                // 正常
        if (themeType_Black == getSysThemeType())
            ui->btnHmMode->setIcon(QIcon(QString(":/resource/black_theme/degree_normal_b_%1.png").arg(CGlobal::language)));
        else
            ui->btnHmMode->setIcon(QIcon(":/resource/white_theme/c_normal_w.png"));
    }

    if (themeType_Black == getSysThemeType()) {     // 存储状态
        ui->btnStorageStat->setIcon(QIcon(":/resource/black_theme/storage_b.png"));
    } else {
        ui->btnStorageStat->setIcon(QIcon(":/resource/black_theme/storage_w.png"));
    }

}

void Tool::on_btnAbout_clicked()
{
//    qDebug() << "send signal 24*********";
//    emit sendSIGNAL(sysSignal_24);
    //deviceInfo devInfo;
    //devInfo.setModal(true);
    //devInfo.exec();
    qDebug() << "aboutdevice";
    getWinManage()->showWindowByType(WIN_ABOUT);
}

void Tool::on_btnHmMode_clicked()
{
    //
    g_isHmMode = !g_isHmMode;

    //
    this->ui->lblHmMode->setText(tr("高度数/正常模式"));   // "High degree/Normal"

    //
    if (g_isHmMode) { //高度数
        if (themeType_Black == getSysThemeType())
            ui->btnHmMode->setIcon(QIcon(QString(":/resource/black_theme/degree_high_b_%1.png").arg(CGlobal::language)));
        else
            ui->btnHmMode->setIcon(QIcon(":/resource/white_theme/c_high_degree_w.png"));
    } else {       //正常
        if (themeType_Black == getSysThemeType())
            ui->btnHmMode->setIcon(QIcon(QString(":/resource/black_theme/degree_normal_b_%1.png").arg(CGlobal::language)));
        else
            ui->btnHmMode->setIcon(QIcon(":/resource/white_theme/c_normal_w.png"));
    }
}

void Tool::on_btnDataTrans_clicked()
{
    qDebug() << __FUNCTION__ << "(): entered ...";
    getWinManage()->showWindowByType(WIN_DATA);
}

void Tool::initConfigs()
{
    // 检查并初始化各个配置变量
//    QString displaymode = appSetting::value("/tool/displaymode").toString();
//    if(displaymode!="Diopter" || displaymode!="Vision"){
//        appSetting::setValue("/tool/displaymode","Diopter");
//        qDebug()<<"init displaymode++++++";
//    }

    int volume = appSetting::value("/tool/volume").toInt();
    if(volume == 0)
        appSetting::setValue("/tool/volume", 80);

    QString ip = appSetting::value("/printerip").toString();
    int port = appSetting::value("/printerport").toInt();

    if(ip == "" || ip.length() < 8)
    {
        qDebug() << "init printer ip to 192.168.123.100";
        appSetting::setValue("/printerip", "192.168.123.100");
        appSetting::setValue("/printerport", 9100);
    }
    if(port == 0 || port < 1000)
    {
        qDebug() << "init printer port to 9100";
        appSetting::setValue("/printerport", 9100);
    }

    //
    g_isSaveSampleImage = appSetting::value("/tool/pdfstate").toBool();
    //g_isSavePreviewImage = g_isSaveSampleImage;

}

// add by wim
void Tool::on_btnMusic_clicked()
{
    //MusicSetting mSetting;
    getWinManage()->showWindowByType(WIN_MUSIC);
    //connect(&WindowsManagers.Music, SIGNAL(sendSIGNAL(int)), this, SIGNAL(sendSIGNAL(int)));
    //mSetting.exec();
}

void Tool::on_pushButtonPrevPage_clicked()
{
    ui->pushButtonNextPage->setVisible(true);
    ui->pushButtonPrevPage->setVisible(false);
    ReleaseMoveRightSlide(0);
}

void Tool::on_pushButtonNextPage_clicked()
{
    ui->pushButtonNextPage->setVisible(false);
    ui->pushButtonPrevPage->setVisible(true);
    ReleaseMoveLeftSlide(0);
}

void Tool::on_btnUpdate_clicked()
{
    // 显示软件更新设置界面
    getWinManage()->showWindowByType(WIN_UPDATE_SET);

}

void Tool::on_btnSettings_clicked()
{
    //setting->updateInfo();
    //QApplication::setActiveWindow(setting);
    //setting->show();
    qDebug()<<"Settings";
    getWinManage()->showWindowByType(WIN_SET);
}

void Tool::on_btnTheme_clicked()
{
    //themebackground *themepicture = new themebackground();
    //this->hide();
    //themepicture->show();
    qDebug()<<"themebackground";
    getWinManage()->showWindowByType(WIN_THEME);
}

void Tool::on_btnBluetooth_clicked()
{
    qDebug()<<"bluetooth";
    getWinManage()->showWindowByType(WIN_BT);
}

void Tool::on_btnStorageStat_clicked()
{
    qDebug()<<"Showing StorageStat Form";
    getWinManage()->showWindowByType(WIN_RUNSTATUS);
}

/************************************** 滑屏处理 **************************************/
//滑动异常处理
void Tool::InquireWidgetLocation()
{
    int duration = 300;
    QPoint locationPoint(0,0);
    if(ui->widget->pos()!=locationPoint && ui->widget_2->pos()!=locationPoint)
    {
        qDebug()<<"-----widget1:"<<ui->widget->pos()<<"  widget2:"<<ui->widget_2->pos();
        if(CurrentPage == 1)
            ReleaseMoveLeftSlideReset(duration);
        else if(CurrentPage == 2)
            ReleaseMoveRightSlideReset(duration);
        InquireLocation->stop();
    }
}

//页面根据实时轨迹左滑
void Tool::QPropertyAnimationMoveLeft()
{
    int duration = 0;       //持续时间
    if(CurrentPage == 1)
    {
        m_leftAnimation->setDuration(duration);
        m_leftAnimation->setStartValue(QPoint(m_startPos.x(),0));
        m_leftAnimation->setEndValue(Page1_LR_Pos);

        m_rightAnimation->setDuration(duration);
        m_rightAnimation->setStartValue(QPoint(800-m_startPos.x(),0));
        m_rightAnimation->setEndValue(Page2_L_Pos);
    }
    else if(CurrentPage == 2)
    {
        m_rightAnimation->setDuration(duration);
        m_rightAnimation->setStartValue(QPoint(m_startPos.x(),0));
        m_rightAnimation->setEndValue(Page1_LR_Pos);

        m_leftAnimation->setDuration(duration);
        m_leftAnimation->setStartValue(QPoint(800-m_startPos.x(),0));
        m_leftAnimation->setEndValue(Page2_L_Pos);
    }
    m_leftAnimation->start();
    m_rightAnimation->start();
}

//页面根据实时轨迹右滑
void Tool::QPropertyAnimationMoveRight()
{
    int duration = 0;       //持续时间
    if(CurrentPage == 1)
    {
        m_leftAnimation->setDuration(duration);
        m_leftAnimation->setStartValue(QPoint(m_startPos.x(),0));
        m_leftAnimation->setEndValue(Page1_LR_Pos);

        m_rightAnimation->setDuration(duration);
        m_rightAnimation->setStartValue(QPoint(0,0));
        m_rightAnimation->setEndValue(Page2_R_Pos);
    }
    else if(CurrentPage == 2)
    {
        m_rightAnimation->setDuration(duration);
        m_rightAnimation->setStartValue(QPoint(m_startPos.x(),0));
        m_rightAnimation->setEndValue(Page1_LR_Pos);

        m_leftAnimation->setDuration(duration);
        m_leftAnimation->setStartValue(QPoint(0,0));
        m_leftAnimation->setEndValue(Page2_R_Pos);
    }
    m_leftAnimation->start();
    m_rightAnimation->start();
}

//界面在duration时间内滑到下一页
void Tool::ReleaseMoveLeftSlide(int duration)
{
    m_leftAnimation->setDuration(duration);
    m_leftAnimation->setStartValue(Page1_LR_Pos);
    m_leftAnimation->setEndValue(QPoint(-800,0));

    m_rightAnimation->setDuration(duration);
    m_rightAnimation->setStartValue(Page2_L_Pos);
    m_rightAnimation->setEndValue(QPoint(0,0));

    m_leftAnimation->start();
    m_rightAnimation->start();

    CurrentPage = 2;
    SlidePage = 2;
}

//界面在duration时间内滑到上一页
void Tool::ReleaseMoveRightSlide(int duration)
{
    m_rightAnimation->setDuration(duration);
    m_rightAnimation->setStartValue(Page1_LR_Pos);
    m_rightAnimation->setEndValue(QPoint(800,0));

    m_leftAnimation->setDuration(duration);
    m_leftAnimation->setStartValue(Page2_R_Pos);
    m_leftAnimation->setEndValue(QPoint(0,0));

    m_leftAnimation->start();
    m_rightAnimation->start();

    CurrentPage = 1;
    SlidePage = 1;
}

//左滑取消恢复到当前页
void Tool::ReleaseMoveLeftSlideReset(int duration)
{
    m_leftAnimation->setDuration(duration);
    m_leftAnimation->setStartValue(Page1_LR_Pos);
    m_leftAnimation->setEndValue(QPoint(0,0));

    m_rightAnimation->setDuration(duration);
    m_rightAnimation->setStartValue(Page2_L_Pos);
    m_rightAnimation->setEndValue(QPoint(800,0));

    m_leftAnimation->start();
    m_rightAnimation->start();
}

//右滑取消恢复到当前页
void Tool::ReleaseMoveRightSlideReset(int duration)
{
    m_rightAnimation->setDuration(duration);
    m_rightAnimation->setStartValue(Page1_LR_Pos);
    m_rightAnimation->setEndValue(QPoint(0,0));

    m_leftAnimation->setDuration(duration);
    m_leftAnimation->setStartValue(Page2_R_Pos);
    m_leftAnimation->setEndValue(QPoint(-800,0));

    m_leftAnimation->start();
    m_rightAnimation->start();
}

void Tool::on_btnDistCalibrate_clicked()
{
    // 弹出距离校准窗口
    g_WinMeasure->setIsDistCalibration(true);
    WinMeasure::setOperationMode(operationMode_NormalMeasure);
    getWinManage()->showWindowByType(WIN_MEASURE);
}
