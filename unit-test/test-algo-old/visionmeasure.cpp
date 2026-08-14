#include "visionmeasure.h"

#include <string>

#include <QMessageBox>
#include <QDir>
#include <QApplication>

#include "opencv2/core/core.hpp"

#include "util-common.h"
#include "logger.h"

#include "virtualinterface.h"

// 类型转换（前置声明）
CvRect CRect_to_CvRect(const CRect &_c_rect);
CRect CvRect_to_CRect(const CvRect &_cv_rect);
CvPoint2D32f CPointF_to_CvPoint2D32f(CPointF _c_point);
CPointF CvPoint2D32f_to_CPointF(CvPoint2D32f _cv_point);

//
CVisionMeasure::CVisionMeasure(QObject *parent) : QObject(parent)
{
    mAlgo.moveToThread(&mAlgoThread);

    qRegisterMetaType<stVisionValue>("stVisionValue");
    qRegisterMetaType<stVisionAbnormal>("stVisionAbnormal");

    QObject::connect(&mAlgo, &CAlgoObj::sigCalcVisionFinished, this, &CVisionMeasure::slotShowResult, Qt::DirectConnection);

}

// 检测指定图像的瞳孔
bool CVisionMeasure::detectPupilOfImg(const QString &_img_path, enAlgoVerAll _pupil_algo_ver, int _img_num, bool _is_calc_expo)
{
    if (!QFile::exists(_img_path)) {
        QMessageBox::critical((QWidget*)this->parent(), "error", "file not found!", QMessageBox::Ok);
        return false;
    }

    //
    if (_img_num < 0) {
        int pos = _img_path.lastIndexOf(QDir::separator());
        _img_num = Util::CScreenerImgsData::getIndexFromFileName(_img_path.mid(pos + 1));
    }

    //
    IplImage *img = cvLoadImage(_img_path.toStdString().data(), CV_LOAD_IMAGE_GRAYSCALE);               // 本地运行，为什么第一次载入的图，底部三分一是花的？之后就正常？
    //IplImage *img_tmp = cvCreateImage(cvSize(img_load->width, img_load->height), IPL_DEPTH_8U, 1);
    //cvCvtColor(img, img_tmp, CV_BGR2GRAY);
    //img = img_tmp;
    //cvFlip(img, NULL, 1);
    uchar *img_data = (uchar *)img->imageData;

    CAlgoObj::setIsCalculatingVision(false);

    CGlobal::algoSaveImgIndex = -1;
    mAlgo.setCurrentPupilAlgoVer(_pupil_algo_ver);

    stPupilInfo pupil_info_r, pupil_info_l;
    //memset(&pupil_info_l, 0, sizeof(stPupilInfo));
    //memset(&pupil_info_r, 0, sizeof(stPupilInfo));

    bool succ = mAlgo.detectPupil(img_data, _img_num, ageRange_4_20_100_YEAE, pupil_info_r, pupil_info_l, false, singleDoubleEyeMode_Both);

    QString str_info = QString::asprintf("CVisionMeasure::detectPupilOfImg(): num: %d, right: (%.0f, %.0f), %.0f; left: (%.0f, %.0f), %.0f", _img_num,
                                         pupil_info_r.center.x, pupil_info_r.center.y, pupil_info_r.radius,
                                         pupil_info_l.center.x, pupil_info_l.center.y, pupil_info_l.radius);
    qDebug() << str_info;

    //
#ifdef TEST_MODE
    {
        bool is_show_img = true;        // 显示识别到的瞳孔
        if (is_show_img) {
            IplImage *img_rgb = cvCreateImage(cvSize(img->width, img->height), IPL_DEPTH_8U, 3);
            cvResetImageROI(img);
            cvCvtColor(img, img_rgb, CV_GRAY2RGB);

            const int SPACE = 10;

            CvPoint center_r = cvPointFrom32f(CPointF_to_CvPoint2D32f(pupil_info_r.center));
            CvPoint center_l = cvPointFrom32f(CPointF_to_CvPoint2D32f(pupil_info_l.center));
            cvCircle(img_rgb, center_r, 1, CV_RGB(0, 255, 0), 1);
            cvCircle(img_rgb, center_r, pupil_info_r.radius + SPACE, CV_RGB(0, 255, 0), 1);
            cvCircle(img_rgb, center_l, 1, CV_RGB(0, 255, 0), 1);
            cvCircle(img_rgb, center_l, pupil_info_l.radius + SPACE, CV_RGB(0, 255, 0), 1);

            cvShowImage(QString("img_%1").arg(_img_num).toLocal8Bit().data(), img_rgb);

            //cvShowImage("img", img_rgb);
            //cvWaitKey();
        }
    }
#endif

    //
    if (_is_calc_expo && succ) {
        int avg = -1;
        bool is_over = false;
        bool is_succ_calc_expo = mAlgo.calcExposure(img_data, _img_num, pupil_info_r, pupil_info_l, avg, is_over, singleDoubleEyeMode_Both);
        if (is_succ_calc_expo) {
            qDebug() << QString::asprintf("avg = %d, is_over = %d", avg, is_over);
        }
    }

    //
    cvReleaseImage(&img);

    //
    if (!succ) {
        logDebug(QString("CVisionMeasure::detectPupilOfImg() : mAlgo.pupilDetectionResultDesc = ") + pupilDetectionResultDesc[mAlgo.pupilDetectionResult]);
        return false;
    } else {
        return true;
    }
}

// 检测指定文件夹中所有图像的瞳孔
void CVisionMeasure::detectPupilOfImgsOfDir(const QString &_dir_path, enAlgoVerAll _pupil_algo_ver, int &_count_succ, int &_count_fail)
{
    _count_succ = 0;
    _count_fail = 0;

    //
    QDir dir_imgs(_dir_path);
    QFileInfoList file_info_list = dir_imgs.entryInfoList();
    QMap<int, QString> file_list;
    foreach (QFileInfo file_info, file_info_list) {
        QString file_name = file_info.fileName();
        if (file_name.endsWith(".bmp") || file_name.endsWith(".png") || file_name.endsWith(".jpg")) {
            int idx_begin = -1;
            int idx_end = -1;
            int num = -1;
            for (int i = 0; i < file_name.length(); i++) {
                if (file_name[i].isNumber()) {
                    if (idx_begin < 0) {
                        idx_begin = i;
                    }
                } else if (idx_begin >= 0) {
                    idx_end = i;
                }
                if (idx_end >= 0) {
                    num = file_name.mid(idx_begin, idx_end - idx_begin).toInt();
                    break;
                }
            }
            file_list.insert(num, file_info.absoluteFilePath());
        }
    }

    bool is_succ;
    QList<int> num_list = file_list.keys();
    foreach (int num, num_list) {
        is_succ = detectPupilOfImg(file_list.value(num), _pupil_algo_ver, num, false);
        if (is_succ) {
            _count_succ++;
        } else {
            qDebug() << "CVisionMeasure::detectPupilOfImgsOfDir(): failed, path = " << file_list.value(num);
            _count_fail++;
        }
    }
}

// 计算指定目录的图集的屈光度
bool CVisionMeasure::calcVisionOfImgs(QString _img_dir, enAlgoVerAll _pupil_algo_ver, enAlgoVerAll _calc_algo_ver, bool _is_single_thread)
{
    //
    Util::CScreenerImgsData imgs_data;
    imgs_data.imgsDir = _img_dir;
    imgs_data.isLoadPreImg = false;
    imgs_data.fileType = Util::imgFileType_Bmp;
    imgs_data.isNeedZeroth = false;
    imgs_data.useSimulateImage = false;
    imgs_data.setImgSize(IMG_WIDTH, IMG_HEIGHT);

    if (imgs_data.loadImgFiles(true)) {
        resultByte.clear();
        for (int i =0; i < 23; i++) {
            BYTE* img_data = imgs_data.getImage(i);
            resultByte.push_back(img_data);
        }

        calcSucceeded = false;
        CGlobal::algoSaveImgIndex = 30;

        enCalcResultState calc_state = calcResultState_Fail;

        mAlgo.setCurrentPupilAlgoVer(_pupil_algo_ver);
        g_SaturationCenterR.z = 0;
        mAlgo.slotDetectPupil(resultByte.at(1), 1, ageRange_4_20_100_YEAE, false);

        if (Util::compDouble(g_SaturationCenterR.z, 5) > 0) {
            //mAlgo.setCurrentPupilAlgoVer(_calc_algo_ver);
            mAlgo.setCurrentPupilAlgoVer(_pupil_algo_ver);
            CGlobal::isSingleThreadCalc = _is_single_thread;
            calc_state = mAlgo.calcVision(resultByte, singleDoubleEyeMode_Both);
        } else {
            return false;
        }

        calcSucceeded = (calcResultState_Succ == calc_state);
        return calcSucceeded;
    } else {
        QMessageBox::information((QWidget*)this->parent(), "Information", "load images failed!", QMessageBox::Ok);
        return false;
    }
}

//
void CVisionMeasure::slotShowResult(bool _is_succ, stVisionValue _vision, stVisionAbnormal _value_unnormal)
{
    calcSucceeded = _is_succ;

    qDebug() << "";
    qDebug() << "sph_r \t cyl_r \t axis_r \t ps_r \t \t sph_l \t cyl_l \t axis_l \t ps_l \t \t pd";
    qDebug() << QString::number(_vision.RSph, 'f', 2).toLocal8Bit().data()
             << "\t" << QString::number(_vision.RCyl, 'f', 2).toLocal8Bit().data()
             << "\t" << QString::number(_vision.RAx).toLocal8Bit().data()
             << "\t" << QString::number(_vision.RPs, 'f', 2).toLocal8Bit().data()
             << "\t"
             << "\t" << QString::number(_vision.LSph, 'f', 2).toLocal8Bit().data()
             << "\t" << QString::number(_vision.LCyl, 'f', 2).toLocal8Bit().data()
             << "\t" << QString::number(_vision.LAx).toLocal8Bit().data()
             << "\t" << QString::number(_vision.LPs, 'f', 2).toLocal8Bit().data()
             << "\t"
             << "\t" << QString::number(_vision.PD, 'f', 2).toLocal8Bit().data()
                ;
    qDebug() << "";
}

// 测试 Runtask::processPic0() 函数
bool CVisionMeasure::test_Runtask_processPic0(const QString &_img_path, int _img_num, enAlgoVerAll _pupil_algo_ver)
{
    if (!QFile::exists(_img_path)) {
        QMessageBox::critical((QWidget*)this->parent(), "error", "file not found!", QMessageBox::Ok);
        return false;
    }

    //
    if (_img_num < 0) {
        int pos = _img_path.lastIndexOf(QDir::separator());
        _img_num = Util::CScreenerImgsData::getIndexFromFileName(_img_path.mid(pos + 1));
    }

    //
    bool succ = false;

    //
    IplImage *img = cvLoadImage(_img_path.toStdString().data(), CV_LOAD_IMAGE_GRAYSCALE);               // 本地运行，为什么第一次载入的图，底部三分一是花的？之后就正常？

    //g_CalcResultState = calcResultState_Succ;

    //RunTask task(img, _img_num, 0);
    //task.currentPupilAlgoVer = _pupil_algo_ver;
    //succ = task.test_processPic();
    // TODO: 这是旧算法代码定义的

    cvReleaseImage(&img);

    //
    return succ;
}

//
bool CVisionMeasure::ledPosiDetect(QString _img_dir, enAlgoVerAll _detect_algo_ver, enAlgoVerAll _calc_algo_ver)
{
//    Util::CScreenerImgsData imgs_data();
//    imgs_data.imgsDir = _img_dir;
//    imgs_data.isLoadPreImg = false;
//    imgs_data.fileType = Util::imgFileType_Bmp;
//    imgs_data.isNeedZeroth = false;
//    imgs_data.useSimulateImage = false;

//    imgs_data.clearFileNames();
//    QDir dir_imgs(_img_dir);
//    QFileInfoList file_info_list = dir_imgs.entryInfoList();
//    foreach (QFileInfo file_info, file_info_list) {
//        QString file_name = file_info.fileName();
//        if (file_name.startsWith("temp") && file_name.endsWith(".bmp")) {       // 命名规则：以 “temp” 开头，后接 2 位图像序号的 bmp 图像文件，参见 Util::CScreenerImgsData::getFileNameByIndex()
//            int idx = file_name.mid(4, 2).toInt();
//            imgs_data.addFileName(idx, file_name);
//        }
//    }

//    if (imgs_data.loadImgFiles(true)) {
//        resultByte.clear();
//        for (int i =0; i < 23; i++) {
//            BYTE* img_data = imgs_data.getImage(i);
//            resultByte.push_back(img_data);
//        }

//        calcSucceeded = false;
//        CGlobal::algoSaveImgIndex = 100;

//        //
//        for (int i = 1; i < resultByte.size(); i++) {
//            IplImage *img = cvCreateImageHeader(cvSize(IMG_WIDTH, IMG_HEIGHT), IPL_DEPTH_8U, 1);
//            cvSetData(img, resultByte.at(i), IMG_WIDTH);

//            // 提高对比度


//            // 高斯模糊


//            // 创建直方图


//            // 确定二值化阈值


//            // 二值化


//            //
//            break;
//        }

//        //
//        qApp->processEvents(QEventLoop::AllEvents, 200);

//        //QString stat = Camera::getRunStat(stat);
//        //return (MEASURE_SUCC == stat);

//        return calcSucceeded;
//    }
//    else {
//        QMessageBox::information((QWidget*)this->parent(), "Information", "load images failed!", QMessageBox::Ok);
//        return false;
//    }
}

