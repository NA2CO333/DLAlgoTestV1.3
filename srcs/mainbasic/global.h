#ifndef GLOBAL_H
#define GLOBAL_H

//#define UNIT_TEST
//#define TEST_SAVE_IMG

#include "globaltypes.h"
#include "util-common.h"
#include "logger.h"

#include "distancedetect.h"
#include "keyboardreader.h"
#include "appsetting.h"
#include "tool.h"
#include "printersetting.h"

#include "algointf.h"

#include "authintf.h"

/***************************************************************
 * 全局配置模块
 * 存放本程序的全局配置值的内存数据读写和永久保存的读写代码。
 * global 模块和普通功能模块的引用规范：
 * （1）属于某个模块的专有类型，应尽量定义在该模块的 .h 文件中，所以 global.h 难免要包含普通功能模块的 .h 文件。
 * （2）普通功能模块的 .h 文件文件不可包含 global.h，只能包含 globaltypes.h，否则可能导致头文件交叉包含而发生“类型未定义”错误。
 * （3）global.h 中不可定义类型，如果是全局类型，须定义在 globaltypes.h 中。
 */

// TODO: “全局”模块应进一步细分？

// TODO: float 类型保存到 .ini 时全部是 @Variant(xxx) 格式？应该一律使用 double 类型？


// 默认日期格式
#define DEFAULT_DATE_FORMAT     "yyyy-MM-dd"

// 默认时间格式（到分钟）
//#define DEFAULT_TIME_FORMAT_M   "hh:mm"       /* 这种非标准的格式，不应保存到数据库，个别的地方要用到时，在模块内定义 */
// 默认时间格式（到秒）
#define DEFAULT_TIME_FORMAT_S   "hh:mm:ss"
// 默认时间格式（到毫秒）
#define DEFAULT_TIME_FORMAT_Z   "hh:mm:ss.zzz"

//
// 底板串口路径
extern const char *G_COM_BASEBOARD;
// 蓝牙串口路径
extern const char *G_COM_BLUETOOTH;
// 测距通信串口路径
extern const char *G_COM_DISTANCE;

// 语言的标识字符串
constexpr char G_LANGUAGE_ENGLISH[] = "english";    // 英文
constexpr char G_LANGUAGE_CHINESE[] = "chinese";    // 中文
constexpr char G_LANGUAGE_GERMAN[]  = "german";     // 德文

extern const char *G_LANGUAGE_DEFAULT;              // 缺省语言

/**
 * 全局变量值的新增：
 * 1、在 .h 文件新增静态变量声明。
 * 2、在 .cpp 文件新增静态变量的定义及初始值。最好再新增用常量定义缺省值，并用该常量初始化变量。
 * 3、分别在 loadConfs()、saveConfs()、restoreConfs() 函数内新增该变量的相关操作。
 */

/**
 * 在内部设置界面增加设置控件：
 * 1、添加控件。
 * 2、分别在 loadValues()、saveValues()、restoreValues() 添加该变量的相关操作。
 */

// TODO: 全局变量全部改为 get set 形式？
// TODO: Config 模块的重设计及优化：字符串只需写一次，避免出错；使编译器能进行名称检查；增强自动化，减少新增变量时的工作量；
//
class CGlobal
{
public:
    static QString logPath;

    // 常量       // TODO: 这些都是配置变量，封装到 config 类？
    static const char * const LOG_ALL;          // 保留所有消息
    static const char * const LOG_TEMP;         // 滤掉所有消息
    static const char * const LOG_SYS;          // 系统消息
    static const char * const LOG_BASEBOARD;    // 底板消息
    static const char * const LOG_DISTANCE;     // 测距消息
    static const char * const LOG_MEASURE;      // 测量控制消息
    static const char * const LOG_CAPTURE;      // 抓图消息
    static const char * const LOG_ALGO;         // 算法消息
    static const char * const LOG_BLUETOOTH;    // 蓝牙消息
    static const char * const LOG_WIFI;         // WIFI消息
    static const char * const LOG_DATATRANS;    // 通信消息
    static const char * const LOG_DATABASE;     // 数据库操作
    static const char * const LOG_SERIAL;       // 串口

    //
    static void init();

    // 变量
    static QString language;                // 语言（与翻译文件名称关键字一致，见 G_LANGUAGE_xxx）

    static QString langCode();              // 语言代码

    static int captureInterval;             // 连拍间隔（秒数）

    static enDistSensorType distSensorType; // 测距模块类型       // NOTE: 此为配置值，运行时实际值由 CDistanceDetect::sensorType() 确定
    //static int distanceInterval;            // 测距时间间隔     // NOTE: 2026-02-28 以前测距间隔设置过长，现须缩短测距间隔，所以废弃旧配置项，改用新的缺省间隔 100ms 的配置项
    static int distanceInterval2;           // 测距时间间隔
    static enDistanceUnit distanceUnit;     // 距离单位

    static bool isDebugMode;                // 是否调试模式

    static bool isStatisticalEnabled;       // 允许出推值（统计值）

    static bool isEnableMonthAgeVision;                         // 是否启用“按月龄推值”
    static bool getIsNeedMonthAgeVision(int _age_range);        // 判断是否要使用月龄估值

    static bool isMultiMeasure;             // 是否多次测量（计算3次出均值）

    static int minPupilAreaRD;              // 最小瞳孔区域面积（前两年龄段）  // RD: reduced dimension
    static int minPupilRectSideRD;          // 最小瞳孔区域边长
    static int minPupilRadius;              // 最小瞳孔半径
    static int minPupilArea;

    static int getMinPupilParamRatio();
    static void setMinPupilParamRatio(int _ratio);

    static int maxPupilFail;                // 灰度检查失败最大次数
    static int maxAlgoFail;                 // 出推值转灯次数（前两年龄段）
    static int pushStatisticSecs;           // 出推值限时（单位：秒。从检测到瞳孔后开始，超时后显示统计结果，仅适用于前两年龄段）

    static bool isReducedVersion;           // 是否缩减版    // 已取消该选项，一律非缩减版（见《视筛功能更新需求》 刘宇 2023-09-06）

    static enProductModel productModel;     // 产品型号

    static int pupilAverageMin_;            // 最小瞳孔灰度
    static int pupilAverageMax;             // 最大瞳孔灰度
    static double exposureMsMin;            // 最小曝光时间（ms）   // NOTE: 注意：以毫秒为单位的曝光时间仅用于UI，方便用户识别，程序内部的控制，统一用微秒！
    static double exposureMsMax;            // 最大曝光时间（ms）
    static double expoCoarseAdjStepMs;      // 曝光粗调步长（ms）   // NOTE: 注意：以毫秒为单位的曝光时间仅用于UI，方便用户识别，程序内部的控制，统一用微秒！
    static double expoFineAdjStepMs;        // 曝光精调步长（ms）

    static float minPupilStaDev;            // 最小瞳孔标准偏差

    static bool isPupilAccutrately;

    static int algoSaveImgIndex;        // 算法调试所用，设置需要保存的图像的索引号

    static bool isReadBarcodeByQt;      // 是否通过 Qt 事件循环读取条码值（若否，则用 Util::CKeyboardReader 获得条码输入）

    /* v1.3.9 的“专业模式”和“普通模式”定义在 setting.cpp -> int Screen_model，据反馈“专业模式有问题”，改为一律使用“普通模式”(const Screen_model=1)。而下面定义的是新的模式变量： */
    static enAlgoMode algoMode;         // 算法模式

    static int minPupilDetectedCount;   // 瞳孔识别最小次数（出推值）

    static QString pupilAlgoVerDesc[];  // 瞳孔识别算法版本的描述

    static double ultraCoefficient;     // 新超声读数 转为 旧超声读数 的换算比例（即：新超声距离值 = 按旧算法算得的距离值 * 换算比例）

    static int distanceOffset;                  // 距离补偿(mm)（旧变量名 distanceFix ）
    static int distTolerance;                   // 距离允差（mm）
    static bool isInvertImg;                    // 是否倒转图像

    //
    static bool isSetLedLevel;                  // 是否设置 LED 电流等级
    static int ledLevelMiddle;                  // 中心 LED 电流等级
    static int ledLevelEccentric;               // 偏心 LED 电流等级

    static bool isAutoTurnLampWhenSelfControl;          // 自控模式时是否自动转灯
    static bool isAutoTurnLampWhenExternalControl;      // 受控模式时是否自动转灯

    static bool isMagnifyPupilImg;              // 是否放大瞳孔图像（预检阶段）

    static CVisionNotation visionNotation;      // 参考视力的显示类型

    static enAgeRange defaultAgeRange;          // 缺省年龄段（用户没有指定年龄段而开始测量时所用的年龄段）

    static int dataTransSerialBaud;             // 数据传输串口连接的波特率

    static int frameRate;                       // 帧率（取景时）

    static bool isSpecifiedAlgo;                // 是否指定算法

    static bool isSingleThreadCalc;             // 是否单线程计算屈光度数

    static int maxGazeDeviation;                // 最大固视偏差（°）

    static bool isSimulatedEye;                 // 是否模拟眼

    static QString organizationName;            // 机构名称（小票的）
    static QString orgNameA4;                   // 机构名称（A4报告的）
    static QString operatorName;                // 操作者

    static CAuthIntf::enAuthType authType;      // 授权类型（试用机状态）
    static QDate authExpiryDate;                // 授权到期日（试用机到期日）

    static bool isDevActivated;                 // 设备是否已激活

    static QString devNum;                      // 产品编号

    static bool isAutoPrintTicket;                          // 是否自动打印小票
    static enTicketPrintConnType ticketPrintConnType;       // 小票打印的连接类型

    static bool getIsExternalControl(bool _internal_stat_only = false);     // 是否外部控制（受控模式）。@param _internal_stat_only 是否仅用内部状态变量
    static void setIsExternalControl(bool _is_external);

    static float resultCorrectSph_General;      // 结果修正_球镜度_常规光路
    static float resultCorrectCyl_General;      // 结果修正_柱镜度_常规光路
    static float resultCorrectSph_Square;       // 结果修正_球镜度_方形视筛箱
    static float resultCorrectCyl_Square;       // 结果修正_柱镜度_方形视筛箱
    static float resultCorrectSph_LSharp;       // 结果修正_球镜度_L形视筛箱
    static float resultCorrectCyl_LSharp;       // 结果修正_柱镜度_L形视筛箱

    static float eyeWhRatio_Model;              // 模拟眼瞳孔长宽比     // 2024-06-07 增加瞳孔长宽比的设置，提供给算法。需求来源：刘宇 2024-06-06
    static float eyeWhRatio_Human;              // 人眼瞳孔长宽比

    static int resultStableCountThresh;         // 结果稳定次数阈值（达到此次数后认为已稳定）
    static double resultStableDiopterThresh;    // 结果稳定度数阈值（屈光度数浮动范围在此范围内认为已稳定）
    static int countMaxMeasureTimes;            // 最大测量次数
    static double hardTriggerIntervalDelayMs;   // 硬触发间隔附加延时（毫秒）（硬触发间隔 = 曝光时间 + 附加延时）    // NOTE: 若为0，则使用默认触发延迟
    static double hardTriggerDelayMs;           // 硬触发延时（毫秒）   // NOTE: 2025-12-24：由下降沿触发改为上升沿触发后，根据下位机程序流程，需要补上触发延时
    static enTriggerInputType triggerInputType;     // 触发输入类型

    static bool isVoicePrompt;                  // 是否语音提示   // NOTE: 音乐和语音是互斥的，即若已开启了“语音提示”，则不再播放音乐

    static enAutoScreenOff autoScreenOff;       // 自动息屏

    //
    static void loadConfs();        // 载入配置
    static void saveConfs();        // 保存配置
    static bool checkConfs();       // 检查修正配置
    static void restoreConfs();     // 重置配置

    //
    static enAlgoVerAll getPupilAlgoVerCfg();                      // 获得算法版本的配置值
    static void setPupilAlgoVer(enAlgoVerAll _pupil_algo_ver);     // 设置算法版本的配置值
    static void judgeAndSetPupilAlgoVer(int _age_range);        // 设置算法版本选定所依据的年龄段
    static enAlgoVerAll getCurrentPupilAlgoVer();                  // 获得当前算法版本（由配置值、年龄段、是否强制指定能因素决定）

    static bool getIsLogEnabled();      // 是否启用 Log
    static void setIsLogEnabled(bool _enabled);
    static bool getIsLogToFile();
    static void setIsLogToFile(bool _enabled);
    static enLogLevel getLogLevel();
    static void setLogLevel(enLogLevel _log_level);
    static void setFilter(QString _tag);
    static QString getFilter();

    static enScreenBrightness getScreenBrightnessCfg();                 // 获得屏幕亮度（百分比的分子）的配置值
    static bool setScreenBrightnessCfg(const enScreenBrightness &_brightness);

    static bool getIsAutoTurnLamp();            // 获得“是否自动转灯”

    static bool getIsMusicEnabled();            // 是否启用音乐功能（是产品特性，而不是用户设置，区别于测量模块的开关）   // NOTE: 音乐和语音用同一个【是否可用】开关，且音乐和语音是互斥的，即若开启了“语音提示”，则不再播放音乐
    static void setIsMusicEnabled(bool _enabled);
    static bool getIsColoredLampEnabled();      // 是否启用彩灯功能（是产品特性，而不是用户设置，区别于测量模块的开关）
    static void setIsColoredLampEnabled(bool _enabled);

    static const QString &pathConfig();         // 配置目录路径

protected:
    static int minPupilParamRatio;              // 最小瞳孔参数比例（百分比数）

    static enAlgoVerAll pupilAlgoVerCfg;        // 瞳孔识别算法版本（用户设定的）
    static enAlgoVerAll currentPupilAlgoVer;    // 瞳孔识别算法版本（根据业务逻辑决定的）

    static CScreenBrightness screenBrightnessCfg;               // 屏幕亮度（百分比的分子）

    static bool m_isExternalControl;            // 是否外部控制（受控模式）

    static bool m_isMusicEnabled;
    static bool m_isColoredLampEnabled;

    static QString s_pathConfig;                // 配置文件夹路径

    static bool s_isLogToFile;                  // 是否输出日志到文件

};

/// =============================================================================
/// 全局变量

#ifndef UNIT_TEST
Util::CBarcodeDataDecoder *kbReader();
#endif

//
enThemeType getSysThemeType(bool _is_reload = false);                           // 获取系统的主题
void setSysThemeType(enThemeType _theme_type, bool _is_save = false);           // 设置系统的主题


#endif // GLOBAL_H
