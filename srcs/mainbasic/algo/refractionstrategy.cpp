#include "refractionstrategy.h"
#include <numeric>
#include <cmath>
#include <limits>
#include <algorithm>

#include <QDebug>
#include <QStringList>

#include "perftimer.h"

using namespace std;

// 初始化静态成员变量
int RefractionStrategy::stableCount = 2;
int RefractionStrategy::maxRecordCount = 3;
double RefractionStrategy::rangeThreshold = 0.5; // 0.5D

// =========================================================================
//  核心执行策略 (Entry Point)
// =========================================================================
bool RefractionStrategy::executeAlgoPolicy(std::vector<stVisionValue> &_result_set,
                                           stVisionValue &_vision,
                                           stVisionAbnormal &_vision_abnormal,
                                           bool &_questionable)
{
    bool is_finished = false;

    // 2. 检查稳定性（寻找满足 range <= 0.5D 的子集）
    StabilityResult stability = RefractionStrategy::checkStabilityIterative(_result_set, stableCount);

    // 3. 判定逻辑：
    //    a. 左右眼均满足条件 (isStable为真)
    //    b. 或者 列表个数 >= maxRecordCount
    if (stability.isStable) {
        // 【稳定分支】：传入稳定子集进行计算
        _vision = RefractionStrategy::calculate(stability.stableSubset, _questionable);
        _vision_abnormal = stVisionAbnormal{};
        is_finished = true;
    } else if ((int)_result_set.size() >= maxRecordCount) {
        // 【强制结算分支】：传入所有结果进行计算（此时数据可能差异较大）
        _vision = RefractionStrategy::calculate(_result_set, _questionable);
        _vision_abnormal = stVisionAbnormal{};
        is_finished = true;
    } else {
        _vision = RefractionStrategy::calculate(_result_set, _questionable);
        _vision_abnormal = stVisionAbnormal{};
        is_finished = false;
    }

    // 正式测量中“为什么继续转灯”必须可从日志直接判断。这里仅记录策略
    // 的输入和决定，不改变原有的2组稳定/3组强制结算规则。
    std::vector<double> rSph;
    std::vector<double> rCyl;
    std::vector<double> lSph;
    std::vector<double> lCyl;
    rSph.reserve(_result_set.size());
    rCyl.reserve(_result_set.size());
    lSph.reserve(_result_set.size());
    lCyl.reserve(_result_set.size());
    for (const stVisionValue &value : _result_set) {
        rSph.push_back(value.RSph);
        rCyl.push_back(value.RCyl);
        lSph.push_back(value.LSph);
        lCyl.push_back(value.LCyl);
    }
    QStringList stableIndices;
    for (int index : stability.stableIndices) {
        stableIndices << QString::number(index);
    }
    const QString decision = stability.isStable
            ? QStringLiteral("stable_subset")
            : (static_cast<int>(_result_set.size()) >= maxRecordCount
               ? QStringLiteral("max_records_forced_settlement")
               : QStringLiteral("waiting_for_next_valid_result"));
    ALGO_DEBUG_LOG(
        qInfo().noquote()
                << QString("RefractionPolicy: records=%1, stable=%2, "
                           "stable_indices=%3, ranges[R_sph=%4,R_cyl=%5,"
                           "L_sph=%6,L_cyl=%7], threshold=%8, decision=%9")
                   .arg(static_cast<int>(_result_set.size()))
                   .arg(stability.isStable ? "yes" : "no")
                   .arg(stableIndices.isEmpty()
                        ? QStringLiteral("none") : stableIndices.join(","))
                   .arg(calculateRange(rSph), 0, 'f', 2)
                   .arg(calculateRange(rCyl), 0, 'f', 2)
                   .arg(calculateRange(lSph), 0, 'f', 2)
                   .arg(calculateRange(lCyl), 0, 'f', 2)
                   .arg(rangeThreshold, 0, 'f', 2)
                   .arg(decision)
    );

#if ENABLE_REFRACTION_POLICY_VERBOSE_LOG
    qDebug() << "RefractionStrategy::executeAlgoPolicy():"
             << "result_count =" << static_cast<int>(_result_set.size())
             << ", stableCount =" << stableCount
             << ", maxRecordCount =" << maxRecordCount
             << ", isStable =" << stability.isStable
             << ", stableSubset =" << static_cast<int>(stability.stableSubset.size())
             << ", is_finished =" << is_finished
             << ", questionable =" << _questionable;
#endif

    return is_finished;
}

// =========================================================================
//  核心计算逻辑 (Calculate Strategy)
// =========================================================================
stVisionValue RefractionStrategy::calculate(const vector<stVisionValue>& measurements,
                                            bool& isQuestionable) {
    stVisionValue final_result = {};
    isQuestionable = false;

    if (measurements.empty()) {
        return final_result;
    }

    // 左右眼独立计算
    processSingleEye(measurements, final_result, whichEye_Right, isQuestionable);
    processSingleEye(measurements, final_result, whichEye_Left, isQuestionable); // 只要有一只眼存疑，整体即存疑

    // 处理瞳距：始终取平均
    vector<double> pd_values;
    for (const auto& m : measurements) {
        pd_values.push_back(static_cast<double>(m.PD));
    }
    final_result.PD = static_cast<int>(round(calculateAverage(pd_values)));

    return final_result;
}

/**
 * @brief 处理单眼视力数据
 * 严格对应流程图 "结果策略" 分支
 */
void RefractionStrategy::processSingleEye(const std::vector<stVisionValue>& measurements,
                                          stVisionValue& final_result,
                                          enWhichEye eye,
                                          bool& isQuestionable)
{
    // --- 1. 数据提取 ---
    std::vector<double> sph_list, cyl_list;
    std::vector<int>    ax_list;

    for (const auto& m : measurements) {
        if (eye == whichEye_Right) {
            sph_list.push_back(m.RSph);
            cyl_list.push_back(m.RCyl);
            ax_list.push_back(m.RAx);
        } else {
            sph_list.push_back(m.LSph);
            cyl_list.push_back(m.LCyl);
            ax_list.push_back(m.LAx);
        }
    }

    if (sph_list.empty()) return;
    size_t count = sph_list.size();

    // --- 2. 策略分支 ---

    // === 分支 A：一组数据 ===
    if (count == 1) {
        double s = sph_list[0];
        double c = cyl_list[0];
        int a    = ax_list[0];

        // 写入结果
        final_result.setRef(s, c, a, eye);
        final_result.copyOtherParamsFrom(measurements[0], eye);

        // 检查存疑：球镜<-7.0D 或 >+4.5D 且 柱镜绝对值>=2D
        if (isResultQuestionable(s, c)) {
            isQuestionable = true;
        }
        return;
    }

    // === 分支 B：多组数据 (>=2) ===
    // 计算极差
    double rangeSph = calculateRange(sph_list);
    double rangeCyl = calculateRange(cyl_list);

    double finalSph, finalCyl;
    int finalAx;
    const stVisionValue* refRecord = nullptr; // 用于非屈光参数的参考记录

    // 判断：球镜或柱镜 <= rangeThreshold (注意：流程图是分别判断，但通常只要有不稳定就需要特殊处理)
    // 流程图逻辑："球镜或柱镜 <= rangeThreshold" -> 是 -> 取平均
    // 这里理解为：如果两者差异都在允许范围内(通常逻辑)，则平均；否则进入差异处理。
    // 严格照图："球镜或柱镜 <= rangeThreshold" 如果为否(即差异大)，走右侧。
    // 代码实现逻辑：只要有一个指标不稳定(>0.5)，就应该进入不稳定处理逻辑，以保证安全。

    bool isStable = (rangeSph <= rangeThreshold && rangeCyl <= rangeThreshold);

    if (isStable) {
        // --- 情况 B1：稳定 (差异小) ---
        // 球镜或柱镜取平均，轴位取平均
        finalSph = calculateAverage(sph_list);
        finalCyl = calculateAverage(cyl_list);
        finalAx  = calculateAverageAxis(ax_list);

        // 其他参数：随最后一次或随机，这里取第一个作为基准
        refRecord = &measurements[0];
    }
    else {
        // --- 情况 B2：不稳定 (差异大) ---
        // 逻辑：球镜或柱镜取最接近平均值 并且 绝对值较小的值
        // 轴位等其他参数选取随柱镜

        // 1. 计算平均值作为靶心
        double avgSph = calculateAverage(sph_list);
        double avgCyl = calculateAverage(cyl_list);

        // 2. 选择球镜：最接近平均值且绝对值更小
        finalSph = selectBestValue(sph_list, avgSph);

        // 3. 选择柱镜：最接近平均值且绝对值更小
        // 注意：这里需要找到选中的那个柱镜对应的原始索引，因为轴位要跟随柱镜
        int bestCylIdx = -1;
        double minMetric = std::numeric_limits<double>::max();

        for (size_t i = 0; i < cyl_list.size(); ++i) {
            double val = cyl_list[i];
            // 评价指标：优先距离平均值近，其次绝对值小
            // 这里的权重设计：距离优先。若距离相等，选绝对值小的。
            double dist = std::abs(val - avgCyl);

            // 为了比较方便，可以只看距离，如果距离极度接近，比绝对值
            if (bestCylIdx == -1) {
                bestCylIdx = i;
                continue;
            }

            double currentDist = std::abs(cyl_list[bestCylIdx] - avgCyl);

            if (dist < currentDist - 0.001) { // 明显更近
                bestCylIdx = i;
            } else if (std::abs(dist - currentDist) <= 0.001) { // 距离相当
                // 比较绝对值，取小的
                if (std::abs(val) < std::abs(cyl_list[bestCylIdx])) {
                    bestCylIdx = i;
                }
            }
        }

        finalCyl = cyl_list[bestCylIdx];
        finalAx  = ax_list[bestCylIdx]; // 轴位随柱镜
        refRecord = &measurements[bestCylIdx]; // 其他参数随柱镜
    }

    // 写入最终结果
    final_result.setRef(finalSph, finalCyl, finalAx, eye);
    if (refRecord) {
        final_result.copyOtherParamsFrom(*refRecord, eye);
    }
}

// =========================================================================
//  辅助函数实现
// =========================================================================

bool RefractionStrategy::isResultQuestionable(double sph, double cyl) {
    // 流程图：球镜<-7.0D 或者 球镜>+4.5D 并且 柱镜绝对值>=2D
    // 注意：C++中 abs() 对浮点数需用 std::abs
    return ((sph < -7.0 || sph > 4.5) && std::abs(cyl) >= 2.0);
}

double RefractionStrategy::selectBestValue(const std::vector<double>& values, double average) {
    if (values.empty()) return 0.0;

    double bestVal = values[0];
    double bestDist = std::abs(values[0] - average);

    for (size_t i = 1; i < values.size(); ++i) {
        double dist = std::abs(values[i] - average);

        // 逻辑：取最接近平均值
        if (dist < bestDist - 0.001) {
            bestVal = values[i];
            bestDist = dist;
        }
        // 并且绝对值较小的值 (当距离相当时)
        else if (std::abs(dist - bestDist) <= 0.001) {
            if (std::abs(values[i]) < std::abs(bestVal)) {
                bestVal = values[i];
            }
        }
    }
    return bestVal;
}

double RefractionStrategy::calculateRange(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    auto mm = std::minmax_element(values.begin(), values.end());
    return *mm.second - *mm.first;
}

double RefractionStrategy::calculateAverage(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / values.size();
}

int RefractionStrategy::calculateAverageAxis(const std::vector<int>& axes) {
    if (axes.empty()) return 0;

    double sinSum = 0.0;
    double cosSum = 0.0;
    const double PI = 3.14159265358979323846;

    for (int ax : axes) {
        // 轴位是0-180度，具有周期性，转成弧度计算再平均
        double rad = ax * 2 * PI / 180.0;
        sinSum += std::sin(rad);
        cosSum += std::cos(rad);
    }

    double avgRad = std::atan2(sinSum, cosSum);
    if (avgRad < 0) avgRad += 2 * PI;

    int avgAx = static_cast<int>(round(avgRad * 180.0 / (2 * PI)));
    return avgAx % 180; // 确保在0-179
}

// ---------------------------------------------------------
//  稳定性检查相关 (保持原有迭代逻辑，但适配接口)
// ---------------------------------------------------------
StabilityResult RefractionStrategy::checkStabilityIterative(const std::vector<stVisionValue>& measurements,
                                                            int stableCount) {
    StabilityResult result;
    size_t n = measurements.size();
    if (n < (size_t)stableCount) return result;

    // 简单组合生成：这里仅实现针对 stableCount=2 的优化，
    // 如果 stableCount > 2，建议使用递归或通用的 NextPermutation 算法。
    // 流程图通常暗示 stableCount 为 2 或 3。

    // 生成所有大小为 stableCount 的组合索引
    std::vector<int> p(stableCount);
    std::iota(p.begin(), p.end(), 0); // 0, 1, 2...

    while (true) {
        // 检查当前组合 p 是否稳定
        if (isCombinationStable(measurements, p)) {
            result.isStable = true;
            result.stableIndices = p;
            for (int idx : p) {
                result.stableSubset.push_back(measurements[idx]);
            }
            return result; // 找到第一组即返回
        }

        // 生成下一个组合
        int i = stableCount - 1;
        while (i >= 0 && p[i] == (int)n - stableCount + i) i--;
        if (i < 0) break;
        p[i]++;
        for (int j = i + 1; j < stableCount; j++) p[j] = p[j - 1] + 1;
    }

    return result;
}

bool RefractionStrategy::isCombinationStable(const std::vector<stVisionValue>& measurements,
                                             const std::vector<int>& combination) {
    // 检查右眼
    std::vector<double> rSph, rCyl, lSph, lCyl;
    for (int idx : combination) {
        rSph.push_back(measurements[idx].RSph);
        rCyl.push_back(measurements[idx].RCyl);
        lSph.push_back(measurements[idx].LSph);
        lCyl.push_back(measurements[idx].LCyl);
    }

    // 左右眼必须都稳定 (极差 <= rangeThreshold)
    if (calculateRange(rSph) > rangeThreshold) return false;
    if (calculateRange(rCyl) > rangeThreshold) return false;
    if (calculateRange(lSph) > rangeThreshold) return false;
    if (calculateRange(lCyl) > rangeThreshold) return false;

    return true;
}



// ==========================================
// 测试辅助工具
// ==========================================

// 辅助构建视力数据对象，支持左右眼不同数据
stVisionValue makeVal(double rs, double rc, int ra,
                     double ls, double lc, int la,
                     int pd = 60, double ps = 4.0, bool ptosis = false) {
    stVisionValue v = {};
    // 右眼参数
    v.RSph = rs; v.RCyl = rc; v.RAx = ra; v.RPs = ps; v.RPtosis = ptosis;
    // 左眼参数
    v.LSph = ls; v.LCyl = lc; v.LAx = la; v.LPs = ps; v.LPtosis = ptosis;
    // 瞳距
    v.PD = pd;
    // RHz/RVz/LHz/LVz 默认 0
    return v;
}

// 辅助断言：浮点数近似相等
bool isNear(double a, double b, double epsilon = 0.001) {
    return std::abs(a - b) < epsilon;
}

// 辅助打印：测试结果
void printTestResult(const string& caseName, bool passed, string msg = "") {
    cout << "[" << (passed ? "PASS" : "FAIL") << "] " << caseName;
    if (!passed && !msg.empty()) cout << " -> " << msg;
    cout << endl;
}

// ==========================================
// 运行所有测试用例
// ==========================================
void runTestCases() {
    cout << fixed << setprecision(3); // 浮点数保留3位小数
    cout << "========================================" << endl;
    cout << "   RefractionStrategy 双眼综合测试" << endl;
    cout << "========================================" << endl;

    // --- 全局配置初始化 ---
    RefractionStrategy::setStableCount(2);
    RefractionStrategy::setMaxRecordCount(3);
    RefractionStrategy::setRangeThreshold(0.5);

    // 变量池
    stVisionValue result = {};
    stVisionAbnormal abnormal = {};
    bool questionable = false;
    bool finished = false;

    // ---------------------------------------------------------
    // Case 1: R/L 均稳定 (Standard Stable & Averaging)
    // ---------------------------------------------------------
    {
        vector<stVisionValue> dataset;
        // R: 稳定且需要平均
        // L: 稳定，且非屈光参数不同（Ps）
        dataset.push_back(makeVal(-3.00, -1.00, 10,  +1.00, -0.50, 80, 62, 4.0, false));
        dataset.push_back(makeVal(-3.25, -1.25, 20,  +1.10, -0.60, 90, 64, 4.2, true)); // 差异 <= 0.5

        finished = RefractionStrategy::executeAlgoPolicy(dataset, result, abnormal, questionable);

        // R 预期: Sph: -3.125, Cyl: -1.125, Ax: 15
        bool rCheck = isNear(result.RSph, -3.125) && isNear(result.RCyl, -1.125) && result.RAx == 15;
        // L 预期: Sph: +1.050, Cyl: -0.550, Ax: 85
        bool lCheck = isNear(result.LSph, 1.050) && isNear(result.LCyl, -0.550) && result.LAx == 85;
        // PD 预期: (62+64)/2 = 63
        bool pdCheck = result.PD == 63;
        // LPs 预期: 继承第一个记录的非屈光参数（代码中继承的是第一个记录）
        bool otherCheck = isNear(result.LPs, 4.0) && result.LPtosis == false;

        printTestResult("Case 1: R/L 均稳定 (平均值计算)",
                        finished == true && rCheck && lCheck && pdCheck && otherCheck && !questionable,
                        "R/L/PD/Other Check Failed");
    }

    // ---------------------------------------------------------
    // Case 2: R 稳定 + L 差异大/不稳定 (Mixed Stability & Forced Settlement)
    // ---------------------------------------------------------
    {
        vector<stVisionValue> dataset;
        // R: 稳定
        // L: 极差大 (2.0D)，且已达到 maxRecordCount=3，强制结算
        dataset.push_back(makeVal(-1.00, -0.50, 90,  -1.00, -1.00, 10, 60));
        dataset.push_back(makeVal(-1.10, -0.50, 95,  -3.00, -2.00, 20, 61));
        dataset.push_back(makeVal(-1.20, -0.60, 100, -5.00, -3.00, 30, 62));

        finished = RefractionStrategy::executeAlgoPolicy(dataset, result, abnormal, questionable);

        // R 预期: 稳定，取平均
        // RSph: (-1.0-1.1-1.2)/3 = -1.100
        // RCyl: (-0.5-0.5-0.6)/3 = -0.533
        // RAx: 95
        bool rCheck = isNear(result.RSph, -1.100) && isNear(result.RCyl, -0.533) && result.RAx == 95;

        // L 预期: 不稳定，取最接近平均值且绝对值较小的
        // LSph Mean = -3.00. -1.0/-3.0/-5.0. 选中 -3.00 (距离0)
        // LCyl Mean = -2.00. -1.0/-2.0/-3.0. 选中 -2.00 (距离0)
        // LAx: 跟随选中 Cyl 的轴位 -> 20 (来自第2组)
        bool lCheck = isNear(result.LSph, -3.00) && isNear(result.LCyl, -2.00) && result.LAx == 20;

        // PD 预期: (60+61+62)/3 = 61
        bool pdCheck = result.PD == 61;

        printTestResult("Case 2: R稳定+L不稳定 (强制结算)",
                        finished == true && rCheck && lCheck && pdCheck && !questionable,
                        "R/L计算策略混合失败");
    }

    // ---------------------------------------------------------
    // Case 3: R 存疑 + L 正常 (Questionable Flag)
    // ---------------------------------------------------------
    {
        vector<stVisionValue> dataset;
        // R: 存疑条件 (Sph < -7.0 && Abs(Cyl) >= 2.0)
        // L: 正常
        dataset.push_back(makeVal(-7.50, -2.00, 180,  +0.50, -0.25, 90, 60)); // R 存疑
        dataset.push_back(makeVal(-7.70, -2.10, 180,  +0.60, -0.25, 95, 60)); // R 存疑

        finished = RefractionStrategy::executeAlgoPolicy(dataset, result, abnormal, questionable);

        // 预期：只要有一眼存疑，整体结果即存疑
        bool rSphCheck = (result.RSph < -7.0 && std::abs(result.RCyl) >= 2.0);

        printTestResult("Case 3: R 存疑 + L 正常 (混合存疑标记)",
                        finished == true && questionable == true && rSphCheck,
                        "Questionable 标记缺失或R眼结果计算错误");
    }

    // ---------------------------------------------------------
    // Case 4: L 仅一组数据 (Single Record Strategy)
    // ---------------------------------------------------------
    {
        vector<stVisionValue> dataset;
        // 只有一组数据时，应直接使用该组数据作为最终结果
        dataset.push_back(makeVal(-2.00, -1.00, 90,  -4.50, -2.50, 180, 60, 4.5, true));

        // Note: stableCount=2，这里会返回 false
        finished = RefractionStrategy::executeAlgoPolicy(dataset, result, abnormal, questionable);

        // 模拟外部跳过 executeAlgoPolicy (因返回false)，直接调用 calculate (如果外部逻辑允许)
        // 但根据流程图，只有当数据量达到 stableCount 或 maxRecordCount 才会进入 calculate 策略。
        // Case 1 已经覆盖了数据不足的情况，这里我们模拟达到稳定条件后，单组数据计算本身的功能。
        // --- 重设配置以强制结算 ---
        RefractionStrategy::setStableCount(1);

        finished = RefractionStrategy::executeAlgoPolicy(dataset, result, abnormal, questionable);

        // R 预期: -2.00, -1.00, 90
        bool rCheck = isNear(result.RSph, -2.00) && isNear(result.RCyl, -1.00) && result.RAx == 90;
        // L 预期: -4.50, -2.50, 180
        bool lCheck = isNear(result.LSph, -4.50) && isNear(result.LCyl, -2.50) && result.LAx == 180;
        // LPs/LPtosis 预期: 4.5 / true
        bool otherCheck = isNear(result.LPs, 4.5) && result.LPtosis == true;
        // 存疑检查：L眼 (-4.5 < -7.0) is False; Abs(-2.5) >= 2.0 is True. (不满足双重条件) -> isQuestionable = false

        printTestResult("Case 4: 单组数据直接使用 (含非屈光参数)",
                        finished == true && rCheck && lCheck && otherCheck && !questionable,
                        "单组数据计算失败或非屈光参数缺失");

        // --- 还原配置 ---
        RefractionStrategy::setStableCount(2);
    }

    // ---------------------------------------------------------
    // Case 5: 轴位平均 (Vector Averaging)
    // ---------------------------------------------------------
    {
        vector<stVisionValue> dataset;
        // R轴位: 179 + 1 -> 0 / 180
        // L轴位: 85 + 95 -> 90
        dataset.push_back(makeVal(-2.0, -1.0, 179,  -2.0, -1.0, 85, 60));
        dataset.push_back(makeVal(-2.0, -1.0, 1,    -2.0, -1.0, 95, 60));

        finished = RefractionStrategy::executeAlgoPolicy(dataset, result, abnormal, questionable);

        // R 轴位检查: 0 或 180
        bool rAxCheck = (result.RAx == 0 || result.RAx == 180);
        // L 轴位检查: (85+95)/2 = 90
        bool lAxCheck = result.LAx == 90;

        printTestResult("Case 5: R/L 轴位循环/算术平均",
                        finished == true && rAxCheck && lAxCheck,
                        "R/L 轴位平均计算错误");
    }

    cout << "\n========================================" << endl;
}
