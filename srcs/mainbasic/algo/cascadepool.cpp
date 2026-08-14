#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <string>
#include <atomic>
#include <condition_variable>
#include <algorithm>

// 其他必要的OpenCV和Qt头文件
#include <opencv2/objdetect.hpp>
#include <QThread>
#include <QDebug>
#include "perftimer.h"

/**
 * @brief 健壮的眼部级联分类器对象池管理类
 *
 * 支持等待可用对象和动态扩容，永不返回空指针
 */
class RobustEyeCascadePool {
private: // 私有成员变量
    std::mutex poolMutex;                                      ///< 保护对象池操作的互斥锁
    std::condition_variable poolCondition;                     ///< 对象可用条件变量
    std::vector<std::unique_ptr<cv::CascadeClassifier>> availablePool; ///< 可用实例池
    std::unordered_map<cv::CascadeClassifier*, bool> inUseMap; ///< 使用中实例映射表

    // 配置参数
    std::atomic<int> maxPoolSize;                             ///< 最大池大小（可动态调整）
    std::atomic<int> currentTotalInstances;                   ///< 当前总实例数
    std::atomic<bool> shutdownFlag{false};                    ///< 关闭标志

    const int INITIAL_POOL_SIZE = 10;                         ///< 初始池大小
    const int MAX_WAIT_TIME_MS = 5000;                       ///< 最大等待时间（毫秒）
    const int EXPANSION_FACTOR = 2;                          ///< 扩容因子

public: // 公有接口
    static RobustEyeCascadePool& getInstance() {
        static RobustEyeCascadePool instance;
        return instance;
    }

    /**
     * @brief 获取级联分类器实例（阻塞等待，永不返回空指针）
     * @param timeoutMs 最大等待时间（毫秒），-1表示无限等待
     * @return std::shared_ptr<cv::CascadeClassifier> 实例指针
     *
     * 如果池为空且未达上限，则创建新实例；如果已达上限，则等待可用实例
     */
    std::shared_ptr<cv::CascadeClassifier> getCascade(int timeoutMs = -1) {
        qDebug() << QString::fromStdString(getStats().toString()) ;
        auto start = std::chrono::high_resolution_clock::now();

        std::unique_lock<std::mutex> lock(poolMutex);

        // 策略1：直接获取可用实例
        if (!availablePool.empty()) {
            return takeFromAvailablePool(lock, start);
        }

        // 策略2：创建新实例（如果未达上限）
        if (currentTotalInstances < maxPoolSize) {
            return createNewInstance(lock, start);
        }

        // 策略3：等待可用实例或超时扩容
        return waitForAvailableOrExpand(lock, start, timeoutMs);
    }

    /**
     * @brief 预热对象池，预先创建大量实例
     * @param preWarmCount 预热实例数量
     * @return bool 预热是否成功
     */
    bool preWarmPool(int preWarmCount) {
        if (preWarmCount <= 0) {
            qWarning() << "无效的预热数量:" << preWarmCount;
            return false;
        }

        std::unique_lock<std::mutex> lock(poolMutex);

        // 调整最大池大小以适应预热需求
        int currentMaxSize = maxPoolSize.load();
        if (preWarmCount > currentMaxSize) {
            maxPoolSize = preWarmCount;
            ALGO_DEBUG_LOG(qInfo() << "调整最大池大小到:" << preWarmCount);
        }

        ALGO_DEBUG_LOG(qInfo() << "开始预热级联分类器对象池，数量:" << preWarmCount << "个实例...");

        int successCount = 0;
        auto totalStart = std::chrono::high_resolution_clock::now();

        int currentTotal = currentTotalInstances.load();
        int currentMax = maxPoolSize.load();

        for (int i = 0; i < preWarmCount && currentTotal < currentMax; ++i) {
            auto instanceStart = std::chrono::high_resolution_clock::now();

            std::unique_ptr<cv::CascadeClassifier> cascade = createCascadeInstance();
            if (cascade) {
                availablePool.push_back(std::move(cascade));
                currentTotalInstances++;
                successCount++;
                currentTotal++;

                auto instanceEnd = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(instanceEnd - instanceStart);
                ALGO_DEBUG_LOG(qInfo() << "预热实例" << i + 1 << "创建成功，耗时:" << duration.count() << "毫秒");
            } else {
                qWarning() << "预热实例" << i + 1 << "创建失败";
            }
        }

        auto totalEnd = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(totalEnd - totalStart);

        ALGO_DEBUG_LOG(
            qInfo() << "对象池预热完成: 成功创建" << successCount << "/"
                    << preWarmCount << "个实例，总耗时:" << totalDuration.count()
                    << "毫秒"
        );

        // 通知所有等待的线程
        poolCondition.notify_all();

        return successCount > 0;
    }

    /**
     * @brief 动态调整对象池大小
     * @param newSize 新的池大小
     * @return bool 调整是否成功
     */
    bool resizePool(int newSize) {
        if (newSize <= 0) {
            qWarning() << "无效的池大小:" << newSize;
            return false;
        }

        std::unique_lock<std::mutex> lock(poolMutex);

        int currentTotal = currentTotalInstances.load();
        if (newSize < currentTotal) {
            qWarning() << "不能缩小池大小到" << newSize
                      << "，当前已有" << currentTotal << "个实例";
            return false;
        }

        int oldSize = maxPoolSize.load();
        maxPoolSize = newSize;

        ALGO_DEBUG_LOG(qInfo() << "对象池大小从" << oldSize << "调整到" << newSize);

        // 通知等待的线程，可能有新的扩容机会
        poolCondition.notify_all();

        return true;
    }

    /**
     * @brief 紧急扩容：立即创建指定数量的新实例
     * @param count 要扩容的数量
     * @return int 实际扩容的数量
     */
    int emergencyExpand(int count) {
        if (count <= 0) return 0;

        std::unique_lock<std::mutex> lock(poolMutex);

        int actuallyExpanded = 0;
        int currentTotal = currentTotalInstances.load();
        int currentMax = maxPoolSize.load();

        // 修正：将原子变量转换为普通int再使用std::min
        int targetCount = std::min(currentTotal + count, currentMax);

        qWarning() << "执行紧急扩容，目标数量:" << targetCount;

        while (currentTotal < targetCount) {
            std::unique_ptr<cv::CascadeClassifier> cascade = createCascadeInstance();
            if (cascade) {
                availablePool.push_back(std::move(cascade));
                currentTotalInstances++;
                currentTotal++;
                actuallyExpanded++;
            } else {
                ALGO_ERROR_LOG(qCritical() << "紧急扩容时创建实例失败");
                break;
            }
        }

        if (actuallyExpanded > 0) {
            ALGO_DEBUG_LOG(qInfo() << "紧急扩容完成，新增" << actuallyExpanded << "个实例");
            poolCondition.notify_all(); // 通知所有等待线程
        }

        return actuallyExpanded;
    }

    /**
     * @brief 获取对象池统计信息
     */
    struct PoolStats {
        int availableCount;    ///< 可用实例数量
        int inUseCount;        ///< 使用中实例数量
        int totalCount;        ///< 实例总数
        int maxPoolSize;       ///< 最大池大小
        bool isHealthy;        ///< 是否健康
        // 成员函数 toString
        std::string toString() const {
            std::ostringstream oss;
            oss << "PoolStats(availableCount=" << availableCount
                << ", inUseCount=" << inUseCount
                << ", totalCount=" << totalCount
                << ", maxPoolSize=" << maxPoolSize
                << ", isHealthy=" << (isHealthy ? "true" : "false")
                << ")";
            return oss.str();
        }
    };

    PoolStats getStats() {
        std::unique_lock<std::mutex> lock(poolMutex);
        PoolStats stats;
        stats.availableCount = availablePool.size();
        stats.inUseCount = inUseMap.size();
        stats.totalCount = currentTotalInstances.load();
        stats.maxPoolSize = maxPoolSize.load();
        stats.isHealthy = (stats.totalCount <= stats.maxPoolSize);
        return stats;
    }

    /**
     * @brief 关闭对象池，拒绝新请求
     */
    void shutdown() {
        std::unique_lock<std::mutex> lock(poolMutex);
        shutdownFlag = true;
        poolCondition.notify_all(); // 唤醒所有等待线程
        ALGO_DEBUG_LOG(qInfo() << "对象池已关闭");
    }

private: // 私有方法
    /**
     * @brief 从可用池中获取实例
     */
    std::shared_ptr<cv::CascadeClassifier> takeFromAvailablePool(
        std::unique_lock<std::mutex>& lock,
        std::chrono::high_resolution_clock::time_point start) {

        std::unique_ptr<cv::CascadeClassifier> cascade(std::move(availablePool.back()));
        availablePool.pop_back();
        inUseMap[cascade.get()] = true;

        logAcquisitionTime("从可用池获取", start);
        return wrapWithDeleter(std::move(cascade));
    }

    /**
     * @brief 创建新实例
     */
    std::shared_ptr<cv::CascadeClassifier> createNewInstance(
        std::unique_lock<std::mutex>& lock,
        std::chrono::high_resolution_clock::time_point start) {

        std::unique_ptr<cv::CascadeClassifier> cascade = createCascadeInstance();
        if (cascade) {
            inUseMap[cascade.get()] = true;
            currentTotalInstances++;

            logAcquisitionTime("创建新实例", start);
            return wrapWithDeleter(std::move(cascade));
        }

        // 创建失败，进入等待策略
        return waitForAvailableOrExpand(lock, start, MAX_WAIT_TIME_MS);
    }

    /**
     * @brief 等待可用实例或超时扩容
     */
    std::shared_ptr<cv::CascadeClassifier> waitForAvailableOrExpand(
        std::unique_lock<std::mutex>& lock,
        std::chrono::high_resolution_clock::time_point start,
        int timeoutMs) {

        ALGO_DEBUG_LOG(qInfo() << "对象池繁忙，等待可用实例...");

        auto waitStart = std::chrono::steady_clock::now();
        bool waitSuccess = false;

        if (timeoutMs == -1) {
            // 无限等待
            poolCondition.wait(lock, [this]() {
                return !availablePool.empty() || shutdownFlag;
            });
            waitSuccess = !shutdownFlag && !availablePool.empty();
        } else {
            // 超时等待
            waitSuccess = poolCondition.wait_for(lock,
                std::chrono::milliseconds(timeoutMs), [this]() {
                    return !availablePool.empty() || shutdownFlag;
                });
        }

        if (shutdownFlag) {
            throw std::runtime_error("对象池已关闭，无法获取实例");
        }

        if (waitSuccess && !availablePool.empty()) {
            // 等待成功，获取实例
            return takeFromAvailablePool(lock, start);
        }

        // 等待超时，尝试紧急扩容
        qWarning() << "等待超时，尝试紧急扩容...";
        int expanded = emergencyExpand(EXPANSION_FACTOR);

        if (expanded > 0 && !availablePool.empty()) {
            return takeFromAvailablePool(lock, start);
        }

        // 最终策略：创建独立的fallback实例
        ALGO_ERROR_LOG(qCritical() << "对象池耗尽且扩容失败，创建独立实例");
        return createFallbackInstance(start);
    }

    /**
     * @brief 创建独立的fallback实例（永不返回空指针）
     */
    std::shared_ptr<cv::CascadeClassifier> createFallbackInstance(
        std::chrono::high_resolution_clock::time_point start) {

        std::unique_ptr<cv::CascadeClassifier> cascade = createCascadeInstance();
        if (!cascade) {
            // 终极fallback：抛出异常
            throw std::runtime_error("无法创建级联分类器实例，所有方法均失败");
        }

        logAcquisitionTime("创建独立实例", start);

        // fallback实例不使用对象池，直接删除
        return std::shared_ptr<cv::CascadeClassifier>(cascade.release(),
            [](cv::CascadeClassifier* ptr) { delete ptr; });
    }

    /**
     * @brief 包装实例为shared_ptr并设置删除器
     */
    std::shared_ptr<cv::CascadeClassifier> wrapWithDeleter(
        std::unique_ptr<cv::CascadeClassifier> cascade) {

        return std::shared_ptr<cv::CascadeClassifier>(cascade.release(),
            [this](cv::CascadeClassifier* ptr) { this->returnCascade(ptr); });
    }

    /**
     * @brief 记录获取时间
     */
    void logAcquisitionTime(const std::string& method,
                           std::chrono::high_resolution_clock::time_point start) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << method << "耗时: " << duration.count() << " 微秒" << std::endl;
    }

    /**
     * @brief 归还实例到对象池
     */
    void returnCascade(cv::CascadeClassifier* cascade) {
        std::unique_lock<std::mutex> lock(poolMutex);

        // 安全检查
        if (inUseMap.find(cascade) == inUseMap.end()) {
            qWarning() << "归还不属于对象池的实例，直接删除";
            delete cascade;
            return;
        }

        inUseMap.erase(cascade);

        // 如果池已关闭，直接删除实例
        if (shutdownFlag) {
            delete cascade;
            currentTotalInstances--;
            return;
        }

        // 正常归还
        availablePool.push_back(std::unique_ptr<cv::CascadeClassifier>(cascade));

        // 通知等待的线程
        poolCondition.notify_one();
    }

    /**
     * @brief 创建级联分类器实例
     */
    std::unique_ptr<cv::CascadeClassifier> createCascadeInstance() {
        std::unique_ptr<cv::CascadeClassifier> cascade(new cv::CascadeClassifier());

        std::vector<std::string> paths = getCascadePaths();
        for (const auto& path : paths) {
            if (cascade->load(path)) {
                return cascade;
            }
        }

        ALGO_ERROR_LOG(qCritical() << "无法从任何路径加载级联分类器文件");
        return std::unique_ptr<cv::CascadeClassifier>();
    }

    std::vector<std::string> getCascadePaths() {
        std::vector<std::string> paths;
#if (OS_TYPE == 3)
        paths.push_back("/usr/share/OpenCV/haarcascades/haarcascade_eye.xml");
#endif
#if (OS_TYPE == 2)
        paths.push_back("/opencv-2.4.10/data/haarcascades/haarcascade_eye.xml");
#endif
        paths.push_back("haarcascade_eye.xml");
        return paths;
    }

    // 构造函数私有化
    RobustEyeCascadePool() : maxPoolSize(INITIAL_POOL_SIZE), currentTotalInstances(0) {
        ALGO_DEBUG_LOG(qInfo() << "健壮对象池初始化完成，最大大小:" << INITIAL_POOL_SIZE);
    }

    // 禁用拷贝
    RobustEyeCascadePool(const RobustEyeCascadePool&) = delete;
    RobustEyeCascadePool& operator=(const RobustEyeCascadePool&) = delete;
};

// ============================================================================
// 全局接口函数 - 新接口（推荐使用）
// ============================================================================

/**
 * @brief 初始化对象池并预热大量实例
 */
bool initializeRobustEyeCascadePool(int preWarmCount = 20) {
    return RobustEyeCascadePool::getInstance().preWarmPool(preWarmCount);
}

/**
 * @brief 获取实例（永不返回空指针）- 新接口
 */
std::shared_ptr<cv::CascadeClassifier> getRobustEyeCascade(int timeoutMs = -1) {
    return RobustEyeCascadePool::getInstance().getCascade(timeoutMs);
}

/**
 * @brief 紧急扩容接口
 */
int emergencyExpandPool(int count) {
    return RobustEyeCascadePool::getInstance().emergencyExpand(count);
}

/**
 * @brief 获取统计信息
 */
RobustEyeCascadePool::PoolStats getRobustPoolStats() {
    return RobustEyeCascadePool::getInstance().getStats();
}

// ============================================================================
// 兼容旧接口 - 保持原有用法不变
// ============================================================================

/**
 * @brief 兼容旧接口：CascadeClassifier& getEyeCascade()
 *
 * 每个线程有自己独立的缓存实例，保持与旧代码完全兼容
 */
cv::CascadeClassifier& getEyeCascade() {
    // 使用thread_local保证每个线程有独立的缓存实例
    static thread_local std::shared_ptr<cv::CascadeClassifier> threadLocalCascade;

    // 如果当前线程还没有缓存实例，从对象池获取
    if (!threadLocalCascade) {
        threadLocalCascade = getRobustEyeCascade(); // 使用新的对象池

        if (!threadLocalCascade) {
            // 对象池获取失败，创建线程局部的fallback实例
            static thread_local cv::CascadeClassifier fallbackCascade;
            static thread_local bool fallbackLoaded = false;

            if (!fallbackLoaded) {
                // 加载fallback实例
                std::vector<std::string> paths = {
#if (OS_TYPE == 3)
                    "/usr/share/OpenCV/haarcascades/haarcascade_eye.xml",
#endif
#if (OS_TYPE == 2)
                    "/opencv-2.4.10/data/haarcascades/haarcascade_eye.xml",
#endif
                    "haarcascade_eye.xml"
                };

                for (const auto& path : paths) {
                    if (fallbackCascade.load(path)) {
                        fallbackLoaded = true;
                        ALGO_DEBUG_LOG(
                            qInfo() << "线程" << QThread::currentThread()
                                    << "创建fallback实例成功，路径:" << path.c_str()
                        );
                        break;
                    }
                }

                if (!fallbackLoaded) {
                    ALGO_ERROR_LOG(
                        qCritical() << "线程" << QThread::currentThread()
                                    << "无法创建fallback实例"
                    );
                    // 抛出异常或返回静态实例
                    static cv::CascadeClassifier globalFallback;
                    static bool globalLoaded = false;

                    if (!globalLoaded) {
                        for (const auto& path : paths) {
                            if (globalFallback.load(path)) {
                                globalLoaded = true;
                                break;
                            }
                        }
                    }
                    return globalFallback;
                }
            }
            return fallbackCascade;
        }
    }

    return *threadLocalCascade;
}

/**
 * @brief 清理当前线程的缓存实例
 *
 * 在线程退出或需要释放资源时调用
 */
void clearThreadLocalEyeCascade() {
    // thread_local变量会在线程退出时自动销毁
    // 此函数用于显式清理（如果需要）
    static thread_local std::shared_ptr<cv::CascadeClassifier> threadLocalCascade;
    threadLocalCascade.reset();
}

/**
 * @brief 旧接口的初始化函数（可选）
 */
bool initializeEyeCascade() {
    // 可以在这里初始化对象池，也可以不调用（懒加载）
    return initializeRobustEyeCascadePool(10); // 默认预热10个实例
}
