#include "exposure-adjuster.h"

#include "algo-invoker.h"
#include "windowsmanager.h"
#include "global.h"

#include <algorithm>
#include <cmath>

// 曝光模块只需要独立的逐帧日志开关，不引入算法头中的qDebug兼容宏。
#ifndef ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#define ENABLE_DL_MERGE_TEST_FULL_VERBOSE 0
#endif
#ifndef ENABLE_PREVIEW_EXPOSURE_VERBOSE_LOG
#define ENABLE_PREVIEW_EXPOSURE_VERBOSE_LOG ENABLE_DL_MERGE_TEST_FULL_VERBOSE
#endif

/// ====================================================================================================================
/// class CExposureAdjust

//
const char * const CExposureAdjuster::S_CLASS_NAME = CExposureAdjuster::staticMetaObject.className();

CExposureAdjuster::CExposureAdjuster(QObject *parent) : QObject(parent)
{

}

CExposureAdjuster::~CExposureAdjuster()
{

}

void CExposureAdjuster::reset()
{
    m_isGrayStable = false;
    m_isFinished = false;
    m_expoHistory.clear();
    m_countTotal = 0;

    // 测试模式下，每次预览都模拟首次进入：不复用上一次可靠曝光，并先恢复统一默认曝光。
    m_coldStartTestActive = (ENABLE_EXPOSURE_COLD_START_TEST != 0) && !m_isFixed;
    if (m_coldStartTestActive) {
        m_hasReliableExposure = false;
    }
    m_coldStartPending = m_coldStartTestActive;
    m_coldStartApplied = false;
    m_coldStartExposureUs = m_coldStartTestActive ? defaultExposureUs() : -1;

    // 正常模式保留可靠曝光；冷启动测试或尚无可靠曝光时启用首次快速搜索。
    m_isInitialOptimizationActive = !m_isFixed
            && (m_coldStartTestActive || !m_hasReliableExposure);
    m_initialCandidateIndex = 0;
    m_initialCandidateValuesTried.clear();
    m_initialCandidateTryCount = 0;
    m_proportionalAdjustCount = 0;
    m_fineAdjustCount = 0;

#if ENABLE_EXPOSURE_TIMING_LOG
    // 每次进入预览曝光流程都开启一个新的计时会话。
    m_exposureTimingActive = true;
    m_exposureTimingReported = false;
    m_previewStartedAt = std::chrono::steady_clock::now();
    m_adjustmentStartedAt = {};
    m_firstPupilDetectedAt = {};
    m_sampleCount = 0;
    m_adjustmentCount = 0;
    m_hasFirstPupilDetected = false;
    m_initialExposure = -1;
    m_finalExposure = -1;
    m_initialGray = -1;
    m_finalGray = -1;
    m_initialFastMode = m_isInitialOptimizationActive;
    m_algorithmTotalMs = 0.0;
    m_setExposureTotalMs = 0.0;
#endif
}

void CExposureAdjuster::setIsFixed(bool _is_yes)
{
    bool is_trigger = false;
    if (_is_yes && !m_isFixed) {
        is_trigger = true;
    }

    m_isFixed = _is_yes;

    if (_is_yes) {
        // 固定曝光和调焦模式不进入首次快速优化。
        m_coldStartTestActive = false;
        m_coldStartPending = false;
        m_coldStartApplied = false;
        m_coldStartExposureUs = -1;
        m_isInitialOptimizationActive = false;
#if ENABLE_EXPOSURE_TIMING_LOG
        m_initialFastMode = false;
#endif
    } else if (m_countTotal == 0
               && ((ENABLE_EXPOSURE_COLD_START_TEST != 0)
                   || !m_hasReliableExposure)) {
        // initMeasure() 可能先 reset()、再关闭上一次固定曝光；此时补建冷启动状态。
        m_coldStartTestActive = (ENABLE_EXPOSURE_COLD_START_TEST != 0);
        if (m_coldStartTestActive) {
            m_hasReliableExposure = false;
            m_coldStartPending = true;
            m_coldStartApplied = false;
            m_coldStartExposureUs = defaultExposureUs();
        }
        m_isInitialOptimizationActive = true;
#if ENABLE_EXPOSURE_TIMING_LOG
        m_initialFastMode = true;
#endif
    }

    if (is_trigger) {
        if (getIsFinished()) {
            // 固定曝光不经过反馈稳定判定，单独结束本次计时会话。
            finishExposureTiming("fixed", m_finalExposure);
            emit sigIsExposureOk();
        }
    }
}

int CExposureAdjuster::defaultExposureUs()
{
    const int expo_us_min = CGlobal::exposureMsMin * 1000;
    const int expo_us_max = CGlobal::exposureMsMax * 1000;
    const int expo_us_step = CGlobal::expoFineAdjStepMs * 1000;

    int expo_us = expo_us_min + expo_us_step * qCeil((double)(expo_us_max - expo_us_min) / expo_us_step) * 0.6;
    return expo_us;
}

int CExposureAdjuster::inputExposureInfo(bool _is_pupil_succ, int _avg_gray, bool _is_over_expo, int _expo_us_curr)
{
#if ENABLE_PREVIEW_EXPOSURE_VERBOSE_LOG
    // 每帧曝光反馈默认不输出；最终曝光计时汇总仍由独立开关保留。
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered"
             << ", _is_pupil_succ = " << Util::bool2str(_is_pupil_succ)
             << ", _avg_gray = " << _avg_gray
             << ", _expo_us_curr = " << _expo_us_curr;
#endif

    Q_UNUSED(_is_over_expo)

#if ENABLE_EXPOSURE_TIMING_LOG
    const auto algorithmStartedAt = std::chrono::steady_clock::now();
    bool exposureTimingFinished = false;
    if (m_sampleCount == 0) {
        m_adjustmentStartedAt = algorithmStartedAt;
        m_initialExposure = _expo_us_curr;
        m_initialGray = _avg_gray;
    }
    if (_is_pupil_succ && !m_hasFirstPupilDetected) {
        // 新计时口径：从预览第一次识别到瞳孔开始，统计至曝光稳定。
        m_hasFirstPupilDetected = true;
        m_firstPupilDetectedAt = algorithmStartedAt;
    }
    m_sampleCount++;
    m_finalExposure = _expo_us_curr;
    m_finalGray = _avg_gray;
#endif

    // 若是固定曝光时间，只需设置一次
    //if (getIsFinished()) {
    //    //
    //    if () {
    //
    //    } else {
    //        return -1;
    //    }
    //}

    //
    m_countTotal++;

    //
    int new_expo = -1;

    //
    const int expo_us_min = CGlobal::exposureMsMin * 1000;
    const int expo_us_max = CGlobal::exposureMsMax * 1000;
    const int expo_us_step = CGlobal::expoFineAdjStepMs * 1000;

    bool isProportionalAdjustment = false;
    bool isFineAdjustment = false;

    const auto clampExposure = [expo_us_min, expo_us_max](int exposure) {
        return std::max(expo_us_min, std::min(expo_us_max, exposure));
    };

    const auto buildInitialCandidates = [this, expo_us_min, expo_us_max]() {
        QList<int> candidates;
        const double exposureRange =
                static_cast<double>(expo_us_max - expo_us_min);
        const auto appendCandidate = [&candidates, expo_us_min, expo_us_max](double value) {
            const int candidate = std::max(
                    expo_us_min,
                    std::min(expo_us_max, static_cast<int>(std::lround(value))));
            if (!candidates.contains(candidate)) {
                candidates.append(candidate);
            }
        };

        // 候选顺序固定为：默认值、约33%、约87%、约13%、最大值。
        appendCandidate(static_cast<double>(defaultExposureUs()));
        appendCandidate(expo_us_min + exposureRange * 0.33);
        appendCandidate(expo_us_min + exposureRange * 0.87);
        appendCandidate(expo_us_min + exposureRange * 0.13);
        appendCandidate(static_cast<double>(expo_us_max));
        return candidates;
    };

    // 冷启动测试的首个反馈帧可能仍使用上一场曝光，先统一下发默认曝光。
    // 若当前值已经是默认曝光，则直接使用当前帧，避免一次无意义的重复下发。
    bool coldStartExposureChanged = false;
    if (m_coldStartPending) {
        m_coldStartPending = false;
        m_coldStartApplied = true;
        m_coldStartExposureUs = clampExposure(defaultExposureUs());
        if (!m_initialCandidateValuesTried.contains(m_coldStartExposureUs)) {
            m_initialCandidateValuesTried.append(m_coldStartExposureUs);
        }
        if (_expo_us_curr != m_coldStartExposureUs) {
            new_expo = m_coldStartExposureUs;
            coldStartExposureChanged = true;
        }
    }

    //
    if (coldStartExposureChanged) {
        // 当前帧是在旧曝光下采集的，不能参与灰度稳定或候选搜索判定。
    } else if (!_is_pupil_succ) {
        if (m_isInitialOptimizationActive) {
#if ENABLE_EXPOSURE_TIMING_LOG
            ++m_initialCandidateTryCount;
#endif
            const QList<int> candidates = buildInitialCandidates();
            // 当前帧已经使用过的候选值视为已尝试，防止从非默认曝光开始时重复回跳。
            if (candidates.contains(_expo_us_curr)
                    && !m_initialCandidateValuesTried.contains(_expo_us_curr)) {
                m_initialCandidateValuesTried.append(_expo_us_curr);
            }
            while (m_initialCandidateIndex < candidates.size()) {
                const int candidate = candidates.at(m_initialCandidateIndex++);
                // 当前曝光已经是候选值时不重复下发该值。
                if (!m_initialCandidateValuesTried.contains(candidate)) {
                    m_initialCandidateValuesTried.append(candidate);
                    new_expo = candidate;
                    break;
                }
            }

            if (new_expo < 0) {
                // 候选值全部尝试失败，回退原有固定步长扫描。
                m_isInitialOptimizationActive = false;
            }
        }

        if (new_expo < 0) {
            // 保留原有失败恢复逻辑，避免快速候选全部失败后无法继续寻找。
            if (1 == m_countTotal) {
                new_expo = clampExposure(defaultExposureUs());
            } else {
                new_expo = _expo_us_curr - expo_us_step;
                if (new_expo < expo_us_min) {
                    new_expo = expo_us_max;
                }
            }
            isFineAdjustment = true;
        }
    } else {
        if (m_isInitialOptimizationActive) {
#if ENABLE_EXPOSURE_TIMING_LOG
            ++m_initialCandidateTryCount;
#endif
            // 检测到瞳孔后停止候选搜索，后续仅执行比例粗调或固定步长精调。
            m_isInitialOptimizationActive = false;
        }

        // 添加曝光历史
        m_expoHistory.append(stExpoInfo {_expo_us_curr, _avg_gray});

        //
        static constexpr int STABLE_TIMES = 3;          // 稳定次数
        static constexpr int STABLE_DIFF_MAX = 10;      // 稳定容差

        // 清理掉多余的曝光历史
        while (m_expoHistory.size() > STABLE_TIMES) {
            m_expoHistory.removeFirst();
        }

        // 使瞳孔平均灰度进入【最小灰度，最大灰度】区间
        bool is_expo_exceed_max = false;                // 曝光时间是否超上限
        bool is_expo_exceed_min = false;                // 曝光时间是否超下限
        if (_avg_gray < CGlobal::pupilAverageMin_
                || _avg_gray > CGlobal::pupilAverageMax) {
            const bool isClearlyOutOfRange =
                    _avg_gray < 50 || _avg_gray > 190;

            if (isClearlyOutOfRange && _avg_gray > 0
                    && _expo_us_curr > 0) {
                const double targetGray =
                        (CGlobal::pupilAverageMin_
                         + CGlobal::pupilAverageMax) * 0.5;
                const double estimatedExposure =
                        static_cast<double>(_expo_us_curr)
                        * targetGray / static_cast<double>(_avg_gray);
                const double minAllowedChange =
                        static_cast<double>(_expo_us_curr) * 0.5;
                const double maxAllowedChange =
                        static_cast<double>(_expo_us_curr) * 1.5;
                const double boundedExposure = std::max(
                        static_cast<double>(expo_us_min),
                        std::min(static_cast<double>(expo_us_max),
                                 std::max(minAllowedChange,
                                          std::min(maxAllowedChange,
                                                   estimatedExposure))));
                new_expo = clampExposure(
                        static_cast<int>(std::lround(boundedExposure)));

                // 舍入后没有变化时，使用原有方向的固定步长保证状态继续推进。
                if (new_expo != _expo_us_curr) {
                    isProportionalAdjustment = true;
                } else {
                    new_expo = -1;
                }
            }

            if (new_expo < 0) {
                if (_avg_gray < CGlobal::pupilAverageMin_) {
                    // 若瞳孔区域灰度过低，则递增曝光时间。
                    new_expo = _expo_us_curr + expo_us_step;
                    if (new_expo > expo_us_max) {
                        is_expo_exceed_max = true;
                        new_expo = -1;
                    }
                } else if (_avg_gray > CGlobal::pupilAverageMax) {
                    // 若瞳孔区域灰度过高，则递减曝光时间。
                    new_expo = _expo_us_curr - expo_us_step;
                    if (new_expo < expo_us_min) {
                        is_expo_exceed_min = true;
                        new_expo = -1;
                    }
                }
                if (new_expo > 0) {
                    isFineAdjustment = true;
                }
            }
        }

        // 若曝光时间超限制提示
        if (is_expo_exceed_max) {
            QString err_msg = tr("瞳孔亮度太低，曝光时间已大于上限！");
            // "The pupil brightness is too low, and the exposure time has exceeded the upper limit!"
            emit sigMsgNotify(err_msg);
        } else if (is_expo_exceed_min) {
            QString err_msg = tr("瞳孔亮度太高，曝光时间已小于下限！");
            // "The pupil brightness is too high, and the exposure time has already been less than the lower limit!"
            emit sigMsgNotify(err_msg);
        }

        // 若计算后，不需新曝光时间，则判定为调光已完成
        if (new_expo < 0) {
            //
            if (m_expoHistory.size() >= STABLE_TIMES) {
                // 灰度稳定检测
                int last_expo_us = m_expoHistory.last().expoUs;
                int last_gray = m_expoHistory.last().gray;
                int count_same = 1;
                for (int i = m_expoHistory.size() - 2; i >= 0; i--) {
                    const stExpoInfo &info = m_expoHistory.at(i);
                    if (info.expoUs == last_expo_us) {
                        //if (info.gray >= expo_us_min && info.expoUs <= expo_us_max) {
                        if (qAbs(info.gray - last_gray) < STABLE_DIFF_MAX) {
                            //
                            count_same++;

                            //
                            if (count_same >= STABLE_TIMES) {
                                m_isGrayStable = true;
                            }
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }

            // 曝光完成
            if (m_isGrayStable) {
                qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): exposure is OK!";

                m_hasReliableExposure = true;
                m_isInitialOptimizationActive = false;

#if ENABLE_EXPOSURE_TIMING_LOG
                // 汇总日志在当前输入函数返回前输出，因此先计入本次算法耗时。
                m_algorithmTotalMs += std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - algorithmStartedAt).count();
                exposureTimingFinished = true;
#endif
                finishExposureTiming("stable", m_finalExposure);

                //
                m_isFinished = true;

                //
                emit sigIsExposureOk();
            }
        }
    }

#if ENABLE_EXPOSURE_TIMING_LOG
    if (new_expo > 0) {
        m_adjustmentCount++;
    }
    if (isProportionalAdjustment) {
        ++m_proportionalAdjustCount;
    }
    if (isFineAdjustment) {
        ++m_fineAdjustCount;
    }
    if (!exposureTimingFinished) {
        m_algorithmTotalMs += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - algorithmStartedAt).count();
    }
#endif

    //
    return new_expo;
}

bool CExposureAdjuster::getIsFinished()
{
    return m_isFinished || m_isFixed;
}

void CExposureAdjuster::recordExposureCommandTime(
        double _elapsed_ms,
        int _initial_exposure_us,
        int _final_exposure_us)
{
#if ENABLE_EXPOSURE_TIMING_LOG
    if (!m_exposureTimingActive || m_exposureTimingReported) {
        return;
    }

    if (m_initialExposure < 0) {
        m_initialExposure = _initial_exposure_us;
    }
    m_finalExposure = _final_exposure_us;
    m_setExposureTotalMs += _elapsed_ms;
#else
    Q_UNUSED(_elapsed_ms)
    Q_UNUSED(_initial_exposure_us)
    Q_UNUSED(_final_exposure_us)
#endif
}

void CExposureAdjuster::finishExposureTiming(
        const char *_result,
        int _final_exposure_us)
{
#if ENABLE_EXPOSURE_TIMING_LOG
    if (!m_exposureTimingActive || m_exposureTimingReported) {
        return;
    }

    m_exposureTimingReported = true;
    m_exposureTimingActive = false;
    if (_final_exposure_us > 0) {
        m_finalExposure = _final_exposure_us;
    }

    const double previewTotalMs =
            std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - m_previewStartedAt).count();
    const double adjustmentTotalMs = m_sampleCount > 0
            ? std::chrono::duration<double, std::milli>(
                      std::chrono::steady_clock::now() - m_adjustmentStartedAt).count()
            : -1.0;
    const QString resultText = _result ? QString::fromUtf8(_result) : QString("unknown");
    const double pupilDetectedToStableMs =
            resultText == QStringLiteral("stable") && m_hasFirstPupilDetected
            ? std::chrono::duration<double, std::milli>(
                      std::chrono::steady_clock::now()
                      - m_firstPupilDetectedAt).count()
            : -1.0;

    // 同一曝光会话只输出一条汇总日志，避免逐帧日志影响现场时序。
    qInfo().noquote()
            << QString("[ExposureTiming] result=%1 preview_total_ms=%2 "
                       "adjustment_total_ms=%3 algorithm_total_ms=%4 "
                       "sample_count=%5 adjustment_count=%6 "
                       "initial_exposure_us=%7 final_exposure_us=%8 "
                       "initial_gray=%9 final_gray=%10 set_exposure_total_ms=%11 "
                       "pupil_detected_to_stable_ms=%12 %13 %14")
               .arg(resultText)
               .arg(previewTotalMs, 0, 'f', 2)
               .arg(adjustmentTotalMs, 0, 'f', 2)
               .arg(m_algorithmTotalMs, 0, 'f', 2)
               .arg(m_sampleCount)
               .arg(m_adjustmentCount)
               .arg(m_initialExposure)
               .arg(m_finalExposure)
               .arg(m_initialGray)
               .arg(m_finalGray)
               .arg(m_setExposureTotalMs, 0, 'f', 2)
               .arg(pupilDetectedToStableMs, 0, 'f', 2)
               .arg(QString("initial_fast_mode=%1 candidate_try_count=%2 "
                            "proportional_adjust_count=%3 fine_adjust_count=%4")
                        .arg(m_initialFastMode ? "yes" : "no")
                        .arg(m_initialCandidateTryCount)
                        .arg(m_proportionalAdjustCount)
                        .arg(m_fineAdjustCount))
               .arg(QString("cold_start_test=%1 cold_start_applied=%2 "
                            "cold_start_exposure_us=%3")
                        .arg(m_coldStartTestActive ? "yes" : "no")
                        .arg(m_coldStartApplied ? "yes" : "no")
                        .arg(m_coldStartExposureUs));
#else
    Q_UNUSED(_result)
    Q_UNUSED(_final_exposure_us)
#endif
}
