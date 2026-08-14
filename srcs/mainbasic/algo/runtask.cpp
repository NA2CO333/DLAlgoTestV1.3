//负责处理测量的22张图片，找到瞳孔(瞳孔圆心)截取计算区域并保存，改动需谨慎
#include "runtask.h"

#include <QDebug>
#include <QTime>
#include <QDir>
#include <QThread>

#include "algorithmthread.h"
#include "global.h"

#ifndef UNIT_TEST
# include "settings/settings.h"
# include "winmeasure.h"
# include "tool.h"
#else
# include "virtualinterface.h"
#endif

#define PI 3.14159
#define MINIRECT 5 * PIX_COEF
#define SLIDELEFT 5 * PIX_COEF

// 精准识别时抠图宽度        /* 须是 4 的倍数，否则如果要对整块图像数据直接内存比较（lhImageCmp()），会出错 */
#define ROIW    ((int)(60 * PIX_COEF / 4)) * 4
#define ROIH    ROIW
// 精准识别时抠图宽度的一半
#define ROIW_HALF   ROIW / 2

#define MAXRECT (IMG_HEIGHT - ROIH - MINIRECT)
#define MAXRECTWIDTH (IMG_WIDTH - ROIW - MINIRECT)

//#define KROIW  10
//#define KROIH  5
//#define KROIWA  21
//#define KROIHA  11
#define ASSESS_MODULE 0.9

//#define GNUM 12

//#define RATIO 4

// TODO: 这个去亮斑算法，须调整（适配度申相机）吗？
#define GLINTREGION 4

#define DIV 100         //add by douzi 20180827
//#define MINIPUPIL 12    // too SMALL pupil size
//#define MAXPUPIL 27     //TOO BIG pupil size
#define ASSESS_MODULE 0.9
#define PROCESS_MODULE 0.8
#define SATURARECT 30       //计算饱和度区间
#define MINIPUPILDIS 206 * PIX_COEF    //最小瞳距150, 1.5cm -10cm
#define MAXPUPILDIS 470 * PIX_COEF
#define COM_VAL 0.000001
#define DIS_HOUBIN 10       //15
#define GREDIENT 5          //10//15
#define YSHIFT 400

//using namespace cv;
using namespace std;

extern double g_pyrArea[2];                   // 转灯前识别到的瞳孔面积，[0]:左，[1]:右
extern int pyrRectWidth[2];
extern QString saveImagePath;
extern bool saveImage;
extern int saturationValue;
extern char pupil_estimate[];

struct pyrRectDist
{
    CvRect rect1;
    CvRect rect2;
    uchar pixel1;
    uchar pixel2;
};

QMutex RunTask::mapMutex;
QMutex RunTask::strabismusMutex;

struct strabismus g_strabismusValue;    // 6张图的眼位数值和（读值时须除以6）           // TODO: 去掉，改用 stVisionValue 里的眼位数值？
pyrAspectRatio g_pyrAR;                 // 6张图像的瞳孔高宽比，并提供计算均值的函数

QMap<int,QPoint> RunTask::leftEyeCenter;
QMap<int,QPoint> RunTask::rightEyeCenter;
int RunTask::moveCnt;
static QString oldUrl = "";

//
/*
void runtaskOutputMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex mutex;
    mutex.lock();

    QString text;
    switch(type)
    {
    case QtDebugMsg:
        text = QString("Debug:");
        break;

    case QtWarningMsg:
        text = QString("Warning:");
        break;

    case QtCriticalMsg:
        text = QString("Critical:");
        break;

    case QtFatalMsg:
        text = QString("Fatal:");
    }

    QString context_info = QString("File:(%1) Line:(%2)").arg(QString(context.file)).arg(context.line);
//    QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss ddd");
//    QString current_date = QString("(%1)").arg(current_date_time);
    QString message = QString("%1 %2").arg(text).arg(msg);

//    QString url = QString("/media/runtask_logInfo_%1.txt").arg(Stringid);
    QString url  = QString("/media/%1_logInfo.txt").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh"));

    QFile file(url);
    oldUrl = url;
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream text_stream(&file);
    text_stream << message << "\r\n";
    file.flush();
    file.close();

    mutex.unlock();
}
*/

//
RunTask::RunTask(IplImage *_img, int _num, int _release_type)
{
    imageNum = _num;
    proceImage = _img;
    releaseAction = _release_type;

    glintMode = false;//add by sun for select calculate method depend  on the distance between glint and circle center
}

RunTask::~RunTask()
{
    autoDelete();
}

void RunTask::run()
{
    logDebug(QString::asprintf("RunTask::run(), ImageNum=%d, currentThreadId()=%lld", imageNum, (qintptr)QThread::currentThreadId()), CGlobal::LOG_ALGO);

    try {
        if (calcResultState_Succ == g_CalcResultState) {
            //qInstallMessageHandler(runtaskOutputMessage);
            if (algoVerAll_2022_12 == currentPupilAlgoVer) {
                processPic(proceImage, imageNum, g_SingleDoubleEye);
            } else if (algoVerAll_2019 == currentPupilAlgoVer) {
                if (singleDoubleEyeMode_Both == g_SingleDoubleEye) {
                    processPic(proceImage, imageNum, singleDoubleEyeMode_Both);
                } else {
                    single_processPic(proceImage, imageNum);
                }
            } else if (algoVerAll_2021_07 == currentPupilAlgoVer || algoVerAll_2022_04_1 == currentPupilAlgoVer || algoVerAll_2022_04_2 == currentPupilAlgoVer) {
                if (singleDoubleEyeMode_Both == g_SingleDoubleEye) {
                    processPic2(proceImage, imageNum);
                } else {
                    single_processPic(proceImage, imageNum);
                }
            } else {
                logCritical("RunTask::run(): logic err!", CGlobal::LOG_ALGO);
                g_CalcResultState = calcResultState_Unknown;
            }
        }

        //
        if (0 == releaseAction) {
            //
        } else if (1 == releaseAction) {
            cvReleaseImageHeader(&proceImage);
        } else if (2 == releaseAction) {
            cvReleaseImage(&proceImage);
        }
    }
    catch (...) {
        logCritical((QString(__PRETTY_FUNCTION__) + ": errno = %1, msg = '" + strerror(errno) + "'").arg(errno), CGlobal::LOG_ALGO);
        g_CalcResultState = calcResultState_Unknown;
    }
}

bool RunTask::getGlintBlobPoint(IplImage *_img, CvPoint &_point)
{
    bool is_succ = false;
    if (Screen_model == 1)              // 普通模式（旧），用 cvMinMaxLoc() 得到映光点
    {
        double max_value = 0.0;
        cvMinMaxLoc(_img, NULL, &max_value, NULL, &_point);

        // 求图像最大像素的平均坐标
        int w = _img->width;
        int h = _img->height;

        int max = max_value;
        int sum_x = 0, sum_y = 0;
        int count = 0;
        int v;
        for (int i = 0; i < w; i++) {
            for (int j = 0; j < h; j++) {
                v = cvGet2D(_img, j, i).val[0];
                if (v == max) {
                    sum_x += i;
                    sum_y += j;
                    count++;
                }
            }
        }
        int mean_x = std::round((double)sum_x / count);
        int mean_y = std::round((double)sum_y / count);

        _point.x = mean_x;
        _point.y = mean_y;

        is_succ = true;
    }
    else if (Screen_model == 2)         //专业模式（旧），用 cv::SimpleBlobDetector 检测映光点，如果聚焦不良，可能失败
    {
        //检测瞳孔亮点
        is_succ = pupilBlobDetect(_img, _point);
    }

    return is_succ;
}

bool RunTask::pupilBlobDetect(IplImage *_image, CvPoint &_center)
{
#ifdef TEST_MODE
    {
        bool is_show_img = false;
        if (is_show_img) {
            cvShowImage("_image:RunTask::pupilBlobDetect()", _image);
        }
    }
#endif

    //设置过检测参数params
    cv::SimpleBlobDetector::Params params;
    params.thresholdStep = 2;       //二值化的阈值步长
    params.minThreshold = 50;       //二值化的起始阈值
    params.maxThreshold = 240;      //二值化的终止阈值
    params.minDistBetweenBlobs = 1; //最小的斑点距离，不同二值图像的斑点间距离小于该值时，被认为是同一个位置的斑点
    params. filterByColor = true;   //斑点颜色的限制变量
    params.blobColor = 255;         //表示只提取黑色斑点；如果该变量为255，表示只提取白色斑点
    params.filterByInertia = true;  //斑点惯性率的限制变量
    params.minInertiaRatio = 0.3;   //斑点的最小惯性率0.05
    params.filterByArea = true;     //斑点面积的限制变量
    params.minArea  = 2;            //斑点的最小面积
    params.maxArea  = 70;           //斑点的最大面积41

    cv::SimpleBlobDetector blobDetect(params);
    vector<cv::KeyPoint> keypoints;
    blobDetect.detect(_image,keypoints);

    bool state = false;
    if(keypoints.size() == 1)
    {
        _center.x = keypoints.at(0).pt.x + (keypoints.at(0).size/2);
        _center.y = keypoints.at(0).pt.y + (keypoints.at(0).size/2);
        state = true;
    }
    else if(keypoints.size() > 1)
    {
        double tempVal = 0;
        CvPoint tempPt;

        for(int i=0;i < keypoints.size();i++)
        {
            CvPoint pt;
            pt.x = keypoints.at(i).pt.x + (keypoints.at(i).size/2);
            pt.y = keypoints.at(i).pt.y + (keypoints.at(i).size/2);
            double pixVal = cvGet2D(_image,pt.y,pt.x).val[0];
            if(pixVal > tempVal){
                tempPt = pt;
                tempVal = pixVal;
            }
        }
        _center = tempPt;

        state = true;
    }
    else
        return false;


    cvSet2D(_image,_center.y,_center.x,cvScalar(0,0,255));
    return state;
}

//
void RunTask::reduceGlintBlob(IplImage *_img_src, const CvPoint &_center, IplImage *_img_dst)
{
    CvPoint center;
    center.x = _center.y;       /* 因为后面的去亮斑代码的 x、y 轴搞反了，所以这里把 x、y 轴互换后，结果刚好正确。 */
    center.y = _center.x;

    if(center.x - 5 >= 0 && center.x + 5 <= 64 && center.y - 5 >= 0 && center.y + 5 <= 64)      // TODO: cofxx 数组只有适配迈德威视相机像素的？度申的图像是否应该缩小？
        for (int i = 0; i < _img_src->height; i++)
        {
            for (int j = 0; j < _img_src->width; j++)
            {

                CvScalar scrop1 = cvGet2D(_img_src, i, j);

                CvScalar ssub;

                if (i >= center.x - GLINTREGION && i <= center.x + GLINTREGION && j <= center.y + GLINTREGION && j >= center.y - GLINTREGION)
                {
                    //                CvScalar nextl = cvGet2D(_img_src, i - 1, j);
                    //                CvScalar nextc = cvGet2D(_img_src, i, j - 1);
                    //                CvScalar nextr = cvGet2D(_img_src, i - 1, j - 1);

                    //			//add by douzi 20180827
                    //                CvScalar nexta = cvGet2D(_img_src, i - 1, j + 1);  //08.24
                    //				CvScalar nextb = cvGet2D(_img_src, i + 1, j - 1);   //08.24

                    //				//ssub.val[0] = (nextl.val[0] + nextc.val[0] + nextr.val[0]) / 3;
                    //				ssub.val[0] = (nextl.val[0] + nextc.val[0] + nextr.val[0] + nexta.val[0] + nextb.val[0]) / 5;  //0824 by ron ,用五个点的平均值来替代这个点
                    //			//add end

                    double center_x = (double)center.x;
                    double center_y = (double)center.y;

                    glintValue[0]  = (double)(cof1 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y - 5).val[0]);
                    glintValue[1]  = (double)(cof2 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y - 4).val[0]);
                    glintValue[2]  = (double)(cof3 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y - 3).val[0]);
                    glintValue[3]  = (double)(cof4 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y - 2).val[0]);
                    glintValue[4]  = (double)(cof5 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y - 1).val[0]);
                    glintValue[5]  = (double)(cof6 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y - 0).val[0]);
                    glintValue[6]  = (double)(cof7 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y + 1).val[0]);
                    glintValue[7]  = (double)(cof8 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y + 2).val[0]);
                    glintValue[8]  = (double)(cof9 [i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y + 3).val[0]);
                    glintValue[9]  = (double)(cof10[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y + 4).val[0]);
                    glintValue[10] = (double)(cof11[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 5, center_y + 5).val[0]);

                    glintValue[11] = (double)(cof12[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 4, center_y - 5).val[0]);
                    glintValue[12] = (double)(cof13[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 3, center_y - 5).val[0]);
                    glintValue[13] = (double)(cof14[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 2, center_y - 5).val[0]);
                    glintValue[14] = (double)(cof15[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 1, center_y - 5).val[0]);
                    glintValue[15] = (double)(cof16[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 0, center_y - 5).val[0]);
                    glintValue[16] = (double)(cof17[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 1, center_y - 5).val[0]);
                    glintValue[17] = (double)(cof18[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 2, center_y - 5).val[0]);
                    glintValue[18] = (double)(cof19[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 3, center_y - 5).val[0]);
                    glintValue[19] = (double)(cof20[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 4, center_y - 5).val[0]);

                    glintValue[20] = (double)(cof21[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 4, center_y + 5).val[0]);
                    glintValue[21] = (double)(cof22[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 3, center_y + 5).val[0]);
                    glintValue[22] = (double)(cof23[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 2, center_y + 5).val[0]);
                    glintValue[23] = (double)(cof24[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 1, center_y + 5).val[0]);
                    glintValue[24] = (double)(cof25[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x - 0, center_y + 5).val[0]);
                    glintValue[25] = (double)(cof26[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 1, center_y + 5).val[0]);
                    glintValue[26] = (double)(cof27[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 2, center_y + 5).val[0]);
                    glintValue[27] = (double)(cof28[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 3, center_y + 5).val[0]);
                    glintValue[28] = (double)(cof29[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 4, center_y + 5).val[0]);

                    glintValue[29] = (double)(cof30[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y - 5).val[0]);
                    glintValue[30] = (double)(cof31[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y - 4).val[0]);
                    glintValue[31] = (double)(cof32[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y - 3).val[0]);
                    glintValue[32] = (double)(cof33[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y - 2).val[0]);
                    glintValue[33] = (double)(cof34[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y - 1).val[0]);
                    glintValue[34] = (double)(cof35[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y - 0).val[0]);
                    glintValue[35] = (double)(cof36[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y + 1).val[0]);
                    glintValue[36] = (double)(cof37[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y + 2).val[0]);
                    glintValue[37] = (double)(cof38[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y + 3).val[0]);
                    glintValue[38] = (double)(cof39[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y + 4).val[0]);
                    glintValue[39] = (double)(cof40[i - center.x + 4][j - center.y + 4] * cvGet2D(_img_src, center_x + 5, center_y + 5).val[0]);

                    double sumValue = 0;
                    for(int i = 0; i < 40; i++)
                    {
                        sumValue += glintValue[i];
                    }

                    ssub.val[0] = sumValue;
                    //                //qDebug()<< _img_idx << "sumValue1:" << sumValue << "ssub.val[0]" << ssub.val[0];
                }
                else
                {
                    ssub.val[0] = scrop1.val[0];
                    //                //qDebug() << _img_idx << "glint 1 out of range!!!!!!!!!!!!!!!!!!!!";

                }

                cvSet2D(_img_dst, i, j, ssub);

            }
        }
}

// 处理一张转灯图
bool RunTask::processPic(IplImage *_img, int _img_idx, enSingleDoubleEyeMode _single_double_eye)
{
    logDebug(QString(__PRETTY_FUNCTION__) + " into ...", CGlobal::LOG_ALGO);

    //
    if (_img == NULL)
    {
        logCritical("img is NULL, return false", CGlobal::LOG_ALGO);

        g_CalcResultState = calcResultState_Unknown;
        return false;
    }

    // 确定灯珠角度   /* 筛查仪上有23颗灯，分为4组，每组一个角度，除了中间那颗，其他22颗对应22张图，1~6张为0°，7～12 为60°，13~18 为120°，21～22 为41° */
    int angle = 0;
    if (_img_idx >= 0 && _img_idx <= 6)
    {
        angle = 0;
    }
    if (_img_idx >= 7 && _img_idx <= 12)
    {
        angle = 60;
    }
    if (_img_idx >= 13 && _img_idx <= 18)
    {
        angle = 120;
    }

    if (_img_idx == 19 || _img_idx == 20)
    {
        angle = 139;
    }
    if (_img_idx == 21 || _img_idx == 22)
    {
        angle = 41;
    }
    logDebug(QString("img idx = %1, angle = %2").arg(_img_idx).arg(angle), CGlobal::LOG_ALGO);

    //
    CvPoint3D32f avalPoint3D[2];
    int pupilDis = 0;
    int feedback = 0;

    stPupilInfo pupil_info_r, pupil_info_l;
    //memset(&pupil_info_r, 0, sizeof(stPupilInfo));
    //memset(&pupil_info_l, 0, sizeof(stPupilInfo));

    if (algoVerAll_2019 == currentPupilAlgoVer) {
        feedback = pyrDetect0(_img, avalPoint3D, &pupilDis, _img_idx);
    } else if (algoVerAll_2022_12 == currentPupilAlgoVer) {
        bool is_succ = AlgorithmThread::detectPupil_4((unsigned char *)_img->imageData, _img_idx, pupil_info_r, pupil_info_l, true, _single_double_eye);

        //qDebug() << "RunTask::processPic(): pupil pos_" << _img_idx << "(right,left) = (" << pupil_info_r.center.x << pupil_info_r.center.y << "), ("
        //            << pupil_info_l.center.x << pupil_info_l.center.y << ")";

        if (is_succ) {
            avalPoint3D[0].x = pupil_info_l.center.x;
            avalPoint3D[0].y = pupil_info_l.center.y;
            avalPoint3D[0].z = pupil_info_l.radius;
            avalPoint3D[1].x = pupil_info_r.center.x;
            avalPoint3D[1].y = pupil_info_r.center.y;
            avalPoint3D[1].z = pupil_info_r.radius;

            if (singleDoubleEyeMode_Both == g_SingleDoubleEye) {
                pupilDis = std::sqrt(std::pow(pupil_info_l.center.x - pupil_info_r.center.x, 2) + std::pow(pupil_info_l.center.y - pupil_info_r.center.y, 2));
            }

            feedback = 1;
        }
    }

    //
    float radius_l, radius_r;
    CvPoint2D32f center_l, center_r;

    qDebug() << "--" << _img_idx << "feedback:" << feedback;

    if (feedback == 1)
    {
        center_l.x = avalPoint3D[0].x;
        center_l.y = avalPoint3D[0].y;

        center_r.x = avalPoint3D[1].x;
        center_r.y = avalPoint3D[1].y;

        radius_l = avalPoint3D[0].z;
        radius_r = avalPoint3D[1].z;

        qDebug()<<"_img_idx:"<<_img_idx<<"--center1(left)< "<<center_l.x<<","<<center_l.y
               <<">  center2(right)<"<<center_r.x<<","<<center_r.y<<">";

        //addPupilCenter(QPoint(center1.x,center1.y),QPoint(center2.x,center2.y),_img_idx); //记录瞳孔中心

        if (!(calcResultState_Succ == g_CalcResultState))
        {
            qDebug() << _img_idx << "error!!!!!!feedback ==" << feedback;

            g_CalcResultState = calcResultState_Unknown;
            return false;
        }
    }
    else
    {
        qDebug() << _img_idx << "processPic++feedback != 1++++return false";

        g_CalcResultState = calcResultState_Unknown;
        return false;
    }

    //
    bool is_need_right  = (singleDoubleEyeMode_Right & _single_double_eye);
    bool is_need_left   = (singleDoubleEyeMode_Left & _single_double_eye);

    //
    bool is_succ = false;
    do {
        //
        bool is_succ_r = false;
        if (is_need_right) {
            is_succ_r = processPicOfOneEye(_img, _img_idx, pupil_info_r, angle, whichEye_Right);
            if (!is_succ_r) {
                logDebug("process right img failed!", CGlobal::LOG_ALGO);
                break;
            }
        }

        bool is_succ_l = false;
        if (is_need_left) {
            is_succ_l = processPicOfOneEye(_img, _img_idx, pupil_info_l, angle, whichEye_Left);
            if (!is_succ_l) {
                logDebug("process left img failed!", CGlobal::LOG_ALGO);
                break;
            }
        }

        //
        is_succ = true;
    } while (false);

    //
#ifdef TEST_MODE
    bool is_show_img = false;        // 是否显示处理后的瞳孔图像
//    if (5 == _img_idx || 6 == _img_idx) {
//        is_show_img = true;
//    }
    if (is_show_img) {
        cvShowImage(QString::asprintf("%d_Rect&RfPnt:RunTask::processPic()", _img_idx).toLocal8Bit().data(), _img);
        //cvWaitKey();  // TODO: 为什么按键无响应？
    }
#endif

    if ((!is_succ) && (calcResultState_Succ == g_CalcResultState)) {
        g_CalcResultState = calcResultState_Unknown;
    }

    //
    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_ALGO);
    return is_succ;
}

// 处理一个眼睛图像
bool RunTask::processPicOfOneEye(IplImage *_img, int _img_idx, const stPupilInfo &_pupil_info, int _angle, enWhichEye _which_eye)
{
    logDebug(QString(__PRETTY_FUNCTION__) + QString(" into, img_idx = %1, which_eye = %2").arg(_img_idx).arg((int)_which_eye), CGlobal::LOG_ALGO);
    ///---************************************************------------------------------------
    //test 1stly
    //需要对图像去除Glint,并去掉高频Noise

    // 裁剪出瞳孔区域
    CvRect rect_pupil;

    static const int ROI_HALF_WIDTH = 32 * PIX_COEF;
    static const int ROI_WIDTH      = ROI_HALF_WIDTH * 2 + 1;

    CvPoint center_int = cvPointFrom32f(_pupil_info.center);

    rect_pupil.x = center_int.x - ROI_HALF_WIDTH;
    rect_pupil.y = center_int.y - ROI_HALF_WIDTH;
    rect_pupil.width = ROI_WIDTH;
    rect_pupil.height = ROI_WIDTH;

    if (       rect_pupil.x < 0 || (rect_pupil.x + rect_pupil.width) > _img->width
            || rect_pupil.y < 0 || (rect_pupil.y + rect_pupil.height) > _img->height)
    {
        qDebug() << "error = false, rect_pupil.x < 0  +++++++++++++++++++return false";

        g_CalcResultState = calcResultState_Unknown;
        return false;
    }

    //qDebug()<<"*****************1111111**************"<<"runTask ThreadID = "<<QThread::currentThreadId();
    IplImage *img_pupil_origin = cvCreateImage(cvSize(ROI_WIDTH, ROI_WIDTH), IPL_DEPTH_8U, 1);      // 瞳孔区域原图

    //
    cvSetImageROI(_img, rect_pupil);
    cvCopy(_img, img_pupil_origin, NULL);
    cvResetImageROI(_img);
//    ////qDebug()<<"*****************22222222**************"<<"runTask ThreadID = "<<QThread::currentThreadId();

//    cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/final_left_"+QString::number(_img_idx,10).toLatin1()+".bmp",crop1RotateImg);
//    cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/final_right_"+QString::number(_img_idx,10).toLatin1()+".bmp",crop2RotateImg);

    // 根据角度旋转图像
    IplImage *img_puple_rotated = cvCreateImage(cvSize(ROI_WIDTH, ROI_WIDTH), IPL_DEPTH_8U, 1);             // 瞳孔区域旋转后的图像
    rotateImage(img_pupil_origin, img_puple_rotated, _angle, cvPoint2D32f(_pupil_info.center.x - center_int.x, _pupil_info.center.y - center_int.y));

    // 瞳孔图像中用于查找映光点的区域
    CvRect rect_glint;

    rect_glint.x = 16 * PIX_COEF;       // 用于检测映光点位置的 ROI 框
    rect_glint.y = 24 * PIX_COEF;
    rect_glint.width = ROI_WIDTH - rect_glint.x * 2;
    rect_glint.height = ROI_WIDTH - rect_glint.y * 2;

    IplImage *img_glint = cvCreateImage(cvSize(rect_glint.width, rect_glint.height), IPL_DEPTH_8U, 1);      // 用于检测映光点坐标的图像

    cvSetImageROI(img_puple_rotated, rect_glint);
    cvCopy(img_puple_rotated, img_glint, NULL);
    cvResetImageROI(img_puple_rotated);

// cvSaveImage("/media/corpRectImg/"+QString::number(_img_idx,10).toLatin1()+"img_glint_l.bmp",img_glint);
// cvSaveImage("/media/corpRectImg/"+QString::number(_img_idx,10).toLatin1()+"img_glint_r.bmp",crop2Img_temp);

//    if(_img_idx==1)
//        ////qDebug()<<_img_idx<< " -----------------MaxLocation1:"<<MaxLocation1.x<<","<<MaxLocation1.y;

    // 得到映光点位置
    CvPoint point_glint;
    bool is_got_glint = getGlintBlobPoint(img_glint, point_glint);
    if(!is_got_glint)
    {
        qDebug() << g_currentSubjectNum << "error = false , pupilBlobDetect failed   ++++"<<_img_idx<<"+++++++++++++++return false";
        WinMeasure::blobFailCnt++;
        cvReleaseImage(&img_pupil_origin);
        cvReleaseImage(&img_puple_rotated);
        cvReleaseImage(&img_glint);

        g_CalcResultState = calcResultState_Unknown;
        return false;
    }

    //================对glint点附近的9*9重新赋值  left========================

    //
    point_glint.x = point_glint.x + rect_glint.x;
    point_glint.y = point_glint.y + rect_glint.y;

    //add for quint test*************************************
    if (_img_idx <= 6)      // 计算凝视角度
    {
//         qDebug()<<_img_idx<<"xieshi-pcenter1.x:"<<pcenter1.x<<",pcenter1.y:"<<pcenter1.y<<",pcenter2.x:"<<pcenter2.x<<",pcenter2.y:"<<pcenter2.y;

        /* 旧代码的眼位计算公式 */     // TODO: 旧代码里的这个公式是怎么推导出来的？
        //double gaze_hori = /*round*/(asin(((double)point_glint.x - ROI_HALF_WIDTH) / ROI_HALF_WIDTH) / PI * 180 * 0.5);
        //double gaze_vert = /*round*/(asin(((double)point_glint.y - ROI_HALF_WIDTH) / ROI_HALF_WIDTH) / PI * 180 * 0.5);

        /* 新的眼位计算公式，来源《视筛固视修改需求_刘宇_20230413.docx》 */
        double gaze_hori = ((double)point_glint.x - ROI_HALF_WIDTH) * PIX_TO_PHY * 7;
        double gaze_vert = ((double)point_glint.y - ROI_HALF_WIDTH) * PIX_TO_PHY * 7;

        //
        strabismusMutex.lock();

        if (whichEye_Right == _which_eye) {
            g_strabismusValue.rightEyeLR += gaze_hori;
            g_strabismusValue.rightEyeUD += gaze_vert;
        } else {
            g_strabismusValue.leftEyeLR += gaze_hori;
            g_strabismusValue.leftEyeUD += gaze_vert;
        }

        strabismusMutex.unlock();
    }

    //add end*****************************************************

    qDebug() << _img_idx << "----begin remove high glint-----";

    //end

    // 消除高亮点
    IplImage *img_pupil_glint_cleaned = cvCloneImage(img_puple_rotated);
    reduceGlintBlob(img_puple_rotated, point_glint, img_pupil_glint_cleaned);

//    qDebug()<<_img_idx<<"*****************rrrrrrrrrrrrrrrrrrrrr**************"<<"runTask ThreadID = "<<QThread::currentThreadId();

    //
    int KROIW = 11 * PIX_COEF;          // ROI 宽度的一半
    int KROIH = 5 * PIX_COEF;           // ROI 高度的一半

    if (algoVerAll_2022_12 == currentPupilAlgoVer) {
        KROIW = ROI_HALF_WIDTH;
        KROIH = (double)KROIW * 5 / 11;
    }

    int KROIWA = KROIW * 2 + 1;         // ROI 宽度
    int KROIHA = KROIH * 2 + 1;         // ROI 高度

    if (_pupil_info.radius <= 15 * PIX_COEF && !(algoVerAll_2022_12 == currentPupilAlgoVer))
    {
        KROIWA = 17 * PIX_COEF;
        KROIW = 8 * PIX_COEF;
    }

    //
    int start_x = img_pupil_glint_cleaned->width / 2 - KROIW;
    int start_y = img_pupil_glint_cleaned->height / 2 - KROIH;

    CvRect rect_calc = cvRect(start_x, start_y, KROIWA, KROIHA);
    cvSetImageROI(img_pupil_glint_cleaned, rect_calc);
    IplImage *img_calc_crop = cvCreateImage(cvSize(KROIWA, KROIHA), img_pupil_glint_cleaned->depth, img_pupil_glint_cleaned->nChannels);   // 瞳孔图像中用于计算屈光度的裁减
    cvCopy(img_pupil_glint_cleaned, img_calc_crop, NULL);
    cvResetImageROI(img_pupil_glint_cleaned);

    IplImage *img_calc_crop_cpy = cvCloneImage(img_calc_crop);

    //
#ifdef TEST_MODE
    bool is_show_img = false;        // 是否显示处理后的瞳孔图像
//    if (5 == _img_idx || 6 == _img_idx) {
//        is_show_img = true;
//    }
    if (is_show_img) {
        cvSetImageROI(_img, rect_pupil);
        cvCopy(img_puple_rotated, _img);                // 复制旋转后的眼睛图像到整图

        cvResetImageROI(_img);

        cvRectangleR(_img, rect_pupil, CV_RGB(255, 255, 255));            // 从整图中抠图的 ROI 框

        CvRect rect_rf_pnt_l = cvRect(rect_glint.x + rect_pupil.x, rect_glint.y + rect_pupil.y, rect_glint.width, rect_glint.height);
        cvRectangleR(_img, rect_rf_pnt_l, CV_RGB(255, 255, 255));        // 检测映光点的框

        cvCircle(_img, cvPoint(point_glint.x + rect_pupil.x, point_glint.y + rect_pupil.y), 16, CV_RGB(255, 255, 255));   // 去除映光点时像素遍历的中心点

        CvRect rect_final_view_l = cvRect(rect_pupil.x, rect_pupil.y + rect_pupil.height + 2, rect_pupil.width, rect_pupil.height);
        cvSetImageROI(_img, rect_final_view_l);
        cvCopy(img_pupil_glint_cleaned, _img);                 // 拷贝去亮斑后的图到整图（左眼）

        cvResetImageROI(_img);

        cvRectangleR(_img, rect_final_view_l, CV_RGB(255, 255, 255));

        CvRect rect_pupil_view_l = cvRect(rect_calc.x + rect_final_view_l.x, rect_calc.y + rect_final_view_l.y, rect_calc.width, rect_calc.height);
        cvRectangleR(_img, rect_pupil_view_l, CV_RGB(255, 255, 255));        // 最后得到的瞳孔区域
    }
#endif

    qDebug() << _img_idx << "****finish remove high glint******";

    // 插入处理后的瞳孔区域到临时列表
    if (calcResultState_Succ == g_CalcResultState)     //瞳孔计算通过
    {
        mapMutex.lock();

        //qDebug()<<_img_idx<<"start insert eyemap"<<QThread::currentThreadId();

        if (whichEye_Right == _which_eye) {
            g_pupilImgRight.insert(_img_idx, img_calc_crop_cpy);    // 插入 右眼 瞳孔图片
        } else {
            g_pupilImgLeft.insert(_img_idx, img_calc_crop_cpy);     // 插入 左眼 瞳孔图片
        }

        if ((singleDoubleEyeMode_Both == g_SingleDoubleEye && whichEye_Left == _which_eye) || (singleDoubleEyeMode_Both != g_SingleDoubleEye)) {
            QString numstr = QString::number(_img_idx) + " succ \n";
            //pupil_estimate.append(numstr);
            QByteArray ba = numstr.toLatin1(); // must
            char* str=ba.data();
            strcat(pupil_estimate, str);
            qDebug()<<_img_idx<<"insert eyemap sucess "<<QThread::currentThreadId();
        }

        mapMutex.unlock();
    }
    else        //瞳孔计算不通过
    {
        //qDebug()<<_img_idx<<"error=false,start release leftImg,rightImg...";
        cvReleaseImage(&img_calc_crop_cpy);   //释放左瞳孔图片数据
        QString numstr = QString::number(_img_idx) + " fault \n";
        //pupil_estimate.append(numstr);
        QByteArray ba = numstr.toLatin1(); // must
        char* str=ba.data();
        strcat(pupil_estimate,str);
        qDebug()<<_img_idx<<"complete release leftImg,rightImg!";
    }

    qDebug() << _img_idx << "--before release";
    cvReleaseImage(&img_calc_crop); //???
    cvReleaseImage(&img_pupil_origin);
    cvReleaseImage(&img_puple_rotated);
    cvReleaseImage(&img_pupil_glint_cleaned);
    cvReleaseImage(&img_glint);

    //
    qDebug() << _img_idx << "--cvRelease,processPic return true";
    logDebug(QString(__PRETTY_FUNCTION__) + " ended", CGlobal::LOG_ALGO);
    return true;
}

int RunTask::processPic2(IplImage *Img, int num)
{

    if (Img == NULL)
    {
//        qDebug()<<"Img == NULL,return";

        g_CalcResultState = calcResultState_Unknown;
        return 0;
    }

    //筛查仪上有23颗灯，分为4组，每组一个角度，除了中间那颗，其他22颗对应22张图，1~6张为0°，7～12 为60°，13~18 为120°，21～22 为41°
    int angle = 0;
    if (num >= 0 && num <= 6)
    {
        angle = 0;
    }
    else if (num >= 7 && num <= 12)
    {
        angle = 60;
    }
    else if (num >= 13 && num <= 18)
    {
        angle = 120;
    }
    else if (num == 19 || num == 20)
    {
        angle = 139;
    }
    else if (num == 21 || num == 22)
    {
        angle = 41;
    }
//    qDebug()<<"runtask++++++++++++++++++"<<num<<"+++++++++++++++++++";

//       int DeviationP;

    //
    CvPoint3D32f avalPoint3D[2];
    int pupilDis = 0;

    int feedback = 0;
//    try {
        feedback = pyrDetect(Img, avalPoint3D, &pupilDis, num);
//    }
//    catch (exception &ex) {
//        logCritical(QString::asprintf("RunTask::processPic2(), executing pyrDetect() exception:\n %s", ex.what()), CGlobal::LOG_ALGO);
//        util_out_trace();
//        throw "caught error in RunTask::processPic2() executing pyrDetect()";
//        //return 0;
//    }
//    catch (...) {
//        logCritical(QString::asprintf("RunTask::processPic2(), executing pyrDetect() exception: unknown"), CGlobal::LOG_ALGO);
//        util_out_trace();
//        throw "caught error in RunTask::processPic2() executing pyrDetect()";
//        //return 0;
//    }

    int radius1, radius2;
    CvPoint center1, center2;

    qDebug() << "--" << num << "feedback:" << feedback;

    //if (10 == num)
    //    qDebug() << endl;

    if (feedback == 1)
    {
        center1.x = avalPoint3D[0].x;
        center1.y = avalPoint3D[0].y;

        center2.x = avalPoint3D[1].x;
        center2.y = avalPoint3D[1].y;

        radius1 = avalPoint3D[0].z;
        radius2 = avalPoint3D[1].z;

        qDebug()<<"num:"<<num<<"--center1< "<<center1.x<<","<<center1.y
               <<">  center2.x<"<<center2.x<<","<<center2.y<<">";

//        addPupilCenter(QPoint(center1.x,center1.y),QPoint(center2.x,center2.y),num); //记录瞳孔中心

        if (!(calcResultState_Succ == g_CalcResultState))
        {
            qDebug() << num << "error!!!!!!feedback ==" << feedback;

            g_CalcResultState = calcResultState_Unknown;
            return 0;
        }
    }
    else
    {
        logDebug(QString::asprintf("RunTask::processPic2(), pupil detection failed! num=%d", num), CGlobal::LOG_ALGO);

        g_CalcResultState = calcResultState_Unknown;
        return 0;
    }


    ///---************************************************------------------------------------
    //test 1stly
    //需要对图像去除Glint,并去掉高频Noise
#if 1

    CvRect cropRect1, cropRect2;

    cropRect1.x = center1.x - 32;
    cropRect1.y = center1.y - 32;
    cropRect1.width = 65;
    cropRect1.height = 65;

    cropRect2.x = center2.x - 32;
    cropRect2.y = center2.y - 32;
    cropRect2.width = 65;
    cropRect2.height = 65;
    if (cropRect1.x < 0 || (cropRect1.x + cropRect1.width) > Img->width
            || cropRect1.y < 0 || (cropRect1.y + cropRect1.height) > Img->height
            || cropRect2.x < 0 || (cropRect2.x + cropRect2.width) > Img->width
            || cropRect2.y < 0 || (cropRect2.y + cropRect2.height) > Img->height)
    {

        qDebug() << "error = false ,cropRect1.x < 0  +++++++++++++++++++return";

        g_CalcResultState = calcResultState_Unknown;
        return 0;
    }


    //qDebug()<<"*****************1111111**************"<<"runTask ThreadID = "<<QThread::currentThreadId();
    IplImage *crop1Img = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);
    IplImage *crop2Img = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);

    IplImage *crop1RotateImg = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);
    IplImage *crop2RotateImg = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);

    //
    cvSetImageROI(Img, cropRect1);
    cvCopy(Img, crop1RotateImg, NULL);
    cvResetImageROI(Img);
//    ////qDebug()<<"*****************22222222**************"<<"runTask ThreadID = "<<QThread::currentThreadId();
    cvSetImageROI(Img, cropRect2);
    cvCopy(Img, crop2RotateImg, NULL);
    cvResetImageROI(Img);

//    cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/final_left_"+QString::number(num,10).toLatin1()+".bmp",crop1RotateImg);
//    cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/final_right_"+QString::number(num,10).toLatin1()+".bmp",crop2RotateImg);

    rotateImage(crop1RotateImg, crop1Img, angle);
    rotateImage(crop2RotateImg, crop2Img, angle);


    IplImage *crop1Img_temp = cvCreateImage(cvSize(33, 17), IPL_DEPTH_8U, 1);
    IplImage *crop2Img_temp = cvCreateImage(cvSize(33, 17), IPL_DEPTH_8U, 1);


    CvRect  cropRect1_temp, cropRect2_temp;

    cropRect1_temp.x = 16;
    cropRect1_temp.y = 24;
    cropRect1_temp.width = 33;
    cropRect1_temp.height = 17;

    cropRect2_temp.x = 16;
    cropRect2_temp.y = 24;
    cropRect2_temp.width = 33;
    cropRect2_temp.height = 17;

    cvSetImageROI(crop1Img, cropRect1_temp);
    cvCopy(crop1Img, crop1Img_temp, NULL);
    cvResetImageROI(crop1Img);

    cvSetImageROI(crop2Img, cropRect2_temp);
    cvCopy(crop2Img, crop2Img_temp, NULL);
    cvResetImageROI(crop2Img);

// cvSaveImage("/media/corpRectImg/"+QString::number(num,10).toLatin1()+"crop1Img_temp.bmp",crop1Img_temp);
// cvSaveImage("/media/corpRectImg/"+QString::number(num,10).toLatin1()+"crop2Img_temp.bmp",crop2Img_temp);



//    if(num==1)
//        ////qDebug()<<num<< " -----------------MaxLocation1:"<<MaxLocation1.x<<","<<MaxLocation1.y;


    //add by douzi 20180827
    CvPoint pcenter1;
    CvPoint pcenter2;


    if(Screen_model == 1)    //add by tao 2020.7.16(普通模式,未加限制)
    {

        //get the max point by first bmp
        double MinValue1 = 0.0;
        double MaxValue1 = 0.0;
        CvPoint MinLocation1;
        CvPoint MaxLocation1;

        cvMinMaxLoc(crop1Img_temp, &MinValue1, &MaxValue1, &MinLocation1, &MaxLocation1);

        double MinValue2 = 0.0;
        double MaxValue2 = 0.0;
        CvPoint MinLocation2;
        CvPoint MaxLocation2;
        cvMinMaxLoc(crop2Img_temp, &MinValue2, &MaxValue2, &MinLocation2, &MaxLocation2);
        //qDebug()<<"*****************33333333**************"<<"runTask ThreadID = "<<QThread::currentThreadId();
        //    if(num==1)
        //        ////qDebug()<<num<< " -----------------MaxLocation1:"<<MaxLocation1.x<<","<<MaxLocation1.y;
        //add by douzi 20180827
        pcenter1 = MaxLocation1;
        pcenter2 = MaxLocation2;
    }
    else if(Screen_model == 2)      //2020.7.16 专业模式,加限制
    {
        //检测瞳孔亮点
        bool leftBlob = pupilBlobDetect(crop1Img_temp,pcenter1);
        bool rightBlob = pupilBlobDetect(crop2Img_temp,pcenter2);

        if(!leftBlob || !rightBlob)
        {
            qDebug() <<g_currentSubjectNum <<"error = false ,pupilBlobDetect failed   ++++"<<num<<"+++++++++++++++return";
            WinMeasure::blobFailCnt++;
            cvReleaseImage(&crop1RotateImg);
            cvReleaseImage(&crop2RotateImg);
            cvReleaseImage(&crop1Img);
            cvReleaseImage(&crop2Img);
            cvReleaseImage(&crop1Img_temp);
            cvReleaseImage(&crop2Img_temp);

            g_CalcResultState = calcResultState_Unknown;
            return 0;
        }
    }

    if(saveImage)   //保存瞳孔图片
    {
        QString Pupil_Image_Path = QString("/media/photo/%1/pupil_pictures").arg(g_currentSubjectNum);
        QDir myDir(Pupil_Image_Path);
        if(!myDir.exists()){
            if(!myDir.mkdir(Pupil_Image_Path)){}
        }
        QString crop1Img_temp_Path = Pupil_Image_Path+QString("/crop1Img_temp_%1.bmp").arg(num);
        QString crop2Img_temp_Path = Pupil_Image_Path+QString("/crop1Img_temp_%1.bmp").arg(num);

        cvSaveImage(crop1Img_temp_Path.toLatin1(),crop1Img_temp);
        cvSaveImage(crop2Img_temp_Path.toLatin1(),crop2Img_temp);
    }
    //################对glint点附近的9*9重新赋值  left########################
    //add by douzi 20180827
    IplImage *final1Img = cvCloneImage(crop1Img);
    IplImage *final2Img = cvCloneImage(crop2Img);
    //add end

//add by sun 20180905
    int tt = pcenter1.x + 16;
    pcenter1.x = pcenter1.y + 24;
    pcenter1.y = tt;

    tt = pcenter2.x + 16;
    pcenter2.x = pcenter2.y + 24;
    pcenter2.y = tt;

    //add for quint test*************************************
    radius1, radius2;
    if(num <= 6) // 计算眼位
    {
        double quint1_LR;
        double quint2_LR;
        double quint1_UD;
        double quint2_UD;

        double midd1_LR;
        double middd1_LR;
        double midd2_LR;
        double middd2_LR;
        double midd1_UD;
        double middd1_UD;
        double midd2_UD;
        double middd2_UD;

        midd1_LR = pcenter1.y;
        midd2_LR = pcenter2.y;
        midd1_UD = pcenter1.x;
        midd2_UD = pcenter2.x;

//         qDebug()<<num<<"xieshi-pcenter1.x:"<<pcenter1.x<<",pcenter1.y:"<<pcenter1.y<<",pcenter2.x:"<<pcenter2.x<<",pcenter2.y:"<<pcenter2.y;

        middd1_LR = (midd1_LR - 32) / 32;
        middd2_LR = (midd2_LR - 32) / 32;
        middd1_UD = (midd1_UD - 32) / 32;
        middd2_UD = (midd2_UD - 32) / 32;

        quint1_LR = round(asin(middd1_LR) / PI * 180 * 0.5);
        quint2_LR = round(asin(middd2_LR) / PI * 180 * 0.5);
        quint1_UD = round(asin(middd1_UD) / PI * 180 * 0.5);
        quint2_UD = round(asin(middd2_UD) / PI * 180 * 0.5);

        strabismusMutex.lock();
        g_strabismusValue.leftEyeLR += quint1_LR;
        g_strabismusValue.leftEyeUD += quint1_UD;
        g_strabismusValue.rightEyeLR += quint2_LR;
        g_strabismusValue.rightEyeUD += quint2_UD;
        strabismusMutex.unlock();
    }


    //add end*****************************************************


    qDebug() << num << "----begin remove high glint-----";


////end

    if(pcenter1.x - 5 >= 0 && pcenter1.x + 5 <= 64 && pcenter1.y - 5 >= 0 && pcenter1.y + 5 <= 64)
        for (int i = 0; i < crop1Img->height; i++)
        {
            for (int j = 0; j < crop1Img->width; j++)
            {

                CvScalar scrop1 = cvGet2D(crop1Img, i, j);

                CvScalar ssub;

                if (i >= pcenter1.x - GLINTREGION && i <= pcenter1.x + GLINTREGION && j <= pcenter1.y + GLINTREGION && j >= pcenter1.y - GLINTREGION)
                {
                    //                CvScalar nextl = cvGet2D(crop1Img, i - 1, j);
                    //                CvScalar nextc = cvGet2D(crop1Img, i, j - 1);
                    //                CvScalar nextr = cvGet2D(crop1Img, i - 1, j - 1);

                    //			//add by douzi 20180827
                    //                CvScalar nexta = cvGet2D(crop1Img, i - 1, j + 1);  //08.24
                    //				CvScalar nextb = cvGet2D(crop1Img, i + 1, j - 1);   //08.24

                    //				//ssub.val[0] = (nextl.val[0] + nextc.val[0] + nextr.val[0]) / 3;
                    //				ssub.val[0] = (nextl.val[0] + nextc.val[0] + nextr.val[0] + nexta.val[0] + nextb.val[0]) / 5;  //0824 by ron ,用五个点的平均值来替代这个点
                    //			//add end

                    double pcenter1X = (double)pcenter1.x;
                    double pcenter1Y = (double)pcenter1.y;

                    glintValue[0] = (double)(cof1[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 5).val[0]);
                    glintValue[1] = (double)(cof2[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 4).val[0]);
                    glintValue[2] = (double)(cof3[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 3).val[0]);
                    glintValue[3] = (double)(cof4[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 2).val[0]);
                    glintValue[4] = (double)(cof5[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 1).val[0]);
                    glintValue[5] = (double)(cof6[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 0).val[0]);
                    glintValue[6] = (double)(cof7[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 1).val[0]);
                    glintValue[7] = (double)(cof8[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 2).val[0]);
                    glintValue[8] = (double)(cof9[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 3).val[0]);
                    glintValue[9] = (double)(cof10[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 4).val[0]);
                    glintValue[10] = (double)(cof11[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 5).val[0]);

                    glintValue[11] = (double)(cof12[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 4, pcenter1Y - 5).val[0]);
                    glintValue[12] = (double)(cof13[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 3, pcenter1Y - 5).val[0]);
                    glintValue[13] = (double)(cof14[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 2, pcenter1Y - 5).val[0]);
                    glintValue[14] = (double)(cof15[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 1, pcenter1Y - 5).val[0]);
                    glintValue[15] = (double)(cof16[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 0, pcenter1Y - 5).val[0]);
                    glintValue[16] = (double)(cof17[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 1, pcenter1Y - 5).val[0]);
                    glintValue[17] = (double)(cof18[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 2, pcenter1Y - 5).val[0]);
                    glintValue[18] = (double)(cof19[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 3, pcenter1Y - 5).val[0]);
                    glintValue[19] = (double)(cof20[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 4, pcenter1Y - 5).val[0]);

                    glintValue[20] = (double)(cof21[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 4, pcenter1Y + 5).val[0]);
                    glintValue[21] = (double)(cof22[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 3, pcenter1Y + 5).val[0]);
                    glintValue[22] = (double)(cof23[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 2, pcenter1Y + 5).val[0]);
                    glintValue[23] = (double)(cof24[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 1, pcenter1Y + 5).val[0]);
                    glintValue[24] = (double)(cof25[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 0, pcenter1Y + 5).val[0]);
                    glintValue[25] = (double)(cof26[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 1, pcenter1Y + 5).val[0]);
                    glintValue[26] = (double)(cof27[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 2, pcenter1Y + 5).val[0]);
                    glintValue[27] = (double)(cof28[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 3, pcenter1Y + 5).val[0]);
                    glintValue[28] = (double)(cof29[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 4, pcenter1Y + 5).val[0]);

                    glintValue[29] = (double)(cof30[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 5).val[0]);
                    glintValue[30] = (double)(cof31[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 4).val[0]);
                    glintValue[31] = (double)(cof32[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 3).val[0]);
                    glintValue[32] = (double)(cof33[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 2).val[0]);
                    glintValue[33] = (double)(cof34[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 1).val[0]);
                    glintValue[34] = (double)(cof35[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 0).val[0]);
                    glintValue[35] = (double)(cof36[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 1).val[0]);
                    glintValue[36] = (double)(cof37[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 2).val[0]);
                    glintValue[37] = (double)(cof38[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 3).val[0]);
                    glintValue[38] = (double)(cof39[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 4).val[0]);
                    glintValue[39] = (double)(cof40[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 5).val[0]);


                    double sumValue = 0;
                    for(int i = 0; i < 40; i++)
                    {
                        sumValue += glintValue[i];
                    }

                    ssub.val[0] = sumValue;
                    //                //qDebug()<< num << "sumValue1:" << sumValue << "ssub.val[0]" << ssub.val[0];
                }
                else
                {
                    ssub.val[0] = scrop1.val[0];
                    //                //qDebug() << num << "glint 1 out of range!!!!!!!!!!!!!!!!!!!!";

                }

                cvSet2D(final1Img, i, j, ssub);

            }
        }

//    qDebug()<<num<<"*****************qqqqqqqqqqqqqqqqqqqq**************"<<"runTask ThreadID = "<<QThread::currentThreadId();
    //################对glint点附近的9*9重新赋值  right  ########################
    if(pcenter2.x - 5 >= 0 && pcenter2.x + 5 <= 64 && pcenter2.y - 5 >= 0 && pcenter2.y + 5 <= 64)
        for (int i = 0; i < crop2Img->height; i++)
        {
            for (int j = 0; j < crop2Img->width; j++)
            {

                CvScalar scrop1 = cvGet2D(crop2Img, i, j);

                CvScalar ssub;

                if (i >= pcenter2.x - GLINTREGION && i <= pcenter2.x + GLINTREGION && j <= pcenter2.y + GLINTREGION && j >= pcenter2.y - GLINTREGION)
                {
//                CvScalar nextl = cvGet2D(crop2Img, i - 1, j);
//                CvScalar nextc = cvGet2D(crop2Img, i, j - 1);
//                CvScalar nextr = cvGet2D(crop2Img, i - 1, j - 1);

//				//edit by douzi 20180827
//				CvScalar nexta = cvGet2D(crop1Img, i - 1, j + 1);  //08.24
//				CvScalar nextb = cvGet2D(crop1Img, i + 1, j - 1);   //08.24

//				//ssub.val[0] = (nextl.val[0] + nextc.val[0] + nextr.val[0]) / 3;
//				ssub.val[0] = (nextl.val[0] + nextc.val[0] + nextr.val[0] + nexta.val[0] + nextb.val[0]) / 5;   //0824 by ron 用5个点的平均值来代替这个点
//				//end
                    double pcenter2X = (double)pcenter2.x;
                    double pcenter2Y = (double)pcenter2.y;

                    glintValue[0] = (double)(cof1[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y - 5).val[0]);
                    glintValue[1] = (double)(cof2[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y - 4).val[0]);
                    glintValue[2] = (double)(cof3[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y - 3).val[0]);
                    glintValue[3] = (double)(cof4[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y - 2).val[0]);
                    glintValue[4] = (double)(cof5[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y - 1).val[0]);
                    glintValue[5] = (double)(cof6[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y - 0).val[0]);
                    glintValue[6] = (double)(cof7[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y + 1).val[0]);
                    glintValue[7] = (double)(cof8[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y + 2).val[0]);
                    glintValue[8] = (double)(cof9[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y + 3).val[0]);
                    glintValue[9] = (double)(cof10[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y + 4).val[0]);
                    glintValue[10] = (double)(cof11[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 5, pcenter2Y + 5).val[0]);


                    glintValue[11] = (double)(cof12[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 4, pcenter2Y - 5).val[0]);
                    glintValue[12] = (double)(cof13[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 3, pcenter2Y - 5).val[0]);
                    glintValue[13] = (double)(cof14[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 2, pcenter2Y - 5).val[0]);
                    glintValue[14] = (double)(cof15[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 1, pcenter2Y - 5).val[0]);
                    glintValue[15] = (double)(cof16[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 0, pcenter2Y - 5).val[0]);
                    glintValue[16] = (double)(cof17[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 1, pcenter2Y - 5).val[0]);
                    glintValue[17] = (double)(cof18[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 2, pcenter2Y - 5).val[0]);
                    glintValue[18] = (double)(cof19[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 3, pcenter2Y - 5).val[0]);
                    glintValue[19] = (double)(cof20[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 4, pcenter2Y - 5).val[0]);

                    glintValue[20] = (double)(cof21[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 4, pcenter2Y + 5).val[0]);
                    glintValue[21] = (double)(cof22[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 3, pcenter2Y + 5).val[0]);
                    glintValue[22] = (double)(cof23[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 2, pcenter2Y + 5).val[0]);
                    glintValue[23] = (double)(cof24[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 1, pcenter2Y + 5).val[0]);
                    glintValue[24] = (double)(cof25[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X - 0, pcenter2Y + 5).val[0]);
                    glintValue[25] = (double)(cof26[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 1, pcenter2Y + 5).val[0]);
                    glintValue[26] = (double)(cof27[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 2, pcenter2Y + 5).val[0]);
                    glintValue[27] = (double)(cof28[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 3, pcenter2Y + 5).val[0]);
                    glintValue[28] = (double)(cof29[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 4, pcenter2Y + 5).val[0]);

                    glintValue[29] = (double)(cof30[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y - 5).val[0]);
                    glintValue[30] = (double)(cof31[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y - 4).val[0]);
                    glintValue[31] = (double)(cof32[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y - 3).val[0]);
                    glintValue[32] = (double)(cof33[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y - 2).val[0]);
                    glintValue[33] = (double)(cof34[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y - 1).val[0]);
                    glintValue[34] = (double)(cof35[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y - 0).val[0]);
                    glintValue[35] = (double)(cof36[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y + 1).val[0]);
                    glintValue[36] = (double)(cof37[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y + 2).val[0]);
                    glintValue[37] = (double)(cof38[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y + 3).val[0]);
                    glintValue[38] = (double)(cof39[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y + 4).val[0]);
                    glintValue[39] = (double)(cof40[i - pcenter2.x + 4][j - pcenter2.y + 4] * cvGet2D(crop2Img, pcenter2X + 5, pcenter2Y + 5).val[0]);


                    double sumValue2 = 0;
                    for(int i = 0; i < 40; i++)
                    {
                        sumValue2 += glintValue[i];
                    }

                    ssub.val[0] = sumValue2;
//                //qDebug() << num << "sumValue2:" << sumValue2 << "ssub.val[0]" << ssub.val[0];

                }
                else
                {
                    ssub.val[0] = scrop1.val[0];
//                //qDebug() << num <<"glint 2 out of range!!!!!!!!!!!!!!!!!!!!";
                }

                cvSet2D(final2Img, i, j, ssub);

            }
        }
//    qDebug()<<num<<"*****************rrrrrrrrrrrrrrrrrrrrr**************"<<"runTask ThreadID = "<<QThread::currentThreadId();

#endif

//    IplImage* crop1RotateImg = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);
//    IplImage* crop2RotateImg = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);

//    rotateImage(final1Img, crop1RotateImg, angle);
//    rotateImage(final2Img, crop2RotateImg, angle);

    //add by sun 20180906
    int startx1, startx2, starty1, starty2;
    IplImage *leftImg, *rightImg;
    /*   if(radius1<16||radius2<16){  close by sun 20180910
           startx1 = crop1RotateImg->width / 2 - 9, starty1 = crop1RotateImg->height / 2 - KROIH;

           startx2 = crop2RotateImg->width / 2 - 9, starty2 = crop2RotateImg->height / 2 - KROIH;

           cvSetImageROI(crop1RotateImg, cvRect(startx1, starty1, 19, KROIHA));
           IplImage *crop1finishStdImg = cvCreateImage(cvSize(19, KROIHA), crop1RotateImg->depth, crop1RotateImg->nChannels);

           cvCopy(crop1RotateImg, crop1finishStdImg, NULL);
           cvResetImageROI(crop1RotateImg);

           cvSetImageROI(crop2RotateImg, cvRect(startx2, starty2, 19, KROIHA));
           IplImage *crop2finishStdImg = cvCreateImage(cvSize(19, KROIHA), crop2RotateImg->depth, crop2RotateImg->nChannels);

           cvCopy(crop2RotateImg, crop2finishStdImg, NULL);
           cvResetImageROI(crop2RotateImg);

           leftImg = cvCloneImage(crop1finishStdImg);
           rightImg = cvCloneImage(crop2finishStdImg);

           cvReleaseImage(&crop1finishStdImg);
           cvReleaseImage(&crop2finishStdImg);

       }
       else{
    */
    //qDebug()<<num<<"*****************wwwwwwwwwwwwwwwwwww**************"<<"runTask ThreadID = "<<QThread::currentThreadId();

    //
    int R_KROIW = 11;
    int L_KROIW = 11;
    int KROIH = 5;
    int R_KROIWA = 23;
    int L_KROIWA = 23;
    int KROIHA = 11;

    if(g_SaturationCenterR.z <= 15)
    {
        R_KROIWA = 17;
        R_KROIW = 8;
    }
    if(g_SaturationCenterL.z <= 15)
    {
        L_KROIWA = 17;
        L_KROIW = 8;
    }

    //
    startx1 = final1Img->width / 2 - L_KROIW, starty1 = final1Img->height / 2 - KROIH;
    startx2 = final2Img->width / 2 - R_KROIW, starty2 = final2Img->height / 2 - KROIH;

    //    int GcDist1 = sqrt((pcenter1.x-center1.x)*(pcenter1.x-center1.x) + (pcenter1.y-center1.y)*(pcenter1.y-center1.y));
    //    int GcDist2 = sqrt((pcenter2.x-center2.x)*(pcenter2.x-center2.x) + (pcenter2.y-center2.y)*(pcenter2.y-center2.y));

    //    int startx1 = pcenter1.x - KROIW, starty1 = pcenter1.y - KROIH;
    //    int startx2 = pcenter2.x - KROIW, starty2 = pcenter2.y - KROIH;

    cvSetImageROI(final1Img, cvRect(startx1, starty1, L_KROIWA, KROIHA));
    IplImage *crop1finishStdImg = cvCreateImage(cvSize(L_KROIWA, KROIHA), final1Img->depth, final1Img->nChannels);

    cvCopy(final1Img, crop1finishStdImg, NULL);
    cvResetImageROI(final1Img);

    cvSetImageROI(final2Img, cvRect(startx2, starty2, R_KROIWA, KROIHA));
    IplImage *crop2finishStdImg = cvCreateImage(cvSize(R_KROIWA, KROIHA), final2Img->depth, final2Img->nChannels);

    cvCopy(final2Img, crop2finishStdImg, NULL);
    cvResetImageROI(final2Img);

    leftImg = cvCloneImage(crop1finishStdImg);
    rightImg = cvCloneImage(crop2finishStdImg);


//    }

    qDebug() << num << "****finish remove high glint******";

    if (calcResultState_Succ == g_CalcResultState)     //瞳孔计算通过
    {
        mapMutex.lock();
        //qDebug()<<num<<"start insert eyemap"<<QThread::currentThreadId();
        g_pupilImgLeft.insert(num, leftImg);    //插入瞳孔图片
        g_pupilImgRight.insert(num, rightImg);
        QString numstr = QString::number(num) + " succ \n";
        //pupil_estimate.append(numstr);
        QByteArray ba = numstr.toLatin1(); // must
        char* str=ba.data();
        strcat(pupil_estimate,str);
        qDebug()<<num<<"insert eyemap sucess "<<QThread::currentThreadId();
        mapMutex.unlock();
    }
    else        //瞳孔计算不通过
    {
        //qDebug()<<num<<"error=false,start release leftImg,rightImg...";
        cvReleaseImage(&leftImg);   //释放左瞳孔图片数据
        cvReleaseImage(&rightImg);  //释放右瞳孔图片数据
        QString numstr = QString::number(num) + " fault \n";
        //pupil_estimate.append(numstr);
        QByteArray ba = numstr.toLatin1(); // must
        char* str=ba.data();
        strcat(pupil_estimate,str);
        qDebug()<<num<<"complete release leftImg,rightImg!";
    }

    qDebug() << num << "--before release";
    cvReleaseImage(&crop1finishStdImg); //???
    cvReleaseImage(&crop2finishStdImg);//???
    cvReleaseImage(&crop1RotateImg);
    cvReleaseImage(&crop2RotateImg);
    cvReleaseImage(&crop1Img);
    cvReleaseImage(&crop2Img);
    cvReleaseImage(&final1Img);
    cvReleaseImage(&final2Img);
    cvReleaseImage(&crop1Img_temp);
    cvReleaseImage(&crop2Img_temp);

    //
    qDebug() << num << "--cvRelease,processPic return 1";
    return 1;
}

int RunTask::single_processPic(IplImage *_Img, int num)
{

//    qInstallMessageHandler(runtaskOutputMessage);

    if (_Img == NULL)
    {
//        qDebug()<<"Img == NULL,return";
        g_CalcResultState = calcResultState_Unknown;

        return 0;
    }


    int angle = 0;
    if (num >= 0 && num <= 6)
    {
        angle = 0;
    }
    if (num >= 7 && num <= 12)
    {
        angle = 60;
    }
    if (num >= 13 && num <= 18)
    {
        angle = 120;
    }

    if (num == 19 || num == 20)
    {
        angle = 139;
    }
    if (num == 21 || num == 22)
    {
        angle = 41;
    }
    qDebug() << "++++++++single runtask ++++++++++" << num << "+++++++++++++++++++";
    IplImage *Img = cvCreateImage(cvSize(IMG_WIDTH / 2, IMG_HEIGHT), IPL_DEPTH_8U, 1);

    if (singleDoubleEyeMode_Left == g_SingleDoubleEye)
        cvSetImageROI(_Img, cvRect(IMG_WIDTH / 2, 0, IMG_WIDTH / 2, IMG_HEIGHT));
    else if (singleDoubleEyeMode_Right == g_SingleDoubleEye)
        cvSetImageROI(_Img, cvRect(0, 0, IMG_WIDTH / 2, IMG_HEIGHT));

    cvCopy(_Img, Img);
    cvResetImageROI(_Img);

    int KROIW = 11;
    int KROIH = 5;
    int KROIWA = 23;
    int KROIHA = 11;

    //add for pc running test
//       saturationCenterL.z = 23;
//       saturationCenterR.z = 23;
//       g_saturationPd = 391;
    //****************************

    if (singleDoubleEyeMode_Right == g_SingleDoubleEye)
    {
        if(g_SaturationCenterR.z <= 14)
        {
            KROIWA = 17;
            KROIW = 8;
        }
    }
    else if (singleDoubleEyeMode_Left == g_SingleDoubleEye)
    {
        if(g_SaturationCenterL.z <= 14)
        {
            KROIWA = 17;
            KROIW = 8;
        }
    }


    CvPoint3D32f avalPoint3D[2];
    int pupilDis = 0;
    int feedback = single_pyrDetect(Img, avalPoint3D, &pupilDis, num);

    qDebug() << num << "--runtask--finish single_pyrDetect";

    int radius1;
    CvPoint center1;

    if (feedback == 1)
    {
        center1.x = avalPoint3D[0].x;
        center1.y = avalPoint3D[0].y;

        radius1 = avalPoint3D[0].z;

        qDebug() << "center1.x = " << center1.x << "; center1.y = " << center1.y;


        if (!(calcResultState_Succ == g_CalcResultState))
        {
            qDebug() << num << "error!!!!!!feedback ==" << feedback;

            cvReleaseImage(&Img);

            return 0;
        }

//        ////qDebug()<<"the  "<<num<<"  Circle +++++  center1 is : ("<<
//                  center1.x<<", "<<center1.y<<"  ) ;  center2 = ("<<
//                  center2.x<<" , "<<center2.y<<" ), r1 = "<<
//                  radius1<<", r2 ="<<radius2<<"runTask ThreadID = "<<QThread::currentThreadId();

    }
    else
    {
        qDebug() << num << "processPic++feedback != 1++++return";
        g_CalcResultState = calcResultState_Unknown;

        cvReleaseImage(&Img);

        return 0;
    }




    ///---************************************************------------------------------------
    //test 1stly
    //需要对图像去除Glint,并去掉高频Noise
#if 1


    CvRect cropRect1;

    cropRect1.x = center1.x - 32;
    cropRect1.y = center1.y - 32;
    cropRect1.width = 65;
    cropRect1.height = 65;

    if (cropRect1.x < 0 || (cropRect1.x + cropRect1.width) > Img->width
            || cropRect1.y < 0 || (cropRect1.y + cropRect1.height) > Img->height)
    {
        qDebug() << "g_CalcResultState = " << (int)g_CalcResultState << ",cropRect1.x < 0  +++++++++++++++++++return";
        g_CalcResultState = calcResultState_Unknown;

        cvReleaseImage(&Img);

        return 0;
    }


    qDebug() << "*********single********1111111**************" << "runTask ThreadID = " << QThread::currentThreadId();
    IplImage *crop1Img = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);
    IplImage *crop1RotateImg = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);
    //
    cvSetImageROI(Img, cropRect1);
    cvCopy(Img, crop1RotateImg, NULL);
    cvResetImageROI(Img);
    qDebug() << "********single*********22222222**************" << "runTask ThreadID = " << QThread::currentThreadId();

    QString currentIDPath = "/media/photo/" + g_currentSubjectNum;
    QDir dir;
    if(!QDir(currentIDPath).exists())
    {
        dir.mkdir(currentIDPath);
    }
    QString crop1ImgPath = currentIDPath + "/cropImg/";
    if(!QDir(crop1ImgPath).exists())
    {
        dir.mkdir(crop1ImgPath);
    }

    rotateImage(crop1RotateImg, crop1Img, angle);
    IplImage *crop1Img_temp = cvCreateImage(cvSize(33, 17), IPL_DEPTH_8U, 1);

    QString rotateImgPath = currentIDPath + "/rotateImg/" ;
    if(!QDir(rotateImgPath).exists())
        dir.mkdir(rotateImgPath);
    cvSaveImage(rotateImgPath.toLatin1() + QString::number(num, 10).toLatin1() + ".bmp", crop1Img);

    CvRect  cropRect1_temp;

    cropRect1_temp.x = 16;
    cropRect1_temp.y = 24;
    cropRect1_temp.width = 33;
    cropRect1_temp.height = 17;

    cvSetImageROI(crop1Img, cropRect1_temp);
    cvCopy(crop1Img, crop1Img_temp, NULL);
    cvResetImageROI(crop1Img);



    qDebug() << "*****************QQQQQQQQQQQ**************" << "runTask ThreadID = " << QThread::currentThreadId();

    CvPoint pcenter1;

    if(Screen_model == 1)  //    //add by tao 2020.7.16(普通模式,未加限制)
    {
        //get the max point by first bmp
        double MinValue1 = 0.0;
        double MaxValue1 = 0.0;
        CvPoint MinLocation1;
        CvPoint MaxLocation1;

        cvMinMaxLoc(crop1Img_temp, &MinValue1, &MaxValue1, &MinLocation1, &MaxLocation1);

        //qDebug()<<"*****************33333333**************"<<"runTask ThreadID = "<<QThread::currentThreadId();
    //    if(num==1)
    //        ////qDebug()<<num<< " -----------------MaxLocation1:"<<MaxLocation1.x<<","<<MaxLocation1.y;

        pcenter1 = MaxLocation1;
    }
    else if(Screen_model == 2)          //2020.7.16  专业模式,加限制
    {
        //检测瞳孔亮点
        bool Blob = pupilBlobDetect(crop1Img_temp,pcenter1);

        if(!Blob)
        {
            qDebug() << "error = false ,pupilBlobDetect failed   +++++++++++++++++++return";
            g_CalcResultState = calcResultState_Unknown;
            cvReleaseImage(&crop1RotateImg);
            cvReleaseImage(&crop1Img);
            cvReleaseImage(&crop1Img_temp);
            cvReleaseImage(&Img);
            return 0;
        }

        if(saveImage)
        {
            QString crop1Img_temp_Path = QString("/media/photo/%1/crop1Img_temp_%2.bmp").arg(g_currentSubjectNum).arg(num);
            cvSaveImage(crop1Img_temp_Path.toLatin1(),crop1Img_temp);
        }
    }
    //################对glint点附近的9*9重新赋值  left########################
    //add by douzi 20180827
    ////qDebug()<<"crop1Img->width = "<<crop1Img->width<<"; crop1Img->height = "<<crop1Img->height;

    IplImage *final1Img = cvCloneImage(crop1Img);
    //add end

//add by sun 20180905
    int tt = pcenter1.x + 16;
    pcenter1.x = pcenter1.y + 24;
    pcenter1.y = tt;

    //add for quint test*************************************
    radius1;
    if(num <= 6)
    {
        if(num == 1)
            g_strabismusValue.clear();

        double quint1_LR;
        double quint1_UD;
        double midd1_LR;
        double middd1_LR;
        double midd1_UD;
        double middd1_UD;

//         midd1_LR = pcenter1.x;
//         midd2_LR = pcenter2.x;
//         midd1_UD = pcenter1.y;
//         midd2_UD = pcenter2.y;

        midd1_LR = pcenter1.y;
        midd1_UD = pcenter1.x;

//         qDebug()<<num<<"pcenter1.x:"<<pcenter1.x<<",pcenter1.y:"<<pcenter1.y;

        middd1_LR = (midd1_LR - 32) / 32;
        middd1_UD = (midd1_UD - 32) / 32;

        quint1_LR = round(asin(middd1_LR) / PI * 180 * 0.5);
        quint1_UD = round(asin(middd1_UD) / PI * 180 * 0.5);

//         qDebug()<<num<<"quint1_LR:"<<quint1_LR<<",quint1_UD:"<<quint1_UD;

        if (singleDoubleEyeMode_Right == g_SingleDoubleEye)
        {
            g_strabismusValue.rightEyeLR += quint1_LR;
            g_strabismusValue.rightEyeUD += quint1_UD;
        }
        else if (singleDoubleEyeMode_Left == g_SingleDoubleEye)
        {
            g_strabismusValue.leftEyeLR += quint1_LR;
            g_strabismusValue.leftEyeUD += quint1_UD;
        }

    }



    //add end*****************************************************



    qDebug() << num << "121212121_single_remove high glint point";


////end

    if(pcenter1.x - 5 >= 0 && pcenter1.x + 5 <= 64 && pcenter1.y - 5 >= 0 && pcenter1.y + 5 <= 64)
        for (int i = 0; i < crop1Img->height; i++)
        {
            for (int j = 0; j < crop1Img->width; j++)
            {
                CvScalar scrop1 = cvGet2D(crop1Img, i, j);

                CvScalar ssub;

                if (i >= pcenter1.x - GLINTREGION && i <= pcenter1.x + GLINTREGION && j <= pcenter1.y + GLINTREGION && j >= pcenter1.y - GLINTREGION)
                {
                    double pcenter1X = (double)pcenter1.x;
                    double pcenter1Y = (double)pcenter1.y;

                    glintValue[0] = (double)(cof1[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 5).val[0]);
                    glintValue[1] = (double)(cof2[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 4).val[0]);
                    glintValue[2] = (double)(cof3[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 3).val[0]);
                    glintValue[3] = (double)(cof4[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 2).val[0]);
                    glintValue[4] = (double)(cof5[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 1).val[0]);
                    glintValue[5] = (double)(cof6[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y - 0).val[0]);
                    glintValue[6] = (double)(cof7[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 1).val[0]);
                    glintValue[7] = (double)(cof8[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 2).val[0]);
                    glintValue[8] = (double)(cof9[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 3).val[0]);
                    glintValue[9] = (double)(cof10[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 4).val[0]);
                    glintValue[10] = (double)(cof11[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 5, pcenter1Y + 5).val[0]);


                    glintValue[11] = (double)(cof12[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 4, pcenter1Y - 5).val[0]);
                    glintValue[12] = (double)(cof13[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 3, pcenter1Y - 5).val[0]);
                    glintValue[13] = (double)(cof14[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 2, pcenter1Y - 5).val[0]);
                    glintValue[14] = (double)(cof15[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 1, pcenter1Y - 5).val[0]);
                    glintValue[15] = (double)(cof16[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 0, pcenter1Y - 5).val[0]);
                    glintValue[16] = (double)(cof17[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 1, pcenter1Y - 5).val[0]);
                    glintValue[17] = (double)(cof18[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 2, pcenter1Y - 5).val[0]);
                    glintValue[18] = (double)(cof19[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 3, pcenter1Y - 5).val[0]);
                    glintValue[19] = (double)(cof20[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 4, pcenter1Y - 5).val[0]);

                    glintValue[20] = (double)(cof21[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 4, pcenter1Y + 5).val[0]);
                    glintValue[21] = (double)(cof22[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 3, pcenter1Y + 5).val[0]);
                    glintValue[22] = (double)(cof23[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 2, pcenter1Y + 5).val[0]);
                    glintValue[23] = (double)(cof24[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 1, pcenter1Y + 5).val[0]);
                    glintValue[24] = (double)(cof25[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X - 0, pcenter1Y + 5).val[0]);
                    glintValue[25] = (double)(cof26[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 1, pcenter1Y + 5).val[0]);
                    glintValue[26] = (double)(cof27[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 2, pcenter1Y + 5).val[0]);
                    glintValue[27] = (double)(cof28[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 3, pcenter1Y + 5).val[0]);
                    glintValue[28] = (double)(cof29[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 4, pcenter1Y + 5).val[0]);

                    glintValue[29] = (double)(cof30[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 5).val[0]);
                    glintValue[30] = (double)(cof31[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 4).val[0]);
                    glintValue[31] = (double)(cof32[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 3).val[0]);
                    glintValue[32] = (double)(cof33[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 2).val[0]);
                    glintValue[33] = (double)(cof34[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 1).val[0]);
                    glintValue[34] = (double)(cof35[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y - 0).val[0]);
                    glintValue[35] = (double)(cof36[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 1).val[0]);
                    glintValue[36] = (double)(cof37[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 2).val[0]);
                    glintValue[37] = (double)(cof38[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 3).val[0]);
                    glintValue[38] = (double)(cof39[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 4).val[0]);
                    glintValue[39] = (double)(cof40[i - pcenter1.x + 4][j - pcenter1.y + 4] * cvGet2D(crop1Img, pcenter1X + 5, pcenter1Y + 5).val[0]);


                    double sumValue = 0;
                    for(int i = 0; i < 40; i++)
                    {
                        sumValue += glintValue[i];
                    }

                    ssub.val[0] = sumValue;
//                //qDebug()<< num << "sumValue1:" << sumValue << "ssub.val[0]" << ssub.val[0];
                }
                else
                {
                    ssub.val[0] = scrop1.val[0];
//                //qDebug() << num << "glint 1 out of range!!!!!!!!!!!!!!!!!!!!";

                }

                cvSet2D(final1Img, i, j, ssub);

            }
        }

    qDebug() << num << "*****************qqqqqqqqqqqqqqqqqqqq**************" << "runTask ThreadID = " << QThread::currentThreadId();

#endif

//    IplImage* crop1RotateImg = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);
//    IplImage* crop2RotateImg = cvCreateImage(cvSize(65, 65), IPL_DEPTH_8U, 1);

//    rotateImage(final1Img, crop1RotateImg, angle);
//    rotateImage(final2Img, crop2RotateImg, angle);

    //add by sun 20180906
    int startx1, starty1;
    IplImage *eyeMapImg1;

    qDebug() << num << "*****************wwwwwwwwwwwwwwwwwww**************" << "runTask ThreadID = " << QThread::currentThreadId();
    startx1 = final1Img->width / 2 - KROIW;
    starty1 = final1Img->height / 2 - KROIH;

    cvSetImageROI(final1Img, cvRect(startx1, starty1, KROIWA, KROIHA));
    IplImage *crop1finishStdImg = cvCreateImage(cvSize(KROIWA, KROIHA), final1Img->depth, final1Img->nChannels);

    cvCopy(final1Img, crop1finishStdImg, NULL);
    cvResetImageROI(final1Img);

    eyeMapImg1 = cvCloneImage(crop1finishStdImg);

    cvRectangleR(final1Img, cvRect(startx1, starty1, KROIWA, KROIHA), cvScalar(255, 0, 0));



    if (calcResultState_Succ == g_CalcResultState)
    {
        mapMutex.lock();
        if (singleDoubleEyeMode_Left == g_SingleDoubleEye)
            g_pupilImgLeft.insert(num, eyeMapImg1);
        else if (singleDoubleEyeMode_Right == g_SingleDoubleEye)
            g_pupilImgRight.insert(num, eyeMapImg1);

        qDebug() << "insert eyemap sucess " << num;
        mapMutex.unlock();

        if(saveImage)
        {
            //add for save eyeMap photo by sun 20180827

            QDir mydir;
            mydir.setPath(saveImagePath);
            mydir.setFilter(QDir::Dirs);
            mydir.setSorting(QDir::Name);
            if (!mydir.exists())
            {
                if(!mydir.mkdir("/media/photo/" + g_currentSubjectNum))
                {
                    qDebug() << "create new dir is fail,the ID = " << g_currentSubjectNum << "not save image";
                }
                else
                    qDebug() << "create dir:" << saveImagePath;
            }
            QString numString = QString::number(num, 10);
            QString singleFinalbuff = saveImagePath + "/singleFinal_" + numString + ".bmp";
            QString singleCropImg = saveImagePath + "/singleCropImg_" + numString + ".bmp";
            QByteArray singleba = singleFinalbuff.toLatin1();
//            cvSaveImage(singleba.data(), eyeMapImg1);
            cvSaveImage(singleCropImg.toLatin1().data(), final1Img);

            qDebug() << "save single eyeMap photo :" << singleba.data();
        }
        //end
    }
    else
    {
        qDebug() << num << "g_CalcResultState = " << (int)g_CalcResultState << ",start release leftImg,rightImg...";
        cvReleaseImage(&eyeMapImg1);
        qDebug() << num << "complete release leftImg,rightImg!";
    }


    cvReleaseImage(&crop1finishStdImg); //???
    cvReleaseImage(&crop1RotateImg);
    cvReleaseImage(&crop1Img);
    cvReleaseImage(&final1Img);
    cvReleaseImage(&crop1Img_temp);
    cvReleaseImage(&Img);//add by douzi 20180929 ***

    qDebug() << num << "--cvRelease,processPic return 1";
    return 1;
}

void RunTask::addPupilCenter(QPoint leftPt, QPoint rightPt,int Num)
{
    eyeCenterMuter.lock();

    leftEyeCenter.insert(Num,leftPt);
    rightEyeCenter.insert(Num,rightPt);

    if(leftEyeCenter.size() >= 22 ||leftEyeCenter.size() >= 22)
        distanceCheck();

    eyeCenterMuter.unlock();
}

bool RunTask::distanceCheck( )
{
    moveCnt = 0;

//    QString filePath = QString("/media/%1_eyeCenterTrack.txt").arg(g_currentSubjectNum);
//    QFile recordFile(filePath);
//    recordFile.open(QIODevice::WriteOnly | QIODevice::Append);
//    QTextStream text_stream(&recordFile);


    for(int i = 1;i< leftEyeCenter.size() - 1;i++)
    {
        QPoint prePt = leftEyeCenter.value(i);
        QPoint nextPt = leftEyeCenter.value(++i);

        double dist = fabs(pow(prePt.x() - nextPt.x(),2) + pow(prePt.y() - nextPt.y(),2));

//        text_stream<<"leftEye:--num "<<i-1<<"-"<<i<<" dist="<<dist<<",saturationValue="<<saturationValue<<"\r\n";

        if(dist > 120){
            qDebug()<<"move too fast!";
            moveCnt++;
        }
    }

    for(int i = 1;i< rightEyeCenter.size() - 1;i++)
    {
        QPoint prePt = rightEyeCenter.value(i);
        QPoint nextPt = rightEyeCenter.value(++i);

        double dist = fabs(pow(prePt.x() - nextPt.x(),2) + pow(prePt.y() - nextPt.y(),2));

        qDebug()<<"num"<<i-1<<"and"<<i<<" dist="<<dist;
//      text_stream<<"rightEye:--num "<<i-1<<"-"<<i<<" dist="<<dist<<",saturationValue="<<saturationValue<<"\r\n";

        if(dist > 120){
            qDebug()<<"move too fast!";
            moveCnt++;
        }
    }

//    text_stream.flush();
//    recordFile.flush();
//    recordFile.close();
//    qDebug()<<"moveCnt:"<<moveCnt;
    leftEyeCenter.clear();
    rightEyeCenter.clear();

    return true;
}

int RunTask::pyrDetect0(IplImage *pyrImg, CvPoint3D32f *avalPoint3D, int *pupilDis, int num)
{
    if (pyrImg == NULL)
    {
        qDebug() << num << "pyrImg == NULL";
        return 0;
    }
    bool autoThresholdMode = false;
    bool bSpotCoarseLocation = true;
    int MaxLocationhR;
    int MaxLocationhL;
    double ThresHcL;
    double ThresHcR;
    int varThresholdStateL = 0;
    int varThresholdStateR = 0;
    int w = pyrImg->width;
    int h = pyrImg->height;

    QTime detectbegin = QTime::currentTime();
    IplImage *Img = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);
    cvCopy(pyrImg, Img);

    ////////////////////////////瞳孔亮斑初定位///////////////////////////////////////

    // ----------------------------图像降维-----------------------------------
    int ratio =  4;     // 调整为原来1/ratio
    int resized_w = w / ratio;
    int resized_h = h / ratio;
    IplImage *SampleImg = cvCreateImage(cvSize(resized_w, resized_h), IPL_DEPTH_8U, 1);

    uchar *data = (uchar *)SampleImg->imageData;
    for (int i = 0; i < resized_h; i++)
    {
        for (int j = 0; j < resized_w; j++)
        {
            *(data + i * SampleImg->widthStep + j) = ((uchar *)(Img->imageData + i * ratio * Img->widthStep))[j * ratio];
        }
    }
    //cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/SampleImg_"+QString::number(num,10).toLatin1()+".bmp",SampleImg);

    //    qDebug() << num<<"runTask******************图像降维**********************" ;
    IplImage *SpotEhanceSampleImg = cvCreateImage(cvSize(resized_w, resized_h), IPL_DEPTH_8U, 1);
    SpotEnhance(SampleImg, SpotEhanceSampleImg);

    CvPoint bcenter1, bcenter2;
    bcenter1.x = 0;
    bcenter1.y = 0;
    bcenter2.x = 0;
    bcenter2.y = 0;

    int edgeTH = 2;

    if (1 == num) {
        qDebug() << "";
    }

    if(true)
    {
        //梯度图和直方图统计获取二值化阈值                                          // TODO: 这一段处理，加强了瞳孔边沿，但是容易导致二值化后瞳孔边沿不连续而使瞳孔区域破碎，增加识别失败概率？
        //qDebug()<<"**********enter gradient detect mode*********"<<num;
        IplImage *bwImg = cvCreateImage(cvSize(resized_w, resized_h), IPL_DEPTH_8U, 1);
        cvSetZero(bwImg);

        //
#ifdef TEST_MODE
        {
            bool is_show_img = false;
            if (is_show_img) {
                cvShowImage("RunTask::pyrDetect0():SampleImg", SampleImg);
            }
        }
#endif

        // 中值滤波
        cv::Mat tImg0Mat;
        cv::medianBlur(cv::Mat(SampleImg), tImg0Mat, 1);
        IplImage tImg0 = IplImage(tImg0Mat);

//
        IplImage *destbwImg = cvCreateImage(cvSize(resized_w, resized_h), IPL_DEPTH_8U, 1);
        //    cvCopy(subImg1,destbwImg);
        cvSetZero(destbwImg);

        // 梯度加强
        CvScalar tempScalar0;
        double min0d[4];
        double min0dv, max0dv;

        for (int i = 2; i < resized_h - 2; i++)
            for (int j = 2; j < resized_w - 2; j++)
            {

                min0d[0] = cvGet2D(&tImg0, i, j - 1).val[0];
                min0d[1] = cvGet2D(&tImg0, i - 1, j).val[0];
                min0d[2] = cvGet2D(&tImg0, i + 1, j).val[0];
                min0d[3] = cvGet2D(&tImg0, i, j + 1).val[0];

                min0dv = min0d[0];
                max0dv = min0d[0];
                for(int k = 1; k < 4; k++)
                {
                    if(min0dv > min0d[k])
                    {
                        min0dv = min0d[k];
                    }
                    if(max0dv < min0d[k])
                    {
                        max0dv = min0d[k];
                    }
                }
                //            if(min0dv==0)
                //                min0dv = 1;
                if(min0dv <= 3 || max0dv == 255)
                {
                    tempScalar0.val[0] = 0;
                    cvSet2D(bwImg, i, j, tempScalar0);
                }
                else
                {
                    tempScalar0.val[0] = max0dv * (cvGet2D(&tImg0, i, j).val[0]) / min0dv;
                    if(tempScalar0.val[0] > 255)
                        tempScalar0.val[0] = 255;

                    cvSet2D(bwImg, i, j, tempScalar0);

                }

            }
        //        //cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/bwImg_"+QString::number(num,10).toLatin1()+".bmp",bwImg);

        //
#ifdef TEST_MODE
        {
            bool is_show_img = false;
            if (is_show_img) {
                cvShowImage("RunTask::pyrDetect0():bwImg", bwImg);
            }
        }
#endif

        //左右眼分开计算阈值
        IplImage *bwImgR = cvCreateImage(cvSize(resized_w / 2, resized_h), IPL_DEPTH_8U, 1);                  // 左右眼分开计算阈值
        IplImage *bwImgL = cvCreateImage(cvSize(resized_w / 2, resized_h), IPL_DEPTH_8U, 1);
        IplImage *destbwImgR = cvCreateImage(cvSize(resized_w / 2, resized_h), IPL_DEPTH_8U, 1);
        IplImage *destbwImgL = cvCreateImage(cvSize(resized_w / 2, resized_h), IPL_DEPTH_8U, 1);

        cvSetImageROI(bwImg, cvRect(0, 0, resized_w / 2, resized_h));
        cvRepeat(bwImg, bwImgR);
        cvResetImageROI(bwImg);

        cvSetImageROI(bwImg, cvRect(resized_w / 2, 0, resized_w / 2, resized_h));
        cvRepeat(bwImg, bwImgL);
        cvResetImageROI(bwImg);

        //add by sun 20181008
        ///////Riht//////////////////
        //qDebug()<<"enter**********Riht************"<<num;

        //
        int HistogramBins = 256;
        float HistogramRange1[2] = {0, 255};
        float *HistogramRange[1] = {&HistogramRange1[0]};
        CvHistogram *Histogram1 = cvCreateHist(1, &HistogramBins, CV_HIST_ARRAY, HistogramRange);
        cvCalcHist(&bwImgR, Histogram1);

        float  MaxValueH;
        double arc;
        //         double ThresHc;
        ThresHcR = 0;
        cvSetReal1D(Histogram1->bins, 0, 0);
        //qDebug()<< num <<" Histogram1 before= "<<num;
        cvGetMinMaxHistValue(Histogram1, 0, &MaxValueH, 0, &MaxLocationhR);

        for (int i = 0; i < HistogramBins; i++)
        {
            arc = cvGetReal1D(Histogram1->bins, i);
            //         //qDebug()<< num <<" arc = "<<arc;
            //        arc = cvGetReal1D(Histogram1 ,i);
            if ( arc < 2 && i > MaxLocationhR)
            {
                ThresHcR = i;
                break;
            }
        }
        //qDebug()<< "after cvReleaseHist+++++++++++ "<<num;

        cvThreshold(bwImgR, destbwImgR, ThresHcR, 255, CV_THRESH_BINARY);

        ///////left//////////////////
        qDebug() << "enter///////left//////////////////" << num;

        //
        int HistogramBinsL = 256;
        float HistogramRange1L[2] = {0, 255};
        float *HistogramRangeL[1] = {&HistogramRange1L[0]};
        CvHistogram *Histogram1L = cvCreateHist(1, &HistogramBinsL, CV_HIST_ARRAY, HistogramRangeL);
        cvCalcHist(&bwImgL, Histogram1L);

        float  MaxValueHL;
        double arcL;
        //      double ThresHcL;
        ThresHcL = 0;
        cvSetReal1D(Histogram1L->bins, 0, 0);
        //qDebug()<< num <<" Histogram1 before= "<<num;
        cvGetMinMaxHistValue(Histogram1L, 0, &MaxValueHL, 0, &MaxLocationhL);

        for (int i = 0; i < HistogramBinsL; i++)
        {
            arcL = cvGetReal1D(Histogram1L->bins, i);
            //         //qDebug()<< num <<" arc = "<<arc;
            //        arc = cvGetReal1D(Histogram1 ,i);
            //         if ( arc < 8 && i>MaxLocationhL){
            if ( arcL < 2 && i > MaxLocationhL)
            {

                ThresHcL = i;
                break;
            }
        }
        //qDebug()<< "after cvReleaseHist+++++++++++ "<<num;

        //    CvScalar ava0Value = cvAvg(bwImg);
        //      //qDebug() << num <<"-ava0Value = "<< ava0Value.val[0];
        ////qDebug() << num <<"-ava0ValuetImg0 = "<< ava0Value3.val[0];

        //二值化
        cvThreshold(bwImgL, destbwImgL, ThresHcL, 255, CV_THRESH_BINARY);

//          //cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/destbwImgL_"+QString::number(num,10).toLatin1()+".bmp",destbwImgL);
        //qDebug()<<"after///////left//////////////////"<<num;

        //
        cvSetImageROI(destbwImg, cvRect(0, 0, resized_w / 2, resized_h));
        cvRepeat(destbwImgR, destbwImg);
        cvResetImageROI(destbwImg);

        cvSetImageROI(destbwImg, cvRect(resized_w / 2, 0, resized_w / 2, resized_h));
        cvRepeat(destbwImgL, destbwImg);
        cvResetImageROI(destbwImg);

        //
        cvReleaseImage(&destbwImgR);
        cvReleaseImage(&destbwImgL);
        cvReleaseImage(&bwImgR);
        cvReleaseImage(&bwImgL);
        cvReleaseHist(&Histogram1);
        cvReleaseHist(&Histogram1L);
        tImg0Mat.release();

        //保存梯度图像bwImg

        //         CvScalar ava0Value4 = cvAvg(destbwImg);
//             //qDebug() << num <<"-ThresHc = "<< ThresHc;
//         cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/fbwImg_"+QString::number(num,10).toLatin1()+".bmp",destbwImg);

        //----------------求二值化连通域，并初定位Spot位置-------------------------

autoThreshold:
        if(autoThresholdMode)
        {
            cvZero(destbwImg);
            cvThreshold(SpotEhanceSampleImg, destbwImg, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
            //cvSaveImage("/media/photo/autoBwImgR_"+QString::number(num,10).toLatin1()+".bmp",destbwImg);
            //qDebug()<<"active autoThresholdMode********************";
        }

        //
#ifdef TEST_MODE
        {
            bool is_show_img = false;
            if (is_show_img) {
                cvShowImage("RunTask::pyrDetect0():destbwImg", destbwImg);
            }
        }
#endif

        // 查找轮廓，对应连通域
        CvSeq *contours = NULL;
        CvMemStorage *mem_storage = cvCreateMemStorage(0);

        //找轮廓
        int contours_num = cvFindContours(destbwImg, mem_storage, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
        //qDebug()<<num<<"gradient the num of contours is: "<< contours_num;

#ifdef TEST_MODE
        {
            bool is_show_img = false;
            if (is_show_img) {
                IplImage *img_contours = cvCreateImage(cvSize(destbwImg->width, destbwImg->height), IPL_DEPTH_8U, 1);
                cvZero(img_contours);
                cvDrawContours(img_contours, contours, CV_RGB(255, 255, 255), CV_RGB(127, 127, 127), 2);
                cvShowImage("RunTask::pyrDetect0_img_contours0", img_contours);
                cvReleaseImage(&img_contours);
            }
        }
#endif

        //
        if(contours_num == 0)
        {
#ifdef TEST_MODE
            {
                cvShowImage("RunTask::pyrDetect0():destbwImg-fail", destbwImg);
            }
#endif
            //qDebug()<<"contours false!! 936";
            cvReleaseImage(&bwImg);
            cvReleaseImage(&destbwImg);
            cvClearMemStorage(mem_storage);
            cvReleaseMemStorage(&mem_storage);
            cvReleaseImage(&SampleImg);
            cvReleaseImage(&SpotEhanceSampleImg);
            cvReleaseImage(&Img);

            qDebug() << "RunTask::processPic() >>> contours_num == 0";
            return 0;
        }

        //    int minAreaTH = 4;
        //    int maxAreaTH = 200;
        int minDTH = 4;
        int maxDTH = 20;
        if(WinMeasure::getCurrentAgeRange() == 0)
        {
            minDTH = 3;
            maxDTH = 18;
        }

        minDTH      *= PIX_COEF;
        maxDTH      *= PIX_COEF;

//        vector<uchar> vec_ValidPixel;
        vector<CvRect> vec_Rect;
        QMap<int, pyrRectDist> map_RectDistdiffer, map_WeightRect;
        map_RectDistdiffer.clear();
        map_WeightRect.clear();

#ifdef TEST_MODE
        bool is_show_contours_resized = false;
        IplImage *img_contours_resized = Q_NULLPTR;
        CvFont font;
        const int len_str_contor_resized = 12;
        char str_contor_resized[len_str_contor_resized] = {'\0'};
        if (is_show_contours_resized) {
            img_contours_resized = cvCreateImage(cvSize(destbwImg->width, destbwImg->height), IPL_DEPTH_8U, 3);
            cvZero(img_contours_resized);
            cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 0.5, 0.5);
        }
#endif

        qDebug() << "RunTask::pyrDetect0(): ResizedImgPupilFilter: img_num = " << num;

        //筛选合适尺寸的轮廓
        const double MIN_W_H_RATIO = 0.65;
        for (; contours; contours = contours->h_next)
        {
            CvRect rect1 = cvBoundingRect(contours, 0);
            //        int Tmp_D = max(rect1.width, rect1.height);
            //        if (Tmp_D >= minDTH && Tmp_D < maxDTH )
            int Tmp_H = rect1.height;
            int Tmp_W = rect1.width;
            double w_h_ratio = (Tmp_H < Tmp_W ? (double)Tmp_H / Tmp_W : (double)Tmp_W / Tmp_H);
            bool is_ok = false;
            //qDebug()<< "runtask contours max high:" << Tmp_H;
            if (Tmp_H >= minDTH && Tmp_W >= minDTH && Tmp_H <= maxDTH && Tmp_W <= maxDTH && w_h_ratio > MIN_W_H_RATIO /*&& area1 > minAreaTH && area1 < maxAreaTH*/)
            {
                vec_Rect.push_back(rect1);
                is_ok = true;
            }

            //
#ifdef TEST_MODE
            qDebug() << QString::asprintf("RunTask::pyrDetect0(): ResizedImgPupilFilter: is_passed = %s, leftTop = (%d, %d), size_len = %d, area = %d",
                                          Util::bool2str(is_ok), rect1.x, rect1.y, Tmp_W, Tmp_H).toLocal8Bit().data();

            if (is_show_contours_resized) {
                cvDrawContours(img_contours_resized, contours, (is_ok ? CV_RGB(0, 0, 255) : CV_RGB(255, 0, 0)), (is_ok ? CV_RGB(0, 0, 255) : CV_RGB(255, 0, 0)), 0);
                if (is_ok)
                {
                    std::snprintf(str_contor_resized, len_str_contor_resized, "%d,%d", rect1.x, rect1.y);
                    cvPutText(img_contours_resized, str_contor_resized, cvPoint(rect1.x - 12, rect1.y - 12), &font, CV_RGB(255, 255, 255));
                }
            }
#endif
        }

#ifdef TEST_MODE
        if (is_show_contours_resized) {
            cvShowImage("img_contours_resized", img_contours_resized);
            cvReleaseImage(&img_contours_resized);
        }
#endif

        //
        CvRect Rect1, Rect2;

        qDebug() << "--vec_Rect.size()=" << vec_Rect.size();
        //和饱和度的瞳孔距离对比，筛选最合适的瞳孔坐标
        if (vec_Rect.size() >= 2)
        {
            for(int i = 0; i < vec_Rect.size() - 1; i++) {
                for(int k = 1; k < vec_Rect.size() - i; k++)
                {
                    pyrRectDist pyrRectsTemp;
                    pyrRectsTemp.rect1 = vec_Rect.at(i);
                    pyrRectsTemp.rect2 = vec_Rect.at(i + k);

                    Rect1 = pyrRectsTemp.rect1;
                    bcenter1.x = Rect1.x + Rect1.width / 2;
                    bcenter1.y = Rect1.y + Rect1.height / 2;

                    Rect2 = pyrRectsTemp.rect2;
                    bcenter2.x = Rect2.x + Rect2.width / 2;
                    bcenter2.y = Rect2.y + Rect2.height / 2;

                    int dist = sqrt(pow((bcenter2.x - bcenter1.x), 2) + pow((bcenter2.y - bcenter1.y), 2));
                    int tempDiff = abs(dist - g_saturationPd);
                    if(tempDiff < g_saturationPd * 0.1 && abs(bcenter2.y - bcenter1.y) < resized_h / 4)
                    {
                        //qDebug()<<num<<"find tempDiff="<<tempDiff;
//                                                vec_pyrRects1.push_back(pyrRectsTemp);

                        uchar tmpPixel1 = 0;
                        for (int y = Rect1.y; y < Rect1.y + Rect1.height; y++)
                        {
                            for (int x = Rect1.x; x < Rect1.x + Rect1.width; x++)
                            {
                                uchar pixel1 = *(SpotEhanceSampleImg->imageData + SpotEhanceSampleImg->widthStep * y + x);
                                if (tmpPixel1 < pixel1)
                                {
                                    tmpPixel1 = pixel1;
                                }
                            }
                        }
                        pyrRectsTemp.pixel1 = tmpPixel1;

                        uchar tmpPixel2 = 0;
                        for (int y = Rect2.y; y < Rect2.y + Rect2.height; y++)
                        {
                            for (int x = Rect2.x; x < Rect2.x + Rect2.width; x++)
                            {
                                uchar pixel2 = *(SpotEhanceSampleImg->imageData + SpotEhanceSampleImg->widthStep * y + x);
                                if (tmpPixel2 < pixel2)
                                {
                                    tmpPixel2 = pixel2;
                                }
                            }
                        }

                        pyrRectsTemp.pixel2 = tmpPixel2;

                        map_RectDistdiffer.insert(tempDiff, pyrRectsTemp);
                        //qDebug() <<num<< "temp Rects:<"<<Rect1.x<<","<<Rect1.y<<">,<"<<Rect2.x<<","<<Rect2.y<<">";
                        //qDebug()<<num<<"tmpPixel1="<<tmpPixel1<<","<<tmpPixel2;

                    }
                }
            }
            //qDebug()<<num<<"map_RectDistdiffer.size="<<map_RectDistdiffer.size();

//                qDebug()<<"vec_pyrRects1 vecRow ="<<vecRow1;
//                qDebug()<<"vec_pyrRects1.size()="<<vec_pyrRects1.size();

            if(map_RectDistdiffer.size() > 0)       // TODO: 这方法过滤不掉瞳距相等的轮廓
            {
                QMap<int, pyrRectDist>::iterator it;
                for(it = map_RectDistdiffer.begin(); it != map_RectDistdiffer.end(); it++)
                {
                    //qDebug()<<"map_RectDistdiffer diff:"<<it.key()<<","<<it.value().rect1.x;
                    int avaPix = (it.value().pixel1 + it.value().pixel2) / 2;
                    int weightRect = avaPix * ((2 + it.key()) / (1 + it.key()));
                    //qDebug()<<num<<"weightRect:"<<weightRect;
                    map_WeightRect.insert(weightRect, it.value());
                }

                //qDebug()<<num<<"map_WeightRect.size="<<map_WeightRect.size();

                if(map_WeightRect.size() == 0)
                {
                    qDebug() << num << "map_WeightRect.size()==0";
                    vec_Rect.clear();
                    vector<CvRect> (vec_Rect).swap(vec_Rect);
                    cvReleaseImage(&bwImg);
                    cvReleaseImage(&destbwImg);
                    cvClearMemStorage(mem_storage);
                    cvReleaseMemStorage(&mem_storage);
                    cvReleaseImage(&SampleImg);
                    cvReleaseImage(&SpotEhanceSampleImg);
                    cvReleaseImage(&Img);

                    qDebug() << num << "+++++++++++release resource+++++++++++complete,return";
                    return 0;
                }

                //
                Rect1 = map_WeightRect.last().rect1;
                Rect2 = map_WeightRect.last().rect2;

                bcenter1.x = Rect1.x + Rect1.width / 2;
                bcenter1.y = Rect1.y + Rect1.height / 2;
                bcenter2.x = Rect2.x + Rect2.width / 2;
                bcenter2.y = Rect2.y + Rect2.height / 2;

                qDebug() << num << "finalPyrRects:<" << Rect1.x << "," << Rect1.y << ">,<" << Rect2.x << "," << Rect2.y << ">";

                //add end****************************************************************************************

                if (bcenter1.x > edgeTH && bcenter1.x < (resized_w - edgeTH) && bcenter2.x > edgeTH && bcenter2.x < (resized_w - edgeTH)
                        && bcenter1.y > edgeTH && bcenter1.y < (resized_h - edgeTH) && bcenter2.y > edgeTH && bcenter2.y < (resized_h - edgeTH))
                {
                    if (abs(bcenter2.x - bcenter1.x) > resized_w / 5 && abs(bcenter2.y - bcenter1.y) < resized_h / 3)
                    {
                        //                        int dist = sqrt(pow((bcenter2.x - bcenter1.x),2)+pow((bcenter2.y - bcenter1.y),2));
                        //                        if(abs(dist - pyrDist) <= 5){
                        bSpotCoarseLocation = true;
                        //                            qDebug()<<"dist="<<dist;
                        //                        }
                    }
                    else
                    {
                        bSpotCoarseLocation = false;
                        //qDebug()<<num<<"-runtask,bSpotCoarseLocation == false1";
                    }
                }
                else
                {
                    bSpotCoarseLocation = false;
                    //qDebug()<<num<<"-runtask,bSpotCoarseLocation == false2";
                }
            }
            else
            {
                bSpotCoarseLocation = false;
//                qDebug()<<num<<"-runtask,bSpotCoarseLocation == false3";

            }

        }
        else
        {
            bSpotCoarseLocation = false;
//                qDebug()<<num<<"-runtask,bSpotCoarseLocation == false4";
        }
//        qDebug()<<"bcenter1.x = "<<bcenter1.x<<"; bcenter1.y = "<<bcenter1.y<<"; Rect1.x = "<<Rect1.x<<"; Rect1.y = "<<Rect1.y<<"; Rect1.width = "<<Rect1.width;
//        qDebug()<<"bcenter2.x = "<<bcenter2.x<<"; bcenter2.y = "<<bcenter2.y<<"; Rect1.x = "<<Rect2.x<<"; Rect2.y = "<<Rect2.y<<"; Rect1.width = "<<Rect2.width;

        //qDebug()<<"after vec_ValidPixel ------1040-----";
//            vec_ValidPixel.clear();
        vec_Rect.clear();
//            vector<uchar> (vec_ValidPixel).swap(vec_ValidPixel);
        vector<CvRect> (vec_Rect).swap(vec_Rect);

        //
        if(bSpotCoarseLocation == false && !autoThresholdMode)
        {
            autoThresholdMode = true;
            cvClearMemStorage(mem_storage);
            cvReleaseMemStorage(&mem_storage);
            //qDebug()<<"bSpotCoarseLocation == false,enter auto threshold mode "<< num;
            goto autoThreshold;     // 用算得的阈值二值化识别失败后，跳回二值化的位置，改用大津算法自动确定二值化阈值再次识别一遍
        }

        //
        cvReleaseImage(&bwImg);
        cvReleaseImage(&destbwImg);
        cvClearMemStorage(mem_storage);
        cvReleaseMemStorage(&mem_storage);
        qDebug() << num << "release tImg0+++++++++++++++++++++";
    }

    cvReleaseImage(&SpotEhanceSampleImg);

    if (bSpotCoarseLocation == false)
    {

        qDebug() << "detect  failed, bSpotCoarseLocation == false " << num;
        cvReleaseImage(&Img);
        cvReleaseImage(&SampleImg);
        return 0;
    }
//    else{
//        qDebug()<<"detect sucess, bSpotCoarseLocation == true "<<num;

//    }

//    QTime endprocessPic = QTime::currentTime();
//    int endprocessPictime = detectbegin.msecsTo(endprocessPic);

    ////////////////////////////求平均灰度值////////////////////////////////
    CvScalar SampleImg_avaVal = cvAvg(SampleImg);
    double fix_ava = SampleImg_avaVal.val[0] * 2;
    //qDebug()<<num<<"---------SampleImg_avaVal:"<<SampleImg_avaVal.val[0]<<"fix_ava:"<<fix_ava<<"-------------";

#ifdef TEST_MODE
        {
            bool is_show_img = false;
            if (is_show_img) {
                cvCircle(SampleImg, bcenter1, 10 * PIX_COEF, CV_RGB(255, 255, 255));
                cvCircle(SampleImg, bcenter2, 10 * PIX_COEF, CV_RGB(255, 255, 255));
                cvShowImage("SampleImg", SampleImg);
            }
        }
#endif

    cvReleaseImage(&SampleImg);

    qDebug() << num << "---finish finish cu dingwei";


    /////////////////////////////////精确定位/////////////////////////////////

    CvPoint realTemp;
    if (bcenter1.x < bcenter2.x)
    {
        realTemp = bcenter1;
        bcenter1 = bcenter2;
        bcenter2 = realTemp;

        //qDebug()<<"exchange center:"<< num;
    }

    // ------------------从原始图中取出感兴趣区域---------------------------

    //------------left eye---------------------

    CvRect RectSub1;
    RectSub1.x = bcenter1.x * ratio - ROIW_HALF;
    RectSub1.y = bcenter1.y * ratio - ROIW_HALF;
    RectSub1.width = 2 * ROIW_HALF;
    RectSub1.height = 2 * ROIW_HALF;

    if (RectSub1.x < 0 || (RectSub1.x + RectSub1.width) > Img->width
            || RectSub1.y < 0 || (RectSub1.y + RectSub1.height) > Img->height)
    {
        //qDebug()<<"RectSub1 failed,return++++"<<num;
        cvReleaseImage(&Img);

        qDebug() << "RunTask::pyrDetect0() >>> err 1";
        return 0;
    }
    cvSetImageROI(Img, RectSub1);
    IplImage *subImg1 = cvCreateImage(cvSize(RectSub1.width, RectSub1.height), Img->depth, Img->nChannels);
    cvCopy(Img, subImg1, NULL);
    cvResetImageROI(Img);

    //
    double PreciseMinValue1 = 0.0;
    double PreciseMaxValue1 = 0.0;
    CvPoint PreciseMinLocation1;
    CvPoint PreciseMaxLocation1;
    // CvPoint center1;
    cvMinMaxLoc(subImg1, &PreciseMinValue1, &PreciseMaxValue1, &PreciseMinLocation1, &PreciseMaxLocation1);

    if(PreciseMaxValue1 < fix_ava )
    {
        //qDebug()<<num<<"PreciseMaxValue1 < fix_ava ----"<<PreciseMaxValue1;
        cvReleaseImage(&subImg1);
        cvReleaseImage(&Img);

        qDebug() << "RunTask::pyrDetect0() >>> err 2";
        return 0;
    }
    ////qDebug()<<"========================2052=========================";

    //
    IplImage *thrSubImg1 = cvCreateImage(cvSize(ROIW, ROIH), IPL_DEPTH_8U, 1);
    cvSetZero(thrSubImg1);

//    cv::Mat tImg1Mat;
//    cv::medianBlur(cv::Mat(subImg1),tImg1Mat,5);
//    IplImage tImg1  = IplImage(tImg1Mat);//
    IplImage *destThrSubImg1 = cvCreateImage(cvSize(ROIW, ROIH), IPL_DEPTH_8U, 1);
    cvSetZero(destThrSubImg1);

    //
    CvScalar tempScalar;
    double mind[4];
    double mindv, maxdv;

    for(int j = 2; j < ROIH - 2; j++)
    {
        for(int i = 2; i < ROIW - 2; i++)
        {

            mind[0] = cvGet2D(subImg1, j, i - 1).val[0];
            mind[1] = cvGet2D(subImg1, j - 1, i).val[0];
            mind[2] = cvGet2D(subImg1, j + 1, i).val[0];
            mind[3] = cvGet2D(subImg1, j, i + 1).val[0];

            //qDebug()<<"**************cvGEt2DTime:"<< cvGet2DTimeBegin.msecsTo(cvGet2DFinishTime);
            mindv = mind[0];
            maxdv = mind[0];
            for(int k = 1; k < 4; k++)
            {
                if(mindv > mind[k])
                {
                    mindv = mind[k];
                }
                if(maxdv < mind[k])
                {
                    maxdv = mind[k];
                }
            }
            //if(mindv==0)
            //    mindv = 1;
            if(mindv <= 3 || maxdv == 255)
            {
                tempScalar.val[0] = 0;
                cvSet2D(thrSubImg1, j, i, tempScalar);
            }
            else
            {
                tempScalar.val[0] = cvGet2D(subImg1, j, i).val[0] / mindv * maxdv;
                cvSet2D(thrSubImg1, j, i, tempScalar);
            }

        }
    }

    //qDebug() <<num<< "jing que ding wei jiaohuan qian ThresHcL1:--"<<ThresHcL;
    CvScalar avaValue = cvAvg(thrSubImg1);

    avaValue.val[0] = avaValue.val[0] * 1.1;
    if(ThresHcL > avaValue.val[0])
        ThresHcL = avaValue.val[0];
    //qDebug() <<num<< "jing que ding wei hou ThresHcL1:--"<<ThresHcL<<"avaValue.val[0]"<<avaValue.val[0];

    CvMemStorage *mem_storage1 = cvCreateMemStorage(0);
    CvSeq *contours1;
    CvSeq *maxContour1;
    double differValue = 10000;

AutoadjustthreshL:

    if (varThresholdStateL == 1)
        ThresHcL = ThresHcL * 1.1;
    if (varThresholdStateL == 2)
        ThresHcL = ThresHcL * 0.8;

    cvThreshold(thrSubImg1, destThrSubImg1, ThresHcL, 255, CV_THRESH_BINARY );

    ////保存梯度图像thrSubImg1
    //cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/L_destThrSubImg1_"+ QString::number(ThresHcL,'f',1).toLatin1() +QString::number(num,10).toLatin1()+".bmp",destThrSubImg1);
    //cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/L_destThrSubImg1_" +QString::number(num,10).toLatin1()+".bmp",destThrSubImg1);

#ifdef TEST_MODE
        {
            bool is_show_img = false;
            if (is_show_img) {
                cvShowImage("RunTask::pyrDetect0():thrSubImg1", thrSubImg1);
                cvShowImage("RunTask::pyrDetect0():destThrSubImg1", destThrSubImg1);
            }
        }
#endif

    // 查找轮廓
    cvClearMemStorage(mem_storage1);
    contours1 = NULL;
    //int contours_num1 = cvFindContours(thrSubImg1, mem_storage1, &contours1, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
    int contours_num1 = cvFindContours(destThrSubImg1, mem_storage1, &contours1, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
    //qDebug() << "after cvFindContours thrSubImg111111111111111111111111--"<<num;

#ifdef TEST_MODE
    {
        bool is_show_img = false;
        if (is_show_img) {
            IplImage *img_contours = cvCreateImage(cvSize(destThrSubImg1->width, destThrSubImg1->height), IPL_DEPTH_8U, 1);
            cvZero(img_contours);
            cvDrawContours(img_contours, contours1, CV_RGB(255, 255, 255), CV_RGB(127, 127, 127), 2);
            cvShowImage("RunTask::pyrDetect0_img_contours1", img_contours);
            cvReleaseImage(&img_contours);
        }
    }
#endif

    if (contours_num1 < 1)
    {
//        printf("检查失败，无法找到瞳孔位置！");
        qDebug() << "jian ce shi bai wu fa zhao dao tong kong wei zhi";
        cvClearMemStorage(mem_storage1);
        cvReleaseMemStorage(&mem_storage1);
        cvReleaseImage(&destThrSubImg1);
        cvReleaseImage(&thrSubImg1);
        cvReleaseImage(&subImg1);
        cvReleaseImage(&Img);

        qDebug() << "RunTask::pyrDetect0() >>> contours_num1 < 1";
        return 0;
    }

    // ***************************find the best Contours**************************
    maxContour1 = contours1;
    CvRect maxRect1;
    maxRect1.height = 0;
    maxRect1.width = 0;
    differValue = 10000;

    float radius1;
    CvPoint2D32f bbcenter1;
    bbcenter1.x = 0;
    bbcenter1.y = 0;
    CvPoint center1;

    // ***************************find the best Contours**************************
    for (; contours1 != 0; contours1 = contours1->h_next)
    {
        CvRect maxRect = cvBoundingRect(contours1, 0);
//-        qDebug()<<num<<"contour1:"<<maxRect.width<<","<<maxRect.height;
        double tempEyeArea = fabs(cvContourArea(contours1));
        double TempDifferValue = abs(tempEyeArea - g_pyrArea[0]);

        double Width = maxRect.width;
        double Height = maxRect.height;
        if((Width / Height) >= 2 || tempEyeArea < 100)
            continue;

//        qDebug()<<"contours1 maxRect.width * maxRect.height   = "<<maxRect.width<<"and"<<maxRect.height<<num;
//      if(abs(tempEyeArea - g_pyrArea[0]) < DeviationP && abs(maxLengthL - (saturationCenterL.z+1.5)*2) < 10)

        if(TempDifferValue < differValue)
        {
            differValue = TempDifferValue;
            maxContour1 = contours1;
            maxRect1 = maxRect;
            //  qDebug()<<num<<"TempDifferValue < differValue-----maxContour1:"<<maxRect.width<<","<<maxRect.height;
        }
        else
        {
            //  qDebug()<<num<<"else TempDifferValue >= differValue-----maxContour1:"<<maxRect.width<<","<<maxRect.height;

        }
    }

    qDebug() << num << "find left maxContour1 success---------------------";

//    double LeftEyeArea = fabs(cvContourArea(maxContour1));
//    qDebug()<<num<<"-----LeftEyeArea="<<LeftEyeArea;

    //add by sun 20181018********************left****************************

    if(g_isHmMode)
    {
//        qDebug()<<"enter hmMode----LeftEyeArea:"<<LeftEyeArea<<",g_pyrArea[0]:"<<g_pyrArea[0];
        /*
                if((LeftEyeArea - g_pyrArea[0]) > g_pyrArea[0]/10 || maxRect1.width-pyrRectWidth[0] > 10){
        //                qDebug()<<"LeftEyeArea:"<<LeftEyeArea<<",g_pyrArea[0]:"<<g_pyrArea[0];
        //                qDebug()<<num<<"LeftEyeArea - g_pyrArea[0] > DeviationP *******************"<<(LeftEyeArea - g_pyrArea[0]);
                        varThresholdStateL = 1;

        //                qDebug()<<num<<"reTestNumLeft ="<<reTestNumLeft;
                        if(reTestNumLeft<=3){
                            reTestNumLeft++;
                            goto AutoadjustthreshL;
                        }
                        else{
        //                    qDebug()<<"reTestNumLeft > 5 ";
                            double reTestEyeAreaL = fabs(cvContourArea(maxContour1));
                            if(reTestEyeAreaL > g_pyrArea[0]*1.4 || reTestEyeAreaL < g_pyrArea[0]*0.5){
        //                        qDebug()<<"retest failed!!";

                                bbcenter1.x=PreciseMaxLocation1.x;
                                bbcenter1.y=PreciseMaxLocation1.y;
        //                        return 0;
                            }
                        }

                }
                else if((LeftEyeArea - g_pyrArea[0]) < - g_pyrArea[0]/10 || maxRect1.width-pyrRectWidth[0] < -10){
        //               qDebug()<<"LeftEyeArea:"<<LeftEyeArea<<",g_pyrArea[0]:"<<g_pyrArea[0];
        //                qDebug()<<num<<"LeftEyeArea - g_pyrArea[0] < -DeviationP *******************"<<(LeftEyeArea - g_pyrArea[0]);
                         varThresholdStateL = 2;

        //                 qDebug()<<num<<"reTestNumLeft ="<<reTestNumLeft;
                         if(reTestNumLeft<=3){
                             reTestNumLeft++;
                             goto AutoadjustthreshL;
                         }
                         else{
        //                     qDebug()<<"reTestNumLeft > 5 ";
                             double reTestEyeAreaL = fabs(cvContourArea(maxContour1));
                             if(reTestEyeAreaL > g_pyrArea[0]*1.4 || reTestEyeAreaL < g_pyrArea[0]*0.5){
        //                         qDebug()<<"retest failed!!";
                                 bbcenter1.x=PreciseMaxLocation1.x;
                                 bbcenter1.y=PreciseMaxLocation1.y;
        //                         return 0;
                             }
                         }

                }
        //        qDebug()<<"after hmMode----LeftEyeArea:"<<LeftEyeArea<<",g_pyrArea[0]:"<<g_pyrArea[0];
        */
        bbcenter1.x = PreciseMaxLocation1.x;
        bbcenter1.y = PreciseMaxLocation1.y;
    }

    //end*******************************************************************

//    qDebug()<<"maxContour1 cvMinEnclosingCircle1 before  = ";
    int conNum1 = cvMinEnclosingCircle(maxContour1, &bbcenter1, &radius1); //对轮廓进行多变形逼近

    ////qDebug()<<"release before runTask ThreadID = "<<QThread::currentThreadId();
    cvClearMemStorage(mem_storage1);
    cvReleaseMemStorage(&mem_storage1);
    cvReleaseImage(&thrSubImg1);
    cvReleaseImage(&destThrSubImg1);
    cvReleaseImage(&subImg1);

    //qDebug()<<"after cvMinEnclosingCircle(maxContour1";
    if (conNum1 == 0)
    {
        cvReleaseImage(&Img);
//        qDebug()<<"conNum1 == 0,return";
        return 0;
    }

    double L_AspectRatio = (double)((double)(maxRect1.height) / (double)(maxRect1.width));;
    if(L_AspectRatio < 0.5)
    {
        qDebug() << num << "false:L_AspectRatio < 0.5 =" << L_AspectRatio;
        return 0;
    }
    if(num > 0 && num < 7)
    {
        g_pyrAR.L_AspectRatio[num - 1] = L_AspectRatio;
        qDebug() << num << "L_AspectRatio=" << L_AspectRatio;
    }

    //*************************minEnclosingCircle*************************20180929
//   IplImage *minEnclosingCircle = cvCreateImage(cvSize(ROIW,ROIH), IPL_DEPTH_8U, 1);
//   cvCopy(subImg1,minEnclosingCircle);
//   cvCircle(minEnclosingCircle, cvPoint(cvRound(bbcenter1.x),cvRound(bbcenter1.y)), cvRound(radius1), CV_RGB(255,255,255), 1, 8, 0 );
//   //cvSaveImage("/media/subImg/minEnclosingCircle_"+QString::number(num,10).toLatin1()+".bmp",minEnclosingCircle);//add by sun 20180928
    //******************************end************************************

    center1.x = bbcenter1.x;
    center1.y = bbcenter1.y;
    //cvCircle(SpotEnhanceSub1, center1, radius1, CV_RGB(255, 255, 255), 3);
    //----------------------------------------------------------------------------


    //-------------------------right eye-----------------------

    CvRect RectSub2;
    RectSub2.x = bcenter2.x * ratio - ROIW_HALF;
    RectSub2.y = bcenter2.y * ratio - ROIW_HALF;
    RectSub2.width = 2 * ROIW_HALF;
    RectSub2.height = 2 * ROIW_HALF;

    if (RectSub2.x < 0 || (RectSub2.x + RectSub2.width) > Img->width
            || RectSub2.y < 0 || (RectSub2.y + RectSub2.height) > Img->height)
    {
//        qDebug()<<"RectSub2 failed,return++++"<<num;
        cvReleaseImage(&Img);
        return 0;
    }
    cvSetImageROI(Img, RectSub2);
//    qDebug()<<"create subImg2 before  = "<<"runTask ThreadID = "<<QThread::currentThreadId();
    IplImage *subImg2 = cvCreateImage(cvSize(RectSub2.width, RectSub2.height), Img->depth, Img->nChannels);
    cvCopy(Img, subImg2, NULL);
    cvResetImageROI(Img);

    //
    double PreciseMinValue2 = 0.0;
    double PreciseMaxValue2 = 0.0;
    CvPoint PreciseMinLocation2;
    CvPoint PreciseMaxLocation2;
//    CvPoint center2;
    cvMinMaxLoc(subImg2, &PreciseMinValue2, &PreciseMaxValue2, &PreciseMinLocation2, &PreciseMaxLocation2);
    if(PreciseMaxValue2 < fix_ava )
    {
        qDebug() << num << "PreciseMaxValue2 < fix_ava ----" << PreciseMaxValue2;
        cvReleaseImage(&subImg2);
        cvReleaseImage(&Img);
        return 0;
    }
    //add by douzi 20180827
    //qDebug()<<"========================tImg2========================="<<num;

    //
    IplImage *thrSubImg2 = cvCreateImage(cvSize(ROIW, ROIH), IPL_DEPTH_8U, 1);
    cvSetZero(thrSubImg2);
//    cv::Mat tImg2Mat;
//    cv::medianBlur(cv::Mat(subImg2),tImg2Mat,5);
//    IplImage tImg2 = IplImage(tImg2Mat);//
    IplImage *destThrSubImg2 = cvCreateImage(cvSize(ROIW, ROIH), IPL_DEPTH_8U, 1);
    cvSetZero(destThrSubImg2);

    //
    CvScalar tempScalar2;
    double min2d[4];
    double min2dv, max2dv;
    //CvScalar avaValue2 = cvAvg(subImg2);

    for(int j = 2; j < ROIH - 2; j++)
    {
        for(int i = 2; i < ROIW - 2; i++)
        {
            min2d[0] = cvGet2D(subImg2, j, i - 1).val[0];
            min2d[1] = cvGet2D(subImg2, j - 1, i).val[0];
            min2d[2] = cvGet2D(subImg2, j + 1, i).val[0];
            min2d[3] = cvGet2D(subImg2, j, i + 1).val[0];
            min2dv = min2d[0];
            max2dv = min2d[0];
            for(int k = 1; k < 4; k++)
            {
                if(min2dv > min2d[k])
                {
                    min2dv = min2d[k];
                }
                if(max2dv < min2d[k])
                {
                    max2dv = min2d[k];
                }
            }
            //if(min2dv==0)
            //    min2dv = 1;
            if(min2dv <= 3 || max2dv == 255)
            {
                tempScalar2.val[0] = 0;
                cvSet2D(thrSubImg2, j, i, tempScalar2);
            }
            else
            {
                tempScalar2.val[0] = cvGet2D(subImg2, j, i).val[0] / min2dv * max2dv;
                cvSet2D(thrSubImg2, j, i, tempScalar2);
            }
        }
    }

    CvScalar avaValue2 = cvAvg(thrSubImg2);

    avaValue2.val[0] = avaValue2.val[0] * 1.1;
    if(ThresHcR > avaValue2.val[0])
        ThresHcR = avaValue2.val[0];

    CvMemStorage *mem_storage2 = cvCreateMemStorage(0);
    CvSeq *contours2;
    CvSeq *maxContour2;
    double differValue2 = 10000;
    float radius2;
    CvPoint2D32f bbcenter2;
    bbcenter2.x = 0;
    bbcenter2.y = 0;
    CvPoint center2;

AutoadjustthreshR:

    if (varThresholdStateR == 1)
        ThresHcR = ThresHcR * 1.1;

    if(varThresholdStateR == 2)
        ThresHcR = ThresHcR * 0.8;

    cvThreshold(thrSubImg2, destThrSubImg2, ThresHcR, 255, CV_THRESH_BINARY);

    //cvSaveImage("/media/photo/" + g_currentSubjectNum.toLatin1() +"/R_destThrSubImg2_"+ QString::number(ThresHcR,'f',1).toLatin1() +QString::number(num,10).toLatin1()+".bmp",destThrSubImg2);
    //cvSaveImage("/media/photo/" + g_currentSubjectNum.toLatin1() +"/R_destThrSubImg2_"+QString::number(num,10).toLatin1()+".bmp",destThrSubImg2);

#ifdef TEST_MODE
        {
            bool is_show_img = false;
            if (is_show_img) {
                cvShowImage("RunTask::pyrDetect0():thrSubImg2", thrSubImg2);
                cvShowImage("RunTask::pyrDetect0():destThrSubImg2", destThrSubImg2);
            }
        }
#endif

    // 查找轮廓
    cvClearMemStorage(mem_storage2);
    contours2 = NULL;
    int contours_num2 = cvFindContours(destThrSubImg2, mem_storage2, &contours2, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
    qDebug() << num << "after cvFindContours(destThrSubImg2";

#ifdef TEST_MODE
        {
            bool is_show_img = false;
            if (is_show_img) {
                IplImage *img_contours = cvCreateImage(cvSize(destThrSubImg2->width, destThrSubImg2->height), IPL_DEPTH_8U, 1);
                cvZero(img_contours);
                cvDrawContours(img_contours, contours2, CV_RGB(255, 255, 255), CV_RGB(127, 127, 127), 2);
                cvShowImage("RunTask::pyrDetect0_img_contours2", img_contours);
                cvReleaseImage(&img_contours);
            }
        }
#endif

    if (contours_num2 < 1)
    {
        //printf("检查失败，无法找到瞳孔位置！");
        //qDebug()<<"contours_num2,jian ce shi bai, wu fa zhao dao tong kong wei zhi";
        //cvReleaseImage(&tImg2);
        cvReleaseImage(&Img);
        cvReleaseImage(&subImg2);
        cvReleaseImage(&thrSubImg2);
        cvReleaseImage(&destThrSubImg2);
        cvClearMemStorage(mem_storage2);
        cvReleaseMemStorage(&mem_storage2);
        return 0;
    }

    maxContour2 = contours2;
    CvRect maxRect2;
    maxRect2.height = 0;
    maxRect2.width = 0;
    differValue2 = 10000;

    // ***************************find the best Contours**************************
    for (; contours2; contours2 = contours2->h_next)
    {
        double tempEyeAreaR = fabs(cvContourArea(contours2));
        double TempDifferValueR = abs(tempEyeAreaR - g_pyrArea[1]);
        CvRect maxRect = cvBoundingRect(contours2, 0);

        double Width = maxRect.width;
        double Height = maxRect.height;
        if((Width / Height) >= 2 || tempEyeAreaR < 100)
            continue;
        //qDebug()<<num<<"contour2:"<<maxRect.width<<","<<maxRect.height;
        //qDebug()<<"TempDifferValueR="<<TempDifferValueR;
        if(TempDifferValueR < differValue2)
        {
            differValue2 = TempDifferValueR;
            maxContour2 = contours2;
            maxRect2 = maxRect;
            //qDebug()<<num<<"TempDifferValueR < differValue2-----maxContour2:"<<maxRect.width<<","<<maxRect.height;
        }
        else
        {
            //qDebug()<<num<<"else TempDifferValueR >= differValue2-----maxContour2:"<<maxRect.width<<","<<maxRect.height;

        }
    }

    //double RightEyeArea = fabs(cvContourArea(maxContour2));

    qDebug() << num << "find right maxContour2 success---------------------";

    if(g_isHmMode)
    {
        //add by sun 20181018*******************right*****************************
        //int varThresholdStateR = false;
        //qDebug()<<"enter hmMode--RightEyeArea:"<<RightEyeArea<<",g_pyrArea[1]:"<<g_pyrArea[1];
        /*
        if((RightEyeArea - g_pyrArea[1]) > g_pyrArea[1]/10 || maxRect2.width-pyrRectWidth[1] > 10){
                qDebug()<<num<<"RightEyeArea - g_pyrArea[1] > DeviationP*********************";
                varThresholdStateR = 1;

        //                qDebug()<<num<<"reTestNumRight ="<<reTestNumRight;
                if(reTestNumRight<=3){
                    reTestNumRight++;
                    goto AutoadjustthreshR;
                }
                else{
        //                    qDebug()<<"reTestNumRight > 5 ";
                    double reTestEyeAreaR = fabs(cvContourArea(maxContour2));
                    if(reTestEyeAreaR > g_pyrArea[1]*1.4 || reTestEyeAreaR < g_pyrArea[1]*0.5){
                        qDebug()<<"retest failed!!";
                        bbcenter2.x=PreciseMaxLocation2.x;
                        bbcenter2.y=PreciseMaxLocation2.y;
        //                        return 0;
                    }
                }

            }
           else if((RightEyeArea - g_pyrArea[1]) < - g_pyrArea[1]/10 || maxRect2.width-pyrRectWidth[1] < -10){
                varThresholdStateR = 2;
        //                qDebug()<<num<<"RightEyeArea - g_pyrArea[1] < DeviationP*********************";
        //                qDebug()<<num<<"reTestNumRight ="<<reTestNumRight;
                if(reTestNumRight<=3){
                    reTestNumRight++;
                    goto AutoadjustthreshR;
                }
                else{
        //                    qDebug()<<"reTestNumRight > 5 ";
                    double reTestEyeAreaR = fabs(cvContourArea(maxContour2));
                    if(reTestEyeAreaR > g_pyrArea[1]*1.4 || reTestEyeAreaR < g_pyrArea[1]*0.5){
        //                        qDebug()<<"retest failed!!";
                        bbcenter2.x=PreciseMaxLocation2.x;
                        bbcenter2.y=PreciseMaxLocation2.y;
        //                        return 0;
                    }
                }

              }
        //        qDebug()<<"after hmMode--RightEyeArea:"<<RightEyeArea<<",g_pyrArea[1]:"<<g_pyrArea[1];
        */

        bbcenter2.x = PreciseMaxLocation2.x;
        bbcenter2.y = PreciseMaxLocation2.y;

        //end*******************************************************************
    }

    int conNum2 = cvMinEnclosingCircle(maxContour2, &bbcenter2, &radius2); //对轮廓进行多变形逼近
//    qDebug()<<num<< "after cvMinEnclosingCircle(maxContour2";
    cvClearMemStorage(mem_storage2);
    cvReleaseMemStorage(&mem_storage2);
    cvReleaseImage(&destThrSubImg2);
    cvReleaseImage(&thrSubImg2);
    cvReleaseImage(&subImg2);
//    qDebug()<<"after release at 1645";
    if (conNum2 == 0)
    {
        qDebug() << "conNum2 == 0,return";
        cvReleaseImage(&Img);
        return 0;
    }

    double R_AspectRatio = (double)((double)(maxRect2.height) / (double)(maxRect2.width));
    if(R_AspectRatio < 0.5)
    {
        qDebug() << num << "false:R_AspectRatio < 0.5 =" << R_AspectRatio;
        return 0;
    }
    if(num > 0 && num < 7)
    {
        g_pyrAR.R_AspectRatio[num - 1] = R_AspectRatio;
        qDebug() << num << "R_AspectRatio=" << R_AspectRatio;
    }
//    qDebug()<<"after maxRect2.width / maxRect2.height >= 2){ at 1645";

    center2.x = bbcenter2.x;
    center2.y = bbcenter2.y;

    cvReleaseImage(&Img);

    //*************************minEnclosingCircle2*************************20180929
//    IplImage *minEnclosingCircle2 = cvCreateImage(cvSize(ROIW,ROIH), IPL_DEPTH_8U, 1);
//    cvCopy(subImg2,minEnclosingCircle2);
//    cvCircle(minEnclosingCircle2, cvPoint(cvRound(bbcenter2.x),cvRound(bbcenter2.y)), cvRound(radius2), CV_RGB(255,255,255), 1, 8, 0 );
//    //cvSaveImage("/media/subImg/minEnclosingCircle2_"+QString::number(num,10).toLatin1()+".bmp",minEnclosingCircle2);//add by sun 20180928
    //******************************end************************************


    //qDebug() << "after cvMinEnclosingCircle2_center2.x"<<num<<center2.x<<","<<center2.y ;
//    center2.x = bbcenter2.x;
//    center2.y = bbcenter2.y;

    center1.x += RectSub1.x;
    center1.y += RectSub1.y;

    center2.x += RectSub2.x;
    center2.y += RectSub2.y;

    int x_distance = (center1.x - center2.x)
                     * (center1.x - center2.x);

    int y_distance = (center1.y - center2.y)
                     * (center1.y - center2.y);
    int pupil_distance = sqrt(x_distance + y_distance);


    //cout << "左眼圆心点为： （ " << center1.x << " ,  " << center1.y << " ), 半径为：" << radius1 << endl;
    //cout << "左眼圆心点为： （ " << center2.x << " ,  " << center2.y << " ), 半径为：" << radius2 << endl;
    //cout << "瞳距为： " <<pupil_distance<< endl;

    int center1LeftY = center1.y - ROIW_HALF;
    int center1LeftX = center1.x - ROIW_HALF;

    int center2LeftY = center2.y - ROIW_HALF;
    int center2LeftX = center2.x - ROIW_HALF;

    //added by ron
    if (center1LeftY < MINIRECT || center1LeftY > MAXRECT
            || center1LeftX < SLIDELEFT || center1LeftX > MAXRECTWIDTH
            || center2LeftY < MINIRECT || center2LeftY > MAXRECT
            || center2LeftX < SLIDELEFT || center2LeftX > MAXRECTWIDTH
            || pupil_distance < MINIPUPILDIS)
    {

        qDebug() << "chao yue bian jie";
        return 0;
    }

    avalPoint3D[0].x = center1.x;
    avalPoint3D[0].y = center1.y;
    if(g_isHmMode)
        avalPoint3D[0].z = g_SaturationCenterL.z;
    else
        avalPoint3D[0].z = radius1;

    avalPoint3D[1].x = center2.x;
    avalPoint3D[1].y = center2.y;
    if(g_isHmMode)
        avalPoint3D[1].z = g_SaturationCenterR.z;
    else
        avalPoint3D[1].z = radius2;

    *pupilDis = pupil_distance;
    qDebug() << num << "--pupil_distance = " << pupil_distance;
    qDebug() << "finish pyrdetect ---------------" << num;
    return 1;
}

int RunTask::pyrDetect(IplImage *pyrImg, CvPoint3D32f *avalPoint3D, int *pupilDis, int num)
{
    if (pyrImg == NULL)
    {
        qDebug() << num << "pyrImg == NULL";
        return 0;
    }

    int w = pyrImg->width;
    int h = pyrImg->height;

    //QTime detectbegin = QTime::currentTime();

    IplImage *Img = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);
    cvCopy(pyrImg, Img);

    bool pupil_detect_succ = false;

    //
    stPupilInfo pupil_info_r, pupil_info_l;
    //memset(&pupil_info_r, 0, sizeof(stPupilInfo));
    //memset(&pupil_info_l, 0, sizeof(stPupilInfo));

    int thresh_val = 0;

    try {
        pupil_detect_succ = AlgorithmThread::detectPupilRoughly(Img, pupil_info_r, pupil_info_l, thresh_val, num);
    }
    catch (exception &ex) {
        logCritical(QString::asprintf("RunTask::pyrDetect() executing AlgorithmThread::detectPupilRoughly() exception:\n %s", ex.what()), CGlobal::LOG_ALGO);
        output_trace<void>();
        throw "caught error in RunTask::pyrDetect() executing AlgorithmThread::detectPupilRoughly()";
        pupil_detect_succ = false;
    }

    if (pupil_detect_succ && CGlobal::isPupilAccutrately) {
        try {
            bool pupil_detect_succ_r = false, pupil_detect_succ_l = false;

                pupil_detect_succ_r = AlgorithmThread::detectPupilAccurately(Img, pupil_info_r, thresh_val, 1, num);
            if (pupil_detect_succ_r)
                pupil_detect_succ_l = AlgorithmThread::detectPupilAccurately(Img, pupil_info_l, thresh_val, 2, num);

            if (!pupil_detect_succ_r || !pupil_detect_succ_l) {
                logCritical("RunTask::pyrDetect() : detectPupilRoughly() succeeded but detectPupilAccurately() failed!");
                pupil_detect_succ = false;
            }
        } catch (exception &ex) {
            logCritical(QString::asprintf("RunTask::pyrDetect() executing AlgorithmThread::detectPupilAccurately() exception:\n %s", ex.what()), CGlobal::LOG_ALGO);
            output_trace<void>();
            throw "caught error in RunTask::pyrDetect() executing AlgorithmThread::detectPupilAccurately()";
            pupil_detect_succ = false;
        }
    }

    //
    cvReleaseImage(&Img);

    //
    if (!pupil_detect_succ)
        return 0;

    //if (10 == num)
    //    qDebug() << endl;

    double L_AspectRatio = (double)pupil_info_l.rect.height / pupil_info_l.rect.width;
    if(L_AspectRatio < 0.5)
    {
        qDebug() << num << "false:L_AspectRatio < 0.5 =" << L_AspectRatio;
        return 0;
    }
    if(num > 0 && num < 7)
    {
        g_pyrAR.L_AspectRatio[num - 1] = L_AspectRatio;
        qDebug() << num << "L_AspectRatio=" << L_AspectRatio;
    }

    double R_AspectRatio = (double)pupil_info_r.rect.height / pupil_info_r.rect.width;
    if(R_AspectRatio < 0.5)
    {
        qDebug() << num << "false:R_AspectRatio < 0.5 =" << R_AspectRatio;
        return 0;
    }
    if(num > 0 && num < 7)
    {
        g_pyrAR.R_AspectRatio[num - 1] = R_AspectRatio;
        qDebug() << num << "R_AspectRatio=" << R_AspectRatio;
    }

    CvPoint2D32f center1 = pupil_info_l.center;
    CvPoint2D32f center2 = pupil_info_r.center;

    int x_distance = (center1.x - center2.x)
                     * (center1.x - center2.x);

    int y_distance = (center1.y - center2.y)
                     * (center1.y - center2.y);
    int pupil_distance = sqrt(x_distance + y_distance);


    //cout << "左眼圆心点为： （ " << center1.x << " ,  " << center1.y << " ), 半径为：" << radius1 << endl;
    //cout << "左眼圆心点为： （ " << center2.x << " ,  " << center2.y << " ), 半径为：" << radius2 << endl;
    //cout << "瞳距为： " <<pupil_distance<< endl;

    int center1LeftY = center1.y - ROIW_HALF;
    int center1LeftX = center1.x - ROIW_HALF;

    int center2LeftY = center2.y - ROIW_HALF;
    int center2LeftX = center2.x - ROIW_HALF;

    //added by ron
    if (center1LeftY < MINIRECT || center1LeftY > MAXRECT
            || center1LeftX < SLIDELEFT || center1LeftX > MAXRECTWIDTH
            || center2LeftY < MINIRECT || center2LeftY > MAXRECT
            || center2LeftX < SLIDELEFT || center2LeftX > MAXRECTWIDTH
            || pupil_distance < MINIPUPILDIS)
    {

        qDebug() << "chao yue bian jie";
        return 0;
    }

    avalPoint3D[0].x = center1.x;
    avalPoint3D[0].y = center1.y;
    if(g_isHmMode)
        avalPoint3D[0].z = g_SaturationCenterL.z;
    else
        avalPoint3D[0].z = pupil_info_l.radius;

    avalPoint3D[1].x = center2.x;
    avalPoint3D[1].y = center2.y;
    if(g_isHmMode)
        avalPoint3D[1].z = g_SaturationCenterR.z;
    else
        avalPoint3D[1].z = pupil_info_r.radius;

    *pupilDis = pupil_distance;
    qDebug() << num << "--pupil_distance = " << pupil_distance;
    qDebug() << "finish pyrdetect ---------------" << num;
    return 1;
}

//单眼模式检测瞳孔
int RunTask::single_pyrDetect(IplImage *pyrImg, CvPoint3D32f *avalPoint3D, int *pupilDis, int num)
{
    qDebug() << "--single_pyrDetect--" << num;
    if (pyrImg == NULL)
    {
        qDebug() << num << "pyrImg == NULL";
        return 0;
    }
    bool autoThresholdMode = false;
    bool bSpotCoarseLocation = true;
    int varThresholdState = 0;
    int MaxLocationh;
    double ThresHc;
    int w = pyrImg->width;
    int h = pyrImg->height;

    IplImage *Img = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);
    cvCopy(pyrImg, Img);

    ////////////////////////////瞳孔亮斑初定位///////////////////////////////////////

    // ----------------------------图像降维-----------------------------------
    int ratio =  4;     // 调整为原来1/ratio
    int sw = w / ratio;
    int sh = h / ratio;
    IplImage *SampleImg = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);


    uchar *data = (uchar *)SampleImg->imageData;
    for (int i = 0; i < sh; i++)
    {
        for (int j = 0; j < sw; j++)
        {
            *(data + i * SampleImg->widthStep + j) = ((uchar *)(Img->imageData + i * ratio * Img->widthStep))[j * ratio];
        }
    }

//    cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/sin_Img_"+QString::number(num,10).toLatin1()+".bmp",SampleImg);


//    qDebug() <<num<< "--after jiangwei****************************************" ;
//    IplImage *SpotEhanceSampleImg = cvCloneImage(SampleImg);
//    SpotEnhance(SampleImg, SpotEhanceSampleImg);
//    qDebug()<<num<<"--after spotEnchance--";
    CvPoint bcenter1;
    bcenter1.x = 0;
    bcenter1.y = 0;


    int edgeTH = 2;
//    qDebug()<<num<<"********enter gradient detect mode*********";

    if(true)
    {

        IplImage *bwImg = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);
        cvSetZero(bwImg);

        cv::Mat tImg0Mat;
        cv::medianBlur(cv::Mat(SampleImg), tImg0Mat, 1);
        IplImage tImg0 = IplImage(tImg0Mat);

        IplImage *destbwImg = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);
        //    cvCopy(subImg1,destbwImg);
        cvSetZero(destbwImg);
        CvScalar tempScalar0;
        double min0d[4];
        double min0dv, max0dv;


        for(int i = 2; i < sh - 2; i++)
            for(int j = 2; j < sw - 2; j++)
            {

                min0d[0] = cvGet2D(&tImg0, i, j - 1).val[0];
                min0d[1] = cvGet2D(&tImg0, i - 1, j).val[0];
                min0d[2] = cvGet2D(&tImg0, i + 1, j).val[0];
                min0d[3] = cvGet2D(&tImg0, i, j + 1).val[0];

                min0dv = min0d[0];
                max0dv = min0d[0];
                for(int k = 1; k < 4; k++)
                {
                    if(min0dv > min0d[k])
                    {
                        min0dv = min0d[k];
                    }
                    if(max0dv < min0d[k])
                    {
                        max0dv = min0d[k];
                    }
                }
                //            if(min0dv==0)
                //                min0dv = 1;
                if(min0dv <= 3 || max0dv == 255)
                {
                    tempScalar0.val[0] = 0;
                    cvSet2D(bwImg, i, j, tempScalar0);
                }
                else
                {
                    tempScalar0.val[0] = max0dv * (cvGet2D(&tImg0, i, j).val[0]) / min0dv;
                    if(tempScalar0.val[0] > 255)
                        tempScalar0.val[0] = 255;

                    cvSet2D(bwImg, i, j, tempScalar0);

                }

            }

//            cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/sin_bwImg_"+QString::number(num,10).toLatin1()+".bmp",bwImg);

//        qDebug()<<"**********after gradient detect mode*********"<<num;


        //add by sun 20181008
//    qDebug()<<"enter**********single_Hist************"<<num;

        int HistogramBins = 256;
        float HistogramRange1[2] = {0, 255};
        float *HistogramRange[1] = {&HistogramRange1[0]};
        CvHistogram *Histogram1 = cvCreateHist(1, &HistogramBins, CV_HIST_ARRAY, HistogramRange);
        cvCalcHist(&bwImg, Histogram1);

        float  MaxValueH;
        double arc;
//                 double ThresHc;
        ThresHc = 0;
        cvSetReal1D(Histogram1->bins, 0, 0);
        //qDebug()<< num <<" Histogram1 before= "<<num;
        cvGetMinMaxHistValue(Histogram1, 0, &MaxValueH, 0, &MaxLocationh);

        for (int i = 0; i < HistogramBins; i++)
        {
            arc = cvGetReal1D(Histogram1->bins, i);
//                     //qDebug()<< num <<" arc = "<<arc;
            //        arc = cvGetReal1D(Histogram1 ,i);
            if ( arc < 2 && i > MaxLocationh)
            {
                ThresHc = i;
                break;
            }
        }
//     qDebug()<< "after cvReleaseHist+++++++++++ "<<num;

        cvThreshold(bwImg, destbwImg, ThresHc, 255, CV_THRESH_BINARY);


//              CvScalar ava0ValueR = cvAvg(bwImgR);
//              cvThreshold(bwImgR, destbwImgR, 2.3*ava0ValueR.val[0], 255, CV_THRESH_BINARY);
//     cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/sin_destbwImg_"+QString::number(num,10).toLatin1()+".bmp",bwImg);

        cvReleaseImage(&bwImg);
        cvReleaseHist(&Histogram1);
        tImg0Mat.release();

        //qDebug()<<"after///////release 914//////////////////"<<num;

        //保存梯度图像bwImg

//         CvScalar ava0Value4 = cvAvg(destbwImg);
//             //qDebug() << num <<"-ThresHc = "<< ThresHc;
//     cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/fbwImg_"+QString::number(num,10).toLatin1()+".bmp",destbwImg);
        //----------------求二值化连通域，并初定位Spot位置-------------------------
autoThreshold:
        if(autoThresholdMode)
        {
            QString path = QString("/media/images/" + g_currentSubjectNum);
            QDir myDir(path);
            if(!myDir.exists())
                myDir.mkdir(path);

            cvZero(destbwImg);
            cvThreshold(SampleImg, destbwImg, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
            cvSaveImage(path.toLatin1()+"/autoBwImgR_" + QString::number(num, 10).toLatin1() + ".bmp", destbwImg);
            qDebug() << num << "active autoThresholdMode********************";
        }
        // 查找轮廓，对应连通域
        CvMemStorage *mem_storage = cvCreateMemStorage(0);
        CvSeq *contours = NULL;
        int contours_num = cvFindContours(destbwImg, mem_storage, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
//    qDebug()<<num<<"gradient the num of contours is: "<< contours_num;
        if(contours_num == 0)
        {
            qDebug() << num << "contours false!! 936";
            cvReleaseImage(&bwImg);
            cvReleaseImage(&destbwImg);
            cvClearMemStorage(mem_storage);
            cvReleaseMemStorage(&mem_storage);
            cvReleaseImage(&SampleImg);
//        cvReleaseImage(&SpotEhanceSampleImg);
            cvReleaseImage(&Img);
            return 0;
        }


        int minAreaTH = 20;
        int maxAreaTH = 200;
        int minDTH = 5;
        int maxDTH = 20;


//        vector<uchar> vec_ValidPixel;
        vector<CvRect> vec_Rect;
        QMap<int, CvRect> map_RectToPixel;
        map_RectToPixel.clear();
        for (; contours; contours = contours->h_next)
        {
            CvRect rect1 = cvBoundingRect(contours, 0);
            double area1 = fabs(cvContourArea(contours));
            //        int Tmp_D = max(rect1.width, rect1.height);
            //        if (Tmp_D >= minDTH && Tmp_D < maxDTH )
            int Tmp_H = rect1.height;
            int Tmp_W = rect1.width;
            //        ////qDebug()<< "runtask contours max high:" << Tmp_H;
            if (Tmp_H >= minDTH && Tmp_W >= minDTH && Tmp_H <= maxDTH && Tmp_W <= maxDTH && area1 > minAreaTH && area1 < maxAreaTH)
            {
                vec_Rect.push_back(rect1);
            }
        }

        qDebug() << num << "--sin-vec_Rect.size():" << vec_Rect.size();


        if (vec_Rect.size() >= 1)
        {

            //add by sun 20181021*********************************************************************
            for(int i = 0; i < vec_Rect.size(); i++)
            {
                uchar tmpPixel1 = 0;
                CvRect Rect1 = vec_Rect.at(i);
                for (int y = Rect1.y; y < Rect1.y + Rect1.height; y++)
                {
                    for (int x = Rect1.x; x < Rect1.x + Rect1.width; x++)
                    {
                        uchar pixel1 = *(SampleImg->imageData + SampleImg->widthStep * y + x);
                        if (tmpPixel1 < pixel1)
                        {
                            tmpPixel1 = pixel1;
                        }
                    }
                }

                map_RectToPixel.insert(tmpPixel1, Rect1);
//                qDebug()<<num<<"map_RectToPixel.size="<<map_RectToPixel.size();
            }


//                qDebug()<<"vec_pyrRects1 vecRow ="<<vecRow1;
//                qDebug()<<"--map_RectDistdiffer.size()="<<map_RectDistdiffer.size();

            CvRect singleRect;
            if(map_RectToPixel.size() > 0)
            {
                singleRect = map_RectToPixel.last();
            }
            else
            {

//                    qDebug()<<num<<"map_RectToPixel.size()==0";
                vec_Rect.clear();
                vector<CvRect> (vec_Rect).swap(vec_Rect);
                cvReleaseImage(&bwImg);
                cvReleaseImage(&destbwImg);
                cvClearMemStorage(mem_storage);
                cvReleaseMemStorage(&mem_storage);
                cvReleaseImage(&SampleImg);
//                    cvReleaseImage(&SpotEhanceSampleImg);
                cvReleaseImage(&Img);
//                    qDebug()<<num<<"+++++++++++release resource+++++++++++complete,return";
                return 0;
            }

            bcenter1.x = singleRect.x + singleRect.width / 2;
            bcenter1.y = singleRect.y + singleRect.height / 2;


            //qDebug() <<num<< "finalPyrRects:<"<<Rect1.x<<","<<Rect1.y<<">,<"<<Rect2.x<<","<<Rect2.y<<">";

            //add end****************************************************************************************

            if (bcenter1.x > edgeTH && bcenter1.x < (sw - edgeTH) && bcenter1.y > edgeTH && bcenter1.y < (sh - edgeTH))
            {
                bSpotCoarseLocation = true;
//                        qDebug()<<num<<"-runtask,bSpotCoarseLocation == true";

            }
            else
            {
                bSpotCoarseLocation = false;
                qDebug() << num << "-runtask,bSpotCoarseLocation == false2";
            }
        }
        else
        {
            bSpotCoarseLocation = false;
            qDebug() << num << "-runtask,bSpotCoarseLocation == false3";

        }


        qDebug() << "--single eye: bcenter1.x = " << bcenter1.x << "; bcenter1.y = " << bcenter1.y;

        //qDebug()<<"after vec_ValidPixel ------1040-----";
//            vec_ValidPixel.clear();
        vec_Rect.clear();
        vector<CvRect> (vec_Rect).swap(vec_Rect);
        map_RectToPixel.clear();


        if(bSpotCoarseLocation == false && !autoThresholdMode)
        {
            autoThresholdMode = true;
            cvClearMemStorage(mem_storage);
            cvReleaseMemStorage(&mem_storage);
            qDebug() << "bSpotCoarseLocation == false,enter auto threshold mode " << num;
            goto autoThreshold;
        }
        cvReleaseImage(&bwImg);
        cvReleaseImage(&destbwImg);
        cvClearMemStorage(mem_storage);
        cvReleaseMemStorage(&mem_storage);
//            cvReleaseImage(&tImg0);

//        qDebug()<<num<<"release tImg0+++++++++++++++++++++";
    }

//    cvReleaseImage(&SpotEhanceSampleImg);

    if (bSpotCoarseLocation == false)
    {

        qDebug() << "detect  failed, bSpotCoarseLocation == false " << num;
        cvReleaseImage(&Img);
        cvReleaseImage(&SampleImg);
        return 0;
    }
//    else{
//        qDebug()<<"detect sucess, bSpotCoarseLocation == true "<<num;

//    }

//    QTime endprocessPic = QTime::currentTime();
//    int endprocessPictime = detectbegin.msecsTo(endprocessPic);

    ////////////////////////////求平均灰度值////////////////////////////////
    CvScalar SampleImg_avaVal = cvAvg(SampleImg);
    double fix_ava = SampleImg_avaVal.val[0] * 2;
//    qDebug()<<num<<"---------SampleImg_avaVal:"<<SampleImg_avaVal.val[0]<<"fix_ava:"<<fix_ava<<"-------------";

    cvReleaseImage(&SampleImg);

    ////qDebug()<<"finish finish cu dingwei  = "<<endprocessPictime;
    /////////////////////////////////精确定位/////////////////////////////////



    // ------------------从原始图中取出感兴趣区域---------------------------
    //------------left eye---------------------
    CvRect RectSub1;
    RectSub1.x = bcenter1.x * ratio - ROIW_HALF;
    RectSub1.y = bcenter1.y * ratio - ROIW_HALF;
    RectSub1.width = 2 * ROIW_HALF;
    RectSub1.height = 2 * ROIW_HALF;

    if (RectSub1.x < 0 || (RectSub1.x + RectSub1.width) > Img->width
            || RectSub1.y < 0 || (RectSub1.y + RectSub1.height) > Img->height)
    {
        qDebug() << "RectSub1 failed,return++++" << num;
        cvReleaseImage(&Img);
        return 0;
    }
    cvSetImageROI(Img, RectSub1);
    IplImage *subImg1 = cvCreateImage(cvSize(RectSub1.width, RectSub1.height), Img->depth, Img->nChannels);
    cvCopy(Img, subImg1, NULL);
    cvResetImageROI(Img);

//    cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/subImg_" +QString::number(num,10).toLatin1()+".bmp",subImg1);


    double PreciseMinValue1 = 0.0;
    double PreciseMaxValue1 = 0.0;
    CvPoint PreciseMinLocation1;
    CvPoint PreciseMaxLocation1;
    // CvPoint center1;
    cvMinMaxLoc(subImg1, &PreciseMinValue1, &PreciseMaxValue1, &PreciseMinLocation1, &PreciseMaxLocation1);

//    if(PreciseMaxValue1 < fix_ava ){
//       qDebug()<<num<<"--PreciseMaxValue1 < fix_ava ----"<<PreciseMaxValue1;
//       cvReleaseImage(&subImg1);
//       cvReleaseImage(&Img);
//       return 0;
//   }
    ////qDebug()<<"========================2052=========================";

    //2080928
//    qDebug() << "before single tImg11111111111111111111111111111--"<<num;


    IplImage *thrSubImg1 = cvCreateImage(cvSize(ROIW, ROIH), IPL_DEPTH_8U, 1);
    cvSetZero(thrSubImg1);
//    IplImage *tImg1 = cvCloneImage(thrSubImg1);


//    cv::Mat tImg1Mat;
//    cv::medianBlur(cv::Mat(subImg1),tImg1Mat,5);
//    IplImage tImg1  = IplImage(tImg1Mat);//
    IplImage *destThrSubImg1 = cvCreateImage(cvSize(ROIW, ROIH), IPL_DEPTH_8U, 1);
    cvSetZero(destThrSubImg1);

    CvScalar tempScalar;
    double mind[4];
    double mindv, maxdv;

    if(true)
    {
        for(int j = 2; j < ROIH - 2; j++)
        {
            for(int i = 2; i < ROIW - 2; i++)
            {

                mind[0] = cvGet2D(subImg1, j, i - 1).val[0];
                mind[1] = cvGet2D(subImg1, j - 1, i).val[0];
                mind[2] = cvGet2D(subImg1, j + 1, i).val[0];
                mind[3] = cvGet2D(subImg1, j, i + 1).val[0];

//            ////qDebug()<<"**************cvGEt2DTime:"<< cvGet2DTimeBegin.msecsTo(cvGet2DFinishTime);
                mindv = mind[0];
                maxdv = mind[0];
                for(int k = 1; k < 4; k++)
                {
                    if(mindv > mind[k])
                    {
                        mindv = mind[k];
                    }
                    if(maxdv < mind[k])
                    {
                        maxdv = mind[k];
                    }
                }
//            if(mindv==0)
//                mindv = 1;
                if(mindv <= 3 || maxdv == 255)
                {
                    tempScalar.val[0] = 0;
                    cvSet2D(thrSubImg1, j, i, tempScalar);
                }
                else
                {
                    tempScalar.val[0] = cvGet2D(subImg1, j, i).val[0] / mindv * maxdv;
                    cvSet2D(thrSubImg1, j, i, tempScalar);
                }

            }
        }
    }
    //qDebug() <<num<< "jing que ding wei jiaohuan qian ThresHcL1:--"<<ThresHcL;
    CvScalar avaValue = cvAvg(thrSubImg1);

    avaValue.val[0] = avaValue.val[0] * 1.1;
    if(ThresHc > avaValue.val[0])
        ThresHc = avaValue.val[0];
    qDebug() << num << "jing que ding wei hou ThresHc1:--" << ThresHc << "avaValue.val[0]" << avaValue.val[0];

    CvMemStorage *mem_storage1 = cvCreateMemStorage(0);
    CvSeq *contours1;
    CvSeq *maxContour1;
    double differValue = 10000;

AutoadjustthreshL:

    if (varThresholdState == 1)
        ThresHc = ThresHc * 1.1;

    if(varThresholdState == 2)
        ThresHc = ThresHc * 0.8;

    cvThreshold(thrSubImg1, destThrSubImg1, ThresHc, 255, CV_THRESH_BINARY );
    //保存梯度图像thrSubImg1
//     cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/L_destThrSubImg1_"+ QString::number(ThresHcL,'f',1).toLatin1() +QString::number(num,10).toLatin1()+".bmp",destThrSubImg1);
//     cvSaveImage("/media/photo/"+ g_currentSubjectNum.toLatin1() +"/destThrSubImg1_" +QString::number(num,10).toLatin1()+".bmp",destThrSubImg1);

    cvClearMemStorage(mem_storage1);
    contours1 = NULL;
//    int contours_num1 = cvFindContours(thrSubImg1, mem_storage1, &contours1, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
    int contours_num1 = cvFindContours(destThrSubImg1, mem_storage1, &contours1, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));
//    qDebug() <<num<< "after cvFindContours thrSubImg111111111111111111111111--,contour num:"<<contours_num1;


    if (contours_num1 < 1)
    {
//        printf("检查失败，无法找到瞳孔位置！");
        qDebug() << "检查失败，无法找到瞳孔位置";
        cvClearMemStorage(mem_storage1);
        cvReleaseMemStorage(&mem_storage1);
        cvReleaseImage(&destThrSubImg1);
        cvReleaseImage(&thrSubImg1);
        cvReleaseImage(&subImg1);
        //cvReleaseImage(&tImg1);
        cvReleaseImage(&Img);
        return 0;
    }


    // ***************************find the best Contours-single**************************
    maxContour1 = contours1;
    CvRect maxRect1;
    maxRect1.height = 0;
    maxRect1.width = 0;
    differValue = 10000;

    float radius1;
    CvPoint2D32f bbcenter1;
    bbcenter1.x = 0;
    bbcenter1.y = 0;
    CvPoint center1;
    double pyrAreaValue = 0;
    if (singleDoubleEyeMode_Left == g_SingleDoubleEye)
        pyrAreaValue = g_pyrArea[0];
    else if (singleDoubleEyeMode_Right == g_SingleDoubleEye)
        pyrAreaValue = g_pyrArea[1];

    for (; contours1 != 0; contours1 = contours1->h_next)
    {

        CvRect maxRect = cvBoundingRect(contours1, 0);
//-        qDebug()<<num<<"contour1:"<<maxRect.width<<","<<maxRect.height;
        double tempEyeArea = fabs(cvContourArea(contours1));
        double TempDifferValue = abs(tempEyeArea - pyrAreaValue);

        double Width = maxRect.width;
        double Height = maxRect.height;
        if((Width / Height) >= 2 || tempEyeArea < 400)
            continue;

//        qDebug()<<"contours1 maxRect.width * maxRect.height   = "<<maxRect.width<<"and"<<maxRect.height<<num;
//      if(abs(tempEyeArea - g_pyrArea[0]) < DeviationP && abs(maxLengthL - (saturationCenterL.z+1.5)*2) < 10)

        if(TempDifferValue < differValue)
        {
            differValue = TempDifferValue;
            maxContour1 = contours1;
            maxRect1 = maxRect;
//              qDebug()<<num<<"TempDifferValue < differValue-----maxContour1:"<<maxRect.width<<","<<maxRect.height;
        }
        else
        {
//              qDebug()<<num<<"else TempDifferValue >= differValue-----maxContour1:"<<maxRect.width<<","<<maxRect.height;

        }
    }

//    qDebug()<<num<<"maxContour1 success---------------------";


    //add by sun 20181018********************left****************************

    if(g_isHmMode)
    {
        qDebug() << "--enter hmMode--:";
        /*
                if((LeftEyeArea - g_pyrArea[0]) > g_pyrArea[0]/10 || maxRect1.width-pyrRectWidth[0] > 10){
        //                qDebug()<<"LeftEyeArea:"<<LeftEyeArea<<",g_pyrArea[0]:"<<g_pyrArea[0];
        //                qDebug()<<num<<"LeftEyeArea - g_pyrArea[0] > DeviationP *******************"<<(LeftEyeArea - g_pyrArea[0]);
                        varThresholdStateL = 1;

        //                qDebug()<<num<<"reTestNumLeft ="<<reTestNumLeft;
                        if(reTestNumLeft<=3){
                            reTestNumLeft++;
                            goto AutoadjustthreshL;
                        }
                        else{
        //                    qDebug()<<"reTestNumLeft > 5 ";
                            double reTestEyeAreaL = fabs(cvContourArea(maxContour1));
                            if(reTestEyeAreaL > g_pyrArea[0]*1.4 || reTestEyeAreaL < g_pyrArea[0]*0.5){
        //                        qDebug()<<"retest failed!!";

                                bbcenter1.x=PreciseMaxLocation1.x;
                                bbcenter1.y=PreciseMaxLocation1.y;
        //                        return 0;
                            }
                        }

                }
                else if((LeftEyeArea - g_pyrArea[0]) < - g_pyrArea[0]/10 || maxRect1.width-pyrRectWidth[0] < -10){
        //               qDebug()<<"LeftEyeArea:"<<LeftEyeArea<<",g_pyrArea[0]:"<<g_pyrArea[0];
        //                qDebug()<<num<<"LeftEyeArea - g_pyrArea[0] < -DeviationP *******************"<<(LeftEyeArea - g_pyrArea[0]);
                         varThresholdStateL = 2;

        //                 qDebug()<<num<<"reTestNumLeft ="<<reTestNumLeft;
                         if(reTestNumLeft<=3){
                             reTestNumLeft++;
                             goto AutoadjustthreshL;
                         }
                         else{
        //                     qDebug()<<"reTestNumLeft > 5 ";
                             double reTestEyeAreaL = fabs(cvContourArea(maxContour1));
                             if(reTestEyeAreaL > g_pyrArea[0]*1.4 || reTestEyeAreaL < g_pyrArea[0]*0.5){
        //                         qDebug()<<"retest failed!!";
                                 bbcenter1.x=PreciseMaxLocation1.x;
                                 bbcenter1.y=PreciseMaxLocation1.y;
        //                         return 0;
                             }
                         }

                }
        //        qDebug()<<"after hmMode----LeftEyeArea:"<<LeftEyeArea<<",g_pyrArea[0]:"<<g_pyrArea[0];
        */
        bbcenter1.x = PreciseMaxLocation1.x;
        bbcenter1.y = PreciseMaxLocation1.y;
    }

    //end*******************************************************************


    int conNum1 = cvMinEnclosingCircle(maxContour1, &bbcenter1, &radius1); //对轮廓进行多变形逼近
//    qDebug()<<"after cvMinEnclosingCircle(maxContour1,conNum1:"<<conNum1;
    if (conNum1 == 0)
    {
        qDebug() << "conNum1 == 0,return";
        cvReleaseImage(&subImg1);
        cvClearMemStorage(mem_storage1);
        cvReleaseMemStorage(&mem_storage1);
        cvReleaseImage(&thrSubImg1);
        cvReleaseImage(&destThrSubImg1);
        cvReleaseImage(&Img);
        return 0;
    }
    //*************************minEnclosingCircle*************************20180929
//    IplImage *minEnclosingCircle = cvCreateImage(cvSize(ROIW,ROIH), IPL_DEPTH_8U, 1);
//    cvCopy(subImg1,minEnclosingCircle);
//    cvDrawContours(minEnclosingCircle, maxContour1, CV_RGB(255, 255, 255), CV_RGB(255, 255, 255), 2, CV_FILLED, 8, cvPoint(0, 0));
//    cvCircle(minEnclosingCircle, cvPoint(cvRound(bbcenter1.x),cvRound(bbcenter1.y)), cvRound(radius1), CV_RGB(255,255,255), 1, 8, 0 );
//    cvSaveImage("/media/subImg/minEnclosingCircle1_single_"+QString::number(num,10).toLatin1()+".bmp",minEnclosingCircle);//add by sun 20180928
    //******************************end************************************
    cvReleaseImage(&subImg1);
    cvClearMemStorage(mem_storage1);
    cvReleaseMemStorage(&mem_storage1);
    cvReleaseImage(&thrSubImg1);
    cvReleaseImage(&destThrSubImg1);
    //cvReleaseImage(&tImg1);

    // 上睑下垂判断
    double AspectRatio = (double)((double)(maxRect1.height) / (double)(maxRect1.width));;
    if(AspectRatio < 0.5){
        qDebug()<<num<<"false:AspectRatio < 0.5 ="<< AspectRatio;
        return 0;
    }
    if(num>0 && num<7){
        if (singleDoubleEyeMode_Left == g_SingleDoubleEye) {
            g_pyrAR.L_AspectRatio[num-1] = AspectRatio;
            qDebug()<<num<<"AspectRatio="<<AspectRatio;
        }
        else if (singleDoubleEyeMode_Right == g_SingleDoubleEye) {
            g_pyrAR.R_AspectRatio[num-1] = AspectRatio;
            qDebug()<<num<<"AspectRatio="<<AspectRatio;
        }

    }

    center1.x = bbcenter1.x;
    center1.y = bbcenter1.y;
    //cvCircle(SpotEnhanceSub1, center1, radius1, CV_RGB(255, 255, 255), 3);
    //----------------------------------------------------------------------------

    center1.x += RectSub1.x;
    center1.y += RectSub1.y;

//    cout <<num<< "--圆心点为： （ " << center1.x << " ,  " << center1.y << " ), 半径为：" << radius1 << endl;

    int center1LeftY = center1.y - ROIW_HALF;
    int center1LeftX = center1.x - ROIW_HALF;

    ////added by ron
    if (center1LeftY < MINIRECT || center1LeftY > MAXRECT
            || center1LeftX < SLIDELEFT || center1LeftX > (IMG_WIDTH / 2 - ROIW))
    {
        qDebug() << "chao yue bian jie";
        return 0;
    }


    avalPoint3D[0].x = center1.x;
    avalPoint3D[0].y = center1.y;
    if(g_isHmMode)
        avalPoint3D[0].z = g_SaturationCenterL.z;
    else
        avalPoint3D[0].z = radius1;


    ////qDebug()<<"pupil_distance = "<<pupil_distance;
    qDebug() << "finish pyrdetect ---------------" << num;
    return 1;
}



void RunTask::SpotEnhance(IplImage *src, IplImage *dst)
{
    int w = src->width;
    int h = src->height;

    IplImage *temp1 = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);
    IplImage *temp2 = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);

    // 去除高光
    cvNot(src, temp1);
    lhMorpFillHole(temp1, temp2);
    cvNot(temp2, temp1);

    //cvShowImage("temp1", temp1);

    // 相减得到高光位置
    cvSub(src, temp1, dst);

    cvReleaseImage(&temp1);
    cvReleaseImage(&temp2);
}

void RunTask::rotateImage(IplImage *img, IplImage *img_rotate, int degree, CvPoint2D32f _deviation)
{
    // 旋转中心为图像中心
    CvPoint2D32f center;
    center.x = float(img->width) / 2 + _deviation.x;
    center.y = float(img->height) / 2 + _deviation.y;

    // 计算二维旋转的仿射变换矩阵
    float m[6];
    CvMat M = cvMat(2, 3, CV_32F, m);
    cv2DRotationMatrix(center, degree, 1, &M);

    // 变换图像，并用黑色填充其余值
    cvWarpAffine(img, img_rotate, &M, CV_INTER_LINEAR | CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
}

void RunTask::lhMorpFillHole(const IplImage *src, IplImage *dst)
{
    IplImage *temp = cvCloneImage(src);
    double min, max;
    cvMinMaxLoc(src, &min, &max);

    //标记图像
    cvRectangle(temp, cvPoint(3, 3), cvPoint(temp->width - 7, temp->height - 7), CV_RGB(max, max, max), -1);

    //将原图像作为掩模图像
    lhMorpRErode(temp, src, dst, NULL, -1);

    cvReleaseImage(&temp);
}

//形态学测地腐蚀和腐蚀重建运算
void RunTask::lhMorpRErode(const IplImage *src, const IplImage *msk, IplImage *dst, IplConvKernel *se, int iterations)
{
    assert(src != NULL  && msk != NULL && dst != NULL && src != dst);

    if (iterations < 0)
    {
        //腐蚀重建
        cvMax(src, msk, dst);
        cvErode(dst, dst, se);
        cvMax(dst, msk, dst);

        int width = src->width;
        int height = src->height;
        IplImage  *temp1 = cvCreateImage(cvSize(width, height), 8, 1);
        do
        {
            //record last result
            cvCopy(dst, temp1);
            cvErode(dst, dst, se);
            cvMax(dst, msk, dst);
        }
        while (lhImageCmp(temp1, dst) != 0);

        cvReleaseImage(&temp1);

        return;

    }
    else if (iterations == 0)
    {
        cvCopy(src, dst);
    }
    else
    {
        //普通测地腐蚀 p137(6.2)
        cvMax(src, msk, dst);
        cvErode(dst, dst, se);
        cvMax(dst, msk, dst);

        for (int i = 1; i < iterations; i++)
        {
            cvErode(dst, dst, se);
            cvMax(dst, msk, dst);
        }
    }
}

int RunTask::lhImageCmp(const IplImage *img1, const IplImage *img2)
{
    assert(img1->width == img2->width && img1->height == img2->height && img1->imageSize == img2->imageSize);
    return memcmp(img1->imageData, img2->imageData, img1->imageSize);
}

bool RunTask::test_processPic()
{
    bool is_succ = processPic(proceImage, imageNum, singleDoubleEyeMode_Both);
    return is_succ;
}

