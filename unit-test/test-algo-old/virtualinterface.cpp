#include "virtualinterface.h"

int WinMeasure::blobFailCnt = 0;
int WinMeasure::countPupilDetectedAlgo = 0;
bool WinMeasure::btStart = false;
int WinMeasure::countPupilDetectSent = 0;

bool CaptureThread::showFrame = true;

bool dataTrans::uploadImageState = false;
int dataTrans::DEFAULT_USB_SERIAL_BARD = 115200;

std::vector<BYTE*> resultByte;
enOpticalPath VersionTypeNum = opticalPath_Straight;     //软件版本类型, 0:通用版, 1:极视互联(蓝牙), 2:筛查箱
QString currentID = "T00000";
struct Patients currentPat[1];
int pusAnalog = 0;
bool SaveFaultImageFlag = false;
float g_DistanceVal = 0;
bool saveImage = false;
int g_hCamera = 0;
bool pdfState = false;
unsigned char *g_saturationBuf = NULL;
bool hmMode = false;
int ageStage = 4;
int serailport_status = 0;
unsigned char *g_computeBuf = NULL;
int showAge = 0;
int Screen_model = 1;
unsigned int gCurrentDistance = -1;

WinMeasure *g_WinMeasure = new WinMeasure;
CCameraIntf *g_CameraIntf = new CCameraIntf;
bool minresolution = false;

//
virtualInterface::virtualInterface()
{

}

OperationMode WinMeasure::opMode = normalMeasure;
std::string WinMeasure::run_stat = "";
std::string WinMeasure::old_run_stat = "";

//获取运行状态
OperationMode WinMeasure::getOpMode()
{
    return opMode;
}

void WinMeasure::getRunStat(std::string &_stat)
{
    _stat = run_stat;
}

//设置设备运行状态
void WinMeasure::setRunStat(const std::string _stat)
{
    old_run_stat = run_stat;
    run_stat = _stat;
}

int WinMeasure::getCurrentAgeRange()
{
    return ageStage;
}

int WinMeasure::getAgeRangeLimited(int _age_range)
{
    return _age_range;
}

bool WinMeasure::goIntoMeasureStep(enMeasureStep _step)
{

}

CameraSdkStatus CameraGetExposureTime(CameraHandle hCamera, double *pfExposureTime)
{

}

CameraSdkStatus CameraPause(CameraHandle hCamera)
{

}

CameraSdkStatus CameraSetExposureTime(CameraHandle hCamera, double fExposureTime)
{

}

CameraSdkStatus CameraPlay(CameraHandle hCamera)
{

}

CameraSdkStatus CameraReleaseImageBuffer(CameraHandle hCamera, BYTE *pbyBuffer)
{

}

CameraSdkStatus CameraSetTriggerMode(CameraHandle hCamera, int iModeSel)
{

}

MLMCommunic::MLMCommunic()
{

}

void MLMCommunic::ToJson(std::string &_json_str)
{

}

int DataTransmiter::ConnMode = 0;

Util::CKeyboardReader::CKeyboardReader()
{

}

Util::CKeyboardReader::~CKeyboardReader()
{

}

float CCameraIntf::getAnalogGain()
{

}

double CCameraIntf::getExposureTime()
{

}

enCameraStatus CCameraIntf::setTriggerMode(enCameraTriggerMode _trigger_mode)
{

}
