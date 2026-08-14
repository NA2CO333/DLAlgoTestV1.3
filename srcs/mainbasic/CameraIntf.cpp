#include "CameraIntf.h"

#include <QDebug>
#include <QTime>
#include <QElapsedTimer>
#include <QThread>

#include "camerainit.h"     // TODO: 移出去？
#include "global.h"
#include "exposure-adjuster.h"
#include "windowsmanager.h"
#include "winmeasure.h"

//
namespace  {
    const QString CAMERA_MODEL = "M3S130M";     // 相机型号
}

QString enumToText_CameraStat(enCameraStat _stat)
{
    switch (_stat) {
    case cameraStat_Unknow          : return "Unknow";
    case cameraStat_Succ            : return "Succ";
    case cameraStat_Fail            : return "Fail";
    case cameraStat_ParamInvalid    : return "ParamInvalid";
    case cameraStat_Timeout         : return "Timeout";
    case cameraStat_Unrecoverable   : return "Unrecoverable";
    }
    return "??";
}

///=============================================================================================================================
/// class CCameraIntf

CCameraIntf *CCameraIntf::newInstance()
{
#if (CAMERA_TYPE == 1)
    return new CCameraMV();
#else
    return new CCameraD3T();
#endif
}

CCameraIntf::CCameraIntf(QObject *_parent) : QObject(_parent)
{
}

CCameraIntf::~CCameraIntf()
{
}

///=============================================================================================================================
/// class CCameraMV

#if (CAMERA_TYPE == 1)

#  include "CameraApi.h"

//
int gCameraHandle = -1;         // 相机设备句柄
enCameraStat lastStatus;

tSdkCameraCapbility g_tCapability;      // 设备描述信息
tSdkFrameHead gLastSdkFrameHead;        // 最后从 SDK 的 读图函数获得的帧头信息

//
CCameraMV::CCameraMV() : CCameraIntf()
{

}

CCameraMV::~CCameraMV()
{

}

// SDK 等初始化操作
enCameraStat CCameraMV::initCamera(int _find_dev_timeout)
{
    enCameraStat ret = cameraStat_Fail;
    QString msg;

    do
    {
        int                 camera_count = 1;
        CameraSdkStatus     camera_stat = CAMERA_STATUS_UNKNOW;
        tSdkCameraDevInfo   tCameraEnumList[1];

        //sdk初始化 0 English 1 中文
        camera_stat  = CameraSdkInit(1);
        if (cameraStat_Succ != camera_stat ) {
            msg = "Init camera failed!";
            ret = cameraStat_InitSdkFail;
            break;
        }

#ifdef CAMERA_MINDVISION_SDK_NEW
        // 设置缓存帧数（“通常需要在 CameraInit 打开相机之前配置好”）
        camera_stat  = CameraSetSysOption("NumBuffers", "30");
#endif

        //枚举设备，并建立设备列表
        camera_stat  = CameraEnumerateDevice(tCameraEnumList, &camera_count);   /* 注意：第二个参数不仅仅是输出，同时也要指定第一个数组元素数，所以不可为 0 或未初始化 */

        //没有连接设备
        if (0 == camera_count) {
            msg = "Camera not found!";
            ret = cameraStat_FindFail;
            break;
        }

        int is_opened;
        camera_stat  = CameraIsOpened(&tCameraEnumList[0], &is_opened);

        if (0 != is_opened) {
            msg = "Open camera failed!";
            ret = cameraStat_OpenFail;
            break;
        }

        //相机初始化。初始化成功后，才能调用任何其他相机相关的操作接口
        camera_stat  = CameraInit(&tCameraEnumList[0], -1, -1, &gCameraHandle);

        //初始化失败
        if(cameraStat_Succ != camera_stat) {
            msg = "Init camera failed!";
            ret = cameraStat_InitCameraFail;
            break;
        }

        // 恢复默认参数
        //camera_stat = CameraLoadParameter(gCameraHandle, PARAMETER_TEAM_DEFAULT);
        //if (camera_stat != cameraStat_OK) {
        //    logCritical("CCameraMV::init_SDK(): Load default params failed!", CGlobal::LOG_CAPTURE);
        //}

        //获得相机的特性描述结构体。该结构体中包含了相机可设置的各种参数的范围信息。决定了相关函数的参数
        camera_stat  = CameraGetCapability(gCameraHandle,&g_tCapability);

        // 设置分辨率
        //tSdkImageResolution     *pImageSizeDesc = g_tCapability.pImageSizeDesc;
        //camera_stat  = CameraSetImageResolution(gCameraHandle,&(pImageSizeDesc[0]));

        int left    = IMG_ROI_LEFT;
        int top     = IMG_ROI_TOP;
        int width   = IMG_ROI_WIDTH;
        int height  = IMG_ROI_HEIGHT;
    #if (OS_TYPE != 2)
        height += top;
    #endif
        // TODO: 在PC平台这里的高度参数为什么必须要设成 16 的倍数才行，否则读图超时，而 demo 里不用？ARM平台好像不用，但可能有未发现的问题？
        // TODO: 在ARM平台这里的第四个参数好像是末行的行号而不是高度？
        camera_stat = setCameraRoiResolution(gCameraHandle, left, top, width, height);

        if (cameraStat_Succ != camera_stat ) {
            msg = "Set roiResolution failed!";
            ret = cameraStat_SetParamFail;
            break;
        }

        // 获得当前分辨率
        //tSdkImageResolution     sResolution;
        //camera_stat  = CameraGetImageResolution(gCameraHandle,&sResolution);

        /*让SDK进入工作模式，开始接收来自相机发送的图像
        数据。如果当前相机是触发模式，则需要接收到
        触发帧以后才会更新图像。    */
        //CameraPlay(gCameraHandle);

        /*
            设置图像处理的输出格式，彩色黑白都支持RGB24位
        */
        if (g_tCapability.sIspCapacity.bMonoSensor) {   //如果是黑白相机，则颜色相关的功能都无法调节
            camera_stat  = CameraSetIspOutFormat(gCameraHandle, CAMERA_MEDIA_TYPE_MONO8);
        }
        else
        {
            logCritical("Camera image type is not MONO8 ?!", CGlobal::LOG_CAPTURE);

            //camera_stat = CameraSetIspOutFormat(gCameraHandle, CAMERA_MEDIA_TYPE_RGB8);
            camera_stat  = CameraSetIspOutFormat(gCameraHandle, CAMERA_MEDIA_TYPE_MONO8);        /* 后面的抓图过程没有考虑兼容彩色图像，所以这里必须设置为黑白。 */
        }

        //****************设置帧率
        camera_stat  = CameraSetFrameSpeed(gCameraHandle, 0);//设定相机输出图像贞率

        //******设置曝光值**********
        GUI_init_exposure(gCameraHandle);

//        int RPos,GPos,BPos,Saturation;

//        CameraGetGain(gCameraHandle,&RPos,&GPos,&BPos);
//        CameraSetGain(gCameraHandle,RPos,GPos,BPos);

//        CameraGetSaturation(gCameraHandle,&Saturation);
//        CameraSetSaturation(gCameraHandle,Saturation);
//        qDebug()<<"RPos = "<<RPos<<"; GPos = "<<GPos<<"; BPos = "<<BPos<<"; Saturation = "<<Saturation;


//        //********获得触发模式**********
//        int  pbySnapMode;
//        CameraGetTriggerMode(gCameraHandle,&pbySnapMode);
//        pbySnapMode = 0;
        camera_stat  = CameraSetTriggerMode(gCameraHandle, SOFT_TRIGGER);
        //***************获得/设置LUT模式下的gamma,对比度
//        int gamma=0;
//        int contrast=0;
//        tGammaRange *sGammaRange = &g_tCapability.sGammaRange;
//        tContrastRange *sContrastRange = &g_tCapability.sContrastRange;

        camera_stat  = CameraSetTriggerCount(gCameraHandle, 1);          // 设置每次触发帧数
        if (cameraStat_Succ != camera_stat ) {
            msg = "Set TriggerCount failed!";
            ret = cameraStat_SetParamFail;
            break;
        }


//        //获得LUT动态生成模式下的Gamma值。
//        CameraGetGamma(gCameraHandle,&gamma);
//        CameraSetGamma(gCameraHandle,gamma);

//        //获得LUT动态生成模式下的对比度值
//        CameraGetContrast(gCameraHandle,&contrast);
//        CameraSetContrast(gCameraHandle,contrast);

//        //****************  获得图像的镜像状态和锐化设定值*******

//        BOOL        m_bHflip=FALSE;
//        BOOL        m_bVflip=FALSE;
//        int         m_Sharpness=0;

//        //tSharpnessRange  *SharpnessRange =   &g_tCapability.sSharpnessRange;
//        //获得图像的镜像状态。
//        CameraGetMirror(gCameraHandle, MIRROR_DIRECTION_HORIZONTAL, &m_bHflip);
//        CameraGetMirror(gCameraHandle, MIRROR_DIRECTION_VERTICAL,   &m_bVflip);

//        CameraSetMirror(gCameraHandle, MIRROR_DIRECTION_HORIZONTAL, m_bHflip);
//        CameraSetMirror(gCameraHandle, MIRROR_DIRECTION_VERTICAL,   m_bVflip);
//        //获取当前锐化设定值。
//        CameraGetSharpness(gCameraHandle, &m_Sharpness);
//        CameraSetSharpness(gCameraHandle, m_Sharpness);
//        //*****************************

        //
        ret = cameraStat_Succ;
    } while (false);

    if (cameraStat_Succ != ret) {
        logCritical(QString("CCameraMV::initCamera() error: ") + msg);
        _msg = msg;
    }
    lastStatus = ret;

    //
    return ret;
}

//设置相机曝光参数
enCameraStat CCameraMV::GUI_init_exposure(int hCamera)
{
    enCameraStat ret = cameraStat_Succ;

    //
//    double	        m_fExpLineTime=0;//当前的行曝光时间，单位为us
//    tSdkExpose      *SdkExpose =   &pCameraInfo->sExposeDesc;
    INT pusAnalogGain;
    CameraSdkStatus camera_stat = CAMERA_STATUS_FAILED;

    //获得相机当前的曝光模式。
//    camera_stat = CameraGetAeState(hCamera,&AEstate);

    //获得自动曝光的亮度目标值。
//    camera_stat = CameraGetAeTarget(hCamera,&pbyAeTarget);

    //获得自动曝光时抗频闪功能的使能状态。
//    camera_stat = CameraGetAntiFlick(hCamera,&FlickEnable);
//    FlickEnable = true;
//    camera_stat = CameraSetAntiFlick(hCamera,FlickEnable);

    // 设置手动曝光模式
    camera_stat = CameraSetAeState(hCamera, false);

    // 设置曝光时间
    int expo = g_WinMeasure->exposureAdjuster()->defaultExposureUs();
    ret = setExposureTime(&expo);

    //获得图像信号的模拟增益值。
    camera_stat = CameraGetAnalogGain(hCamera,&pusAnalogGain);
//    qDebug()<<"pusAnalogGain = "<<pusAnalogGain<< ";   "<<(pusAnalogGain * g_tCapability.sExposeDesc.fAnalogGainStep);

    camera_stat = CameraSetAnalogGain(hCamera, analogGain * g_tCapability.sExposeDesc.fAnalogGainStep);

//    //获得自动曝光时，消频闪的频率选择。
//    camera_stat = CameraGetLightFrequency(hCamera,&piFrequencySel);

/*
    获得一行的曝光时间。对于CMOS传感器，其曝光
    的单位是按照行来计算的，因此，曝光时间并不能在微秒
    级别连续可调。而是会按照整行来取舍。这个函数的
    作用就是返回CMOS相机曝光一行对应的时间。
*/
//    camera_stat = CameraGetExposureLineTime(hCamera, &m_fExpLineTime);
//    qDebug()<<"AEstate = "<<AEstate<<" ; 曝光时间 == "<<gExposureTime<<"; FlickEnable= "<<FlickEnable;

    //设置相机曝光的模式。自动或者手动。

    return ret;
}

int CCameraMV::getImgWidth()
{
    return gLastSdkFrameHead.iWidth;
}

int CCameraMV::getImgHeight()
{
    return gLastSdkFrameHead.iHeight;
}

enCameraStat CCameraMV::getImageBuffer(uchar **_img_data, int _timeout)
{
    enCameraStat ret = cameraStat_Fail;

//    //
//    if (gLastSdkFrameHead.uiMediaType != CAMERA_MEDIA_TYPE_MONO8) {      // TODO: 有必要检查吗？
//        logCritical("CCameraMV::getImageBuffer(): uiMediaType != CAMERA_MEDIA_TYPE_MONO8 !", CGlobal::LOG_CAPTURE);

//        // TODO: 退出？

//    }

    CameraSdkStatus camera_stat = CameraGetImageBuffer(gCameraHandle, &gLastSdkFrameHead, _img_data, _timeout);
    if (CAMERA_STATUS_SUCCESS == camera_stat) {
        ret = cameraStat_Succ;
    } else if (CAMERA_STATUS_TIME_OUT == camera_stat) {
        ret = cameraStat_Timeout;
    }

    return ret;
}

enCameraStat CCameraMV::imageProcess(uchar *_img_in, uchar *_img_out)
{
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus camera_stat = CameraImageProcess(gCameraHandle, _img_in, _img_out, &gLastSdkFrameHead);
    if (CAMERA_STATUS_SUCCESS == camera_stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

enCameraStat CCameraMV::releaseImageBuffer(uchar *_img_data)
{
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus camera_stat = CameraReleaseImageBuffer(gCameraHandle, _img_data);
    if (CAMERA_STATUS_SUCCESS == camera_stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

tSdkFrameHead frameHead;

enCameraStat CCameraMV::setExposureTime(int *_exposure_time)
{
    enCameraStat ret = cameraStat_Fail;

    //CameraPause(gCameraHandle);       // TODO: 这里不应该 CameraPause() 吧？

    CameraSdkStatus stat = CameraSetExposureTime(gCameraHandle, *_exposure_time);

    if (CAMERA_STATUS_SUCCESS == stat) {
        stat = CameraGetExposureTime(gCameraHandle, _exposure_time);
        if (CAMERA_STATUS_SUCCESS == stat) {
            exposureTime = *_exposure_time;
        }
    }

    //CameraPlay(gCameraHandle);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

bool CCameraMV::setAnalogGain(float _gain)
{
    enCameraStat ret = cameraStat_Fail;

    int gain = _gain * g_tCapability.sExposeDesc.fAnalogGainStep;
    CameraSdkStatus stat = CameraSetAnalogGain(gCameraHandle, gain);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

bool CCameraMV::getContrast(int *_contrast)
{
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus stat = CameraGetContrast(gCameraHandle, _contrast);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

bool CCameraMV::resetParams()
{
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus stat = CameraLoadParameter(gCameraHandle, PARAMETER_TEAM_DEFAULT);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

bool CCameraMV::disconnectCamera()
{
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus stat = CameraStop(gCameraHandle);
    stat = CameraUnInit(gCameraHandle);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

enCameraStat CCameraMV::setTriggerMode(enCameraTriggerMode _trigger_mode)
{
    // 通用接口的触发模式转 SDK 的触发模式，数组的索引为通用接口的触发模式值
    const int TRIGGER_MODE_INTF_TO_SDK[] = {0, 1, 2};

    //
    enCameraStat ret = cameraStat_Fail;

    //
    if (_trigger_mode < cameraTriggerMode_Min || _trigger_mode > cameraTriggerMode_Max) {
        logWarning("CCameraMV::setTriggerMode(): out of range!", CGlobal::LOG_CAPTURE);
        return ret;
    }

    //
    CameraSdkStatus stat = CAMERA_STATUS_FAILED;
    //stat = CameraPause(gCameraHandle);              /* 实测：是否先 Pause() 或 Stop()，对于是否丢帧或错帧，没有明显影响 */

    int trigger_mode = TRIGGER_MODE_INTF_TO_SDK[(int)_trigger_mode];
    stat = CameraSetTriggerMode(gCameraHandle, trigger_mode);
    //if (CAMERA_STATUS_SUCCESS == stat)
    //    stat = CameraPlay(gCameraHandle);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

float CCameraMV::getAnalogGain()
{
    return analogGain;
}

int CCameraMV::getExposureTime()
{
    return exposureTime;
}

enCameraStat CCameraMV::stop()
{
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus stat = CameraStop(gCameraHandle);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

enCameraStat CCameraMV::softTrigger()
{
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus stat = CameraSoftTrigger(gCameraHandle);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

enCameraStat CCameraMV::play()
{
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus stat = CameraStop(gCameraHandle);
    stat = CameraPlay(gCameraHandle);

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

// 设置 ROI 分辨率
/* 参考：SDK/demo/QT5_Demo/mainwindow.cpp -> MainWindow::SetCameraResolution(...)
 * offsetx, offsety, width, height: 偏移宽高都选取为16的倍数兼容性最好（不同的相机对这个的要求不同，有的只需要2的倍数，有的可能需要16的倍数）
 */
enCameraStat CCameraMV::setCameraRoiResolution(int _camera_handle, int _offset_x, int _offset_y, int _width, int _height)
{
    tSdkImageResolution sRoiResolution;
    memset(&sRoiResolution, 0, sizeof(tSdkImageResolution));

    // 设置成0xff表示自定义分辨率，设置成0到N表示选择预设分辨率
    sRoiResolution.iIndex = 0xFF;

    // iWidthFOV表示相机的视场宽度，iWidth表示相机实际输出宽度
    // 大部分情况下iWidthFOV=iWidth。有些特殊的分辨率模式如BIN2X2：iWidthFOV=2*iWidth，表示视场是实际输出宽度的2倍
    sRoiResolution.iWidth = _width;
    sRoiResolution.iWidthFOV = _width;

    // 高度，参考上面宽度的说明
    sRoiResolution.iHeight = _height;
    sRoiResolution.iHeightFOV = _height;

    // 视场偏移
    sRoiResolution.iHOffsetFOV = _offset_x;
    sRoiResolution.iVOffsetFOV = _offset_y;

    // ISP软件缩放宽高，都为0则表示不缩放
    sRoiResolution.iWidthZoomSw = 0;
    sRoiResolution.iHeightZoomSw = 0;

    // BIN SKIP 模式设置（需要相机硬件支持）
    sRoiResolution.uBinAverageMode = 0;
    sRoiResolution.uBinSumMode = 0;
    sRoiResolution.uResampleMask = 0;
    sRoiResolution.uSkipMode = 0;

    //
    enCameraStat ret = cameraStat_Fail;

    CameraSdkStatus stat = CameraSetImageResolution(_camera_handle, &sRoiResolution);

    // TODO: 获得抓拍模式下的分辨率？上面只是预览分辨率？
    if (CAMERA_STATUS_SUCCESS != stat) {
        stat = CameraSetResolutionForSnap(_camera_handle, &sRoiResolution);
    }

    if (CAMERA_STATUS_SUCCESS == stat) {
        ret = cameraStat_Succ;
    }

    return ret;
}

#else

///=============================================================================================================================
/// class CCameraD3T

/* SDK From：DVP2-ARM64_2022.7.22.tar.gz，lib 版本号来自与 so 文件在同一压缩包内的 DVPCamera.h */

/* 清空 数据流 buffer （度申头文件未公开的宏变量定义） */
#define DSCAM_PARAM_BUFFER_CLEAR  0x03d

// 相机掉线重试次数
#define RETRY_TIMES     2
#define RETRY_DELAY_MS  300

//
dvpHandle gCameraHandle = -1;           // 相机设备句柄
dvpFrame gLastFrameInfo;                // 最后一帧的帧信息

// 前置声明：相机 API 错误码 转 描述
QString errStr(dvpStatus _err_id);

// 通用接口的触发模式转 SDK 的触发模式，数组的索引为通用接口的触发模式值
const dvpTriggerSource TRIGGER_MODE_INTF_TO_SDK[] = {(dvpTriggerSource)-1, TRIGGER_SOURCE_SOFTWARE, TRIGGER_SOURCE_LINE1};

//
bool CCameraD3T::isDevUnableUse = false;

//
const char * const CCameraD3T::S_CLASS_NAME = CCameraD3T::staticMetaObject.className();

CCameraD3T::CCameraD3T(QObject *_parent) : CCameraIntf(_parent)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    memset(&gLastFrameInfo, 0, sizeof(dvpFrame));

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
}

CCameraD3T::~CCameraD3T()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);


    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
}

// 判断相机 SDK 状态值是否需重试
bool CCameraD3T::getIsErrorNeedRetry(dvpStatus _stat_api)
{
    return !((DVP_STATUS_IGNORED == _stat_api));
}

// SDK 等初始化操作
stCameraStatInfo CCameraD3T::initCamera(int _find_dev_timeout)
{
    logDebug((QString(__PRETTY_FUNCTION__) + " into ..., isOn = %1").arg(Util::bool2str(isOn)), CGlobal::LOG_CAPTURE);

    //
    stCameraStatInfo stat_info(cameraStat_Succ);
    dvpStatus stat_api = DVP_STATUS_OK;
    QString msg;

    //
    if (isOn) {
        logDebug(QString(__PRETTY_FUNCTION__) + " : logic error, is on already!", CGlobal::LOG_CAPTURE);
        return stat_info;
    }

    //
    if (isIniting) {
        logDebug(QString(__PRETTY_FUNCTION__) + " : logic error, is initing!", CGlobal::LOG_CAPTURE);
        return stat_info;
    }

    //
    isIniting = true;

    //
    do
    {
        int camera_count = 0;
        dvpCameraInfo camera_info[4];

        // 刷新相机列表并得到相机数量
        QElapsedTimer tmr;
        tmr.start();
        do {
            dvpUint32 camera_count_out = 0;
            stat_api = dvpRefresh(&camera_count_out);
            camera_count = camera_count_out;
            if (camera_count > 0) {
                break;
            } else {
                logDebug("dvpRefresh() failed! waiting for retry ...", CGlobal::LOG_CAPTURE);
                QThread::msleep(500);
            }
        } while (tmr.elapsed() < _find_dev_timeout);

        if (0 == camera_count) {
            msg = QString("camera not found!");
            stat_api = DVP_STATUS_NO_DEVICE_FOUND;
            break;
        }

        // 枚举相机信息
        if (camera_count > 4) {
            camera_count = 4;
        }
        int camera_idx = -1;
        for (int i = 0; i < camera_count; i++) {
            stat_api = dvpEnum(i, &camera_info[i]);
            if (DVP_STATUS_OK != stat_api) {
                msg = QString("enum camera failed!");
                break;
            }

            const dvpCameraInfo &info = camera_info[i];

            //
            QString dev_info_str = QString("CCameraD3T::initCamera(): camera info: \n")
                     + "    Vendor                 : " + info.Vendor                 + "\n"    /* 设计厂商 */
                     + "    Manufacturer           : " + info.Manufacturer           + "\n"    /* 生产厂商 */
                     + "    Model                  : " + info.Model                  + "\n"    /* 型号 */
                     + "    Family                 : " + info.Family                 + "\n"    /* 系列 */
                     + "    LinkName               : " + info.LinkName               + "\n"    /* 连接名 */
                     + "    SensorInfo             : " + info.SensorInfo             + "\n"    /* 传感器描述 */
                     + "    HardwareVersion        : " + info.HardwareVersion        + "\n"    /* 硬件版本 */
                     + "    FirmwareVersion        : " + info.FirmwareVersion        + "\n"    /* 固件版本 */
                     + "    KernelVersion          : " + info.KernelVersion          + "\n"    /* 内核驱动版本 */
                     + "    DscamVersion           : " + info.DscamVersion           + "\n"    /* 设备驱动版本 */
                     + "    FriendlyName           : " + info.FriendlyName           + "\n"    /* 友好设备名称 */
                     + "    PortInfo               : " + info.PortInfo               + "\n"    /* 接口描述 */
                     + "    SerialNumber           : " + info.SerialNumber           + "\n"    /* 序列号 一般可以更改 */
                     + "    CameraInfo             : " + info.CameraInfo             + "\n"    /* 相机描述 */
                     + "    UserID                 : " + info.UserID                 + "\n"    /* 用户命名 */
                     + "    OriginalSerialNumber   : " + info.OriginalSerialNumber   + "\n"    /* 原始序列号 */
                     ;
            logInfo(dev_info_str);

            //
            if (QString(info.Model).contains(CAMERA_MODEL)) {
                camera_idx = i;
                break;
            }
        }

        //
        if (camera_idx < 0) {
            msg = "camera not found!";
            break;
        }

        // 打开相机
        stat_api = dvpOpen(camera_idx, OPEN_NORMAL, &gCameraHandle);
        if (DVP_STATUS_OK != stat_api) {
            msg = "open camera failed!";
            break;
        }

        // 恢复默认参数
        stat_api = dvpLoadDefault(gCameraHandle);
        if (DVP_STATUS_OK != stat_api) {
            msg = "load camera default params failed!";
            break;
        }

        // 设置分辨率 / ROI
        imgLeft     = IMG_ROI_LEFT;
        imgTop      = IMG_ROI_TOP;
        imgWidth    = IMG_ROI_WIDTH;
        imgHeight   = IMG_ROI_HEIGHT;

        // TODO: 在PC平台里的高度参数为什么必须要设成 16 的倍数才行，否则读图超时，而 demo 里不用？ARM平台好像不用，但可能有未发现的问题？
        // TODO: 在ARM平台里的第四个参数好像是末行的行号而不是高度？

        dvpRegion region;
        memset(&region, 0, sizeof(dvpRegion));

        region.X = imgLeft;
        region.Y = imgTop;
        region.W = imgWidth;
        region.H = imgHeight;

        //
        stat_api = dvpSetRoi(gCameraHandle, region);
        if (DVP_STATUS_OK != stat_api) {
            msg = "set roi failed!";
            break;
        }

        // 设置帧率
        dvpSelectionDescr sel_desc;
        stat_api  = dvpGetPixelRateSelDescr(gCameraHandle, &sel_desc);
        if (DVP_STATUS_OK != stat_api) {
            msg = "get speed rate descr failed!";
            break;
        }
        int rate_sel_count = sel_desc.uCount;
        dvpSelection sel;
        for (int i = 0; i < rate_sel_count; i++) {
            stat_api  = dvpGetPixelRateSelDetail(gCameraHandle, i, &sel);
            if (DVP_STATUS_OK != stat_api) {
                msg = "get speed rate detail failed!";
                break;
            }
            logDebug(QString::asprintf("Do3Think camera rate selection: %d, %s", i, sel.string), CGlobal::LOG_CAPTURE);
        }
        stat_api  = dvpSetPixelRateSel(gCameraHandle, 0);    // 设置高速率（高速率时行曝光间隔较小？）
        if (DVP_STATUS_OK != stat_api) {
            msg = "set speed rate failed!";
            break;
        }

        // 设置默认曝光值
        int expo = g_WinMeasure->exposureAdjuster()->defaultExposureUs();

        stat_info = setExposureTime(&expo);
        if (cameraStat_Succ != stat_info.cameraStat) {
            msg = "set exposure failed!";
            break;
        }

        // 设置默认模拟增益
        float gain;
        stat_api = dvpGetAnalogGain(gCameraHandle, &gain);
        logDebug(QString::asprintf("old gain = %.2f", gain), CGlobal::LOG_CAPTURE);
        if (DVP_STATUS_OK != stat_api) {
            msg = "get gain failed!";
            break;
        }

        analogGain = 2.0;       // NOTE: 此缺省值，来自2022年底刘宇的视筛新版的 python 原型程序的调试结果？
        stat_api = (setAnalogGain(analogGain) ? DVP_STATUS_OK : DVP_STATUS_FAILED);
        if (DVP_STATUS_OK != stat_api) {
            msg = "set speed rate failed!";
            break;
        }

        // 设置帧缓存配置
        dvpBufferConfig buff_conf;
        buff_conf.mode = BUFFER_MODE_NEWEST;            // 缓存模式：最新帧输出，旧帧将被覆盖
        buff_conf.uQueueSize = 30;                      // 帧缓存队列大小      // NOTE: 这里降低没有意义？应根据转灯图集大小来设置，避免转灯期间系统忙而增加丢帧率？但SDK和应用的实时性是同一等级？
        buff_conf.bDropNew = false;
        buff_conf.bLite = true;
        stat_api = dvpSetBufferConfig(gCameraHandle, buff_conf);
        logDebug(QString::asprintf("dvpSetBufferConfig() -> %d", stat_api), CGlobal::LOG_CAPTURE);
        if (DVP_STATUS_OK != stat_api) {
            logWarning("camera api error!", CGlobal::LOG_CAPTURE);
            msg = "set buffer config failed!";
            break;
        }

        // 设置帧缓存队列大小
        //setBufferQueueSize(1);
        // TODO: 前面已设置？

        // 开启触发使能（软触发和硬触发都需要开启）
        isAutoTriggerOpened = false;
        stat_api = dvpSetTriggerState(gCameraHandle, true);
        logDebug(QString::asprintf("dvpSetTriggerState(true) -> %d", stat_api), CGlobal::LOG_CAPTURE);
        if (DVP_STATUS_OK != stat_api) {
            logWarning("camera api error!", CGlobal::LOG_CAPTURE);
            msg = "set trigger state failed!";
            break;
        }

        // 触发输入类型：下降沿触发
        stat_info = setTriggerInputType(CGlobal::triggerInputType);
        if (cameraStat_Succ != stat_info.cameraStat) {
            logWarning("camera api error!", CGlobal::LOG_CAPTURE);
            msg = "set trigger input type failed!";
            break;
        }

        // 每次触发一帧
        stat_api = dvpSetFramesPerTrigger(gCameraHandle, 1);
        logDebug(QString::asprintf("dvpSetFramesPerTrigger(1) -> %d", stat_api), CGlobal::LOG_CAPTURE);
        if (DVP_STATUS_OK != stat_api) {
            logWarning("camera api error!", CGlobal::LOG_CAPTURE);
            msg = "set frames per trigger failed!";
            break;
        }

        // 设置触发信号源
        //stat_api = dvpSetTriggerSource(gCameraHandle, TRIGGER_SOURCE_SOFTWARE);
        /* 据厂家技术说法，触发源设为硬触发，软触发也是有效的，所以如果初始化为硬触发，不需再在软硬触发源间切换，减少这个 api 的调用，避免堵塞。 */
        stat_api = dvpSetTriggerSource(gCameraHandle, TRIGGER_SOURCE_LINE1);
        logDebug(QString::asprintf("dvpSetTriggerSource() -> %d", stat_api), CGlobal::LOG_CAPTURE);
        if (DVP_STATUS_OK != stat_api) {
            logWarning("camera api error!", CGlobal::LOG_CAPTURE);
            msg = "set trigger source failed!";
            break;
        }

        // 设置循环触发功能的使能状态 ？
        //stat_api = dvpSetSoftTriggerLoopState(gCameraHandle, false);
        //logDebug(QString::asprintf("dvpSetSoftTriggerLoopState() -> %d", stat_api), CGlobal::LOG_CAPTURE);
        //if (DVP_STATUS_OK != stat_api) {
        //    logWarning("camera api error!", CGlobal::LOG_CAPTURE);
        //    msg = "set SoftTriggerLoopState failed!";
        //    break;
        //}

        //stat = dvpSetInputIoFunction(gCameraHandle, INPUT_IO_1, INPUT_FUNCTION_TRIGGER);    // ？？
        // TODO: 触发延迟？触发过滤？触发使能情况下的“循环触发”是什么意思？


        // 注册事件回调：“重新连接”
        stat_api = dvpRegisterEventCallback(gCameraHandle, CCameraD3T::dvpEventCallback, EVENT_RECONNECTED, NULL);
        if (DVP_STATUS_OK != stat_api) {
            msg = "register EventCallback RECONNECTED failed!";
            break;
        }

        // 开始视频流
        stat_api = dvpStart(gCameraHandle);
        if (DVP_STATUS_OK != stat_api) {
            msg = "Start failed!";
            break;
        }

        // 暂停视频流
        //stat_api = dvpHold(gCameraHandle);
        //if (DVP_STATUS_OK != stat_api) {
        //    msg = "Start failed!";
        //    break;
        //}

        //
        stat_info.cameraStat = cameraStat_Succ;
    } while (false);

    if (!Util::CIntArray(3, DVP_STATUS_OK, DVP_STATUS_IGNORED, DVP_STATUS_IN_PROCESS).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    msg = msg + QString(" error=%1, ").arg(stat_api) + errStr(stat_api);
    stat_info.errMsg = msg;

    //
    if (cameraStat_Succ != stat_info.cameraStat) {
        logCritical(QString::asprintf("CCameraD3T::initCamera(): init failed! error_code : %d, ", stat_api) + msg);
    }

    //
    lastStatus = stat_info.cameraStat;
    isOn = (cameraStat_Succ == lastStatus);
    CCameraD3T::isDevUnableUse = isOn;

    //
    isIniting = false;

    //
    logDebug((QString(__PRETTY_FUNCTION__) + " ended, isOn = %1").arg(Util::bool2str(isOn)), CGlobal::LOG_CAPTURE);
    return stat_info;
}

// 反初始化
stCameraStatInfo CCameraD3T::uninitCamera(int _timeout)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info = disconnectCamera();

    isOn = false;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

int CCameraD3T::getImgWidth()
{
    return imgWidth;
}

int CCameraD3T::getImgHeight()
{
    return imgHeight;
}

stCameraStatInfo CCameraD3T::getImageBuffer(uchar **_img_data, int _timeout)
{
    return doGetImageBuffer(_img_data, _timeout);
}

stCameraStatInfo CCameraD3T::doGetImageBuffer(uchar **_img_data, int _timeout)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    /* 当前案例没有设置相机的曝光增益等参数，只展示在默认的ROI区域显示帧信息 */
    void *p = Q_NULLPTR;
    dvpStatus stat_api = dvpGetFrame(gCameraHandle, &gLastFrameInfo, &p, _timeout);
    if (DVP_STATUS_OK == stat_api) {
        *_img_data = (uchar *)p;
    } else if (DVP_STATUS_TIME_OUT == stat_api) {
        stat_info.cameraStat = cameraStat_Timeout;
    } else {
        logWarning(QString::asprintf("dvpGetFrame() failed! -> %d", stat_api), CGlobal::LOG_CAPTURE);
        //if (getIsErrorNeedRetry(stat_api))
        {
            // 失败重试
            for (int i = 0; i < RETRY_TIMES; i++) {
                logWarning(QString::asprintf("CCameraD3T::getImageBuffer(): camera err = %d, retry %d times", stat_api, i + 1), CGlobal::LOG_CAPTURE);
                QThread::msleep(RETRY_DELAY_MS);
                stat_api = dvpGetFrame(gCameraHandle, &gLastFrameInfo, &p, _timeout);
                if (DVP_STATUS_OK == stat_api) {
                    *_img_data = (uchar *)p;
                    break;
                } else if (DVP_STATUS_TIME_OUT == stat_api) {
                    stat_info.cameraStat = cameraStat_Timeout;
                    break;
                }
            }
            // 若还是失败，则返回“不可恢复错误”
            if (!Util::CIntArray(2, DVP_STATUS_OK, DVP_STATUS_TIME_OUT).contains(stat_api)) {
                logCritical("CCameraD3T::getImageBuffer(): retry failed! camera unable work continue!", CGlobal::LOG_CAPTURE);
                stat_info.cameraStat = cameraStat_Unrecoverable;
            }
        } /*else {              // TODO: 这其它错误，有没有临时性的，非不可恢复错误？
            logWarning("logic wrong ???");
            stat_info.cameraStat = cameraStat_Fail;
        }*/
    }

    // 显示帧数和帧率      // TODO: 这是干嘛的？
    //dvpFrameCount frame_count_info;
    //dvpStatus stat_api_count = dvpGetFrameCount(gCameraHandle, &frame_count_info);
    //if (DVP_STATUS_OK == stat_api_count) {
    //    //logWarning(printf("framecount: %d, framerate: %f\n", frame_count_info.uFrameCount, frame_count_info.fFrameRate);
    //
    //    // 显示帧信息
    //    //logWarning(printf("frame:%llu, timestamp:%llu, %d*%d, %dbytes, format:%d\r\n",
    //    //    gLastFrameInfo.uFrameID,
    //    //    gLastFrameInfo.uTimestamp,
    //    //    gLastFrameInfo.iWidth,
    //    //    gLastFrameInfo.iHeight,
    //    //    gLastFrameInfo.uBytes,
    //    //    gLastFrameInfo.format);
    //} else {
    //    //logWarning(printf("get framecount failed\n");
    //}

    //
    if (!Util::CIntArray(2, DVP_STATUS_OK, DVP_STATUS_TIME_OUT).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    lastStatus = stat_info.cameraStat;

    logDebug((QString(__PRETTY_FUNCTION__) + ": ended, api_ret = %1").arg(stat_api), CGlobal::LOG_CAPTURE);
    return stat_info;
}

stCameraStatInfo CCameraD3T::imageProcess(uchar *_img_in, uchar *_img_out)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    //stat_info.cameraStat = cameraStat_Fail;
    stat_info.cameraStat = cameraStat_Succ;

    memcpy(_img_out, _img_in, imgWidth * imgHeight);

    // TODO: 去掉？

    //if (DVP_STATUS_OK != stat_api && cameraStat_Succ == stat_info.cameraStat) {
    //    stat_info.cameraStat = cameraStat_Fail;
    //}

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

stCameraStatInfo CCameraD3T::releaseImageBuffer(uchar *_img_data)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    Q_UNUSED(_img_data)

    stCameraStatInfo stat_info(cameraStat_Succ);

    //stat_info.cameraStat = cameraStat_Fail;
    stat_info.cameraStat = cameraStat_Succ;

    // TODO: 去掉？

    //if (DVP_STATUS_OK != stat_api && cameraStat_Succ == stat_info.cameraStat) {
    //    stat_info.cameraStat = cameraStat_Fail;
    //}

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

stCameraStatInfo CCameraD3T::setExposureTime(int *_exposure_time)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    //dvpStop(gCameraHandle);

    double expo = *_exposure_time;
    dvpStatus stat_api = dvpSetExposure(gCameraHandle, expo);

    if (DVP_STATUS_OK == stat_api) {
        stat_api = dvpGetExposure(gCameraHandle, &expo);
        if (DVP_STATUS_OK == stat_api) {
            *_exposure_time = expo;
            exposureTime = expo;
        }
    } else if (getIsErrorNeedRetry(stat_api)) {
        logWarning(QString::asprintf("setExposureTime() failed! -> %d", stat_api), CGlobal::LOG_CAPTURE);
        // 失败重试
        for (int i = 0; i < RETRY_TIMES; i++) {
            logWarning(QString::asprintf("CCameraD3T::setExposureTime(): camera err = %d, retry %d times", stat_api, i + 1), CGlobal::LOG_CAPTURE);
            QThread::msleep(RETRY_DELAY_MS);
            stat_api = dvpGetExposure(gCameraHandle, &expo);
            if (DVP_STATUS_OK == stat_api) {
                *_exposure_time = expo;
                exposureTime = expo;
                break;
            }
        }
        // 若还是失败，则返回“不可恢复错误”
        if (DVP_STATUS_OK != stat_api) {
            logCritical("CCameraD3T::setExposureTime(): retry failed! camera unable work continue!", CGlobal::LOG_CAPTURE);
            stat_info.cameraStat = cameraStat_Unrecoverable;
        }
    }

    //dvpStart(gCameraHandle);

    if (!Util::CIntArray(2, DVP_STATUS_OK, DVP_STATUS_IGNORED).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

bool CCameraD3T::setAnalogGain(float _gain)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    enCameraStat ret = cameraStat_Fail;

    dvpStatus stat_api = dvpSetAnalogGain(gCameraHandle, _gain);
    if (DVP_STATUS_OK == stat_api) {
        analogGain = _gain;
        ret = cameraStat_Succ;
    }

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return (cameraStat_Succ == ret);
}

bool CCameraD3T::getContrast(int *_contrast)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    enCameraStat ret = cameraStat_Fail;

    dvpInt32 contrast;
    dvpStatus stat_api = dvpGetContrast(gCameraHandle, &contrast);

    if (DVP_STATUS_OK == stat_api) {
        *_contrast = (int)contrast;

        ret = cameraStat_Succ;
    }

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return (cameraStat_Succ == ret);
}

stCameraStatInfo CCameraD3T::resetParams()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    // TODO:

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

stCameraStatInfo CCameraD3T::disconnectCamera()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Fail);
    dvpStatus stat_api;

    do {
        // 反注册事件回调：“重新连接”
        stat_api = dvpUnregisterEventCallback(gCameraHandle, CCameraD3T::dvpEventCallback, EVENT_RECONNECTED, NULL);

        stat_api = dvpStop(gCameraHandle);

        stat_api = dvpClose(gCameraHandle);

    } while (false);

    //
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;
    }

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

// 相机 API 事件回调函数
dvpInt32 CCameraD3T::dvpEventCallback(dvpHandle handle, dvpEvent event, void *pContext, dvpInt32 param, dvpVariant *pVariant)
{
    logDebug((QString(__PRETTY_FUNCTION__) + ": into ..., event = %1").arg((int)event), CGlobal::LOG_CAPTURE);

    static Util::CIntArray dev_ok_events(4,
                                         EVENT_CONNECTED,           /* 连接成功 */
                                         EVENT_RECONNECTED,         /* 重新连接 */
                                         EVENT_STREAM_STARTRD,      /* 数据流已经启动 */
                                         EVENT_FRAME_START          /* 帧开始传输 */
                                         );
    static Util::CIntArray dev_bad_events(1,
                                          EVENT_LOST_CONNECTION     /* 失去连接 */
                                          );

    Q_UNUSED(handle)
    Q_UNUSED(pContext)
    Q_UNUSED(param)
    Q_UNUSED(pVariant)

    if (dev_ok_events.contains(event)) {
        CCameraD3T::isDevUnableUse = false;
    } else if (dev_bad_events.contains(event)) {
        CCameraD3T::isDevUnableUse = true;
    }

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return 0;
}

stCameraStatInfo CCameraD3T::setTriggerMode(enCameraTriggerMode _trigger_mode)
{
    logDebug((QString(__PRETTY_FUNCTION__) + ": into ..., trigger_mode = %1").arg((int)_trigger_mode), CGlobal::LOG_CAPTURE);

    // TODO: clear buffer 应该放到此处？软换硬，或者硬换软，都应该 clear ？

    stCameraStatInfo stat_info(cameraStat_Succ);

    // 参数检查
    if (_trigger_mode < cameraTriggerMode_Min || _trigger_mode > cameraTriggerMode_Max) {
        logWarning("CCameraD3T::setTriggerMode(): out of range!", CGlobal::LOG_CAPTURE);
        stat_info.cameraStat = cameraStat_ParamInvalid;
        return stat_info;
    }

    //
    dvpStatus stat_api = doSetTriggerMode(_trigger_mode);
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;
    } else if (getIsErrorNeedRetry(stat_api)) {
        logWarning(QString::asprintf("setTriggerMode() failed! -> %d", stat_api), CGlobal::LOG_CAPTURE);
        // 失败重试
        for (int i = 0; i < RETRY_TIMES; i++) {
            logWarning(QString::asprintf("CCameraD3T::setTriggerMode(): camera err = %d, retry %d times", stat_api, i + 1), CGlobal::LOG_CAPTURE);
            QThread::msleep(RETRY_DELAY_MS);
            stat_api = doSetTriggerMode(_trigger_mode);
            if (DVP_STATUS_OK == stat_api) {
                stat_info.cameraStat = cameraStat_Succ;
                break;
            }
        }
        // 若还是失败，则返回“不可恢复错误”
        if (DVP_STATUS_OK != stat_api) {
            logCritical("CCameraD3T::setTriggerMode(): retry failed! camera unable work continue!", CGlobal::LOG_CAPTURE);
            stat_info.cameraStat = cameraStat_Unrecoverable;
        }
    }

    if (!Util::CIntArray(3, DVP_STATUS_OK, DVP_STATUS_IGNORED, DVP_STATUS_IN_PROCESS).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

stCameraStatInfo CCameraD3T::setTriggerDelayUs(int _delay_us)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, _delay_us = " << _delay_us;

    stCameraStatInfo stat_info(cameraStat_Succ);

    // 设置触发延时
    //qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): calling dvpSetTriggerDelay() ...";
    dvpStatus stat_api = dvpSetTriggerDelay(gCameraHandle, _delay_us);        // NOTE: 度申相机的触发延时设置对软硬触发都生效
    //qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): dvpSetTriggerDelay() called";
    logDebug(QString::asprintf("dvpSetTriggerDelay() -> %d", stat_api), CGlobal::LOG_CAPTURE);
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;
    } else {
        stat_info.cameraStat = cameraStat_Fail;
        stat_info.errMsg = "set trigger delay failed!";
        logWarning(QString("%1::%2(): %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(stat_info.errMsg), CGlobal::LOG_CAPTURE);
    }
    stat_info.errCode = stat_api;

    //
    lastStatus = stat_info.cameraStat;

    //
    return stat_info;
}

stCameraStatInfo CCameraD3T::setTriggerInputType(enTriggerInputType _type)
{
    stCameraStatInfo stat_info(cameraStat_Succ);
    dvpStatus stat_api = DVP_STATUS_OK;

    dvpTriggerInputType trigger_type = convertTriggerInputType(_type);
    stat_api = dvpSetTriggerInputType(gCameraHandle, trigger_type);
    logDebug(QString::asprintf("dvpSetTriggerInputType() -> %d", stat_api), CGlobal::LOG_CAPTURE);
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;
    } else {
        stat_info.cameraStat = cameraStat_Fail;
        stat_info.errMsg = "dvpSetTriggerInputType() failed!";
        logWarning(QString("%1::%2(): %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(stat_info.errMsg), CGlobal::LOG_CAPTURE);
    }
    stat_info.errCode = stat_api;

    //
    lastStatus = stat_info.cameraStat;

    //
    return stat_info;
}

stCameraStatInfo CCameraD3T::getTriggerInputType(enTriggerInputType &_type)
{
    stCameraStatInfo stat_info(cameraStat_Succ);
    dvpStatus stat_api = DVP_STATUS_OK;

    dvpTriggerInputType trigger_type;
    stat_api = dvpGetTriggerInputType(gCameraHandle, &trigger_type);
    logDebug(QString::asprintf("dvpGetTriggerInputType() -> %d", stat_api), CGlobal::LOG_CAPTURE);
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;

        //
        _type = convertTriggerInputType(trigger_type);
    } else {
        stat_info.cameraStat = cameraStat_Fail;
        stat_info.errMsg = "dvpGetTriggerInputType() failed!";
        logWarning(QString("%1::%2(): %3").arg(S_CLASS_NAME).arg(__FUNCTION__).arg(stat_info.errMsg), CGlobal::LOG_CAPTURE);
    }
    stat_info.errCode = stat_api;

    //
    lastStatus = stat_info.cameraStat;

    //
    return stat_info;
}

dvpStatus CCameraD3T::doSetTriggerMode(enCameraTriggerMode _trigger_mode)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    //
    dvpStatus stat_api = DVP_STATUS_OK;

    do {
        //
        if (cameraTriggerMode_Auto == _trigger_mode) {
            //pause();

            // 关闭触发使能
            if (isAutoTriggerOpened) {
                stat_api = dvpSetTriggerState(gCameraHandle, false);
                logDebug(QString::asprintf("dvpSetTriggerState(false) -> %d", stat_api), CGlobal::LOG_CAPTURE);
                if (DVP_STATUS_OK == stat_api) {
                    isAutoTriggerOpened = false;
                } else {
                    logWarning("camera api error!", CGlobal::LOG_CAPTURE);
                    break;
                }
            } else {
                logDebug("dvpSetTriggerState() is skipped, isAutoTriggerOpened is already false.", CGlobal::LOG_CAPTURE);
            }

            //restart();
        } else if (cameraTriggerMode_Soft == _trigger_mode || cameraTriggerMode_Hard == _trigger_mode) {
            //pause();        // TODO: 这个暂停之后重开启，好像有没有都可以？

            // 开启触发使能
            if (!isAutoTriggerOpened) {
                stat_api = dvpSetTriggerState(gCameraHandle, true);
                logDebug(QString::asprintf("dvpSetTriggerState(true) -> %d", stat_api), CGlobal::LOG_CAPTURE);
                if (DVP_STATUS_OK == stat_api) {
                    isAutoTriggerOpened = true;
                } else {
                    logWarning("camera api error!", CGlobal::LOG_CAPTURE);
                    break;
                }
            } else {
                logDebug("dvpSetTriggerState() is skipped, isAutoTriggerOpened is already true", CGlobal::LOG_CAPTURE);
            }

            // 设置触发源
            //dvpTriggerSource trigger_mode = TRIGGER_MODE_INTF_TO_SDK[(int)_trigger_mode];
            //stat_api = dvpSetTriggerSource(gCameraHandle, trigger_mode);
            //logDebug(QString::asprintf("dvpSetTriggerSource() -> %d", stat_api), CGlobal::LOG_CAPTURE);
            //if (DVP_STATUS_OK != stat_api) {
            //    logWarning("camera api error!", CGlobal::LOG_CAPTURE);
            //    break;
            //}
            /* 据厂家技术说法，触发源设为硬触发，软触发也是有效的，所以如果初始化为硬触发，不需再在软硬触发源间切换，减少这个 api 的调用，避免堵塞。 */

            //restart();
        }
    } while (false);

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_api;
}

dvpTriggerInputType CCameraD3T::convertTriggerInputType(enTriggerInputType _type)
{
    switch (_type) {
    case enTriggerInputType::FallingEdge    : return TRIGGER_NEG_EDGE;
    case enTriggerInputType::RisingEdge     : return TRIGGER_POS_EDGE;
    default                                 : return (dvpTriggerInputType)-1;
    }
}

enTriggerInputType CCameraD3T::convertTriggerInputType(dvpTriggerInputType _type)
{
    switch (_type) {
    case TRIGGER_NEG_EDGE   : return enTriggerInputType::FallingEdge;
    case TRIGGER_POS_EDGE   : return enTriggerInputType::RisingEdge ;
    default                 : return enTriggerInputType::Unknown;
    }
}

float CCameraD3T::getAnalogGain()
{
    return analogGain;
}

int CCameraD3T::getExposureTime()
{
    return exposureTime;
}

stCameraStatInfo CCameraD3T::softTrigger()
{
    return doSoftTrigger();
}

stCameraStatInfo CCameraD3T::doSoftTrigger()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    dvpStatus stat_api = dvpTriggerFire(gCameraHandle);
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;
    } else if (getIsErrorNeedRetry(stat_api)) {
        logWarning(QString::asprintf("dvpTriggerFire() failed! -> %d", stat_api), CGlobal::LOG_CAPTURE);
        // 失败重试
        for (int i = 0; i < RETRY_TIMES; i++) {
            logWarning(QString::asprintf("CCameraD3T::softTrigger(): camera err = %d, retry %d times", stat_api, i + 1), CGlobal::LOG_CAPTURE);
            QThread::msleep(RETRY_DELAY_MS);
            stat_api = dvpTriggerFire(gCameraHandle);
            if (DVP_STATUS_OK == stat_api) {
                stat_info.cameraStat = cameraStat_Succ;
                break;
            }
        }
        // 若还是失败，则返回“不可恢复错误”
        if (DVP_STATUS_OK != stat_api) {
            logCritical("CCameraD3T::getImageBuffer(): retry failed! camera unable work continue!", CGlobal::LOG_CAPTURE);
            stat_info.cameraStat = cameraStat_Unrecoverable;
        }
    }

    if (!Util::CIntArray(1, DVP_STATUS_OK).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

stCameraStatInfo CCameraD3T::play()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    logDebug("CCameraD3T::play() -> dvpStart() ...", CGlobal::LOG_CAPTURE);
    dvpStatus stat_api = dvpStart(gCameraHandle);

    if (!Util::CIntArray(3, DVP_STATUS_OK, DVP_STATUS_IGNORED, DVP_STATUS_IN_PROCESS).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

stCameraStatInfo CCameraD3T::stop()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    dvpStatus stat_api = dvpStop(gCameraHandle);
    //logDebug(QString("dvpStop() -> %1").arg(stat_api), CGlobal::LOG_CAPTURE);
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;
    } else if (getIsErrorNeedRetry(stat_api)) {
        logWarning(QString::asprintf("dvpStop() failed! -> %d", stat_api), CGlobal::LOG_CAPTURE);
        // 失败重试
        for (int i = 0; i < RETRY_TIMES; i++) {
            logWarning(QString::asprintf("CCameraD3T::stop(): camera err = %d, retry %d times", stat_api, i + 1), CGlobal::LOG_CAPTURE);
            QThread::msleep(RETRY_DELAY_MS);
            stat_api = dvpStop(gCameraHandle);
            if (DVP_STATUS_OK == stat_api) {
                stat_info.cameraStat = cameraStat_Succ;
                break;
            }
        }
        // 若还是失败，则返回“不可恢复错误”
        if (DVP_STATUS_OK != stat_api) {
            logCritical("CCameraD3T::stop(): retry failed! camera unable work continue!", CGlobal::LOG_CAPTURE);
            stat_info.cameraStat = cameraStat_Unrecoverable;
        }
    }

    if (!Util::CIntArray(3, DVP_STATUS_OK, DVP_STATUS_IGNORED, DVP_STATUS_IN_PROCESS).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

// 暂停视频流
stCameraStatInfo CCameraD3T::pause()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    logDebug("CCameraD3T::pause() -> dvpHold() ...", CGlobal::LOG_CAPTURE);
    dvpStatus stat_api = dvpHold(gCameraHandle);
    //logDebug(QString::asprintf("dvpHold() -> %d", stat_api), CGlobal::LOG_CAPTURE);
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;
    } else if (getIsErrorNeedRetry(stat_api)) {
        logWarning(QString::asprintf("dvpHold() failed! -> %d", stat_api), CGlobal::LOG_CAPTURE);
        // 失败重试
        for (int i = 0; i < RETRY_TIMES; i++) {
            logWarning(QString::asprintf("CCameraD3T::pause(): camera err = %d, retry %d times", stat_api, i + 1), CGlobal::LOG_CAPTURE);
            QThread::msleep(RETRY_DELAY_MS);
            stat_api = dvpHold(gCameraHandle);
            if (DVP_STATUS_OK == stat_api) {
                stat_info.cameraStat = cameraStat_Succ;
                break;
            }
        }
        // 若还是失败，则返回“不可恢复错误”
        if (DVP_STATUS_OK != stat_api) {
            logCritical("CCameraD3T::pause(): retry failed! camera unable work continue!", CGlobal::LOG_CAPTURE);
            stat_info.cameraStat = cameraStat_Unrecoverable;
        }
    }

    if (!Util::CIntArray(3, DVP_STATUS_OK, DVP_STATUS_IGNORED, DVP_STATUS_IN_PROCESS).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

// 重启视频流
stCameraStatInfo CCameraD3T::restart()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    stCameraStatInfo stat_info(cameraStat_Succ);

    logDebug("CCameraD3T::restart() -> dvpRestart() ...", CGlobal::LOG_CAPTURE);
    dvpStatus stat_api = dvpRestart(gCameraHandle);
    logDebug(QString::asprintf("dvpRestart() -> %d", stat_api), CGlobal::LOG_CAPTURE);
    if (DVP_STATUS_OK == stat_api) {
        stat_info.cameraStat = cameraStat_Succ;
    } else if (getIsErrorNeedRetry(stat_api)) {
        logWarning(QString::asprintf("dvpRestart() failed! -> %d", stat_api), CGlobal::LOG_CAPTURE);
        // 失败重试
        for (int i = 0; i < RETRY_TIMES; i++) {
            logWarning(QString::asprintf("CCameraD3T::restart(): camera err = %d, retry %d times", stat_api, i + 1), CGlobal::LOG_CAPTURE);
            QThread::msleep(RETRY_DELAY_MS);
            stat_api = dvpRestart(gCameraHandle);
            if (DVP_STATUS_OK == stat_api) {
                stat_info.cameraStat = cameraStat_Succ;
                break;
            }
        }
        // 若还是失败，则返回“不可恢复错误”
        if (DVP_STATUS_OK != stat_api) {
            logCritical("CCameraD3T::restart(): retry failed! camera unable work continue!", CGlobal::LOG_CAPTURE);
            stat_info.cameraStat = cameraStat_Unrecoverable;
        }
    }

    if (!Util::CIntArray(2, DVP_STATUS_OK, DVP_STATUS_IGNORED).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
        stat_info.cameraStat = cameraStat_Fail;
    }
    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

// 清空帧缓存
stCameraStatInfo CCameraD3T::clearFrameBuffer(const int _timeout_ms, int &_count_cleared)
{
    //
    static constexpr int DEFAULT_TIMEOUT_MS = 50;       // 缺省超时（ms）

    //
    int timeout_ms = _timeout_ms;
    if (timeout_ms <= 0) {
        timeout_ms = DEFAULT_TIMEOUT_MS;
    }

    //
    return doClearFrameBuffer(timeout_ms, _count_cleared);

    //_count_cleared = -1;
    //return doClearFrameBufferByApi();
}

stCameraStatInfo CCameraD3T::setBufferQueueSize(int _size)
{
    //
    stCameraStatInfo stat_info(cameraStat_Succ);

    //
    dvpStatus stat_api = dvpSetBufferQueueSize(gCameraHandle, _size);
    if (DVP_STATUS_OK != stat_api) {
        stat_info.errCode = stat_api;
        stat_info.errMsg = QString("Failed to setBufferQueueSize() of camera to %1!").arg(_size);
    }

    //
    return stat_info;
}

stCameraStatInfo CCameraD3T::doClearFrameBufferByApi()
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    //
    stCameraStatInfo stat_info(cameraStat_Succ);

    dvpStatus stat_api = dvpSet(gCameraHandle, DSCAM_PARAM_BUFFER_CLEAR, NULL, NULL);
    if (DVP_STATUS_OK != stat_api) {
        stat_info.errCode = stat_api;
        stat_info.errMsg = "call buffer clear failed";
    }

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

stCameraStatInfo CCameraD3T::doClearFrameBuffer(const int _timeout_ms, int &_count_cleared)
{
    logDebug(QString(__PRETTY_FUNCTION__) + ": into ...", CGlobal::LOG_CAPTURE);

    //static const int TIME_OUT_COUNT_CLEAR_SOFT = 2;     // 清空软触发帧缓存的取图超时次数
    static const int TIME_OUT_COUNT_CLEAR_SOFT = 1;
    static const int MAX_LOOP_TIMES = 50;               // 最大循环次数       // TODO: 有没死循环风险？

    //
    stCameraStatInfo stat_info(cameraStat_Succ);

    //
    void *p;
    dvpStatus stat_api = DVP_STATUS_FAILED;

    int count_cleared = 0;
    int count_timeout = 0;
    int count_loop = 0;
    while (true) {
        count_loop++;
        if (count_loop > MAX_LOOP_TIMES) {
            qCritical() << QString::asprintf("clear frame buffer loop too much times( %d ), abnormal!", count_loop);
            break;
        }

        stat_api = dvpGetFrame(gCameraHandle, &gLastFrameInfo, &p, _timeout_ms);
        if (DVP_STATUS_OK == stat_api) {
            count_cleared++;
        } else if (DVP_STATUS_TIME_OUT == stat_api) {
            count_timeout++;
        } else if (DVP_STATUS_NOT_STARTED == stat_api) {    // NOTE: 若未开始，不需清除缓存
            stat_info.cameraStat = cameraStat_Succ;
            break;
        } else if (getIsErrorNeedRetry(stat_api)) {
            // 失败重试
            for (int i = 0; i < RETRY_TIMES; i++) {
                logWarning(QString::asprintf("CCameraD3T::clearFrameBuffer(): camera err = %d, retry %d times, count_cleared = %d, count_timeout = %d",
                                             stat_api, i + 1, count_cleared, count_timeout), CGlobal::LOG_CAPTURE);
                QThread::msleep(RETRY_DELAY_MS);
                stat_api = dvpGetFrame(gCameraHandle, &gLastFrameInfo, &p, _timeout_ms);
                if (DVP_STATUS_OK == stat_api) {
                    count_cleared++;
                    break;
                } else if (DVP_STATUS_TIME_OUT != stat_api) {
                    count_timeout++;
                    break;
                }
            }
            // 若还是失败，则返回“不可恢复错误”
            if (!Util::CIntArray(2, DVP_STATUS_OK, DVP_STATUS_TIME_OUT).contains(stat_api)) {
                logCritical("CCameraD3T::clearFrameBuffer(): retry failed! camera unable work continue!", CGlobal::LOG_CAPTURE);
                stat_info.cameraStat = cameraStat_Unrecoverable;
            }
        }

        //
        if (count_timeout >= TIME_OUT_COUNT_CLEAR_SOFT) {
            stat_info.cameraStat = cameraStat_Succ;
            break;
        }
    }
    logDebug(QString::asprintf("CCameraD3T::clearFrameBuffer(): drop %d frames.", count_cleared), CGlobal::LOG_CAPTURE);

    //
    //if (!Util::CIntArray(4, DVP_STATUS_OK, DVP_STATUS_TIME_OUT, DVP_STATUS_IGNORED, DVP_STATUS_NOT_STARTED).contains(stat_api) && cameraStat_Succ == stat_info.cameraStat) {
    //    stat_info.cameraStat = cameraStat_Fail;
    //}

    stat_info.errCode = stat_api;
    //stat_info.errMsg = ;

    //
    _count_cleared = count_cleared;

    //
    lastStatus = stat_info.cameraStat;

    logDebug(QString(__PRETTY_FUNCTION__) + ": ended", CGlobal::LOG_CAPTURE);
    return stat_info;
}

// 相机 API 错误码 转 描述
QString errStr(dvpStatus _err_id)
{
    QString err_str;

    switch (_err_id) {
    case DVP_STATUS_TIME_OUT:
        err_str = "timeout";
        break;
    case DVP_STATUS_DEVICE_IS_DISCONNECTED:
        err_str = "disconnected";
        break;
    default:
        break;
    }

    //
    return err_str;
}

#endif
