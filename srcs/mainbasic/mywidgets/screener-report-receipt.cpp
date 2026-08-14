#include "screener-report-receipt.h"

#include "util-common.h"
#include "global.h"
#include "algo-invoker.h"

//
using CLine  = Common::CReportTextLine;
using CField = Common::CReportTextField;

//
CScreenerReportReceipt::CScreenerReportReceipt(QObject *_parent) : Common::CReportText(_parent)
{
    //
    setLineWidth(W_LINE);

}

void CScreenerReportReceipt::setData(const CPatient &_pat, const QString &_judgement_desc, enSingleDualEyeMode _single_dual_eye,
                                     bool _has_estimated, bool _is_cyl_negative)
{
    m_pat.cloneFrom(_pat);
    m_judgementDescStr = _judgement_desc;
    m_singleDualEye = _single_dual_eye;
    m_hasEstimatedVision = _has_estimated;
    m_isCylNegative = _is_cyl_negative;
}

void CScreenerReportReceipt::updateData()
{
    //
    bool has_right = (singleDualEyeMode_Right & m_singleDualEye);
    bool has_left  = (singleDualEyeMode_Left & m_singleDualEye);

    //
    QString right_sph       = m_pat.patientrighteyesph  ;
    QString right_cyl       = m_pat.patientrighteyecyl  ;
    QString right_ax        = m_pat.patientrighteyeax   ;

    QString left_sph        = m_pat.patientlefteyesph   ;
    QString left_cyl        = m_pat.patientlefteyecyl   ;
    QString left_ax         = m_pat.patientlefteyeax    ;

    double  RESULT_1_R_SPH  = m_pat.RESULT_1_R_SPH      ;
    double  RESULT_1_R_CYL  = m_pat.RESULT_1_R_CYL      ;
    double  RESULT_1_R_AX   = m_pat.RESULT_1_R_AX       ;
    double  RESULT_1_L_SPH  = m_pat.RESULT_1_L_SPH      ;
    double  RESULT_1_L_CYL  = m_pat.RESULT_1_L_CYL      ;
    double  RESULT_1_L_AX   = m_pat.RESULT_1_L_AX       ;

    double  RESULT_2_R_SPH  = m_pat.RESULT_2_R_SPH      ;
    double  RESULT_2_R_CYL  = m_pat.RESULT_2_R_CYL      ;
    double  RESULT_2_R_AX   = m_pat.RESULT_2_R_AX       ;
    double  RESULT_2_L_SPH  = m_pat.RESULT_2_L_SPH      ;
    double  RESULT_2_L_CYL  = m_pat.RESULT_2_L_CYL      ;
    double  RESULT_2_L_AX   = m_pat.RESULT_2_L_AX       ;

    double  RESULT_3_R_SPH  = m_pat.RESULT_3_R_SPH      ;
    double  RESULT_3_R_CYL  = m_pat.RESULT_3_R_CYL      ;
    double  RESULT_3_R_AX   = m_pat.RESULT_3_R_AX       ;
    double  RESULT_3_L_SPH  = m_pat.RESULT_3_L_SPH      ;
    double  RESULT_3_L_CYL  = m_pat.RESULT_3_L_CYL      ;
    double  RESULT_3_L_AX   = m_pat.RESULT_3_L_AX       ;

    //
    m_hasRight_1 = !(Util::compDouble(RESULT_1_R_SPH, 0) == 0 && Util::compDouble(RESULT_1_R_CYL, 0) == 0 && Util::compDouble(RESULT_1_R_AX, 0) == 0);
    m_hasLeft_1  = !(Util::compDouble(RESULT_1_L_SPH, 0) == 0 && Util::compDouble(RESULT_1_L_CYL, 0) == 0 && Util::compDouble(RESULT_1_L_AX, 0) == 0);

    m_hasRight_2 = !(Util::compDouble(RESULT_2_R_SPH, 0) == 0 && Util::compDouble(RESULT_2_R_CYL, 0) == 0 && Util::compDouble(RESULT_2_R_AX, 0) == 0);
    m_hasLeft_2  = !(Util::compDouble(RESULT_2_L_SPH, 0) == 0 && Util::compDouble(RESULT_2_L_CYL, 0) == 0 && Util::compDouble(RESULT_2_L_AX, 0) == 0);

    m_hasRight_3 = !(Util::compDouble(RESULT_3_R_SPH, 0) == 0 && Util::compDouble(RESULT_3_R_CYL, 0) == 0 && Util::compDouble(RESULT_3_R_AX, 0) == 0);
    m_hasLeft_3  = !(Util::compDouble(RESULT_3_L_SPH, 0) == 0 && Util::compDouble(RESULT_3_L_CYL, 0) == 0 && Util::compDouble(RESULT_3_L_AX, 0) == 0);

    //
    CAlgoInvoker::switchCylSign_StrAx(right_sph       , right_cyl     , right_ax      , m_isCylNegative   , right_sph       , right_cyl     , right_ax      );
    CAlgoInvoker::switchCylSign_StrAx(left_sph        , left_cyl      , left_ax       , m_isCylNegative   , left_sph        , left_cyl      , left_ax       );

    CAlgoInvoker::switchCylSign_DblAx(RESULT_1_R_SPH  , RESULT_1_R_CYL, RESULT_1_R_AX , m_isCylNegative   , RESULT_1_R_SPH  , RESULT_1_R_CYL, RESULT_1_R_AX );
    CAlgoInvoker::switchCylSign_DblAx(RESULT_1_L_SPH  , RESULT_1_L_CYL, RESULT_1_L_AX , m_isCylNegative   , RESULT_1_L_SPH  , RESULT_1_L_CYL, RESULT_1_L_AX );

    CAlgoInvoker::switchCylSign_DblAx(RESULT_2_R_SPH  , RESULT_2_R_CYL, RESULT_2_R_AX , m_isCylNegative   , RESULT_2_R_SPH  , RESULT_2_R_CYL, RESULT_2_R_AX );
    CAlgoInvoker::switchCylSign_DblAx(RESULT_2_L_SPH  , RESULT_2_L_CYL, RESULT_2_L_AX , m_isCylNegative   , RESULT_2_L_SPH  , RESULT_2_L_CYL, RESULT_2_L_AX );

    CAlgoInvoker::switchCylSign_DblAx(RESULT_3_R_SPH  , RESULT_3_R_CYL, RESULT_3_R_AX , m_isCylNegative   , RESULT_3_R_SPH  , RESULT_3_R_CYL, RESULT_3_R_AX );
    CAlgoInvoker::switchCylSign_DblAx(RESULT_3_L_SPH  , RESULT_3_L_CYL, RESULT_3_L_AX , m_isCylNegative   , RESULT_3_L_SPH  , RESULT_3_L_CYL, RESULT_3_L_AX );

    //
    const double VISION_PREC = (g_MinResolution ? 0.01 : 0.25);

    NAME            = m_pat.patientname;
    GENDER          = m_pat.getSexDisc();
    NUMBER          = m_pat.patientid;
    BIRTHDATE       = m_pat.getBirthDateStr();

    R_DS_1          = CAlgoInvoker::doubleToDiopterStr(RESULT_1_R_SPH , VISION_PREC);
    R_DC_1          = CAlgoInvoker::doubleToDiopterStr(RESULT_1_R_CYL , VISION_PREC);
    R_AX_1          = Util::compDouble(RESULT_1_R_CYL, 0) != 0 ? QString::number(Util::roundDouble(RESULT_1_R_AX, VISION_PREC), 'f', 0) : "";
    R_SE_1          = CAlgoInvoker::doubleToDiopterStr(CAlgoInvoker::getSE(RESULT_1_R_SPH, RESULT_1_R_CYL), VISION_PREC);

    R_DS_2          = CAlgoInvoker::doubleToDiopterStr(RESULT_2_R_SPH , VISION_PREC);
    R_DC_2          = CAlgoInvoker::doubleToDiopterStr(RESULT_2_R_CYL , VISION_PREC);
    R_AX_2          = Util::compDouble(RESULT_2_R_CYL, 0) != 0 ? QString::number(Util::roundDouble(RESULT_2_R_AX, VISION_PREC), 'f', 0) : "";
    R_SE_2          = CAlgoInvoker::doubleToDiopterStr(CAlgoInvoker::getSE(RESULT_2_R_SPH, RESULT_2_R_CYL), VISION_PREC);

    R_DS_3          = CAlgoInvoker::doubleToDiopterStr(RESULT_3_R_SPH , VISION_PREC);
    R_DC_3          = CAlgoInvoker::doubleToDiopterStr(RESULT_3_R_CYL , VISION_PREC);
    R_AX_3          = Util::compDouble(RESULT_3_R_CYL, 0) != 0 ? QString::number(Util::roundDouble(RESULT_3_R_AX, VISION_PREC), 'f', 0) : "";
    R_SE_3          = CAlgoInvoker::doubleToDiopterStr(CAlgoInvoker::getSE(RESULT_3_R_SPH, RESULT_3_R_CYL), VISION_PREC);

    R_DS_AVG        = right_sph;
    R_DC_AVG        = right_cyl;
    R_AX_AVG        = right_ax;
    R_SE_AVG        = m_pat.patientrightse;

    L_DS_1          = CAlgoInvoker::doubleToDiopterStr(RESULT_1_L_SPH , VISION_PREC);
    L_DC_1          = CAlgoInvoker::doubleToDiopterStr(RESULT_1_L_CYL , VISION_PREC);
    L_AX_1          = Util::compDouble(RESULT_1_L_CYL, 0) != 0 ? QString::number(Util::roundDouble(RESULT_1_L_AX, VISION_PREC), 'f', 0) : "";
    L_SE_1          = CAlgoInvoker::doubleToDiopterStr(CAlgoInvoker::getSE(RESULT_1_L_SPH, RESULT_1_L_CYL), VISION_PREC);

    L_DS_2          = CAlgoInvoker::doubleToDiopterStr(RESULT_2_L_SPH , VISION_PREC);
    L_DC_2          = CAlgoInvoker::doubleToDiopterStr(RESULT_2_L_CYL , VISION_PREC);
    L_AX_2          = Util::compDouble(RESULT_2_L_CYL, 0) != 0 ? QString::number(Util::roundDouble(RESULT_2_L_AX, VISION_PREC), 'f', 0) : "";
    L_SE_2          = CAlgoInvoker::doubleToDiopterStr(CAlgoInvoker::getSE(RESULT_2_L_SPH, RESULT_2_L_CYL), VISION_PREC);

    L_DS_3          = CAlgoInvoker::doubleToDiopterStr(RESULT_3_L_SPH    , VISION_PREC);
    L_DC_3          = CAlgoInvoker::doubleToDiopterStr(RESULT_3_L_CYL    , VISION_PREC);
    L_AX_3          = Util::compDouble(RESULT_3_L_CYL, 0) != 0 ? QString::number(Util::roundDouble(RESULT_3_L_AX, VISION_PREC), 'f', 0) : "";
    L_SE_3          = CAlgoInvoker::doubleToDiopterStr(CAlgoInvoker::getSE(RESULT_3_L_SPH, RESULT_3_L_CYL), VISION_PREC);

    L_DS_AVG        = left_sph;
    L_DC_AVG        = left_cyl;
    L_AX_AVG        = left_ax;
    L_SE_AVG        = m_pat.patientleftse;

    PS_R            = m_pat.patientrightpd + "mm";
    PS_L            = m_pat.patientleftpd + "mm";

    EYE_POSI_R      = (has_right ? m_pat.getEyePositionDiscR() : "");
    EYE_POSI_L      = (has_left ? m_pat.getEyePositionDiscL() : "");

    PD              = m_pat.patientpd + "mm";

    VISUAL          = tr("右眼") + " " + (has_right ? CAlgoInvoker::diopterToVision(m_pat.patientrighteyesph, m_pat.patientrighteyecyl, CGlobal::visionNotation.getValue()) : "") + ", "     // "OD"
                    + tr("左眼") + " " + (has_left ? CAlgoInvoker::diopterToVision(m_pat.patientlefteyesph, m_pat.patientlefteyecyl, CGlobal::visionNotation.getValue()) : "");             // "OS"

    JUDGEMENT       = m_judgementDescStr.replace('\n', ' ');

    ORGANIZATION    = tr("检查机构：") + CGlobal::organizationName;      // "Organization: "
    OPERATOR        = tr("操作者：") + (!CGlobal::operatorName.isEmpty() ? CGlobal::operatorName : "        ");         // "Operator: "

    EXAM_TIME       = QLocale(QLocale::English).toString(m_pat.getTestTime(), "yyyy-MM-dd hh:mm AP");
}

bool CScreenerReportReceipt::buildReportLayout(QString &_err_msg)
{
    //
    Common::CReportText *report = dynamic_cast<Common::CReportText *>(this);
    if (!report) {
        _err_msg = "Program Error: Failed to cast report template pointer!";
        return false;
    }

    //
    //bool is_multi = m_pat.IS_MULTI;
    bool is_multi = CGlobal::isMultiMeasure;

    //
    (*report) << CLine() << CField(tr("屈光检查单"), -1, Qt::AlignCenter);   // "Refraction Examination Report"
    (*report) << CLine() << CField(QString(" ").repeated(W_LINE));
    (*report) << CLine() << CField(tr("姓  名："), W_COL_NAME_) << NAME << CField(" ", 1) << CField(tr("出生日期："), W_COL_BIRTH_) << BIRTHDATE;   // "Name: ", "Birthdate: "
    (*report) << CLine() << CField(tr("诊疗号："), W_COL_NUM_) << NUMBER << CField(" ", 1) << CField(tr("性    别："), W_COL_GENDER_) << GENDER;    // "ID: ", "Gender: "
    (*report) << CLine() << CField(QString("-").repeated(W_LINE));

    (*report) << CLine() << CField("OD/" + tr("右眼"));   // "Right eye"
    (*report) << CLine() << CField("    ")
              << CField(tr("球镜(DS)"), W_COL_DS, Qt::AlignCenter)
              << CField(tr("柱镜(DC)"), W_COL_DC, Qt::AlignCenter)
              << CField(tr("轴位(AX)"), W_COL_AX, Qt::AlignCenter)
              << CField(tr("等效球镜(SE)"), W_COL_SE, Qt::AlignCenter);   // "DS", "DC", "AX", "SE"
    if (is_multi) {
        if (m_hasRight_1) {
            (*report) << CLine() << CField("    ") << R_DS_1 << R_DC_1 << R_AX_1 << R_SE_1;
        }
        if (m_hasRight_2) {
            (*report) << CLine() << CField("    ") << R_DS_2 << R_DC_2 << R_AX_2 << R_SE_2;
        }
        if (m_hasRight_3) {
            (*report) << CLine() << CField("    ") << R_DS_3 << R_DC_3 << R_AX_3 << R_SE_3;
        }
    }
    (*report) << CLine() << CField(CGlobal::isMultiMeasure ? "AVG " : "    ") << R_DS_AVG << R_DC_AVG << R_AX_AVG << R_SE_AVG;
    (*report) << CLine() << CField(tr("瞳孔大小(PS)：")) << PS_R;    // "PS: "
    (*report) << CLine() << CField(tr("眼位：")) << EYE_POSI_R;    // "Eye Position: "
    (*report) << CLine();

    (*report) << CLine() << CField("OS/" + tr("左眼"));   // "Left eye"
    (*report) << CLine() << CField("    ")
              << CField(tr("球镜(DS)"), W_COL_DS, Qt::AlignCenter)
              << CField(tr("柱镜(DC)"), W_COL_DC, Qt::AlignCenter)
              << CField(tr("轴位(AX)"), W_COL_AX, Qt::AlignCenter)
              << CField(tr("等效球镜(SE)"), W_COL_SE, Qt::AlignCenter);   // "DS", "DC", "AX", "SE"
    if (is_multi) {
        if (m_hasLeft_1) {
            (*report) << CLine() << CField("    ") << L_DS_1 << L_DC_1 << L_AX_1 << L_SE_1;
        }
        if (m_hasLeft_2) {
            (*report) << CLine() << CField("    ") << L_DS_2 << L_DC_2 << L_AX_2 << L_SE_2;
        }
        if (m_hasLeft_3) {
            (*report) << CLine() << CField("    ") << L_DS_3 << L_DC_3 << L_AX_3 << L_SE_3;
        }
    }
    (*report) << CLine() << CField(is_multi ? "AVG " : "    ") << L_DS_AVG << L_DC_AVG << L_AX_AVG << L_SE_AVG;
    (*report) << CLine() << CField(tr("瞳孔大小(PS)：")) << PS_L;    // "PS: "
    (*report) << CLine() << CField(tr("眼位：")) << EYE_POSI_L;    // "Eye Position: "
    (*report) << CLine();

    (*report) << CLine() << CField(tr("瞳距(PD)：")) << PD;    // "PD: "
    if (m_hasEstimatedVision) {
        (*report) << CLine() << CField(QString("-").repeated(W_LINE));
        (*report) << CLine() << CField(tr("*参考视力：")) << VISUAL;   // "*Estimated Visual Acuity: "
        (*report) << CLine() << CField(tr("*参考视力仅根据屈光数据推算，不构成主观验光依据")); // "*Estimated from refraction only, not from actual testing"
    }
    (*report) << CLine() << CField(QString("-").repeated(W_LINE));
    (*report) << CLine() << CField(tr("检查结果：")) << JUDGEMENT;   // "Results: "
    (*report) << CLine() << ORGANIZATION << OPERATOR;
    (*report) << CLine() << CField(tr("检查时间：")) << EXAM_TIME;   // "Examination Time: "
    (*report) << CLine() << CField(tr("*****数据仅供参考，请联系专业人员解读*****"), -1, Qt::AlignCenter);  // "*Please contact a professional to interpret data"
    (*report) << CLine();
    (*report) << CLine();
    (*report) << CLine();

    //
    return true;
}
