#ifndef REFRACTION_STRATEGY_H
#define REFRACTION_STRATEGY_H

#include <vector>
#include "algointf.h"

// 稳定性检查结果结构体
struct StabilityResult {
    bool isStable; // 是否存在满足条件的子集
    std::vector<stVisionValue> stableSubset; // 满足条件的子集列表
    std::vector<int> stableIndices; // 满足条件的测量结果索引

    StabilityResult() : isStable(false) {}
};

class RefractionStrategy {
public:
    static int getStableCount() { return stableCount; }
    static void setStableCount(int count) { stableCount = count; }

    static int getMaxRecordCount() { return maxRecordCount; }
    static void setMaxRecordCount(int count) { maxRecordCount = count; }

    static double getRangeThreshold() { return rangeThreshold; }
    static void setRangeThreshold(double range) { rangeThreshold = range; }

    /**
     * @brief 执行算法策略（状态机入口）
     * 对应流程图中：流式拍摄 -> 计算结果 -> 判断是否结束
     */
    static bool executeAlgoPolicy(std::vector<stVisionValue> &_result_set,
                                  stVisionValue &_vision,
                                  stVisionAbnormal &_vision_abnormal,
                                  bool &_questionable);

    /**
     * @brief 核心计算逻辑（对应流程图下半部分的“结果策略”）
     */
    static stVisionValue calculate(const std::vector<stVisionValue>& measurements,
                                   bool& isQuestionable);

    /**
     * @brief 检查稳定性（寻找极差<=0.5的子集）
     */
    static StabilityResult checkStabilityIterative(const std::vector<stVisionValue>& measurements,
                                                   int stableCount);

    /**
     * @brief 检查单组结果是否存疑
     * 流程图条件：(球镜<-7.0D 或 >+4.5D) 且 柱镜绝对值>=2D
     */
    static bool isResultQuestionable(double sph, double cyl);

private:
    static int stableCount;
    static int maxRecordCount;
    static double rangeThreshold; // 极差阈值，默认 0.5D



    // 处理单眼逻辑
    static void processSingleEye(const std::vector<stVisionValue>& measurements,
                                 stVisionValue& final_result,
                                 enWhichEye eye,
                                 bool& isQuestionable);

    // 辅助：从一组数据中选取最接近平均值且绝对值较小的数
    static double selectBestValue(const std::vector<double>& values, double average);

    // 辅助：计算向量极差
    static double calculateRange(const std::vector<double>& values);

    // 辅助：计算平均值
    static double calculateAverage(const std::vector<double>& values);

    // 辅助：计算轴位平均值（处理循环）
    static int calculateAverageAxis(const std::vector<int>& axes);

    // 辅助：检查组合是否稳定
    static bool isCombinationStable(const std::vector<stVisionValue>& measurements,
                                    const std::vector<int>& combination);
};

void runTestCases();

#endif // REFRACTION_STRATEGY_H
