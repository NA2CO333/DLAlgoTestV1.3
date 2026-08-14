#ifndef CSCREENERREPORTRECEIPT_H
#define CSCREENERREPORTRECEIPT_H

#include <QObject>

#include "report-text.h"
#include "data.h"

// 视筛小票报表
class CScreenerReportReceipt : public Common::CReportText
{
    Q_OBJECT
public:
    explicit CScreenerReportReceipt(QObject *_parent = nullptr);

    /**
     * @brief setData
     * @param _pat
     * @param _judgement_desc
     * @param _has_estimated    是否有参考视力
     * @param _is_cyl_negative  是否负散光
     */
    void setData(const CPatient &_pat, const QString &_judgement_desc, enSingleDualEyeMode _single_dual_eye,
                 bool _has_estimated, bool _is_cyl_negative);

    void updateData() override;             // 更新报表数据

protected:
    bool buildReportLayout(QString &_err_msg) override;     // 构造报表布局

    CPatient m_pat;
    QString m_judgementDescStr;             // 视力判断描述
    enSingleDualEyeMode m_singleDualEye;    // 单双眼模式
    bool m_hasEstimatedVision;              // 是否有参考视力
    bool m_isCylNegative;                   // 是否负散光

    bool m_hasRight_1 = false;              // 右眼 是否有“多次测量”的记录 1
    bool m_hasLeft_1  = false;

    bool m_hasRight_2 = false;              // 右眼 是否有“多次测量”的记录 2
    bool m_hasLeft_2  = false;

    bool m_hasRight_3 = false;
    bool m_hasLeft_3  = false;
    //
    static constexpr int W_LINE         = 48;       // 列的总宽度    /* 英文字母宽度为1，中文字符宽度为2 */

    static constexpr int W_COL_NAME_    =  8;       // 字段宽度：姓名标题
    static constexpr int W_COL_NAME     = 19;       // 字段宽度：姓名
    static constexpr int W_COL_BIRTH_   = 10;       // 字段宽度：生日标题
    static constexpr int W_COL_BIRTH    = 10;       // 字段宽度：生日

    static constexpr int W_COL_NUM_     =  8;       // 字段宽度：编号标题
    static constexpr int W_COL_NUM      = 19;       // 字段宽度：编号
    static constexpr int W_COL_GENDER_  = -1;       // 字段宽度：性别标题
    static constexpr int W_COL_GENDER   = -1;       // 字段宽度：性别

    static constexpr int W_COL_DS       = 11;       // 字段宽度：DS
    static constexpr int W_COL_DC       = 11;       // 字段宽度：DC
    static constexpr int W_COL_AX       = 10;       // 字段宽度：AX
    static constexpr int W_COL_SE       = 12;       // 字段宽度：SE

    //
    Common::CReportTextField NAME           {"", W_COL_NAME     , Qt::AlignLeft};
    Common::CReportTextField BIRTHDATE      {"", W_COL_BIRTH    , Qt::AlignLeft};
    Common::CReportTextField NUMBER         {"", W_COL_NUM      , Qt::AlignLeft};
    Common::CReportTextField GENDER         {"", W_COL_GENDER   , Qt::AlignLeft};

    Common::CReportTextField R_DS_1         {"", W_COL_DS       , Qt::AlignCenter};
    Common::CReportTextField R_DC_1         {"", W_COL_DC       , Qt::AlignCenter};
    Common::CReportTextField R_AX_1         {"", W_COL_AX       , Qt::AlignCenter};
    Common::CReportTextField R_SE_1         {"", W_COL_SE       , Qt::AlignCenter};

    Common::CReportTextField R_DS_2         {"", W_COL_DS       , Qt::AlignCenter};
    Common::CReportTextField R_DC_2         {"", W_COL_DC       , Qt::AlignCenter};
    Common::CReportTextField R_AX_2         {"", W_COL_AX       , Qt::AlignCenter};
    Common::CReportTextField R_SE_2         {"", W_COL_SE       , Qt::AlignCenter};

    Common::CReportTextField R_DS_3         {"", W_COL_DS       , Qt::AlignCenter};
    Common::CReportTextField R_DC_3         {"", W_COL_DC       , Qt::AlignCenter};
    Common::CReportTextField R_AX_3         {"", W_COL_AX       , Qt::AlignCenter};
    Common::CReportTextField R_SE_3         {"", W_COL_SE       , Qt::AlignCenter};

    Common::CReportTextField R_DS_AVG       {"", W_COL_DS       , Qt::AlignCenter};
    Common::CReportTextField R_DC_AVG       {"", W_COL_DC       , Qt::AlignCenter};
    Common::CReportTextField R_AX_AVG       {"", W_COL_AX       , Qt::AlignCenter};
    Common::CReportTextField R_SE_AVG       {"", W_COL_SE       , Qt::AlignCenter};

    Common::CReportTextField L_DS_1         {"", W_COL_DS       , Qt::AlignCenter};
    Common::CReportTextField L_DC_1         {"", W_COL_DC       , Qt::AlignCenter};
    Common::CReportTextField L_AX_1         {"", W_COL_AX       , Qt::AlignCenter};
    Common::CReportTextField L_SE_1         {"", W_COL_SE       , Qt::AlignCenter};

    Common::CReportTextField L_DS_2         {"", W_COL_DS       , Qt::AlignCenter};
    Common::CReportTextField L_DC_2         {"", W_COL_DC       , Qt::AlignCenter};
    Common::CReportTextField L_AX_2         {"", W_COL_AX       , Qt::AlignCenter};
    Common::CReportTextField L_SE_2         {"", W_COL_SE       , Qt::AlignCenter};

    Common::CReportTextField L_DS_3         {"", W_COL_DS       , Qt::AlignCenter};
    Common::CReportTextField L_DC_3         {"", W_COL_DC       , Qt::AlignCenter};
    Common::CReportTextField L_AX_3         {"", W_COL_AX       , Qt::AlignCenter};
    Common::CReportTextField L_SE_3         {"", W_COL_SE       , Qt::AlignCenter};

    Common::CReportTextField L_DS_AVG       {"", W_COL_DS       , Qt::AlignCenter};
    Common::CReportTextField L_DC_AVG       {"", W_COL_DC       , Qt::AlignCenter};
    Common::CReportTextField L_AX_AVG       {"", W_COL_AX       , Qt::AlignCenter};
    Common::CReportTextField L_SE_AVG       {"", W_COL_SE       , Qt::AlignCenter};

    Common::CReportTextField PS_R           {"", -1, Qt::AlignLeft};
    Common::CReportTextField PS_L           {"", -1, Qt::AlignLeft};

    Common::CReportTextField EYE_POSI_R     {"", -1, Qt::AlignLeft};
    Common::CReportTextField EYE_POSI_L     {"", -1, Qt::AlignLeft};

    Common::CReportTextField PD             {"", -1, Qt::AlignLeft};

    Common::CReportTextField VISUAL         {"", -1, Qt::AlignLeft};   // 估算视力

    Common::CReportTextField JUDGEMENT      {"", -1, Qt::AlignLeft};

    Common::CReportTextField ORGANIZATION   {"", -1, Qt::AlignLeft};
    Common::CReportTextField OPERATOR       {"", -1, Qt::AlignRight};

    Common::CReportTextField EXAM_TIME      {"", -1, Qt::AlignLeft};   // 检查时间

};

#endif // CSCREENERREPORTRECEIPT_H
