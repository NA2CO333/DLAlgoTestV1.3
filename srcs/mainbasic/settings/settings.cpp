#include "settings.h"
#include "ui_settings.h"

#include "windowsmanager.h"
#include "appsetting.h"
#include "global.h"
#include "utilui.h"
#include "update.h"
#include "logger.h"

//
extern loginWin *mLogin;

//
CScreenTimeout g_ScreenTimeout      = screenTimeout_Default;            // 筛查超时
CShutdownNoOperation g_ShutdownSecs = shutdownNoOperation_Default;      // 无操作关机时间

enOpticalPathType g_opticalPathType = opticalPathType_General;          // 光路类型


/// ====================================================================================================
/// class CBusiDataSetting
///

// 重置
void CBusiDataSetting::reset()
{
    // 距离提示
    isEnableDistance        = true;
    // 是否保存 PDF
    isSavePreviewImage      = true;
    // 是否需要用户密码
    needLogin               = false;
    // 默认年龄段
    defaultAgeRange         = CGlobal::defaultAgeRange;
    // 筛查超时
    screenTimeout.reset();
    // 自动关机时间
    shutdownSecs.reset();
    // 版本类型（光路类型）
    opticalPathType.reset();
    // 参考视力记录方法
    visionNotation          = CGlobal::visionNotation;
    // 屏幕亮度
    screenBrightness        = CGlobal::getScreenBrightnessCfg();
    // 自动息屏
    autoScreenOff           = CGlobal::autoScreenOff;
    // 【普通/专业】模式
    algoMode                = CGlobal::algoMode;
    // 是否多次测量
    isMultiMeasure          = CGlobal::isMultiMeasure;
}

// 比较
bool CBusiDataSetting::isEqualTo(const CBusiDataSetting &_busi_data) const
{
    return (true
            && _busi_data.isEnableDistance      == this->isEnableDistance
            && _busi_data.isSavePreviewImage    == this->isSavePreviewImage
            && _busi_data.needLogin             == this->needLogin
            && _busi_data.defaultAgeRange       == this->defaultAgeRange
            && _busi_data.screenTimeout         == this->screenTimeout
            && _busi_data.shutdownSecs          == this->shutdownSecs
            && _busi_data.opticalPathType       == this->opticalPathType
            && _busi_data.visionNotation        == this->visionNotation
            && _busi_data.screenBrightness      == this->screenBrightness
            && _busi_data.autoScreenOff         == this->autoScreenOff
            && _busi_data.algoMode              == this->algoMode
            && _busi_data.isMultiMeasure        == this->isMultiMeasure
            );
}

/// ====================================================================================================
/// class settings
///

// 工具--设置界面，超时时间以及距离显示等
settings::settings(QWidget *parent) : CBaseWidget(parent), ui(new Ui::settings)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    // 设置 “默认年龄段” 下拉选项框的选项
    ui->cbbDefaultAgeRange->clear();
    QStringList list_age_range;
    CAgeRange::getAgeRangeDescList(list_age_range);
    ui->cbbDefaultAgeRange->addItems(list_age_range);

    // 设置 “筛查超时” 下拉选项框的选项
    QStringList list_screen_timeout;
    busiDataOrigin.screenTimeout.discrips(list_screen_timeout);
    ui->cbbScreenTimeout->clear();
    ui->cbbScreenTimeout->addItems(list_screen_timeout);

    // 设置 “无操作关机” 下拉选项框的选项
    QStringList list_poweroff;
    busiDataOrigin.shutdownSecs.discrips(list_poweroff);
    ui->cbbPowerOffTime->clear();
    ui->cbbPowerOffTime->addItems(list_poweroff);

    // 设置 “版本类型” 下拉选项框的选项
    QStringList list_optical_type;
    busiDataOrigin.opticalPathType.discrips(list_optical_type);
    ui->cbbVersionType->clear();
    ui->cbbVersionType->addItems(list_optical_type);

    // 设置 “视力记录法” 下拉选项框的选项
    QStringList list_vision_type;
    busiDataOrigin.visionNotation.discrips(list_vision_type);
    ui->cbbVisionNotation->clear();
    ui->cbbVisionNotation->addItems(list_vision_type);

    // 设置 “屏幕亮度” 下拉选项框的选项
    QStringList list_brightness;
    busiDataOrigin.screenBrightness.discrips(list_brightness);
    ui->cbbScreenBrightness->clear();
    ui->cbbScreenBrightness->addItems(list_brightness);

    // 【自动息屏】选项列表
    m_optionsAutoScreenOff << enAutoScreenOff::Duration_1
                           << enAutoScreenOff::Duration_2
                           << enAutoScreenOff::Duration_3
                           << enAutoScreenOff::Never;

    for (const auto &option : m_optionsAutoScreenOff) {
        ui->cbbAutoScreenOff->addItem(enumToText_AutoScreenOff(option), static_cast<int>(option));
    }

    // 全局变量初始化      // TODO: 清理，移到全局模块
    g_ScreenTimeout = getCfg_ScreenTimeout();     // 筛查超时
    g_ShutdownSecs = getCfg_PoweroffTime();       // 自动关机时间
    g_opticalPathType = getCfg_OpticalPathType();       // 版本类型（光路类型）

}

settings::~settings()
{
    delete ui;
}

bool settings::getCfg_IsEnableDistance()
{
    bool is_enabled = true;     // 缺省值
    QVariant is_enabled_vari = appSetting::value("/tool/ultraWaveNotice");
    if (is_enabled_vari.isValid()) {
        is_enabled = is_enabled_vari.toBool();
    } else {
        setCfg_IsEnableDistance(is_enabled);
    }
    return is_enabled;
}

void settings::setCfg_IsEnableDistance(bool _is_enabled)
{
    appSetting::setValue("/tool/ultraWaveNotice", _is_enabled);
}

enScreenTimeout settings::getCfg_ScreenTimeout()
{
    int timeout_secs = appSetting::value("tool/ScreenTimeout").toInt();
    CScreenTimeout screen_timeout;
    bool is_valid = screen_timeout.setValue((enScreenTimeout)timeout_secs);
    if (!is_valid) {
        //screen_timeout.reset();
        screen_timeout.setValue(screenTimeout_Default);
        appSetting::setValue("tool/ScreenTimeout", screen_timeout.toInt());
    }
    return screen_timeout.getValue();
}

enShutdownNoOperation settings::getCfg_PoweroffTime()
{
    int poweroff_secs = appSetting::value("tool/PowerOffTime").toInt();
    CShutdownNoOperation shutdown_no_operation;
    bool is_valid = shutdown_no_operation.setValue((enShutdownNoOperation)poweroff_secs);
    if (!is_valid) {
        shutdown_no_operation.reset();
        appSetting::setValue("tool/PowerOffTime", shutdown_no_operation.toInt());
    }
    return shutdown_no_operation.getValue();
}

enOpticalPathType settings::getCfg_OpticalPathType()
{
    return (enOpticalPathType)appSetting::value("/tool/versiontype").toInt();
}

int settings::getScreenTimeoutSecs()
{
    return g_ScreenTimeout.toInt();
}

int settings::getPoweroffTimeSecs()
{
    return g_ShutdownSecs.toInt();
}

void settings::showEvent(QShowEvent *)
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

    // 更新语言
    updateLanguage();

    // 更新主题
    updateTheme(getSysThemeType());

}

void settings::hideEvent(QHideEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

}

void settings::updateTheme(enThemeType _theme)          // TODO: 直接对整个窗体设置样式表，不必逐个部件设置
{
    //QPalette palette; //因为界面更新用的是自定义的函数,所以不能用调色板QPalette来设置背景图片了
    if(themeType_Black == _theme){
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p,list_Label){
            p->setStyleSheet("QLabel { color:rgb(204,204,204); }");
        }

        QList<QCheckBox *> list_CheckBox = findChildren<QCheckBox *>();
        foreach(QCheckBox *p,list_CheckBox){
            p->setStyleSheet("QCheckBox{background-color:transparent;color:rgb(250,250,252);} \
                              QCheckBox::indicator {width: 20px; height: 20px;} \
                              QCheckBox::indicator:checked{image:url(:/resource/checked.png); color:rgb(250,250,252);} \
                              QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png);}"
                             );
        }

        //ui->checkBoxSavePDF->setStyleSheet(         "QCheckBox{color:rgb(250,250,252);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        //ui->ckbUserPassword->setStyleSheet(         "QCheckBox{color:rgb(250,250,252);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        //ui->ckbEnableDistance->setStyleSheet(         "QCheckBox{color:rgb(250,250,252);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        //ui->comboBox_Screen_Mode1->setStyleSheet(   "QCheckBox{color:rgb(250,250,252);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        //ui->comboBox_Screen_Mode2->setStyleSheet(   "QCheckBox{color:rgb(250,250,252);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");

        QString style_combobox = R"(
QComboBox{ background-color: rgb(51,56,62); color: rgb(204,204,204); }
QComboBox::drop-down { image: url(:/resource/black_theme/combo-arrow-down_b.png); }
QComboBox QAbstractItemView { background-color: rgb(51,56,62); color: rgb(204,204,204); border: 2px solid rgb(149,149,149); }
QComboBox QAbstractItemView::item { min-height: 40px; }
QComboBox QAbstractItemView::item:selected { background-color: rgb(48,140,198); color: rgb(255,255,255); }
                )";

        ui->cbbScreenTimeout->setStyleSheet(style_combobox);
        ui->cbbPowerOffTime->setStyleSheet(style_combobox);
        ui->cbbVersionType->setStyleSheet(style_combobox);
        ui->cbbDefaultAgeRange->setStyleSheet(style_combobox);
        ui->cbbVisionNotation->setStyleSheet(style_combobox);
        ui->cbbScreenBrightness->setStyleSheet(style_combobox);
        ui->cbbAutoScreenOff->setStyleSheet(style_combobox);

        ui->pushButtonPassWord->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(28,28,30); color:rgb(250,250,252);}");
        ui->pushButton_restore->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(28,28,30); color:rgb(250,250,252);}");

        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/black_theme/save_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
    }
    else{
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p,list_Label){
            p->setStyleSheet("color:rgb(1,1,1);");
        }

        QList<QCheckBox *> list_CheckBox = findChildren<QCheckBox *>();
        foreach(QCheckBox *p,list_CheckBox){
            p->setStyleSheet("QCheckBox::indicator {width: 20px; height: 20px;} \
                              QCheckBox::indicator:checked{image:url(:/resource/checked.png); color:rgb(1,1,1);}"
                             );
        }

        ui->checkBoxSavePDF->setStyleSheet("QCheckBox{color:rgb(1,1,1);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        ui->ckbUserPassword->setStyleSheet("QCheckBox{color:rgb(1,1,1);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        ui->ckbEnableDistance->setStyleSheet("QCheckBox{color:rgb(1,1,1);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        ui->comboBox_Screen_Mode1->setStyleSheet("QCheckBox{color:rgb(1,1,1);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        ui->comboBox_Screen_Mode1->setStyleSheet("QCheckBox{color:rgb(1,1,1);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);}");
        ui->cbbScreenTimeout->setStyleSheet("QComboBox{color:rgb(1,1,1);}");
        ui->cbbPowerOffTime->setStyleSheet("QComboBox{color:rgb(1,1,1);}");
        ui->cbbVersionType->setStyleSheet("QComboBox{color:rgb(1,1,1);}");
        ui->cbbDefaultAgeRange->setStyleSheet("QComboBox{color:rgb(1,1,1);}");
        ui->pushButtonPassWord->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(201,201,204); color:rgb(1,1,1);}");
        ui->pushButton_restore->setStyleSheet("QPushButton{border-radius:3px; height:30px; background-color:rgb(201,201,204); color:rgb(1,1,1);}");
        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/white_theme/save_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
    }
    //this->setPalette(palette);

}

void settings::updateLanguage()
{
    //
    //if (language) {
    //    ui->ckbEnableDistance->setText("启用测距");
    //    ui->ckbUserPassword->setText("用户密码");
    //    ui->checkBoxSavePDF->setText("保存预览图");
    //    ui->label_Home->setText("主页");
    //    ui->label_Back->setText("返回");
    //    ui->label_Save->setText("保存");
    //
    //    ui->pushButtonPassWord->setText("修改密码");
    //    ui->pushButton_restore->setText("恢复设置");
    //    ui->comboBox_Screen_Mode1->setText("普通模式");
    //    ui->comboBox_Screen_Mode2->setText("专业模式(对受测者、操作者及环境要求较高)");
    //
    //    ui->lblDefaultAgeRange->setText("默认年龄段");
    //    ui->label_Screen->setText("筛查超时");
    //    ui->label_Power_off->setText("无操作关机");
    //
    //    ui->label_VersionType->setText("光路类型");
    //    ui->lblVisionNotation->setText("视力记录法");
    //    ui->lblScreenBrightness->setText("屏幕亮度");
    //} else {
    //    ui->ckbEnableDistance->setText("ShowDistance");
    //    ui->ckbUserPassword->setText("UserPassword");
    //    ui->checkBoxSavePDF->setText("SavePreviewImage");
    //    ui->label_Home->setText("Home");
    //    ui->label_Back->setText("Back");
    //    ui->label_Save->setText("Save");
    //
    //    ui->pushButtonPassWord->setText("EditPassword");
    //    ui->pushButton_restore->setText("ResetAll");
    //    ui->comboBox_Screen_Mode1->setText("NormalMode");
    //    ui->comboBox_Screen_Mode2->setText("Professional mode (Higher demand for tester,operator,environment)");
    //
    //    ui->lblDefaultAgeRange->setText("DefaultAgeRange");
    //    ui->label_Screen->setText("ScreeningTimeout");
    //    ui->label_Power_off->setText("TimedShutdown");
    //
    //    ui->label_VersionType->setText("Optical Type");
    //    ui->lblVisionNotation->setText("VisionNotation");
    //    ui->lblScreenBrightness->setText("ScreenBrightness");
    //}

    // 下拉列表的文本更新
    QStringList list_age_range;                                         // 默认年龄段
    CAgeRange::getAgeRangeDescList(list_age_range);
    for (int i = 0; i < ui->cbbDefaultAgeRange->count(); i++) {
        ui->cbbDefaultAgeRange->setItemText(i, list_age_range.at(i));
    }

    QStringList list_screen_timeout;                                    // 筛查超时
    busiDataOrigin.screenTimeout.discrips(list_screen_timeout);
    for (int i = 0; i < ui->cbbScreenTimeout->count(); i++) {
        ui->cbbScreenTimeout->setItemText(i, list_screen_timeout.at(i));
    }

    QStringList list_poweroff;                                          // 无操作关机
    busiDataOrigin.shutdownSecs.discrips(list_poweroff);
    for (int i = 0; i < ui->cbbPowerOffTime->count(); i++) {
        ui->cbbPowerOffTime->setItemText(i, list_poweroff.at(i));
    }

    QStringList list_optical_type;                                      // 光路类型
    busiDataOrigin.opticalPathType.discrips(list_optical_type);
    for (int i = 0; i < ui->cbbVersionType->count(); i++) {
        ui->cbbVersionType->setItemText(i, list_optical_type.at(i));
    }

    QStringList list_vision_type;                                       // 视力记录法
    busiDataOrigin.visionNotation.discrips(list_vision_type);
    for (int i = 0; i < ui->cbbVisionNotation->count(); i++) {
        ui->cbbVisionNotation->setItemText(i, list_vision_type.at(i));
    }

    // 标题更新
    getWinManage()->updateWindowTitle(this, tr("设置"));  // "Settings"

}

void settings::configToBusiData(CBusiDataSetting &_busi_data)
{
    // 业务数据对象置零
    _busi_data.reset();

    // 距离提示
    _busi_data.isEnableDistance = getCfg_IsEnableDistance();

    // 是否保存 PDF
    _busi_data.isSavePreviewImage = g_isSaveSampleImage;

    // 是否需要用户密码
    _busi_data.needLogin = appSetting::value("/login/needlogin").toBool();

    // 默认年龄段
    _busi_data.defaultAgeRange = CGlobal::defaultAgeRange;

    // 筛查超时
    _busi_data.screenTimeout = g_ScreenTimeout;

    // 自动关机时间
    _busi_data.shutdownSecs = g_ShutdownSecs;

    // 版本类型（光路类型）
    _busi_data.opticalPathType = g_opticalPathType;

    // 参考视力记录方法
    _busi_data.visionNotation = CGlobal::visionNotation;

    // 屏幕亮度
    _busi_data.screenBrightness = CGlobal::getScreenBrightnessCfg();

    // 自动息屏
    _busi_data.autoScreenOff = CGlobal::autoScreenOff;

    // 【普通/专业】模式
    _busi_data.algoMode = CGlobal::algoMode;

    // 版本号
    _busi_data.version = tr("软件版本：") + aboutdevice::getVerStrForReg();  // "Software Vertion: "

    // 是否多次测量
    _busi_data.isMultiMeasure = CGlobal::isMultiMeasure;

}

void settings::saveBusiData(const CBusiDataSetting &_busi_data)
{
    // 距离提示
    setCfg_IsEnableDistance(_busi_data.isEnableDistance);

    // 是否保存 PDF
    g_isSaveSampleImage = _busi_data.isSavePreviewImage;
    //g_isSavePreviewImage = g_isSaveSampleImage;

    appSetting::setValue("/tool/pdfstate", g_isSaveSampleImage);

    // 是否需要用户密码
    appSetting::setValue("/login/needlogin", _busi_data.needLogin);

    // 默认年龄段
    CGlobal::defaultAgeRange = _busi_data.defaultAgeRange;

    // 筛查超时
    g_ScreenTimeout = _busi_data.screenTimeout;
    appSetting::setValue("/tool/ScreenTimeout", g_ScreenTimeout.toInt());

    // 自动关机时间
    g_ShutdownSecs = _busi_data.shutdownSecs;
    appSetting::setValue("/tool/PowerOffTime", g_ShutdownSecs.toInt());

    // 光路类型（原“版本类型”）
    g_opticalPathType = _busi_data.opticalPathType.getValue();
    appSetting::setValue("/tool/versiontype", (int)g_opticalPathType);

    // 参考视力记录方法
    CGlobal::visionNotation = _busi_data.visionNotation;

    // 屏幕亮度
    CGlobal::setScreenBrightnessCfg(_busi_data.screenBrightness.getValue());

    PowerControl::setScreenBrightnessNormal();

    // 自动息屏
    CGlobal::autoScreenOff = _busi_data.autoScreenOff;

    // 【普通/专业】模式
    CGlobal::algoMode = _busi_data.algoMode;

    // 版本号
    //_busi_data.version = read only;

    // 是否多次测量
    CGlobal::isMultiMeasure = _busi_data.isMultiMeasure;

    // 保存全局配置
    CGlobal::saveConfs();

    // 当前业务数据对象克隆
    busiDataOrigin = _busi_data;
}

void settings::busiDataToUi(const CBusiDataSetting &_busi_data)
{
    // 距离提示
    ui->ckbEnableDistance->setChecked(_busi_data.isEnableDistance);

    // 是否保存预览图
    ui->checkBoxSavePDF->setChecked(_busi_data.isSavePreviewImage);

    // 是否需要用户密码
    ui->ckbUserPassword->setChecked(_busi_data.needLogin);

    // 默认年龄段
    ui->cbbDefaultAgeRange->setCurrentIndex((int)_busi_data.defaultAgeRange);

    // 筛查超时
    ui->cbbScreenTimeout->setCurrentIndex(_busi_data.screenTimeout.currentIndex());

    // 自动关机时间
    ui->cbbPowerOffTime->setCurrentIndex(_busi_data.shutdownSecs.currentIndex());

    // 版本类型（光路类型）
    ui->cbbVersionType->setCurrentIndex((int)_busi_data.opticalPathType.currentIndex());

    // 参考视力记录方法
    ui->cbbVisionNotation->setCurrentIndex(_busi_data.visionNotation.currentIndex());

    // 屏幕亮度
    ui->cbbScreenBrightness->setCurrentIndex(_busi_data.screenBrightness.currentIndex());

    // 自动息屏
    int idx_auto_screen_off = m_optionsAutoScreenOff.indexOf(_busi_data.autoScreenOff);
    if (idx_auto_screen_off < 0 || idx_auto_screen_off > ui->cbbAutoScreenOff->count() - 1) {
        logCritical(QString("%1::%2(): idx_auto_screen_off(=%3) is out of bound!").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(idx_auto_screen_off));
        idx_auto_screen_off = -1;
    }
    ui->cbbAutoScreenOff->setCurrentIndex(idx_auto_screen_off);

    // 【普通/专业】模式
    ui->comboBox_Screen_Mode1->setChecked(algoMode_General == _busi_data.algoMode);
    ui->comboBox_Screen_Mode2->setChecked(!(algoMode_General == _busi_data.algoMode));

    // 版本号
    ui->lblVersion->setText(_busi_data.version);

    // 是否多次测量
    ui->ckbIsMultiMeasure->setChecked(_busi_data.isMultiMeasure);

}

void settings::uiToBusiData(CBusiDataSetting &_busi_data)
{
    // 距离提示
    _busi_data.isEnableDistance = ui->ckbEnableDistance->isChecked();

    // 是否保存预览图
    _busi_data.isSavePreviewImage = ui->checkBoxSavePDF->isChecked();

    // 是否需要用户密码
    _busi_data.needLogin = ui->ckbUserPassword->isChecked();

    // 默认年龄段
    _busi_data.defaultAgeRange = (enAgeRange)ui->cbbDefaultAgeRange->currentIndex();

    // 筛查超时
    _busi_data.screenTimeout.setCurrentIndex(ui->cbbScreenTimeout->currentIndex());

    // 自动关机时间
    _busi_data.shutdownSecs.setCurrentIndex(ui->cbbPowerOffTime->currentIndex());

    // 版本类型（光路类型）
    _busi_data.opticalPathType.setCurrentIndex(ui->cbbVersionType->currentIndex());

    // 参考视力记录方法
    _busi_data.visionNotation = (enVisionNotation)ui->cbbVisionNotation->currentIndex();

    // 屏幕亮度
    _busi_data.screenBrightness.setCurrentIndex(ui->cbbScreenBrightness->currentIndex());

    // 自动息屏
    _busi_data.autoScreenOff = static_cast<enAutoScreenOff>(ui->cbbAutoScreenOff->currentData().toInt());

    // 【普通/专业】模式
    _busi_data.algoMode = (ui->comboBox_Screen_Mode1->isChecked() ? algoMode_General : algoMode_Professional);

    // 版本号
    //_busi_data.version = read only;

    // 是否多次测量
    _busi_data.isMultiMeasure = ui->ckbIsMultiMeasure->isChecked();

}

// 询问是否保存并根据用户的选择保存或放弃
void settings::askAndSave(const CBusiDataSetting &_busi_data)
{
    QString text = tr("是否保存修改?");   // "Save the modifications?"
    bool ret = getWinManage()->showNoticeWin(text);
    if (ret) {
        saveBusiData(_busi_data);
    }
}

void settings::on_pushButton_Save_clicked()
{
    CBusiDataSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    } else {
        getWinManage()->showSuspensionPrompt(tr("数据未被修改")); // "Data not modified"
    }

    //this->ignore();     //不需要关闭窗口时才用到 tao 2020.4.27
    //this->accept();     //需要关闭窗口时才用到(//发送OK状态)
}

void settings::on_pushButton_Back_clicked()
{
    CBusiDataSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    getWinManage()->showWindowByType(WIN_TOOL);
}

void settings::on_pushButton_Home_clicked()
{
    CBusiDataSetting busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    getWinManage()->showWindowByType(WIN_HOME);
}

void settings::on_pushButtonPassWord_clicked()
{
    mLogin->setMode(loginWin::oldPassWord);
    getWinManage()->showWindowByType(WIN_LOG);
}

void settings::on_pushButton_restore_clicked()
{
    QString text = tr("是否重置为出厂设置？"); // "Reset to factory settings?"
    NoticeWin msg;
    msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    msg.setContent(text);
    if (msg.exec() == QDialog::Accepted)            // TODO: 默认配置值的定义，及对配置文件的直接访问，应由 Global 模块负责
    {
        // 本窗体 ==============================

        //
        CGlobal::restoreConfs();

        //
        busiDataOrigin.reset();

        //
        saveBusiData(busiDataOrigin);

        //
        busiDataToUi(busiDataOrigin);

        //
        this->update();

        // 其它 ===============================

        g_SingleDualEye = singleDualEyeMode_Both;       // 单双眼模式
        g_MinResolution = false;   //分辨率0.25D
        g_isHmMode = false;     //正常模式

        //appSetting::setValue("global/language", G_LANGUAGE_ENGLISH);   // 默认英文

        // 数据传输参数
//        appSetting::setValue("/data/ip", "192.168.5.100");
//        appSetting::setValue("/data/port", "8081");
//        appSetting::setValue("/data/datapath", "/data");
//        appSetting::setValue("/data/imagepath", "/image");
//        appSetting::setValue("/data/authpath", "/auth");
//        appSetting::setValue("/data/usrpath", "/client");
//        appSetting::setValue("/data/clientlistpath", "/clientlist");
//        appSetting::setValue("/data/usr", "test");
//        appSetting::setValue("/data/code", "123");
//        appSetting::setValue("/data/authId", "ssi");
//        appSetting::setValue("/data/upload", true);
//        appSetting::setValue("/data/usrverify", false);
//        appSetting::setValue("/data/uploadimage", false);
//        appSetting::setValue("/data/autoGetInfo", false);

        // 打印设置


        // 音乐
        appSetting::setValue("/tool/playMode", (int)playbackMode_SingleLoop);
        appSetting::setValue("/tool/volume", 80);

        // 主题
        setSysThemeType(themeType_Black, true);      //黑色主题

        //wifi
        appSetting::setValue("/wifi/isWifiOpened", false);      //关闭wifi状态
        //gWinWifi->setIsOpened(false);                           //关闭wifi

        //蓝牙
        WinBluetooth::setBtStatCfg(false);       //关闭蓝牙
        //appSetting::setValue("/tool/bluetoothflag",false);    //蓝牙模式关

        //工程界面
        saveImage = false;                                      //关闭存图
        //appSetting::setValue("tool/saveimage", saveImage);

        //
        appSetting::sync();
    }
}

void settings::on_comboBox_Screen_Mode1_clicked(bool checked)
{
    ui->comboBox_Screen_Mode2->setChecked(!checked);
}

void settings::on_comboBox_Screen_Mode2_clicked(bool checked)
{
    ui->comboBox_Screen_Mode1->setChecked(!checked);
}
