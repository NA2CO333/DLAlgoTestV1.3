#ifndef EXPOSUREADJUSTER_H
#define EXPOSUREADJUSTER_H

#ifndef ENABLE_EXPOSURE_TIMING_LOG
// 预览曝光耗时汇总日志独立控制，默认开启便于现场测量。
#define ENABLE_EXPOSURE_TIMING_LOG 1
#endif

#include <chrono>
#include <QObject>

// 曝光时间调整
class CExposureAdjuster : public QObject
{
    Q_OBJECT

public:
    explicit CExposureAdjuster(QObject *parent = 0);
    ~CExposureAdjuster();

    void reset();                       // 重置
    void setIsFixed(bool _is_yes);      // 设置是否固定曝光时间

    int defaultExposureUs();            // 获取默认的曝光时间（单位 us）    // NOTE: 注意：以毫秒为单位的曝光时间仅用于UI，方便用户识别，程序内部的控制，统一用微秒！

    /**
     * @brief 数据输入：图像曝光信息（并输出新的曝光时间）
     * @param _is_pupil_succ    瞳孔检测是否成功个
     * @param _avg_gray
     * @param _is_over_expo
     * @param _expo_us_curr     当前曝光时间（us）
     * @return 若返回值大于0，则为新的曝光时间。若返回-1，则表示本次不需调整曝光时间
     */
    int inputExposureInfo(bool _is_pupil_succ, int _avg_gray, bool _is_over_expo, int _expo_us_curr);

    bool getIsFinished();                               // 曝光时间的调节是否已经完成

    // 记录一次相机曝光参数下发耗时，最终并入本次曝光汇总日志。
    void recordExposureCommandTime(double _elapsed_ms,
                                   int _initial_exposure_us,
                                   int _final_exposure_us);

    // 结束本次曝光计时会话，保证同一会话最多输出一次汇总日志。
    void finishExposureTiming(const char *_result, int _final_exposure_us);

signals:
    void sigIsExposureOk();                     // 【曝光时间 OK】
    void sigMsgNotify(QString _msg);            // 【消息通知】

protected:
    static const char * const S_CLASS_NAME;     // 本类的类名

    // 曝光信息
    struct stExpoInfo {
        int expoUs {0};     // 曝光时间（us）
        int gray {0};       // 灰度
    };

    bool m_isGrayStable = false;                    // 是否灰度稳定（曝光时间不变时，用于减少眨眼期间，或者瞳孔识别错误时误触发『调光完成』事件）
    bool m_isFinished = false;                      // 曝光调节是否已完成
    bool m_isFixed = false;                         // 是否固定曝光时间
    QList<stExpoInfo> m_expoHistory;                // 曝光历史

    int m_countTotal = 0;                           // 总调节次数

    // 记录本次程序运行期间是否已经获得过可靠曝光；reset()不能清除该状态。
    bool m_hasReliableExposure = false;
    bool m_isInitialOptimizationActive = false;
    int m_initialCandidateIndex = 0;                // 下一个候选曝光在序列中的位置
    QList<int> m_initialCandidateValuesTried;       // 本次快速搜索已经反馈过的候选值
    int m_initialCandidateTryCount = 0;             // 首次快速模式已反馈的候选次数
    int m_proportionalAdjustCount = 0;               // 比例粗调次数
    int m_fineAdjustCount = 0;                       // 固定步长精调次数

#if ENABLE_EXPOSURE_TIMING_LOG
    bool m_exposureTimingActive = false;             // 是否存在正在统计的曝光会话
    bool m_exposureTimingReported = false;           // 是否已经输出过汇总日志

    std::chrono::steady_clock::time_point m_previewStartedAt;
    std::chrono::steady_clock::time_point m_adjustmentStartedAt;

    int m_sampleCount = 0;                           // 曝光反馈采样次数
    int m_adjustmentCount = 0;                       // 实际下发新曝光值次数

    int m_initialExposure = -1;
    int m_finalExposure = -1;
    int m_initialGray = -1;
    int m_finalGray = -1;

    bool m_initialFastMode = false;

    double m_algorithmTotalMs = 0.0;                 // 曝光计算函数耗时总和
    double m_setExposureTotalMs = 0.0;               // 相机曝光参数下发耗时总和
#endif

// ==================================================================================================
//
//    int findExpoInfoAscending(int _expo);
//    int findClosestHigherExpo(int _expo);
//
//    // 设置曝光时间范围
//    void setExposureTimeRange(int minValue, int maxValue) {
//        if (minValue <= maxValue) {
//            exposureTimeMin = minValue;
//            exposureTimeMax = maxValue;
//        } else {
//            // 可选的错误处理：交换值或抛出异常
//            exposureTimeMin = maxValue;
//            exposureTimeMax = minValue;
//        }
//    }
//
//    // 获取曝光时间最小值
//    int getExposureTimeMin() const {
//        return exposureTimeMin;
//    }
//
//    // 获取曝光时间最大值
//    int getExposureTimeMax() const {
//        return exposureTimeMax;
//    }
//
//    // 设置目标灰度范围
//    void setTargetGreyRange(int minValue, int maxValue) {
//        if (minValue <= maxValue) {
//            targetGreyMin = minValue;
//            targetGreyMax = maxValue;
//        } else {
//            // 可选的错误处理：交换值或抛出异常
//            targetGreyMin = maxValue;
//            targetGreyMax = minValue;
//        }
//    }
//
//    // 获取目标灰度最小值
//    int getTargetGreyMin() const {
//        return targetGreyMin;
//    }
//
//    // 获取目标灰度最大值
//    int getTargetGreyMax() const {
//        return targetGreyMax;
//    }
//
//private:
//    // 新增：二分查找区间状态（改为成员变量，避免静态局部变量污染）
//    int m_search_low;
//    int m_search_high;
//
//    // 曝光时间物理限制
//    int exposureTimeMax = 16000;    // 最大曝光（微秒）
//    int exposureTimeMin = 1000;     // 最小曝光（微秒）
//
//    // 目标灰度范围控制（新增：可配置的目标区间）
//    int targetGreyMin = 100;
//    int targetGreyMax = 160;
//    // ========== 新增：阶段切换阈值 ==========
//    // 误差超过目标范围的倍数时退回粗调
//    static constexpr int FINE_TO_GRADIENT_THRESHOLD = 3;
};

#endif // EXPOSUREADJUSTER_H
