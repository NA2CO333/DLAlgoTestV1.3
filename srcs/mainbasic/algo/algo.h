#ifndef CALGO_202212_H
#define CALGO_202212_H

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <QRunnable>
#include <QMutex>
#include <QThreadPool>
#include <QString>
#include <atomic>
#include <array>
#include <bitset>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <opencv2/opencv.hpp>

#include "algointf.h"
#include "logger.h"
#include "algo_utils.h"
#include "perftimer.h"
#include "pupil_light_tracker.h"

class PupilPairOnnxDetector;
struct PupilPairResult;
#if defined(ENABLE_RKNN_C800) && ENABLE_RKNN_C800
class PupilPairRknnDetector;
#endif
struct FormalCrossRoundState;
struct FormalStreamingRoundState;
struct PupilCrossRoundTargetGradientCache;

void testSaveByteImageinFolder(unsigned char *pFrameBuffer, int index, QString _sub_dir_name, int mode=0);
void SaveByteFaultImageinFolder(unsigned char *pFrameBuffer, int index , QString _sub_dir_name, int mode , QString strPath);

// 各张转灯图的瞳孔高宽比，并提供计算均值的函数（用于判断上睑下垂、是否眨眼）
class pyrAspectRatio{
public:
    double R_AspectRatio[22];
    double L_AspectRatio[22];

    // 重置
    void clear(){
        memset(&R_AspectRatio, 0, 22 * sizeof (double));
        memset(&L_AspectRatio, 0, 22 * sizeof (double));
    }

    // 得到左眼 1~6 图平均瞳孔高宽比
    double getLeftAveraVal()
    {
        return calculateAverage(L_AspectRatio);
    }

    // 得到右眼 1~6 图平均瞳孔高宽比
    double getRightAveraVal()
    {
        return calculateAverage(R_AspectRatio);
    }

    // 是否眨眼了
    bool checkIsBlinked()
    {
        /* 判断逻辑：所有图像的上睑下垂状态一致，即都为是或都为否，则为非眨眼 */
        bool is_ptosis_new;

        bool is_ptosis_r = (R_AspectRatio[0] < PTOSIS_THRESH);
        for (int i = 1; i < 22; i++) {
            is_ptosis_new = (R_AspectRatio[i] < PTOSIS_THRESH);
            if (is_ptosis_new != is_ptosis_r) {
                logDebug((QString(__PRETTY_FUNCTION__) + ": R_AspectRatio[%1] = %2, different, evaluated as isBlinked!").arg(i + 1).arg(R_AspectRatio[i]), LOG_TAG);
                return true;
            }
        }

        bool is_ptosis_l = (L_AspectRatio[0] < PTOSIS_THRESH);
        for (int i = 1; i < 22; i++) {
            is_ptosis_new = (L_AspectRatio[i] < PTOSIS_THRESH);
            if (is_ptosis_new != is_ptosis_l) {
                logDebug((QString(__PRETTY_FUNCTION__) + ": L_AspectRatio[%1] = %2, different, evaluated as isBlinked!").arg(i + 1).arg(L_AspectRatio[i]), LOG_TAG);
                return true;
            }
        }

        return false;
    }

private:
    double calculateAverage(double ratio[]) {
        double avaraVal = 0;
        double sum = 0;
        double tmpMax = 0;
        double tmpMin = 1;
        for(int i=0;i<6;i++){
            sum += ratio[i];
            if(ratio[i] > tmpMax){
                tmpMax = ratio[i];
            }
            if(ratio[i] < tmpMin){
                tmpMin = ratio[i];
            }
//            qDebug()<<"L_a["<<i<<"]="<<L_AspectRatio[i];
        }
//        qDebug()<<"Left--tmpMax="<<tmpMax<<",tmpMin="<<tmpMin<<"sum="<<sum;
        avaraVal = (sum - tmpMax - tmpMin)/4;
        return avaraVal;
    }

};

// 斜视数据
struct strabismus{
    double leftEyeLR;       // 左眼水平斜视
    double leftEyeUD;       // 左眼垂直斜视
    double rightEyeLR;
    double rightEyeUD;

    void clear(){
        leftEyeLR = 0;
        leftEyeUD = 0;
        rightEyeLR = 0;
        rightEyeUD = 0;
    }
};

struct DsPairConfig {
    int idx1;
    int idx2;
    double sign;
};

// 算法实现类（algoVer_2022_12 版算法）
class CAlgo : public CAlgoIntf
{
    friend class CAlgoIntf;
public:
    // 设置【普通/专业】模式
    void setAlgoMode(enAlgoMode _algo_mode){algoMode = _algo_mode;}

    // 设置【是否存图】
    void setIsSaveImg(bool _is_save_img){isSaveImg = _is_save_img;}

    // 设置【最大固视偏差（°）】
    void setMaxGazeDeviation(int _max_gaze_dev){maxGazeDeviation = _max_gaze_dev;}

    // 设置【是否高度数模式】
    void setIsHmMode(bool _is_hm_mode){isHmMode = _is_hm_mode;}

    // 设置【是否但线程计算】
    void setIsSingleThread(bool _is_single_thread){isSingleThread = _is_single_thread;}

    // 设置【是否模拟眼】
    void setIsSimulatedEye(bool _is_simulated){isSimulatedEye = _is_simulated;}

    //设置模拟眼和人眼瞳孔的长宽比阈值，作为瞳孔的过滤条件
    void setWHRatio(float _modeleye_wh_ratio,float _humaneye_wh_ratio);

    /**
     * @brief 计算单张图像清晰度
     * @return 数值越大越清晰；-1.0表示输入或计算失败
     */
    double calcImageClarity(const cv::Mat &_image) const override;

    stAlgoCommandResult handleAlgoCommand(
            const stAlgoCommand &command) override;


    void insertPupilImg(int index,cv::Mat pupil,enWhichEye eye);

    // 检测瞳孔（算法4：二值化采用了自适应阈值，优化了结构）
    bool detectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l,
                              bool _is_calc_vision, enSingleDualEyeMode _single_dual_eye);

    // 计算图像的曝光量信息，返回是否计算成功
    bool calcExposure(unsigned char *_img_data, int _img_idx, stPupilInfo _pupil_info_r, stPupilInfo _pupil_info_l, int &_avg, bool &_over_expo,
                             enSingleDualEyeMode _single_dual_eye);

    enCalcResultState performVisionPreprocessing(
        const std::vector<unsigned char*>& _img_list,
        enAgeRange _age_range,
        std::string _sub_dir_name,
        enSingleDualEyeMode _single_dual_eye,
        double DSL[10],
        double DSR[10]);

    // 计算屈光度
    enCalcResultState calcVision(const std::vector<unsigned char *> &_img_list, enAgeRange _age_range, std::string _sub_dir_name, enSingleDualEyeMode _single_dual_eye,
                                             stVisionValue &_vision, stVisionAbnormal &_abnormal);

    enCalcResultState calculateRefraction(
        enAgeRange age_range,
        enSingleDualEyeMode _single_dual_eye,
        const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoRight,
        const std::bitset<FRAME_ARRAY_SIZE>& validRight,
        const std::array<stPupilInfo, FRAME_ARRAY_SIZE>& pupilInfoLeft,
        const std::bitset<FRAME_ARRAY_SIZE>& validLeft,
        const double DSR[10],
        const double DSL[10],
        stVisionValue& vision,
        stVisionAbnormal& abnormal,
        bool enable_debug_output);

    void saveIntermediateData(const double* DSR, const double* DSL,
                                    bool need_left, bool need_right,
                                    double sphL, double cylL, double axiL,
                                    double sphR, double cylR, double axiR);


    VisionCallback m_visionCb;   // 可替代原函数指针，更灵活

    void setVisionResultCallback(VisionCallback cb);
    bool getPupilCropResult(stPupilCropResult& result) override;

    bool calcVisionBegin(const enAgeRange _age_range, const std::string _sub_dir_name, const enSingleDualEyeMode _single_dual_eye,int roundNo);

    enCalcResultState appendImage(const cv::Mat &img, const int roundIdx, const int imgNo, const bool isInRange);

    /* 半径策略 */
    enum class RadiusMode { PairWise, Global };

protected:
    // 构造函数
    CAlgo();
    // 解析函数
    ~CAlgo();

    int imgWidth;
    int imgHeight;

    enAlgoMode algoMode = algoMode_Default;
    int maxGazeDeviation = 0;                       // 最大固视偏差（°）
    bool isSaveImg = false;
    bool isHmMode = false;                  // 是否高度数模式

    static bool pyrDetectMode;
    static int bSpotCoarseLocationCnt;

    QThreadPool *pool = nullptr;

    static int pupilState;        // 瞳孔识别状态。0-正常，1-瞳孔过小，2-无法识别

    bool isSingleThread = false;        // 是否单线程计算屈光度
    bool isSimulatedEye = false;        //是否模拟眼模式

    // 正式深度学习整轮任务使用独立单线程池，保证各轮按提交顺序串行执行，
    // 避免多轮模型推理与轻量跟踪同时占满RK3568 CPU。
    QThreadPool m_formalHybridPool;

    float modeleye_wh_ratio=0.60;
    float humaneye_wh_ratio=0.70;

    static void calcPupilAvgAndOverExpo(IplImage *_img_pupil, float &_avg, int &_count_over);
    static void calcPupilAvgAndOverExpoOptimized(const cv::Mat& img_pupil,
                                                  float& avg, int& count_over);

    enCalcResultState processAllImages(const std::vector<unsigned char*>& img_list,
                                             enAgeRange age_range,
                                             const std::string& sub_dir_name,
                                             enSingleDualEyeMode eye_mode,
                                             int start_index);

    bool processSingleImage(unsigned char* image_data,
                                  int image_index,
                                  enAgeRange age_range,
                                  enSingleDualEyeMode eye_mode,
                                  const std::string& sub_dir_name);

    void waitForSingleThreadCompletion();

    enCalcResultState waitForImageProcessing(bool need_left, bool need_right,
                                                   int expected_count, int timeout_ms, int max_timeout_count);



    enCalcResultState handleImageCountError(const std::vector<unsigned char*>& img_list,
                                                 const std::string& sub_dir_name);

    enCalcResultState handleProcessingTimeout(const std::vector<unsigned char*>& img_list,
                                                   const std::string& sub_dir_name,
                                                   enCalcResultState error_state);

    void saveFailureImages(const std::vector<unsigned char*>& img_list,
                                const std::string& sub_dir_name,
                                const std::string& reason);

    template<typename ImageArrayType>
    void savePupilROIImages(enSingleDualEyeMode _single_dual_eye,
                                  const ImageArrayType& pupilImgRight,
                                  const ImageArrayType& pupilImgLeft,
                                  const std::map<int, cv::Mat>& pupilROIImgRight,
                                  const std::map<int, cv::Mat>& pupilROIImgLeft);

    void checkPtosis(enSingleDualEyeMode _single_dual_eye);

    void fillVisionResults(stVisionValue& vision, stVisionAbnormal& abnormal,
                                 enSingleDualEyeMode _single_dual_eye,
                                 double sphL, double cylL, int axiL,
                                 double sphR, double cylR, int axiR,
                                 double pupilRadiusAvgRight,
                                 double pupilRadiusAvgLeft,
                                 CPointF pupilSpotAvgRight,
                                 CPointF pupilSpotAvgLeft,
                                 double pupil_distance);

    void checkVisionAbnormal(stVisionValue& vision, stVisionAbnormal& abnormal,
                             enSingleDualEyeMode _single_dual_eye,
                             double DL0, double DL60, double DL120,
                             double DR0, double DR60, double DR120,
                             const double DSL[10],const double DSR[10]);

    void checkAxisUntrusted(bool& axis_untrusted, double D0, double D60, double D120);

    void checkSphTooLarge(bool& sph_too_large, double& sph, double D0, double D60, const double DS[10]);

    void polishVisionResults(stVisionValue& vision, stVisionAbnormal& abnormal,
                                   enSingleDualEyeMode _single_dual_eye);


    static int getCurve();

private:
    // 保留原函数名以维持函数内部诊断日志文字，仅收敛为统一命令的私有实现。
    void setTurnLampSaveContext(const std::string &_patient_img_dir,
                                const std::string &_batch_dir,
                                const std::string &_source_dir,
                                const std::string &_usb_root);
    void resetMeasurementRuntime();
    void finishFormalRoundInput(const int roundIdx);
    bool isFormalAsyncPupilMode() const;

    std::atomic<int> m_runningTasks{0}; // 记录当前正在跑的子任务数量
    std::atomic<int> m_imgIndex{0};
#if ENABLE_ALGO_TIMING_LOG
    std::atomic<int> m_timingFormalFrameCount{0}; // 当前测量送入算法的正式转灯图数量
    std::atomic<int> m_timingRoundCount{0};       // 当前测量已经开始的转灯轮数
#endif
    enCalcResultState resultState;

    static const int COMPLETION_THRESHOLD = 12;
    static const int FAILROUNDS_THRESHOLD = 2;
    // 正式异步和传统流程统一使用20轮安全容量。
    // 正常情况下，取得2个稳定有效轮或3个有效轮后会提前结束。
    static const int MAX_ROUNDS = 20;

    // 正式拍摄照片任务的最大并行数。
    // C800及首张129 ROI确认完成后，最多并行处理3张缓存照片。
    static constexpr int FORMAL_MAX_CONCURRENT_PHOTO_TASKS = 3;

    //==========旧接口需要用到的成员变量===========================
    bool UEP[2];   // 分别为左眼和右眼是否上睑下垂，[0]:左，[1]:右
    pyrAspectRatio pyrAR;                 // 6张图像的瞳孔高宽比，并提供计算均值的函数

    //============新的多轮计算==================
    struct FrameSlot {
        // 深度学习混合路径先缓存完整一轮；处理开始后立即释放，避免多轮累计占用内存。
        cv::Mat frame;
        // 正式逐照片路径固定复用的400×160灰度小图，避免C800/匹配重复缩放。
        cv::Mat smallFrame;
        bool hasFrame = false;
        bool pupilDetected = false;
        bool processed = false;
        QString failureReason;
    };

    // 逐照片异步路径使用的只读锚点；发布后不允许后续照片任务修改。
    struct FormalPupilAnchor {
        cv::Mat image;
        cv::Mat smallImage;
        stPupilInfo right;
        stPupilInfo left;
        bool rightValid = false;
        bool leftValid = false;
        int lampNumber = -1;
        PupilLightFrame sourceFrame;
        // 不可变锚点梯度只准备一次；每张目标图任务使用自己的浅拷贝追加梯度。
        PupilLightTrackerCache trackerCache;
        bool trackerCacheReady = false;
    };

    // 正式路径按眼维护锚点生命周期，预测坐标不能直接作为跨轮源。
    enum FormalAnchorState {
        FormalAnchor_NoAnchor = 0,
        FormalAnchor_Confirmed
    };

    struct FormalEyeAnchorState {
        FormalAnchorState state = FormalAnchor_NoAnchor;
        stPupilInfo confirmedCoordinate;
        bool finalReliable = false;
        bool confirmedInCurrentRound = false;
        int consecutiveFailureCount = 0;
    };

    struct FormalAsyncRoundState {
        std::bitset<FRAME_ARRAY_SIZE> receivedFrames;
        std::bitset<FRAME_ARRAY_SIZE> scheduledFrames;
        std::bitset<FRAME_ARRAY_SIZE> processedFrames;
        bool inputFinished = false;
        bool anchorReady = false;
        // 覆盖C800、ROI、锚点缓存准备和最终发布提交的完整任务生命周期。
        bool anchorTaskRunning = false;
        bool settlementStarted = false;
        // 照片全部处理完后先标记ready；只有轮次顺序允许时才排入结算。
        bool settlementReady = false;
        bool settlementQueued = false;
        bool settlementCompleted = false;
        // 只统计本轮已经提交且尚未退出的照片任务。
        int photoTasksInFlight = 0;
        // 每个物理轮次独立记录C800调用代际，禁止用全局标志误判后续轮次。
        bool modelAttempted = false;
        // 仅在C800、ROI处理、锚点构造及发布全部完成后置位。
        bool modelFinished = false;
        // 每个物理轮最多执行一次C800；同一轮后续照片固定使用已确认
        // 的锚点进行匹配与129 ROI精修，C800跨轮由调度器串行化。
        // 本物理轮真实最早到达的有效照片槽位，0表示尚未收到照片。
        int firstReceivedImgNo = 0;
        // 本轮实际用于C800的照片槽位，0表示本轮未调用C800。
        int modelInputImgNo = 0;
        // 首次DS结算及跨轮候选提交/丢弃完成后置位；只在m_mutex下访问。
        bool settlementCompletedOnce = false;
        // 首张C800或同照片129 ROI失败后，当前轮只保存残轮，不再继续算法处理。
        bool earlyRetryRequested = false;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
        // 小图匹配诊断计数只用于测试副本日志，不参与结果判断。
        std::array<int, PupilLightMatchReason_Count> smallMatchRightCounts{};
        std::array<int, PupilLightMatchReason_Count> smallMatchLeftCounts{};
#endif
        int masterAnchorMatchSuccess = 0;
        int masterAnchorMatchFailed = 0;
        std::uint64_t measurementGeneration = 0;
        std::uint64_t roundGeneration = 0;
        std::shared_ptr<const FormalPupilAnchor> anchor;
        FormalEyeAnchorState rightAnchor;
        FormalEyeAnchorState leftAnchor;
        // 每只眼独立维护的LK参考；只允许最终129 ROI确认后的照片更新。
        PupilLightFlowReference flowRight;
        PupilLightFlowReference flowLeft;
        // 记录最后一张照片（或缺失槽位）被标记为已处理的时间。
        std::chrono::steady_clock::time_point lastProcessedAt;
        // 固定主模板使用日志只输出一次，避免每张照片刷屏。
        bool masterAnchorUseLogged = false;

        void clear()
        {
            receivedFrames.reset();
            scheduledFrames.reset();
            processedFrames.reset();
            inputFinished = false;
            anchorReady = false;
            anchorTaskRunning = false;
            settlementStarted = false;
            settlementReady = false;
            settlementQueued = false;
            settlementCompleted = false;
            photoTasksInFlight = 0;
            modelAttempted = false;
            modelFinished = false;
            firstReceivedImgNo = 0;
            modelInputImgNo = 0;
            settlementCompletedOnce = false;
            earlyRetryRequested = false;
#if ENABLE_DL_MERGE_TEST_DIAGNOSTICS
            smallMatchRightCounts.fill(0);
            smallMatchLeftCounts.fill(0);
#endif
            masterAnchorMatchSuccess = 0;
            masterAnchorMatchFailed = 0;
            measurementGeneration = 0;
            roundGeneration = 0;
            anchor.reset();
            rightAnchor = FormalEyeAnchorState{};
            leftAnchor = FormalEyeAnchorState{};
            flowRight.clear();
            flowLeft.clear();
            lastProcessedAt = std::chrono::steady_clock::time_point();
            masterAnchorUseLogged = false;
        }
    };

    struct RoundResult {
        enCalcResultState state = calcResultState_Succ;
        stVisionValue vision{};
        stVisionAbnormal abnormal{};
        bool valid = false;
    };

    struct DsItem {
        double value = 0.0;
        bool nativeValid = false;
        bool finalValid = false;
        bool rangeAffected = false;
        int patchedFromRound = -1;
    };

    struct RoundDs {
        std::array<DsItem, 10> right;
        std::array<DsItem, 10> left;
        bool generated = false;
        bool rejected = false;
        bool pending = false;
        bool submitted = false;
    };

    struct MeasurementRound {
        int roundIndex = -1;
        // 新增：标记本轮是否已经触发了完成逻辑
        bool isProcessing = false;
        bool hybridProcessingStarted = false;
        bool diagnosisLogged = false;
        bool rejected = false;
        std::atomic<int> pupilFailDetailLogCount {0};
        std::string savePatientDir;
        std::string saveBatchDir;
        std::string saveSourceDir;
        std::string saveUsbRoot;

        RoundResult result;

        // 正式逐照片异步流程的收图、调度、处理和结算状态。
        FormalAsyncRoundState asyncState;
        std::array<FrameSlot, FRAME_ARRAY_SIZE> frames;
        // === 核心优化：map → array，避免动态分配 ===
        std::array<cv::Mat, FRAME_ARRAY_SIZE> pupilImgRight;
        std::array<cv::Mat, FRAME_ARRAY_SIZE> pupilImgLeft;
        std::array<stPupilInfo, FRAME_ARRAY_SIZE> pupilInfoRight;
        std::array<stPupilInfo, FRAME_ARRAY_SIZE> pupilInfoLeft;
        std::array<cv::Mat, FRAME_ARRAY_SIZE> pupilROIImgLeft;
        std::array<cv::Mat, FRAME_ARRAY_SIZE> pupilROIImgRight;

        double pupilRadiusAvgRight = -1;
        double pupilRadiusAvgLeft = -1;
        CPointF pupilSpotAvgRight{0.0, 0.0};
        CPointF pupilSpotAvgLeft{0.0, 0.0};

        // 容量改为 23，支持索引 1~22
        std::bitset<FRAME_ARRAY_SIZE> validRight;
        std::bitset<FRAME_ARRAY_SIZE> validLeft;
        // 低置信定位已通过几何和ROI校验，因此允许先参与DS；后续仅对
        // 依赖这些照片的DS条目执行同轮一致性复核，避免模板分数一票否决。
        std::bitset<FRAME_ARRAY_SIZE> lowConfidenceRight;
        std::bitset<FRAME_ARRAY_SIZE> lowConfidenceLeft;
        // 距离超限只标记本轮风险，不再提前拒绝整轮或停止收图。
        std::bitset<FRAME_ARRAY_SIZE> outOfRangeFrames;
        int outOfRangeFrameCount = 0;
        // 同一照片双眼共享的完整传统兜底统计，按照片调用次数计数。
        int traditionalPairFallbackCalls = 0;
        double traditionalPairFallbackElapsedMs = 0.0;

        bool isFullyValid(enSingleDualEyeMode mode) const {
            if (mode == singleDualEyeMode_Right) {
                for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
                    if (!validRight.test(i)) return false;
                }
                return true;
            }
            if (mode == singleDualEyeMode_Left) {
                for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
                    if (!validLeft.test(i)) return false;
                }
                return true;
            }
            // 双眼模式
            for (int i = 1; i <= FRAMES_PER_ROUND; ++i) {
                if (!validRight.test(i) || !validLeft.test(i)) return false;
            }
            return true;
        }

        void setPupil(int imgNo, const cv::Mat& roi, const stPupilInfo& info, enWhichEye eye) {
            if (imgNo < 1 || imgNo > FRAMES_PER_ROUND) return;
            if (eye == whichEye_Right) {
                pupilImgRight[imgNo] = roi.clone();
                pupilInfoRight[imgNo] = info;
                validRight.set(imgNo);
            } else {
                pupilImgLeft[imgNo] = roi.clone();
                pupilInfoLeft[imgNo] = info;
                validLeft.set(imgNo);
            }
        }

        bool isPupilValid(int imgNo, enWhichEye eye) const {
            if (imgNo < 1 || imgNo > FRAMES_PER_ROUND) return false;
            return (eye == whichEye_Right) ? validRight.test(imgNo) : validLeft.test(imgNo);
        }

        const cv::Mat& getPupil(int imgNo, enWhichEye eye) const {
            if (imgNo < 1 || imgNo > FRAMES_PER_ROUND) {
                static cv::Mat empty;
                return empty;
            }
            return (eye == whichEye_Right) ? pupilImgRight[imgNo] : pupilImgLeft[imgNo];
        }

        void clear() {
            for (int i = 1; i < FRAME_ARRAY_SIZE; ++i) {
                frames[i] = FrameSlot{};
                if (!pupilImgRight[i].empty()) {
                    pupilImgRight[i].release();
                }
                if (!pupilImgLeft[i].empty()) {
                    pupilImgLeft[i].release();
                }
                if (!pupilROIImgRight[i].empty()) {
                    pupilROIImgRight[i].release();
                }
                if (!pupilROIImgLeft[i].empty()) {
                    pupilROIImgLeft[i].release();
                }
            }
            validRight.reset();
            validLeft.reset();
            lowConfidenceRight.reset();
            lowConfidenceLeft.reset();
            traditionalPairFallbackCalls = 0;
            traditionalPairFallbackElapsedMs = 0.0;
            isProcessing = false;
            hybridProcessingStarted = false;
            diagnosisLogged = false;
            rejected = false;
            outOfRangeFrames.reset();
            outOfRangeFrameCount = 0;
            asyncState.clear();
            pupilFailDetailLogCount.store(0, std::memory_order_relaxed);
            savePatientDir.clear();
            saveBatchDir.clear();
            saveSourceDir.clear();
            saveUsbRoot.clear();
        }
    };

    // === 成员变量 ===
    mutable std::mutex m_mutex; // 保护所有成员状态
    struct PupilCropCache
    {
        bool captured = false;
        bool resultReady = false;
        std::uint64_t measurementGeneration = 0;
        int roundIdx = -1;
        int imgNo = 0;
        cv::Mat sourceImage;
        bool rightValid = false;
        bool leftValid = false;
        cv::Point2f rightCenter;
        cv::Point2f leftCenter;

        void clear()
        {
            captured = false;
            resultReady = false;
            measurementGeneration = 0;
            roundIdx = -1;
            imgNo = 0;
            sourceImage.release();
            rightValid = false;
            leftValid = false;
            rightCenter = cv::Point2f();
            leftCenter = cv::Point2f();
        }
    };
    mutable std::mutex m_pupilCropCacheMutex; // 只保护结果页裁图缓存
    PupilCropCache m_pupilCropCache;
    mutable std::mutex m_settlementMutex; // 串行化整轮结算，避免长时间占用 m_mutex
    enAgeRange m_age = ageRange_4_20_100_YEAE;
    enSingleDualEyeMode m_eye = singleDualEyeMode_Right;
    std::string m_subDir;
    int maxRounds = 5;
    std::array<MeasurementRound, MAX_ROUNDS> m_rounds;
    std::array<RoundDs, MAX_ROUNDS> m_roundDs;
    std::vector<int> m_pendingRoundIndices;
    std::atomic<int> failRounds{0};//失败轮计数
    std::atomic<bool> m_formalModelAttempted{false};
    std::atomic<bool> m_formalModelFinished{false};
    // 最多同时保留两个已激活物理轮；同轮照片任务仍可并发。
    std::deque<int> m_activeFormalRounds;
    int m_formalInterleaveCursor = 0;
    int m_formalPhotoTasksInFlight = 0;
    int m_activeFormalProcessingRound {-1};
    bool m_formalSettlementTaskRunning = false;
    std::atomic<std::uint64_t> m_formalMeasurementGeneration{0};
    std::atomic<std::uint64_t> m_formalRoundGenerationCounter{0};
    std::atomic<std::uint64_t> m_formalCrossRoundSourceGeneration{0};
    std::atomic<int> m_formalAsyncTasksInFlight{0};
    std::mutex m_formalAsyncWaitMutex;
    std::condition_variable m_formalAsyncWaitCondition;

    struct FormalC800Request {
        int roundIdx = -1;
        int imgNo = 0;
        cv::Mat image;
        cv::Mat smallImage;
        std::uint64_t measurementGeneration = 0;
        std::uint64_t roundGeneration = 0;
        std::string reason;
    };
    // C800本身保持单任务串行；请求在这里排队，不让照片工作线程阻塞等待。
    std::deque<FormalC800Request> m_pendingFormalC800Starts;
    bool m_formalC800TaskRunning = false;
    int m_formalC800RunningRound = -1;

    // C800双帧模型统一用于预览兜底和正式首张锚定。
    // 正式入口把当前400×160真实图与同尺寸黑图拼接，只采用真实图半区输出。
    std::unique_ptr<PupilPairOnnxDetector> m_pupilPairOnnxDetector;
#if defined(ENABLE_RKNN_C800) && ENABLE_RKNN_C800
    // RK3568优先使用NPU；运行时异常后关闭本进程内NPU后端并自动回退ONNX CPU。
    std::unique_ptr<PupilPairRknnDetector> m_pupilPairRknnDetector;
    std::atomic<bool> m_pupilPairRknnAvailable{false};
#endif
    // 首轮22张轨迹和梯度模板缓存，仅由正式混合单线程池访问。
    std::unique_ptr<FormalCrossRoundState> m_formalCrossRoundState;
    // 整次测量只使用一个不可变主模板；仅由m_mutex保护读写。
    std::shared_ptr<const FormalPupilAnchor> m_formalMasterAnchor;
    bool m_formalMasterAnchorReady = false;
    std::uint64_t m_formalMasterAnchorGeneration = 0;
    std::uint64_t m_formalMasterAnchorMeasurementGeneration = 0;
    int m_formalMasterAnchorRound = -1;
    int m_formalMasterAnchorImgNo = 0;
    mutable std::mutex m_pupilModelMutex;
    // 第1轮主识别图片到齐后，流式任务用它等待后续拍摄图；
    // appendImage只负责通知，不在相机线程中执行模型或梯度计算。
    std::condition_variable m_formalFrameCondition;
    std::atomic<bool> m_formalStreamingStop{false};
    bool m_pupilModelLoadAttempted = false;
    // 仅在正式混合轮已经排队或正在运行时禁止预览模型抢占OpenCV CPU资源。
    // 不能按“整次测量”屏蔽，否则中途退回准备界面重新对准时模型无法兜底。
    std::atomic<int> m_formalHybridTasksInFlight{0};
    std::atomic<bool> m_previewModelSuppressionLogged{false};
    std::chrono::steady_clock::time_point m_lastPreviewModelAttempt {};

    static constexpr int PUPIL_OUTPUT_CROP_WIDTH = 140;
    static constexpr int PUPIL_OUTPUT_CROP_HEIGHT = 225;
    // 先围绕精修瞳孔中心截取较小区域，再放大到固定输出尺寸，
    // 在不改变嵌入式接口尺寸的前提下让瞳孔约放大两倍。
    static constexpr int PUPIL_OUTPUT_SOURCE_WIDTH = 70;
    static constexpr int PUPIL_OUTPUT_SOURCE_HEIGHT = 112;

    // --- 控制标志 ---
    std::atomic<int> m_hasEmittedFinalResult{false};      // 是否已回调最终结果（防重复）
    std::string m_turnLampSavePatientDir;                 // 转灯图患者目录，用于写 round_xx/algo_status.txt
    std::string m_turnLampSaveBatchDir;                   // 转灯图测量批次目录
    std::string m_turnLampSaveSourceDir;                  // 转灯图来源目录
    std::string m_turnLampSaveUsbRoot;                    // 本次测量固定的 U 盘根目录快照

    void onRoundCompleted(int roundIdx);
    void tryFinalizeRoundLocked(int roundIdx);
    bool processRoundSettlementLocked(int roundIdx);
    bool buildRoundDsLocked(int roundIdx);
    bool tryPatchRoundDsLocked(int targetRoundIdx);
    bool calculateRoundResultFromDsLocked(int roundIdx);
    void emitCurrentResultsLocked(bool forceFinished = false);
    // 统一发布正式路径的最终PupilNotFound，回调必须发生在解锁后。
    bool emitFormalPupilNotFoundOnce(
            int roundIdx, const QString& reason);
    void writeRoundAlgoStatusLocked(int roundIdx,
                                    const QString &algoState,
                                    const QString &reason,
                                    bool isFinalResultRound = false,
                                    bool policyFinished = false,
                                    bool policyQuestionable = true) const;
    void clearDsPatchState();
    void calculateDsPairsForEyeLocked(const std::array<cv::Mat, FRAME_ARRAY_SIZE>& roiImages,
                                      const std::bitset<FRAME_ARRAY_SIZE>& validFrames,
                                      const std::array<DsPairConfig, 9>& configs,
                                      bool isRightEye,
                                      std::array<DsItem, 10>& items) const;
    // 仅复核由低置信照片参与的DS条目；常规高置信DS不改变既有计算口径。
    void applyLowConfidenceDsQualityGateLocked(
            int roundIdx,
            bool isRightEye,
            const std::bitset<FRAME_ARRAY_SIZE>& lowConfidenceFrames,
            std::array<DsItem, 10>& items) const;
    void applyRiskyDsQualityGateLocked(
            int roundIdx,
            bool isRightEye,
            const std::bitset<FRAME_ARRAY_SIZE>& lowConfidenceFrames,
            const std::bitset<FRAME_ARRAY_SIZE>& outOfRangeFrames,
            std::array<DsItem, 10>& items) const;
    int missingDsCount(const std::array<DsItem, 10>& items) const;
    QString missingDsText(const std::array<DsItem, 10>& items) const;
    bool hasFuturePatchCandidate(int targetRoundIdx) const;
    bool patchDsItemsFromNearestRound(std::array<DsItem, 10>& targetItems,
                                      int targetRoundIdx,
                                      bool rightEye);

    void resetRoundState(int roundNo);

    bool ensurePupilModelLoaded();
    bool pupilPairModelAvailable() const;
    bool inferPupilPairModel(const cv::Mat& firstImage,
                             const cv::Mat& secondImage,
                             PupilPairResult* result,
                             float logitThreshold,
                             int minimumComponentArea,
                             std::string* errorMessage);
    bool detectPupilByModelForPreview(const cv::Mat& img,
                                      enSingleDualEyeMode eyeMode,
                                      stPupilInfo& pupilRight,
                                      stPupilInfo& pupilLeft);
    bool shouldUseFormalHybrid() const;
    void processFormalHybridRound(
            int roundIdx,
            const std::shared_ptr<FormalStreamingRoundState>& streamingState =
                    std::shared_ptr<FormalStreamingRoundState>());
    void processFormalStreamingRound(int roundIdx);
    bool processFormalCrossRound(int roundIdx,
                                 const std::vector<cv::Mat>& images,
                                 const PupilCrossRoundTargetGradientCache*
                                         targetGradientCache = nullptr);
    void clearFormalCrossRoundState();
    void clearFormalMasterAnchorLocked(const char* reason = nullptr);
    // 只从MeasurementRound中已经写入的最终精修坐标构建跨轮轨迹。
    bool buildRefinedCrossRoundFramesLocked(
            int roundIdx,
            std::vector<PupilLightFrame>& refinedFrames) const;
    // 在锁内判断已提交跨轮源是否覆盖当前测量所需的全部可靠眼。
    bool hasCompleteFormalCrossRoundSourceLocked() const;
    // 轮次结算前只暂存候选，结算后再决定提交或丢弃。
    void stageCrossRoundSourceCandidate(
            int roundIdx,
            const std::vector<cv::Mat>& images);
    void commitOrDiscardCrossRoundSourceCandidates();
    // 正式异步深度/跨轮坐标统一先做当前图片小 ROI 精修，失败保持缺失；
    // 旧兼容路径是否允许传统兜底由调用方显式传参决定。
    bool refineFormalPupilPrediction(const cv::Mat& image,
                                     int imageNumber,
                                     enWhichEye whichEye,
                                     const stPupilInfo& predicted,
                                     stPupilInfo& refined,
                                     PupilRoiRefineResult& diagnostic) const;
    cv::Mat cropPupilOutputImage(const cv::Mat& source,
                                 const cv::Point2f& center) const;
    void cacheFirstFramePupilCropSource(
            int roundIdx,
            int imgNo,
            const cv::Mat& source,
            bool rightValid,
            const stPupilInfo& rightPupil,
            bool leftValid,
            const stPupilInfo& leftPupil,
            std::uint64_t measurementGeneration,
            std::uint64_t roundGeneration);
    void markPupilCropResultReady();
    static const char* formalAnchorStateName(FormalAnchorState state);
    void setFormalAnchorConfirmedLocked(int roundIdx,
                                        bool rightEye,
                                        const stPupilInfo& confirmed,
                                        const char* reason);

    // 新正式入口：每张照片只接收、调度和提交一次，旧整轮阶段机不参与。
    enCalcResultState appendFormalAsyncImage(const cv::Mat& image,
                                              int roundIdx,
                                              int imgNo,
                                              bool isInRange);
    void schedulePendingFormalFrames(int roundIdx,
                                     int maxPhotoTasksForRound =
                                             FORMAL_MAX_CONCURRENT_PHOTO_TASKS,
                                     int* scheduledPhotoTasks = nullptr);
    void activateWaitingFormalRounds();
    void pumpFormalInterleavedFrames();
    void completeFormalPhotoTask(int roundIdx,
                                 std::uint64_t measurementGeneration,
                                 std::uint64_t roundGeneration);
    void scheduleNextReadyFormalSettlement();
    void processOneFormalAsyncFrame(
            int roundIdx,
            int imgNo,
            const cv::Mat& image,
            const cv::Mat& smallImage,
            const std::shared_ptr<const FormalPupilAnchor>& anchor,
            std::uint64_t measurementGeneration,
            std::uint64_t roundGeneration);
    void updateFormalFlowReferences(
            int roundIdx,
            int imgNo,
            const cv::Mat& image,
            bool rightValid,
            const stPupilInfo& pupilRight,
            bool leftValid,
            const stPupilInfo& pupilLeft,
            std::uint64_t measurementGeneration,
            std::uint64_t roundGeneration);
    void publishFormalAnchorFromRound(int roundIdx,
                                      int imgNo,
                                      const cv::Mat& image,
                                      std::uint64_t measurementGeneration,
                                      std::uint64_t roundGeneration);
    void enqueueFormalAsyncSettlement(int roundIdx,
                                      std::uint64_t measurementGeneration,
                                      std::uint64_t roundGeneration);
    void finalizeFormalAsyncRound(int roundIdx);
    void startFormalC800Task(int roundIdx,
                             int imgNo,
                             const cv::Mat& image,
                             const cv::Mat& smallImage,
                             std::uint64_t measurementGeneration,
                             std::uint64_t roundGeneration,
                             const std::string& reason);
    void requestFormalC800Task(int roundIdx,
                               int imgNo,
                               const cv::Mat& image,
                               const cv::Mat& smallImage,
                               std::uint64_t measurementGeneration,
                               std::uint64_t roundGeneration,
                               const std::string& reason);
    void finishFormalC800Task();
    bool runFormalC800Once(const cv::Mat& image,
                           PupilLightFrame* modelFrame,
                           std::string* errorMessage);
    bool runFormalC800Pair(const cv::Mat& firstImage,
                           const cv::Mat& secondImage,
                           PupilLightFrame* firstFrame,
                           PupilLightFrame* secondFrame,
                           std::string* errorMessage);
    void processAndStoreLocatedFrame(int roundIdx,
                                    int imgNo,
                                    const cv::Mat& image,
                                    bool rightLocated,
                                    stPupilInfo pupilRight,
                                    bool leftLocated,
                                    stPupilInfo pupilLeft,
                                    std::uint64_t measurementGeneration = 0,
                                    std::uint64_t roundGeneration = 0,
                                    bool allowTraditionalFallback = true,
                                    bool onlyProcessMissingEyes = false,
                                    bool predictionsAlreadyRefined = false);

    void logRoundDiagnosisLocked(int roundIdx, const QString &trigger);

    void saveDSArr(const std::string &subDirName,
                          const double *DSR,
                          const double *DSL,
                          bool needLeft,
                          bool needRight) const;

};



#endif // CALGO_202212_H
