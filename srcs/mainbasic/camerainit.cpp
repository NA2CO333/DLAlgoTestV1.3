//相机初始化,SDK初始化，图片信息等，改动需谨慎

#include "camerainit.h"

#include <sys/prctl.h>

#include <QDebug>
#include <QThread>
#include <QTime>

#include "CameraIntf.h"

#include "noticewin.h"
#include "messagewin.h"
#include "global.h"
#include "capturethread.h"
#include "windowsmanager.h"
#include "hardware.h"

#ifdef SURPORT_FRAME_BUFFER
#  include <linux/mxcfb.h>
#  include "lcd.h"

struct fb_dev_info fb_info;
char *fbp ;
int devfb, filefb;
struct fb_var_screeninfo scrinfo;
#endif

unsigned long screensize, one_screensize;
char bmpname[20] = {0};

//
int g_SaveParameter_num=0;  //保存参数组
int g_read_fps=0;           //统计读取帧率
int g_disply_fps=0;         //统计显示帧率

#ifdef SURPORT_FRAME_BUFFER
//Add by sxh
void show_framebuffer_2(int fd, struct fb_var_screeninfo scrinfo)
{
    scrinfo.yoffset = scrinfo.yres * 2;
    if (ioctl(fd, FBIOPAN_DISPLAY, &scrinfo) < 0) {
        printf("ioctl FBIOPAN_DISPLAY\n");
    }
}

void show_framebuffer_1(int fd, struct fb_var_screeninfo scrinfo)
{
    scrinfo.yoffset = scrinfo.yres;
    if (ioctl(fd, FBIOPAN_DISPLAY, &scrinfo) < 0) {
        printf("ioctl FBIOPAN_DISPLAY\n");
    }
}

void show_framebuffer_0(int fd, struct fb_var_screeninfo scrinfo)
{
    scrinfo.yoffset = 0;
    if (ioctl(fd, FBIOPAN_DISPLAY, &scrinfo) < 0) {
        printf("ioctl FBIOPAN_DISPLAY\n");
    }
}

static int open_fb(const char* name, struct fb_var_screeninfo* scrinfo)
{
    int devfb;
    int retval;
//    struct mxcfb_gbl_alpha gbl_alpha;
//    struct mxcfb_color_key key;

    devfb = open(name, O_RDWR);
    if(!devfb) {
        printf("devfb open error!\r\n");
        return -1;
    }

    if(ioctl(devfb, FBIOGET_VSCREENINFO, scrinfo)) {
        printf("get screen infomation error!\r\n");
        return -1;
    }

    //printf("open_fb, sensor_width = %d, sensor_height = %d\r\n", gCameraFrameInfo.sensor_width, gCameraFrameInfo.sensor_height);
    scrinfo->bits_per_pixel = 24;
    //scrinfo->xres = gCameraFrameInfo.sensor_width;
    //scrinfo->yres = gCameraFrameInfo.sensor_height;
    //scrinfo->yres_virtual = gCameraFrameInfo.sensor_height * 3;
    scrinfo->xres = SCREEN_WIDTH;
    scrinfo->yres = SCREEN_HEIGHT;
    scrinfo->yres_virtual = SCREEN_HEIGHT * 3;

    retval = ioctl(devfb, FBIOPUT_VSCREENINFO, scrinfo);
    if (retval < 0) {
        printf("set screen infomation error!\r\n");
        return -1;
    }

    if(ioctl(devfb, FBIOGET_VSCREENINFO, scrinfo)) {
        printf("get screen infomation error!\r\n");
        return -1;
    }

//沈工 去掉开机屏幕虚影20180723
//    retval = ioctl(devfb, FBIOBLANK, FB_BLANK_UNBLANK);
//    if (retval < 0) {
//        printf("set screen FBIOBLANK error!\r\n");
//        return -1;
//    }
/*
    gbl_alpha.alpha = 192;
    gbl_alpha.enable = 1;
    retval = ioctl(devfb, MXCFB_SET_GBL_ALPHA, &gbl_alpha);
    if (retval < 0) {
        printf("set MXCFB_SET_GBL_ALPHA error!\r\n");
        return -1;
    }

    key.enable = 1;
    key.color_key = 0;
    retval = ioctl(devfb, MXCFB_SET_CLR_KEY, &key);
    if (retval < 0) {
        printf("set MXCFB_SET_CLR_KEY error!\r\n");
        return -1;
    }
*/
    return devfb;
}

static int init_fb(void)
{
    //printf("init_fb, sensor_width = %d, sensor_height = %d\r\n", gCameraFrameInfo.sensor_width, gCameraFrameInfo.sensor_height);
    devfb = open_fb("/dev/fb1", &scrinfo);
    if(!devfb) {
        printf("devfb open error!\r\n");
        return -1;
    }
    //printf(".xres=%d, .yres=%d, .bit=%d\r\n",scrinfo.xres, scrinfo.yres, scrinfo.bits_per_pixel);

    //printf(".xres_virtual=%d, .yres_virtual=%d\r\n",scrinfo.xres_virtual, scrinfo.yres_virtual);

    if(24 == scrinfo.bits_per_pixel) {
        printf("screen infomation.bits %d!\r\n", scrinfo.bits_per_pixel);

    } else if(32 == scrinfo.bits_per_pixel) {
        printf("screen infomation.bits %d!\r\n", scrinfo.bits_per_pixel);
    } else {
        return -1;
    }

    //计算需要的映射内存大小
    screensize = scrinfo.xres * scrinfo.yres_virtual * scrinfo.bits_per_pixel / 8;
    printf("screensize=%lu!r\n", screensize);
    one_screensize = scrinfo.xres * scrinfo.yres * scrinfo.bits_per_pixel / 8;

    //内存映射
    fbp = (char *)mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, devfb, 0);
    if(-1 == (int)fbp) {
        printf("mmap error!\r\n");
        return -1;
    }

}
//Add end
#endif

CameraInitThread::CameraInitThread(QObject *parent) :
    QThread(parent)
{

}

void CameraInitThread::run()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_CAPTURE);

    // 设置线程名称
    prctl(PR_SET_NAME, "CameraInitThread", nullptr, nullptr, nullptr);

    //
#ifdef SURPORT_FRAME_BUFFER
        if (-1 == init_fb())        // TODO: 这个不应该放这里
            printf("init fb1 failed\r\n");
#endif

    //QTime initstart = QTime::currentTime();
    stCameraStatInfo stat_info = g_CameraIntf->initCamera(10000);

    emit sigCameraInitFinished(stat_info.cameraStat, stat_info.errMsg);

    //int usedTime = initstart.msecsTo(QTime::currentTime());
    //qDebug()<<"usedTime = "<<usedTime;

    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_CAPTURE);
}

void CameraInitThread::cameraTurnOff(bool _is_wait)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_CAPTURE);

    // 相机反初始化
    g_CameraIntf->uninitCamera();

    // 相机断电
    CHardware::cameraPowerOff();

    // 断电等待     // TODO: USB 通信故障时，这个等待时间可能更长？
    if (_is_wait) {
        Util::waitMs(3000);
    }

    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_CAPTURE);
}

bool CameraInitThread::cameraTurnOn()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_CAPTURE);

    // 相机上电
    CHardware::cameraPowerOn();

    // 上电等待
    //Util::waitMs(2000);       /* 这里好像不需延时，因为后面相机初始化时会循环查找相机 */

    // 相机初始化
    stCameraStatInfo camera_stat(cameraStat_Succ);
    if (!g_CameraIntf->getIsOn()) {
        camera_stat = g_CameraIntf->initCamera(5000);
    }

    //
    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_CAPTURE);
    return (cameraStat_Succ == camera_stat.cameraStat);
}

// 相机重上电    /* 注意：若相机初始化中，应先等待（目前开机时的初始化是在非主线程执行的） */
bool CameraInitThread::cameraRePowerOn()
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_CAPTURE);

    // 关闭相机
    cameraTurnOff();

    // 断电等待     // TODO: USB 通信故障时，这个等待时间可能更长？
    Util::waitMs(3000);

    // 打开相机
    bool succ = cameraTurnOn();

    //
    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_CAPTURE);
    return succ;
}

