#pragma once

#include <string>
#include <sstream>
#include <cmath>
#include <vector>
#include <bitset>
#include <QDebug>

/**
 * @brief RANSAC算法参数配置结构体
 *
 * 提供多种预设配置：快速、标准、精确、生产环境
 */
struct RANSACParams {
    // 基础参数
    int maxIterations;          ///< 最大迭代次数
    double distanceThreshold;   ///< 距离阈值，判断内点
    int minSamples;             ///< 每次随机采样最少点数
    double minInlierRatio;      ///< 最小内点比例
    double confidence;          ///< 置信度
    int randomSeed;             ///< 随机种子：-1=真随机，≥0=固定种子

    // 高级参数（可选）
    int averageRuns;            ///< 平均运行次数，>1时取平均值
    double maxAllowedVariance;  ///< 最大允许方差（稳定性检查）
    bool enableVerification;    ///< 是否启用结果验证

    /**
     * @brief 默认构造函数 - 使用标准配置
     */
    RANSACParams()
        : maxIterations(1000), distanceThreshold(0.5), minSamples(2),
          minInlierRatio(0.3), confidence(0.99), randomSeed(-1),
          averageRuns(1), maxAllowedVariance(0.01), enableVerification(false) {}

    /**
     * @brief 快速配置 - 用于实时处理或调试
     * @details 迭代次数少，速度最快，精度较低
     */
    static RANSACParams Fast() {
        RANSACParams params;
        params.maxIterations = 500;
        params.distanceThreshold = 0.8;    // 宽松阈值
        params.minInlierRatio = 0.2;       // 低内点比例要求
        params.confidence = 0.95;          // 较低置信度
        params.randomSeed = -1;           // 真随机
        params.averageRuns = 1;
        params.maxAllowedVariance = 0.05;  // 允许较大方差
        params.enableVerification = false;
        return params;
    }

    /**
     * @brief 标准配置 - 平衡精度和速度
     * @details 适用于大多数应用场景
     */
    static RANSACParams Standard() {
        RANSACParams params;
        params.maxIterations = 1000;
        params.distanceThreshold = 0.5;    // 适中阈值
        params.minInlierRatio = 0.3;        // 标准内点比例
        params.confidence = 0.99;          // 高置信度
        params.randomSeed = -1;
        params.averageRuns = 1;
        params.maxAllowedVariance = 0.01;
        params.enableVerification = false;
        return params;
    }

    /**
     * @brief 精确配置 - 高精度要求场景
     * @details 迭代次数多，速度较慢，精度最高
     */
    static RANSACParams Precise() {
        RANSACParams params;
        params.maxIterations = 5000;
        params.distanceThreshold = 0.2;     // 严格阈值
        params.minInlierRatio = 0.5;        // 高内点比例要求
        params.confidence = 0.999;          // 极高置信度
        params.randomSeed = -1;
        params.averageRuns = 1;
        params.maxAllowedVariance = 0.005;  // 严格方差限制
        params.enableVerification = true;
        return params;
    }

    /**
     * @brief 生产环境配置 - 稳定可靠，适合工业应用
     * @details 固定随机种子确保可重复性，适中的精度和速度平衡
     */
    static RANSACParams Production() {
        RANSACParams params;
        params.maxIterations = 3000;        // 充分迭代
        params.distanceThreshold = 0.3;     // 适中严格阈值
        params.minInlierRatio = 0.4;        // 合理内点比例
        params.confidence = 0.995;          // 高置信度
        params.randomSeed = 42;             // 固定种子，确保可重复性
        params.averageRuns = 3;             // 3次平均提高稳定性
        params.maxAllowedVariance = 0.001;  // 严格方差控制
        params.enableVerification = true;   // 启用结果验证
        return params;
    }

    /**
     * @brief 超稳定配置 - 用于关键任务，不计时间成本
     * @details 最高稳定性要求，运行时间较长
     */
    static RANSACParams UltraStable() {
        RANSACParams params;
        params.maxIterations = 10000;
        params.distanceThreshold = 0.15;    // 非常严格阈值
        params.minInlierRatio = 0.6;        // 高内点比例
        params.confidence = 0.9999;         // 极高置信度
        params.randomSeed = 12345;          // 固定种子
        params.averageRuns = 5;             // 5次平均
        params.maxAllowedVariance = 0.0001;  // 极严格方差控制
        params.enableVerification = true;
        return params;
    }

    /**
     * @brief 自定义配置 - 手动设置所有参数
     */
    static RANSACParams Custom(int iterations, double distThreshold,
                              double inlierRatio, double conf, int seed = -1,
                              int avgRuns = 1, double maxVariance = 0.01,
                              bool verify = false) {
        RANSACParams params;
        params.maxIterations = iterations;
        params.distanceThreshold = distThreshold;
        params.minInlierRatio = inlierRatio;
        params.confidence = conf;
        params.randomSeed = seed;
        params.averageRuns = avgRuns;
        params.maxAllowedVariance = maxVariance;
        params.enableVerification = verify;
        return params;
    }

    /**
     * @brief 验证参数有效性
     * @return 参数有效返回true，否则返回false
     */
    bool isValid() const {
        return maxIterations > 0 &&
               distanceThreshold > 0 &&
               minSamples >= 2 &&
               minInlierRatio > 0 && minInlierRatio <= 1 &&
               confidence > 0 && confidence <= 1 &&
               averageRuns > 0 &&
               maxAllowedVariance >= 0;
    }

    /**
     * @brief 获取参数描述信息（用于日志记录）
     */
    std::string toString() const {
        std::stringstream ss;
        ss << "RANSACParams{"
           << "iterations=" << maxIterations
           << ", threshold=" << distanceThreshold
           << ", minInlierRatio=" << minInlierRatio
           << ", confidence=" << confidence
           << ", seed=" << (randomSeed >= 0 ? std::to_string(randomSeed) : "random")
           << ", avgRuns=" << averageRuns
           << ", maxVariance=" << maxAllowedVariance
           << ", verification=" << (enableVerification ? "on" : "off")
           << "}";
        return ss.str();
    }
};

const RANSACParams DEFAULT_PARAMS=RANSACParams::Production();


// RANSAC直线模型
struct LineModel {
    double slope;     // 斜率
    double intercept; // 截距

    LineModel() : slope(0), intercept(0) {}
    LineModel(double k, double b) : slope(k), intercept(b) {}

    // 计算点到直线的距离
    double distance(double x, double y) const {
        // 直线方程: y = slope*x + intercept
        // 点到直线距离: |slope*x - y + intercept| / sqrt(slope^2 + 1)
        return std::abs(slope * x - y + intercept) / std::sqrt(slope * slope + 1);
    }

    // 预测y值
    double predict(double x) const {
        return slope * x + intercept;
    }
};

// 使用最小二乘法拟合直线（用于RANSAC的内点重新拟合）
LineModel fitLineLSQ(const std::vector<double>& xData, const std::vector<double>& yData);

// RANSAC核心算法
LineModel fitLineRANSAC(const std::vector<double>& xData, const std::vector<double>& yData,
                       const RANSACParams& params, std::vector<bool>& inlierMask);

// 从两个点计算直线模型
LineModel computeModelFromPoints(double x1, double y1, double x2, double y2);

double calculateResidualVariance(const std::vector<double>& xData,
                               const std::vector<double>& yData,
                               const LineModel& model,
                               const std::vector<bool>& inlierMask);
double calculateMeanResidual(const std::vector<double>& xData,
                           const std::vector<double>& yData,
                           const LineModel& model,
                           const std::vector<bool>& inlierMask);

double calculateRSquared(const std::vector<double>& xData,
                       const std::vector<double>& yData,
                       const LineModel& model,
                       const std::vector<bool>& inlierMask);
