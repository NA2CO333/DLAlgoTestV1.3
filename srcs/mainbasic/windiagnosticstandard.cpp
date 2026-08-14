#include "windiagnosticstandard.h"
#include "ui_windiagnosticstandard.h"

#include <QLineEdit>

#include "winmanage.h"
#include "global.h"
#include "globalclass.h"
#include "result.h"
#include "appsetting.h"

// 屈光数据精度
#define M_PRECISION   0.25F

/// ====================================================================================================
/// class CBusiDataDiagnostic
///

void CBusiDataDiagnostic::reset()
{
    for (int i = 0; i < 5; i++) {
        diagnosticStandards[i].setToDefault((enAgeRange)i);
    }

}

bool CBusiDataDiagnostic::isEqualTo(const CBusiDataDiagnostic &_busi_data) const
{
    for (int i = 0; i < 5; i++) {
        if (Util::compDouble(this->diagnosticStandards[i].astigmatismFollowUp, _busi_data.diagnosticStandards[i].astigmatismFollowUp) != 0) {
            return false;
        }
        if (Util::compDouble(this->diagnosticStandards[i].astigmatismDiagnose, _busi_data.diagnosticStandards[i].astigmatismDiagnose) != 0) {
            return false;
        }
        if (Util::compDouble(this->diagnosticStandards[i].myopiaFollowUp, _busi_data.diagnosticStandards[i].myopiaFollowUp) != 0) {
            return false;
        }
        if (Util::compDouble(this->diagnosticStandards[i].myopiaDiagnose, _busi_data.diagnosticStandards[i].myopiaDiagnose) != 0) {
            return false;
        }
        if (Util::compDouble(this->diagnosticStandards[i].hyperopiaFollowUp, _busi_data.diagnosticStandards[i].hyperopiaFollowUp) != 0) {
            return false;
        }
        if (Util::compDouble(this->diagnosticStandards[i].hyperopiaDiagnose, _busi_data.diagnosticStandards[i].hyperopiaDiagnose) != 0) {
            return false;
        }
    }

    return true;
}

/// ====================================================================================================
/// class WinDiagnosticStandard
///

QLineEdit *g_edtOfSpinBox[6];

CBusiDataDiagnostic WinDiagnosticStandard::busiDataOrigin;

//
WinDiagnosticStandard::WinDiagnosticStandard(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::WinDiagnosticStandard)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    // 清掉设计期间设置的 StyleSheet
    QList<QWidget *> list_childs = this->findChildren<QWidget *>();
    for (int i = 0; i < list_childs.size(); i++) {
        list_childs[i]->setStyleSheet("");
    }

    // SpinBox 控件批量设置
    QList<CDoubleSpinBox *> list_spin_box = this->findChildren<CDoubleSpinBox *>();
    CDoubleSpinBox *spin_box = Q_NULLPTR;
    for (int i = 0; i < list_spin_box.count(); i++) {
        spin_box = list_spin_box[i];

        // 基本属性设置
        spin_box->setDecimals(2);
        spin_box->setMinimum(-8.0);
        spin_box->setMaximum(8.0);
        spin_box->setSingleStep(M_PRECISION);
        spin_box->setWrapping(false);

        // 取消窗体设计期间的设置值
        spin_box->setValue(0.0);

        // 禁用自动全选
        spin_box->setAutoSelectDisabled(true);

        // 事件槽函数设置
        //QObject::connect(spin_box, (void (CDoubleSpinBox::*)(double))(&CDoubleSpinBox::valueChanged), this, &WinDiagnosticStandard::slot_CDoubleSpinBox_valueChanged, Qt::QueuedConnection);
        QObject::connect(spin_box, QOverload<double>::of(&CDoubleSpinBox::valueChanged), this, &WinDiagnosticStandard::slot_CDoubleSpinBox_valueChanged, Qt::QueuedConnection);
        QObject::connect(spin_box, &CDoubleSpinBox::sigReachedExtremeValue, this, &WinDiagnosticStandard::slot_CDoubleSpinBox_ReachedExtremeValue, Qt::QueuedConnection);

    }
    ui->doubleSpinBox_0->setMinimum(0.0);

    //
    ui->wgtTabBackground->lower();

    //
    configToBusiData(WinDiagnosticStandard::busiDataOrigin);

}

WinDiagnosticStandard::~WinDiagnosticStandard()
{
    delete ui;
}

// 计算柱状图长度
double calcHistogramLength(const double _v, const double _s_0, const double _s_1, const double _s_2)
{
    static constexpr double MAX = 8.0;
    double ret = 0;
    if (_v <= _s_0) {
        ret = 0;
    } else if (_v < _s_1) {
        ret = 0 + (_v - _s_0) / (_s_1 - _s_0);
    } else if (_v < _s_2) {
        ret = 1 + (_v - _s_1) / (_s_2 - _s_1);
    } else if (_v < MAX) {
        ret = 2 + (_v - _s_2) / (MAX - _s_2);
    } else {
        ret = 3;
    }
    return ret;
}

bool WinDiagnosticStandard::getDiagnostic(const CPatient &_patient,
                                          bool _has_right, double &_astigmatism_r, double &_myopia_r, double &_hyperopia_r,
                                          bool _has_left, double &_astigmatism_l, double &_myopia_l, double &_hyperopia_l)
{
    static const CBusiDataDiagnostic &diagnose_data = WinDiagnosticStandard::busiDataOrigin;

    enAgeRange age_range = _patient.getAgeRange();
    if (!(age_range >= ageRange_Min && age_range <= ageRange_Max)) {
        //logWarning();
        return false;
    }

    const stDiagnosticStandard &standard = diagnose_data.diagnosticStandards[(int)age_range];

    //
    if (_has_right) {
        _astigmatism_r  = calcHistogramLength(std::abs(_patient.patientrighteyecyl.toDouble()), 0, standard.astigmatismFollowUp, standard.astigmatismDiagnose);
        _myopia_r       = calcHistogramLength(_patient.patientrighteyesph.toDouble() * -1, standard.hyperopiaFollowUp * -1, standard.myopiaFollowUp * -1, standard.myopiaDiagnose * -1);    // 近视的大小方向和另外两种相反
        _hyperopia_r    = calcHistogramLength(_patient.patientrighteyesph.toDouble(), standard.myopiaFollowUp, standard.hyperopiaFollowUp, standard.hyperopiaDiagnose);

        // 限制最小值
        _astigmatism_r  = std::max(0.1, _astigmatism_r );
        _myopia_r       = std::max(0.1, _myopia_r      );
        _hyperopia_r    = std::max(0.1, _hyperopia_r   );
    }

    if (_has_left) {
        _astigmatism_l  = calcHistogramLength(std::abs(_patient.patientlefteyecyl.toDouble()), 0, standard.astigmatismFollowUp, standard.astigmatismDiagnose);
        _myopia_l       = calcHistogramLength(_patient.patientlefteyesph.toDouble() * -1, standard.hyperopiaFollowUp * -1, standard.myopiaFollowUp * -1, standard.myopiaDiagnose * -1);
        _hyperopia_l    = calcHistogramLength(_patient.patientlefteyesph.toDouble(), standard.myopiaFollowUp, standard.hyperopiaFollowUp, standard.hyperopiaDiagnose);

        // 限制最小值
        _astigmatism_l  = std::max(0.1, _astigmatism_l );
        _myopia_l       = std::max(0.1, _myopia_l      );
        _hyperopia_l    = std::max(0.1, _hyperopia_l   );
    }

    //
    return true;
}

void WinDiagnosticStandard::showEvent(QShowEvent *)
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

    // 根据正负散光的配置设置散光编辑框的方向及最大最小值
    //if (Result::isCylNegative()) {
    //    // TODO: ？
    //}

}

void WinDiagnosticStandard::updateTheme(enThemeType _theme)
{
    static QString form_style_black;

    static bool form_style_black_read = false;
    if (!form_style_black_read) {
        Util::readFileToQStr(":/resource/qss/windiagnosticstandard.qss", form_style_black);
        form_style_black_read = true;
    }

    //
    //QPalette palette;
    if (themeType_Black == _theme) {
        this->setStyleSheet(form_style_black);

        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));

        // 背景图
        //ui->lblBgImg->setPixmap(QPixmap(":/resource/black_theme/diagnostic-bg.png"));
    } else if (themeType_White == _theme) {
        // TODO:
    }
    //this->setPalette(palette);
    //this->setAutoFillBackground(true);

}

void WinDiagnosticStandard::updateLanguage()
{
    // 标签按键的“年龄段”
    for (int i = 0; i < 5; i++) {
        ui->tabWidget->setTabText(i, CAgeRange::getAgeRangeDesc((enAgeRange)i));
    }

    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("随访诊治标准"));  // "Following Up & Diagnostic Criterion"

    //
    //if (language) {
    //    ui->lblHeadNormal->setText("正常");
    //    ui->lblHeadFollowUp->setText("定期随访");
    //    ui->lblHeadDiagnose->setText("诊治");
    //
    //    ui->lblAstigmatism->setText("散光：");
    //    ui->lblMyopia->setText("近视：");
    //    ui->lblHyperopia->setText("远视：");
    //
    //    ui->lblChartMyopiaDiagnose->setText("诊治");
    //    ui->lblChartMyopiaFollow->setText("定期随访");
    //    ui->lblChartNormal->setText("正常");
    //    ui->lblChartHyperopiaFollow->setText("定期随访");
    //    ui->lblChartHyperopiaDiagno->setText("诊治");
    //
    //    ui->btnRestore->setText("恢复默认");
    //
    //    ui->lblHome->setText("主页");
    //    ui->lblSave->setText("保存");
    //    ui->lblBack->setText("返回");
    //} else {
    //    ui->lblHeadNormal->setText("Normal");
    //    ui->lblHeadFollowUp->setText("FollowUp");      // (Regular Follow Up)
    //    ui->lblHeadDiagnose->setText("Diagnose");
    //
    //    ui->lblAstigmatism->setText("Astigmatism:");
    //    ui->lblMyopia->setText("Myopia:");
    //    ui->lblHyperopia->setText("Hyperopia:");
    //
    //    ui->lblChartMyopiaDiagnose->setText("Diagnose");
    //    ui->lblChartMyopiaFollow->setText("FollowUp");
    //    ui->lblChartNormal->setText("Normal");
    //    ui->lblChartHyperopiaFollow->setText("FollowUp");
    //    ui->lblChartHyperopiaDiagno->setText("Diagnose");
    //
    //    ui->btnRestore->setText("Restore Default");
    //
    //    ui->lblHome->setText("Home");
    //    ui->lblSave->setText("Save");
    //    ui->lblBack->setText("Back");
    //}

}

QString WinDiagnosticStandard::getCfg_astigmatismFollowUp()
{
    return appSetting::value("/standard/astigmatismFollowUp").toString();
}

QString WinDiagnosticStandard::getCfg_astigmatismDiagnose()
{
    return appSetting::value("/standard/astigmatismDiagnose").toString();
}

QString WinDiagnosticStandard::getCfg_myopiaFollowUp()
{
    return appSetting::value("/standard/myopiaFollowUp").toString();
}

QString WinDiagnosticStandard::getCfg_myopiaDiagnose()
{
    return appSetting::value("/standard/myopiaDiagnose").toString();
}

QString WinDiagnosticStandard::getCfg_hyperopiaFollowUp()
{
    return appSetting::value("/standard/hyperopiaFollowUp").toString();
}

QString WinDiagnosticStandard::getCfg_hyperopiaDiagnose()
{
    return appSetting::value("/standard/hyperopiaDiagnose").toString();
}

void WinDiagnosticStandard::configToBusiData(CBusiDataDiagnostic &_busi_data)
{
    QString cfg_astigmatism_follow      = getCfg_astigmatismFollowUp();
    QStringList list_astigmatism_follow     = cfg_astigmatism_follow.split(',');
    QString cfg_astigmatism_diagnose    = getCfg_astigmatismDiagnose();
    QStringList list_astigmatism_diagnose   = cfg_astigmatism_diagnose.split(',');
    QString cfg_myopia_follow           = getCfg_myopiaFollowUp();
    QStringList list_myopia_follow          = cfg_myopia_follow.split(',');
    QString cfg_myopia_diagnose         = getCfg_myopiaDiagnose();
    QStringList list_myopia_diagnose        = cfg_myopia_diagnose.split(',');
    QString cfg_hyperopia_follow        = getCfg_hyperopiaFollowUp();
    QStringList list_hyperopia_follow       = cfg_hyperopia_follow.split(',');
    QString cfg_hyperopia_diagnose      = getCfg_hyperopiaDiagnose();
    QStringList list_hyperopia_diagnose     = cfg_hyperopia_diagnose.split(',');

    if (cfg_astigmatism_follow.size() == 0 && cfg_astigmatism_diagnose.size() == 0) {   // 若没有配置，则设为默认值，且保存到配置文件
        _busi_data.reset();
        saveBusiData(_busi_data);
    } else {
        for (int i = 0; i < 5; i++) {
            _busi_data.diagnosticStandards[i].astigmatismFollowUp   = (list_astigmatism_follow.size()    >= i + 1 ? list_astigmatism_follow.at(i).toDouble() : 0);
            _busi_data.diagnosticStandards[i].astigmatismDiagnose   = (list_astigmatism_diagnose.size()  >= i + 1 ? list_astigmatism_diagnose.at(i).toDouble() : 0);
            _busi_data.diagnosticStandards[i].myopiaFollowUp        = (list_myopia_follow.size()         >= i + 1 ? list_myopia_follow.at(i).toDouble() : 0);
            _busi_data.diagnosticStandards[i].myopiaDiagnose        = (list_myopia_diagnose.size()       >= i + 1 ? list_myopia_diagnose.at(i).toDouble() : 0);
            _busi_data.diagnosticStandards[i].hyperopiaFollowUp     = (list_hyperopia_follow.size()      >= i + 1 ? list_hyperopia_follow.at(i).toDouble() : 0);
            _busi_data.diagnosticStandards[i].hyperopiaDiagnose     = (list_hyperopia_diagnose.size()    >= i + 1 ? list_hyperopia_diagnose.at(i).toDouble() : 0);
        }
    }
}

void WinDiagnosticStandard::saveBusiData(const CBusiDataDiagnostic &_busi_data)
{
    QString cfg_str;

    cfg_str = "";
    for (int i = 0; i < 5; i++) {
        if (i > 0) {
            cfg_str += ",";
        }
        cfg_str += Util::doubleToQStr(_busi_data.diagnosticStandards[i].astigmatismFollowUp, 2);
    }
    appSetting::setValue("/standard/astigmatismFollowUp", cfg_str);

    cfg_str = "";
    for (int i = 0; i < 5; i++) {
        if (i > 0) {
            cfg_str += ",";
        }
        cfg_str += Util::doubleToQStr(_busi_data.diagnosticStandards[i].astigmatismDiagnose, 2);
    }
    appSetting::setValue("/standard/astigmatismDiagnose", cfg_str);

    cfg_str = "";
    for (int i = 0; i < 5; i++) {
        if (i > 0) {
            cfg_str += ",";
        }
        cfg_str += Util::doubleToQStr(_busi_data.diagnosticStandards[i].myopiaFollowUp, 2);
    }
    appSetting::setValue("/standard/myopiaFollowUp", cfg_str);

    cfg_str = "";
    for (int i = 0; i < 5; i++) {
        if (i > 0) {
            cfg_str += ",";
        }
        cfg_str += Util::doubleToQStr(_busi_data.diagnosticStandards[i].myopiaDiagnose, 2);
    }
    appSetting::setValue("/standard/myopiaDiagnose", cfg_str);

    cfg_str = "";
    for (int i = 0; i < 5; i++) {
        if (i > 0) {
            cfg_str += ",";
        }
        cfg_str += Util::doubleToQStr(_busi_data.diagnosticStandards[i].hyperopiaFollowUp, 2);
    }
    appSetting::setValue("/standard/hyperopiaFollowUp", cfg_str);

    cfg_str = "";
    for (int i = 0; i < 5; i++) {
        if (i > 0) {
            cfg_str += ",";
        }
        cfg_str += Util::doubleToQStr(_busi_data.diagnosticStandards[i].hyperopiaDiagnose, 2);
    }
    appSetting::setValue("/standard/hyperopiaDiagnose", cfg_str);

    //
    busiDataOrigin = _busi_data;

}

void WinDiagnosticStandard::busiDataToUi(const CBusiDataDiagnostic &_busi_data)
{
    //
    for (int i = 0; i < 5; i++) {
        tabWidgetData[i] = _busi_data.diagnosticStandards[i];
    }

    //
    tabWidgetDataToEdits(ui->tabWidget->currentIndex());

}

void WinDiagnosticStandard::tabWidgetDataToEdits(const int _idx_tab)
{
    currentTabIndex = _idx_tab;

    //
    const stDiagnosticStandard &standard = tabWidgetData[_idx_tab];

    // 禁用 SpinBox 的信号
    ui->doubleSpinBox_0->blockSignals(true);
    ui->doubleSpinBox_1->blockSignals(true);
    ui->doubleSpinBox_2->blockSignals(true);
    ui->doubleSpinBox_3->blockSignals(true);
    ui->doubleSpinBox_4->blockSignals(true);
    ui->doubleSpinBox_5->blockSignals(true);

    // 设置 SpinBox 的值之前清空最大最小值
    ui->doubleSpinBox_0->setMinimum(-8.0);
    ui->doubleSpinBox_0->setMaximum(8.0);
    ui->doubleSpinBox_1->setMinimum(-8.0);
    ui->doubleSpinBox_1->setMaximum(8.0);
    ui->doubleSpinBox_2->setMinimum(-8.0);
    ui->doubleSpinBox_2->setMaximum(8.0);
    ui->doubleSpinBox_3->setMinimum(-8.0);
    ui->doubleSpinBox_3->setMaximum(8.0);
    ui->doubleSpinBox_4->setMinimum(-8.0);
    ui->doubleSpinBox_4->setMaximum(8.0);
    ui->doubleSpinBox_5->setMinimum(-8.0);
    ui->doubleSpinBox_5->setMaximum(8.0);

    // 设置 SpinBox 的值
    setSpinBoxValue(ui->doubleSpinBox_0, standard.astigmatismFollowUp);
    setSpinBoxValue(ui->doubleSpinBox_1, standard.astigmatismDiagnose);
    setSpinBoxValue(ui->doubleSpinBox_2, standard.myopiaDiagnose);
    setSpinBoxValue(ui->doubleSpinBox_3, standard.myopiaFollowUp);
    setSpinBoxValue(ui->doubleSpinBox_4, standard.hyperopiaFollowUp);
    setSpinBoxValue(ui->doubleSpinBox_5, standard.hyperopiaDiagnose);

    // 设置 SpinBox 的值之后再次设置的最大最小值
    ui->doubleSpinBox_0->setMinimum(0.0);
    ui->doubleSpinBox_0->setMaximum(ui->doubleSpinBox_1->value() - M_PRECISION);

    ui->doubleSpinBox_1->setMaximum(8.0);

    ui->doubleSpinBox_2->setMinimum(-8.0);
    ui->doubleSpinBox_2->setMaximum(ui->doubleSpinBox_3->value() - M_PRECISION);

    ui->doubleSpinBox_3->setMinimum(ui->doubleSpinBox_2->value() + M_PRECISION);
    ui->doubleSpinBox_3->setMaximum(ui->doubleSpinBox_4->value() - M_PRECISION);

    ui->doubleSpinBox_4->setMinimum(ui->doubleSpinBox_3->value() + M_PRECISION);
    ui->doubleSpinBox_4->setMaximum(ui->doubleSpinBox_5->value() - M_PRECISION);

    ui->doubleSpinBox_5->setMinimum(ui->doubleSpinBox_4->value() + M_PRECISION);
    ui->doubleSpinBox_5->setMaximum(8.0);

    // 恢复 SpinBox 的信号
    ui->doubleSpinBox_0->blockSignals(false);
    ui->doubleSpinBox_1->blockSignals(false);
    ui->doubleSpinBox_2->blockSignals(false);
    ui->doubleSpinBox_3->blockSignals(false);
    ui->doubleSpinBox_4->blockSignals(false);
    ui->doubleSpinBox_5->blockSignals(false);

    // 设置图表标签的值
    ui->lblMyopeaDiagnose->setText(ui->doubleSpinBox_2->text());
    ui->lblMyopeaFollowUp->setText(ui->doubleSpinBox_3->text());
    ui->lblHyperopiaFollowUp->setText(ui->doubleSpinBox_4->text());
    ui->lblHyperopiaDiagnose->setText(ui->doubleSpinBox_5->text());

}

void WinDiagnosticStandard::editsToTabWidgetData(const int _idx_tab)
{
    //
    stDiagnosticStandard &standard = tabWidgetData[_idx_tab];

    //
    standard.astigmatismFollowUp  = ui->doubleSpinBox_0->value();
    standard.astigmatismDiagnose  = ui->doubleSpinBox_1->value();
    standard.myopiaDiagnose       = ui->doubleSpinBox_2->value();
    standard.myopiaFollowUp       = ui->doubleSpinBox_3->value();
    standard.hyperopiaFollowUp    = ui->doubleSpinBox_4->value();
    standard.hyperopiaDiagnose    = ui->doubleSpinBox_5->value();

}

void WinDiagnosticStandard::setSpinBoxValue(CDoubleSpinBox *_spin_box, double _value)
{
    _spin_box->setPrefix((_value < 0) ? "" : "+");   // 设置前缀，使正数显示“+”号
    _spin_box->setValue(_value);
}

void WinDiagnosticStandard::uiToBusiData(CBusiDataDiagnostic &_busi_data)
{
    //
    editsToTabWidgetData(ui->tabWidget->currentIndex());

    //
    for (int i = 0; i < 5; i++) {
        _busi_data.diagnosticStandards[i] = tabWidgetData[i];
    }

}

void WinDiagnosticStandard::askAndSave(const CBusiDataDiagnostic &_busi_data)
{
    QString text = tr("是否保存修改?");   // "Save the modifications?"
    bool ret = getWinManage()->showNoticeWin(text);
    if (ret) {
        saveBusiData(_busi_data);
    }
}

void WinDiagnosticStandard::slot_CDoubleSpinBox_valueChanged(double _value)
{
    QObject *sender = QObject::sender();
    CDoubleSpinBox *spin_box = dynamic_cast<CDoubleSpinBox *>(sender);
    if (spin_box) {
        // 设置前缀，使正数显示“+”号
        spin_box->setPrefix((_value < 0) ? "" : "+");

        // 设置前后控件的最大最小值
        QString sub1, sub2;
        int idx_str = Util::splitStr(spin_box->objectName(), "_", sub1, sub2);;
        if (idx_str > 0) {
            int idx_box = sub2.toUInt();
            if (idx_box >= 0 && idx_box <= 5) {
                if (0 == idx_box) {
                    ui->doubleSpinBox_1->setMinimum(_value + M_PRECISION);
                } else if (1 == idx_box) {
                    ui->doubleSpinBox_0->setMaximum(_value - M_PRECISION);
                } else if (2 == idx_box) {
                    ui->doubleSpinBox_3->setMinimum(_value + M_PRECISION);

                    //
                    ui->lblMyopeaDiagnose->setText(spin_box->text());
                } else if (3 == idx_box) {
                    ui->doubleSpinBox_2->setMaximum(_value - M_PRECISION);
                    ui->doubleSpinBox_4->setMinimum(_value + M_PRECISION);

                    //
                    ui->lblMyopeaFollowUp->setText(spin_box->text());
                } else if (4 == idx_box) {
                    ui->doubleSpinBox_3->setMaximum(_value - M_PRECISION);
                    ui->doubleSpinBox_5->setMinimum(_value + M_PRECISION);

                    //
                    ui->lblHyperopiaFollowUp->setText(spin_box->text());
                } else if (5 == idx_box) {
                    ui->doubleSpinBox_4->setMaximum(_value - M_PRECISION);

                    //
                    ui->lblHyperopiaDiagnose->setText(spin_box->text());
                }
            } else {
                logCritical(QString(__PRETTY_FUNCTION__) + ": logical error, index of SpinBox invalid, object setting failed!");
            }
        } else {
            logCritical(QString(__PRETTY_FUNCTION__) + ": logical error, SpinBox name format error, object not found!");
        }

    } else {
        logCritical(QString(__PRETTY_FUNCTION__) + ": logical error, SpinBox type cast failed!");
    }
}

void WinDiagnosticStandard::slot_CDoubleSpinBox_ReachedExtremeValue(int _extreme_type)
{
    QString msg = tr("已达到%1值"); // "%1 value reached"
    if (-1 == _extreme_type) {
        msg = msg.arg(tr("最小")); // "minimum"
    } else if (1 == _extreme_type) {
        msg = msg.arg(tr("最大"));    // "maximum"
    }
    if (-1 == _extreme_type || 1 == _extreme_type) {
        getWinManage()->showSuspensionPrompt(msg, 1000);
    } else {
        //logWarning("");
    }
}

void WinDiagnosticStandard::on_btnHome_clicked()
{
    CBusiDataDiagnostic busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    getWinManage()->showWindowByType(WIN_HOME);
}

void WinDiagnosticStandard::on_btnBack_clicked()
{
    CBusiDataDiagnostic busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    //getWinManage()->showWindowByType(WIN_TOOL);
    getWinManage()->backToLastWidget();
}

void WinDiagnosticStandard::on_btnSave_clicked()
{
    CBusiDataDiagnostic busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    } else {
        getWinManage()->showSuspensionPrompt(tr("数据未被修改")); // "Data not modified"
    }
}

void WinDiagnosticStandard::on_tabWidget_currentChanged(int index)
{
    //
    editsToTabWidgetData(currentTabIndex);

    //
    tabWidgetDataToEdits(index);
}

void WinDiagnosticStandard::on_btnRestore_clicked()
{
    CBusiDataDiagnostic busi_data;
    busi_data.reset();
    busiDataToUi(busi_data);
}
