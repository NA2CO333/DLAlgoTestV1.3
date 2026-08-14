#include "measurectrl.h"

#include "CameraIntf.h"
#include "windowsmanager.h"
#include "global.h"
#include "algo-invoker.h"
#include "../algo/perftimer.h"
#include "refractionstrategy.h"
#include "util-common.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QThreadPool>
#include <QtGlobal>
#include <QRunnable>

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
#include <atomic>
#include <chrono>
#include <memory>
#endif

extern bool saveImage;

namespace {

static constexpr int VALID_TURN_LAMP_IMAGE_COUNT = G_TURN_LAMP_FRAME_COUNT - 1;

struct stSavedTurnLampImage {
    int imageNum {-1};
    QByteArray imageBytes;
};

struct stTurnLampRoundSaveMeta {
    int imgCount {-1};
    int cachedCount {0};
    int sourceCachedCount {0};
    bool isAborted {false};
    enCaptureError captureError {captureError_Unknown};
    QString reason;
    QString sourceDirName;
    QString timestamp;
    int exposure {-1};
    bool isDistanceFit {false};
};

// 已完成图片写入、但尚未执行统一刷盘的轮次状态。
struct stTurnLampRoundSaveStatus {
    QString imgDir;
    int expected {VALID_TURN_LAMP_IMAGE_COUNT};
    int saved {0};
    int failed {0};
    int skipped {0};
    QString state;
    QString lastError;
};

// 转灯图始终使用一个后台线程顺序写入。同步状态独立加锁，不能阻塞测量线程。
QThreadPool g_turnLampSavePool;
bool g_isTurnLampSavePoolInited {false};
QMutex g_turnLampSaveMutex;
int g_turnLampSavePendingTaskCount {0};
bool g_turnLampSaveAcceptNewTask {true};
bool g_turnLampSaveFlushRequested {false};
bool g_turnLampSaveSyncing {false};
QList<stTurnLampRoundSaveStatus> g_turnLampSavePendingSyncStatuses;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
qint64 g_turnLampSaveFlushRequestedNs {0};

qint64 turnLampSaveNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
}

double turnLampSaveElapsedMs(qint64 _start_ns, qint64 _end_ns)
{
    if (_start_ns <= 0 || _end_ns < _start_ns) {
        return 0.0;
    }
    return static_cast<double>(_end_ns - _start_ns) / 1000000.0;
}

// 任务开始可能早于提交函数返回，用原子时间点把“提交结束”安全传给后台任务。
struct stTurnLampSaveTiming {
    qint64 enqueueStartNs {0};
    std::atomic<qint64> enqueueEndNs {0};
};

void logTurnLampSaveTiming(int _round_idx,
                           double _enqueue_ms,
                           double _queue_wait_ms,
                           double _write_ms,
                           double _task_total_ms,
                           int _saved,
                           int _failed,
                           int _skipped)
{
    qDebug().noquote()
            << QString("[SaveTiming] round=%1,enqueue_ms=%2,queue_wait_ms=%3,"
                       "write_ms=%4,task_total_ms=%5,saved=%6,failed=%7,skipped=%8")
               .arg(_round_idx)
               .arg(_enqueue_ms, 0, 'f', 2)
               .arg(_queue_wait_ms, 0, 'f', 2)
               .arg(_write_ms, 0, 'f', 2)
               .arg(_task_total_ms, 0, 'f', 2)
               .arg(_saved)
               .arg(_failed)
               .arg(_skipped);
}
#endif

void ensureTurnLampSavePool()
{
    QMutexLocker locker(&g_turnLampSaveMutex);
    if (!g_isTurnLampSavePoolInited) {
        g_turnLampSavePool.setMaxThreadCount(1);
        g_turnLampSavePool.setExpiryTimeout(30000);
        g_isTurnLampSavePoolInited = true;
    }
}

bool writeTurnLampRoundSaveStatus(const stTurnLampRoundSaveStatus &_status,
                                  int _pending_image_count,
                                  const QString &_sync_state)
{
    // 状态文件只在本轮图片保存结束后写一次，避免在后台保存线程反复创建临时文件和重命名。
    QFile statusFile(QString("%1/save_status.txt").arg(_status.imgDir));
    if (!statusFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        // 状态文件写入失败仍返回失败，但正式流程不额外输出存图诊断日志。
        return false;
    }

    QTextStream out(&statusFile);
    out << "state=" << _status.state << "\n";
    out << "expected=" << _status.expected << "\n";
    out << "saved=" << _status.saved << "\n";
    out << "failed=" << _status.failed << "\n";
    out << "skipped=" << _status.skipped << "\n";
    out << "pending=" << _pending_image_count << "\n";
    out << "sync_state=" << _sync_state << "\n";
    out << "last_error=" << _status.lastError << "\n";
    out << "updated_at=" << QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") << "\n";
    statusFile.close();
    return true;
}

void runTurnLampSaveFlush()
{
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    qint64 flushRequestedNs = 0;
#endif
    {
        QMutexLocker locker(&g_turnLampSaveMutex);
        g_turnLampSavePendingSyncStatuses.clear();
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        flushRequestedNs = g_turnLampSaveFlushRequestedNs;
#endif
    }

    // 仅在用户关闭“开启存图”后统一执行一次，不再在每一轮存图后强制刷盘。
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    const qint64 syncStartNs = turnLampSaveNowNs();
#endif
    Util::CUDisk::sync();
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    const qint64 syncEndNs = turnLampSaveNowNs();
#endif

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    int pendingTaskCount = 0;
#endif
    {
        QMutexLocker locker(&g_turnLampSaveMutex);
        g_turnLampSaveSyncing = false;
        g_turnLampSaveFlushRequested = false;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        g_turnLampSaveFlushRequestedNs = 0;
#endif
        // 用户可能在等待刷盘期间又重新开启存图，此时才重新接收新任务。
        g_turnLampSaveAcceptNewTask = saveImage;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        pendingTaskCount = g_turnLampSavePendingTaskCount;
#endif
    }

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    qDebug().noquote()
            << QString("[SaveTiming] flush_wait_ms=%1,sync_ms=%2,pending=%3")
               .arg(turnLampSaveElapsedMs(flushRequestedNs, syncStartNs), 0, 'f', 2)
               .arg(turnLampSaveElapsedMs(syncStartNs, syncEndNs), 0, 'f', 2)
               .arg(pendingTaskCount);
#endif
}

class CFlushTurnLampSaveQueueTask : public QRunnable
{
public:
    CFlushTurnLampSaveQueueTask() { setAutoDelete(true); }
    void run() override { runTurnLampSaveFlush(); }
};

void finishTurnLampSaveTask(const stTurnLampRoundSaveStatus &_status)
{
    bool needSyncNow = false;
    {
        QMutexLocker locker(&g_turnLampSaveMutex);
        if (!_status.imgDir.isEmpty()) {
            g_turnLampSavePendingSyncStatuses.append(_status);
        }

        if (g_turnLampSavePendingTaskCount > 0) {
            --g_turnLampSavePendingTaskCount;
        }

        if (g_turnLampSaveFlushRequested
                && g_turnLampSavePendingTaskCount == 0
                && !g_turnLampSaveSyncing) {
            g_turnLampSaveSyncing = true;
            needSyncNow = true;
        }
    }

    // 当前任务已经是队列中的最后一个任务，直接在该后台线程完成统一刷盘。
    if (needSyncNow) {
        runTurnLampSaveFlush();
    }
}

QString captureErrorToText(enCaptureError _capture_error)
{
    switch (_capture_error) {
    case captureError_NoError         : return "NoError";
    case captureError_FrameSetEmpty   : return "FrameSetEmpty";
    case captureError_FrameSingle     : return "FrameSingle";
    case captureError_FrameLoss       : return "FrameLoss";
    case captureError_FrameExcess     : return "FrameExcess";
    case captureError_CameraUnusable  : return "CameraUnusable";
    case captureError_Unknown         : return "Unknown";
    default                           : return "Unknown";
    }
}

QString distanceStateToText(enDistanceState _state)
{
    switch (_state) {
    case distStat_Unknown : return "Unknown";
    case distStat_TooNear : return "TooNear";
    case distStat_FitNear : return "FitNear";
    case distStat_Fit     : return "Fit";
    case distStat_FitFar  : return "FitFar";
    case distStat_TooFar  : return "TooFar";
    default               : return "Unknown";
    }
}

class CSaveTurnLampRoundTask : public QRunnable
{
public:
    CSaveTurnLampRoundTask(const QString &_usb_root,
                           const QString &_img_dir_name,
                           const QString &_batch_dir_name,
                           const QString &_source_dir_name,
                           int _round_idx,
                           const QVector<stSavedTurnLampImage> &_images,
                           const stTurnLampRoundSaveMeta &_meta
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
                           ,const std::shared_ptr<stTurnLampSaveTiming> &_timing
#endif
                           )
        : m_usbRoot(_usb_root),
          m_imgDirName(_img_dir_name),
          m_batchDirName(_batch_dir_name),
          m_sourceDirName(_source_dir_name),
          m_roundIdx(_round_idx),
          m_images(_images),
          m_meta(_meta)
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
          ,m_timing(_timing)
#endif
    {
        setAutoDelete(true);
    }

    void run() override
    {
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        const qint64 workerStartNs = turnLampSaveNowNs();
        const qint64 enqueueEndNs = m_timing
                ? m_timing->enqueueEndNs.load(std::memory_order_acquire) : 0;
        const double enqueueMs = m_timing
                ? turnLampSaveElapsedMs(m_timing->enqueueStartNs, enqueueEndNs)
                : 0.0;
        const double queueWaitMs = turnLampSaveElapsedMs(enqueueEndNs, workerStartNs);
#endif

        // U 盘根目录在一次测量开始时已固定到任务快照中；后台线程不得再次 mount -l。
        if (m_usbRoot.isEmpty() || !QDir(m_usbRoot).exists()) {
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
            logTurnLampSaveTiming(m_roundIdx, enqueueMs, queueWaitMs, 0.0,
                                  turnLampSaveElapsedMs(workerStartNs,
                                                        turnLampSaveNowNs()),
                                  0, m_images.size(), 0);
#endif
            finishTurnLampSaveTask(stTurnLampRoundSaveStatus());
            return;
        }

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        const qint64 writeStartNs = turnLampSaveNowNs();
#endif
        const QString imgDir = QString("%1/screener_images/%2/%3/%4/round_%5")
                .arg(m_usbRoot)
                .arg(m_imgDirName)
                .arg(m_batchDirName)
                .arg(m_sourceDirName)
                .arg(m_roundIdx, 2, 10, QLatin1Char('0'));
        Util::makePath(imgDir);

        stTurnLampRoundSaveStatus saveStatus;
        saveStatus.imgDir = imgDir;
        saveStatus.state = "writing";
        // 缺图/非法图也要反映在状态文件中，expected 始终保持完整一轮的 22 张。
        saveStatus.skipped = qMax(0, VALID_TURN_LAMP_IMAGE_COUNT - m_images.size());
        // 先写轮次信息，即使本轮没有有效图片，也能留下诊断线索。
        QFile infoFile(QString("%1/round_info.txt").arg(imgDir));
        if (infoFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&infoFile);
            out << "round_index=" << m_roundIdx << "\n";
            out << "source=" << m_meta.sourceDirName << "\n";
            out << "img_count=" << m_meta.imgCount << "\n";
            out << "cached_count=" << m_meta.cachedCount << "\n";
            out << "source_cached_count=" << m_meta.sourceCachedCount << "\n";
            out << "aborted=" << (m_meta.isAborted ? "true" : "false") << "\n";
            out << "capture_error=" << captureErrorToText(m_meta.captureError) << "\n";
            out << "reason=" << m_meta.reason << "\n";
            out << "exposure=" << m_meta.exposure << "\n";
            out << "distance_fit=" << (m_meta.isDistanceFit ? "true" : "false") << "\n";
            out << "timestamp=" << m_meta.timestamp << "\n";
            infoFile.close();
        }

        int savedCount = 0;
        int failedCount = 0;
        int skippedCount = saveStatus.skipped;

        for (stSavedTurnLampImage image : m_images) {
            if (image.imageNum < 1
                    || image.imageNum > VALID_TURN_LAMP_IMAGE_COUNT
                    || image.imageBytes.size() < IMG_WIDTH * IMG_HEIGHT) {
                skippedCount++;
                continue;
            }

            QString filePath = QString("%1/temp%2.bmp")
                    .arg(imgDir)
                    .arg(image.imageNum, 2, 10, QLatin1Char('0'));
            bool succ = Util::saveImgDataToImgFile2(
                        reinterpret_cast<uchar *>(image.imageBytes.data()),
                        IMG_WIDTH,
                        IMG_HEIGHT,
                        filePath,
                        1);
            if (!succ) {
                failedCount++;
            } else {
                savedCount++;
            }
        }

        saveStatus.saved = savedCount;
        saveStatus.failed = failedCount;
        saveStatus.skipped = skippedCount;
        saveStatus.state = (failedCount > 0) ? "write_failed" : "write_completed";
        if (failedCount > 0) {
            saveStatus.lastError = QString("image_write_failed=%1").arg(failedCount);
        }
        // 一轮完成后 pending 必须为 0；刷盘由关闭“开启存图”时统一触发。
        writeTurnLampRoundSaveStatus(saveStatus, 0, "deferred");
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        const qint64 writeEndNs = turnLampSaveNowNs();
        logTurnLampSaveTiming(m_roundIdx, enqueueMs, queueWaitMs,
                              turnLampSaveElapsedMs(writeStartNs, writeEndNs),
                              turnLampSaveElapsedMs(workerStartNs, writeEndNs),
                              savedCount, failedCount, skippedCount);
#endif
        finishTurnLampSaveTask(saveStatus);
    }

private:
    QString m_usbRoot;
    QString m_imgDirName;
    QString m_batchDirName;
    QString m_sourceDirName;
    int m_roundIdx {-1};
    QVector<stSavedTurnLampImage> m_images;
    stTurnLampRoundSaveMeta m_meta;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    std::shared_ptr<stTurnLampSaveTiming> m_timing;
#endif
};

void saveTurnLampRoundImagesAsync(const QVector<stTurnLampImageInfo> &_images,
                                  const QString &_usb_root,
                                  const QString &_img_dir_name,
                                  const QString &_batch_dir_name,
                                  const QString &_source_dir_name,
                                  int _round_idx,
                                  const stTurnLampRoundSaveMeta &_meta)
{
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    const auto timing = std::make_shared<stTurnLampSaveTiming>();
    timing->enqueueStartNs = turnLampSaveNowNs();
#endif
    QVector<stSavedTurnLampImage> copiedImages;
    copiedImages.reserve(_images.size());

    const int dataLen = IMG_WIDTH * IMG_HEIGHT;
    for (const stTurnLampImageInfo &image : _images) {
        if (image.imageNum < 1
                || image.imageNum > VALID_TURN_LAMP_IMAGE_COUNT
                || image.imageData == nullptr) {
            continue;
        }

        stSavedTurnLampImage copied;
        copied.imageNum = image.imageNum;
        copied.imageBytes = QByteArray(reinterpret_cast<const char *>(image.imageData), dataLen);
        copiedImages.append(copied);
    }

    stTurnLampRoundSaveMeta meta = _meta;
    meta.cachedCount = copiedImages.size();
    meta.sourceCachedCount = _images.size();
    meta.sourceDirName = _source_dir_name;

    // 保存图片可能涉及 U 盘 IO，后台单线程顺序写入，避免阻塞转灯采集。
    ensureTurnLampSavePool();
    {
        QMutexLocker locker(&g_turnLampSaveMutex);
        if (!g_turnLampSaveAcceptNewTask) {
            return;
        }
        ++g_turnLampSavePendingTaskCount;
    }

#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
    auto *saveTask = new CSaveTurnLampRoundTask(_usb_root,
                                                 _img_dir_name,
                                                 _batch_dir_name,
                                                 _source_dir_name,
                                                 _round_idx,
                                                 copiedImages,
                                                 meta,
                                                 timing);
    // 提交时间必须在启动任务前写入，后台线程无需反向等待提交线程。
    timing->enqueueEndNs.store(turnLampSaveNowNs(), std::memory_order_release);
    g_turnLampSavePool.start(saveTask, -1);
#else
    g_turnLampSavePool.start(new CSaveTurnLampRoundTask(_usb_root,
                                                         _img_dir_name,
                                                         _batch_dir_name,
                                                         _source_dir_name,
                                                         _round_idx,
                                                         copiedImages,
                                                         meta),
                            -1);
#endif

}

} // namespace

// =====================================================================================================================
//
// =====================================================================================================================

const char *enumToText_MeasureStep(enMeasureStep _step)
{
    switch (_step) {
    case measureStep_Unknow             : return "Unknow";
    case measureStep_Ready              : return "Ready";
    case measureStep_Collect            : return "Collect";
    case measureStep_Calc               : return "Calc";
    case measureStep_CalcFinished       : return "CalcFinished";
    case measureStep_MeasureFinished    : return "MeasureFinished";
    default                             : return "??";
    }
}

// =====================================================================================================================
// class CMeasureCtrl
// =====================================================================================================================

CMeasureCtrl::CMeasureCtrl(CCaptureThread *_capture_thread, QObject *parent) :
    QObject(parent),
    m_captureThread(_capture_thread)
{
    m_listDistInfo = new std::vector<stDistInfo>();

    m_listFrameInfo = new std::vector<stFrameInfo>();
    m_listImgSetInfo = new std::vector<stImgSetInfo>();

    m_imgSet = new std::vector<uchar *>();

    m_frameBuff = new std::vector<uchar *>();

}

CMeasureCtrl::~CMeasureCtrl()
{
    doBeforeMeasure();

    //
    for (int i = m_frameBuff->size() - 1; i >= 0; i--) {
        uchar *buff = m_frameBuff->at(i);
        free(buff);
    }
    m_frameBuff->clear();

    delete m_frameBuff;
    m_frameBuff = Q_NULLPTR;

}

void CMeasureCtrl::setTurnLampImageSaveEnabled(bool _enabled)
{
    QMutexLocker locker(&g_turnLampSaveMutex);
    if (_enabled) {
        // 正在刷盘时不接收新任务；刷盘完成后会根据 saveImage 自动恢复。
        if (!g_turnLampSaveSyncing && !g_turnLampSaveFlushRequested) {
            g_turnLampSaveAcceptNewTask = true;
        }
    } else {
        g_turnLampSaveAcceptNewTask = false;
    }
}

void CMeasureCtrl::flushTurnLampImageSaveQueue()
{
    ensureTurnLampSavePool();

    bool needScheduleFlushTask = false;
    {
        QMutexLocker locker(&g_turnLampSaveMutex);
        // 先关入口，确保之后不会再插入新的存图任务。
        g_turnLampSaveAcceptNewTask = false;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        if (!g_turnLampSaveFlushRequested) {
            g_turnLampSaveFlushRequestedNs = turnLampSaveNowNs();
        }
#endif
        g_turnLampSaveFlushRequested = true;

        if (g_turnLampSavePendingTaskCount == 0 && !g_turnLampSaveSyncing) {
            g_turnLampSaveSyncing = true;
            needScheduleFlushTask = true;
        }

    }

    // 队列为空时也放到后台执行，避免工程师模式界面被 sync() 阻塞。
    if (needScheduleFlushTask) {
        g_turnLampSavePool.start(new CFlushTurnLampSaveQueueTask(), -1);
    }
}

void CMeasureCtrl::setAlgoInvoker(CAlgoInvoker *_algo_invoker)
{
    m_algoInvoker = _algo_invoker;
}

void CMeasureCtrl::doBeforeMeasure()
{
    m_isExposureOk = false;

    m_listDistInfo->clear();
    m_listFrameInfo->clear();
    m_listImgSetInfo->clear();

    m_imgSet->clear();

    m_turnLampSaveBatchName = QString("measure_%1")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
    m_turnLampSaveSourceName = "auto_detect";
    // 只在一轮测量开始时读取一次挂载信息。所有后台存图和算法状态任务均持有此路径快照。
    m_turnLampSaveUsbRoot = saveImage ? Util::CUDisk::getPath() : QString();
    if (m_algoInvoker) {
        // 只有开启存图时才同步算法状态文件目录；未开启存图时清空上下文，避免生成空目录/algo_status.txt。
        if (saveImage) {
            m_algoInvoker->executeAlgoCommand(
                    stAlgoCommand::makeSetTurnLampSaveContext(
                            g_WinMeasure->currPatient().getImgDirName().toStdString(),
                            m_turnLampSaveBatchName.toStdString(),
                            m_turnLampSaveSourceName.toStdString(),
                            m_turnLampSaveUsbRoot.toStdString()));
        } else {
            m_algoInvoker->executeAlgoCommand(
                    stAlgoCommand::makeSetTurnLampSaveContext(
                            std::string(), std::string(),
                            std::string(), std::string()));
        }
    }

    //
    //for (int i = m_frameBuff->size() - 1; i >= 0; i--) {
    //    uchar *buff = m_frameBuff->at(i);
    //    free(buff);
    //}
    //m_frameBuff->clear();

    //
    RefractionStrategy::setStableCount(CGlobal::resultStableCountThresh);
    RefractionStrategy::setRangeThreshold(CGlobal::resultStableDiopterThresh);
    RefractionStrategy::setMaxRecordCount(CGlobal::countMaxMeasureTimes);

}

void CMeasureCtrl::doBeforeTurnLamp()
{
    //
    m_turnLampImageSets.append(QVector<stTurnLampImageInfo>());

}

void CMeasureCtrl::doAfterTurnLamp(int _img_count, bool _is_aborted, enCaptureError _capture_error, const QString &_reason)
{
    saveLatestTurnLampImages(_img_count, _is_aborted, _capture_error, _reason);
}

void CMeasureCtrl::doAfterMeasure()
{
    //
    releaseTurnLampImageSets();

}

void CMeasureCtrl::setExposureTime(int *_exposure_time)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): _exposure_time = " << *_exposure_time;

    stCameraStatInfo stat_info = g_CameraIntf->setExposureTime(_exposure_time);
    if (cameraStat_Unrecoverable == stat_info.cameraStat) {
        logCritical("CMeasureCtrl::setExposureTime(): camera unable work!", CGlobal::LOG_CAPTURE);
        m_captureThread->sigCaptureErr(captureError_CameraUnusable, QString::asprintf("err[%d] on set exposure", stat_info.errCode) + ". " + stat_info.errMsg);
    }
}

int CMeasureCtrl::getExposureTime()
{
    return g_CameraIntf->getExposureTime();
}

bool CMeasureCtrl::getOneFrameMem(uchar *&_img_data, int &_img_idx)
{
    static const int data_len = IMG_WIDTH_WHOLE * IMG_HEIGHT_WHOLE;
    // TODO: 如果这里设置为 ROI 后的像素总数，在视筛里（libMVSDK.so.2.1.0.20）跑几十帧后会 “malloc(): smallbin double linked list corrupted”（也有过“std::bad_alloc”？）之类异常，但是 PC 里不会。
    /* sdk的bug？导致即使 ROI 了但 CameraImageProcess() 依然访问了整图像素导致非法内存访问？ */

    //
    const int MAX_BUFF_COUNT = 200;

    //
    uchar *buff = Q_NULLPTR;
    static int idx = -1;

    if (m_frameBuff->size() < MAX_BUFF_COUNT) {
        buff = (uchar *)malloc(data_len);
        //memset(buff, 0, data_len);
        m_frameBuff->push_back(buff);
        idx = m_frameBuff->size() - 1;
    } else {
        idx++;
        if (idx > MAX_BUFF_COUNT - 1) {
            idx = 0;
        }
        buff = m_frameBuff->at(idx);
    }

    _img_data = buff;
    _img_idx = idx;

    return true;
}

std::vector<uchar *> *CMeasureCtrl::getImgSet()
{
    return m_imgSet;
}

bool CMeasureCtrl::executeAlgoPolicy(std::vector<stVisionValue> &_result_set, stVisionValue &_vision, stVisionAbnormal &_vision_abnormal, bool &_questionable)
{
    return RefractionStrategy::executeAlgoPolicy(_result_set, _vision, _vision_abnormal, _questionable);
}

void CMeasureCtrl::startMeasure()
{
    emit sigStartMeasure();
}

void CMeasureCtrl::judgeTurnLamp()
{
    // 统一使用WinMeasure的距离判断，同时兼容L型箱和忽略距离模式。
    const bool is_dist_ok =
            g_WinMeasure && g_WinMeasure->getIsDistanceFit();

    if (is_dist_ok && getIsExposureOk()) {
        logDebug(QString("%1: distance and exposure all OK! emit goto turn lamp...").arg(__PRETTY_FUNCTION__), CGlobal::LOG_MEASURE);
        jumpIntoMeasureStepImmediately(measureStep_Collect);
    }
}

void CMeasureCtrl::jumpIntoMeasureStepLatter(enMeasureStep _step)
{
    emit sigGoIntoMeasureStep(_step);
}

bool CMeasureCtrl::jumpIntoMeasureStepImmediately(enMeasureStep _step, bool _is_force)
{
    return g_WinMeasure->goIntoMeasureStep(_step, _is_force);
}

void CMeasureCtrl::savePreviewImages()
{
    // 保存预览图
    if (g_isSaveSampleImage) {
        // 将 12、18 图保存为预览图
        qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): saving image 12 & 18 ...";
        if (m_img12 && m_img18) {
            QString img_dir_name = g_WinMeasure->currPatient().getImgDirName();
            CAlgoInvoker::testSaveByteImageinFolder_(m_img12, 12, img_dir_name, 3);
            CAlgoInvoker::testSaveByteImageinFolder_(m_img18, 18, img_dir_name, 3);
        } else {
            qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): Failed to save preview images: image set is empty!";
        }
    }
}

void CMeasureCtrl::slotIsExposureOk()
{
    m_isExposureOk = true;

    // 曝光刚完成时立即重新判断，避免最后一次有效瞳孔识别先于曝光状态到达而错过转灯。
    judgeTurnLamp();
}

bool CMeasureCtrl::getIsExposureOk()
{
    return m_isExposureOk;
}

void CMeasureCtrl::inputDist(int _dist_val, enDistanceState _dist_stat)
{
    m_listDistInfo->push_back({_dist_val, _dist_stat});
}

void CMeasureCtrl::inputTurnLampImg(int _img_idx, int _img_num)
{
    //
    uchar *img_data = (*m_frameBuff)[_img_idx];     // TODO: 转灯图应该立即拷贝一份，不应留在缓冲区，否则可能被后续帧覆盖，导致图像错乱！

    // 添加帧信息到转灯图缓冲队列
    stFrameInfo img_info;

    img_info.idxBuff = _img_idx;
    img_info.num = _img_num;
    img_info.idxDist = m_listDistInfo->size() - 1;

    m_listFrameInfo->push_back(img_info);

    // 图像数据拷贝
    const int img_width = IMG_WIDTH;
    const int img_height = IMG_HEIGHT;
    int data_len = img_width * img_height;

    uchar *buffer = new uchar[data_len];
    memcpy(buffer, img_data, data_len);
    img_data = buffer;

    // 保留相机传入的真实帧编号。第0帧是转灯起始/清缓存帧，
    // 不对应灯位，因此不做光路编号转换；是否参与计算由算法模块决定。
    int img_num = _img_num;
    if (img_num != 0
            && opticalPathType_General != g_opticalPathType) {
        QString err_msg;
        bool succ = m_algoInvoker->convertImageAndNumberToGeneralOpticalType(g_opticalPathType, img_data, img_num, img_num, err_msg);
        if (!succ) {
            logCritical(__PRETTY_FUNCTION__ + QString(": ") + err_msg);
        }
    }

    // 图像数据添加到转灯图集列表
    m_turnLampImageSets.last().append( stTurnLampImageInfo { img_num, img_data } );

    //
    if (12 == img_num) {
        m_img12 = img_data;
    } else if (18 == img_num) {
        m_img18 = img_data;
    }

    // 推送转灯帧图像到算法模块     // NOTE: (2025-11-18)新算法策略
    //int round_idx = m_listImgSetInfo->size() - 1;
    int round_idx = g_WinMeasure->countTurnLamp() - 1;
    // 正式照片与预览阶段统一使用WinMeasure的距离结论，兼容L型箱忽略距离模式。
    const bool is_dist_fit =
            g_WinMeasure && g_WinMeasure->getIsDistanceFit();

    // 第0～22帧统一提交算法；第0帧由CAlgo::appendImage()接收后忽略。
    m_algoInvoker->appendImage(img_data, IMG_WIDTH, IMG_HEIGHT, round_idx, img_num, is_dist_fit);
    //qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): _img_idx = " << _img_idx << ", img_data: " << reinterpret_cast<uintptr_t>(img_data) << ", img_num: " << img_num;

}

bool CMeasureCtrl::inputTurnLampOnce(int _img_count)
{
    qDebug() << "CMeasureCtrl::" << __FUNCTION__ << "() into ... " << "img_count = " << _img_count;

    // 构建一个图集
    stImgSetInfo img_set_info;
    for (int i = m_listFrameInfo->size() - 1; i >= 0; i--) {
        const stFrameInfo &frame_info = m_listFrameInfo->at(i);
        if (0 == frame_info.num) {
            img_set_info.idxFirst = i;
            img_set_info.imgCount = m_listFrameInfo->size() - img_set_info.idxFirst;

            if (_img_count != img_set_info.imgCount) {
                logCritical(QString("%1: assert failed: img count err!").arg(__PRETTY_FUNCTION__), CGlobal::LOG_MEASURE);

                img_set_info.reset();

                // TODO: ?

            }

            //
            break;
        }
    }

    //
    bool succ = false;

    if (img_set_info.isValid()) {
        //
        m_listImgSetInfo->push_back(img_set_info);

        // 检查是否可以进入计算阶段
        succ = judgeCanIntoCalc();
    } else {
        logCritical(QString("%1::%2(): img_set_info is not valid?!").arg(S_CLASS_NAME).arg(__FUNCTION__));

    }

    //
    qDebug() << "CMeasureCtrl::" << __FUNCTION__ << "() ended ... " << "succ = " << Util::bool2str(succ);
    return succ;
}

void CMeasureCtrl::setTurnLampSaveSource(const QString &_source_name)
{
    if (_source_name.isEmpty()) {
        m_turnLampSaveSourceName = "auto_detect";
    } else {
        m_turnLampSaveSourceName = _source_name;
    }
    if (m_algoInvoker) {
        // 来源目录可能在手动转灯、自动转灯、测试模式之间切换；未开启存图时保持算法侧上下文为空。
        if (saveImage) {
            m_algoInvoker->executeAlgoCommand(
                    stAlgoCommand::makeSetTurnLampSaveContext(
                            g_WinMeasure->currPatient().getImgDirName().toStdString(),
                            m_turnLampSaveBatchName.toStdString(),
                            m_turnLampSaveSourceName.toStdString(),
                            m_turnLampSaveUsbRoot.toStdString()));
        } else {
            m_algoInvoker->executeAlgoCommand(
                    stAlgoCommand::makeSetTurnLampSaveContext(
                            std::string(), std::string(),
                            std::string(), std::string()));
        }
    }
}

void CMeasureCtrl::saveLatestTurnLampImages(int _img_count, bool _is_aborted, enCaptureError _capture_error, const QString &_reason)
{
    if (!saveImage) {
        return;
    }

    if (m_turnLampImageSets.isEmpty()) {
        return;
    }

    const int imgSetIndex = m_turnLampImageSets.size() - 1;
    const bool isDistanceFit = (!m_listDistInfo->empty() && distStat_Fit == m_listDistInfo->back().distStat);
    stTurnLampRoundSaveMeta meta;
    meta.imgCount = _img_count;
    meta.isAborted = _is_aborted;
    meta.captureError = _capture_error;
    meta.reason = _reason;
    meta.timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    meta.exposure = g_CameraIntf->getExposureTime();
    meta.isDistanceFit = isDistanceFit;

    saveTurnLampRoundImagesAsync(m_turnLampImageSets.at(imgSetIndex),
                                 m_turnLampSaveUsbRoot,
                                 g_WinMeasure->currPatient().getImgDirName(),
                                 m_turnLampSaveBatchName,
                                 m_turnLampSaveSourceName,
                                 imgSetIndex,
                                 meta);
}

void CMeasureCtrl::saveAllCompleteTurnLampImages(int _img_count)
{
    if (!saveImage) {
        return;
    }

    for (int i = 0; i < m_turnLampImageSets.size(); ++i) {
        if (m_turnLampImageSets.at(i).size() < VALID_TURN_LAMP_IMAGE_COUNT) {
            continue;
        }

        stTurnLampRoundSaveMeta meta;
        meta.imgCount = _img_count;
        meta.isAborted = false;
        meta.captureError = captureError_NoError;
        meta.reason = "save_all_complete";
        meta.timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
        meta.exposure = g_CameraIntf->getExposureTime();
        meta.isDistanceFit = (!m_listDistInfo->empty() && distStat_Fit == m_listDistInfo->back().distStat);

        saveTurnLampRoundImagesAsync(m_turnLampImageSets.at(i),
                                     m_turnLampSaveUsbRoot,
                                     g_WinMeasure->currPatient().getImgDirName(),
                                     m_turnLampSaveBatchName,
                                     m_turnLampSaveSourceName,
                                     i,
                                     meta);
    }

}

//void CMeasureCtrl::inputPupilDetectInfo(bool _is_succ, int _img_idx, stPupilInfo _pupil_info_r, stPupilInfo _pupil_info_l)
//{
//    //
//    int idx_frame = -1;
//    for (int i = m_listFrameInfo->size() - 1; i >= 0; i--) {
//        if (_img_idx == (*m_listFrameInfo)[i].idxBuff) {
//            idx_frame = i;
//            break;
//        }
//    }
//
//    if (idx_frame >= 0) {
//        stFrameInfo &frame_info = (*m_listFrameInfo)[idx_frame];
//        frame_info.detecStat = (_is_succ ? 1 : 2);
//
//        if (_is_succ) {
//            frame_info.ptPupilR.setX(_pupil_info_r.center.x);
//            frame_info.ptPupilR.setY(_pupil_info_r.center.y);
//            frame_info.ptBlinkR.setX(_pupil_info_r.spotPt.x);
//            frame_info.ptBlinkR.setY(_pupil_info_r.spotPt.y);
//            frame_info.circularityR = _pupil_info_r.circularity;
//
//            frame_info.ptPupilL.setX(_pupil_info_l.center.x);
//            frame_info.ptPupilL.setY(_pupil_info_l.center.y);
//            frame_info.ptBlinkL.setX(_pupil_info_l.spotPt.x);
//            frame_info.ptBlinkL.setY(_pupil_info_l.spotPt.y);
//            frame_info.circularityL = _pupil_info_l.circularity;
//        } else {
//#if (OS_TYPE == 2)
//            // 调试时存图
//            static bool is_save_fail = false;
//            if (is_save_fail) {
//                QString file_path = QString::asprintf("/root/debug/idxBuff_%.2d.png", _img_idx);
//                Util::saveImgDataToImgFile2((*m_frameBuff)[_img_idx], IMG_WIDTH, IMG_HEIGHT, file_path);
//            }
//#endif
//        }
//
//        // 如果是图集的最后一张，则检查是否可以进入计算阶段
//        if (22 == frame_info.num) {                       // TODO: 因为判断转灯结束，需要等待，所以可确定这里比图集结束消息那里慢？但是时间长了之后，瞳孔识别会不会滞后？
//            judgeCanIntoCalc();
//        }
//
//    } else {
//        logCritical("CMeasureCtrl::inputPupilDetectInfo(): img idx out of bound!", CGlobal::LOG_MEASURE);
//
//        // TODO:
//
//    }
//}

bool CMeasureCtrl::judgeCanIntoCalc()
{
    qDebug() << "CMeasureCtrl::" << __FUNCTION__ << "() into ... ";

    //
    bool is_img_set_ok = false;      // 图集是否满足计算要求

    //
    do {
        // 检查和构建用于计算的图集
        //if (!is_img_set_ok || m_isMultiMeasure)
        {
            if (0 == m_listImgSetInfo->size()) {
                logCritical(QString("%1: img set list is empty!").arg(__PRETTY_FUNCTION__), CGlobal::LOG_MEASURE);

                break;
            }

            //
            const stImgSetInfo &img_set_info = m_listImgSetInfo->at(m_listImgSetInfo->size() - 1);

            //
            int idx_frame_from = img_set_info.idxFirst;
            int idx_frame_to = idx_frame_from + img_set_info.imgCount - 1;
            if (idx_frame_from < 0 || idx_frame_to > (int)m_listFrameInfo->size() - 1) {
                QString err_idx = "idx of frameInfo out of range!";
                emit sigErrMsg(err_idx);

                break;
            }

            // 检查并选择可用的图集
            //bool img_set_ok = checkImgSet(img_set_info, err_str);
            bool img_set_ok = true;

            // 各图的距离不能超出景深范围
            //if (!isIgnoreDist()) {
            //    QString err_dist;
            //    img_set_ok = checkImgSetDist_2(img_set_info, err_dist);
            //    if (!img_set_ok) {
            //        //emit sigErrMsg(err_dist);
            //        emit sigErrMsg(tr("请保持不动"));    // "Please stay still"
            //
            //        break;
            //    }
            //}

            //
            if (img_set_ok) {
                m_imgSet->clear();
                stFrameInfo frame_info;
                for (int i = idx_frame_from; i <= idx_frame_to; i++) {
                    frame_info = (*m_listFrameInfo)[i];
                    m_imgSet->push_back((*m_frameBuff)[frame_info.idxBuff]);
                }

                //
                is_img_set_ok = true;
            } else {
                //

            }
        //} else {
        //    logCritical("CMeasureCtrl::(): logic error, is_img_set_ok is true but still judging?", CGlobal::LOG_MEASURE);
        //
        //    // TODO: ?
        //
        }
    } while (false);

    if (is_img_set_ok) {
        // 进入计算阶段
        //jumpIntoMeasureStepImmediately(measureStep_Calc);        // NOTE: (2025-11-18)旧的算法流程

        // 直接使转灯模块进入计算状态        // NOTE: (2025-11-18)新算法策略
        jumpIntoMeasureStepImmediately(measureStep_Calc);
    } else {
        qCritical() << "CMeasureCtrl::judgeCanIntoCalc(): m_imgSet error! what wrong?";

        //jumpIntoMeasureStepImmediately(measureStep_Collect);       // TODO: ？？
    }

    //
    qDebug() << "CMeasureCtrl::" << __FUNCTION__ << "() ended ... " << "is_img_set_ok = " << Util::bool2str(is_img_set_ok);
    return is_img_set_ok;
}

void CMeasureCtrl::releaseTurnLampImageSets()
{
    // 清空转灯图集
    for (const QVector<stTurnLampImageInfo>&img_set : m_turnLampImageSets) {
        for (const stTurnLampImageInfo img_info : img_set) {
            delete [] img_info.imageData;
        }
    }
    m_turnLampImageSets.clear();

    // 这两个图像数据是来自转灯图集，须同步清空
    m_img12 = nullptr;
    m_img18 = nullptr;

}

//bool CMeasureCtrl::checkImgSet(stImgSetInfo &_img_set_info, QString &_err_str)
//{
//    // 图集数量检查
//    if (_img_set_info.imgCount != G_TURN_LAMP_FRAME_COUNT) {
//        _err_str = QString("img count ") + (_img_set_info.imgCount < G_TURN_LAMP_FRAME_COUNT ? "less" : "more") + " than " + QString::number(G_TURN_LAMP_FRAME_COUNT) + "!";
//        logInfo("CMeasureCtrl::checkImgSet(): " + _err_str, CGlobal::LOG_MEASURE);
//        return false;
//    }
//
//    //
//    int idx_begin = _img_set_info.idxFirst;
//    int idx_end = idx_begin + _img_set_info.imgCount - 1;
//    if (idx_end > (int)m_listFrameInfo->size()) {
//        _err_str = "img count less than " + QString::number(G_TURN_LAMP_FRAME_COUNT) + "!";
//        logInfo("CMeasureCtrl::checkImgSet(): " + _err_str, CGlobal::LOG_MEASURE);
//        return false;
//    }
//
//    // 距离检查                         // TODO: 0.5s 转一次灯，测距只要检查一次即可？或者加大测距频度？
//    bool is_dist_ok = checkImgSetDist(_img_set_info, _err_str);
//    if (!is_dist_ok) {
//        return false;
//    }
//
//    // 瞳孔坐标、映光点坐标、圆度检查
//    int min_pupil_r_x = 0xFFFF, max_pupil_r_x = -1, min_pupil_r_y = 0xFFFF, max_pupil_r_y = -1;
//    int min_pupil_l_x = 0xFFFF, max_pupil_l_x = -1, min_pupil_l_y = 0xFFFF, max_pupil_l_y = -1;
//
//    int min_blink_r_x = 0xFFFF, max_blink_r_x = -1, min_blink_r_y = 0xFFFF, max_blink_r_y = -1;
//    int min_blink_l_x = 0xFFFF, max_blink_l_x = -1, min_blink_l_y = 0xFFFF, max_blink_l_y = -1;
//
//    double min_circularity_r = 0xFFFF, max_circularity_r = -1;
//    double min_circularity_l = 0xFFFF, max_circularity_l = -1;
//
//    stFrameInfo frame_info;
//    for (int i = idx_begin; i <= idx_end; i++) {
//        frame_info = (*m_listFrameInfo)[i];
//
//        // 瞳孔是否识别成功
//        if (1 != frame_info.detecStat) {
//            if (CGlobal::isDebugMode) {
//                static bool is_save_img_set = false;
//                if (is_save_img_set) {
//                    QString file_path = QString("/root/debug/measureCtrl_") + QDateTime::currentDateTime().toString("yyyyMMddmm");
//                    Util::makePath(file_path);
//                    file_path += QDir::separator() + QString("temp_%1.bmp");
//                    uchar *img_data = Q_NULLPTR;
//                    for (int i = idx_begin; i <= idx_end; i++) {
//                        stFrameInfo frame_info_test = (*m_listFrameInfo)[i];
//                        img_data = (*m_frameBuff)[frame_info_test.idxBuff];
//                        Util::saveImgDataToImgFile2(img_data, IMG_WIDTH, IMG_HEIGHT, file_path.arg(i - idx_begin, 2, 10, QLatin1Char('0')), 1);
//                    }
//                    is_save_img_set = false;
//                }
//            }
//
//            //
//            _err_str = QString::asprintf("pupil not detected: %d !", frame_info.detecStat);
//            return false;
//        }
//
//        //
//        if (min_pupil_r_x > frame_info.ptPupilR.x())
//            min_pupil_r_x = frame_info.ptPupilR.x();
//        if (min_pupil_r_y > frame_info.ptPupilR.y())
//            min_pupil_r_y = frame_info.ptPupilR.y();
//
//        if (min_pupil_l_x > frame_info.ptPupilL.x())
//            min_pupil_l_x = frame_info.ptPupilL.x();
//        if (min_pupil_l_y > frame_info.ptPupilL.y())
//            min_pupil_l_y = frame_info.ptPupilL.y();
//
//        if (min_blink_r_x > frame_info.ptBlinkR.x())
//            min_blink_r_x = frame_info.ptBlinkR.x();
//        if (min_blink_r_y > frame_info.ptBlinkR.y())
//            min_blink_r_y = frame_info.ptBlinkR.y();
//
//        if (min_blink_l_x > frame_info.ptBlinkL.x())
//            min_blink_l_x = frame_info.ptBlinkL.x();
//        if (min_blink_l_y > frame_info.ptBlinkL.y())
//            min_blink_l_y = frame_info.ptBlinkL.y();
//
//        if (min_circularity_r > frame_info.circularityR)
//            min_circularity_r = frame_info.circularityR;
//        if (min_circularity_l > frame_info.circularityL)
//            min_circularity_l = frame_info.circularityL;
//
//        if (max_pupil_r_x < frame_info.ptPupilR.x())
//            max_pupil_r_x = frame_info.ptPupilR.x();
//        if (max_pupil_r_y < frame_info.ptPupilR.y())
//            max_pupil_r_y = frame_info.ptPupilR.y();
//
//        if (max_pupil_l_x < frame_info.ptPupilL.x())
//            max_pupil_l_x = frame_info.ptPupilL.x();
//        if (max_pupil_l_y < frame_info.ptPupilL.y())
//            max_pupil_l_y = frame_info.ptPupilL.y();
//
//        if (max_blink_r_x < frame_info.ptBlinkR.x())
//            max_blink_r_x = frame_info.ptBlinkR.x();
//        if (max_blink_r_y < frame_info.ptBlinkR.y())
//            max_blink_r_y = frame_info.ptBlinkR.y();
//
//        if (max_blink_l_x < frame_info.ptBlinkL.x())
//            max_blink_l_x = frame_info.ptBlinkL.x();
//        if (max_blink_l_y < frame_info.ptBlinkL.y())
//            max_blink_l_y = frame_info.ptBlinkL.y();
//
//        if (max_circularity_r < frame_info.circularityR)
//            max_circularity_r = frame_info.circularityR;
//        if (max_circularity_l < frame_info.circularityL)
//            max_circularity_l = frame_info.circularityL;
//    }
//
//    //
//    const static int MAX_PUPIL_COORD_RANGE = 5;         // 最大瞳孔坐标极差（毫米）
//    const static int MAX_BLINK_COORD_RANGE = 5;         // 最大映光点坐标极差
//    const static double MAX_CIRCULARITY_RANGE = 30;     // 最大圆度极差（*100）
//
//    const static QString str_pupil_coord_range = "range of pupil coord too large!";
//    const static QString str_blink_coord_range = "range of blink coord too large!";
//    const static QString str_circularity_range = "range of circularity too large!";
//
//    float pupil_range_r_x = ((float)max_pupil_r_x - min_pupil_r_x) * PIX_TO_PHY;
//    if (pupil_range_r_x > MAX_PUPIL_COORD_RANGE) {
//        _err_str = str_pupil_coord_range + QString::asprintf(" r_x range: %.2f", pupil_range_r_x);
//        return false;
//    }
//    float pupil_range_l_x = ((float)max_pupil_l_x - min_pupil_l_x) * PIX_TO_PHY;
//    if (pupil_range_l_x > MAX_PUPIL_COORD_RANGE) {
//        _err_str = str_pupil_coord_range + QString::asprintf(" l_x range: %.2f", pupil_range_l_x);
//        return false;
//    }
//    float pupil_range_r_y = ((float)max_pupil_r_y - min_pupil_r_y) * PIX_TO_PHY;
//    if (pupil_range_r_y > MAX_PUPIL_COORD_RANGE) {
//        _err_str = str_pupil_coord_range + QString::asprintf(" r_y range: %.2f", pupil_range_r_y);
//        return false;
//    }
//    float pupil_range_l_y = ((float)max_pupil_l_y - min_pupil_l_y) * PIX_TO_PHY;
//    if (pupil_range_l_y > MAX_PUPIL_COORD_RANGE) {
//        _err_str = str_pupil_coord_range + QString::asprintf(" l_y range: %.2f", pupil_range_l_y);
//        return false;
//    }
//
//    float blink_range_r_x = ((float)max_blink_r_x - min_blink_r_x) * PIX_TO_PHY;
//    if (blink_range_r_x > MAX_BLINK_COORD_RANGE) {
//        _err_str = str_blink_coord_range + QString::asprintf(" r_x range: %.2f", blink_range_r_x);
//        return false;
//    }
//    float blink_range_l_x = ((float)max_blink_l_x - min_blink_l_x) * PIX_TO_PHY;
//    if (blink_range_l_x > MAX_BLINK_COORD_RANGE) {
//        _err_str = str_blink_coord_range + QString::asprintf(" l_x range: %.2f", blink_range_l_x);
//        return false;
//    }
//    float blink_range_r_y = ((float)max_blink_r_y - min_blink_r_y) * PIX_TO_PHY;
//    if (blink_range_r_y > MAX_BLINK_COORD_RANGE) {
//        _err_str = str_blink_coord_range + QString::asprintf(" r_y range: %.2f", blink_range_r_y);
//        return false;
//    }
//    float blink_range_l_y = ((float)max_blink_l_y - min_blink_l_y) * PIX_TO_PHY;
//    if (blink_range_l_y > MAX_BLINK_COORD_RANGE) {
//        _err_str = str_blink_coord_range + QString::asprintf(" l_y range: %.2f", blink_range_l_y);
//        return false;
//    }
//
//    float circularity_r = ((float)max_circularity_r - min_circularity_r) * 100;
//    if (circularity_r > MAX_CIRCULARITY_RANGE) {
//        _err_str = str_circularity_range + QString::asprintf(" r range: %.2f", circularity_r);
//        return false;
//    }
//    float circularity_l = ((float)max_circularity_l - min_circularity_l) * 100;
//    if (circularity_l > MAX_CIRCULARITY_RANGE) {
//        _err_str = str_circularity_range + QString::asprintf(" l range: %.2f", circularity_l);
//        return false;
//    }
//
//    //
//    return true;
//}

//bool CMeasureCtrl::checkImgSetDist(stImgSetInfo &_img_set_info, QString &_err_str)
//{
//    int idx_begin = _img_set_info.idxFirst;
//    int idx_end = idx_begin + _img_set_info.imgCount - 1;
//
//    //
//    int dist_check = 0;
//    int dist_fit = 23;
//
//    stFrameInfo frame_info;
//    stDistInfo dist_info;
//    std::vector<int> list_tmp;
//    for (int i = idx_begin; i <= idx_end; i++) {
//        frame_info = (*m_listFrameInfo)[i];
//        if (frame_info.idxDist >= 0 && frame_info.idxDist < (int)m_listDistInfo->size()) {
//            dist_info = (*m_listDistInfo)[frame_info.idxDist];
//        } else {
//            logCritical(QString(__PRETTY_FUNCTION__) + ": idxDist out of bound!", CGlobal::LOG_MEASURE);
//            dist_check = 1;
//            break;
//        }
//        if (distStat_Fit != dist_info.distStat) {
//            dist_fit--;
//        }
//        list_tmp.push_back(dist_info.distVal);
//    }
//    float dist_dev = Util::calcStdDev(list_tmp);
//
//    const static int MAX_DIST_STD_DEV = 15;         // 最大距离数据标准差
//    const static int MIN_DIST_FIT = 22;             // 最小距离状态合适个数
//
//    if (dist_check) {
//        if (dist_fit < MIN_DIST_FIT) {
//            dist_check = 2;
//        }
//        if (dist_dev > MAX_DIST_STD_DEV) {
//            dist_check = 3;
//        }
//    }
//
//    if (0 != dist_check) {
//        _err_str = QString::asprintf("distVal check failed! stat = %d, dist_fit = %d, dist_dev = %.2f", dist_check, dist_fit, dist_dev);
//        return false;
//    }
//
//    return true;
//}

//bool CMeasureCtrl::checkImgSetDist_2(stImgSetInfo &_img_set_info, QString &_err_str)
//{
//    bool is_ok = true;
//
//    int idx_begin = _img_set_info.idxFirst;
//    int idx_end = idx_begin + _img_set_info.imgCount - 1;
//
//    stFrameInfo frame_info;
//    stDistInfo dist_info;
//    for (int i = idx_begin; i <= idx_end; i++) {
//        frame_info = (*m_listFrameInfo)[i];
//        if (frame_info.idxDist >= 0 && frame_info.idxDist < (int)m_listDistInfo->size()) {
//            dist_info = (*m_listDistInfo)[frame_info.idxDist];
//        } else {
//            _err_str = "idxDist out of bound";
//            logCritical(QString(__PRETTY_FUNCTION__) + ": " + _err_str, CGlobal::LOG_MEASURE);
//
//            is_ok = false;
//            break;
//        }
//
//        if ((dist_info.distVal < STD_DISTANCE - FIELD_DEPTH) || (dist_info.distVal > STD_DISTANCE + FIELD_DEPTH)) {
//            is_ok = false;
//            break;
//        }
//    }
//
//    //
//    return is_ok;
//}
