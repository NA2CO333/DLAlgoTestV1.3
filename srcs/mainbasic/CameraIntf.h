#ifndef CCAMERAINTF_H
#define CCAMERAINTF_H

#include <QObject>

#include "globaltypes.h"

// 相机错误
enum enCameraStat
{
    cameraStat_Unknow               = -1,       // 未知
    cameraStat_Succ                 = 0,        // 成功
    cameraStat_Fail                 = 1,        // 操作失败
    cameraStat_ParamInvalid         ,           // 传入的参数非法
    cameraStat_Timeout              ,           // 相机获取图像超时     /* 逻辑错误，属于较特殊的错误，要根据具体业务逻辑处理 */
    cameraStat_Unrecoverable        = 9999,     // 不可恢复错误       /* 当发生这种错误时，说明相机已无法继续工作 */
};
QString enumToText_CameraStat(enCameraStat _stat);

// 相机触发模式
enum enCameraTriggerMode
{
    cameraTriggerMode_Auto      = 0,    // 自动触发
    cameraTriggerMode_Soft,             // 软触发
    cameraTriggerMode_Hard,             // 硬触发

    cameraTriggerMode_Min = cameraTriggerMode_Auto,
    cameraTriggerMode_Max = cameraTriggerMode_Hard,
};

// 相机状态
struct stCameraStatInfo
{
    enCameraStat cameraStat;
    int errCode;        // 错误码（一般是相机 API 的返回值）
    QString errMsg;

    stCameraStatInfo(enCameraStat _camera_stat /*= cameraStat_Succ*/) {     /* 不使用默认参数，构造时值明确 */
        cameraStat = _camera_stat;
        errCode = 0;

    }
};

/// 通用相机接口
class CCameraIntf : public QObject               // TODO: 工期有限，函数名暂时拷贝旧代码的，还需检查优化和精简
{
    Q_OBJECT
public:
    static CCameraIntf *newInstance();
    ~CCameraIntf();

    //
    virtual stCameraStatInfo initCamera(int _find_dev_timeout = 3000) = 0;      // 初始化      /* 注意：若初始化中，不应再初始化 */
    virtual stCameraStatInfo uninitCamera(int _find_dev_timeout = 2000) = 0;

    virtual int getImgWidth() = 0;
    virtual int getImgHeight() = 0;

    virtual stCameraStatInfo getImageBuffer(uchar **_img_data, int _timeout) = 0;       // 获取帧图像    // TODO: 命名应去掉 Buffer ？
    virtual stCameraStatInfo imageProcess(uchar *_img_in, uchar *_img_out) = 0;           // TODO: 合并到 getImageBuffer() ？
    virtual stCameraStatInfo releaseImageBuffer(uchar *_img_data) = 0;                  // 释放帧图像（缓冲区内的）
    virtual stCameraStatInfo softTrigger() = 0;
    virtual stCameraStatInfo play() = 0;              /* play() 和 stop() 可能消耗较大，频繁操作应该用 pause() 和 restart() */
    virtual stCameraStatInfo stop() = 0;
    virtual stCameraStatInfo pause() = 0;
    virtual stCameraStatInfo restart() = 0;

    virtual stCameraStatInfo setBufferQueueSize(int _size) = 0;                                 // 设置缓存队列大小
    virtual stCameraStatInfo clearFrameBuffer(const int _timeout_ms, int &_count_cleared) = 0;  // 清空帧缓冲区

    virtual stCameraStatInfo setExposureTime(int *_exposure_time) = 0;                  // 设置曝光时间（单位：微秒）
    virtual int getExposureTime() = 0;                                                  // 获取曝光时间（单位：微秒）
    virtual stCameraStatInfo setTriggerMode(enCameraTriggerMode _trigger_mode) = 0;

    virtual stCameraStatInfo setTriggerDelayUs(int _delay_us) = 0;                  // 设置触发延时

    virtual stCameraStatInfo setTriggerInputType(enTriggerInputType _type) = 0;     // 设置 触发输入类型
    virtual stCameraStatInfo getTriggerInputType(enTriggerInputType &_type) = 0;    // 获取 触发输入类型

    virtual bool setAnalogGain(float _gain) = 0;
    virtual float getAnalogGain() = 0;

    virtual bool getContrast(int *_contrast) = 0;

    virtual stCameraStatInfo resetParams() = 0;
    virtual stCameraStatInfo disconnectCamera() = 0;

    //void dvpSetFlipVerticalState();       // 设置是否垂直翻转

    bool getIsOn() { return isOn; }                             // 是否已打开（初始化已完成）
    bool getIsIniting() { return isIniting; }                   // 是否正在初始化（此时 isOn 为 false，但只能等待，不可继续再初始化）
    enCameraStat getCameraStatus() { return lastStatus; }

protected:
    explicit CCameraIntf(QObject *_parent = nullptr);

    bool isOn = false;
    bool isIniting = false;
    enCameraStat lastStatus;

};

// 迈德威视 相机
#if (CAMERA_TYPE == 1)

/// 迈德威视 相机接口
class CCameraMV : public CCameraIntf        // TODO: 分离到独立模块，只有 CameraIntf.cpp 才引用本声明？
{
public:
    ~CCameraMV();

    enCameraStat initCamera(int _find_dev_timeout = 3000) override;
    enCameraStat GUI_init_exposure(int hCamera);

    int getImgWidth();
    int getImgHeight();

    enCameraStat getImageBuffer(uchar **_img_data, int _timeout);
    enCameraStat imageProcess(uchar *_img_in, uchar *_img_out);
    enCameraStat releaseImageBuffer(uchar *_img_data);
    enCameraStat softTrigger();
    enCameraStat play();
    enCameraStat stop();
    enCameraStat pause();
    enCameraStat restart();

    enCameraStat setExposureTime(int *_exposure_time);
    int getExposureTime();
    enCameraStat setTriggerMode(enCameraTriggerMode _trigger_mode);

    bool setAnalogGain(float _gain);
    float getAnalogGain();

    bool getContrast(int *_contrast);

    enCameraStat getCameraStatus();

    bool resetParams();
    bool disconnectCamera();

    enCameraStat setCameraRoiResolution(int _camera_handle, int _offset_x, int _offset_y, int _width, int _height);

protected:
    friend class CCameraIntf;

    explicit CCameraMV();

    int exposureTime = 0;           // 曝光时间（微秒）
    float analogGain = 2.5;         // 模拟增益（倍数）

};

#else

#  include "DVPCamera.h"

/// 度申 相机接口
class CCameraD3T : public CCameraIntf        // TODO: 分离到独立模块，只有 CameraIntf.cpp 才引用本声明？     // TODO: 线程安全？加锁？或确保只有一个线程访问本对象？
{
    Q_OBJECT
public:
    ~CCameraD3T();

    stCameraStatInfo initCamera(int _find_dev_timeout = 3000) override;
    stCameraStatInfo uninitCamera(int _timeout = 2000) override;

    int getImgWidth() override;
    int getImgHeight() override;

    stCameraStatInfo getImageBuffer(uchar **_img_data, int _timeout) override;
    stCameraStatInfo imageProcess(uchar *_img_in, uchar *_img_out) override;
    stCameraStatInfo releaseImageBuffer(uchar *_img_data) override;
    stCameraStatInfo softTrigger() override;
    stCameraStatInfo play() override;
    stCameraStatInfo stop() override;
    stCameraStatInfo pause() override;
    stCameraStatInfo restart() override;

    stCameraStatInfo clearFrameBuffer(const int _timeout_ms, int &_count_cleared) override;
    stCameraStatInfo setBufferQueueSize(int _size) override;            // NOTE: 已在初始化时通过 dvpSetBufferConfig() 设置？

    stCameraStatInfo setExposureTime(int *_exposure_time) override;
    int getExposureTime() override;
    stCameraStatInfo setTriggerMode(enCameraTriggerMode _trigger_mode) override;

    stCameraStatInfo setTriggerDelayUs(int _delay_us) override;                 // 设置触发延时

    stCameraStatInfo setTriggerInputType(enTriggerInputType _type) override;    // 设置 触发输入类型
    stCameraStatInfo getTriggerInputType(enTriggerInputType &_type) override;   // 获取 触发输入类型

    bool setAnalogGain(float _gain) override;
    float getAnalogGain() override;

    bool getContrast(int *_contrast) override;

    stCameraStatInfo resetParams() override;
    stCameraStatInfo disconnectCamera() override;

    static dvpInt32 dvpEventCallback(dvpHandle handle, dvpEvent event, void *pContext, dvpInt32 param, struct dvpVariant *pVariant);

protected:
    friend class CCameraIntf;
    static const char * const S_CLASS_NAME;     // 本类的类名

    explicit CCameraD3T(QObject *_parent = nullptr);

    static bool isDevUnableUse;

    int imgLeft;
    int imgTop;
    int imgWidth;
    int imgHeight;

    int exposureTime = 0;           // 曝光时间（微秒）
    float analogGain = 2.0;         // 模拟增益（倍数）     // NOTE: 此缺省值，来自2022年底刘宇的视筛新版的 python 原型程序的调试结果？

    bool isAutoTriggerOpened = false;   // 是否已打开自动触发    /* 出现过反复切换触发使能状态时疑似堵塞在 dvpSetTriggerMode() 函数中，所以尽量减少该函数的调用。 */

    bool getIsErrorNeedRetry(dvpStatus _stat_api);

    stCameraStatInfo doGetImageBuffer(uchar **_img_data, int _timeout);
    stCameraStatInfo doSoftTrigger();
    stCameraStatInfo doClearFrameBuffer(const int _timeout_ms, int &_count_cleared);
    stCameraStatInfo doClearFrameBufferByApi();
    dvpStatus doSetTriggerMode(enCameraTriggerMode _trigger_mode);

    dvpTriggerInputType convertTriggerInputType(enTriggerInputType _type);
    enTriggerInputType convertTriggerInputType(dvpTriggerInputType _type);

};

#endif

#endif // CCAMERAINTF_H
