#include "report.h"

#include <cmath>

#include <QDir>
#include <QFile>
#include <QFont>
#include <QRect>
#include <QPdfWriter>
#include <QPainter>
#include <QDebug>
#include <QApplication>

#include "globaltypes.h"
#include "util-common.h"
#include "algo-invoker.h"

//
static constexpr int DPI                = 300;          // 分辨率 DPI，指 "Dots Per Inch"，A4 纸张的点尺寸 = (8.27 inch * 11.69 inch) * DPI = 2481 * 3507

static constexpr int MAX_LOGO_WIDTH     = 690;          // logo 最大宽度（单位：像素，不是点）
static constexpr int MAX_LOGO_HEIGHT    = 250;          // logo 最大高度（单位：像素，不是点）

static constexpr int QR_CODE_WIDTH      = 250;          // 二维码边长（单位：像素，不是点）

static const int PAGE_LEFT      = 80;       // 页 左 边沿坐标（单位为点，2481 * 3507，见下面的 DPI 设置）
static const int PAGE_RIGHT     = 2401;     // 页 右 边沿坐标
static const int PAGE_TOP       = 100;      // 页 上 边沿坐标
static const int PAGE_BOTTOM    = 3407;     // 页 下 边沿坐标

/// ============================================================================================================
/// 其它函数
//TODO: 移到公用单元？

// 等宽高比缩放宽度和高度
void scaleSizeTo(int &_width_from, int &_height_from, const int _width_to, const int _height_to)
{
    double ratio_w_h_from = (double)_width_from / _height_from;
    double ratio_w_h_to = (double)_width_to / _height_to;
    if (ratio_w_h_from > ratio_w_h_to) {
        _width_from = _width_to;
        _height_from = _width_from / ratio_w_h_from;
    } else {
        _height_from = _height_to;
        _width_from = _height_from * ratio_w_h_from;
    }
}

/// ============================================================================================================
/// class CReport

//
bool CReport::isReducedVersion = false;

QString CReport::organizationName = "";
QString CReport::operatorName = "";

QPixmap *CReport::organizationLogo = Q_NULLPTR;
QPixmap *CReport::qrCodeImg = Q_NULLPTR;
QPixmap *CReport::imgResultBg = Q_NULLPTR;
QPixmap *CReport::imgSubject = Q_NULLPTR;

QString CReport::suggestion;

//
CReport::CReport(QObject *_perent) : QObject(_perent)
{

}

CReport::~CReport()
{

}

void CReport::init()
{
    static bool is_inited = false;
    if (is_inited) {
        return;
    }

    //
    organizationLogo = new QPixmap;

    qrCodeImg = new QPixmap;

    imgResultBg = new QPixmap;
    imgResultBg->load(":/resource/reportResultBg.png");

    imgSubject = new QPixmap;

    //
    is_inited = true;
}

// 设置【是否裁减版】
void CReport::setIsReducedVersion(bool _is_reduced)
{
    CReport::isReducedVersion = _is_reduced;
}

// 设置【机构名称】
void CReport::setOrganizationName(const QString &_org_name)
{
    organizationName = _org_name;
}

void CReport::setOperatorName(const QString &_operator_name)
{
    operatorName = _operator_name;
}

// 获取【机构 Logo】图像
const QPixmap &CReport::getOrganizationLogo(bool _is_reload)
{
    static bool is_loaded = false;

    //
    if (!is_loaded || _is_reload) {
        QString img_logo_path = QString(REPORT_CONFIG_DIR) + QDir::separator() + REPORT_ORG_LOGO_IMG_FILE_NAME;
        if (!QFile::exists(img_logo_path)) {
            img_logo_path = ":/resource/reportDefaultLogo.png";
        }
        organizationLogo->load(img_logo_path);

        //
        is_loaded = true;
    }

    //
    return *organizationLogo;
}

void CReport::processOrganizationLogo(QPixmap &_img)
{
    // 若长或宽超出最大尺寸，则缩小
    if (_img.width() > MAX_LOGO_WIDTH || _img.height() > MAX_LOGO_HEIGHT) {
        int width = _img.width(), height = _img.height();
        scaleSizeTo(width, height, MAX_LOGO_WIDTH, MAX_LOGO_HEIGHT);
        QPixmap img = _img;
        _img = img.scaledToWidth(width);    // TODO: 这样写，内存的释放有没问题？
    }
}

// 获取【二维码】图像
const QPixmap &CReport::getQrCodeImg(bool _is_reload)
{
    static bool is_loaded = false;

    //
    if (!is_loaded || _is_reload) {
        QString img_qrcode_path = QString(REPORT_CONFIG_DIR) + QDir::separator() + REPORT_QR_CODE_IMG_FILE_NAME;
        if (!QFile::exists(img_qrcode_path)) {
            img_qrcode_path = ":/resource/reportDefaultQrCode.png";
        }
        qrCodeImg->load(img_qrcode_path);

        //
        is_loaded = true;
    }

    //
    return *qrCodeImg;
}

void CReport::processQrCodeImg(QPixmap &_img)
{
    // 缩放为固定尺寸
    if (_img.width() > QR_CODE_WIDTH || _img.height() > QR_CODE_WIDTH) {
        QPixmap img = _img;
        _img = img.scaled(QR_CODE_WIDTH, QR_CODE_WIDTH);    // TODO: 这样写，内存的释放有没问题？
    }
}

//
//void outputText(QPainter _painter, int _x, int _y, QString _text)
//{
//
//}

// 生成 PDF 报告
void CReport::generatePdfReport(stReportData _data)
{
    static constexpr QColor COLOR_BLACK(68, 68, 68);
    static constexpr QColor COLOR_DARK_GRAY(102, 102, 102);
    static constexpr QColor COLOR_GRAY(160, 160, 160);
    static constexpr QColor COLOR_LIGHT_GRAY(240, 240, 240);

    static constexpr int FONT_SIZE_DEFAULT  = 36;       // 默认字体尺寸（DPI）

    static const QString UDLR = QStringLiteral("↑↓←→");

    //
    QFont font_default;
    QFont font_bold = font_default;

    font_default.setFamily("Noto Sans SC Regular");

    font_bold.setFamily("Noto Sans SC Black");
    font_bold.setBold(true);

    //
    int idx_last_sep = _data.destPath.lastIndexOf(QDir::separator());
    QString dir_path = _data.destPath.mid(0, idx_last_sep);

    QDir dir(dir_path);
    if (!dir.exists()) {
        dir.mkpath(dir_path);
    }

    //
    QFile file_pdf(_data.destPath);
    file_pdf.open(QIODevice::WriteOnly | QIODevice::Truncate);

    QPdfWriter writer(&file_pdf);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));

    writer.setResolution(DPI);           // 设置纸张的分辨率

    //qDebug() << "physicalDpi: " << writer.physicalDpiX() << ", " << writer.physicalDpiY();
    //qDebug() << "logicalDpi:  " << writer.logicalDpiX() << ", " << writer.logicalDpiY();

    double font_ratio = 0.25F;      // TODO: 这个字体尺寸与 DPI 的关系是怎样的？

    //
    bool is_has_right  = (singleDualEyeMode_Right & _data.singleDualEyeMode);
    bool is_has_left   = (singleDualEyeMode_Left & _data.singleDualEyeMode);

    //
    QPainter painter;
    painter.begin(&writer);     /* 此绘图设备的尺寸单位并不是像素，而是上面设置的分辨率对应的点 */

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setAlignment(Qt::AlignCenter);

    // logo 图像
    {
        const QRect RECT_LOGO(84, PAGE_TOP, 628, 196);    // logo 区域限制
        const QPixmap &img = getOrganizationLogo();
        int x = RECT_LOGO.x();
        int y = RECT_LOGO.y() + (RECT_LOGO.height() - img.height()) / 2;
        painter.drawPixmap(QRect(x, y, img.width(), img.height()), img);
    }

    // 二维码 图像
    {
        const QRect RECT_QRCODE(1933, 63, 438, 260);    // 二维码区域限制
        const QPixmap &img = getQrCodeImg();
        int x = RECT_QRCODE.x() + (RECT_QRCODE.width() - img.width()) / 2;
        int y = RECT_QRCODE.y() + (RECT_QRCODE.height() - img.height()) / 2;
        painter.drawPixmap(QRect(x, y, img.width(), img.height()), img);
    }

    // 机构名称
    {
        font_bold.setPointSize(std::round(70 * font_ratio));        // TODO: 字体、样式都放到前面统一定义？
        painter.setFont(font_bold);
        painter.setPen(COLOR_BLACK);
        QRect rect_org_name = painter.fontMetrics().boundingRect(CReport::organizationName);
        painter.drawText(((PAGE_RIGHT - PAGE_LEFT) - rect_org_name.width()) / 2, PAGE_TOP + 20, CReport::organizationName);
    }

    // 标题
    {
        font_bold.setPointSize(std::round(65 * font_ratio));
        painter.setFont(font_bold);
        painter.setPen(COLOR_DARK_GRAY);
        QString report_title = tr("视力筛查仪报告单");
        QRect rect_report_title = painter.fontMetrics().boundingRect(report_title);
        painter.drawText(((PAGE_RIGHT - PAGE_LEFT) - rect_report_title.width()) / 2, 200, report_title);
    }

    //
    static constexpr int INFO_TOP           = 448;          // 基本信息区域的 top 坐标
    static constexpr int INFO_ROW_INTERVAL  = 88;

    // 基本信息边框
    {
        painter.setPen(QPen(QBrush(COLOR_LIGHT_GRAY), 6));
        painter.setBrush(Qt::NoBrush);
        int left = 85, top = 371, right = 2419, bottom = 648;
        painter.drawRect(QRect(left, top, right - left, bottom - top));
    }

    // 姓名
    {
        painter.setPen(COLOR_DARK_GRAY);
        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);
        painter.drawText(118, INFO_TOP + INFO_ROW_INTERVAL * 0, tr("姓名："));     // "Name: "

        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setPen(COLOR_BLACK);
        painter.drawText(320, INFO_TOP + INFO_ROW_INTERVAL * 0, _data.patientname);
    }

    // 编号
    {
        painter.setPen(COLOR_DARK_GRAY);
        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);
        painter.drawText(118, INFO_TOP + INFO_ROW_INTERVAL * 1, tr("编号："));     // "No.: "

        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setPen(COLOR_BLACK);
        painter.drawText(320, INFO_TOP + INFO_ROW_INTERVAL * 1, _data.patientid);
    }

    // 测量时间
    {
        painter.setPen(COLOR_DARK_GRAY);
        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);
        painter.drawText(118, INFO_TOP + INFO_ROW_INTERVAL * 2, tr("检查时间："));   // "MeasureTime: "

        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setPen(COLOR_BLACK);
        painter.drawText(480, INFO_TOP + INFO_ROW_INTERVAL * 2, _data.patienttesttime);
    }

    // 性别
    {
        painter.setPen(COLOR_DARK_GRAY);
        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);
        painter.drawText(873, INFO_TOP + INFO_ROW_INTERVAL * 0, tr("性别："));     // "Sex: "

        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setPen(COLOR_BLACK);
        painter.drawText(1120, INFO_TOP + INFO_ROW_INTERVAL * 0, _data.patientsex);
    }

    // 年级/籍贯
    {
        painter.setPen(COLOR_DARK_GRAY);
        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);
        painter.drawText(873, INFO_TOP + INFO_ROW_INTERVAL * 1, (_data.isBatch ? tr("年级：") : tr("籍贯："))); // "Grade: "

        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setPen(COLOR_BLACK);
        painter.drawText(1120, INFO_TOP + INFO_ROW_INTERVAL * 1, (_data.isBatch ? _data.patientstugrade : _data.patientstuclass));
    }

    // 出生日期
    {
        painter.setPen(COLOR_DARK_GRAY);
        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);
        painter.drawText(1619, INFO_TOP + INFO_ROW_INTERVAL * 0, tr("出生日期："));  // "BirthDate: "

        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setPen(COLOR_BLACK);
        painter.drawText(1856, INFO_TOP + INFO_ROW_INTERVAL * 0, _data.patientdate);
    }

    // 班级（筛查记录才有班级）
    if (_data.isBatch)
    {
        painter.setPen(COLOR_DARK_GRAY);
        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);
        painter.drawText(1619, INFO_TOP + INFO_ROW_INTERVAL * 1, tr("班级："));    // "Class: "

        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setPen(COLOR_BLACK);
        painter.drawText(1856, INFO_TOP + INFO_ROW_INTERVAL * 1, _data.patientstuclass);
    }

    // 测量结果的背景图
    {
        QPoint point_result_bg(84, 740);
        painter.drawPixmap(QRect(point_result_bg.x(), point_result_bg.y(), imgResultBg->width(), imgResultBg->height()), *imgResultBg);
    }

    // 瞳孔直径
    {
        font_default.setPointSize(std::round(28 * font_ratio));
        painter.setFont(font_default);
        painter.setPen(COLOR_BLACK);
        if (is_has_right) {
            painter.drawText(122,  805, _data.patientrightpd);
        }
        if (is_has_left) {
            painter.drawText(1237, 805, _data.patientleftpd);
        }
    }

    // 瞳距
    {
        if (is_has_right && is_has_left) {
            painter.drawText(675, 823, _data.patientpd);
        }
    }

    // 凝视
    {
        if (is_has_right) {
            int gaze_r_v = _data.patientrightvs.toInt();
            int gaze_r_h = _data.patientrighths.toInt();
            painter.drawText(112,  900, (gaze_r_v > 0 ? UDLR[0] : UDLR[1]) + QString::number(std::abs(gaze_r_v)));
            painter.drawText(112,  954, (gaze_r_h > 0 ? UDLR[2] : UDLR[3]) + QString::number(std::abs(gaze_r_h)));
        }

        if (is_has_left) {
            int gaze_l_v = _data.patientleftvs.toInt();
            int gaze_l_h = _data.patientlefths.toInt();
            painter.drawText(1232, 900, (gaze_l_v > 0 ? UDLR[0] : UDLR[1]) + QString::number(std::abs(gaze_l_v)));
            painter.drawText(1232, 954, (gaze_l_h > 0 ? UDLR[2] : UDLR[3]) + QString::number(std::abs(gaze_l_h)));
        }
    }

    // 屈光度数
    {
        // 右眼
        if (is_has_right) {
            painter.drawText(382,  1083, _data.patientrightse);
            painter.drawText(250,  1204, _data.patientrighteyesph);
            painter.drawText(383,  1204, _data.patientrighteyecyl);
            painter.drawText(522,  1204, _data.patientrighteyeax);
        }

        // 左眼
        if (is_has_right) {
            painter.drawText(935,  1083, _data.patientleftse);
            painter.drawText(801,  1204, _data.patientlefteyesph);
            painter.drawText(936,  1204, _data.patientlefteyecyl);
            painter.drawText(1084, 1204, _data.patientlefteyeax);
        }
    }

    // 被测者图像
    {
        QRect rect_img_subject(1377, 756, 1020, 483);

        *imgSubject = CAlgoInvoker::readAndEqualizeHistFromFileToPixmap(_data.imgPath);
        if (!imgSubject->isNull()) {
            //qDebug() << "image pixel ratio = " << imgSubject.devicePixelRatio();

            painter.drawPixmap(rect_img_subject, *imgSubject);
        } else {
            painter.setPen(COLOR_GRAY);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(rect_img_subject);
        }
    }

    // 结果判断描述
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(COLOR_GRAY);
        const int LEFT = 82, TOP = 1298, RIGHT = 2412, BOTTOM = 1464;
        painter.drawRect(LEFT, TOP, RIGHT - LEFT, BOTTOM - TOP);

        painter.setPen(Qt::white);
        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_bold);
        painter.drawText(120, TOP + 60 * 1, tr("结果"));  // "Judging"

        const int LEFT_JUDGEMENT = 540;
        QRectF rect;
        option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        if (is_has_right) {
            rect = QRectF(LEFT_JUDGEMENT,  TOP, (RIGHT - LEFT_JUDGEMENT) / 2, (BOTTOM - TOP) / 2);
            //painter.setBrush(Qt::red);
            //painter.drawRect(rect);
            painter.drawText(rect, tr("右眼：") + _data.judgementRight, option); // "RightEye: "
        }
        if (is_has_left) {
            rect = QRectF((RIGHT + LEFT_JUDGEMENT) / 2, TOP, (RIGHT - LEFT_JUDGEMENT) / 2, (BOTTOM - TOP) / 2);
            //painter.setBrush(Qt::blue);
            //painter.drawRect(rect);
            painter.drawText(rect, tr("左眼：") + _data.judgementLeft, option);  // "LeftEye: "
        }
        if (is_has_right && is_has_left) {
            rect = QRectF(LEFT_JUDGEMENT, (BOTTOM + TOP) / 2, RIGHT - LEFT_JUDGEMENT, (BOTTOM - TOP) / 2);
            //painter.setBrush(Qt::yellow);
            //painter.drawRect(rect);
            painter.drawText(rect, _data.judgementBoth, option);
        }
    }

    // 数据准确性声明
    {
        painter.setPen(COLOR_DARK_GRAY);
        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);
        QString text = tr("由于视力筛查检查的是人眼睛瞬时的动态屈光值，眼睛状况可能随着时间发生变化，所以应定期进行视力筛查。");
                                  // "Due to the fact that vision screening measures the instantaneous dynamic diopter of the eye, the eye condition may \n change over time, so regular vision screening should be conducted."
        //drawTextAutoWrap(painter, 254, 1540, text, 70);
        option.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        QRectF rect =  QRectF(PAGE_LEFT, 1500, PAGE_RIGHT - PAGE_LEFT, 140);
        //painter.drawRect(rect);
        painter.drawText(rect, text, option);
    }

    // 视力判断图表(表头和文字标注)
    {
        option.setAlignment(Qt::AlignCenter);

        painter.setPen(Qt::NoPen);
        painter.setBrush(COLOR_GRAY);
        const int RECT_LEFT = 82, RECT_TOP = 1658, RECT_RIGHT = 2412, RECT_BOTTOM = 1731;
        painter.drawRect(RECT_LEFT, RECT_TOP, RECT_RIGHT - RECT_LEFT, RECT_BOTTOM - RECT_TOP);

        font_bold.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_bold);
        painter.setPen(COLOR_DARK_GRAY);
        painter.drawText(QRectF(RECT_LEFT,  RECT_TOP, (RECT_RIGHT - RECT_LEFT) / 2, RECT_BOTTOM - RECT_TOP), tr("右眼"), option);    // "Right Eye"
        painter.drawText(QRectF((RECT_RIGHT + RECT_LEFT) / 2,  RECT_TOP, (RECT_RIGHT - RECT_LEFT) / 2, RECT_BOTTOM - RECT_TOP), tr("左眼"), option);    // "Left Eye"

        font_default.setPointSize(std::round(FONT_SIZE_DEFAULT * font_ratio));
        painter.setFont(font_default);

        const int ROW_HEIGHT = 94;
        const int CELL_WIDTH = 318;
        const int X_RIGHT = 83, X_LEFT = 1458;
        QRectF rect;

        rect = QRectF(X_RIGHT + CELL_WIDTH * 0,  RECT_BOTTOM - 20, CELL_WIDTH, ROW_HEIGHT + 50);
        //painter.drawRect(rect);
        painter.drawText(rect, tr("建议眼科诊治"), option);   // "Out of Range"
        rect = QRectF(X_RIGHT + CELL_WIDTH * 1,  RECT_BOTTOM - 20, CELL_WIDTH, ROW_HEIGHT + 50);
        //painter.drawRect(rect);
        painter.drawText(rect, tr("定期随访"), option);     // "Follow-up"
        rect = QRectF(X_RIGHT + CELL_WIDTH * 2,  RECT_BOTTOM - 20, CELL_WIDTH, ROW_HEIGHT + 50);
        //painter.drawRect(rect);
        painter.drawText(rect, tr("正常范围内"), option);    // "In Range"

        rect = QRectF(X_LEFT + CELL_WIDTH * 0, RECT_BOTTOM - 20, CELL_WIDTH, ROW_HEIGHT + 50);
        //painter.drawRect(rect);
        painter.drawText(rect, tr("正常范围内"), option);   // "In Range"
        rect = QRectF(X_LEFT + CELL_WIDTH * 1, RECT_BOTTOM - 20, CELL_WIDTH, ROW_HEIGHT + 50);
        //painter.drawRect(rect);
        painter.drawText(rect, tr("定期随访"), option);    //  "Follow-up"
        rect = QRectF(X_LEFT + CELL_WIDTH * 2, RECT_BOTTOM - 20, CELL_WIDTH, ROW_HEIGHT + 50);
        //painter.drawRect(rect);
        painter.drawText(rect, tr("建议眼科诊治"), option);  // "Out of Range"
    }

    // 视力判断图表（图表部分）
    {
        const int ROW_HEIGHT = 94;
        const int CELL_WIDTH = 318;
        const int X_RIGHT = 83, X_LEFT = 1458, TABLE_TOP = 1851;      // 右眼、左眼条形图背景的 x 起点及 y 起点

        painter.setPen(QPen(QBrush(COLOR_DARK_GRAY), 4));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(X_RIGHT, TABLE_TOP, CELL_WIDTH * 3, ROW_HEIGHT * 6 - 4);
        painter.drawRect(X_LEFT,  TABLE_TOP, CELL_WIDTH * 3, ROW_HEIGHT * 6 - 4);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(238, 128, 127));
        painter.drawRect(X_RIGHT + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 0, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 0, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 1, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 1, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 2, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 2, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 3, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 3, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 4, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 4, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 5, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 5, CELL_WIDTH, ROW_HEIGHT - 4);

        painter.setBrush(QColor(255, 228, 225));
        painter.drawRect(X_RIGHT + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 0, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 0, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 1, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 1, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 2, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 2, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 3, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 3, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 4, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 4, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 5, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 1, TABLE_TOP + ROW_HEIGHT * 5, CELL_WIDTH, ROW_HEIGHT - 4);

        painter.setBrush(QColor(255, 255, 255));
        painter.drawRect(X_RIGHT + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 0, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 0, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 1, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 1, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 2, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 2, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 3, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 3, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 4, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 4, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_RIGHT + CELL_WIDTH * 2, TABLE_TOP + ROW_HEIGHT * 5, CELL_WIDTH, ROW_HEIGHT - 4);
        painter.drawRect(X_LEFT  + CELL_WIDTH * 0, TABLE_TOP + ROW_HEIGHT * 5, CELL_WIDTH, ROW_HEIGHT - 4);

        // 条形数据
        const int h = ROW_HEIGHT * 0.4;
        painter.setBrush(QColor(20, 93, 133));

        if (is_has_right) {
            painter.drawRect(X_RIGHT + CELL_WIDTH * (3 - _data.astigmatismR), TABLE_TOP + 26 + ROW_HEIGHT * 0, CELL_WIDTH * _data.astigmatismR, h);
            painter.drawRect(X_RIGHT + CELL_WIDTH * (3 - _data.myopiaR),      TABLE_TOP + 26 + ROW_HEIGHT * 1, CELL_WIDTH * _data.myopiaR,      h);
            painter.drawRect(X_RIGHT + CELL_WIDTH * (3 - _data.hyperopiaR),   TABLE_TOP + 26 + ROW_HEIGHT * 2, CELL_WIDTH * _data.hyperopiaR,   h);

            painter.drawRect(X_RIGHT + CELL_WIDTH * (3 - (_data.isGazeR ? 1.5 : 0)),          TABLE_TOP + 26 + ROW_HEIGHT * 3, CELL_WIDTH * (_data.isGazeR ? 1.5 : 0), h);
            painter.drawRect(X_RIGHT + CELL_WIDTH * (3 - (_data.isAnisometropic ? 1.5 : 0)), TABLE_TOP + 26 + ROW_HEIGHT * 4, CELL_WIDTH * (_data.isAnisometropic ? 1.5 : 0), h);
            painter.drawRect(X_RIGHT + CELL_WIDTH * (3 - (_data.isPtosisR ? 1.5 : 0)),        TABLE_TOP + 26 + ROW_HEIGHT * 5, CELL_WIDTH * (_data.isPtosisR ? 1.5 : 0), h);
        }

        if (is_has_left) {
            painter.drawRect(X_LEFT, TABLE_TOP + 26 + ROW_HEIGHT * 0, CELL_WIDTH * _data.astigmatismL, h);
            painter.drawRect(X_LEFT, TABLE_TOP + 26 + ROW_HEIGHT * 1, CELL_WIDTH * _data.myopiaL,      h);
            painter.drawRect(X_LEFT, TABLE_TOP + 26 + ROW_HEIGHT * 2, CELL_WIDTH * _data.hyperopiaL,   h);

            painter.drawRect(X_LEFT, TABLE_TOP + 26 + ROW_HEIGHT * 3, CELL_WIDTH * (_data.isGazeL ? 1.5 : 0), h);
            painter.drawRect(X_LEFT, TABLE_TOP + 26 + ROW_HEIGHT * 4, CELL_WIDTH * (_data.isAnisometropic ? 1.5 : 0), h);
            painter.drawRect(X_LEFT, TABLE_TOP + 26 + ROW_HEIGHT * 5, CELL_WIDTH * (_data.isPtosisL ? 1.5 : 0), h);
        }

        // 条形图中间的文字标题
        const int CAPTION_X = X_RIGHT + CELL_WIDTH * 3;
        const int CAPTION_WIDTH = X_LEFT - CAPTION_X;
        painter.setPen(COLOR_DARK_GRAY);
        option.setAlignment(Qt::AlignCenter);
        //painter.drawRect(QRectF(CAPTION_X, TABLE_TOP + ROW_HEIGHT * 0, CAPTION_WIDTH, ROW_HEIGHT));
        painter.drawText(QRectF(CAPTION_X, TABLE_TOP + ROW_HEIGHT * 0, CAPTION_WIDTH, ROW_HEIGHT), tr("散光"), option);          // "astigmatism"
        painter.drawText(QRectF(CAPTION_X, TABLE_TOP + ROW_HEIGHT * 1, CAPTION_WIDTH, ROW_HEIGHT), tr("近视"), option);          // "myopia"
        painter.drawText(QRectF(CAPTION_X, TABLE_TOP + ROW_HEIGHT * 2, CAPTION_WIDTH, ROW_HEIGHT), tr("远视"), option);          // "hyperopia"
        painter.drawText(QRectF(CAPTION_X, TABLE_TOP + ROW_HEIGHT * 3, CAPTION_WIDTH, ROW_HEIGHT), tr("斜视度"), option);         // "gaze"
        painter.drawText(QRectF(CAPTION_X, TABLE_TOP + ROW_HEIGHT * 4, CAPTION_WIDTH, ROW_HEIGHT), tr("屈光参差"), option);        // "anisometropia"
        painter.drawText(QRectF(CAPTION_X, TABLE_TOP + ROW_HEIGHT * 5, CAPTION_WIDTH, ROW_HEIGHT), tr("上睑下垂"), option);        // "Ptosis"
    }

    // 治疗建议
    {
        painter.setPen(COLOR_BLACK);

        const int TOP = 2550;

        QString &suggestion = getSuggestion();
        option.setAlignment(Qt::AlignLeft | Qt::AlignTop);
        QRectF rect = QRectF(PAGE_LEFT + 8, TOP, PAGE_RIGHT - PAGE_LEFT, PAGE_BOTTOM - TOP);
        //painter.drawRect(rect);
        painter.drawText(rect, suggestion, option);
    }

    // 检查者
    {
        painter.drawText(1600, 3250, tr("操作者："));   // "Operator: "

        painter.drawText(1760, PAGE_BOTTOM - 150, operatorName);
    }

    // 底注
    {
        QString text = tr("筛查结果不能替代专业医生或验光师的全面检查"); // "The screening results cannot replace the comprehensive \n examination by professional doctors or optometrists"
        //drawTextAutoWrap(painter, 1356, 3360, text, 70);
        option.setAlignment(Qt::AlignRight | Qt::AlignTop);
        QRectF rect = QRectF(PAGE_LEFT, 3360, PAGE_RIGHT - PAGE_LEFT, 140);
        //painter.drawRect(rect);
        painter.drawText(rect, text, option);
    }

    //
    painter.end();

    //
    file_pdf.close();

}

void CReport::loadSuggestion(const QString &_file_path, QString &_suggestion)
{
    _suggestion.clear();

    //
    if (!QFile::exists(_file_path)) {
        return;
    }

    //
    bool is_read_succ = Util::readFileToQStr(_file_path, _suggestion, "UTF-8");
    if (!is_read_succ) {
        // TODO:

    }
}

bool CReport::saveSuggestion(const QString &_suggestion, QString &_msg_err)
{
    static const QString FILE_PATH = QString(REPORT_CONFIG_DIR) + QDir::separator() + REPORT_SUGGESTION_FILE_NAME;

    // 保存到文件
    bool is_succ_write = Util::writeQStrToFile(_suggestion, FILE_PATH, "UTF-8");
    if (is_succ_write) {
        return true;
    } else {
        _msg_err = tr("保存建议文本文件失败！");    // "Failed to save suggestion text to file!"
        return false;
    }
}

QString &CReport::getSuggestion(bool _is_reload)
{
    static bool is_loaded = false;
    if (!is_loaded || _is_reload) {
        QString file_path = QString(REPORT_CONFIG_DIR) + QDir::separator() + REPORT_SUGGESTION_FILE_NAME;
        if (!QFile::exists(file_path)) {
            file_path = ":/resource/suggestion.txt";
        }
        loadSuggestion(file_path, suggestion);

        //
        is_loaded = true;
    }

    return suggestion;
}

//void CReport::drawTextAutoWrap(QPainter &_painter, int _x, int _y, QString _text, int _row_height)
//{
//    QStringList str_list;
//    Util::qstrToStringList(_text, str_list);
//    for (int i = 0; i < str_list.size(); i++) {
//        QString text = str_list[i];
//        text.replace("\n", "");
//        _painter.drawText(_x, _y + _row_height * i, text);
//    }
//}
