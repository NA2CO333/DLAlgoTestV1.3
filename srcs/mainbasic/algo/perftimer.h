#ifndef PERFTIMER_H
#define PERFTIMER_H

#include <QDebug>

#ifndef PERF_LOG
#define PERF_LOG 0
#endif

// code_DL_merge_test专用诊断总开关；只影响日志和耗时观测，不参与算法决策。
#ifndef ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#define ENABLE_DL_MERGE_TEST_DIAGNOSTICS 1
#endif

// 逐帧高频日志默认关闭，必要时可在测试副本中临时改为1。
#ifndef ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#define ENABLE_DL_MERGE_TEST_FULL_VERBOSE 0
#endif

// 统一的算法详细日志开关；正常构建关闭，现场需要逐项排查时再打开。
#ifndef ENABLE_ALGO_VERBOSE_LOG
#define ENABLE_ALGO_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

// 正式版关键日志：只输出模型初始化、每轮摘要和最终结果。
#ifndef ENABLE_ALGO_KEY_LOG
#define ENABLE_ALGO_KEY_LOG 1
#endif

// 测试副本保留算法内部的诊断qDebug；逐帧高频入口仍由各自日志宏控制。
#ifndef ENABLE_ALGO_DEBUG_LOG
#define ENABLE_ALGO_DEBUG_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

// 测试副本默认开启分阶段耗时统计。
#ifndef ENABLE_ALGO_TIMING_LOG
#define ENABLE_ALGO_TIMING_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

// 严重异常日志：正式版保持开启。
#ifndef ENABLE_ALGO_ERROR_LOG
#define ENABLE_ALGO_ERROR_LOG 1
#endif

// 预览和转灯逐帧日志只由完整高频开关控制，不能被耗时开关反向打开。
#ifndef ENABLE_PREVIEW_VERBOSE_LOG
#define ENABLE_PREVIEW_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

#ifndef ENABLE_PREVIEW_DIAG_LOG
#define ENABLE_PREVIEW_DIAG_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

#ifndef ENABLE_PREVIEW_FRAME_VERBOSE_LOG
#define ENABLE_PREVIEW_FRAME_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

#ifndef ENABLE_PREVIEW_EXPOSURE_VERBOSE_LOG
#define ENABLE_PREVIEW_EXPOSURE_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

#ifndef ENABLE_PREVIEW_PUPIL_FAIL_DETAIL_LOG
#define ENABLE_PREVIEW_PUPIL_FAIL_DETAIL_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

#ifndef ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG
#define ENABLE_TURN_LAMP_PUPIL_FAIL_DETAIL_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

#ifndef ENABLE_TURN_LAMP_PUPIL_FAIL_SUMMARY_LOG
#define ENABLE_TURN_LAMP_PUPIL_FAIL_SUMMARY_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

#ifndef ENABLE_REFRACTION_POLICY_VERBOSE_LOG
#define ENABLE_REFRACTION_POLICY_VERBOSE_LOG 0
#endif

#ifndef ENABLE_REFRACTION_QUALITY_LOG
#define ENABLE_REFRACTION_QUALITY_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

#ifndef ENABLE_ALGO_ROUND_DIAG_LOG
#define ENABLE_ALGO_ROUND_DIAG_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

#ifndef ENABLE_PREVIEW_WEAK_GATE_VERBOSE_LOG
#define ENABLE_PREVIEW_WEAK_GATE_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

#ifndef ENABLE_ALGO_APPEND_VERBOSE_LOG
#define ENABLE_ALGO_APPEND_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

// 达到最大记录轮数后是否停止继续转灯：
// 0 = 不限制，继续按当前流程转灯，用于观察自然会转几轮；1 = 达到 maxRecordCount 后停住并异步等待结果。
#ifndef ENABLE_STOP_TURNLAMP_AT_MAX_RECORD_COUNT
#define ENABLE_STOP_TURNLAMP_AT_MAX_RECORD_COUNT 0
#endif

// 测试副本默认开启低频关键流程日志；逐帧细节仍由完整高频开关控制。
#ifndef ENABLE_PREVIEW_KEY_FLOW_LOG
#define ENABLE_PREVIEW_KEY_FLOW_LOG ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#endif

// 关键日志：正式版保留。
#if ENABLE_ALGO_KEY_LOG
#define ALGO_KEY_LOG(statement) do { statement; } while (0)
#else
#define ALGO_KEY_LOG(statement) do { } while (0)
#endif

// 详细日志：正式版在编译期完全移除。
#if ENABLE_ALGO_DEBUG_LOG
#define ALGO_DEBUG_LOG(statement) do { statement; } while (0)
#else
#define ALGO_DEBUG_LOG(statement) do { } while (0)
#endif

// 兼容历史算法文件中的裸qDebug调用；正式构建将其编译为无输出对象，
// 避免旧的逐图/逐眼调试语句绕过统一日志开关。qWarning不能在这里全局替换，
// 异常输出必须由ALGO_ERROR_LOG或对应的错误日志宏显式控制。
#if !ENABLE_ALGO_DEBUG_LOG
#ifdef qDebug
#undef qDebug
#endif
#define qDebug() QNoDebug()
#endif

// 严重异常日志：正式版保留。
#if ENABLE_ALGO_ERROR_LOG
#define ALGO_ERROR_LOG(statement) do { statement; } while (0)
#else
#define ALGO_ERROR_LOG(statement) do { } while (0)
#endif

// 测试副本的低频诊断日志宏；只输出观测信息，不改变任何状态或返回值。
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#define DL_MERGE_TEST_DIAG_LOG(statement) do { statement; } while (0)
#else
#define DL_MERGE_TEST_DIAG_LOG(statement) do { } while (0)
#endif

#if PERF_LOG

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <limits>
#include <QDebug>
#include <QString>

typedef std::chrono::steady_clock Clock;
typedef std::chrono::time_point<Clock> TimePoint;

class PerfTimer {
    std::string m_name;
    TimePoint m_start;

    struct Stats {
        int64_t total_ns;
        int count;
        int64_t min_ns;
        int64_t max_ns;

        Stats() : total_ns(0), count(0),
                  min_ns(std::numeric_limits<int64_t>::max()),
                  max_ns(0) {}
    };

public:
    typedef std::unordered_map<std::string, Stats> StatsMap;

private:
    static StatsMap s_stats;  // 声明（定义在外部）

public:
    explicit PerfTimer(const std::string& name);
    ~PerfTimer();
    static void printStats();
    static void printOnce(const std::string& name, int64_t ns);
};

#define PERF_SCOPE(name) PerfTimer _perf_timer_##__LINE__(name)
#define PERF_POINT(name) \
    { \
        auto now = std::chrono::system_clock::now(); \
        auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()); \
        auto total_seconds = total_ms.count() / 1000; \
        \
        qDebug().nospace() << "[PERF_POINT] " \
            << QStringLiteral("%1:%2:%3.%4") \
                .arg((total_seconds / 3600) % 24, 2, 10, QChar('0')) \
                .arg((total_seconds / 60) % 60, 2, 10, QChar('0')) \
                .arg(total_seconds % 60, 2, 10, QChar('0')) \
                .arg(total_ms.count() % 1000, 3, 10, QChar('0')) \
            << " " << name; \
    }

#define PERF_START(var) TimePoint _perf_start_##var = Clock::now()
#define PERF_END(var, name) do { \
    TimePoint _perf_end_##var = Clock::now(); \
    int64_t _perf_elapsed_##var = std::chrono::duration_cast<std::chrono::nanoseconds>( \
        _perf_end_##var - _perf_start_##var).count(); \
    PerfTimer::printOnce(name, _perf_elapsed_##var); \
} while(0)

#else  // PERF_LOG == 0

#define PERF_SCOPE(name) ((void)0)
#define PERF_POINT(name) ((void)0)
#define PERF_START(var) ((void)0)
#define PERF_END(var, name) ((void)0)

#endif  // PERF_LOG

#if ENABLE_ALGO_TIMING_LOG

#include <atomic>
#include <chrono>
#include <cstdint>

enum AlgoTimingPhase {
    AlgoTimingPhase_None = 0,
    AlgoTimingPhase_Preview,
    AlgoTimingPhase_Formal
};

enum AlgoTimingStage {
    AlgoTimingStage_ImageClone = 0,
    AlgoTimingStage_EyeDetectTotal,
    AlgoTimingStage_EyeResize,
    AlgoTimingStage_EyePreprocess,
    AlgoTimingStage_HaarNormal,
    AlgoTimingStage_HaarEqualized,
    AlgoTimingStage_HaarRelaxed,
    AlgoTimingStage_HaarCascadeAccess,
    AlgoTimingStage_EyeRectBuild,
    AlgoTimingStage_PupilRight,
    AlgoTimingStage_PupilLeft,
    AlgoTimingStage_PupilContour,
    AlgoTimingStage_LowFallbackRight,
    AlgoTimingStage_LowFallbackLeft,
    AlgoTimingStage_HalfFallbackRight,
    AlgoTimingStage_HalfFallbackLeft,
    AlgoTimingStage_PreviewGate,
    AlgoTimingStage_ExposureCalc,
    AlgoTimingStage_AppendClone,
    AlgoTimingStage_QueueWait,
    AlgoTimingStage_FormalTaskTotal,
    AlgoTimingStage_DetectPupilTotal,
    AlgoTimingStage_AccuratePupil,
    AlgoTimingStage_ProcessRight,
    AlgoTimingStage_ProcessLeft,
    AlgoTimingStage_RoiCropRotate,
    AlgoTimingStage_MaskCopy,
    AlgoTimingStage_GlintProcess,
    AlgoTimingStage_ProcessPupilRoi,
    AlgoTimingStage_CompareEach,
    AlgoTimingStage_DsBuild,
    AlgoTimingStage_DsPatch,
    AlgoTimingStage_RefractionRight,
    AlgoTimingStage_RefractionLeft,
    AlgoTimingStage_ResultPostProcess,
    AlgoTimingStage_Policy,
    AlgoTimingStage_RoundSettlement,
    // 深度学习正式流程的分阶段耗时；只统计，不参与算法决策。
    AlgoTimingStage_C800Total,
    AlgoTimingStage_C800Preprocess,
    AlgoTimingStage_C800Forward,
    AlgoTimingStage_C800Postprocess,
    AlgoTimingStage_FormalSmallResize,
    AlgoTimingStage_FormalSmallMatch,
    AlgoTimingStage_Roi129Right,
    AlgoTimingStage_Roi129Left,
    AlgoTimingStage_Count
};

enum AlgoTimingEvent {
    AlgoTimingEvent_PreviewSuccess = 0,
    AlgoTimingEvent_PreviewFailure,
    AlgoTimingEvent_LowFallbackSuccess,
    AlgoTimingEvent_HalfFallbackSuccess,
    AlgoTimingEvent_FormalPupilSuccess,
    AlgoTimingEvent_FormalPupilFailure,
    AlgoTimingEvent_FormalSmallMatchSuccess,
    AlgoTimingEvent_FormalSmallMatchFailure,
    AlgoTimingEvent_FormalRoi129Success,
    AlgoTimingEvent_FormalRoi129Failure,
    AlgoTimingEvent_ProcessSuccess,
    AlgoTimingEvent_ProcessFailure,
    AlgoTimingEvent_Count
};

class AlgoTiming
{
public:
    static const int MAX_TIMING_ROUNDS = 20;

    static int64_t nowNs();
    static void setContext(AlgoTimingPhase phase, int roundIdx);
    static AlgoTimingPhase currentPhase();
    static int currentRound();

    static void record(AlgoTimingStage stage, int64_t elapsedNs);
    static void record(AlgoTimingStage stage, int64_t elapsedNs,
                       AlgoTimingPhase phase, int roundIdx);
    // 将已有的毫秒结果统一转换为纳秒统计，避免重复实现统计逻辑。
    static void recordMilliseconds(AlgoTimingStage stage, double elapsedMs);
    static void event(AlgoTimingEvent event);
    // 只记录正式照片任务并发峰值，最终在整次测量摘要中输出。
    static void recordFormalPhotoTasksInFlight(int count);
    static int maxFormalPhotoTasksInFlight();

    static void beginMeasurement();
    static void beginRound(int roundIdx);
    static void markFormalFrame(int roundIdx);
    // 正式轮次物理采集结束后调用；即使存在缺失槽位，也用于区分采集结束与算法结束。
    static void markFormalRoundCaptureComplete(int roundIdx);
    static void completePreviewFrame(bool success);
    static void printRoundSummary(int roundIdx, const char* resultState);
    static void printMeasurementSummary(int roundCount, int formalFrameCount,
                                        bool finished);
};

class AlgoTimingContextScope
{
public:
    AlgoTimingContextScope(AlgoTimingPhase phase, int roundIdx);
    ~AlgoTimingContextScope();

private:
    AlgoTimingPhase m_previousPhase;
    int m_previousRound;
};

class AlgoTimingScope
{
public:
    explicit AlgoTimingScope(AlgoTimingStage stage);
    ~AlgoTimingScope();
    void stop();

private:
    AlgoTimingStage m_stage;
    int64_t m_startNs;
    bool m_stopped;
};

#define ALGO_TIMING_JOIN_INNER(a, b) a##b
#define ALGO_TIMING_JOIN(a, b) ALGO_TIMING_JOIN_INNER(a, b)
#define ALGO_TIMING_SCOPE(stage) \
    AlgoTimingScope ALGO_TIMING_JOIN(_algo_timing_scope_, __LINE__)(stage)
#define ALGO_TIMING_START(var) const int64_t _algo_timing_start_##var = AlgoTiming::nowNs()
#define ALGO_TIMING_END(var, stage) \
    AlgoTiming::record(stage, AlgoTiming::nowNs() - _algo_timing_start_##var)

#else

#define ALGO_TIMING_SCOPE(stage) ((void)0)
#define ALGO_TIMING_START(var) ((void)0)
#define ALGO_TIMING_END(var, stage) ((void)0)

#endif  // ENABLE_ALGO_TIMING_LOG

#endif  // PERFTIMER_H
