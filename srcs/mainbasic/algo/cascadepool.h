#ifndef ROBUSTEYECASCADEPOOL_H
#define ROBUSTEYECASCADEPOOL_H

#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <algorithm>

// OpenCV头文件
#include <opencv2/objdetect.hpp>

// Qt头文件（可选，如果使用Qt的日志系统）
#include <QString>
#include <QDebug>

/**
 * @brief 健壮的眼部级联分类器对象池管理类
 *
 * 支持等待可用对象和动态扩容，永不返回空指针
 * 同时保持与旧代码的完全兼容性
 */
class RobustEyeCascadePool {
private:
    std::mutex poolMutex_;                                      ///< 保护对象池操作的互斥锁
    std::condition_variable poolCondition_;                    ///< 对象可用条件变量
    std::vector<std::unique_ptr<cv::CascadeClassifier>> availablePool_; ///< 可用实例池
    std::unordered_map<cv::CascadeClassifier*, bool> inUseMap_; ///< 使用中实例映射表

    // 配置参数
    std::atomic<int> maxPoolSize_;                            ///< 最大池大小（可动态调整）
    std::atomic<int> currentTotalInstances_;                  ///< 当前总实例数
    std::atomic<bool> shutdownFlag_;                         ///< 关闭标志

    const int INITIAL_POOL_SIZE = 10;                         ///< 初始池大小
    const int MAX_WAIT_TIME_MS = 5000;                       ///< 最大等待时间（毫秒）
    const int EXPANSION_FACTOR = 2;                          ///< 扩容因子

public:
    /**
     * @brief 对象池统计信息结构体
     */
    struct PoolStats {
        int availableCount;    ///< 可用实例数量
        int inUseCount;        ///< 使用中实例数量
        int totalCount;        ///< 实例总数
        int maxPoolSize;       ///< 最大池大小
        bool isHealthy;        ///< 是否健康

        PoolStats() : availableCount(0), inUseCount(0), totalCount(0), maxPoolSize(0), isHealthy(false) {}
    };

    /**
     * @brief 获取对象池单例实例
     * @return RobustEyeCascadePool& 单例引用
     */
    static RobustEyeCascadePool& getInstance();

    /**
     * @brief 获取级联分类器实例（阻塞等待，永不返回空指针）
     * @param timeoutMs 最大等待时间（毫秒），-1表示无限等待
     * @return std::shared_ptr<cv::CascadeClassifier> 实例指针
     *
     * 如果池为空且未达上限，则创建新实例；如果已达上限，则等待可用实例
     */
    std::shared_ptr<cv::CascadeClassifier> getCascade(int timeoutMs = -1);

    /**
     * @brief 预热对象池，预先创建大量实例
     * @param preWarmCount 预热实例数量
     * @return bool 预热是否成功
     */
    bool preWarmPool(int preWarmCount);

    /**
     * @brief 动态调整对象池大小
     * @param newSize 新的池大小
     * @return bool 调整是否成功
     */
    bool resizePool(int newSize);

    /**
     * @brief 紧急扩容：立即创建指定数量的新实例
     * @param count 要扩容的数量
     * @return int 实际扩容的数量
     */
    int emergencyExpand(int count);

    /**
     * @brief 获取对象池统计信息
     * @return PoolStats 统计信息
     */
    PoolStats getStats();

    /**
     * @brief 关闭对象池，拒绝新请求
     */
    void shutdown();

    /**
     * @brief 检查对象池是否已关闭
     * @return bool 是否已关闭
     */
    bool isShutdown() const { return shutdownFlag_.load(); }

    /**
     * @brief 获取当前最大池大小
     * @return int 最大池大小
     */
    int getMaxPoolSize() const { return maxPoolSize_.load(); }

    /**
     * @brief 获取当前总实例数
     * @return int 总实例数
     */
    int getTotalInstances() const { return currentTotalInstances_.load(); }

private:
    // 私有构造函数和析构函数
    RobustEyeCascadePool();
    ~RobustEyeCascadePool() = default;

    // 禁用拷贝和赋值
    RobustEyeCascadePool(const RobustEyeCascadePool&) = delete;
    RobustEyeCascadePool& operator=(const RobustEyeCascadePool&) = delete;
    RobustEyeCascadePool(RobustEyeCascadePool&&) = delete;
    RobustEyeCascadePool& operator=(RobustEyeCascadePool&&) = delete;

    // 私有辅助方法
    std::shared_ptr<cv::CascadeClassifier> takeFromAvailablePool(
        std::unique_lock<std::mutex>& lock,
        std::chrono::high_resolution_clock::time_point start);

    std::shared_ptr<cv::CascadeClassifier> createNewInstance(
        std::unique_lock<std::mutex>& lock,
        std::chrono::high_resolution_clock::time_point start);

    std::shared_ptr<cv::CascadeClassifier> waitForAvailableOrExpand(
        std::unique_lock<std::mutex>& lock,
        std::chrono::high_resolution_clock::time_point start,
        int timeoutMs);

    std::shared_ptr<cv::CascadeClassifier> createFallbackInstance(
        std::chrono::high_resolution_clock::time_point start);

    std::shared_ptr<cv::CascadeClassifier> wrapWithDeleter(
        std::unique_ptr<cv::CascadeClassifier> cascade);

    void logAcquisitionTime(const std::string& method,
                           std::chrono::high_resolution_clock::time_point start);

    void returnCascade(cv::CascadeClassifier* cascade);

    std::unique_ptr<cv::CascadeClassifier> createCascadeInstance();

    std::vector<std::string> getCascadePaths();
};

// ============================================================================
// 全局接口函数声明
// ============================================================================

/**
 * @brief 初始化对象池并预热大量实例
 * @param preWarmCount 预热实例数量，默认20个
 * @return bool 初始化是否成功
 */
bool initializeRobustEyeCascadePool(int preWarmCount = 20);

/**
 * @brief 获取级联分类器实例（新接口，推荐使用）
 * @param timeoutMs 最大等待时间（毫秒），-1表示无限等待
 * @return std::shared_ptr<cv::CascadeClassifier> 实例指针
 */
std::shared_ptr<cv::CascadeClassifier> getRobustEyeCascade(int timeoutMs = -1);

/**
 * @brief 紧急扩容接口
 * @param count 要扩容的数量
 * @return int 实际扩容的数量
 */
int emergencyExpandPool(int count);

/**
 * @brief 获取对象池统计信息
 * @return RobustEyeCascadePool::PoolStats 统计信息
 */
RobustEyeCascadePool::PoolStats getRobustPoolStats();

/**
 * @brief 关闭对象池
 */
void shutdownEyeCascadePool();

// ============================================================================
// 兼容旧接口的函数声明
// ============================================================================

/**
 * @brief 兼容旧接口：获取眼部级联分类器引用
 * @return cv::CascadeClassifier& 实例引用
 *
 * 每个线程有自己独立的缓存实例，保持与旧代码完全兼容
 * 用法：CascadeClassifier& eyeCascade = getEyeCascade();
 */
cv::CascadeClassifier& getEyeCascade();

/**
 * @brief 清理当前线程的缓存实例
 *
 * 在线程退出或需要释放资源时调用
 */
void clearThreadLocalEyeCascade();

/**
 * @brief 旧接口的初始化函数（可选）
 * @return bool 初始化是否成功
 */
bool initializeEyeCascade();

/**
 * @brief 检查当前线程是否有缓存的实例
 * @return bool 是否有缓存实例
 */
bool hasThreadLocalCascade();

/**
 * @brief 获取对象池状态信息（字符串形式，用于日志输出）
 * @return QString 状态信息字符串
 */
QString getEyeCascadePoolStatus();

#endif // ROBUSTEYECASCADEPOOL_H
