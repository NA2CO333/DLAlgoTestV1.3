#include "algo-invoker.h"

#include <string>

#include <QMessageBox>
#include <QDir>
#include <QApplication>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
//#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "util-common.h"
#include "logger.h"

#include "algo.h"

// 类型转换（前置声明）
CvRect CRect_to_CvRect(const CRect &_c_rect);
CRect CvRect_to_CRect(const CvRect &_cv_rect);
CvPoint2D32f CPointF_to_CvPoint2D32f(CPointF _c_point);
CPointF CvPoint2D32f_to_CPointF(CvPoint2D32f _cv_point);

// ========================================================
// 工具函数

// 计算图像的均值和标准差
void calcMeanStdDev(const cv::Mat &_img, double &_mean, double &_std_dev)
{
    cv::Mat mat_mean, mat_stddev;
    cv::meanStdDev(_img, mat_mean, mat_stddev);      // TODO: 通过标准差判断衡量对比度，是否合理？
    _mean = mat_mean.ptr<double>(0)[0];
    _std_dev = mat_stddev.ptr<double>(0)[0];
}

// 计算图像的均值和标准差
void calcMeanStdDev(unsigned char *_img_data, double &_mean, double &_std_dev)
{
    cv::Mat img(IMG_HEIGHT, IMG_WIDTH, CV_8UC1, _img_data);
    calcMeanStdDev(img, _mean, _std_dev);
}

// ========================================================
// class CAlgoInvoker

//
CvPoint3D32f g_SaturationCenterL;   // 左眼，最后一次瞳孔识别时得到的瞳孔中心坐标和瞳孔半径
CvPoint3D32f g_SaturationCenterR;   // 右眼，最后一次瞳孔识别时得到的瞳孔中心坐标和瞳孔半径

QElapsedTimer g_elapsedDetectOnce;

int g_lastDetectTime = 0;

int g_PupilState = 0;        // 瞳孔识别状态。0-正常，1-瞳孔过小，2-无法识别

//
CAlgoInvoker::CAlgoInvoker(QObject *parent) : QObject(parent)
{
    //
    qRegisterMetaType<stVisionValue>("stVisionValue");
    qRegisterMetaType<stVisionAbnormal>("stVisionAbnormal");

    //
    //m_workThread = new QThread();
    //this->moveToThread(m_workThread);

    //
    m_algoIntf = CAlgoIntf::createInstance(cameraType_D3T_M3ST130M);

    m_algoIntf->setAlgoMode(algoMode_General);      // 设置【普通/专业】模式
    m_algoIntf->setMaxGazeDeviation(10);            // 设置【最大固视偏差（°）】
    m_algoIntf->setIsSaveImg(false);                //
    m_algoIntf->setIsHmMode(false);                 //
    m_algoIntf->setIsSingleThread(false);           //
    m_algoIntf->setIsSimulatedEye(false);           //
    m_algoIntf->setWHRatio(0.6, 0.75);              // 设置模拟眼和人眼瞳孔的长宽比阈值，作为瞳孔的过滤条件

}

bool CAlgoInvoker::detectPupilOfImg(const QString &_img_path, enAlgoVer _pupil_algo_ver, int _img_num, bool _is_calc_expo)
{
    // 检测指定图像的瞳孔
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

    //algoSaveImgIndex = -1;
    //setCurrentPupilAlgoVer(_pupil_algo_ver);

    stPupilInfo pupil_info_r, pupil_info_l;
    //memset(&pupil_info_l, 0, sizeof(stPupilInfo));
    //memset(&pupil_info_r, 0, sizeof(stPupilInfo));

    bool succ = detectPupil(img_data, _img_num, ageRange_4_20_100_YEAE, pupil_info_r, pupil_info_l, false, singleDualEyeMode_Right);

    QString str_info = QString::asprintf("CAlgoInvoker::detectPupilOfImg(): num: %d, right: (%.0f, %.0f), %.0f; left: (%.0f, %.0f), %.0f", _img_num,
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

            const int SPACE = 0;

            cv::Mat mat_rgb = cv::cvarrToMat(img_rgb);
            CvPoint center_r = cvPointFrom32f(CPointF_to_CvPoint2D32f(pupil_info_r.center));
            CvPoint center_l = cvPointFrom32f(CPointF_to_CvPoint2D32f(pupil_info_l.center));
            cv::circle(mat_rgb, center_r, 1, CV_RGB(0, 255, 0), 1);
            cv::circle(mat_rgb, center_r, pupil_info_r.radius + SPACE, CV_RGB(0, 255, 0), 1);
            cv::circle(mat_rgb, center_l, 1, CV_RGB(0, 255, 0), 1);
            cv::circle(mat_rgb, center_l, pupil_info_l.radius + SPACE, CV_RGB(0, 255, 0), 1);

            cv::imshow(QString("img_%1").arg(_img_num).toLocal8Bit().data(), mat_rgb);

            //cvShowImage("img", img_rgb);
            //cvWaitKey();
        }
    }
#endif

    //
    if (_is_calc_expo && succ) {
        int avg = -1;
        bool is_over = false;
        bool is_succ_calc_expo = calcExposure(img_data, _img_num, pupil_info_r, pupil_info_l, avg, is_over, singleDualEyeMode_Both);
        if (is_succ_calc_expo) {
            qDebug() << QString::asprintf("avg = %d, is_over = %d", avg, is_over);
        }
    }

    //
    cvReleaseImage(&img);

    //
    if (!succ) {
        //logDebug();
        return false;
    } else {
        return true;
    }
}

void CAlgoInvoker::detectPupilOfImgsOfDir(const QString &_dir_path, enAlgoVer _pupil_algo_ver, int &_count_succ, int &_count_fail)
{
    _count_succ = 0;
    _count_fail = 0;

    // 检测指定文件夹中所有图像的瞳孔
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
            qDebug() << "CAlgoInvoker::detectPupilOfImgsOfDir(): failed, path = " << file_list.value(num);
            _count_fail++;
        }
    }
}

bool CAlgoInvoker::calcVisionOfImgs(QString _img_dir, enAlgoVer _pupil_algo_ver, enAlgoVer _calc_algo_ver, bool _is_single_thread,stVisionValue &vision,stVisionAbnormal &vision_abnormal)
{
    // 计算指定目录的图集的屈光度
    Util::CScreenerImgsData imgs_data;
    imgs_data.imgsDir = _img_dir;
    imgs_data.isLoadPreImg = false;
    imgs_data.fileType = Util::imgFileType_Bmp;
    imgs_data.isNeedZeroth = false;
    imgs_data.useSimulateImage = false;
    imgs_data.setImgSize(IMG_WIDTH, IMG_HEIGHT);

    if (imgs_data.loadImgFiles(true)) {
        std::vector<uchar*> resultByte;
        resultByte.clear();
        for (int i =0; i < 23; i++) {
            uchar* img_data = imgs_data.getImage(i);
            resultByte.push_back(img_data);
        }

        calcSucceeded = false;

        enCalcResultState calc_state = calcResultState_Fail;

        //setCurrentPupilAlgoVer(_pupil_algo_ver);
        g_SaturationCenterR.z = 0;
        slotDetectPupil(resultByte.at(1), 1, ageRange_4_20_100_YEAE, false);

        if (Util::compDouble(g_SaturationCenterR.z, 5) > 0) {
            //setCurrentPupilAlgoVer(_calc_algo_ver);
            //setCurrentPupilAlgoVer(_pupil_algo_ver);
            m_algoIntf->setIsSingleThread(_is_single_thread);
            calc_state = calcVision(resultByte, singleDualEyeMode_Right, "debug_xxx",vision,vision_abnormal);
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

bool CAlgoInvoker::calcAverageGrey(const QString &_dir_path)
{
    // open output file     // TODO: 把灰度值写入数据库更方便统计分析？
    QFile file(_dir_path + QDir::separator() + "AverageGrey.csv");
    file.open(QFile::WriteOnly | QFile::Truncate);
    QTextStream stream(&file);

    stream << "MeasureTime,CreationTime,Number,AverageGrey\r\n";

    // 遍历所有存图文件夹
    QString dir_path = _dir_path + QDir::separator() + "media" + QDir::separator() + "photo";
    QDir dir(dir_path);
    dir.setPath(dir_path);
    QFileInfoList dir_info_list = dir.entryInfoList(QDir::Dirs);
    QFileInfoList file_info_list;
    QString dir_name = "";
    QString dir_sub_path = "";
    QDir dir_sub;
    QString measure_time = "";
    QString create_time = "";
    //int record_size;
    QString file_name = "";
    QString file_path = "";
    float average_grey = 0;

    for (QFileInfo dir_info : dir_info_list) {
        if (!dir_info.isDir())
            continue;

        dir_name = dir_info.fileName();
        if (dir_name == "." || dir_name == "..")
            continue;

        // 得到测量时间
        measure_time = "";      // TODO: ？？？
        create_time = "";

        // 选择图像文件
        file_name = "";
        dir_sub_path = dir_path + QDir::separator() + dir_name;
        dir_sub.setPath(dir_sub_path);
        file_info_list = dir_sub.entryInfoList(QDir::Files);
        foreach (QFileInfo file_info, file_info_list) {
            if (file_info.fileName().endsWith(".bmp")) {
                file_name = file_info.fileName();
                break;
            }
        }
        if (file_name.length() == 0)
            continue;

        // 计算平均灰度
        file_path = dir_sub_path + QDir::separator() + file_name;
        cv::Mat img = cv::imread(file_path.toStdString(), cv::IMREAD_GRAYSCALE);
        average_grey = Util::getAverageGrayRough(img);

        //
        stream << measure_time << "," << create_time << "," << dir_name << "," << QString::number(average_grey, 'g', 2) << "\r\n";
    }

    //
    stream.flush();
    file.flush();
    file.close();

    //
    return true;
}

bool CAlgoInvoker::calcContrast(const QString &_file_path, double &_mean, double &_std_dev)
{
    if (_file_path.length() > 0) {
        cv::Mat mat = cv::imread(_file_path.toLocal8Bit().data(), cv::IMREAD_GRAYSCALE);

        calcMeanStdDev(mat, _mean, _std_dev);

        return true;
    } else {
        return false;
    }
}

void CAlgoInvoker::slotDetectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, bool _is_need_calc_expo)
{
    qDebug() << "->->->->->-> Begin pupil detection ......";

    //
    enSingleDualEyeMode single_dual_eye =  singleDualEyeMode_Both;

    //
    stPupilInfo pupil_info_r, pupil_info_l;
    //memset(&pupil_info_r, 0, sizeof(stPupilInfo));
    //memset(&pupil_info_l, 0, sizeof(stPupilInfo));

    int avg = -1;
    bool over_expo = false;

    try {
        bool succ = detectPupil(_img_data, _img_idx, _age_range, pupil_info_r, pupil_info_l, false, single_dual_eye);
        if (succ) {
            if (_is_need_calc_expo) {
                int avg_in = -1;
                bool over_expo_in = false;
                bool is_calc_succ = calcExposure(_img_data, _img_idx, pupil_info_r, pupil_info_l, avg_in, over_expo_in, single_dual_eye);
                if (is_calc_succ) {
                    avg = avg_in;
                    over_expo = over_expo_in;
                }
            }
        } else {
            //static bool is_save_fail = false;
            //if (is_save_fail) {
            //    QString file_path = QString::asprintf("/root/debug/idxBuff_%.2d.png", _img_idx);
            //    Util::saveImgDataToImgFile2(_img_data, IMG_WIDTH, IMG_HEIGHT, file_path);
            //}
        }

        //
        //emit sigPupilDetectionResult(_img_data, _img_idx, succ, pupil_info_r, pupil_info_l, avg, over_expo);

    } catch (...) {
        //logCritical();

        //emit sigAlgoErr(algoErrType_DetectPupil, strerror(errno));

        //emit sigPupilDetectionResult(_img_data, _img_idx, false, pupil_info_r, pupil_info_l, avg, over_expo);
    }

    qDebug() << "<-<-<-<-<-<- End pupil detection ......";
}

void CAlgoInvoker::slotShowResult(bool _is_succ, stVisionValue _vision, stVisionAbnormal _value_unnormal)
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

bool CAlgoInvoker::detectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, bool _is_calc_vision, enSingleDualEyeMode _single_dual_eye)
{
    //
    g_elapsedDetectOnce.start();

    bool is_succ = m_algoIntf->detectPupil(_img_data, _img_idx, _age_range, _pupil_info_r, _pupil_info_l, _is_calc_vision, _single_dual_eye);

    g_lastDetectTime = g_elapsedDetectOnce.elapsed();

    g_PupilState = (is_succ ? 0 : 2);       // TODO: 瞳孔过小的判断？

    // 左、右眼是否需要计算
    bool is_need_right  = (singleDualEyeMode_Right & _single_dual_eye);
    bool is_need_left   = (singleDualEyeMode_Left & _single_dual_eye);

    //
    memset(&g_SaturationCenterR, 0, sizeof(CvPoint3D32f));
    memset(&g_SaturationCenterL, 0, sizeof(CvPoint3D32f));

    if (is_need_right) {
        g_SaturationCenterR.x = _pupil_info_r.center.x;
        g_SaturationCenterR.y = _pupil_info_r.center.y;
        g_SaturationCenterR.z = _pupil_info_r.radius;
    }
    if (is_need_left) {
        g_SaturationCenterL.x = _pupil_info_l.center.x;
        g_SaturationCenterL.y = _pupil_info_l.center.y;
        g_SaturationCenterL.z = _pupil_info_l.radius;
    }

    //
    return is_succ;
}

bool CAlgoInvoker::calcExposure(unsigned char *_img_data, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, int &_avg, bool &_over_expo, enSingleDualEyeMode _single_dual_eye)
{
    bool is_succ = m_algoIntf->calcExposure(_img_data, _img_idx, _pupil_info_r, _pupil_info_l, _avg, _over_expo, _single_dual_eye);

    //
    return is_succ;
}

enCalcResultState CAlgoInvoker::calcVision(std::vector<unsigned char *> &_img_list, enSingleDualEyeMode _single_dual_eye, QString _sub_dir_name,stVisionValue &vision,stVisionAbnormal &vision_abnormal)
{
    //
    enAgeRange age_range = enAgeRange::ageRange_4_20_100_YEAE;

    //
    if (_sub_dir_name.length() > 0) {
        QString path_img = QString("/media/photo/%1").arg(_sub_dir_name);
        QDir dir_img(path_img);
        if (dir_img.exists()) {
            QString cmd = QString("rm %1 -r").arg(path_img);
            system(cmd.toLatin1().data());
        }
    } else {
        qWarning() << "_sub_dir_name is empty !";

    }

    // 调用算法接口计算屈光
    enCalcResultState calc_state = m_algoIntf->calcVision(_img_list, age_range, _sub_dir_name.toStdString(), _single_dual_eye, vision, vision_abnormal);
    logDebug(QString("m_algoIntf->calcVision() -> %1").arg(calc_state));

    bool is_succ = (calcResultState_Succ == calc_state);
    if (is_succ) {
        // 将 12、18 图保存为预览图
        if (/*g_isSaveSampleImage*/false) {
            qDebug() << "saving image 12 & 18 ...";
            //testSaveByteImageinFolder_(_img_list[12], 12, _sub_dir_name, 3);
            //testSaveByteImageinFolder_(_img_list[18], 18, _sub_dir_name, 3);
        }
    }

    //
    //emit sigCalcVisionFinished(calc_state, vision, vision_abnormal);

    //
    return calc_state;
}

void CAlgoInvoker::test1(const QString &_file_path)
{
    IplImage *img = cvLoadImage(_file_path.toLocal8Bit().data(), CV_LOAD_IMAGE_GRAYSCALE);
    stPupilInfo pupil_info_r, pupil_info_l;
    //memset(&pupil_info_r, 0, sizeof(stPupilInfo));
    //memset(&pupil_info_l, 0, sizeof(stPupilInfo));
    int thresh_val, tag;

    bool succ = false;
    //succ = detectPupil_2(img, pupil_info_r, pupil_info_l, thresh_val, tag);
    //succ = detectPupil_3(img, pupil_info_r, pupil_info_l, thresh_val, tag);
    // TODO: 这是旧算法代码定义的

    qDebug() << succ;

}

void CAlgoInvoker::test2(const QString &_file_path)
{
    IplImage *img = cvLoadImage(_file_path.toLatin1().data(), CV_LOAD_IMAGE_GRAYSCALE);
    CvRect rect = cvRect(10, 10, 50, 50);
    cvSetImageROI(img, rect);
    CvRect rect_roi = cvGetImageROI(img);
    //cvSaveImage("/root/debug/test_roi.bmp", img);
    cvReleaseImage(&img);

}

void CAlgoInvoker::setHmode(bool flag)
{
    m_algoIntf->setIsHmMode(flag);
}

void CAlgoInvoker::test3()
{
    //    Mat image = cv::imread("/root/work/screener-unit-test/bin/satur_204551514_ae-10973_d-944_g-0_p-0_expo-N.bmp");
    //    //Mat image = cv::imread("/root/debug/img_gauss.bmp");
    //    //imshow("SoureImage",image);
    //    cvtColor(image,image,CV_RGB2GRAY);
    //    Mat imageOutput;
    //    Mat imageOtsu;
    //    int thresholdValue=OtsuAlgThreshold(image);
    //    qDebug()<<"类间方差为： "<<thresholdValue<<endl;
    //    cv::threshold(image,imageOutput,thresholdValue,255,CV_THRESH_BINARY);
    //    cv::threshold(image,imageOtsu,0,255,CV_THRESH_OTSU); //Opencv Otsu算法

    //    cv::imwrite("/root/debug/OutputImage.bmp", imageOutput);
    //    cv::imwrite("/root/debug/OpencvOtsu.bmp", imageOtsu);

    //
    cv::Mat mat_11 = cv::imread("/mnt/hgfs/VMWareShare/screener_docs/Test/高亮度测试/11.png");
    double mean_11, std_dev_11;
    calcMeanStdDev(mat_11, mean_11, std_dev_11);

    cv::Mat mat_12 = cv::imread("/mnt/hgfs/VMWareShare/screener_docs/Test/高亮度测试/12.png");
    double mean_12, std_dev_12;
    calcMeanStdDev(mat_12, mean_12, std_dev_12);

    cv::Mat mat_21 = cv::imread("/mnt/hgfs/VMWareShare/screener_docs/Test/高亮度测试/21.png");
    double mean_21, std_dev_21;
    calcMeanStdDev(mat_21, mean_21, std_dev_21);

    cv::Mat mat_22 = cv::imread("/mnt/hgfs/VMWareShare/screener_docs/Test/高亮度测试/22.png");
    double mean_22, std_dev_22;
    calcMeanStdDev(mat_22, mean_22, std_dev_22);

    qDebug() << "mean_11: " << mean_11 << ", mean_12: " << mean_12 << ", mean_21: " << mean_21 << ", mean_22: " << mean_22 << "\r\n";
}
