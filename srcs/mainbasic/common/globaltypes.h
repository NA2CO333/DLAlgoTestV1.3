#ifndef GLOBALTYPES_H
#define GLOBALTYPES_H

/*************************************************
 * 全局类型定义
 *
 * 规范：
 * 1、尽量仅引用 C/C++ 标准库？因为算法等模块可能不使用 Qt 库。若需使用 Qt 库，应放到 "globalclass.h" 。
 * 2、
 *
 */

/// =============================================================================================================================================
/// 宏定义

// PDF 保存目录（版本日期 <= 20230512 的旧版程序的 PDF 保存目录是 /usr/pdfFile ）
#define PDF_REPORT_DIR  "/media/reports"


/// =============================================================================================================================================
/// 常量定义

const char SEX_CODE_MALE    = 'M';      // 男（性别代号）
const char SEX_CODE_FEMALE  = 'F';      // 女（性别代号）


/// =============================================================================================================================================
/// 其它全局类型

// 年龄段
enum enAgeRange {
    ageRange_Invalid            = -1,   // 未知/非法值
    ageRange_0_06_12_MONTH,
    ageRange_1_01_03_YEAR,
    ageRange_2_03_06_YEAR,
    ageRange_3_06_20_YEAR,
    ageRange_4_20_100_YEAE,

    ageRange_Min        = ageRange_0_06_12_MONTH,
    ageRange_Max        = ageRange_4_20_100_YEAE,
};

// 相机类型
enum enCameraType {
    cameraType_D3T_M3ST130M,

    cameraType_Min              = cameraType_D3T_M3ST130M,
    cameraType_Max              = cameraType_D3T_M3ST130M,
};

// 单双眼模式        // NOTE: 支持通过按位与运算来判断是否包含某只眼，比如 bool has_right = (singleDualEyeMode_Right & _single_dual_eye)
enum enSingleDualEyeMode {
    singleDualEyeMode_Right     = 1,    // 右眼模式
    singleDualEyeMode_Left      = 2,    // 左眼模式
    singleDualEyeMode_Both      = 3,    // 双眼模式

    singleDualEyeMode_Min       = singleDualEyeMode_Right,    // 最小值（用于值的合法性检查）
    singleDualEyeMode_Max       = singleDualEyeMode_Both,
};

// 光路类型（对应、替换 v1.3、1.4 旧代码的"版本类型"）     // TODO: 整理
enum enOpticalPathType {
    opticalPathType_General,        // 常规（直线的）
    opticalPathType_Square,         // 方形
    opticalPathType_LShape,         // L形
};

// 参考视力的显示类型（视力记录法）
enum enVisionNotation {
    visionNotation_None = 0,        // 不显示
    visionNotation_FivePoint,       // 显示为“五分制”
    visionNotation_Decimal,         // 显示为“小数制”

    visionNotation_Min = visionNotation_None,
    visionNotation_Max = visionNotation_Decimal,
};

// 性别
enum enSex {
    sex_Male        = 0,        // 男
    sex_Fefale  ,               // 女

    sex_Min = sex_Male,
    sex_Max = sex_Fefale,
};

// 系统信号     // TODO: 这是根据旧代码定义的整型变量定义的，待从全局结构梳理检查优化
enum enSysSignal {
    sysSignal_01                    = 1,
    sysSignal_02                    = 2,
    sysSignal_03                    = 3,
    sysSignal_04                    = 4,
    sysSignal_05                    = 5,
    sysSignal_06                    = 6,
    sysSignal_07                    = 7,
    sysSignal_08                    = 8,
    sysSignal_09                    = 9,
    sysSignal_10                    = 10,
    sysSignal_11                    = 11,
    sysSignal_12                    = 12,
    sysSignal_13                    = 13,
    sysSignal_14                    = 14,
    sysSignal_15                    = 15,
    sysSignal_16                    = 16,
    sysSignal_17                    = 17,
    sysSignal_18                    = 18,
    sysSignal_19                    = 19,
    sysSignal_20                    = 20,
    sysSignal_21                    = 21,
    sysSignal_22                    = 22,

    sysSignal_PowerOffPressed       = 23,       // 关机按钮被按下

    sysSignal_24                    = 24,

    sysSignal_ChargingOn            = 25,       // 直充被插入
    sysSignal_ChargingOff           = 26,       // 直充被拔出
    sysSignal_ChargingFull          = 226,      // 直充已充满

    //sysSignal_MusicOn               = 27,       // 打开音乐
    //sysSignal_MusicOff              = 28,       // 关闭音乐

    sysSignal_BtPowerClosed         = 29,       // 蓝牙电源已关闭（底板的蓝牙模块）
    sysSignal_BtConnected           = 30,       // 蓝牙已连接（底板的蓝牙模块）

    sysSignal_PowerOffCommand       = 40,       // 外部控制系统的关机命令

    sysSignal_WifiScanOff           = 100,      // 关闭 WiFi 定时扫描（旧代码中这个信号是发送往 WiFi 管理窗体）
    sysSignal_WifiScanOn            = 101,      // 开启 WiFi 定时扫描（旧代码中这个信号是发送往 WiFi 管理窗体）

};

// 产品型号
enum enProductModel {
    productModel_SL100S,        // "SL-100S"
    productModel_SL100,         // "SL-100"
    productModel_SL100P,        // "SL-100P"
};

// 二维码内容的类型
enum enQrCodeType {
    qrCodeType_Unknown  = -1,   // 未知 或 解析失败
    qrCodeType_JSON     = 1,    // JSON
    qrCodeType_CSV      = 2,    // CSV
    qrCodeType_Number   = 3,    // 仅被测者编号
};

// 被测者信息来源
enum enPatientSource {
    patientSource_Unknown,      // 未知
    patientSource_Manual,       // 手动新建
    patientSource_Database,     // 数据库
    patientSource_Scanning,     // 扫码
    patientSource_Command,      // 通信指令
};

// 触发输入类型
enum class enTriggerInputType {
    Unknown         = -1,       // 未知
    FallingEdge     = 0,        // 下降沿触发
    RisingEdge      ,           // 上升沿触发
};

#endif // GLOBALTYPES_H
