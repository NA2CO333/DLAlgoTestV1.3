#include "perftimer.h"

#if ENABLE_ALGO_TIMING_LOG

#include <QDebug>
#include <QString>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

static const int TIMING_CONTEXT_COUNT = AlgoTiming::MAX_TIMING_ROUNDS + 1;
static const int PREVIEW_CONTEXT_INDEX = 0;

struct AtomicTimingStat {
    std::atomic<int64_t> totalNs;
    std::atomic<int64_t> count;
    std::atomic<int64_t> minNs;
    std::atomic<int64_t> maxNs;

    AtomicTimingStat()
        : totalNs(0),
          count(0),
          minNs(std::numeric_limits<int64_t>::max()),
          maxNs(0)
    {
    }
};

struct TimingSnapshot {
    int64_t totalNs = 0;
    int64_t count = 0;
    int64_t minNs = 0;
    int64_t maxNs = 0;
};

AtomicTimingStat g_timingStats[TIMING_CONTEXT_COUNT][AlgoTimingStage_Count];
std::atomic<int64_t> g_timingEvents[TIMING_CONTEXT_COUNT][AlgoTimingEvent_Count];
std::atomic<int64_t> g_previewFrameCount(0);
std::atomic_flag g_previewPrintLock = ATOMIC_FLAG_INIT;
std::atomic<int64_t> g_measurementStartNs(0);
std::atomic<int64_t> g_firstFormalFrameNs(0);
std::atomic<int64_t> g_roundBeginNs[AlgoTiming::MAX_TIMING_ROUNDS];
std::atomic<int64_t> g_roundFirstFrameNs[AlgoTiming::MAX_TIMING_ROUNDS];
// 每轮第22张已入算法缓存的时刻，近似代表该轮转灯采集结束。
std::atomic<int64_t> g_roundCaptureCompleteNs[AlgoTiming::MAX_TIMING_ROUNDS];
std::atomic<int64_t> g_lastCaptureCompleteNs(0);
std::atomic<int> g_lastCaptureCompleteRound(-1);
std::atomic<int64_t> g_roundPrinted[AlgoTiming::MAX_TIMING_ROUNDS];
std::atomic<int64_t> g_maxFormalPhotoTasksInFlight(0);

thread_local AlgoTimingPhase g_timingPhase = AlgoTimingPhase_None;
thread_local int g_timingRound = -1;

struct TimingStaticInitializer {
    TimingStaticInitializer()
    {
        for (int context = 0; context < TIMING_CONTEXT_COUNT; ++context) {
            for (int event = 0; event < AlgoTimingEvent_Count; ++event) {
                g_timingEvents[context][event].store(0, std::memory_order_relaxed);
            }
        }
        for (int round = 0; round < AlgoTiming::MAX_TIMING_ROUNDS; ++round) {
            g_roundBeginNs[round].store(0, std::memory_order_relaxed);
            g_roundFirstFrameNs[round].store(0, std::memory_order_relaxed);
            g_roundCaptureCompleteNs[round].store(0, std::memory_order_relaxed);
            g_roundPrinted[round].store(0, std::memory_order_relaxed);
        }
    }
};

TimingStaticInitializer g_timingStaticInitializer;

int contextIndex(AlgoTimingPhase phase, int roundIdx)
{
    if (phase == AlgoTimingPhase_Preview) {
        return PREVIEW_CONTEXT_INDEX;
    }
    if (phase == AlgoTimingPhase_Formal
            && roundIdx >= 0
            && roundIdx < AlgoTiming::MAX_TIMING_ROUNDS) {
        return roundIdx + 1;
    }
    return -1;
}

void updateMin(std::atomic<int64_t>& target, int64_t value)
{
    int64_t current = target.load(std::memory_order_relaxed);
    while (value < current
           && !target.compare_exchange_weak(current, value,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
    }
}

void updateMax(std::atomic<int64_t>& target, int64_t value)
{
    int64_t current = target.load(std::memory_order_relaxed);
    while (value > current
           && !target.compare_exchange_weak(current, value,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
    }
}

void resetContext(int index)
{
    if (index < 0 || index >= TIMING_CONTEXT_COUNT) {
        return;
    }
    for (int i = 0; i < AlgoTimingStage_Count; ++i) {
        g_timingStats[index][i].totalNs.store(0, std::memory_order_relaxed);
        g_timingStats[index][i].count.store(0, std::memory_order_relaxed);
        g_timingStats[index][i].minNs.store(std::numeric_limits<int64_t>::max(),
                                            std::memory_order_relaxed);
        g_timingStats[index][i].maxNs.store(0, std::memory_order_relaxed);
    }
    for (int i = 0; i < AlgoTimingEvent_Count; ++i) {
        g_timingEvents[index][i].store(0, std::memory_order_relaxed);
    }
}

TimingSnapshot snapshot(int index, AlgoTimingStage stage, bool take)
{
    TimingSnapshot out;
    if (index < 0 || index >= TIMING_CONTEXT_COUNT) {
        return out;
    }

    AtomicTimingStat& stat = g_timingStats[index][stage];
    if (take) {
        out.totalNs = stat.totalNs.exchange(0, std::memory_order_relaxed);
        out.count = stat.count.exchange(0, std::memory_order_relaxed);
        out.minNs = stat.minNs.exchange(std::numeric_limits<int64_t>::max(),
                                        std::memory_order_relaxed);
        out.maxNs = stat.maxNs.exchange(0, std::memory_order_relaxed);
    } else {
        out.totalNs = stat.totalNs.load(std::memory_order_relaxed);
        out.count = stat.count.load(std::memory_order_relaxed);
        out.minNs = stat.minNs.load(std::memory_order_relaxed);
        out.maxNs = stat.maxNs.load(std::memory_order_relaxed);
    }
    if (out.count == 0 || out.minNs == std::numeric_limits<int64_t>::max()) {
        out.minNs = 0;
    }
    return out;
}

int64_t takeEvent(int index, AlgoTimingEvent event)
{
    return g_timingEvents[index][event].exchange(0, std::memory_order_relaxed);
}

int64_t readEvent(int index, AlgoTimingEvent event)
{
    return g_timingEvents[index][event].load(std::memory_order_relaxed);
}

QString statText(const char* name, const TimingSnapshot& stat)
{
    if (stat.count <= 0) {
        // 未调用的阶段不展开无意义的 n=0，避免摘要被空阶段刷屏。
        return QString();
    }
    const double avgMs = static_cast<double>(stat.totalNs)
            / static_cast<double>(stat.count) / 1000000.0;
    return QString("%1={n=%2,total=%3ms,avg=%4ms,min=%5ms,max=%6ms}")
            .arg(QString::fromLatin1(name))
            .arg(static_cast<qlonglong>(stat.count))
            .arg(stat.totalNs / 1000000.0, 0, 'f', 2)
            .arg(avgMs, 0, 'f', 2)
            .arg(stat.minNs / 1000000.0, 0, 'f', 2)
            .arg(stat.maxNs / 1000000.0, 0, 'f', 2);
}

TimingSnapshot aggregateFormal(AlgoTimingStage stage)
{
    TimingSnapshot total;
    total.minNs = std::numeric_limits<int64_t>::max();
    for (int round = 0; round < AlgoTiming::MAX_TIMING_ROUNDS; ++round) {
        const TimingSnapshot item = snapshot(round + 1, stage, false);
        total.totalNs += item.totalNs;
        total.count += item.count;
        if (item.count > 0) {
            total.minNs = std::min(total.minNs, item.minNs);
            total.maxNs = std::max(total.maxNs, item.maxNs);
        }
    }
    if (total.count == 0) {
        total.minNs = 0;
    }
    return total;
}

TimingSnapshot aggregateFormalStages(AlgoTimingStage firstStage,
                                     AlgoTimingStage secondStage)
{
    TimingSnapshot total = aggregateFormal(firstStage);
    const TimingSnapshot second = aggregateFormal(secondStage);
    total.totalNs += second.totalNs;
    total.count += second.count;
    if (second.count > 0) {
        total.minNs = total.count == second.count
                ? second.minNs : std::min(total.minNs, second.minNs);
        total.maxNs = std::max(total.maxNs, second.maxNs);
    }
    return total;
}

} // namespace

int64_t AlgoTiming::nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
}

void AlgoTiming::setContext(AlgoTimingPhase phase, int roundIdx)
{
    g_timingPhase = phase;
    g_timingRound = roundIdx;
}

AlgoTimingPhase AlgoTiming::currentPhase()
{
    return g_timingPhase;
}

int AlgoTiming::currentRound()
{
    return g_timingRound;
}

void AlgoTiming::record(AlgoTimingStage stage, int64_t elapsedNs)
{
    record(stage, elapsedNs, g_timingPhase, g_timingRound);
}

void AlgoTiming::record(AlgoTimingStage stage, int64_t elapsedNs,
                        AlgoTimingPhase phase, int roundIdx)
{
    const int index = contextIndex(phase, roundIdx);
    if (index < 0 || stage < 0 || stage >= AlgoTimingStage_Count || elapsedNs < 0) {
        return;
    }

    AtomicTimingStat& stat = g_timingStats[index][stage];
    stat.totalNs.fetch_add(elapsedNs, std::memory_order_relaxed);
    stat.count.fetch_add(1, std::memory_order_relaxed);
    updateMin(stat.minNs, elapsedNs);
    updateMax(stat.maxNs, elapsedNs);
}

void AlgoTiming::recordMilliseconds(AlgoTimingStage stage, double elapsedMs)
{
    if (!std::isfinite(elapsedMs) || elapsedMs < 0.0) {
        return;
    }
    record(stage, static_cast<int64_t>(elapsedMs * 1000000.0));
}

void AlgoTiming::event(AlgoTimingEvent eventValue)
{
    const int index = contextIndex(g_timingPhase, g_timingRound);
    if (index < 0 || eventValue < 0 || eventValue >= AlgoTimingEvent_Count) {
        return;
    }
    g_timingEvents[index][eventValue].fetch_add(1, std::memory_order_relaxed);
}

void AlgoTiming::recordFormalPhotoTasksInFlight(int count)
{
    if (count <= 0) {
        return;
    }
    updateMax(g_maxFormalPhotoTasksInFlight, static_cast<int64_t>(count));
}

int AlgoTiming::maxFormalPhotoTasksInFlight()
{
    return static_cast<int>(g_maxFormalPhotoTasksInFlight.load(
            std::memory_order_relaxed));
}

void AlgoTiming::beginMeasurement()
{
    // 正式测量开始后清掉不足30帧的预览尾数，避免带入下一次预览。
    resetContext(PREVIEW_CONTEXT_INDEX);
    g_previewFrameCount.store(0, std::memory_order_relaxed);
    g_previewPrintLock.clear(std::memory_order_release);
    for (int i = 1; i < TIMING_CONTEXT_COUNT; ++i) {
        resetContext(i);
    }
    for (int i = 0; i < MAX_TIMING_ROUNDS; ++i) {
        g_roundBeginNs[i].store(0, std::memory_order_relaxed);
        g_roundFirstFrameNs[i].store(0, std::memory_order_relaxed);
        g_roundCaptureCompleteNs[i].store(0, std::memory_order_relaxed);
        g_roundPrinted[i].store(0, std::memory_order_relaxed);
    }
    g_measurementStartNs.store(nowNs(), std::memory_order_relaxed);
    g_firstFormalFrameNs.store(0, std::memory_order_relaxed);
    g_lastCaptureCompleteNs.store(0, std::memory_order_relaxed);
    g_lastCaptureCompleteRound.store(-1, std::memory_order_relaxed);
    g_maxFormalPhotoTasksInFlight.store(0, std::memory_order_relaxed);
}

void AlgoTiming::beginRound(int roundIdx)
{
    if (roundIdx < 0 || roundIdx >= MAX_TIMING_ROUNDS) {
        return;
    }
    resetContext(roundIdx + 1);
    g_roundBeginNs[roundIdx].store(nowNs(), std::memory_order_relaxed);
    g_roundFirstFrameNs[roundIdx].store(0, std::memory_order_relaxed);
    g_roundCaptureCompleteNs[roundIdx].store(0, std::memory_order_relaxed);
    g_roundPrinted[roundIdx].store(0, std::memory_order_relaxed);
}

void AlgoTiming::markFormalFrame(int roundIdx)
{
    if (roundIdx < 0 || roundIdx >= MAX_TIMING_ROUNDS) {
        return;
    }
    const int64_t now = nowNs();
    int64_t expected = 0;
    g_firstFormalFrameNs.compare_exchange_strong(expected, now,
                                                  std::memory_order_relaxed);
    expected = 0;
    g_roundFirstFrameNs[roundIdx].compare_exchange_strong(expected, now,
                                                          std::memory_order_relaxed);
}

void AlgoTiming::markFormalRoundCaptureComplete(int roundIdx)
{
    if (roundIdx < 0 || roundIdx >= MAX_TIMING_ROUNDS) {
        return;
    }
    const int64_t now = nowNs();
    int64_t expected = 0;
    // 同一轮如出现重复帧，只记录首次“22张均到齐”的真实采集结束点。
    if (g_roundCaptureCompleteNs[roundIdx].compare_exchange_strong(
            expected, now, std::memory_order_relaxed)) {
        g_lastCaptureCompleteNs.store(now, std::memory_order_relaxed);
        g_lastCaptureCompleteRound.store(roundIdx, std::memory_order_relaxed);
    }
}

void AlgoTiming::completePreviewFrame(bool success)
{
    setContext(AlgoTimingPhase_Preview, -1);
    event(success ? AlgoTimingEvent_PreviewSuccess : AlgoTimingEvent_PreviewFailure);
    const int64_t frame = g_previewFrameCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (frame % 30 != 0 || g_previewPrintLock.test_and_set(std::memory_order_acquire)) {
        return;
    }

    const int index = PREVIEW_CONTEXT_INDEX;
    const TimingSnapshot total = snapshot(index, AlgoTimingStage_DetectPupilTotal, true);
    const TimingSnapshot clone = snapshot(index, AlgoTimingStage_ImageClone, true);
    const TimingSnapshot eyeTotal = snapshot(index, AlgoTimingStage_EyeDetectTotal, true);
    const TimingSnapshot resize = snapshot(index, AlgoTimingStage_EyeResize, true);
    const TimingSnapshot preprocess = snapshot(index, AlgoTimingStage_EyePreprocess, true);
    const TimingSnapshot haarNormal = snapshot(index, AlgoTimingStage_HaarNormal, true);
    const TimingSnapshot haarEq = snapshot(index, AlgoTimingStage_HaarEqualized, true);
    const TimingSnapshot haarRelaxed = snapshot(index, AlgoTimingStage_HaarRelaxed, true);
    const TimingSnapshot cascadeAccess = snapshot(index, AlgoTimingStage_HaarCascadeAccess, true);
    const TimingSnapshot eyeRect = snapshot(index, AlgoTimingStage_EyeRectBuild, true);
    const TimingSnapshot pupilRight = snapshot(index, AlgoTimingStage_PupilRight, true);
    const TimingSnapshot pupilLeft = snapshot(index, AlgoTimingStage_PupilLeft, true);
    const TimingSnapshot pupilContour = snapshot(index, AlgoTimingStage_PupilContour, true);
    const TimingSnapshot lowRight = snapshot(index, AlgoTimingStage_LowFallbackRight, true);
    const TimingSnapshot lowLeft = snapshot(index, AlgoTimingStage_LowFallbackLeft, true);
    const TimingSnapshot gate = snapshot(index, AlgoTimingStage_PreviewGate, true);
    const TimingSnapshot exposure = snapshot(index, AlgoTimingStage_ExposureCalc, true);
    const int64_t successCount = takeEvent(index, AlgoTimingEvent_PreviewSuccess);
    const int64_t failureCount = takeEvent(index, AlgoTimingEvent_PreviewFailure);
    const int64_t lowSuccess = takeEvent(index, AlgoTimingEvent_LowFallbackSuccess);

    qDebug().noquote() << QString("[AlgoTiming][Preview] frames=%1 success=%2 failed=%3")
                          .arg(static_cast<qlonglong>(successCount + failureCount))
                          .arg(static_cast<qlonglong>(successCount))
                          .arg(static_cast<qlonglong>(failureCount));
    qDebug().noquote() << "[AlgoTiming][Preview]"
                       << statText("total", total)
                       << statText("clone", clone)
                       << statText("eye_total", eyeTotal);
    qDebug().noquote() << "[AlgoTiming][Preview]"
                       << statText("resize", resize)
                       << statText("preprocess", preprocess)
                       << statText("haar_normal", haarNormal)
                       << statText("haar_equalized", haarEq)
                       << statText("haar_relaxed", haarRelaxed)
                       << statText("cascade_access", cascadeAccess);
    qDebug().noquote() << "[AlgoTiming][Preview]"
                       << statText("eye_rect", eyeRect)
                       << statText("pupil_right", pupilRight)
                       << statText("pupil_left", pupilLeft)
                       << statText("contour_core", pupilContour)
                       << statText("low_right", lowRight)
                       << statText("low_left", lowLeft)
                       << QString("low_success=%1").arg(static_cast<qlonglong>(lowSuccess));
    qDebug().noquote() << "[AlgoTiming][Preview]"
                       << statText("gate", gate)
                       << statText("exposure", exposure);

    g_previewPrintLock.clear(std::memory_order_release);
}

void AlgoTiming::printRoundSummary(int roundIdx, const char* resultState)
{
    if (roundIdx < 0 || roundIdx >= MAX_TIMING_ROUNDS) {
        return;
    }
    int64_t expected = 0;
    if (!g_roundPrinted[roundIdx].compare_exchange_strong(expected, 1,
                                                          std::memory_order_relaxed)) {
        return;
    }

    const int index = roundIdx + 1;
    const int64_t now = nowNs();
    const int64_t roundBegin = g_roundBeginNs[roundIdx].load(std::memory_order_relaxed);
    const int64_t firstFrame = g_roundFirstFrameNs[roundIdx].load(std::memory_order_relaxed);
    const int64_t captureComplete =
        g_roundCaptureCompleteNs[roundIdx].load(std::memory_order_relaxed);
    const double beginWallMs = roundBegin > 0 ? (now - roundBegin) / 1000000.0 : 0.0;
    const double frameWallMs = firstFrame > 0 ? (now - firstFrame) / 1000000.0 : 0.0;
    const double captureMs = firstFrame > 0 && captureComplete > 0
        ? (captureComplete - firstFrame) / 1000000.0 : 0.0;
    const double postCaptureMs = captureComplete > 0
        ? (now - captureComplete) / 1000000.0 : 0.0;
    const int64_t pupilSuccess = readEvent(index, AlgoTimingEvent_FormalPupilSuccess);
    const int64_t pupilFailure = readEvent(index, AlgoTimingEvent_FormalPupilFailure);
    const int64_t processSuccess = readEvent(index, AlgoTimingEvent_ProcessSuccess);
    const int64_t processFailure = readEvent(index, AlgoTimingEvent_ProcessFailure);
    const int64_t smallMatchSuccess =
        readEvent(index, AlgoTimingEvent_FormalSmallMatchSuccess);
    const int64_t smallMatchFailure =
        readEvent(index, AlgoTimingEvent_FormalSmallMatchFailure);
    const int64_t roi129Success =
        readEvent(index, AlgoTimingEvent_FormalRoi129Success);
    const int64_t roi129Failure =
        readEvent(index, AlgoTimingEvent_FormalRoi129Failure);
    const int64_t legacyHalfFallbackSuccess =
        readEvent(index, AlgoTimingEvent_HalfFallbackSuccess);

    qDebug().noquote() << QString("[AlgoTiming][Round] round=%1 result=%2 begin_to_complete=%3ms first_frame_to_complete=%4ms first_frame_to_capture_complete=%5ms capture_complete_to_result=%6ms pupil_success=%7 pupil_failed=%8 process_success=%9 process_failed=%10 small_match_success=%11 small_match_failed=%12 roi129_success=%13 roi129_failed=%14 legacy_half_fallback_success=%15")
                          .arg(roundIdx)
                          .arg(QString::fromLatin1(resultState))
                          .arg(beginWallMs, 0, 'f', 2)
                          .arg(frameWallMs, 0, 'f', 2)
                          .arg(captureMs, 0, 'f', 2)
                          .arg(postCaptureMs, 0, 'f', 2)
                          .arg(static_cast<qlonglong>(pupilSuccess))
                          .arg(static_cast<qlonglong>(pupilFailure))
                          .arg(static_cast<qlonglong>(processSuccess))
                          .arg(static_cast<qlonglong>(processFailure))
                          .arg(static_cast<qlonglong>(smallMatchSuccess))
                          .arg(static_cast<qlonglong>(smallMatchFailure))
                          .arg(static_cast<qlonglong>(roi129Success))
                          .arg(static_cast<qlonglong>(roi129Failure))
                          .arg(static_cast<qlonglong>(legacyHalfFallbackSuccess));
    qDebug().noquote() << "[AlgoTiming][Round]"
                       << statText("append_clone", snapshot(index, AlgoTimingStage_AppendClone, false))
                       << statText("queue_wait", snapshot(index, AlgoTimingStage_QueueWait, false))
                       << statText("task_total", snapshot(index, AlgoTimingStage_FormalTaskTotal, false))
                       << statText("detect_pupil", snapshot(index, AlgoTimingStage_DetectPupilTotal, false))
                       << statText("process_right", snapshot(index, AlgoTimingStage_ProcessRight, false))
                       << statText("process_left", snapshot(index, AlgoTimingStage_ProcessLeft, false));
    qDebug().noquote() << "[AlgoTiming][RoundDL]"
                       << statText("c800_total", snapshot(index, AlgoTimingStage_C800Total, false))
                       << statText("c800_preprocess", snapshot(index, AlgoTimingStage_C800Preprocess, false))
                       << statText("c800_forward", snapshot(index, AlgoTimingStage_C800Forward, false))
                       << statText("c800_postprocess", snapshot(index, AlgoTimingStage_C800Postprocess, false))
                       << statText("small_resize", snapshot(index, AlgoTimingStage_FormalSmallResize, false))
                       << statText("small_match", snapshot(index, AlgoTimingStage_FormalSmallMatch, false))
                       << statText("roi129_right", snapshot(index, AlgoTimingStage_Roi129Right, false))
                       << statText("haar_normal", snapshot(index, AlgoTimingStage_HaarNormal, false))
                       << statText("pupil_right", snapshot(index, AlgoTimingStage_PupilRight, false))
                       << statText("pupil_left", snapshot(index, AlgoTimingStage_PupilLeft, false))
                       << statText("roi129_left", snapshot(index, AlgoTimingStage_Roi129Left, false))
                       << statText("accurate", snapshot(index, AlgoTimingStage_AccuratePupil, false));
    qDebug().noquote() << "[AlgoTiming][RoundCalc]"
                       << statText("process_pupil_roi", snapshot(index, AlgoTimingStage_ProcessPupilRoi, false))
                       << statText("compare_each", snapshot(index, AlgoTimingStage_CompareEach, false))
                       << statText("ds_build", snapshot(index, AlgoTimingStage_DsBuild, false))
                       << statText("ds_patch", snapshot(index, AlgoTimingStage_DsPatch, false))
                       << statText("refraction_right", snapshot(index, AlgoTimingStage_RefractionRight, false))
                       << statText("refraction_left", snapshot(index, AlgoTimingStage_RefractionLeft, false))
                       << statText("postprocess", snapshot(index, AlgoTimingStage_ResultPostProcess, false))
                       << statText("policy", snapshot(index, AlgoTimingStage_Policy, false))
                       << statText("settlement", snapshot(index, AlgoTimingStage_RoundSettlement, false));
}

void AlgoTiming::printMeasurementSummary(int roundCount, int formalFrameCount,
                                         bool finished)
{
    const int64_t now = nowNs();
    const int64_t begin = g_measurementStartNs.load(std::memory_order_relaxed);
    const int64_t firstFrame = g_firstFormalFrameNs.load(std::memory_order_relaxed);
    const int64_t captureComplete =
        g_lastCaptureCompleteNs.load(std::memory_order_relaxed);
    const int lastCaptureRound =
        g_lastCaptureCompleteRound.load(std::memory_order_relaxed);
    qDebug().noquote()
            << QString("[AlgoTiming][Measure] finished=%1 rounds=%2 frames=%3 max_photo_tasks=%4 calc_begin_to_result=%5ms first_frame_to_result=%6ms last_complete_round=%7 first_frame_to_capture_complete=%8ms capture_complete_to_result=%9ms")
               .arg(finished ? 1 : 0)
               .arg(roundCount)
               .arg(formalFrameCount)
               .arg(maxFormalPhotoTasksInFlight())
               .arg(begin > 0 ? (now - begin) / 1000000.0 : 0.0, 0, 'f', 2)
               .arg(firstFrame > 0 ? (now - firstFrame) / 1000000.0 : 0.0, 0, 'f', 2)
               .arg(lastCaptureRound)
               .arg(firstFrame > 0 && captureComplete > 0
                        ? (captureComplete - firstFrame) / 1000000.0 : 0.0,
                    0, 'f', 2)
               .arg(captureComplete > 0
                        ? (now - captureComplete) / 1000000.0 : 0.0,
                    0, 'f', 2);
    qDebug().noquote() << "[AlgoTiming][Measure]"
                       << statText("queue_wait_all", aggregateFormal(AlgoTimingStage_QueueWait))
                       << statText("detect_pupil_all", aggregateFormal(AlgoTimingStage_DetectPupilTotal))
                       << statText("compare_each_all", aggregateFormal(AlgoTimingStage_CompareEach))
                       << statText("policy_all", aggregateFormal(AlgoTimingStage_Policy));
    qDebug().noquote() << "[AlgoTiming][MeasureDL]"
                       << statText("c800_total_all", aggregateFormal(AlgoTimingStage_C800Total))
                       << statText("c800_forward_all", aggregateFormal(AlgoTimingStage_C800Forward))
                       << statText("small_match_all", aggregateFormal(AlgoTimingStage_FormalSmallMatch))
                       << statText("roi129_all", aggregateFormalStages(
                                      AlgoTimingStage_Roi129Right,
                                      AlgoTimingStage_Roi129Left));
}

AlgoTimingContextScope::AlgoTimingContextScope(AlgoTimingPhase phase, int roundIdx)
    : m_previousPhase(AlgoTiming::currentPhase()),
      m_previousRound(AlgoTiming::currentRound())
{
    AlgoTiming::setContext(phase, roundIdx);
}

AlgoTimingContextScope::~AlgoTimingContextScope()
{
    AlgoTiming::setContext(m_previousPhase, m_previousRound);
}

AlgoTimingScope::AlgoTimingScope(AlgoTimingStage stage)
    : m_stage(stage),
      m_startNs(AlgoTiming::nowNs()),
      m_stopped(false)
{
}

AlgoTimingScope::~AlgoTimingScope()
{
    stop();
}

void AlgoTimingScope::stop()
{
    if (m_stopped) {
        return;
    }
    m_stopped = true;
    AlgoTiming::record(m_stage, AlgoTiming::nowNs() - m_startNs);
}

#endif  // ENABLE_ALGO_TIMING_LOG

#if PERF_LOG

// 静态成员定义
PerfTimer::StatsMap PerfTimer::s_stats;

PerfTimer::PerfTimer(const std::string& name) : m_name(name) {
    m_start = Clock::now();
}

PerfTimer::~PerfTimer() {
    TimePoint end = Clock::now();
    int64_t elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end - m_start).count();

    Stats& stat = s_stats[m_name];
    stat.total_ns += elapsed;
    ++stat.count;
    if (elapsed < stat.min_ns) stat.min_ns = elapsed;
    if (elapsed > stat.max_ns) stat.max_ns = elapsed;
}

void PerfTimer::printStats() {
    qDebug() << "\n========== PERF STATS ==========";
    qDebug() << QString("%1 %2 %3 %4 %5 %6")
                .arg("Name", -30)
                .arg("Total(ms)", 10)
                .arg("Avg(us)", 10)
                .arg("Min(us)", 10)
                .arg("Max(us)", 10)
                .arg("Count", 8);
    qDebug() << QString(80, '-');

    std::vector<std::pair<int64_t, std::string> > sorted;
    for (StatsMap::const_iterator it = s_stats.begin();
         it != s_stats.end(); ++it) {
        sorted.push_back(std::make_pair(it->second.total_ns, it->first));
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const std::pair<int64_t, std::string>& a,
           const std::pair<int64_t, std::string>& b) {
            return a.first > b.first;
        });

    for (size_t i = 0; i < sorted.size(); ++i) {
        const std::string& name = sorted[i].second;
        const Stats& stat = s_stats[name];
        double total_ms = stat.total_ns / 1000000.0;
        int64_t avg_us = (stat.total_ns / stat.count) / 1000;
        int64_t min_us = stat.min_ns / 1000;
        int64_t max_us = stat.max_ns / 1000;

        qDebug() << QString("%1 %2 %3 %4 %5 %6")
                    .arg(QString::fromStdString(name), -30)
                    .arg(total_ms, 10, 'f', 2)
                    .arg(avg_us, 10)
                    .arg(min_us, 10)
                    .arg(max_us, 10)
                    .arg(stat.count, 8);
    }
    qDebug() << QString(80, '=');
}

void PerfTimer::printOnce(const std::string& name, int64_t ns) {
    qDebug() << QString("[PERF] %1: %2 us (%3 ms)")
                .arg(QString::fromStdString(name), -25)
                .arg(ns / 1000, 8)
                .arg(ns / 1000000.0, 6, 'f', 2);
}

#endif  // PERF_LOG
