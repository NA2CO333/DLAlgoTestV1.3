#ifndef HAAR_THREAD_SAFE_H
#define HAAR_THREAD_SAFE_H

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

// 提前声明外部路径获取函数
std::vector<std::string> getCascadePaths();

/**
 * @brief 线程安全的 Haar 级联分类器管理器 (单例模式)
 *
 * 设计优势：
 * 1. 启动时一次性将 XML 读入内存，后续线程均从内存加载，消除磁盘 IO 瓶颈。
 * 2. 使用 thread_local 为每个工作线程维护独立的分类器实例，彻底杜绝多线程竞争。
 */
class HaarThreadSafeManager {
public:
    // 禁用拷贝和赋值
    HaarThreadSafeManager(const HaarThreadSafeManager&) = delete;
    HaarThreadSafeManager& operator=(const HaarThreadSafeManager&) = delete;

    // 获取单例实例
    static HaarThreadSafeManager& getInstance() {
        static HaarThreadSafeManager instance;
        return instance;
    }

    /**
     * @brief 获取当前线程专属的分类器引用
     * @return cv::CascadeClassifier& 分类器的引用
     */
    cv::CascadeClassifier& getCascade();

private:
    HaarThreadSafeManager(); // 私有构造，初始化时读取内存
    ~HaarThreadSafeManager() = default;

    // 核心数据：存储在堆内存中的 XML 文件内容
    std::vector<uchar> m_memoryBuffer;
};

/**
 * @brief 重构后的线程安全眼部检测函数
 *
 * @param _img 输入图像
 * @param scaleFactor 缩放因子
 * @param miniNeighbors 最小邻居数
 * @param scale 输出框的放大系数
 * @param minSize 最小眼眶尺寸
 * @param maxSize 最大眼眶尺寸
 * @return std::pair<cv::Rect, cv::Rect> 左右眼矩形框
 */
std::pair<cv::Rect, cv::Rect> haarDetectEyesSafe(const cv::Mat& _img,
                                                 const double scaleFactor,
                                                 int miniNeighbors,
                                                 const float scale,
                                                 const cv::Size& minSize = cv::Size(25,25),
                                                 const cv::Size& maxSize = cv::Size(45,45));

#endif // HAAR_THREAD_SAFE_H
