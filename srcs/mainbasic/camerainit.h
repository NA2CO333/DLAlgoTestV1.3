#ifndef CAMERAINITTHREAD_H
#define CAMERAINITTHREAD_H

#include <QObject>
#include <QThread>
#include <QRect>

#include "CameraIntf.h"

/* 注意：opencv 的图像数据步宽必须是 4 的整数倍，所以如果这里的图像宽度不是 4 的整数倍，可能存在兼容问题。 */

#if (SCREEN_SIZE_TYPE == 1)
// 屏幕尺寸     // TODO: 移到全局模块
#  define SCREEN_WIDTH          800
#  define SCREEN_HEIGHT         480
// 状态栏高度
#  define STATUSBAR_HEIGHT      30
// 窗体中可绘帧区域的高度
#  define IMG_DRAWING_HEIGHT    324
#else
// 屏幕尺寸
#  define SCREEN_WIDTH          1280
#  define SCREEN_HEIGHT         720
// 状态栏高度
#  define STATUSBAR_HEIGHT      45
// 窗体中可绘帧区域的高度
#  define IMG_DRAWING_HEIGHT    486
#endif

#if (CAMERA_TYPE == 1)
// 相机图像整图尺寸
#  define IMG_WIDTH_WHOLE       752
#  define IMG_HEIGHT_WHOLE      480
// 相机图像 ROI 区域      /* left, top, width, height 都要精确到 16 的倍数 */
#  define IMG_ROI_LEFT          0
#  define IMG_ROI_TOP           32
#  define IMG_ROI_WIDTH         IMG_WIDTH_WHOLE
#  define IMG_ROI_HEIGHT        336
#else
// 相机图像整图尺寸
#  define IMG_WIDTH_WHOLE       1280
#  define IMG_HEIGHT_WHOLE      1024
// 相机图像 ROI 区域      /* left, top, width, height 都要精确到 16 的倍数 */
#  define IMG_ROI_WIDTH         IMG_WIDTH_WHOLE
#  define IMG_ROI_HEIGHT        512
#  define IMG_ROI_LEFT          0
#  define IMG_ROI_TOP           (IMG_HEIGHT_WHOLE - IMG_ROI_HEIGHT) / 2
#endif

// 实际获得的相机图像的尺寸
#define IMG_WIDTH           IMG_ROI_WIDTH
#define IMG_HEIGHT          IMG_ROI_HEIGHT

//
class CameraInitThread : public QThread
{
    Q_OBJECT
public:
    //CameraInit();
    explicit CameraInitThread(QObject *parent = 0);
    void run();

    static void cameraTurnOff(bool _is_wait = false);
    static bool cameraTurnOn();
    static bool cameraRePowerOn();

signals:
    void sigCameraInitFinished(enCameraStat _status, QString _msg);

};

#ifdef SURPORT_FRAME_BUFFER
//
extern char *fbp ;
extern int devfb, filefb;
extern struct fb_var_screeninfo scrinfo;
extern unsigned long screensize, one_screensize;

void show_framebuffer_0(int fd, struct fb_var_screeninfo scrinfo);
void show_framebuffer_1(int fd, struct fb_var_screeninfo scrinfo);
void show_framebuffer_2(int fd, struct fb_var_screeninfo scrinfo);
#endif

#endif // CAMERAINITTHREAD_H
