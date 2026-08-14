#include "winmultiresults.h"
#include "ui_winmultiresults.h"

#include "algo-invoker.h"
#include "util-common.h"
#include "global.h"
#include "algo-invoker.h"

//
WinMultiResults::WinMultiResults(QWidget *_parent) :
    CBaseWidget(_parent),
    ui(new Ui::WinMultiResults)
{
    ui->setupUi(this);
}

WinMultiResults::~WinMultiResults()
{
    delete ui;
}

void WinMultiResults::showEvent(QShowEvent *)
{
    // 更新主题样式
    updateTheme(getSysThemeType());

}

void WinMultiResults::setData(const CPatient &_pat, bool _has_right, bool _has_left, bool _is_cyl_negative)
{
    //
    QString right_sph       = _pat.patientrighteyesph   ;
    QString right_cyl       = _pat.patientrighteyecyl   ;
    QString right_ax        = _pat.patientrighteyeax    ;

    QString left_sph        = _pat.patientlefteyesph    ;
    QString left_cyl        = _pat.patientlefteyecyl    ;
    QString left_ax         = _pat.patientlefteyeax     ;

    double  RESULT_1_R_SPH  = _pat.RESULT_1_R_SPH   ;
    double  RESULT_1_R_CYL  = _pat.RESULT_1_R_CYL   ;
    double  RESULT_1_R_AX   = _pat.RESULT_1_R_AX    ;
    double  RESULT_1_L_SPH  = _pat.RESULT_1_L_SPH   ;
    double  RESULT_1_L_CYL  = _pat.RESULT_1_L_CYL   ;
    double  RESULT_1_L_AX   = _pat.RESULT_1_L_AX    ;

    double  RESULT_2_R_SPH  = _pat.RESULT_2_R_SPH   ;
    double  RESULT_2_R_CYL  = _pat.RESULT_2_R_CYL   ;
    double  RESULT_2_R_AX   = _pat.RESULT_2_R_AX    ;
    double  RESULT_2_L_SPH  = _pat.RESULT_2_L_SPH   ;
    double  RESULT_2_L_CYL  = _pat.RESULT_2_L_CYL   ;
    double  RESULT_2_L_AX   = _pat.RESULT_2_L_AX    ;

    double  RESULT_3_R_SPH  = _pat.RESULT_3_R_SPH   ;
    double  RESULT_3_R_CYL  = _pat.RESULT_3_R_CYL   ;
    double  RESULT_3_R_AX   = _pat.RESULT_3_R_AX    ;
    double  RESULT_3_L_SPH  = _pat.RESULT_3_L_SPH   ;
    double  RESULT_3_L_CYL  = _pat.RESULT_3_L_CYL   ;
    double  RESULT_3_L_AX   = _pat.RESULT_3_L_AX    ;

    //
    bool has_right_1 = _has_right && !(Util::compDouble(RESULT_1_R_SPH, 0) == 0 && Util::compDouble(RESULT_1_R_CYL, 0) == 0 && Util::compDouble(RESULT_1_R_AX, 0) == 0);
    bool has_left_1  = _has_left  && !(Util::compDouble(RESULT_1_L_SPH, 0) == 0 && Util::compDouble(RESULT_1_L_CYL, 0) == 0 && Util::compDouble(RESULT_1_L_AX, 0) == 0);

    bool has_right_2 = _has_right && !(Util::compDouble(RESULT_2_R_SPH, 0) == 0 && Util::compDouble(RESULT_2_R_CYL, 0) == 0 && Util::compDouble(RESULT_2_R_AX, 0) == 0);
    bool has_left_2  = _has_left  && !(Util::compDouble(RESULT_2_L_SPH, 0) == 0 && Util::compDouble(RESULT_2_L_CYL, 0) == 0 && Util::compDouble(RESULT_2_L_AX, 0) == 0);

    bool has_right_3 = _has_right && !(Util::compDouble(RESULT_3_R_SPH, 0) == 0 && Util::compDouble(RESULT_3_R_CYL, 0) == 0 && Util::compDouble(RESULT_3_R_AX, 0) == 0);
    bool has_left_3  = _has_left  && !(Util::compDouble(RESULT_3_L_SPH, 0) == 0 && Util::compDouble(RESULT_3_L_CYL, 0) == 0 && Util::compDouble(RESULT_3_L_AX, 0) == 0);

    //
    CAlgoInvoker::switchCylSign_StrAx(right_sph       , right_cyl     , right_ax      , _is_cyl_negative, right_sph       , right_cyl     , right_ax      );
    CAlgoInvoker::switchCylSign_StrAx(left_sph        , left_cyl      , left_ax       , _is_cyl_negative, left_sph        , left_cyl      , left_ax       );

    CAlgoInvoker::switchCylSign_DblAx(RESULT_1_R_SPH  , RESULT_1_R_CYL, RESULT_1_R_AX , _is_cyl_negative, RESULT_1_R_SPH  , RESULT_1_R_CYL, RESULT_1_R_AX );
    CAlgoInvoker::switchCylSign_DblAx(RESULT_1_L_SPH  , RESULT_1_L_CYL, RESULT_1_L_AX , _is_cyl_negative, RESULT_1_L_SPH  , RESULT_1_L_CYL, RESULT_1_L_AX );

    CAlgoInvoker::switchCylSign_DblAx(RESULT_2_R_SPH  , RESULT_2_R_CYL, RESULT_2_R_AX , _is_cyl_negative, RESULT_2_R_SPH  , RESULT_2_R_CYL, RESULT_2_R_AX );
    CAlgoInvoker::switchCylSign_DblAx(RESULT_2_L_SPH  , RESULT_2_L_CYL, RESULT_2_L_AX , _is_cyl_negative, RESULT_2_L_SPH  , RESULT_2_L_CYL, RESULT_2_L_AX );

    CAlgoInvoker::switchCylSign_DblAx(RESULT_3_R_SPH  , RESULT_3_R_CYL, RESULT_3_R_AX , _is_cyl_negative, RESULT_3_R_SPH  , RESULT_3_R_CYL, RESULT_3_R_AX );
    CAlgoInvoker::switchCylSign_DblAx(RESULT_3_L_SPH  , RESULT_3_L_CYL, RESULT_3_L_AX , _is_cyl_negative, RESULT_3_L_SPH  , RESULT_3_L_CYL, RESULT_3_L_AX );

    //
    const double VISION_PREC = (g_MinResolution ? 0.01 : 0.25);      // 屈光度的精度

    double se_1_r = CAlgoInvoker::getSE(RESULT_1_R_SPH, RESULT_1_R_CYL);
    double se_2_r = CAlgoInvoker::getSE(RESULT_2_R_SPH, RESULT_2_R_CYL);
    double se_3_r = CAlgoInvoker::getSE(RESULT_3_R_SPH, RESULT_3_R_CYL);

    double se_1_l = CAlgoInvoker::getSE(RESULT_1_L_SPH, RESULT_1_L_CYL);
    double se_2_l = CAlgoInvoker::getSE(RESULT_2_L_SPH, RESULT_2_L_CYL);
    double se_3_l = CAlgoInvoker::getSE(RESULT_3_L_SPH, RESULT_3_L_CYL);

    ui->lblOd1Ds->setText(  has_right_1 ? CAlgoInvoker::doubleToDiopterStr(RESULT_1_R_SPH, VISION_PREC) : "");
    ui->lblOd1Dc->setText(  has_right_1 ? CAlgoInvoker::doubleToDiopterStr(RESULT_1_R_CYL, VISION_PREC) : "");
    ui->lblOd1Ax->setText(  has_right_1 && Util::compDouble(RESULT_1_R_CYL , 0) != 0 ? QString::number(RESULT_1_R_AX , 'f', 0) : "");
    ui->lblOd1Se->setText(  has_right_1 ? CAlgoInvoker::doubleToDiopterStr(se_1_r, VISION_PREC) : "");

    ui->lblOd2Ds->setText(  has_right_2 ? CAlgoInvoker::doubleToDiopterStr(RESULT_2_R_SPH, VISION_PREC) : "");
    ui->lblOd2Dc->setText(  has_right_2 ? CAlgoInvoker::doubleToDiopterStr(RESULT_2_R_CYL, VISION_PREC) : "");
    ui->lblOd2Ax->setText(  has_right_2 && Util::compDouble(RESULT_2_R_CYL , 0) != 0 ? QString::number(RESULT_2_R_AX , 'f', 0) : "");
    ui->lblOd2Se->setText(  has_right_2 ? CAlgoInvoker::doubleToDiopterStr(se_2_r, VISION_PREC) : "");

    ui->lblOd3Ds->setText(  has_right_3 ? CAlgoInvoker::doubleToDiopterStr(RESULT_3_R_SPH, VISION_PREC) : "");
    ui->lblOd3Dc->setText(  has_right_3 ? CAlgoInvoker::doubleToDiopterStr(RESULT_3_R_CYL, VISION_PREC) : "");
    ui->lblOd3Ax->setText(  has_right_3 && Util::compDouble(RESULT_3_R_CYL , 0) != 0 ? QString::number(RESULT_3_R_AX , 'f', 0) : "");
    ui->lblOd3Se->setText(  has_right_3 ? CAlgoInvoker::doubleToDiopterStr(se_3_r, VISION_PREC) : "");

    ui->lblOdAvgDs->setText(_has_right  ? right_sph  : "");
    ui->lblOdAvgDc->setText(_has_right  ? right_cyl  : "");
    ui->lblOdAvgAx->setText(_has_right  ? right_ax   : "");
    ui->lblOdAvgSe->setText(_has_right  ? _pat.patientrightse : "");

    ui->lblOs1Ds->setText(  has_left_1 ? CAlgoInvoker::doubleToDiopterStr(RESULT_1_L_SPH, VISION_PREC) : "");
    ui->lblOs1Dc->setText(  has_left_1 ? CAlgoInvoker::doubleToDiopterStr(RESULT_1_L_CYL, VISION_PREC) : "");
    ui->lblOs1Ax->setText(  has_left_1 && Util::compDouble(RESULT_1_L_CYL , 0) != 0 ? QString::number(RESULT_1_L_AX , 'f', 0) : "");
    ui->lblOs1Se->setText(  has_left_1 ? CAlgoInvoker::doubleToDiopterStr(se_1_l, VISION_PREC) : "");

    ui->lblOs2Ds->setText(  has_left_2 ? CAlgoInvoker::doubleToDiopterStr(RESULT_2_L_SPH, VISION_PREC) : "");
    ui->lblOs2Dc->setText(  has_left_2 ? CAlgoInvoker::doubleToDiopterStr(RESULT_2_L_CYL, VISION_PREC) : "");
    ui->lblOs2Ax->setText(  has_left_2 && Util::compDouble(RESULT_2_L_CYL , 0) != 0 ? QString::number(RESULT_2_L_AX , 'f', 0) : "");
    ui->lblOs2Se->setText(  has_left_2 ? CAlgoInvoker::doubleToDiopterStr(se_2_l, VISION_PREC) : "");

    ui->lblOs3Ds->setText(  has_left_3 ? CAlgoInvoker::doubleToDiopterStr(RESULT_3_L_SPH, VISION_PREC) : "");
    ui->lblOs3Dc->setText(  has_left_3 ? CAlgoInvoker::doubleToDiopterStr(RESULT_3_L_CYL, VISION_PREC) : "");
    ui->lblOs3Ax->setText(  has_left_3 && Util::compDouble(RESULT_3_L_CYL , 0) != 0 ? QString::number(RESULT_3_L_AX , 'f', 0) : "");
    ui->lblOs3Se->setText(  has_left_3 ? CAlgoInvoker::doubleToDiopterStr(se_3_l, VISION_PREC) : "");

    ui->lblOsAvgDs->setText(_has_left  ? left_sph   : "");
    ui->lblOsAvgDc->setText(_has_left  ? left_cyl   : "");
    ui->lblOsAvgAx->setText(_has_left  ? left_ax    : "");
    ui->lblOsAvgSe->setText(_has_left  ? _pat.patientleftse : "");
}

void WinMultiResults::updateTheme(enThemeType _theme)
{
    // 加载样式表文本
    static QString form_style_black;
    static bool form_style_black_read = false;
    if (!form_style_black_read) {
        Util::readFileToQStr(":/resource/qss/winmultiresults.qss", form_style_black);
        form_style_black_read = true;
    }

    // 设置样式表
    if (themeType_Black == _theme) {
        this->setStyleSheet(form_style_black);
    } else if (themeType_White == _theme) {
        // TODO:
    }
    //this->update();
}
