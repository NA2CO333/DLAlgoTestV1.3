#include "ransac.h"
#include <limits>
#include <random>
#include "logger.h"
#include "perftimer.h"

// 使用最小二乘法拟合直线
LineModel fitLineLSQ(const std::vector<double>& xData, const std::vector<double>& yData) {
    if (xData.size() < 2) {
        return LineModel();
    }

    double xSum = 0.0, ySum = 0.0, xySum = 0.0, xxSum = 0.0;
    size_t n = xData.size();

    for (size_t i = 0; i < n; ++i) {
        xSum += xData[i];
        ySum += yData[i];
        xySum += xData[i] * yData[i];
        xxSum += xData[i] * xData[i];
    }

    double denominator = n * xxSum - xSum * xSum;
    if (std::abs(denominator) < 1e-10) {
        // 垂直线或所有x相同，返回水平线
        return LineModel(0, ySum / n);
    }

    double slope = (n * xySum - xSum * ySum) / denominator;
    double intercept = (ySum - slope * xSum) / n;

    return LineModel(slope, intercept);
}

// 从两个点计算直线模型
LineModel computeModelFromPoints(double x1, double y1, double x2, double y2) {
    if (std::abs(x2 - x1) < 1e-10) {
        // 垂直线，返回一个大斜率
        return LineModel(1e10, 0);
    }

    double slope = (y2 - y1) / (x2 - x1);
    double intercept = y1 - slope * x1;

    return LineModel(slope, intercept);
}

// RANSAC核心算法
LineModel fitLineRANSAC(const std::vector<double>& xData,
                                    const std::vector<double>& yData,
                                    const RANSACParams& params,
                                    std::vector<bool>& inlierMask) {
    if (xData.size() < (size_t)params.minSamples) {
        qDebug() << "Insufficient data points for RANSAC, falling back to LSQ";
        return fitLineLSQ(xData, yData);
    }

    // ==================== 参数使用示例 ====================

    // 1. 设置随机种子（新参数）
    std::mt19937 rng;
    if (params.randomSeed >= 0) {
        rng = std::mt19937(params.randomSeed);  // 固定种子
        PERF_POINT(QString("Using fixed random seed:%1").arg(params.randomSeed));

    } else {
        std::random_device rd;
        rng = std::mt19937(rd());  // 真随机
        PERF_POINT("Using true random seed");
    }

    // 2. 使用参数中的距离阈值
    double distanceThreshold = params.distanceThreshold;

    LineModel bestModel;
    std::vector<bool> bestInlierMask(xData.size(), false);
    int bestInlierCount = 0;
    double bestVariance = std::numeric_limits<double>::max();

    int iterations = 0;
    int maxIterations = params.maxIterations;  // 使用参数中的迭代次数

    // 3. RANSAC主循环（使用所有参数）
    while (iterations < maxIterations) {
        // 随机采样（使用minSamples参数）
        std::vector<size_t> indices;
        while (indices.size() < (size_t)params.minSamples) {
            std::uniform_int_distribution<size_t> dist(0, xData.size() - 1);
            size_t idx = dist(rng);
            if (std::find(indices.begin(), indices.end(), idx) == indices.end()) {
                indices.push_back(idx);
            }
        }

        // 拟合模型（LineModel不变）
        LineModel model = computeModelFromPoints(
            xData[indices[0]], yData[indices[0]],
            xData[indices[1]], yData[indices[1]]
        );

        // 计算内点（使用distanceThreshold参数）
        std::vector<bool> currentInlierMask(xData.size(), false);
        int currentInlierCount = 0;

        for (size_t i = 0; i < xData.size(); ++i) {
            double dist = model.distance(xData[i], yData[i]);
            if (dist < distanceThreshold) {  // 使用参数阈值
                currentInlierMask[i] = true;
                currentInlierCount++;
            }
        }

        // 4. 使用minInlierRatio参数进行筛选
        double currentInlierRatio = static_cast<double>(currentInlierCount) / xData.size();
        if (currentInlierRatio < params.minInlierRatio) {
            iterations++;
            continue;  // 内点比例不足，跳过
        }

        // 5. 结果验证（新参数）
        bool isBetterModel = false;
        if (params.enableVerification) {
            double currentVariance = calculateResidualVariance(xData, yData, model, currentInlierMask);
            if (currentVariance < bestVariance) {
                bestVariance = currentVariance;
                isBetterModel = true;
            }
        } else {
            // 传统方式：比较内点数量
            isBetterModel = (currentInlierCount > bestInlierCount);
        }

        if (isBetterModel) {
            bestInlierCount = currentInlierCount;
            bestInlierMask = currentInlierMask;
            bestModel = model;

            // 6. 动态调整迭代次数（使用confidence参数）
            if (params.confidence > 0.9) {  // 高置信度时动态调整
                double logProb = std::log(1.0 - std::pow(currentInlierRatio, params.minSamples));
                if (logProb < 0) {
                    maxIterations = std::min(params.maxIterations,
                                           static_cast<int>(std::log(1.0 - params.confidence) / logProb));
                }
            }
        }

        iterations++;

        // 提前终止条件
        if (static_cast<double>(bestInlierCount) / xData.size() > 0.95) {
            PERF_POINT(QString("RANSAC early termination at iteration").arg(iterations));
            break;
        }
    }

    PERF_POINT(QString( "RANSAC completed:iterations %1,%2 inliers out of %3").arg(iterations).arg(bestInlierCount).arg(xData.size()));

    // 7. 用内点重新拟合最终模型（LineModel结构不变）
    std::vector<double> inlierX, inlierY;
    for (size_t i = 0; i < xData.size(); ++i) {
        if (bestInlierMask[i]) {
            inlierX.push_back(xData[i]);
            inlierY.push_back(yData[i]);
        }
    }

    inlierMask = bestInlierMask;

    if (inlierX.size() >= (size_t)params.minSamples) {
        LineModel refinedModel = fitLineLSQ(inlierX, inlierY);
        PERF_POINT(QString("Refined model: slope = %1 , intercept = %2").arg(refinedModel.slope).arg(refinedModel.intercept));

        return refinedModel;
    } else {
        qDebug() << "Using initial model due to insufficient inliers";
        return bestModel;
    }
}

/**
 * @brief 计算RANSAC模型残差方差
 *
 * 用于评估RANSAC拟合质量，方差越小表示拟合越好
 *
 * @param xData 输入数据的x坐标
 * @param yData 输入数据的y坐标
 * @param model 拟合的直线模型
 * @param inlierMask 内点掩码，标记哪些点是内点
 * @return double 残差方差值
 */
double calculateResidualVariance(const std::vector<double>& xData,
                               const std::vector<double>& yData,
                               const LineModel& model,
                               const std::vector<bool>& inlierMask) {
    // 检查输入有效性
    if (xData.size() != yData.size() || xData.size() != inlierMask.size()) {
        qDebug() << "Error: Input data size mismatch in calculateResidualVariance";
        return std::numeric_limits<double>::max();
    }

    // 统计内点数量和残差平方和
    int inlierCount = 0;
    double residualSumSquares = 0.0;

    for (size_t i = 0; i < xData.size(); ++i) {
        if (inlierMask[i]) {  // 只计算内点的残差
            // 计算预测值
            double predictedY = model.predict(xData[i]);

            // 计算残差（实际值 - 预测值）
            double residual = yData[i] - predictedY;

            // 累加残差平方
            residualSumSquares += residual * residual;
            inlierCount++;
        }
    }

    // 计算方差（需要至少2个内点）
    if (inlierCount < 2) {
        qDebug() << "Warning: Insufficient inliers (" << inlierCount
                 << ") for variance calculation";
        return std::numeric_limits<double>::max();
    }

    double variance = residualSumSquares / (inlierCount - 1);  // 无偏估计
    return variance;
}

/**
 * @brief 计算平均残差（可选）
 *
 * 提供另一种拟合质量评估指标
 */
double calculateMeanResidual(const std::vector<double>& xData,
                           const std::vector<double>& yData,
                           const LineModel& model,
                           const std::vector<bool>& inlierMask) {
    if (xData.size() != yData.size() || xData.empty()) {
        return std::numeric_limits<double>::max();
    }

    int inlierCount = 0;
    double residualSum = 0.0;

    for (size_t i = 0; i < xData.size(); ++i) {
        if (inlierMask[i]) {
            double predictedY = model.predict(xData[i]);
            double residual = std::abs(yData[i] - predictedY);
            residualSum += residual;
            inlierCount++;
        }
    }

    return (inlierCount > 0) ? residualSum / inlierCount : std::numeric_limits<double>::max();
}

/**
 * @brief 计算决定系数 R²（拟合优度）
 *
 * R²越接近1表示拟合效果越好
 */
double calculateRSquared(const std::vector<double>& xData,
                       const std::vector<double>& yData,
                       const LineModel& model,
                       const std::vector<bool>& inlierMask) {
    if (xData.size() < 2) return 0.0;

    // 计算内点的平均值
    double meanY = 0.0;
    int inlierCount = 0;

    for (size_t i = 0; i < yData.size(); ++i) {
        if (inlierMask[i]) {
            meanY += yData[i];
            inlierCount++;
        }
    }

    if (inlierCount < 2) return 0.0;
    meanY /= inlierCount;

    // 计算总平方和（SST）和残差平方和（SSR）
    double sst = 0.0;  // 总平方和
    double ssr = 0.0;  // 残差平方和

    for (size_t i = 0; i < xData.size(); ++i) {
        if (inlierMask[i]) {
            double predictedY = model.predict(xData[i]);
            sst += (yData[i] - meanY) * (yData[i] - meanY);
            ssr += (yData[i] - predictedY) * (yData[i] - predictedY);
        }
    }

    // 避免除零
    if (std::abs(sst) < 1e-10) return 1.0;

    double rSquared = 1.0 - (ssr / sst);
    return std::max(0.0, std::min(1.0, rSquared));  // 限制在[0,1]范围内
}
