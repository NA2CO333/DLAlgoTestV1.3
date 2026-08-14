#include "algorithm.h"



#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"


Algorithm::Algorithm()
{

}

bool Algorithm::detectPupilRoughly(IplImage *_img, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l)
{
#define REDUCE_DIMENSION_RATIO  4       // 降维比例

    // 图像降维
    int sw = _img->width / REDUCE_DIMENSION_RATIO; // 752/4 = 188
    int sh = _img->height / REDUCE_DIMENSION_RATIO; // 480/4 = 120

    IplImage *img_resized = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);
    cvResize(_img, img_resized, CV_INTER_LINEAR);

#ifdef UNIT_TEST
    cvSaveImage("/root/debug/img_resized.bmp", img_resized);
#endif

    cv::Scalar mean_value = cv::mean(cv::Mat(img_resized));

    int avePixel = mean_value[0];
    if(avePixel < 15)
        SimulatedEyeMode = true;
    else
        SimulatedEyeMode = false;

    // 高斯滤波处理
    IplImage *img_gauss = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);
    cv::GaussianBlur(Mat(img_resized), Mat(img_gauss), cv::Size(5, 5), 0, 0);     /* 如果不模糊处理，二值化得到的轮廓很乱，很多杂点。 */
    //cvCopy(img_resized, img_gauss);

#ifdef UNIT_TEST
    cvSaveImage("/root/debug/img_gauss.bmp", img_gauss);
#endif

    // 计算ostu大津算法的二值化阈值
    const int _PUPIL_RECT_WIDTH = 17;   // 17*4*120/752=10.85，一般人瞳孔直径小于此值

    int _THRES_VALUES[17] = {0, -3, 3, -6, 6, -9, 9, -12, 12, -15, 15, -18, 18, -50, 50, -80, 80};
    int _LEN_THRES_VALS = sizeof(_THRES_VALUES) / sizeof(_THRES_VALUES[0]);

    int thresh_otsu;
    bool thresh_otsu_got = false;


    IplImage *destbwImg = cvCreateImage(cvSize(sw, sh), IPL_DEPTH_8U, 1);
    IplImage *sub_img = cvCreateImage(cvSize(_PUPIL_RECT_WIDTH, _PUPIL_RECT_WIDTH), IPL_DEPTH_8U, 1);
    vector<CvSeq *> vec_contours;
    vector<stPupilInfo> vec_pupil_info;
    bool pupil_detected = false;
    for (int i = 0; i < _LEN_THRES_VALS; i++) {
        CvMemStorage *mem_storage = cvCreateMemStorage(0);
        do {
            // 二值化图像
            if (!thresh_otsu_got) {
                thresh_otsu = cvThreshold(img_gauss, destbwImg, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
                thresh_otsu_got = true;
            } else {
                int thresh = thresh_otsu + _THRES_VALUES[i];
                if (thresh < 0 || thresh > 255)
                    continue;

                cvThreshold(img_gauss, destbwImg, thresh, 255, CV_THRESH_BINARY);
            }

#ifdef UNIT_TEST
            cvSaveImage("/root/debug/destbwImg_thresh.bmp", destbwImg);
#endif

            // 轮廓查找
            CvSeq *contours = NULL;
            int ConNum = cvFindContours(destbwImg, mem_storage, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));

            if(ConNum < 2)
                continue;

            // 根据连通域面积的大小、包围框和连通域的面积比的大小、包围框的宽高比的大小来过滤连通域
            //const int   _MIN_AREA = ageStage <= 1 ? CGlobal::minPupilAreaRD : 14;   // 连通域最小面积
            const int   _MIN_AREA = 14;         // 连通域最小面积
            const int   _MAX_AREA = 250;
            const float _MIN_WH_RATIO = 0.6;    // 包围框的宽高比的最小值
            const float _MAX_WH_RATIO = 1.5;
            const float _MAX_AREA_RATIO = 1.6;  // 包围框和连通域的面积比的最大值

            vec_contours.clear();
            vec_pupil_info.clear();
            for (; contours; contours = contours->h_next)
            {
                double area_contour = fabs(cvContourArea(contours, CV_WHOLE_SEQ)); // 获取当前轮廓面积
                if (area_contour < _MIN_AREA || area_contour >= _MAX_AREA)
                    continue;

                CvRect rect_contour = cvBoundingRect(contours);     // 返回二维点集的最外面(up-right)矩形边界

                float wh_ratio = (float)rect_contour.width / rect_contour.height;
                if (wh_ratio < _MIN_WH_RATIO || wh_ratio > _MAX_WH_RATIO)
                    continue;

                float area_ratio = ((float)(rect_contour.width - 1) * (rect_contour.height - 1) + 1) / area_contour;
                if (area_ratio > _MAX_AREA_RATIO)
                    continue;

                // 计算连通域的质心
                CvMoments m;
                cvMoments(contours, &m);
                CvPoint centroid;
                centroid.x = (int)(m.m10 / m.m00);
                centroid.y = (int)(m.m01 / m.m00);

                // 检查质心是否与图像边界有一定距离，若小于这个距离，则无效
                const int _MIN_BOUNDARY_DIST = _PUPIL_RECT_WIDTH / 2 + 4;

                if (centroid.x < _MIN_BOUNDARY_DIST || centroid.x > sw - _MIN_BOUNDARY_DIST ||
                        centroid.y < _MIN_BOUNDARY_DIST || centroid.y > sh - _MIN_BOUNDARY_DIST)
                    continue;

                // 计算以质心为中心的一定尺寸矩形区域的灰度标准差，并作为连通域的过滤条件
                cv::Rect rect_pupil = cvRect(centroid.x - _PUPIL_RECT_WIDTH / 2, centroid.y - _PUPIL_RECT_WIDTH / 2, _PUPIL_RECT_WIDTH, _PUPIL_RECT_WIDTH);
                cvSetImageROI(img_resized, rect_pupil);
                cvCopy(img_resized, sub_img);
                cvResetImageROI(img_resized);

                Mat mat_mean, mat_stddev;
                cv::meanStdDev(Mat(sub_img), mat_mean, mat_stddev);
                double std_dev = mat_stddev.ptr<double>(0)[0];

                if (std_dev <= 7)  // TODO： 以标准偏差作为判断条件貌似不合理？
                    continue;

                //
                stPupilInfo pupil_info;
                pupil_info.area = area_contour;
                pupil_info.rect = rect_contour;
                vec_contours.push_back(contours);
                vec_pupil_info.push_back(pupil_info);
            }

            if (vec_contours.size() == 2)
            {
                CvPoint2D32f center1, center2;
                float radius1, radius2;

                cvMinEnclosingCircle(vec_contours.at(0), &center1, &radius1);
                cvMinEnclosingCircle(vec_contours.at(1), &center2, &radius2);

                int edgeTH = 3;
                if (center1.x > edgeTH && center1.x < (sw - edgeTH) && center2.x > edgeTH && center2.x < (sw - edgeTH)
                        && center1.y > edgeTH && center1.y < (sh - edgeTH) && center2.y > edgeTH && center2.y < (sh - edgeTH))
                {
                    if (abs(center2.x - center1.x) > sw / 5 && abs(center2.y - center1.y) < sh / 3)
                    {
                        vec_pupil_info.at(0).center = center1;
                        vec_pupil_info.at(0).radius = radius1;
                        vec_pupil_info.at(1).center = center2;
                        vec_pupil_info.at(1).radius = radius2;

                        pupil_detected = true;
                    }
                }
            }
            else
            {

            }
        } while (false);

        // 释放内存
        cvClearMemStorage(mem_storage);
        cvReleaseMemStorage(&mem_storage);

        //
        if (pupil_detected) {
            _pupil_info_r = vec_pupil_info.at(0);
            _pupil_info_l = vec_pupil_info.at(1);

            if (_pupil_info_l.center.x < _pupil_info_r.center.x) {
                stPupilInfo pupil_info_tmp = _pupil_info_r;
                _pupil_info_r = _pupil_info_l;
                _pupil_info_l = pupil_info_tmp;
            }

            _pupil_info_r.area *= (REDUCE_DIMENSION_RATIO * REDUCE_DIMENSION_RATIO);
            _pupil_info_r.center.x *= REDUCE_DIMENSION_RATIO;
            _pupil_info_r.center.y *= REDUCE_DIMENSION_RATIO;
            _pupil_info_r.radius *= REDUCE_DIMENSION_RATIO;
            _pupil_info_r.rect.x *= REDUCE_DIMENSION_RATIO;
            _pupil_info_r.rect.y *= REDUCE_DIMENSION_RATIO;
            _pupil_info_r.rect.width *= REDUCE_DIMENSION_RATIO;
            _pupil_info_r.rect.height *= REDUCE_DIMENSION_RATIO;

            _pupil_info_l.area *= (REDUCE_DIMENSION_RATIO * REDUCE_DIMENSION_RATIO);
            _pupil_info_l.center.x *= REDUCE_DIMENSION_RATIO;
            _pupil_info_l.center.y *= REDUCE_DIMENSION_RATIO;
            _pupil_info_l.radius *= REDUCE_DIMENSION_RATIO;
            _pupil_info_l.rect.x *= REDUCE_DIMENSION_RATIO;
            _pupil_info_l.rect.y *= REDUCE_DIMENSION_RATIO;
            _pupil_info_l.rect.width *= REDUCE_DIMENSION_RATIO;
            _pupil_info_l.rect.height *= REDUCE_DIMENSION_RATIO;

            //
            break;
        } else {

        }
    }

    //
    vec_contours.clear();
    vec_contours.swap(vec_contours);
    vec_pupil_info.clear();
    vec_pupil_info.swap(vec_pupil_info);

    cvReleaseImage(&sub_img);
    cvReleaseImage(&destbwImg);
    cvReleaseImage(&img_gauss);
    cvReleaseImage(&img_resized);

    //
    return pupil_detected;
}

bool Algorithm::detectPupilAccurately(IplImage *_img, stPupilInfo &_pupil_info)
{
#define EYE_IMG_WIDTH 80             // 瞳孔计算截图尺寸（80/(752/120)=12.77mm）

    CvRect rect_eye = cvRect(_pupil_info.center.x - EYE_IMG_WIDTH / 2, _pupil_info.center.y - EYE_IMG_WIDTH / 2, EYE_IMG_WIDTH, EYE_IMG_WIDTH);

    // 截取瞳孔位置一定区域的图像
    IplImage *img_eye = cvCreateImage(cvSize(EYE_IMG_WIDTH, EYE_IMG_WIDTH), IPL_DEPTH_8U, 1);
    cvSetImageROI(_img, rect_eye);
    cvCopy(_img, img_eye);
    cvResetImageROI(_img);

    // 高斯滤波处理
    IplImage *img_gauss = cvCreateImage(cvSize(EYE_IMG_WIDTH, EYE_IMG_WIDTH), IPL_DEPTH_8U, 1);
    cv::GaussianBlur(Mat(img_eye), Mat(img_gauss), cv::Size(5, 5), 0, 0);     /* 如果不模糊处理，二值化得到的轮廓很乱，很多杂点。 */
    //cvCopy(img_eye, img_gauss);

#ifdef UNIT_TEST
    cvSaveImage("/root/debug/img_gauss_accurate.bmp", img_gauss);
#endif

    // 以ostu大津算法二值化图像
    const int _PUPIL_RECT_WIDTH = 17 * REDUCE_DIMENSION_RATIO;   // 17*4*120/752=10.85，一般人瞳孔直径小于此值

    int _THRES_VALUES[17] = {0, -3, 3, -6, 6, -9, 9, -12, 12, -15, 15, -18, 18, -50, 50, -80, 80};
    int _LEN_THRES_VALS = sizeof(_THRES_VALUES) / sizeof(_THRES_VALUES[0]);

    int thresh_otsu;
    bool thresh_otsu_got = false;

    IplImage *destbwImg = cvCreateImage(cvSize(EYE_IMG_WIDTH, EYE_IMG_WIDTH), IPL_DEPTH_8U, 1);
    IplImage *sub_img = cvCreateImage(cvSize(_PUPIL_RECT_WIDTH, _PUPIL_RECT_WIDTH), IPL_DEPTH_8U, 1);
    vector<CvSeq *> vec_contours;
    bool pupil_detected = false;
    for (int i = 0; i < _LEN_THRES_VALS; i++) {
        CvMemStorage *mem_storage = cvCreateMemStorage(0);
        do {
            // 二值化图像
            if (!thresh_otsu_got) {
                thresh_otsu = cvThreshold(img_gauss, destbwImg, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
                thresh_otsu_got = true;
            } else {
                int thresh = thresh_otsu + _THRES_VALUES[i];
                if (thresh < 0 || thresh > 255)
                    continue;

                cvThreshold(img_gauss, destbwImg, thresh, 255, CV_THRESH_BINARY);
            }

#ifdef UNIT_TEST
            cvSaveImage("/root/debug/destbwImg_thresh_accurate.bmp", destbwImg);
#endif

            // 轮廓查找
            CvSeq *contours = NULL;
            int ConNum = cvFindContours(destbwImg, mem_storage, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE, cvPoint(0, 0));

            if(ConNum < 1)
                continue;

            // 根据连通域面积的大小、包围框和连通域的面积比的大小、包围框的宽高比的大小来过滤连通域
            const int _MIN_AREA = 14 * REDUCE_DIMENSION_RATIO * REDUCE_DIMENSION_RATIO;      // 连通域最小面积
            const int _MAX_AREA = 250 * REDUCE_DIMENSION_RATIO * REDUCE_DIMENSION_RATIO;
            const float _MIN_WH_RATIO = 0.6;        // 包围框的宽高比的最小值
            const float _MAX_WH_RATIO = 1.5;
            const float _MAX_AREA_RATIO = 1.6;      // 包围框和连通域的面积比的最大值

            vec_contours.clear();
            for (; contours; contours = contours->h_next)
            {
                double area_contour = fabs(cvContourArea(contours, CV_WHOLE_SEQ)); // 获取当前轮廓面积
                if (area_contour < _MIN_AREA || area_contour >= _MAX_AREA)
                    continue;

                CvRect rect_contour = cvBoundingRect(contours);     // 返回二维点集的最外面(up-right)矩形边界

                float wh_ratio = (float)rect_contour.width / rect_contour.height;
                if (wh_ratio < _MIN_WH_RATIO || wh_ratio > _MAX_WH_RATIO)
                    continue;

                float area_ratio = ((float)(rect_contour.width - 1) * (rect_contour.height - 1) + 1) / area_contour;
                if (area_ratio > _MAX_AREA_RATIO)
                    continue;

                // 计算连通域的质心
                CvMoments m;
                cvMoments(contours, &m);
                CvPoint centroid;
                centroid.x = (int)(m.m10 / m.m00);
                centroid.y = (int)(m.m01 / m.m00);

                // 检查质心是否与图像边界有一定距离，若小于这个距离，则无效
                const int _MIN_BOUNDARY_DIST = _PUPIL_RECT_WIDTH / 2 + 4 * REDUCE_DIMENSION_RATIO;

                if (centroid.x < _MIN_BOUNDARY_DIST || centroid.x > EYE_IMG_WIDTH - _MIN_BOUNDARY_DIST ||
                        centroid.y < _MIN_BOUNDARY_DIST || centroid.y > EYE_IMG_WIDTH - _MIN_BOUNDARY_DIST)
                    continue;

                // 计算以连通域质心为中心的一定尺寸矩形区域的灰度标准差，并作为连通域的过滤条件
                cv::Rect rect_pupil = cvRect(centroid.x - _PUPIL_RECT_WIDTH / 2, centroid.y - _PUPIL_RECT_WIDTH / 2, _PUPIL_RECT_WIDTH, _PUPIL_RECT_WIDTH);
                cvSetImageROI(img_gauss, rect_pupil);
                cvCopy(img_gauss, sub_img);
                cvResetImageROI(img_gauss);

                Mat mat_mean, mat_stddev;
                cv::meanStdDev(Mat(sub_img), mat_mean, mat_stddev);
                double std_dev = mat_stddev.ptr<double>(0)[0];

                if (std_dev <= 7)  // TODO： 以标准偏差作为判断条件貌似不合理？
                    continue;

                //
                _pupil_info.area = area_contour;
                _pupil_info.rect = rect_contour;
                vec_contours.push_back(contours);
            }

            if (vec_contours.size() == 1)
            {
                CvPoint2D32f center1;
                float radius1;

                cvMinEnclosingCircle(vec_contours.at(0), &center1, &radius1);

                _pupil_info.center = center1;
                _pupil_info.radius = radius1;

                pupil_detected = true;
            }
            else
            {

            }

            // 释放内存
            cvClearMemStorage(mem_storage);
            cvReleaseMemStorage(&mem_storage);
        } while (false);

        //
        if (pupil_detected) {
            _pupil_info.center.x += rect_eye.x;
            _pupil_info.center.y += rect_eye.y;
            _pupil_info.rect.x += rect_eye.x;
            _pupil_info.rect.y += rect_eye.y;

            //
            break;
        } else {

        }
    }

    //
    vec_contours.clear();
    vec_contours.swap(vec_contours);

    cvReleaseImage(&sub_img);
    cvReleaseImage(&destbwImg);
    cvReleaseImage(&img_gauss);
    cvReleaseImage(&img_eye);

    //
    return pupil_detected;
}

