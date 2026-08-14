#ifndef VIRTUALINTERFACE_H
#define VIRTUALINTERFACE_H

#include <vector>

#include <QObject>
#include <QVector>
#include <QVariant>

#include "opencv2/core/core.hpp"

//using namespace std;
//using namespace cv;

//using std::string;
//using std::vector;
//using cv::Mat;
//using cv::Rect;

//
typedef unsigned char BYTE;

// 光路类型
enum enOpticalPath {
    opticalPath_Straight,       // 直面
    opticalPath_Controlled,     // 受控       // TODO: 旧代码混到一起，但它不属于光路类型，待分离出来
    opticalPath_Square,         // 方形
    //opticalPath_LShape,         // L形
};

extern std::vector<BYTE*> resultByte;
extern enOpticalPath VersionTypeNum;
extern QString currentID;
extern struct Patients currentPat[1];
extern int pusAnalog;
extern float g_DistanceVal;
extern bool saveImage;
extern int g_hCamera;
extern bool pdfState;
extern unsigned char *g_saturationBuf;
extern bool hmMode;
extern int ageStage;
extern int serailport_status;
extern unsigned char *g_computeBuf;
extern int showAge;
extern int Screen_model;
extern bool SaveFaultImageFlag;
extern unsigned int gCurrentDistance;

//
#if (CAMERA_TYPE == 1)
#  define IMG_WIDTH       752
//#  define IMG_HEIGHT      480
#  define IMG_HEIGHT      336
#elif (CAMERA_TYPE == 2)
#  ifndef SPECIAL_ROI_SIZE
#    define IMG_WIDTH       1280
#    define IMG_HEIGHT      512
#  else
#    define IMG_WIDTH       1152
#    define IMG_HEIGHT      480
#  endif
#endif

//#if (SCREEN_SIZE_TYPE == 1)
//#  define SCREEN_WIDTH    800
//#else
//#  define SCREEN_WIDTH    1280
//#endif

//
class virtualInterface
{
public:
    virtualInterface();
};

//
enum OperationMode      //运行模式      // TODO: 这个模式容易混乱，比如从历史页面进入拍摄界面再回到历史界面，该怎么设置？
{
    normalMeasure   = 0,//常规测量
    historyRecord,      //历史记录列表
    batchRecord,        //批量模式记录列表
    batchMeasure,       //批量模式测量
    inputMeasure,       //输入型测量，如扫码、蓝牙控制测试
    normalReTest,       //常规重新测量
    batchReTest,        //批量模式重新测量
};

// 测量步骤
enum enMeasureStep {
    measureStep_Unknow          = -1,       // 未知
    measureStep_Ready,                      // 【准备】步骤：包括距离调整到合适，然后识别瞳孔，然后调整曝光时间
    measureStep_Collect,                    // 【采集】步骤：指转灯，然后抓图，构建用于屈光计算的图集
    measureStep_Calc,                       // 【计算】步骤：此时正在计算屈光值
    measureStep_Finished,                   // 【结果】步骤：此时正在显示测量结果

    measureStep_Min             = measureStep_Unknow,
    measureStep_Max             = measureStep_Finished,
};

// 距离单位
enum enDistanceUnit {
    distanceUnit_cm         = 0,        // 厘米
    distanceUnit_mm         = 1,        // 毫米
};

//
extern OperationMode opMode;

class WinMeasure
{
public:
    static int blobFailCnt;
    static int countPupilDetectedAlgo;

    static std::string old_run_stat;

    static bool btStart;

    static int countPupilDetectSent;

    static OperationMode getOpMode();
    static void getRunStat(std::string &stat);
    static void setRunStat(const std::string);

    static int getCurrentAgeRange();
    static int getAgeRangeLimited(int _age_range);

    static bool goIntoMeasureStep(enMeasureStep _step);

protected:
    static std::string run_stat;
    static OperationMode opMode;

};

class CaptureThread
{
public:
    static bool showFrame;
};

class dataTrans
{
public:
    static bool uploadImageState;
    static int DEFAULT_USB_SERIAL_BARD;
};

//功能码"dev_stat" 对应data字段如下：
const std::string BUSY = "busy"; //拍摄中
const std::string AVAILABLE = "available";   //空闲中
//功能码"run_stat" 对应data字段如下：
const std::string TOOFAR = "too far";        //太远
const std::string TOOCLOSE = "too close";    //太近
const std::string SUITABLE = "suitable distance"; //请保持不动
const std::string OUT_OF_RANGE     =   "pupil out of range"; //瞳孔尺寸超过范围
const std::string UNDETECTED       =   "failed to detect pupil"; //无法检测瞳孔
const std::string OUT_OF_TIME        = "measure timeout";    //测量超时
const std::string PUPILTOOSMALL  =   "pupil too small";      //瞳孔过小
const std::string NOTRUNNING     =   "camera not running";   //不在运行状态
const std::string DETECT_PUPIL	=   "dectect pupil";        //检测瞳孔
const std::string GRAB_FRAME      =   "grab frame";          //抓取图像
const std::string CALCULATING    =   "calculating";          //计算结果
const std::string MEASURE_SUCC    =   "measure succ";        //测量成功
const std::string MEASURE_FAIL    =   "measure fail";        //测量失败

//
struct Patients{
    QString patientid;              //编号
    QString patientname;            //姓名
    QString patientagerange;        //年龄
    QString patientsex;             //性别
    QString patientdate;
    QString patientlefteyesph;      //左眼球经(屈光)
    QString patientlefteyecyl;      //左眼柱经(散光)
    QString patientlefteyeax;       //左眼轴位角
    QString patientleftse;          //左眼综合度数
    QString patientleftpd;          //左眼瞳孔直径
    bool    patientleftptosis;      //左眼上睑下垂
    QString patientlefths;          //左眼斜视 horizen
    QString patientleftvs;          //左眼斜视 vertical

    QString patientrighteyesph;     //右眼球经(屈光)
    QString patientrighteyecyl;     //右眼柱经(散光)
    QString patientrighteyeax;      //右眼轴位角
    QString patientrightse;         //右眼综合度数
    QString patientrightpd;         //右眼瞳孔直径
    bool    patientrightptosis;     //右眼上睑下垂
    QString patientrighths;         //右眼斜视 horizen
    QString patientrightvs;         //右眼斜视 vertical

    QString patientpd;              //瞳距
    QString patientstuclass;        //班级
    QString patienttesttime;        //测试时间
    QString patientPhone;           //电话
    QString patientAddress;         //地址
    QString patientWechat;          //微信
    QString barcodeData;            //扫码数据
    QString batchNo;                //批次编号
    QString comment1;               //外部编号
    QString Comment2;               //是否标准数据
    bool isTest;                    //是否已测
    bool isBatch;                   //是否批量数据
    bool isNeedUpload;              //是否即使上传
    bool isUploaded;                //是否已上传
    bool isNeedImage;
    bool isUploadedImage;
    QString creattime;              //创建时间  2020.11.30  tao
    int  id;                        //数据库id主键(id顺序要在末尾,不然数据会混乱)

    Patients(){}
    Patients(
        QString patientid,
        QString patientname,
        QString patientagerange,
        QString patientsex,
        QString patientdate,
        QString patientlefteyesph,
        QString patientlefteyecyl,
        QString patientlefteyeax,
        QString patientleftse,
        QString patientleftpd,
        bool    patientleftptosis,      //上睑下垂
        QString patientlefths,          //斜视 horizen
        QString patientleftvs,           //斜视 vertical
        QString patientrighteyesph,
        QString patientrighteyecyl,
        QString patientrighteyeax,
        QString patientrightse,
        QString patientrightpd,
        bool    patientrightptosis,      //上睑下垂
        QString patientrighths,          //斜视 horizen
        QString patientrightvs,          //斜视 vertical
        QString patientpd,
        QString patientstuclass,
        QString patienttesttime,
        QString patientPhone,
        QString patientAddress,
        QString patientWechat,
        QString barcodeData,
        QString batchNo,
        QString comment1,
        QString Comment2,
        bool isTest,
        bool isBatch,
        bool isNeedUpload,
        bool isUploaded,
        bool isNeedImage,
        bool isUploadedImage,
        QString creattime
    )  //受测者（病人）
    {
        this->patientid = patientid;
        this->patientname = patientname;
        this->patientagerange = patientagerange;
        this->patientsex = patientsex;
        this->patientdate=patientdate;
        this->patientlefteyesph = patientlefteyesph;
        this->patientlefteyecyl = patientlefteyecyl;
        this->patientlefteyeax = patientlefteyeax;
        this->patientleftse = patientleftse;
        this->patientleftpd = patientleftpd;
        this->patientleftptosis = patientleftptosis;
        this->patientlefths = patientlefths;
        this->patientleftvs = patientleftvs;
        this->patientrighteyesph = patientrighteyesph;
        this->patientrighteyecyl=patientrighteyecyl;
        this->patientrighteyeax = patientrighteyeax;
        this->patientrightse = patientrightse;
        this->patientrightpd = patientrightpd;
        this->patientrightptosis = patientrightptosis;
        this->patientrighths = patientrighths;
        this->patientrightvs = patientrightvs;
        this->patientpd = patientpd;
        this->patientstuclass = patientstuclass;
        this->patienttesttime = patienttesttime;
        this->patientPhone = patientPhone;
        this->patientAddress = patientAddress;
        this->patientWechat = patientWechat;
        this->barcodeData = barcodeData;
        this->batchNo = batchNo;
        this->comment1 = comment1;
        this->Comment2 = Comment2;
        this->isTest = isTest;
        this->isBatch = isBatch;
        this->isNeedUpload = isNeedUpload;
        this->isUploaded = isUploaded;
        this->isNeedImage = isNeedImage;
        this->isUploadedImage = isUploadedImage;
        this->creattime = creattime;
    }

public:
    void clear(){
        this->patientid = "";
        this->patientname = "";
        this->patientagerange = "";
        this->patientsex = "";
        this->patientdate= "";
        this->patientlefteyesph = "";
        this->patientlefteyecyl = "";
        this->patientlefteyeax = "";
        this->patientleftse = "";
        this->patientleftpd = "";
        this->patientleftptosis = false;
        this->patientlefths = "";
        this->patientleftvs = "";
        this->patientrighteyesph = "";
        this->patientrighteyecyl="";
        this->patientrighteyeax = "";
        this->patientrightse = "";
        this->patientrightpd = "";
        this->patientrightptosis = false;
        this->patientrighths = "";
        this->patientrightvs = "";
        this->patientpd = "";
        this->patientstuclass = "";
        this->patienttesttime = "";
        this->patientPhone = "";
        this->patientAddress = "";
        this->patientWechat = "";
        this->barcodeData = "";
        this->batchNo = "";
        this->comment1 = "";
        this->Comment2 = "";
        this->isTest = false;
        this->isBatch = false;
        this->isNeedUpload = false;
        this->isUploaded = false;
        this->isNeedImage = false;
        this->isUploadedImage = false;
    }
};


//
namespace DataTrans
{
    // 通信数据类
    class MLMCommunic
    {
    public:
        MLMCommunic();
        //string  module;   // 模块码；【必有】
        std::string  func;       // 功能码；【必有】
        std::string  stat;       // 状态码
        std::string  data;       // 数据JSON字符串（不同的模块名对应不同的数据类）
        std::string  check;      // data 字符串的 MD5 值
        std::string  msg;        // 文本描述
        std::string  version;    // 数据格式/协议版本号
        std::string  stamp;      // 序列号

        void ToJson(std::string& _json_str);
        //bool FromJson(std::string& _json_str);
    protected:
        //void Clear();
    };

    // 功能码
    const std::string FUNC_QUERY     = "query";
    const std::string FUNC_NEW       = "new";
    const std::string FUNC_UPDATE    = "update";
    //const string FUNC_DEL       = "del";  //2020.10.12屏蔽  tao
    //2020.10.12 tao
    const std::string FUNC_DEL           = "delete";
    const std::string FUNC_DISTANCE      = "distance";			//distance
    const std::string FUNC_START       	= "start";			//启动拍摄
    const std::string FUNC_STOP       	= "stop";			//停止拍摄
    const std::string FUNC_GRAB_FRAME    = "grab_frame";     //开始拍摄
    const std::string FUNC_DEV_STAT    	= "dev_stat"; 		//设备状态
    const std::string FUNC_RUN_STAT    	= "run_stat"; 		//拍摄中状态
    const std::string FUN_DEL            = "delete";         //删除数据库
    const std::string UNAVAILABLE        = "unavailable";    //不在运行状态

    // 状态码
    const std::string STAT_SUCC  = "succ";   // 成功
    const std::string STAT_FAIL  = "fail";   // 失败
}

using DataTrans::MLMCommunic;

class DataTransmiter
{
public:
    static int ConnMode;
};

// 相机 API
typedef int CameraSdkStatus;
typedef int CameraHandle;

CameraSdkStatus CameraGetExposureTime(CameraHandle hCamera, double *pfExposureTime);
CameraSdkStatus CameraPause(CameraHandle hCamera);
CameraSdkStatus CameraSetExposureTime(CameraHandle hCamera, double fExposureTime);
CameraSdkStatus CameraPlay(CameraHandle hCamera);
CameraSdkStatus CameraReleaseImageBuffer(CameraHandle hCamera, BYTE *pbyBuffer);
CameraSdkStatus CameraSetTriggerMode(CameraHandle hCamera, int iModeSel);

enum enDistanceType {
    distanceType_Mb1010,
};

namespace Util {

class CKeyboardReader
{
public:
    CKeyboardReader();
    ~CKeyboardReader();

    bool Block = false;

};

}

// 相机状态值
enum enCameraStatus
{
    cameraStatus_UnknowErr      = -1,   // 未知错误
    cameraStatus_Succ           = 0,    // 正常
    cameraStatus_InitSdkFail,           // 初始化 SDK 失败
    cameraStatus_FindFail,              // 查找相机失败
    cameraStatus_OpenFail,              // 打开相机失败
    cameraStatus_InitCameraFail,        // 初始化相机失败
    cameraStatus_SetParamFail,          // 设置参数失败
    cameraStatus_Timeout,               // 相机获取图像超时
    cameraStatus_OuterErr,              // 外部的错误
};

// 相机触发模式
enum enCameraTriggerMode
{
    cameraTriggerMode_Auto      = 0,    // 自动触发
    cameraTriggerMode_Soft,             // 软触发
    cameraTriggerMode_Hard,             // 硬触发

    cameraTriggerMode_Min = cameraTriggerMode_Auto,
    cameraTriggerMode_Max = cameraTriggerMode_Hard,
};

//
class CCameraIntf
{
public:
    float getAnalogGain();
    double getExposureTime();
    enCameraStatus setTriggerMode(enCameraTriggerMode _trigger_mode);

};

//
extern WinMeasure *g_WinMeasure;
extern CCameraIntf *g_CameraIntf;
extern bool minresolution;

#endif // VIRTUALINTERFACE_H
