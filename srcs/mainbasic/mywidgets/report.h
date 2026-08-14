#ifndef CREPORT_H
#define CREPORT_H

#include <QPixmap>

#include "data.h"

// 报表配置文件路径                 // TODO: 配置目录，统一到 CGlobal::pathConfig() 目录下
#define REPORT_CONFIG_DIR           "/media/report-cfg"
// A4报告机构LOGO图像文件名
#define REPORT_ORG_LOGO_IMG_FILE_NAME   "logo.png"
// A4报告机构二维码图像文件名
#define REPORT_QR_CODE_IMG_FILE_NAME    "qr-code.png"
// A4报告诊治建议文本文件名
#define REPORT_SUGGESTION_FILE_NAME     "suggestion.txt"

// 报告数据
struct stReportData {
    QString patientid;
    QString patientname;
    QString patientsex;
    QString patientstugrade;        // 年级/籍贯
    QString patientstuclass;        // 班级（筛查记录才有班级）
    QString patientdate;
    QString patientagerange;
    QString patienttesttime;

    enSingleDualEyeMode singleDualEyeMode;      // 单双眼模式

    QString patientrighteyeax;
    QString patientlefteyeax;

    QString patientrightpd;
    QString patientleftpd;

    QString patientpd;

    QString patientrightse;
    QString patientleftse;

    QString patientrighteyesph;
    QString patientlefteyesph;

    QString patientrighteyecyl;
    QString patientlefteyecyl;

    QString patientrighths;         //右眼斜视 horizen
    QString patientrightvs;         //右眼斜视 vertical

    QString patientlefths;          //左眼斜视 horizen
    QString patientleftvs;          //左眼斜视 vertical

    QString judgementRight;         // 右眼视力判断描述
    QString judgementLeft;          // 左眼视力判断描述
    QString judgementBoth;          // 双眼视力判断描述

    double astigmatismR;            // 右眼 散光诊断参数（报表中的条形数值分三段，这里定最大值为3，每段对应1）
    double myopiaR;                 // 右眼 近视诊断参数
    double hyperopiaR;              // 右眼 远视诊断参数

    double astigmatismL;            // 左眼 散光诊断参数（报表中的条形图百分比）
    double myopiaL;                 // 左眼 近视诊断参数
    double hyperopiaL;              // 左眼 远视诊断参数

    bool isGazeR;                   // 右眼 是否斜视
    bool isPtosisR;                 // 右眼 是否上睑下垂

    bool isGazeL;                   // 左眼 是否斜视
    bool isPtosisL;                 // 左眼 是否上睑下垂

    bool isAnisometropic;           // 是否屈光参差

    QString imgPath;                // 预览图像文件路径
    QString destPath;               // 报告文件的保存路径

    bool isBatch;                   // 是否批量筛查记录

};

// 报告单生成功能的封装
class CReport : public QObject
{
    Q_OBJECT
public:
    explicit CReport(QObject *_perent = nullptr);
    ~CReport();

    // 初始化（须在调用其它静态函数前，调用本函数）
    static void init();

    // 设置【是否裁减版】
    static void setIsReducedVersion(bool _is_reduced);

    // 设置【机构名称】
    static void setOrganizationName(const QString &_org_name);

    // 设置【操作者姓名】
    static void setOperatorName(const QString &_operator_name);

    // 获取【机构 Logo】图像
    static const QPixmap &getOrganizationLogo(bool _is_reload = false);

    // 处理【机构 Logo】图像
    static void processOrganizationLogo(QPixmap &_img);

    // 获取【二维码】图像
    static const QPixmap &getQrCodeImg(bool _is_reload = false);

    // 处理【二维码】图像
    static void processQrCodeImg(QPixmap &_img);

    // 生成 PDF 报告
    static void generatePdfReport(stReportData _data);

    // 载入诊治建议文本
    static void loadSuggestion(const QString &_file_path, QString &_suggestion);

    // 保存诊治建议文本
    static bool saveSuggestion(const QString &_suggestion, QString &_msg_err);

    // 获取诊治建议文本（每行是不包含换行符的）
    static QString &getSuggestion(bool _is_reload = false);

protected:
    static bool isReducedVersion;

    static QString organizationName;
    static QString operatorName;

    static QPixmap *organizationLogo;       // 组织机构图标（不应直接访问，应通过 get 函数访问）
    static QPixmap *qrCodeImg;              // 二维码图像（不应直接访问，应通过 get 函数访问）

    static QString suggestion;              // 诊治建议文本（不应直接访问，应通过 get 函数访问）

    static QPixmap *imgResultBg;
    static QPixmap *imgSubject;

    //static void drawTextAutoWrap(QPainter &_painter, int _x, int _y, QString _text, int _row_height);     /* 这函数没意义？ */

};

#endif // CREPORT_H
