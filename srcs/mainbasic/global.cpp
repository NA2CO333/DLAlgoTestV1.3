#include "global.h"

#include <QApplication>
#include <QFile>
#include <QDir>

#include "appsetting.h"
#include "windatatrans.h"
#include "log-appender.h"

// =============================================================================================================================================
// 宏定义



// =============================================================================================================================================
// 常量定义

#if (OS_TYPE == 1)
// 底板串口路径（i.MX6Q）                                   /* 在 .cpp 里定义，避免在 .h 里定义，每次修改都要重新编译整个程序 */
const char *G_COM_BASEBOARD     = "/dev/ttySAC1";
// 蓝牙串口路径（i.MX6Q）
const char *G_COM_BLUETOOTH     = "/dev/ttySAC2";
// 测距通信串口路径（i.MX6Q）
const char *G_COM_DISTANCE      = "/dev/ttySAC3";
# elif (OS_TYPE == 3)
// 底板串口路径（rk3568）
const char *G_COM_BASEBOARD     = "/dev/ttyS4";
// 蓝牙串口路径（rk3568）
const char *G_COM_BLUETOOTH     = "/dev/ttyS3";
// 测距通信串口路径（rk3568）
const char *G_COM_DISTANCE      = "/dev/ttyS7";
#elif (OS_TYPE == 2)
// 底板串口路径（PC）
const char *G_COM_BASEBOARD     = "/dev/ttyUSB0";
// 蓝牙串口路径（PC）
const char *G_COM_BLUETOOTH     = "/dev/ttyUSB1";
// 测距通信串口路径（PC）                                     /* Tips: 如果串口被占用，检查数据传输是否设置了“使用 USB 串口”，且没有足够的 USB 串口供其使用。见 CSerialDatatrans::getSerialPortPath() */
const char *G_COM_DISTANCE      = "/dev/ttyUSB2";
#endif

const char *G_LANGUAGE_DEFAULT  = G_LANGUAGE_CHINESE;       // 缺省语言

// =============================================================================================================================================
// 静态变量定义

QString CGlobal::logPath = "/media/log";    // 日志目录

//
const char * const CGlobal::LOG_ALL         = "";               // TODO: 这个常量定义应放到 log 内部？
const char * const CGlobal::LOG_TEMP        = "temp";
const char * const CGlobal::LOG_SYS         = "sys";
const char * const CGlobal::LOG_BASEBOARD   = "baseboard";
const char * const CGlobal::LOG_DISTANCE    = "distance";
const char * const CGlobal::LOG_MEASURE     = "measure";
const char * const CGlobal::LOG_CAPTURE     = "capture";
const char * const CGlobal::LOG_ALGO        = "algo";
const char * const CGlobal::LOG_BLUETOOTH   = "bluetooth";
const char * const CGlobal::LOG_WIFI        = "wifi";
const char * const CGlobal::LOG_DATATRANS   = "datatrans";
const char * const CGlobal::LOG_DATABASE    = "database";
const char * const CGlobal::LOG_SERIAL      = "serial";

//
int CGlobal::minPupilParamRatio         = 100;                              // 最小瞳孔参数比例（百分比数）   // NOTE: 只有最旧版的算法用到

#define DEFAULT_MIN_PUPIL_AREA_RD       9
int CGlobal::minPupilAreaRD             = DEFAULT_MIN_PUPIL_AREA_RD;        // 最小瞳孔区域面积（前两年龄段）  // RD: reduced dimension（降维）

#define DEFAULT_MIN_PUPIL_RECT_SIDE_RD  3
int CGlobal::minPupilRectSideRD         = DEFAULT_MIN_PUPIL_RECT_SIDE_RD;   // 最小瞳孔区域边长

#define DEFAULT_MIN_PUPIL_RADIUS        7
int CGlobal::minPupilRadius             = DEFAULT_MIN_PUPIL_RADIUS;         // 最小瞳孔半径（7 * PIX_TO_PHY ~= 1.1）

#define DEFAULT_MIN_PUPIL_AREA          260
int CGlobal::minPupilArea               = DEFAULT_MIN_PUPIL_AREA;           // 最小瞳孔面积（前两年龄段）

QString CGlobal::language               = G_LANGUAGE_ENGLISH;               // 语言（与翻译文件名称关键字一致）（默认为英文）

int CGlobal::captureInterval            = 10;

enDistSensorType CGlobal::distSensorType    = enDistSensorType::SIMAN_SDM10;
//int CGlobal::distanceInterval           = 100;
int CGlobal::distanceInterval2           = 100;
enDistanceUnit CGlobal::distanceUnit    = distanceUnit_cm;

bool CGlobal::isDebugMode               = false;

bool CGlobal::isStatisticalEnabled      = false;    // 允许出统计值（“推值”）
bool CGlobal::isEnableMonthAgeVision    = false;    // 是否启用“按月龄推值”

bool CGlobal::isMultiMeasure            = false;    // 是否多次测量（计算3次出均值）  // NOTE: 2025-11-20 应用新的算法策略，此变量的作用改为是否显示结果详情

//
#define DEFOUT_MAX_PUPIL_FAIL       10
#define DEFOUT_MAX_ALGO_FAIL        0
#define DEFOUT_MAX_MEASURE_SECS     15

int CGlobal::maxPupilFail       = DEFOUT_MAX_PUPIL_FAIL;        // 灰度检查失败最大次数
int CGlobal::maxAlgoFail        = DEFOUT_MAX_ALGO_FAIL;         // 出推值转灯次数（前两年龄段）
int CGlobal::pushStatisticSecs  = DEFOUT_MAX_MEASURE_SECS;      // 出推值限时（单位：秒。从检测到瞳孔后开始，超时后显示统计结果，仅适用于前两年龄段）

#define DEFAULT_PUPIL_AVERAGE_MIN        60
#define DEFAULT_PUPIL_AVERAGE_MAX       160
#define DEFAULT_EXPOSURE_MS_MIN         1.0
#define DEFAULT_EXPOSURE_MS_MAX         16.0
#define DEFAULT_EXPO_COARSE_ADJ_STEP    0.5
#define DEFAULT_EXPO_FINE_ADJ_STEP      0.2

int CGlobal::pupilAverageMin_       = DEFAULT_PUPIL_AVERAGE_MIN     ;   // 最小瞳孔灰度          // TODO: 这个值的标准？v1.3.9 的值是 80    // 20260713: 使已保存的旧默认值 100 变成新默认值 60
int CGlobal::pupilAverageMax        = DEFAULT_PUPIL_AVERAGE_MAX     ;   // 最大瞳孔灰度
double CGlobal::exposureMsMin       = DEFAULT_EXPOSURE_MS_MIN       ;   // 最小曝光时间（ms）
double CGlobal::exposureMsMax       = DEFAULT_EXPOSURE_MS_MAX       ;   // 最大曝光时间（ms）
double CGlobal::expoCoarseAdjStepMs = DEFAULT_EXPO_COARSE_ADJ_STEP  ;   // 曝光粗调步长（ms）
double CGlobal::expoFineAdjStepMs   = DEFAULT_EXPO_FINE_ADJ_STEP    ;   // 曝光精调步长（ms）

float CGlobal::minPupilStaDev   = 4.9;          // 最小瞳孔标准偏差

bool CGlobal::isReducedVersion  = false;        // 是否裁减版

enProductModel CGlobal::productModel = productModel_SL100P;     // 产品型号

QString CGlobal::pupilAlgoVerDesc[algoVerAll_Max + 1]  = {"v_2019", "v_2021-07", "v_2022-04_1", "v_2022-04_2", "v_2022-12"};
enAlgoVerAll CGlobal::pupilAlgoVerCfg      = algoVerAll_2022_12;      // 瞳孔识别算法版本（用户设定）
enAlgoVerAll CGlobal::currentPupilAlgoVer  = algoVerAll_2022_12;      // 瞳孔识别算法版本（业务逻辑选定）
bool CGlobal::isSpecifiedAlgo           = true;                 // 是否指定算法
bool CGlobal::isPupilAccutrately        = false;

int CGlobal::algoSaveImgIndex   = -1;

bool CGlobal::isReadBarcodeByQt = false;

enAlgoMode CGlobal::algoMode            = algoMode_General;

#define DEFAULT_MIN_PUPIL_DETECTED_COUNT 3
int CGlobal::minPupilDetectedCount = DEFAULT_MIN_PUPIL_DETECTED_COUNT;

double CGlobal::ultraCoefficient    = 1;        // 超声换算系数（以 MB1010 超声系数为 1，新超声系数是 0.129）    /* 默认为 1，兼容旧程序升级到本版本程序。因为安装没有改配置项的旧程序的机器应该是旧超声 */

int CGlobal::distanceOffset         = 0;            // 距离补偿(mm)（旧变量名 distanceFix ）
int CGlobal::distTolerance          = 30;           // 距离允差（mm）

bool CGlobal::isInvertImg           = false;        // 是否倒转图像

bool CGlobal::isSetLedLevel         = true;         // 是否设置 LED 电流等级
int CGlobal::ledLevelMiddle         = 12;           // 中心 LED 电流等级
int CGlobal::ledLevelEccentric      = 16;           // 偏心 LED 电流等级

bool CGlobal::isAutoTurnLampWhenSelfControl         = true;         // 自控模式时是否自动转灯
bool CGlobal::isAutoTurnLampWhenExternalControl     = false;        // 受控模式时是否自动转灯

bool CGlobal::isMagnifyPupilImg     = false;        // 是否放大瞳孔图像（预检阶段）

CVisionNotation CGlobal::visionNotation    = CVisionNotation(visionNotation_None);   // 参考视力的显示类型

// 缺省年龄段（设为 6~20岁，因为视筛仪测得最多的是这个年龄段）
#define DEFAULT_AGE_RANGE   ageRange_3_06_20_YEAR
enAgeRange CGlobal::defaultAgeRange = DEFAULT_AGE_RANGE;        // 缺省年龄段（用户没有指定年龄段而开始测量时所用的年龄段）

int CGlobal::dataTransSerialBaud    = WinDataTrans::DEFAULT_USB_SERIAL_BARD;

int CGlobal::frameRate              = 10;

bool CGlobal::isSingleThreadCalc    = false;

// 屏幕亮度（百分数的分子），选倒数第二个为缺省值
CScreenBrightness CGlobal::screenBrightnessCfg;

#define MAX_GAZE_DEVIATION 10
int CGlobal::maxGazeDeviation       = MAX_GAZE_DEVIATION;   // 最大固视偏差（°）

bool CGlobal::isSimulatedEye        = false;        // 是否模拟眼

QString CGlobal::organizationName   = "";           // 机构名称（小票的）
QString CGlobal::orgNameA4          = "";           // 机构名称（A4报告的）
QString CGlobal::operatorName       = "";           // 操作者

CAuthIntf::enAuthType CGlobal::authType = CAuthIntf::authType_Permanent;    // 授权类型         /* 默认为永久授权，管理员激发”激活试用机“之后，才设为试用 */
QDate CGlobal::authExpiryDate;                                              // 授权到期日

#define DEFAULT_DEV_ACTIVATED true
bool CGlobal::isDevActivated        = DEFAULT_DEV_ACTIVATED;    // 设备是否已激活      // NOTE: 默认值为已激活，兼容旧机器。新机器出库前须将机器置为未激活。

#define DEFAULT_DEV_NUM     "SL-100-"
QString CGlobal::devNum;                                        // 产品编号

bool CGlobal::isAutoPrintTicket                        = false;                        // 是否自动打印小票
enTicketPrintConnType CGlobal::ticketPrintConnType    = ticketPrintConnType_BT;      // 小票打印的连接类型

float CGlobal::resultCorrectSph_General     = +0.00;        // 结果修正_球镜度_常规光路
float CGlobal::resultCorrectCyl_General     = +0.00;        // 结果修正_柱镜度_常规光路
float CGlobal::resultCorrectSph_Square      = +0.50;        // 结果修正_球镜度_方形视筛箱
float CGlobal::resultCorrectCyl_Square      = +0.25;        // 结果修正_柱镜度_方形视筛箱
float CGlobal::resultCorrectSph_LSharp      = +0.00;        // 结果修正_球镜度_L形视筛箱
float CGlobal::resultCorrectCyl_LSharp      = +0.00;        // 结果修正_柱镜度_L形视筛箱

float CGlobal::eyeWhRatio_Model             = 0.60;         // 模拟眼瞳孔长宽比     // 2024-06-07 增加瞳孔长宽比的设置，提供给算法。需求来源：刘宇 2024-06-06
float CGlobal::eyeWhRatio_Human             = 0.75;         // 人眼瞳孔长宽比

int CGlobal::resultStableCountThresh        = 2;            // 结果稳定次数阈值（达到此次数后认为已稳定）
double CGlobal::resultStableDiopterThresh   = 0.5;          // 结果稳定度数阈值（屈光度数浮动范围在此范围内认为已稳定）
int CGlobal::countMaxMeasureTimes           = 3;            // 最大测量次数
double CGlobal::hardTriggerIntervalDelayMs  = 6.5;          // 硬触发间隔附加延时（毫秒）    // NOTE: 若为0，则使用默认触发延迟
double CGlobal::hardTriggerDelayMs          = 2.2;          // 硬触发延时（毫秒）
enTriggerInputType CGlobal::triggerInputType = enTriggerInputType::Unknown;  // 触发输入类型

bool CGlobal::isVoicePrompt                 = false;        // 是否语音提示（音乐和语音是互斥的，即若开启了“语音提示”，则不再播放音乐）

enAutoScreenOff CGlobal::autoScreenOff      = enAutoScreenOff::Default;     // 自动息屏

// 保护变量 ====================

bool CGlobal::m_isExternalControl           = false;        // 是否外部控制

bool CGlobal::m_isMusicEnabled              = true;
bool CGlobal::m_isColoredLampEnabled        = true;

QString CGlobal::s_pathConfig;                              // 配置文件夹路径

bool CGlobal::s_isLogToFile                 = false;        // 是否输出日志到文件

// =============================================================================================================================================
// 类定义

void CGlobal::init()
{
    // 本模块变量的初始化
    screenBrightnessCfg = CScreenBrightness(enScreenBrightness(0)).reset();
    s_pathConfig = QCoreApplication::applicationDirPath() + QDir::separator() + "config";

    //
#if (OS_TYPE == 2)
    //isDebugMode = true;
#endif

    //
    restoreConfs();

    // log 模块的基本设置
    logger()->setIsEnabled(true);                                           // 启用 logger （非必须，已默认启用）
    logger()->setLogLevel(logLevel_Debug);                                  // 设置 log 级别

    //logger()->addFilterTag(LOG_UI);                                         // 添加过滤标签（若有添加，则只输出已添加了标签的 log 消息）
    //logger()->addFilterTag(LOG_ALGO);
    //logger()->addFilterTag(LOG_CAMERA);

    // 添加 log Appender：输出到 控制台
    CLogConsoleAppender *appender_console = new CLogConsoleAppender();
    logger()->addAppender(appender_console);

    // 添加 log Appender：输出到 故障记录
    //CLogFaultAppender *appender_fault = new CLogFaultAppender();
    //logger()->addAppender(appender_fault);

    // 桌面版禁用 KeyboardReader
#if (OS_TYPE == 2)
    //isReadBarcodeByQt = true;   // 否则即使在别的程序界面按回车，也会触发 KeyboardReader 的 sigGetLine 信号，导致弹出被测者信息框
#endif

#ifndef TEST_MODE
    //
    if (!isReadBarcodeByQt)
    {
        kbReader()->Block = false;
    }
#endif

    // 初始化随机数发生器
    srand(time(NULL));

    // 检查确保基本的目录的存在
    if (!QFile::exists(pathConfig())) {
        if (!QDir().mkpath(pathConfig())) {
            logCritical("Failed to make dir \"" + pathConfig() + "\"");
        }
    }

    // 载入配置值
    CGlobal::loadConfs();

    // 各个模块的 init() 过程的挂接
    CDataIntfGuanXin::init();

}

QString CGlobal::langCode()
{
    if (G_LANGUAGE_ENGLISH == CGlobal::language) {
        return QString::fromStdString(DataTrans::LANGUAGE_ENGLISH);
    } else if (G_LANGUAGE_CHINESE == CGlobal::language) {
        return QString::fromStdString(DataTrans::LANGUAGE_CHINESE);
    } else if (G_LANGUAGE_GERMAN == CGlobal::language) {
        return QString::fromStdString(DataTrans::LANGUAGE_ENGLISH);
    } else {
        return QString::fromStdString(DataTrans::LANGUAGE_ENGLISH);
    }
}

//
int CGlobal::getMinPupilParamRatio()
{
    return minPupilParamRatio;
}

void CGlobal::setMinPupilParamRatio(int _ratio)
{
    CGlobal::minPupilAreaRD     = round((float)DEFAULT_MIN_PUPIL_AREA_RD        * _ratio / 100);
    CGlobal::minPupilRectSideRD = round((float)DEFAULT_MIN_PUPIL_RECT_SIDE_RD   * _ratio / 100);
    CGlobal::minPupilRadius     = round((float)DEFAULT_MIN_PUPIL_RADIUS         * _ratio / 100);
    CGlobal::minPupilArea       = round((float)DEFAULT_MIN_PUPIL_AREA           * _ratio / 100);

    CGlobal::minPupilParamRatio = _ratio;
}

bool CGlobal::getIsExternalControl(bool _internal_stat_only)
{
    if (_internal_stat_only) {
        return m_isExternalControl;
    } else {
        enDataInterfaceCfg intf_type = WinDataTrans::getCfg_intfType();
        bool is_external = m_isExternalControl &&
                (dataInterfaceCfg_Bluetooth == intf_type ||
                 dataInterfaceCfg_UsbUart == intf_type ||
                 dataInterfaceCfg_Uart == intf_type
                 );
        return is_external;
    }
}

void CGlobal::setIsExternalControl(bool _is_external)
{
    m_isExternalControl = _is_external;
}

enAlgoVerAll CGlobal::getPupilAlgoVerCfg()
{
    return pupilAlgoVerCfg;
}

void CGlobal::setPupilAlgoVer(enAlgoVerAll _pupil_algo_ver)
{
    pupilAlgoVerCfg = _pupil_algo_ver;
}

// 判度并设置是否使用旧算法
void CGlobal::judgeAndSetPupilAlgoVer(int _age_range)
{
    if (isSpecifiedAlgo) {                              // 如果强制指定算法，则当前算法与选定算法一致，不管其它业务逻辑
        CGlobal::currentPupilAlgoVer = pupilAlgoVerCfg;
    } else {                                            // 否则，当前算法受业务逻辑影响
        if (algoMode_General == CGlobal::algoMode) {    // 普通模式，前两年龄段使用 设定的算法，其它年龄段使用 "v_2019"
            CGlobal::currentPupilAlgoVer = (_age_range <= 1 ? pupilAlgoVerCfg : algoVerAll_2019);
        } else {                                        // 专业模式，全部年龄段都使用旧算法（新算法未完善）
            //CGlobal::currentPupilAlgoVer = algoVerAll_2019;
            CGlobal::currentPupilAlgoVer = algoVerAll_Default;
        }
    }
}

enAlgoVerAll CGlobal::getCurrentPupilAlgoVer()
{
    return currentPupilAlgoVer;
}

bool CGlobal::getIsNeedMonthAgeVision(int _age_range)
{
    // 普通模式，前两年龄段
    return (CGlobal::isEnableMonthAgeVision && (algoMode_General == CGlobal::algoMode) && (_age_range <= 1));
}

bool CGlobal::getIsLogEnabled()
{
    return logger()->getIsEnabled();
}

void CGlobal::setIsLogEnabled(bool _enabled)
{
    logger()->setIsEnabled(_enabled);
}

bool CGlobal::getIsLogToFile()
{
    return s_isLogToFile;
}

void CGlobal::setIsLogToFile(bool _enabled)
{
    s_isLogToFile = _enabled;

    // 添加 log Appender：输出到 文件
#if M_OS_TYPE != 2
    static CLogFileAppender *appender_file {nullptr};

    if (_enabled) {
        if (!appender_file) {
            appender_file = new CLogFileAppender();
        }

        logger()->addAppender(appender_file);

        QString log_root_path = (qApp->applicationDirPath() + "/%1").arg("log-files");
        appender_file->setRootDirPath(log_root_path);                           // 设置 log 文件的根目录

        appender_file->setOneFilePerDay(true);                                  // 设置每天一个日志文件（默认是每小时一个文件）

        //appender_file->AddSeparatedTag(LOG_ALGO);                               // 添加独立保存的 log 标签
        //appender_file->AddSeparatedTag(LOG_MEASURE);
    } else {
        if (appender_file) {
            logger()->removeAppender(appender_file);
        }
    }
#endif
}

enLogLevel CGlobal::getLogLevel()
{
    return logger()->getLogLevel();
}

void CGlobal::setLogLevel(enLogLevel _log_level)
{
    logger()->setLogLevel(_log_level);
}

void CGlobal::setFilter(QString _tag)
{
    const QStringList &filters = logger()->getFilterTags();

    if (!_tag.isEmpty()) {
        if (!filters.contains(_tag)) {
            logger()->addFilterTag(_tag);
        }
    } else {
        logger()->clearFilterTags();
    }
}

QString CGlobal::getFilter()
{
    const QStringList &filters = logger()->getFilterTags();

    if (!filters.isEmpty()) {
        return filters.at(0);       // TODO: 这里只能得到第一个
    } else {
        return "";
    }
}

enScreenBrightness CGlobal::getScreenBrightnessCfg()
{
    return screenBrightnessCfg.getValue();
}

bool CGlobal::setScreenBrightnessCfg(const enScreenBrightness &_brightness)
{
    return screenBrightnessCfg.setValue(_brightness);
}

bool CGlobal::getIsAutoTurnLamp()
{
    return (!CGlobal::getIsExternalControl() ? CGlobal::isAutoTurnLampWhenSelfControl : CGlobal::isAutoTurnLampWhenExternalControl);
}

bool CGlobal::getIsMusicEnabled()
{
    return m_isMusicEnabled;
}

void CGlobal::setIsMusicEnabled(bool _enabled)
{
    m_isMusicEnabled = _enabled;
}

bool CGlobal::getIsColoredLampEnabled()
{
    return m_isColoredLampEnabled;
}

void CGlobal::setIsColoredLampEnabled(bool _enabled)
{
    m_isColoredLampEnabled = _enabled;
}

const QString &CGlobal::pathConfig()
{
    // 确保目录存在
    if (QFile::exists(s_pathConfig)) {
        QDir dir(s_pathConfig);
        dir.mkpath(s_pathConfig);
    }

    //
    return s_pathConfig;
}

// 载入全局变量配置值    // TODO: 配置字段名的变量化？     // TODO: 配置分段的整理
/* 若配置文件未含改 key，则会保存下缺省值 */
void CGlobal::loadConfs()
{
    //CGlobal::maxAlgoFail            = appSetting::value("global/maxAlgoFail", CGlobal::maxAlgoFail).toInt();                    // 这里读取配置时的缺省值不应设为当前值，因为当前值不一定等于缺省值
    //CGlobal::isReducedVersion       = appSetting::value("global/isReducedVersion", CGlobal::isReducedVersion).toBool();
    CGlobal::productModel           = (enProductModel)appSetting::value("global/productModel", (int)CGlobal::productModel).toInt();
    CGlobal::pupilAverageMin_       = appSetting::value("global/pupilAverageMin_", CGlobal::pupilAverageMin_).toInt();
    CGlobal::pupilAverageMax        = appSetting::value("global/pupilAverageMax", CGlobal::pupilAverageMax).toInt();
    CGlobal::exposureMsMin          = appSetting::value("global/exposureMsMin", CGlobal::exposureMsMin).toDouble();
    CGlobal::exposureMsMax          = appSetting::value("global/exposureMsMax", CGlobal::exposureMsMax).toDouble();
    CGlobal::expoCoarseAdjStepMs    = appSetting::value("global/expoCoarseAdjStepMs", CGlobal::expoCoarseAdjStepMs).toFloat();
    CGlobal::pushStatisticSecs      = appSetting::value("global/pushStatisticSecs", CGlobal::pushStatisticSecs).toInt();
    CGlobal::minPupilStaDev         = appSetting::value("global/minPupilStaDev", CGlobal::minPupilStaDev).toFloat();
    //CGlobal::isDebugMode            = appSetting::value("global/isDebugMode", CGlobal::isDebugMode).toBool();       // TODO: 这个还是不要保存了？或者保存但只在下次重启时有效？

    //CGlobal::isSpecifiedAlgo    = appSetting::value("global/isSpecifiedAlgo", CGlobal::isSpecifiedAlgo).toBool();
    //CGlobal::pupilAlgoVerCfg    = (enAlgoVerAll)(appSetting::value("global/pupilAlgoVerCfg", CGlobal::pupilAlgoVerCfg).toInt());
    //CGlobal::isPupilAccutrately     = appSetting::value("global/isPupilAccutrately", CGlobal::isPupilAccutrately).toBool();

    //CGlobal::isStatisticalEnabled   = appSetting::value("global/enableStatisticalEnabled", CGlobal::isStatisticalEnabled).toBool();
    //CGlobal::isEnableMonthAgeVision = appSetting::value("global/isEnableMonthAgeVision", CGlobal::isEnableMonthAgeVision).toBool();

    CGlobal::isMultiMeasure             = appSetting::value("/global/isMultiMeasure", CGlobal::isMultiMeasure).toBool();

    CGlobal::language           = appSetting::value("global/language", CGlobal::language).toString();
    CGlobal::captureInterval    = appSetting::value("global/captureInterval", CGlobal::captureInterval).toInt();

    //CGlobal::setIsLogEnabled(appSetting::value("global/isLogEnabled_20220722", CGlobal::getIsLogEnabled()).toBool());
    //CGlobal::setIsLogToFile(appSetting::value("global/isLogToFile", CGlobal::getIsLogToFile()).toBool());                // TODO: 这个还是不要保存了？

    CGlobal::algoMode           = (enAlgoMode)appSetting::value("global/algoMode", (int)algoMode_Professional).toInt();
    CGlobal::minPupilDetectedCount  = appSetting::value("global/minPupilDetectedCount", DEFAULT_MIN_PUPIL_DETECTED_COUNT).toInt();

    //CGlobal::distSensorType     = (enDistSensorType)appSetting::value("global/distSensorType", (int)CGlobal::distSensorType).toInt();   /* 目前正式产品中只有一种测距模块类型，为了避免配置错误而导致故障，禁用这项配置的保存 */
    CGlobal::distanceInterval2  = appSetting::value("global/distanceInterval2", CGlobal::distanceInterval2).toInt();
    CGlobal::distanceUnit       = (enDistanceUnit)appSetting::value("global/distanceUnit", (int)CGlobal::distanceUnit).toInt();

    CGlobal::ultraCoefficient   = appSetting::value("global/ultraCoefficient", CGlobal::ultraCoefficient).toDouble();

    CGlobal::distanceOffset     = appSetting::value("camera/distanceFix", CGlobal::distanceOffset).toInt();
    CGlobal::distTolerance      = appSetting::value("global/distTolerance", CGlobal::distTolerance).toInt();

    CGlobal::isInvertImg        = appSetting::value("global/isInvertImg", CGlobal::isInvertImg).toBool();

    CGlobal::isSetLedLevel      = appSetting::value("global/isSetLedLevel", CGlobal::isSetLedLevel).toBool();
    CGlobal::ledLevelMiddle     = appSetting::value("global/ledLevelMiddle", CGlobal::ledLevelMiddle).toInt();
    CGlobal::ledLevelEccentric  = appSetting::value("global/ledLevelEccentric", CGlobal::ledLevelEccentric).toInt();

    CGlobal::isAutoTurnLampWhenExternalControl  = appSetting::value("global/isAutoTurnLampWhenExternalControl", CGlobal::isAutoTurnLampWhenExternalControl).toBool();

    CGlobal::visionNotation.setValue((enVisionNotation)(appSetting::value("global/visionNotation", CGlobal::visionNotation.toInt()).toInt()));

    CGlobal::defaultAgeRange    = (enAgeRange)appSetting::value("global/defaultAgeRange", CGlobal::defaultAgeRange).toInt();

    CGlobal::dataTransSerialBaud      = appSetting::value("/data/dataTransSerialBaud", CGlobal::dataTransSerialBaud).toInt();

    CGlobal::frameRate          = appSetting::value("global/frameRate", CGlobal::frameRate).toInt();

    bool is_brightness_valid = CGlobal::screenBrightnessCfg.setValue((enScreenBrightness)appSetting::value("global/screenBrightnessCfg",
                                                                                                           CGlobal::screenBrightnessCfg.toInt()
                                                                                                           ).toInt()
                                                                     );
    if (!is_brightness_valid) {
        CGlobal::screenBrightnessCfg.reset();
    }

    CGlobal::maxGazeDeviation   = appSetting::value("global/maxGazeDeviation", MAX_GAZE_DEVIATION).toInt();

    CGlobal::organizationName   = appSetting::value("global/organizationName", "").toString();
    CGlobal::orgNameA4          = appSetting::value("global/orgNameA4", "").toString();
    CGlobal::operatorName       = appSetting::value("global/operatorName", "").toString();

    CGlobal::authType           = (CAuthIntf::enAuthType)appSetting::value("global/authType", "").toInt();
    CGlobal::authExpiryDate     = appSetting::value("global/authExpiryDate", "").toDate();

    CGlobal::isDevActivated     = appSetting::value("global/isDevActivated", DEFAULT_DEV_ACTIVATED).toBool();

    CGlobal::devNum             = appSetting::value("tool/devnum", DEFAULT_DEV_NUM).toString();

    CGlobal::isAutoPrintTicket            = appSetting::value("/tool/autoprint", CGlobal::isAutoPrintTicket).toBool();
    CGlobal::ticketPrintConnType   = (enTicketPrintConnType)appSetting::value("/tool/ticketPrintConnType", (int)CGlobal::ticketPrintConnType).toInt();

    CGlobal::resultCorrectSph_General   = appSetting::value("/global/resultCorrectSph_General", CGlobal::resultCorrectSph_General).toFloat();
    CGlobal::resultCorrectCyl_General   = appSetting::value("/global/resultCorrectCyl_General", CGlobal::resultCorrectCyl_General).toFloat();
    CGlobal::resultCorrectSph_Square    = appSetting::value("/global/resultCorrectSph_Square", CGlobal::resultCorrectSph_Square).toFloat();
    CGlobal::resultCorrectCyl_Square    = appSetting::value("/global/resultCorrectCyl_Square", CGlobal::resultCorrectCyl_Square).toFloat();
    CGlobal::resultCorrectSph_LSharp    = appSetting::value("/global/resultCorrectSph_LSharp", CGlobal::resultCorrectSph_LSharp).toFloat();
    CGlobal::resultCorrectCyl_LSharp    = appSetting::value("/global/resultCorrectCyl_LSharp", CGlobal::resultCorrectCyl_LSharp).toFloat();

    CGlobal::eyeWhRatio_Model           = appSetting::value("/global/eyeWhRatio_Model", CGlobal::eyeWhRatio_Model).toFloat();
    CGlobal::eyeWhRatio_Human           = appSetting::value("/global/eyeWhRatio_Human", CGlobal::eyeWhRatio_Human).toFloat();

    CGlobal::resultStableCountThresh    = appSetting::value("/global/resultStableCountThresh", CGlobal::resultStableCountThresh).toInt();
    CGlobal::resultStableDiopterThresh  = appSetting::value("/global/resultStableDiopterThresh", CGlobal::resultStableDiopterThresh).toDouble();
    CGlobal::countMaxMeasureTimes       = appSetting::value("/global/countMaxMeasureTimes", CGlobal::countMaxMeasureTimes).toInt();
    CGlobal::hardTriggerIntervalDelayMs = appSetting::value("/global/hardTriggerIntervalDelayMs", CGlobal::hardTriggerIntervalDelayMs).toDouble();
    CGlobal::hardTriggerDelayMs         = appSetting::value("/global/hardTriggerDelayMs", CGlobal::hardTriggerDelayMs).toDouble();
    CGlobal::triggerInputType          = static_cast<enTriggerInputType>(appSetting::value("/global/triggerSignalType", static_cast<int>(CGlobal::triggerInputType)).toInt());

    CGlobal::isVoicePrompt              = appSetting::value("/global/isVoicePrompt", CGlobal::isVoicePrompt).toBool();
    CGlobal::autoScreenOff              = static_cast<enAutoScreenOff>(appSetting::value("/global/autoScreenOff", static_cast<int>(CGlobal::autoScreenOff)).toInt());

    CGlobal::m_isExternalControl        = appSetting::value("/tool/isExternalControl", CGlobal::m_isExternalControl).toBool();

    CGlobal::m_isMusicEnabled           = appSetting::value("/global/isMusicEnabled", CGlobal::m_isMusicEnabled).toBool();
    CGlobal::m_isColoredLampEnabled     = appSetting::value("/global/isColoredLampEnabled", CGlobal::m_isColoredLampEnabled).toBool();

    //
    if (!checkConfs()) {        // TODO: 如果这里检查不通过，界面上有没有提示，问题难查找？
        saveConfs();
    }

}

// 保存配置
void CGlobal::saveConfs()       // TODO: 未设置过的值不保存，这样，缺省值改变之后，可以生效
{
    //
    checkConfs();

    //
    //appSetting::setValue("global/maxAlgoFail", CGlobal::maxAlgoFail);
    //appSetting::setValue("global/isReducedVersion", CGlobal::isReducedVersion);
    appSetting::setValue("global/productModel", (int)CGlobal::productModel);
    appSetting::setValue("global/pupilAverageMin_", CGlobal::pupilAverageMin_);
    appSetting::setValue("global/pupilAverageMax", CGlobal::pupilAverageMax);
    appSetting::setValue("global/exposureMsMin", CGlobal::exposureMsMin);
    appSetting::setValue("global/exposureMsMax", CGlobal::exposureMsMax);
    appSetting::setValue("global/expoCoarseAdjStepMs", CGlobal::expoCoarseAdjStepMs);
    appSetting::setValue("global/pushStatisticSecs", CGlobal::pushStatisticSecs);
    appSetting::setValue("global/minPupilStaDev", CGlobal::minPupilStaDev);
    //appSetting::setValue("global/isDebugMode", CGlobal::isDebugMode);

    //appSetting::setValue("global/isSpecifiedAlgo", CGlobal::isSpecifiedAlgo);
    //appSetting::setValue("global/pupilAlgoVerCfg", (int)CGlobal::pupilAlgoVerCfg);
    //appSetting::setValue("global/isPupilAccutrately", CGlobal::isPupilAccutrately);

    //appSetting::setValue("global/isStatisticalEnabled", CGlobal::isStatisticalEnabled);
    //appSetting::setValue("global/isEnableMonthAgeVision", CGlobal::isEnableMonthAgeVision);

    appSetting::setValue("/global/isMultiMeasure", CGlobal::isMultiMeasure);

    appSetting::setValue("global/language", CGlobal::language);
    appSetting::setValue("global/captureInterval", CGlobal::captureInterval);

    //appSetting::setValue("global/isLogEnabled_20220722", CGlobal::getIsLogEnabled());
    //appSetting::setValue("global/isLogToFile", CGlobal::getIsLogToFile());

    //appSetting::setValue("global/distSensorType", (int)CGlobal::distSensorType);
    appSetting::setValue("global/distanceInterval2", CGlobal::distanceInterval2);
    appSetting::setValue("global/distanceUnit", (int)CGlobal::distanceUnit);

    appSetting::setValue("global/algoMode", (int)CGlobal::algoMode);
    appSetting::setValue("global/minPupilDetectedCount", CGlobal::minPupilDetectedCount);

    appSetting::setValue("global/ultraCoefficient", CGlobal::ultraCoefficient);

    appSetting::setValue("camera/distanceFix", CGlobal::distanceOffset);
    appSetting::setValue("global/distTolerance", CGlobal::distTolerance);

    appSetting::setValue("global/isInvertImg", CGlobal::isInvertImg);

    appSetting::setValue("global/isSetLedLevel", CGlobal::isSetLedLevel);
    appSetting::setValue("global/ledLevelMiddle", CGlobal::ledLevelMiddle);
    appSetting::setValue("global/ledLevelEccentric", CGlobal::ledLevelEccentric);

    appSetting::setValue("global/isAutoTurnLampWhenExternalControl", CGlobal::isAutoTurnLampWhenExternalControl);

    appSetting::setValue("global/visionNotation", CGlobal::visionNotation.toInt());

    appSetting::setValue("global/defaultAgeRange", (int)CGlobal::defaultAgeRange);

    appSetting::setValue("/data/dataTransSerialBaud", CGlobal::dataTransSerialBaud);

    appSetting::setValue("global/frameRate", CGlobal::frameRate);

    appSetting::setValue("global/screenBrightnessCfg", (int)CGlobal::screenBrightnessCfg.getValue());

    appSetting::setValue("global/maxGazeDeviation", CGlobal::maxGazeDeviation);

    appSetting::setValue("global/organizationName", CGlobal::organizationName);
    appSetting::setValue("global/orgNameA4", CGlobal::orgNameA4);
    appSetting::setValue("global/operatorName", CGlobal::operatorName);

    appSetting::setValue("global/authType", (int)CGlobal::authType);
    appSetting::setValue("global/authExpiryDate", CGlobal::authExpiryDate.toString(DEFAULT_DATE_FORMAT));

    appSetting::setValue("global/isDevActivated", CGlobal::isDevActivated);

    appSetting::setValue("tool/devnum", CGlobal::devNum);

    appSetting::setValue("/tool/autoprint", CGlobal::isAutoPrintTicket);
    appSetting::setValue("/tool/ticketPrintConnType", (int)CGlobal::ticketPrintConnType);

    appSetting::setValue("/global/resultCorrectSph_General", CGlobal::resultCorrectSph_General);
    appSetting::setValue("/global/resultCorrectCyl_General", CGlobal::resultCorrectCyl_General);
    appSetting::setValue("/global/resultCorrectSph_Square", CGlobal::resultCorrectSph_Square);
    appSetting::setValue("/global/resultCorrectCyl_Square", CGlobal::resultCorrectCyl_Square);
    appSetting::setValue("/global/resultCorrectSph_LSharp", CGlobal::resultCorrectSph_LSharp);
    appSetting::setValue("/global/resultCorrectCyl_LSharp", CGlobal::resultCorrectCyl_LSharp);

    appSetting::setValue("/global/eyeWhRatio_Model", CGlobal::eyeWhRatio_Model);
    appSetting::setValue("/global/eyeWhRatio_Human", CGlobal::eyeWhRatio_Human);

    appSetting::setValue("/global/resultStableCountThresh", CGlobal::resultStableCountThresh);
    appSetting::setValue("/global/resultStableDiopterThresh", CGlobal::resultStableDiopterThresh);
    appSetting::setValue("/global/countMaxMeasureTimes", CGlobal::countMaxMeasureTimes);
    appSetting::setValue("/global/hardTriggerIntervalDelayMs", CGlobal::hardTriggerIntervalDelayMs);
    appSetting::setValue("/global/hardTriggerDelayMs", CGlobal::hardTriggerDelayMs);
    appSetting::setValue("/global/triggerSignalType", static_cast<int>(CGlobal::triggerInputType));

    appSetting::setValue("/global/isVoicePrompt", CGlobal::isVoicePrompt);
    appSetting::setValue("/global/autoScreenOff", static_cast<int>(CGlobal::autoScreenOff));

    appSetting::setValue("/tool/isExternalControl", CGlobal::m_isExternalControl);

    appSetting::setValue("/global/isMusicEnabled", CGlobal::m_isMusicEnabled);
    appSetting::setValue("/global/isColoredLampEnabled", CGlobal::m_isColoredLampEnabled);

    //
    appSetting::sync();
}

// 数据检验修正       // TODO: 这里的逻辑检查，不应该划分到全局模块，而是应该放到对应的配置模块，全局初始化时应调用各个模块的检查校正函数处理一次？
bool CGlobal::checkConfs()
{
    bool is_all_ok = true;

//#ifndef TEST_MODE
    //
    //if (!CDistanceDetect::checkAndCorrectDistType(CGlobal::distSensorType)) {
    //    is_all_ok = false;
    //}

    //if ((CGlobal::visionNotation < visionNotation_Min) || (CGlobal::visionNotation > visionNotation_Max)) {
    //    CGlobal::visionNotation = visionNotation_FivePoint;
    //    is_all_ok = false;
    //}

    // 数据传输的串口连接波特率
    bool is_ok_datatrans_serial_baud = WinDataTrans::checkSerialBaud(CGlobal::dataTransSerialBaud);
    if (!is_ok_datatrans_serial_baud) {
        is_all_ok = false;

        //CGlobal::dataTransSerialBaud = default;
    }

    // 屏幕亮度
    bool is_succ_scr_bri = CGlobal::setScreenBrightnessCfg(CGlobal::screenBrightnessCfg.getValue());
    if (!is_succ_scr_bri) {
        is_all_ok = false;
    }

    // 参考视力记录方法
    if (CGlobal::visionNotation.getValue() < visionNotation_Min || CGlobal::visionNotation.getValue() > visionNotation_Max) {
        CGlobal::visionNotation = visionNotation_None;
        is_all_ok = false;
    }

    // 小票打印连接方式
    if (!(ticketPrintConnType_WiFi == CGlobal::ticketPrintConnType) && !(ticketPrintConnType_BT == CGlobal::ticketPrintConnType)) {
        CGlobal::ticketPrintConnType = ticketPrintConnType_BT;
        is_all_ok = false;
    }

    // 兼容旧版的 languge 配置值（若没有新的语言配置项，则根据旧的语言配置项设置新的语言配置项）
    QString language_old = appSetting::value("tool/language").toString();
    QString language_new = appSetting::value("global/language").toString();
    if (language_new.isEmpty()) {
        if ("true" == language_old || language_old.length() == 0) {
            CGlobal::language = G_LANGUAGE_CHINESE;
        } else if ("false" == CGlobal::language) {
            CGlobal::language = G_LANGUAGE_ENGLISH;
        }
        is_all_ok = false;
    }

    // 测距类型  
    //if (enDistSensorType::Mb1010 != CGlobal::distSensorType) {
    //    CGlobal::distSensorType = enDistSensorType::Mb1010;
    //    is_all_ok = false;
    //}

//#endif

    //
    return is_all_ok;
}

// 重置配置
void CGlobal::restoreConfs()    // TODO: 缺省值用常量或宏声明     // TODO：把缺省值的定义放到这里来，对象构造时调用？
{
    // TODO: 实现方式改为删除配置文件，然后重启？变量本来就应该有初始化值，这里不应该再维护一次缺省值？

    //
    CGlobal::captureInterval    = 10;

    CGlobal::CGlobal::maxPupilFail      = DEFOUT_MAX_PUPIL_FAIL;        // 灰度检查失败最大次数
    CGlobal::CGlobal::maxAlgoFail       = DEFOUT_MAX_ALGO_FAIL;         // 出推值转灯次数（前两年龄段）
    CGlobal::CGlobal::pushStatisticSecs = DEFOUT_MAX_MEASURE_SECS;      // 最大测量时间（单位：秒。从灰度及瞳孔检查开始，超时后显示统计结果，适用于前两年龄段）

    CGlobal::pupilAverageMin_       = DEFAULT_PUPIL_AVERAGE_MIN     ;       // 最小瞳孔灰度          // TODO: 这个值的标准？v1.3.9 的值是 80
    CGlobal::pupilAverageMax        = DEFAULT_PUPIL_AVERAGE_MAX     ;       // 最大瞳孔灰度
    CGlobal::exposureMsMin          = DEFAULT_EXPOSURE_MS_MIN       ;       // 最小曝光时间（ms）
    CGlobal::exposureMsMax          = DEFAULT_EXPOSURE_MS_MAX       ;       // 最大曝光时间（ms）
    CGlobal::expoCoarseAdjStepMs    = DEFAULT_EXPO_COARSE_ADJ_STEP  ;       // 曝光粗调步长（ms）
    CGlobal::expoFineAdjStepMs      = DEFAULT_EXPO_COARSE_ADJ_STEP  ;       // 曝光精调步长（ms）

    CGlobal::minPupilStaDev     = 4.9;          // 最小瞳孔标准偏差

    CGlobal::algoMode           = algoMode_General;
    CGlobal::minPupilDetectedCount  = DEFAULT_MIN_PUPIL_DETECTED_COUNT;

    /** Hardware （硬件类型的设置不可重置，只能人工选择或自动识别） */
    //CGlobal::distSensorType     = enDistSensorType::Mb1010;

    //
    //CGlobal::distanceInterval   = 200;
    CGlobal::distanceInterval2   = 100;
    CGlobal::distanceUnit       = distanceUnit_cm;

    CGlobal::isStatisticalEnabled   = false;
    CGlobal::isEnableMonthAgeVision = false;

    CGlobal::isMultiMeasure         = false;

    CGlobal::defaultAgeRange        = DEFAULT_AGE_RANGE;

    CGlobal::visionNotation.reset();

    CGlobal::screenBrightnessCfg.reset();

    CGlobal::resultStableCountThresh    = 2;
    CGlobal::resultStableDiopterThresh  = 0.5;
    CGlobal::countMaxMeasureTimes       = 3;
    CGlobal::hardTriggerIntervalDelayMs = 6.5;
    CGlobal::hardTriggerDelayMs         = 2.2;
    CGlobal::triggerInputType          = enTriggerInputType::Unknown;

    CGlobal::isVoicePrompt      = false;
    CGlobal::autoScreenOff      = enAutoScreenOff::Default;
}

// =============================================================================================================================================
// 全局对象定义

/* 注意：C++ 语言源文件里的变量定义语句执行是有先后顺序的。logger 的构造方法里访问了 CGlobal::logPath，若它在该静态变量的定义前执行，则造成程序崩溃。
 * 为了确保构造下方对象时所有静态变量已初始化，在 .pro 文件中，util、logger、global 模块的引入顺序须如前所列之顺序，global 须放最后。
 */
// TODO: 这些变量，该改为指针？在 CGlobal::init() 和 CGlobal::final() 里初始化和结束化，在 main() 里调用？

#ifndef UNIT_TEST
Util::CBarcodeDataDecoder *kbReaderPtr = Q_NULLPTR;     // TODO: 定义为局部指针，通过函数供外部访问
Util::CBarcodeDataDecoder *kbReader()
{
    if (!kbReaderPtr) {
        kbReaderPtr = new Util::CBarcodeDataDecoder();
    }
    return kbReaderPtr;
}
#endif

// 当前主题
enThemeType themeType = themeType_Unknown;

enThemeType getSysThemeType(bool _is_reload) {
    if (themeType_Unknown == themeType) {
        _is_reload = true;
    }
    if (_is_reload) {
        //enThemeType theme_type = (enThemeType)appSetting::value("/tool/theme").toInt();
        enThemeType theme_type = themeType_Black;       /* 新UI的白色样式未实现，为了避免出问题，这里固定为黑色。 */
        if (theme_type < themeType_Min || theme_type > themeType_Max) {
            theme_type = themeType_Min;
        }
        if (theme_type != themeType) {
            themeType = theme_type;
        }
    }
    return themeType;
}

void setSysThemeType(enThemeType _theme_type, bool _is_save) {
    //
    if (! (_theme_type >= themeType_Min && _theme_type <= themeType_Max) ) {
        _theme_type = themeType_Min;
    }

    //
    themeType = _theme_type;
    if (_is_save) {
        //appSetting::setValue("/tool/theme", (int)themeType);
    }

    //
    //if (themeType_Black == themeType) {
    //    CBaseFormIntf::changeAppStyleSheet(":/resource/qss/black.qss");       // TODO: 未完成
    //} else if (themeType_White == themeType) {
    //    CBaseFormIntf::changeAppStyleSheet(":/resource/qss/white.qss");
    //}
}
