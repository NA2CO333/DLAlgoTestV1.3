#include "algo-invoker.h"

#include <functional>

#include <QTime>
#include <QDir>
#include <QCoreApplication>

#include "util-common.h"
#include "global.h"

#ifndef UNIT_TEST
# include "camerainit.h"
#endif

#include "mainwindow.h"
#include "windowsmanager.h"

//
enSingleDualEyeMode g_SingleDualEye = singleDualEyeMode_Both;     // 单双眼模式      // NOTE: 【单双眼模式】不永久保存

CvPoint3D32f g_SaturationCenterL;   // 左眼，最后一次瞳孔识别时得到的瞳孔中心坐标和瞳孔半径
CvPoint3D32f g_SaturationCenterR;   // 右眼，最后一次瞳孔识别时得到的瞳孔中心坐标和瞳孔半径

int g_lastDetectTime = 0;

int g_PupilState = 0;        // 瞳孔识别状态。0-正常，1-瞳孔过小，2-无法识别

/* 待检查：

 g_opticalPathType -> WinMeasure::setRunStat() ?


*/

//
const char *enumToText_AlgoErrType(enAlgoErrType _err_type)
{
    switch (_err_type) {
    case algoErrType_DetectPupil    : return "DetectPupil_Error";
    case algoErrType_CalcVision     : return "CalcVision_Error";
    }
    return "???Error";
}

QString enumToText_CalcResultState(enCalcResultState _result_stat)
{
    switch (_result_stat) {
    case calcResultState_Fail                   : return QCoreApplication::translate("algo-invoker.cpp", "未知失败"          );       // "UnknownFailure"
    case calcResultState_Succ                   : return QCoreApplication::translate("algo-invoker.cpp", "成功"              );       // "Succ"
    case calcResultState_GazeOver               : return QCoreApplication::translate("algo-invoker.cpp", "眼位超范围"        );       // "GazeOver"
    case calcResultState_Blinked                : return QCoreApplication::translate("algo-invoker.cpp", "眨眼"              );       // "Blinked"
    case calcResultState_PupilNotFound          : return QCoreApplication::translate("algo-invoker.cpp", "未找到瞳孔"        );       // "PupilNotFound"
    case calcResultState_VisionAbnormal         : return QCoreApplication::translate("algo-invoker.cpp", "视力值异常"        );       // "VisionAbnormal"
    case calcResultState_ParamInvalid           : return QCoreApplication::translate("algo-invoker.cpp", "参数非法"          );       // "ParamInvalid"
    case calcResultState_Timeout                : return QCoreApplication::translate("algo-invoker.cpp", "超时"              );       // "Timeout"
    case calcResultState_ImageError             : return QCoreApplication::translate("algo-invoker.cpp", "图像错误"          );       // "ImageError"
    case calcResultState_ProgramException       : return QCoreApplication::translate("algo-invoker.cpp", "程序异常"          );       // "ProgramException"
    case calcResultState_OutOfRange             : return QCoreApplication::translate("algo-invoker.cpp", "瞳孔坐标超范围"    );       // "OutOfRange"
    case calcResultState_ResultCallbackTimeout  : return QCoreApplication::translate("algo-invoker.cpp", "结果计算超时"      );       // "ResultCallbackTimeout"
    case calcResultState_Aborted                : return QCoreApplication::translate("algo-invoker.cpp", "被终止"            );       // "Aborted"
    case calcResultState_RetryNextRound         : return QCoreApplication::translate("algo-invoker.cpp", "重试下一轮"        );       // "RetryNextRound"
    }
    return "??";
}

const char *enumToName_CalcResultState(enCalcResultState _result_stat)
{
    switch (_result_stat) {
    case calcResultState_Fail                   : return "UnknownFailure";
    case calcResultState_Succ                   : return "Succ";
    case calcResultState_GazeOver               : return "GazeOver";
    case calcResultState_Blinked                : return "Blinked";
    case calcResultState_PupilNotFound          : return "PupilNotFound";
    case calcResultState_VisionAbnormal         : return "VisionAbnormal";
    case calcResultState_ParamInvalid           : return "ParamInvalid";
    case calcResultState_Timeout                : return "Timeout";
    case calcResultState_ImageError             : return "ImageError";
    case calcResultState_ProgramException       : return "ProgramException";
    case calcResultState_OutOfRange             : return "OutOfRange";
    case calcResultState_ResultCallbackTimeout  : return "ResultCallbackTimeout";
    case calcResultState_Aborted                : return "Aborted";
    case calcResultState_RetryNextRound         : return "RetryNextRound";
    }
    return "??";
}

const char *enumToText_DiopterCalcStat(enDiopterCalcStat _stat)
{
    switch (_stat) {
    case enDiopterCalcStat::NotBegin        : return "NotBegin";
    case enDiopterCalcStat::HasBegin        : return "HasBegin";
    case enDiopterCalcStat::HasCallback     : return "HasCallback";
    case enDiopterCalcStat::HasEnd          : return "HasEnd";
    }
    return "??";
}

// 静态变量
enPupilDetectionResult CAlgoInvoker::pupilDetectionResult = PupilDetectionResult_Unknow;
bool CAlgoInvoker::isCalculatingVision = false;
int CAlgoInvoker::curve = 2;

int CAlgoInvoker::timesCnt = 0;
int CAlgoInvoker::visionValueSource = false;

enAlgoVerAll CAlgoInvoker::currentPupilAlgoVer = algoVerAll_2019;       // NOTE: 已废弃

int CAlgoInvoker::bSpotCoarseLocationCnt = 0;       // TODO: 这是啥？

//
CAlgoInvoker::CAlgoInvoker(QObject *_parent) : QObject(_parent)
{
    //
    m_algoIntf = CAlgoIntf::createInstance(cameraType_D3T_M3ST130M);

    qRegisterMetaType<stAlgoCommand>("stAlgoCommand");

    //
    QObject::connect(this, &CAlgoInvoker::sigCalcVisionBegin, this, &CAlgoInvoker::slot_this_CalcVisionBegin, Qt::QueuedConnection);
    QObject::connect(this, &CAlgoInvoker::sigExecuteAlgoCommand,
                     this, &CAlgoInvoker::slot_this_ExecuteAlgoCommand,
                     Qt::QueuedConnection);
    QObject::connect(this, &CAlgoInvoker::sigAppendImage, this, &CAlgoInvoker::slot_this_AppendImage, Qt::QueuedConnection);

    //
    m_timerCalcVisionCallback = new QTimer();
    m_timerCalcVisionCallback->setSingleShot(true);
    QObject::connect(m_timerCalcVisionCallback, &QTimer::timeout, this, &CAlgoInvoker::slot_timerCalcVisionCallback_timeout, Qt::QueuedConnection);

}

void CAlgoInvoker::init()
{
    //
    m_algoIntf->setAlgoMode(CGlobal::algoMode);
    m_algoIntf->setMaxGazeDeviation(CGlobal::maxGazeDeviation);
    m_algoIntf->setIsSaveImg(saveImage);
    m_algoIntf->setIsHmMode(g_isHmMode);
    m_algoIntf->setIsSingleThread(CGlobal::isSingleThreadCalc);
    m_algoIntf->setIsSimulatedEye(CGlobal::isSimulatedEye);
    m_algoIntf->setWHRatio(CGlobal::eyeWhRatio_Model, CGlobal::eyeWhRatio_Human);

    auto callback_result = [this] (enCalcResultState _result_stat, int _round_idx,
            const stVisionValue &_vision, const stVisionAbnormal &_abnormal,
            const std::vector<stVisionValue> &_result_set, bool _finished, bool _questionable)
    {
        this->doOnGetCalcVisionResult(_result_stat, _round_idx, _vision, _abnormal,
                                      _result_set, _finished, _questionable);
    };
    m_algoIntf->setVisionResultCallback(callback_result);

    //
    reset();
}

void CAlgoInvoker::reset()
{
    if (m_algoIntf) {
        // 返回预览或取消测量时同步解除正式测量运行态。
        executeAlgoCommand(
                stAlgoCommand::makeCancelMeasurementRuntime());
    }
    m_diopterCalcStat = enDiopterCalcStat::NotBegin;
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_diopterCalcStat changed: " << enumToText_DiopterCalcStat(m_diopterCalcStat);
}

stAlgoCommandResult CAlgoInvoker::executeAlgoCommand(
        const stAlgoCommand &command)
{
    if (m_algoIntf == nullptr) {
        return stAlgoCommandResult::makeFailure(
                "algorithm interface is null");
    }

    switch (command.type) {
    case enAlgoCommandType::SetTurnLampSaveContext:
    case enAlgoCommandType::FinishFormalRoundInput:
        // 设置存图上下文和本轮结束必须排在appendImage之后。
        emit sigExecuteAlgoCommand(command, QPrivateSignal());
        return stAlgoCommandResult::makeSuccess();

    case enAlgoCommandType::CancelMeasurementRuntime:
    case enAlgoCommandType::QueryFormalAsyncPupilMode:
        // 取消必须立即生效，模式查询必须同步返回。
        return m_algoIntf->handleAlgoCommand(command);
    }

    return stAlgoCommandResult::makeFailure(
            "unsupported algorithm command");
}

QString CAlgoInvoker::diopterToVision(QString _sph_str, QString _cyl_str, enVisionNotation _vision_type)
{
    double sph = _sph_str.toDouble();
    double cyl = _cyl_str.toDouble();
    return diopterToVision(sph, cyl, _vision_type);
}

QString CAlgoInvoker::diopterToVision(double _sph, double _cyl, enVisionNotation _vision_type)
{
    /* 2025-10-28 变更（视力与屈光对照关系 V1.0.1 20251028_崔继友.docx）：
     * 若 |DC| < +0.75D，采用SE进行视力转换；
     * 若 |DC| ≥ 0.75D，比较 |DS| 与 |DC/2| 大小，哪个大就用哪个当作|SE|做视力对照。
     * 上述 "|xx|" 是指绝对值，而没有标注 "||" 的则是含符号的数值。
     */
    /* 2026-04-25 修正了 20251028 版的错误：
     * 1、值为0.75的条件变量应该用 DC 的绝对值而不是 DS；。
     * 2、DS 与 |DC/2| 取较大者改为 |DS| 与 |DC/2| 取较大者。
     */

    //
    QString vision = "";

    //
    //if (se > 0) {
    //    vision = (language ? "远视 " : "Hyperopia");
    //} else if (se < 0) {
    //    vision = (language ? "近视 " : "Myopia");
    //}

    // 屈光值取整
    const double VISION_PREC = (g_MinResolution ? 0.01 : 0.25);

    //_sph = Util::roundDouble(_sph, VISION_PREC);
    //_cyl = Util::roundDouble(_cyl, VISION_PREC);

    //
    double param = 0;
    if (Util::compDouble(qAbs(_cyl), 0.75) < 0) {
        double se = getSE(_sph, _cyl);
        se = Util::roundDouble(se, VISION_PREC);
        param = qAbs(se);
    } else {
        param = (qMax(qAbs(_sph), qAbs(_cyl / 2)));
        //param = Util::roundDouble(param, VISION_PREC);
    }

    //
    if (Util::compDouble(param, 6.75) >= 0) { vision = (visionNotation_FivePoint == _vision_type ? "<4.0" : (visionNotation_Decimal == _vision_type ? "<0.1" : "")); } else
    if (Util::compDouble(param, 6.25) >= 0 && Util::compDouble(param, 6.75) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.0" : (visionNotation_Decimal == _vision_type ? "0.10" : "")); } else
    if (Util::compDouble(param, 5.25) >= 0 && Util::compDouble(param, 6.25) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.1" : (visionNotation_Decimal == _vision_type ? "0.12" : "")); } else
    if (Util::compDouble(param, 4.75) >= 0 && Util::compDouble(param, 5.25) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.2" : (visionNotation_Decimal == _vision_type ? "0.15" : "")); } else
    if (Util::compDouble(param, 4.25) >= 0 && Util::compDouble(param, 4.75) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.3" : (visionNotation_Decimal == _vision_type ? "0.20" : "")); } else
    if (Util::compDouble(param, 3.75) >= 0 && Util::compDouble(param, 4.25) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.4" : (visionNotation_Decimal == _vision_type ? "0.25" : "")); } else
    if (Util::compDouble(param, 3.25) >= 0 && Util::compDouble(param, 3.75) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.5" : (visionNotation_Decimal == _vision_type ? "0.30" : "")); } else
    if (Util::compDouble(param, 2.75) >= 0 && Util::compDouble(param, 3.25) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.6" : (visionNotation_Decimal == _vision_type ? "0.40" : "")); } else
    if (Util::compDouble(param, 2.25) >= 0 && Util::compDouble(param, 2.75) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.7" : (visionNotation_Decimal == _vision_type ? "0.50" : "")); } else
    if (Util::compDouble(param, 1.75) >= 0 && Util::compDouble(param, 2.25) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.8" : (visionNotation_Decimal == _vision_type ? "0.60" : "")); } else
    if (Util::compDouble(param, 1.25) >= 0 && Util::compDouble(param, 1.75) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "4.9" : (visionNotation_Decimal == _vision_type ? "0.80" : "")); } else
    if (Util::compDouble(param, 0.75) >= 0 && Util::compDouble(param, 1.25) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "5.0" : (visionNotation_Decimal == _vision_type ? "1.00" : "")); } else
    if (Util::compDouble(param, 0.25) >= 0 && Util::compDouble(param, 0.75) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "5.1" : (visionNotation_Decimal == _vision_type ? "1.20" : "")); } else
    if (Util::compDouble(param, 0.00) >= 0 && Util::compDouble(param, 0.25) < 0) { vision = (visionNotation_FivePoint == _vision_type ? "5.2" : (visionNotation_Decimal == _vision_type ? "1.50" : "")); }

    //
    return vision;
}

float CAlgoInvoker::getSE(float _sph, float _cyl)
{
    return _sph + _cyl / 2;
}

// 获取推值（流程见《20220328_“前两年龄段按月龄估值”功能程序设计.docx》）
void CAlgoInvoker::getStatisticalValues(int _age_range, QDate _birth_date, stVisionValue &_vision, stVisionAbnormal &_vision_abnormal)
{
    /* 处理逻辑：
     * 如果不需要按月龄估值，则取统计值，否则取月龄估值
     */

    //
    _vision = {};

    // 获取统计值
    _vision = getRandomStatisticalVision(_age_range);

    // 如果需要使用月龄屈光值，则替换
    bool is_need_statistical = !CGlobal::getIsNeedMonthAgeVision(_age_range);
    if (!is_need_statistical) {
        // 检查并设置瞳距
        if (Util::compDouble(g_SaturationCenterL.x, 0) > 0 && Util::compDouble(g_SaturationCenterR.x, 0) > 0) {
            _vision.PD = fabs(g_SaturationCenterL.x - g_SaturationCenterR.x) * PIX_TO_PHY;
        }

        // 获得生日
        QDate *birth_date_ptr = (_birth_date.isValid() ? &_birth_date : Q_NULLPTR);

        // 获得月龄屈光值
        setMonthAgeVisionByBirthday(_vision, _age_range, birth_date_ptr);
    }

    //
    _vision_abnormal = {};
}

bool CAlgoInvoker::getIsCalculatingVision()
{
    return isCalculatingVision;
}

void CAlgoInvoker::setIsCalculatingVision(bool _val)
{
    isCalculatingVision = _val;
}

int CAlgoInvoker::getCurve()
{
    return curve;
}

void CAlgoInvoker::setCurve(int _curve)
{
    curve = _curve;
}

//void CAlgoInvoker::scope_limitation(CPatient &_patient)
//{
//    _patient.patientleftse      = CAlgoInvoker::resultReceieve(_patient.patientleftse);
//    _patient.patientlefteyesph  = CAlgoInvoker::resultReceieve(_patient.patientlefteyesph);
//    _patient.patientlefteyecyl  = CAlgoInvoker::resultReceieve(_patient.patientlefteyecyl);
//    _patient.patientrightse     = CAlgoInvoker::resultReceieve(_patient.patientrightse);
//    _patient.patientrighteyesph = CAlgoInvoker::resultReceieve(_patient.patientrighteyesph);
//    _patient.patientrighteyecyl = CAlgoInvoker::resultReceieve(_patient.patientrighteyecyl);
//}
//
//QString CAlgoInvoker::resultReceieve(const QString &temp)
//{
//    if (CAlgoInvoker::truncAbnormalChar(temp).toDouble() < -10.5) {
//        return "<-10.5";
//    } else if(CAlgoInvoker::truncAbnormalChar(temp).toDouble() > 10.5) {
//        return ">+10.5";
//    } else {
//        return temp;
//    }
//}
//
//QString CAlgoInvoker::truncAbnormalChar(QString _val_str)
//{
//    return _val_str.replace("<", "").replace(">", "");
//}
//
//bool CAlgoInvoker::isVisionStrNormal(const QString &_vision)
//{
//    if (_vision.startsWith(">") || _vision.startsWith("<")) {
//        return false;
//    } else {
//        return true;
//    }
//}

// 计算均值和标准差
void CAlgoInvoker::calcMeanStdDev(const cv::Mat &_img, double &_mean, double &_std_dev)
{
    cv::Mat mat_mean, mat_stddev;
    cv::meanStdDev(_img, mat_mean, mat_stddev);      // TODO: 通过标准差判断衡量对比度，是否合理？
    _mean = mat_mean.ptr<double>(0)[0];
    _std_dev = mat_stddev.ptr<double>(0)[0];
}

void CAlgoInvoker::calcMeanStdDev(unsigned char *_img_data, double &_mean, double &_std_dev)
{
    cv::Mat img(IMG_HEIGHT, IMG_WIDTH, CV_8UC1, _img_data);
    calcMeanStdDev(img, _mean, _std_dev);
}

// 从多组统计视力数据中随机抽出一组
stVisionValue CAlgoInvoker::getRandomStatisticalVision(int _age_idx)
{
    // 统计视力数据（前两个年龄段，每个年龄段5组视力数据。数据来源：梁健晖）
    static stVisionValue StatisticalVisions[2][5] = {
        {
            {+2.25, -0.75, 78,  4.2, +1, -1, false, 48, +2.50, -1.00, 97,  4.2, +3, -1, false},
            {+2.50, -1.25, 89,  3.8, -1, -1, false, 45, +2.25, -1.00, 103, 3.8, +1, -1, false},
            {+2.50, -1.00, 100, 4.6, -3, +1, false, 50, +3.00, -1.25, 76,  4.6, -1, +1, false},
            {+2.75, -1.25, 103, 4.2, -4, -3, false, 48, +2.50, -1.00, 87,  4.2, -1, -3, false},
            {+2.50, -1.00, 93,  4.2, -3, -1, false, 48, +3.00, -1.25, 99,  4.6, +3, -1, false},
        },
        {
            {+2.00, -1.00, 76,  4.8, -2, +1, false, 49, +2.25, -1.25, 83,  4.8, +2, +2, false},
            {+2.25, -1.00, 80,  4.4, +1, -1, false, 47, +2.00, -0.75, 117, 4.4, +4, -1, false},
            {+2.00, -0.75, 72,  5.0, -2, -2, false, 52, +1.75, -0.50, 96,  5.0, -1, -1, false},
            {+2.50, -1.00, 107, 4.8, -1, +2, false, 47, +2.25, -1.00, 103, 4.8, +1, +2, false},
            {+1.75, -0.75, 110, 4.4, +1, -1, false, 49, +2.00, -0.50, 86,  4.4, +2, -1, false},
        },
    };

    // 是否统计值或月龄屈光值
    visionValueSource = 2;

    //
    int i = rand() % 5;
    if (_age_idx >= 0 && _age_idx <= 1) {
        return StatisticalVisions[_age_idx][i];
    } else {
        logCritical(__PRETTY_FUNCTION__ + QString(": getRandomStatisticalVision(): age group index %1 is out of range, the value reterned is false!").arg(_age_idx));
        return StatisticalVisions[1][i];
    }
}

bool CAlgoInvoker::setMonthAgeVisionByBirthday(stVisionValue &_vision, int _age_range, QDate *_birth_date)
{
    int month_age = -1;

    if (_birth_date) {
        // 根据生日计算月龄
        QDate today = QDate::currentDate();
        month_age = (today.year() - _birth_date->year()) * 12 + today.month() - _birth_date->month();
    } else {
        // 根据瞳距估算月龄
        if (_vision.PD > 0) {
            double pd = _vision.PD;

            // 瞳距值限制在 [26, 48] 区间
            if(pd < 26)
                pd = 26;
            else if(pd > 48)
                pd = 48;

            //
            month_age = round(36 * (pd - 26) / (48 - 26));

            // 如果上面算得月龄超出测量时所选年龄段，则等于最接近的年龄段区间边界值
            int min_month_age, max_month_age;
            getMonthBoundaryOfAgeRange(_age_range, min_month_age, max_month_age);

            if (month_age < min_month_age)
                month_age = min_month_age;
            else if (month_age > max_month_age)
                month_age = max_month_age;
        }
    }

    if (month_age >= 0) {
        return setMonthAgeVision(_vision, month_age);
    } else {
        return false;
    }
}

//
bool CAlgoInvoker::setMonthAgeVision(stVisionValue &_vision, int _month_age)
{
    struct stMonthAgeDiopter {
        float sph;
        float cyl;
    };

    const int MONTH_AGE_BOUNDS[6][2] = {        /* 数据来源：《20220321_刘宇_视筛0~3岁婴幼儿屈光数值一致性的解决办法.docx》，《开封市1359例婴幼儿屈光状态调查分析_李琳.pdf》 */
        { 6,  9},
        {10, 12},
        {13, 18},
        {19, 24},
        {25, 30},
        {31, 36},
    };

    const stMonthAgeDiopter MONTH_AGE_DIOPTER[] = {
        {+2.00, +1.37},
        {+1.81, +0.97},
        {+1.69, +0.77},
        {+1.69, +0.70},
        {+1.63, +0.79},
        {+1.58, +0.70},
    };

    //
    int idx = -1;
    if (_month_age < 6) {
        idx = 0;
    } else if (_month_age > 36) {
        idx = 5;
    } else {
        for (int i = 0; i < 6; i++) {
            if (_month_age >= MONTH_AGE_BOUNDS[i][0] && _month_age <= MONTH_AGE_BOUNDS[i][1]) {
                idx = i;
                break;
            }
        }
    }

    if (idx >= 0 && idx <=5) {
        float sph = MONTH_AGE_DIOPTER[idx].sph;
        float cyl = MONTH_AGE_DIOPTER[idx].cyl;

        // 正柱镜转为负柱镜
        sph = sph + cyl;
        cyl = -cyl;

        // 随机增减 0.25
        const float VALUE_ADD[3] = {-0.25, 0, +0.25};

        int rand_1 = Util::randomInt() % 3;
        _vision.LSph = sph + VALUE_ADD[rand_1];

        int rand_2 = Util::randomInt() % 3;
        _vision.LCyl = cyl + VALUE_ADD[rand_2];

        int rand_3 = Util::randomInt() % 3;
        _vision.RSph = sph + VALUE_ADD[rand_3];

        int rand_4 = Util::randomInt() % 3;
        _vision.RCyl = cyl + VALUE_ADD[rand_4];

        // 设置轴位值
        setMonthAgeAxis(_vision, _month_age);

        // 是否统计值或月龄屈光值
        visionValueSource = 1;

        //
        return true;
    } else {
        return false;
    }
}

// 获得月龄屈光度的轴位（设计文档：《20220415_梁健晖_前两年龄段轴位问题.docx》）
void CAlgoInvoker::setMonthAgeAxis(stVisionValue &_vision, int _month_age)
{
    if (_month_age < 6)
        _month_age = 6;
    if (_month_age > 48)
        _month_age = 48;

    // 用多项时计算轴位
    int axis = _month_age * -1.5 + 99.2;

    // 随机浮动
    int rand_offset_l = Util::randomInt() % 7 - 3;
    int axis_l = axis + rand_offset_l;
    int rand_offset_r = Util::randomInt() % 7 - 3;
    int axis_r = axis + rand_offset_r;

    // 随机翻转 /** 不能随机翻转，否则降低一致性，违背月龄估值的初衷 **/
    //bool is_reverse_l = Util::randomInt() % 2;
    //axis_l = (is_reverse_l ? 180 - axis_l : axis_l);
    //bool is_reverse_r = Util::randomInt() % 2;
    //axis_r = (is_reverse_r ? 180 - axis_r : axis_r);

    //
    _vision.LAx = axis_l;
    _vision.RAx = axis_r;
}

// 得到指定年龄段的最小和最大月龄
void CAlgoInvoker::getMonthBoundaryOfAgeRange(int _age_range, int &_min_month_age, int &_max_month_age)
{
    if (_age_range < 0)
        _age_range = 0;
    else if (_age_range > 4)
        _age_range = 4;

    if(0 == _age_range) {           // 6-12个月
        _min_month_age = 6;
        _max_month_age = 12;
    } else if (1 == _age_range) {   // 1-3岁
        _min_month_age = 12;
        _max_month_age = 36;
    } else if (2 == _age_range) {   // 3-6岁
        _min_month_age = 36;
        _max_month_age = 72;
    } else if (3 == _age_range) {   // 6-20岁
        _min_month_age = 72;
        _max_month_age = 240;
    } else if (4 == _age_range) {   // 20-100岁
        _min_month_age = 240;
        _max_month_age = 1200;
    }
}

//
static IplImage *srcImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
static IplImage *destImg = cvCreateImage(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);

//
void CAlgoInvoker::savePdfPreviewImg(unsigned char *pFrameBuffer)
{
    int p[3];
    p[0] = CV_IMWRITE_JPEG_QUALITY;
    p[1] = 20;
    p[2] = 0;

    cvSetData(srcImg, pFrameBuffer, IMG_WIDTH);
    cvEqualizeHist(srcImg, destImg);

    QDir previewImgDir("/media/pdfPreviewImg");
    if(!previewImgDir.exists())
    {
        qDebug() << "creat pdfPreviewImg";
        previewImgDir.mkdir("/media/pdfPreviewImg");
    }

    QString previewImgFile("/media/pdfPreviewImg/current_pdfPreview.jpg");      // saturation.bmp
    qDebug() << "save previewImgPath:" << previewImgFile;

    //cvSaveImage(previewImgFile.toLatin1(), destImg, p);
    cv::Mat mat = cv::cvarrToMat(destImg);
    cv::imwrite(previewImgFile.toStdString(), mat);

    cvZero(srcImg);
    cvZero(destImg);
}

int CAlgoInvoker::correctAxis(const int _ax)
{
    int ax = _ax;

    ax = ax % 180;
    if (ax < 0) {
        ax += 180;
    }

    return ax;
}

void CAlgoInvoker::generateDoubles(const double _avg, const double _deviation, const double _prec,
                               double &_num1, double &_num2, double &_num3)
{
    // 随机数种子
    static std::random_device rd;

    // 随机数引擎（算法）
    std::mt19937 gen(rd());

    // 随机数区间
    std::uniform_real_distribution<> dis(_avg, _avg + _deviation);

    // 生成三个随机数
    double random1 = dis(gen);
    double random2 = dis(gen);
    double random3 = dis(gen);

    // 移动3个随机数，使其均值等于 avg
    double avg_dev = _avg - (random1 + random2 + random3) / 3;

    _num1 = random1 + avg_dev;
    _num2 = random2 + avg_dev;
    _num3 = random3 + avg_dev;

    _num1 = Util::roundDouble(_num1, _prec);
    _num2 = Util::roundDouble(_num2, _prec);
    _num3 = Util::roundDouble(_num3, _prec);

    // TODO: 算法优化：先随机产生第一数，然后根据第一数与均值的偏差，限制第二数的随机范围，然后根据前两数和均值算得第三数。


}

QString CAlgoInvoker::doubleToDiopterStr(double _diopter, double _resolution)
{
    _diopter = Util::roundDouble(_diopter, _resolution);
    QString str = QString::number(_diopter, 'f', 2);
    int comp = Util::compDouble(_diopter, 0);
    if (comp == 0) {
        str = " " + str;
    } if (comp > 0) {
        str = "+" + str;
    }
    return str;
}

void CAlgoInvoker::switchCylSign_IntAx(const double &_sph_old, const double &_cyl_old, const int &_ax_old,
                           bool _is_cyl_negative, double &_sph_new, double &_cyl_new, int &_ax_new)
{
    if (!_is_cyl_negative) {    // 正散转换
        _sph_new = _sph_old + _cyl_old;     // 正散的球镜(屈光) = 负散球镜(屈光)+负散柱镜(散光)
        _cyl_new = -_cyl_old;               // 正散柱镜(散光) = 负散柱镜(散光)取反
        if (_ax_old >= 90) {                // 负散轴位角度>=90:正散轴位角 = 负散轴位角度-90度
            _ax_new = _ax_old - 90;
        } else {                            // 负散轴位角度<90:正散轴位角 = 负散轴位角度+90度
            _ax_new = _ax_old + 90;
        }
    } else {                    // 原始数据是负散值，不需要变化
        _sph_new = _sph_old;
        _cyl_new = _cyl_old;
        _ax_new  = _ax_old;
    }
}

void CAlgoInvoker::switchCylSign_DblAx(const double &_sph_old, const double &_cyl_old, const double &_ax_old,
                           bool _is_cyl_negative, double &_sph_new, double &_cyl_new, double &_ax_new)
{
    if (!_is_cyl_negative) {    // 正散转换
        int ax;
        switchCylSign_IntAx(_sph_old, _cyl_old, _ax_old, _is_cyl_negative, _sph_new, _cyl_new, ax);
        _ax_new = ax;
    } else {                    // 原始数据是负散值，不需要变化
        _sph_new = _sph_old;
        _cyl_new = _cyl_old;
        _ax_new  = _ax_old;
    }
}

void CAlgoInvoker::switchCylSign_StrAx(const QString &_sph_old, const QString &_cyl_old, const QString &_ax_old, bool _is_cyl_negative,
                           QString &_sph_new, QString &_cyl_new, QString &_ax_new)
{
    // 初始化为原值
    _sph_new = _sph_old;
    _cyl_new = _cyl_old;
    _ax_new  = _ax_old;

    // 正散转换（若原始数据是负散值，不需要变化）
    if (!_is_cyl_negative && !_sph_old.isEmpty()) {
        //
        double sph_old = _sph_old.toDouble();
        double cyl_old = _cyl_old.toDouble();
        double ax_old = _ax_old.toDouble();

        //
        double sph_new, cyl_new, ax_new;
        switchCylSign_DblAx(sph_old, cyl_old, ax_old, _is_cyl_negative, sph_new, cyl_new, ax_new);

        //
        _sph_new.clear();
        if (Util::compDouble(sph_new, 0) > 0) {    //显示时补上“+”
            _sph_new = "+";
        }
        _sph_new.append(QString::number(sph_new, 'f', 2));

        _cyl_new.clear();
        if (Util::compDouble(cyl_new, 0) > 0) {    //显示时补上“+”
            _cyl_new = "+";
        }
        _cyl_new.append(QString::number(cyl_new, 'f', 2));

        _ax_new = QString::number((int)ax_new);
    }
}

//void CAlgoInvoker::saturationSaveImage(unsigned char *pFrameBuffer, double value)
//{
//    qDebug() << "saturationSaveImage, gDistanceVal = " << g_DistanceVal;

//    if(currentPat.patientid == oldID)
//    {
//        timesCnt++;
//    }
//    else
//    {
//        oldID = currentPat.patientid;
//        timesCnt = 1;
//    }

//    QString strName = currentPat.patientid;
//    QString path;
//    if(value == 404)     //failed
//        path = QString("/media/photo/%1/saturated_pictures_f").arg(strName);
//    else
//        path = QString("/media/photo/%1/saturated_pictures").arg(strName);
//    QDir myDir(path);
//    if(!myDir.exists()){
//        if(!myDir.mkpath(path))
//            return;
//    }

//    IplImage *tempImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
//    cvSetData(tempImg, pFrameBuffer, IMG_WIDTH);

//    char buff[512];
//    //sprintf(buff, "%s%s", path.toLatin1().data(), "/saturation.bmp");
//    sprintf(buff, "%s%s%d%s", path.toLatin1().data(), "/", (int)g_DistanceVal, "_saturation.bmp");     //saturated_pictures
//    qDebug() << "save saturation_bmp:"<< buff;

//    cvSaveImage(buff, tempImg);
//    cvReleaseImageHeader(&tempImg);
//}

////保存超时图片(注意,相机超时时是不够23张图片的)  2020.11.3  tao     // TODO: 和 algo-invoker.cpp -> testSaveByteImageinFolder_() 整合
//void CAlgoInvoker::SaveByteFaultImageinFolder(unsigned char *pFrameBuffer, int index , int mode)
//{
//    QString PathName;
//    QString PresentID;
//    if(WinMeasure::getOperationMode() == normalMeasure)
//        PresentID = currentPat.patientid;
//    else
//        PresentID = currentPat.patientid;
//    if(PresentID != Old_Id)
//    {
//        Old_Id = PresentID;
//        gCameraRestartCnt = 0;
//    }

//    if(mode >= 1 && mode <= 3)
//        PathName = QString("/media/photo/%1").arg(PresentID);    //保存图片位置
//    else
//        PathName = QString("/media/photo/%1_%2/timeout_pictures").arg(PresentID).arg(gCameraRestartCnt);

//    QDir myDir(PathName);
//    if(!myDir.exists()){
//        if(!myDir.mkpath(PathName))
//            return;
//    }

//    QString strAnalog = QString::number(g_CameraIntf->getAnalogGain() * 100);
//    QString NumCount = QString("%1").arg(index,2,10,QLatin1Char('0')); //如果是个位数时,前面补0
//    QString finalbuff = PathName + "/temp" + NumCount + "_" + strAnalog + "_" + QString::number(g_CameraIntf->getExposureTime()) + ".bmp";  //QString::number(aeValue,'f',1) 保留1位小数

//    QByteArray ba = finalbuff.toLatin1();
//    IplImage *tempImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
//    cvSetData(tempImg, pFrameBuffer, IMG_WIDTH);
//    if(mode == 1){      //左眼模式
//        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
//        cv::Mat roi(image, cv::Rect(IMG_WIDTH / 2, 0, IMG_WIDTH / 2, IMG_HEIGHT));
//        cv::imwrite(finalbuff.toStdString(),roi);
//        return;
//    }
//    else if(mode == 2){  //右眼模式
//        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
//        cv::Mat roi(image, cv::Rect(0, 0, IMG_WIDTH / 2, IMG_HEIGHT));
//        cv::imwrite(finalbuff.toStdString(),roi);
//        return;
//    }
//    else{               //双眼模式
//        IplImage *tempImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
//        cvSetData(tempImg, pFrameBuffer, IMG_WIDTH);
//        cvSaveImage(ba.data(), tempImg);
//        cvReleaseImageHeader(&tempImg);
//    }
//}

void CAlgoInvoker::setCurrentPupilAlgoVer(enAlgoVerAll _pupil_algo_ver)
{
    currentPupilAlgoVer = _pupil_algo_ver;
}

/*
功能：计算图像的清晰度（来自：向祖松）
参数：grayImage--输入灰度图像
返回：图像的清晰度值，负数表示错误
*/
float CAlgoInvoker::calcClarity(const cv::Mat &grayImage)            // TODO: 和 CAlgoInvoker::calcClarity() 相比如何？
{
    float clarity = 0.0;

    if (grayImage.empty())
    {
        std::cout << "image data is null!" << std::endl;
        return clarity;
    }

    // 计算图像的平均梯度能量值
    int Df = 0;
    int gray = 0;
    for (int i = 0; i < grayImage.rows - 1; i++)
    {
        for (int j = 0; j < grayImage.cols - 1; j++)
        {
            int curGray = grayImage.at<uchar>(i, j);
            int rightGray = grayImage.at<uchar>(i, j + 1);
            int downGray = grayImage.at<uchar>(i + 1, j);
            Df += (curGray - rightGray) * (curGray - rightGray) + (curGray - downGray) * (curGray - downGray);

            gray += curGray;
        }
    }

    double d = (double)Df / (grayImage.rows * grayImage.cols);
    double grayAvg = (double)gray / (grayImage.rows * grayImage.cols);

    //if (abs(grayAvg) < 1e-5)
    //{
    //    cout << "devide by zero" << endl;
    //    return -1.0F;
    //}

    clarity = d / grayAvg;

    return clarity;
}

float CAlgoInvoker::calcClarity(unsigned char *_img_data)
{
    cv::Mat img(IMG_HEIGHT, IMG_WIDTH, CV_8UC1, _img_data);
    return calcClarity(img);
}

void CAlgoInvoker::setCountDetect(unsigned int _count)
{
    countDetect = _count;
}

unsigned int CAlgoInvoker::getCountDetect()
{
    return countDetect;
}

void CAlgoInvoker::slotDetectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, bool _is_need_calc_expo)
{
    qDebug() << "->->->->->-> Begin pupil detection ......";

    //
    g_WinMeasure->countPupilDetectSent--;

    //
    enSingleDualEyeMode single_dual_eye =  g_SingleDualEye;

    //
    stPupilInfo pupil_info_r, pupil_info_l;
    //memset(&pupil_info_r, 0, sizeof(stPupilInfo));
    //memset(&pupil_info_l, 0, sizeof(stPupilInfo));

    int avg = -1;
    bool over_expo = false;

    try {
        bool succ = detectPupil(_img_data, _img_idx, _age_range, pupil_info_r, pupil_info_l, false, single_dual_eye);
        if (succ) {
            if (_is_need_calc_expo) {
                int avg_in = -1;
                bool over_expo_in = false;
                bool is_calc_succ = calcExposure(_img_data, _img_idx, pupil_info_r, pupil_info_l, avg_in, over_expo_in, single_dual_eye);
                if (is_calc_succ) {
                    avg = avg_in;
                    over_expo = over_expo_in;
                }
            }
        } else {
            //static bool is_save_fail = false;
            //if (is_save_fail) {
            //    QString file_path = QString::asprintf("/root/debug/idxBuff_%.2d.png", _img_idx);
            //    Util::saveImgDataToImgFile2(_img_data, IMG_WIDTH, IMG_HEIGHT, file_path);
            //}
        }

        //
        emit sigPupilDetectionResult(_img_data, _img_idx, succ, pupil_info_r, pupil_info_l, avg, over_expo);

    } catch (...) {
        //logCritical();

        emit sigAlgoErr(algoErrType_DetectPupil, strerror(errno));

        emit sigPupilDetectionResult(_img_data, _img_idx, false, pupil_info_r, pupil_info_l, avg, over_expo);
    }

    qDebug() << "<-<-<-<-<-<- End pupil detection ......";
}

//void CAlgoInvoker::slotCalcVision(std::vector<unsigned char *> *_img_list, const CPatient &_patient)
//{
//    qDebug() << __PRETTY_FUNCTION__ << ": slotCalcVision() into ...";
//
//    //
//    isCalculatingVision = true;
//
//    //
//    try {
//        // 若光路类型不是常规，则将图集转换为与常规光路类型一致
//        if (opticalPathType_General != g_opticalPathType) {
//            convertImageSetToGeneralOpticalType(g_opticalPathType, _img_list);
//        }
//
//        //
//        QString img_dir_name = _patient.getImgDirName();
//        calcVision(*_img_list, g_SingleDualEye, img_dir_name, _patient.getBirthDate());
//
//    } catch (...) {
//        QString err_str = strerror(errno);
//        logWarning(__PRETTY_FUNCTION__ + QString(": slotCalcVision(): error: ") + err_str, CGlobal::LOG_ALGO);
//
//        emit sigAlgoErr(algoErrType_CalcVision, err_str);
//
//        emit sigCalcVisionFinished(calcResultState_ProgramException, stVisionValue {}, stVisionAbnormal {});
//    }
//    //emit sigReleaseTurnLampImgList();
//
//    isCalculatingVision = false;
//    qDebug() << __PRETTY_FUNCTION__ << ": slotCalcVision() ended ...";
//}

void CAlgoInvoker::slot_this_CalcVisionBegin(const enAgeRange _age_range, const QString _img_dir_name, const enSingleDualEyeMode _single_dual_eye,
                                             int _round_idx, CAlgoInvoker::QPrivateSignal)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): calling calcVisionBegin() of algo model: "
             << QString("_age_range = %1, _img_dir_name = %2, _single_dual_eye = %3")
                .arg(CAgeRange::getAgeRangeDesc(_age_range)).arg(_img_dir_name).arg(enumToText_SingleDualEyeMode(_single_dual_eye));

    //
    bool succ = m_algoIntf->calcVisionBegin(_age_range, _img_dir_name.toStdString(), _single_dual_eye, _round_idx);
    if (!succ) {
        constexpr char err_msg[] = "Failed to call algo interface calcVisionBegin()!";
        qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): " << err_msg;
        emit sigMsgNotify(err_msg);
    }
}

void CAlgoInvoker::slot_this_ExecuteAlgoCommand(
        stAlgoCommand command,
        CAlgoInvoker::QPrivateSignal)
{
    if (m_algoIntf == nullptr) {
        return;
    }

    const stAlgoCommandResult result =
            m_algoIntf->handleAlgoCommand(command);
    if (!result.success) {
        // 命令失败仍然结束当前命令处理，避免在算法外新增高频诊断输出。
        return;
    }
}

void CAlgoInvoker::slot_this_AppendImage(unsigned char *_img_data, const int _w, const int _h, const int _round_idx,
                                         const int _img_num, const bool _is_dist_fit)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): " << QString("_img_num = %1").arg(_img_num);

    //
    cv::Mat img(_h, _w, CV_8UC1, _img_data);

    //cv::imshow("AppendImage", img);
    //cv::waitKey(0);
    //cv::destroyAllWindows();

    enCalcResultState stat_algo = m_algoIntf->appendImage(img, _round_idx, _img_num, _is_dist_fit);
    // 正式结果已经产生后，采集线程可能仍会送来本轮末尾的少量帧。
    // 算法接口会返回Aborted；这是预期的清场行为，不应再弹出“算法调用失败”。
    if (calcResultState_Aborted == stat_algo
            || calcResultState_RetryNextRound == stat_algo) {
        return;
    }
    if (calcResultState_Succ != stat_algo) {
        constexpr char err_msg[] = "Failed to call algo interface appendImage()!";
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): " << err_msg;
        emit sigMsgNotify(err_msg);
    }
}

void CAlgoInvoker::slot_timerCalcVisionCallback_timeout()
{
    //
    doOnCalcVisionTimeout();
}

bool CAlgoInvoker::detectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l,
                           bool _is_calc_vision, enSingleDualEyeMode _single_dual_eye)
{
    //
    g_elapsedDetectOnce.start();

    bool is_succ = m_algoIntf->detectPupil(_img_data, _img_idx, _age_range, _pupil_info_r, _pupil_info_l, _is_calc_vision, _single_dual_eye);

    g_lastDetectTime = g_elapsedDetectOnce.elapsed();

    g_PupilState = (is_succ ? 0 : 2);       // TODO: 瞳孔过小的判断？

    // 左、右眼是否需要计算
    bool is_need_right  = (singleDualEyeMode_Right & _single_dual_eye);
    bool is_need_left   = (singleDualEyeMode_Left & _single_dual_eye);

    //
    memset(&g_SaturationCenterR, 0, sizeof(CvPoint3D32f));
    memset(&g_SaturationCenterL, 0, sizeof(CvPoint3D32f));

    if (is_need_right) {
        g_SaturationCenterR.x = _pupil_info_r.center.x;
        g_SaturationCenterR.y = _pupil_info_r.center.y;
        g_SaturationCenterR.z = _pupil_info_r.radius;
    }
    if (is_need_left) {
        g_SaturationCenterL.x = _pupil_info_l.center.x;
        g_SaturationCenterL.y = _pupil_info_l.center.y;
        g_SaturationCenterL.z = _pupil_info_l.radius;
    }

    //
    return is_succ;
}

bool CAlgoInvoker::calcExposure(unsigned char *_img_data, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l,
                            int &_avg, bool &_over_expo, enSingleDualEyeMode _single_dual_eye)
{
    bool is_succ = m_algoIntf->calcExposure(_img_data, _img_idx, _pupil_info_r, _pupil_info_l, _avg, _over_expo, _single_dual_eye);

    //
    return is_succ;
}

//enCalcResultState CAlgoInvoker::calcVision(std::vector<unsigned char *> &_img_list, enSingleDualEyeMode _single_dual_eye,
//                                       QString _img_dir_name, QDate _birth_date)
//{
//    Q_UNUSED(_birth_date)
//
//    //
//    stVisionValue vision {};
//    stVisionAbnormal abnormal {};
//
//    //
//    enAgeRange age_range = WinMeasure::getCurrentAgeRange();
//    age_range = WinMeasure::getAgeRangeLimited(age_range);
//
//    //
//    if (_img_dir_name.length() > 0) {
//        QString path_img = QString("/media/photo/%1").arg(_img_dir_name);
//        QDir dir_img(path_img);
//        if (dir_img.exists()) {
//            QString cmd = QString("rm %1 -r").arg(path_img);
//            system(cmd.toLatin1().data());
//        }
//    } else {
//        qWarning() << "_img_dir_name is empty !";
//
//    }
//
//    // 调用算法接口计算屈光
//    enCalcResultState calc_stat = calcResultState_Succ;
//
//    //
//    emit sigCalcVisionFinished(calc_stat, vision, abnormal);
//
//    //
//    return calc_stat;
//}

void CAlgoInvoker::doOnGetCalcVisionResult(const enCalcResultState _result_stat, int _round_idx,
                                           const stVisionValue &_vision, const stVisionAbnormal &_abnormal,
                                           const std::vector<stVisionValue> &_result_set, bool _finished, bool _questionable)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ... "
             << ", _result_stat = " << enumToName_CalcResultState(_result_stat)
             << ", _result_set.size() = " << _result_set.size()
             << ", _finished = " << Util::bool2str(_finished)
             << ", diopterCalcStat = " << enumToText_DiopterCalcStat(m_diopterCalcStat);

    //
    //stopCalcVisionTimeoutChecking();      // NOTE: 2025-12-18：改为测量模块连续转灯，收到计算成功消息后直接显示结果，不需调用算法策略。所以不需超时检查。

    // 结果回调的检查：须已开始计算，且未回调，才能回调             // NOTE: 避免被重复回调
    //if (enDiopterCalcStat::HasBegin != m_diopterCalcStat) {
    //    QString err_msg;
    //    switch (m_diopterCalcStat) {
    //    case enDiopterCalcStat::NotBegin:
    //        err_msg = "m_diopterCalcStat checking failed: not started";
    //        break;
    //    case enDiopterCalcStat::HasBegin:
    //        err_msg = QString("m_diopterCalcStat checking failed: LogicError: unexpected value(%1)!").arg(enumToText_DiopterCalcStat(m_diopterCalcStat));
    //        break;
    //    case enDiopterCalcStat::HasCallback:
    //        err_msg = "m_diopterCalcStat checking failed: callback repeated!";
    //        break;
    //    case enDiopterCalcStat::HasEnd:
    //        err_msg = "m_diopterCalcStat checking failed: has ended!";
    //        break;
    //    }
    //
    //    //
    //    emit sigAlgoErr(algoErrType_CalcVision, err_msg);
    //    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): " << err_msg;
    //
    //    //
    //    return;
    //}
    // NOTE: 流程改为不等待结果回调，直接连续转灯后，这个检查的逻辑不对了。

    //
    m_diopterCalcStat = enDiopterCalcStat::HasCallback;
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_diopterCalcStat changed: " << enumToText_DiopterCalcStat(m_diopterCalcStat);

    //
    //if (g_calcVision.isValid()) {
    //    qDebug() << "CalcVision time consume: " << g_calcVision.elapsed() << "ms";
    //}

    // 信号发送
    emit sigCalcVisionCallbackReceived(_result_stat, _round_idx, _vision, _abnormal,
                                       _result_set, _finished, _questionable);
}

void CAlgoInvoker::doOnCalcVisionTimeout()
{
    qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    // 超时事件里，应检查当前状态
    if (enDiopterCalcStat::HasBegin == m_diopterCalcStat) {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): emiting sigCalcVisionCallbackReceived(ResultCallbackTimeout) ...";
        emit sigCalcVisionCallbackReceived(calcResultState_ResultCallbackTimeout, -1,
                                           stVisionValue {}, stVisionAbnormal {},
                                           std::vector<stVisionValue> {}, false, false);
    } else {
        qWarning() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): LogicError: current m_diopterCalcStat(="
                   << enumToText_DiopterCalcStat(m_diopterCalcStat) << ") not correct! timeout skipped!";
    }

}

///============================================================================
/// 局部函数

// 测试过程中保存22张图片     // TODO: 这是从 algo.cpp -> testSaveByteImageinFolder() 拷贝，待整合
void CAlgoInvoker::testSaveByteImageinFolder_(unsigned char *pFrameBuffer, int index, QString _img_dir_name, int mode)
{
    //qDebug()<<"testSaveImageinFolder*********************************************";
    QString strName = _img_dir_name;
    QString file_path;

    QString path_img_dir = "";
    if(mode >= 1 && mode <= 3)
        path_img_dir = QString("/media/photo/%1").arg(strName);    // 保存图片位置
    else
        path_img_dir = QString("/media/photo/%1/succ_pictures").arg(strName);

    QDir img_dir(path_img_dir);
    if(!img_dir.exists()){
        //qDebug() << "making img dir '" << path_img_dir << "'";
        bool is_succ_mkdir = img_dir.mkpath(path_img_dir);
        if (!is_succ_mkdir) {
            qWarning() << "make dir '" << path_img_dir << "' failed";
            return;
        } else {
            if (!QFile::exists(path_img_dir)) {
                qWarning() << "error: dir '" << path_img_dir << "' make failed";
            }
        }
    }

    QString NumCount = QString("%1").arg(index, 2, 10, QLatin1Char('0')); //如果是个位数时,前面补0
    file_path = path_img_dir + "/temp" + NumCount + ".jpg";         // NOTE: 2025-06-04 为降低图像占用空间，格式由 .bmp 转为 .jpg
    qDebug() << "path of image dir = '" << file_path << "'";

    IplImage *tempImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
    cvSetData(tempImg, pFrameBuffer, IMG_WIDTH);
    if(mode == 1){      //左眼模式
        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
        cv::Mat roi(image, cv::Rect(IMG_WIDTH / 2, 0, IMG_WIDTH / 2, IMG_HEIGHT));
        cv::imwrite(file_path.toStdString(), roi);
        return;
    }
    else if(mode == 2){  //右眼模式
        cv::Mat image = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1,pFrameBuffer).clone(); // make a copy
        cv::Mat roi(image, cv::Rect(0, 0, IMG_WIDTH / 2, IMG_HEIGHT));
        cv::imwrite(file_path.toStdString(), roi);
        return;
    }
    else{               //双眼模式
        qDebug() << "saving img to '" << file_path << "'";

        //IplImage *tempImg = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
        //cvSetData(tempImg, pFrameBuffer, IMG_WIDTH);
        //cvSaveImage(file_path.toLatin1().data(), tempImg);
        //cvReleaseImageHeader(&tempImg);

        cv::Mat mat = cv::Mat(IMG_HEIGHT, IMG_WIDTH, CV_8UC1, pFrameBuffer);
        cv::imwrite(file_path.toStdString(), mat);

        qDebug() << "img saved";
    }
}

// 对调转灯图集中指定的两张图像的位置
void CAlgoInvoker::swapPosiOfTurnLampImgs(std::vector<unsigned char *> &_img_set, const QPair<int, int> &_pair)
{
    uchar * img_data = _img_set.at(_pair.first);
    _img_set[_pair.first] = _img_set.at(_pair.second);
    _img_set[_pair.second] = img_data;
}

void CAlgoInvoker::convertImageSetToGeneralOpticalType(const enOpticalPathType _optical_type, std::vector<unsigned char *> *_img_set)
{
    // 若非“直线”光路类型，变换图像，使之与“直线”光路类型一致
    if (opticalPathType_Square == _optical_type) {      /* 方形视筛箱，等校于被测者旋转 180 度倒立，所以图像和灯号都要旋转180度 */
        // 灯序旋转180度
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 1,  2});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 3,  4});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 5,  6});

        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 7,  8});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 9, 10});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {11, 12});

        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {13, 14});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {15, 16});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {17, 18});

        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {19, 20});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {21, 22});

        // 图像旋转180度
        IplImage *img = cvCreateImageHeader(cvSize(g_CameraIntf->getImgWidth(), g_CameraIntf->getImgHeight()), IPL_DEPTH_8U, 1);
        for (int i = (int)_img_set->size() - 1; i >= 0; i--) {
            cvSetData(img, _img_set->at(i), g_CameraIntf->getImgWidth());
            cvFlip(img, NULL, -1);       // flip_mode 参数值：-1, 旋转180度，0:垂直镜像，1:水平镜像
        }
        cvReleaseImageHeader(&img);
        img = NULL;
    } else if (opticalPathType_LShape == _optical_type) {
        // 若是使用了 L 形视筛箱，则将灯序水平对调
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 1,  2});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 3,  4});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 5,  6});

        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 7, 13});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> { 9, 15});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {11, 17});

        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {14,  8});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {16, 10});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {18, 12});

        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {20, 22});
        swapPosiOfTurnLampImgs(*_img_set, QPair<int, int> {21, 19});

        // 若是使用了 L 形视筛箱，则将图像水平镜像
        IplImage *img = cvCreateImageHeader(cvSize(g_CameraIntf->getImgWidth(), g_CameraIntf->getImgHeight()), IPL_DEPTH_8U, 1);
        for (int i = (int)_img_set->size() - 1; i >= 0; i--) {
            cvSetData(img, _img_set->at(i), g_CameraIntf->getImgWidth());
            cvFlip(img, NULL, 1);       // flip_mode 参数值：-1, 旋转180度，0:垂直镜像，1:水平镜像
        }
        cvReleaseImageHeader(&img);
        img = NULL;
    }
}

bool CAlgoInvoker::convertImageAndNumberToGeneralOpticalType(const enOpticalPathType _optical_type, unsigned char * const _img_data,
                                                            const int _img_num_old, int &_img_num_new, QString &_err_msg)
{
    //
    _img_num_new = -1;

    //
    static const QList<QPair<int, int>> IMG_NUM_PAIR__SQUARE = QList<QPair<int, int>>()     // 方形筛查箱的图号对换：灯序旋转180度
            << QPair<int, int> { 1,  2}
            << QPair<int, int> { 3,  4}
            << QPair<int, int> { 5,  6}

            << QPair<int, int> { 7,  8}
            << QPair<int, int> { 9, 10}
            << QPair<int, int> {11, 12}

            << QPair<int, int> {13, 14}
            << QPair<int, int> {15, 16}
            << QPair<int, int> {17, 18}

            << QPair<int, int> {19, 20}
            << QPair<int, int> {21, 22}
            ;

    static const QList<QPair<int, int>> IMG_NUM_PAIR__LSHAPE = QList<QPair<int, int>>()     // L 形筛查箱的图号对换：灯序水平对调
            << QPair<int, int> { 1,  2}
            << QPair<int, int> { 3,  4}
            << QPair<int, int> { 5,  6}

            << QPair<int, int> { 7, 13}
            << QPair<int, int> { 9, 15}
            << QPair<int, int> {11, 17}

            << QPair<int, int> {14,  8}
            << QPair<int, int> {16, 10}
            << QPair<int, int> {18, 12}

            << QPair<int, int> {20, 22}
            << QPair<int, int> {21, 19}
            ;

    //
    const QList<QPair<int, int>> *img_num_pair_list {nullptr};
    int flip_mode = -2;
    if (opticalPathType_Square == _optical_type) {      /* 方形视筛箱，等效于被测者旋转 180 度倒立，所以图像和灯号都要旋转180度 */
        // 灯号转换
        img_num_pair_list = &IMG_NUM_PAIR__SQUARE;

        // 图像旋转180度
        flip_mode = -1;       // flip_mode 参数值：-1, 旋转180度，0:垂直镜像，1:水平镜像
    } else if (opticalPathType_LShape == _optical_type) {
        // 灯号转换
        img_num_pair_list = &IMG_NUM_PAIR__LSHAPE;

        // 若是使用了 L 形视筛箱，则将图像水平镜像
        flip_mode = 1;       // flip_mode 参数值：-1, 旋转180度，0:垂直镜像，1:水平镜像
    }

    //
    int img_num_new = -1;
    if (img_num_pair_list) {
        for (const auto pair : (*img_num_pair_list)) {
            if (_img_num_old == pair.first) {
                img_num_new = pair.second;
                break;
            } else if (_img_num_old == pair.second) {
                img_num_new = pair.first;
                break;
            }
        }
    }

    if (img_num_new <= 0) {
        _err_msg = QString("ParamError: Failed to convert image number! opticalType = %1, num_old = %2")
                .arg(COpticalPathType::getDiscrip(_optical_type)).arg(_img_num_old);
        return false;
    }
    _img_num_new = img_num_new;

    //
    if (flip_mode >= -1 && flip_mode <= 1) {
        IplImage *img = cvCreateImageHeader(cvSize(g_CameraIntf->getImgWidth(), g_CameraIntf->getImgHeight()), IPL_DEPTH_8U, 1);
        cvSetData(img, _img_data, g_CameraIntf->getImgWidth());
        cvFlip(img, NULL, flip_mode);
        cvReleaseImageHeader(&img);
        img = NULL;
    }

    //
    return true;
}

cv::Mat CAlgoInvoker::pixmapToMat(const QPixmap &_pixmap) {
    QImage image = _pixmap.toImage();

     // Handle potential format issues (e.g., convert to RGB888)
     if(image.format() != QImage::Format_RGB888 && image.format() != QImage::Format_Grayscale8){
         image = image.convertToFormat(QImage::Format_RGB888);
     }

     return cv::Mat(
         image.height(),
         image.width(),
         CV_8UC3,
         const_cast<uchar*>(image.bits()),
         static_cast<size_t>(image.bytesPerLine())
     ).clone(); // clone to ensure deep copy and data alignment.
}

QPixmap CAlgoInvoker::matToPixmap(const cv::Mat &_mat)
{
    QImage img(_mat.data,
               _mat.cols,
               _mat.rows,
               static_cast<int>(_mat.step),
               _mat.channels() == 1 ? QImage::Format_Grayscale8 : QImage::Format_BGR888
                         );

    return QPixmap::fromImage(std::move(img));
}

cv::Mat CAlgoInvoker::readAndEqualizeHistFromFileToCvMat(const QString &_path)
{
    cv::Mat mat = cv::imread(_path.toStdString(), cv::IMREAD_GRAYSCALE);
    if (!mat.empty()) {
        cv::Mat mat_2;
        cv::equalizeHist(mat, mat_2);
        return mat_2;
    } else {
        return cv::Mat();
    }
}

QPixmap CAlgoInvoker::readAndEqualizeHistFromFileToPixmap(const QString &_path)
{
    cv::Mat mat = CAlgoInvoker::readAndEqualizeHistFromFileToCvMat(_path);
    if (!mat.empty()) {
        QPixmap pixmap = CAlgoInvoker::matToPixmap(mat);
        return pixmap;
    } else {
        return QPixmap();
    }
}

IplImage *CAlgoInvoker::readAndEqualizeHistFromFileToIplImage(const QString &_path)
{
    IplImage *img = cvLoadImage(_path.toLatin1(), CV_LOAD_IMAGE_GRAYSCALE);
    if (img) {
        IplImage *img2 = cvCreateImage(cvSize(img->width, img->height), img->depth, img->nChannels);
        cvEqualizeHist(img, img2);
        cvReleaseImage(&img);
        img = nullptr;
        return img2;
    } else {
        return nullptr;
    }
}

void CAlgoInvoker::calcVisionBegin(const enAgeRange _age_range, const QString _img_dir_name, const enSingleDualEyeMode _single_dual_eye,
                                   int _round_idx)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, _round_idx = " << _round_idx;

    //
    emit sigCalcVisionBegin(_age_range, _img_dir_name, _single_dual_eye, _round_idx, QPrivateSignal());

    //
    m_diopterCalcStat = enDiopterCalcStat::HasBegin;
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): m_diopterCalcStat changed: " << enumToText_DiopterCalcStat(m_diopterCalcStat);

    //
    //g_calcVision.start();

}

void CAlgoInvoker::appendImage(unsigned char *_img_data, const int _w, const int _h, const int _round_idx, const int _img_num, const bool _is_dist_fit)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): " << QString("_img_num = %1").arg(_img_num);

    //
    emit sigAppendImage(_img_data, _w, _h, _round_idx, _img_num, _is_dist_fit, QPrivateSignal());
}

void CAlgoInvoker::calcVisionEnd()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ... diopterCalcStat = " << enumToText_DiopterCalcStat(m_diopterCalcStat);

    // 通知算法模块中止
    //emit sigCalcVisionBegin(ageRange_4_20_100_YEAE, "", singleDualEyeMode_Both, -1, QPrivateSignal());      // TODO: 这样合适吗？

    // 结果计算完成事件
    doOnGetCalcVisionResult(calcResultState_Aborted, -1,
                            stVisionValue {}, stVisionAbnormal {},
                            std::vector<stVisionValue> {}, false, false);

}

void CAlgoInvoker::startCalcVisionTimeoutChecking()
{
    // 结果回调的检查：须在超时内回调
    static constexpr int TIMEOUT_MS = 3000;         // NOTE: 2025-11-21 嵌入式平台实测耗时 1200ms~1400ms
    // TODO: 2025-11-25 实测 2000ms 超时，基本都通不过，暂定去掉这个检查？

    //
    m_timerCalcVisionCallback->start(TIMEOUT_MS);

}

void CAlgoInvoker::stopCalcVisionTimeoutChecking()
{
    // 停止回调超时
    if (m_timerCalcVisionCallback->isActive()) {
        m_timerCalcVisionCallback->stop();
    }

}

///============================================================================
/// class CCalcImgInfo

CCalcImgInfo::CCalcImgInfo(QObject *parent) : QObject(parent)
{

}

CCalcImgInfo::~CCalcImgInfo()
{
    reset();
}

void CCalcImgInfo::reset()
{
    mean = -1;
    stdDev = -1;
    clarity = -1;
}

void CCalcImgInfo::slotCalcImgInfo(unsigned char *_img_data)
{
    static bool is_save = false;

    cv::Mat img(IMG_HEIGHT, IMG_WIDTH, CV_8UC1, _img_data);

    if (is_save) {
        QString dir_path = "/media/photo";
        if (!QFile::exists(dir_path)) {
            QString cmd = QString("mkdir -p %1").arg(dir_path);
            system(cmd.toUtf8().data());
        }

        std::string file_path = (dir_path + QDir::separator() + "%1.bmp").arg(QTime::currentTime().toString("HHmmss")).toStdString();
        cv::imwrite(file_path, img);

        is_save = false;
    }

    clarity = CAlgoInvoker::calcClarity(img);
    CAlgoInvoker::calcMeanStdDev(img, mean, stdDev);
}
