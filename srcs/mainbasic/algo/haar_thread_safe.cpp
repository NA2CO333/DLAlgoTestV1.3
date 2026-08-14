#include "haar_thread_safe.h"
#include <QDebug>
#include <fstream>
#include <sstream>
#include <unistd.h>   // 用于 getpid, close
#include <fcntl.h>    // 用于 open 系统调用
#include <QThread>
#include "perftimer.h"

using namespace cv;

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
//返回平均矩形
Rect averageRect(const std::vector<Rect>& rects) {
    if (rects.empty()) {
        return cv::Rect(); // 返回空值的Rect
    }
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;

    for (const Rect& rect : rects) {
        x += rect.x;
        y += rect.y;
        width += rect.width;
        height += rect.height;
    }

    int num_rects = rects.size();
    x /= num_rects;
    y /= num_rects;
    width /= num_rects;
    height /= num_rects;

    return cv::Rect(x, y, width, height);
}

//非极大值抑制
Rect maxRect(std::vector<Rect>& rects) {
    Rect max_rect;
    int max_width = 0;
    if (rects.size() > 0) {
        for (Rect& rect : rects) {
            if (rect.width > max_width) {
                max_rect = rect;
                max_width = rect.width;
            }
        }
    }
    return max_rect;
}
// ==============================================================
// HaarThreadSafeManager 实现
// ==============================================================

HaarThreadSafeManager::HaarThreadSafeManager() {
    m_memoryBuffer.clear();
    std::vector<std::string> paths = getCascadePaths();

    // 1. 主线程启动时，一次性将 XML 读入堆内存
    for (const auto& path : paths) {
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (ifs.is_open()) {
            std::streamsize size = ifs.tellg();
            ifs.seekg(0, std::ios::beg);

            m_memoryBuffer.resize(size);
            if (ifs.read(reinterpret_cast<char*>(m_memoryBuffer.data()), size)) {
                qDebug() << "Successfully pre-loaded Haar cascade into heap memory from:" << path.c_str()
                         << "(Size:" << size << " bytes)";
                break;
            }
        }
    }

    if (m_memoryBuffer.empty()) {
        ALGO_ERROR_LOG(
            qCritical() << "FATAL: Failed to read Haar cascade XML into memory!"
        );
    }
}

cv::CascadeClassifier& HaarThreadSafeManager::getCascade() {
    // 【核心重构】：使用原始指针，指向堆内存
    static thread_local cv::CascadeClassifier* tls_cascade_ptr = [this]() {
        cv::CascadeClassifier* ptr = new cv::CascadeClassifier(); // 在堆上分配

        if (!m_memoryBuffer.empty()) {
            std::ostringstream oss;
            oss << "/dev/shm/haar_cache_pid_" << getpid() << "_tid_" << QThread::currentThreadId() << ".xml";
            std::string tmp_path = oss.str();

            bool load_success = false;
            int fd = -1;

            try {
                fd = open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd != -1) {
                    ssize_t written = write(fd, m_memoryBuffer.data(), m_memoryBuffer.size());
                    close(fd);
                    fd = -1;

                    if (written == static_cast<ssize_t>(m_memoryBuffer.size())) {
                        load_success = ptr->load(tmp_path); // 使用指针调用 load
                    }
                }
            } catch (...) {
                if (fd != -1) close(fd);
                ALGO_ERROR_LOG(
                    qCritical() << "Exception during memory file creation for cascade."
                );
            }

            if (load_success) {
                qDebug() << "Thread [" << QThread::currentThreadId()
                         << "] Initialized 专属 CascadeClassifier (Heap mode).";
            } else {
                ALGO_ERROR_LOG(
                    qCritical() << "Thread [" << QThread::currentThreadId()
                                << "] Failed to load classifier from RAM disk!"
                );
            }

            std::remove(tmp_path.c_str());
        }

        return ptr; // 返回指针（故意不释放，交给 OS 线程回收机制）
    }();

    // 解引用返回引用，保持外部调用方式完全不变
    return *tls_cascade_ptr;
}


// ==============================================================
// haarDetectEyesSafe 实现 (与之前相同，保持不变)
// ==============================================================

std::pair<cv::Rect, cv::Rect> haarDetectEyesSafe(const cv::Mat& _img,
                                                 const double scaleFactor,
                                                 int miniNeighbors,
                                                 const float scale,
                                                 const cv::Size& minSize,
                                                 const cv::Size& maxSize) {
    cv::Rect rightEye, leftEye;
    std::vector<cv::Rect> eyes;

    try {
        PERF_SCOPE("detectMultiScale");

        cv::CascadeClassifier* eyeCascadePtr = nullptr;
        {
#if ENABLE_ALGO_TIMING_LOG
            ALGO_TIMING_SCOPE(AlgoTimingStage_HaarCascadeAccess);
#endif
            eyeCascadePtr = &HaarThreadSafeManager::getInstance().getCascade();
        }
        cv::CascadeClassifier& eyeCascade = *eyeCascadePtr;

        if (eyeCascade.empty()) {
            return std::make_pair(rightEye, leftEye);
        }
        eyeCascade.detectMultiScale(_img, eyes, scaleFactor, miniNeighbors, CASCADE_SCALE_IMAGE, minSize, maxSize);

    } catch (const cv::Exception& e) {
        ALGO_ERROR_LOG(
            qCritical() << "OpenCV Exception in haarDetectEyesSafe:" << e.what()
        );
    } catch (...) {
        ALGO_ERROR_LOG(qCritical() << "Unknown Exception in haarDetectEyesSafe");
    }

    if (eyes.size() > 0) {
        std::vector<cv::Rect> rRects;
        std::vector<cv::Rect> lRects;
        for(size_t i = 0; i < eyes.size(); i++) {
            Rect e = eyes.at(i);
            Rect eye(scale * e.x, scale * e.y, scale * e.width, scale * e.height);
            if(e.x + e.width / 2 <= _img.cols / 2) {
                rRects.push_back(eye);
            } else {
                lRects.push_back(eye);
            }
        }
        rightEye = maxRect(rRects);
        leftEye = maxRect(lRects);
    }

    return std::make_pair(rightEye, leftEye);
}
