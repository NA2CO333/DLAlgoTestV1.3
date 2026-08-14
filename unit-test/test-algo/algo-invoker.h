#ifndef ALGO_INVOKER_H
#define ALGO_INVOKER_H

#include <QObject>
#include <QThread>

#include "algointf.h"

// （测试用的）算法调用的封装
class CAlgoInvoker : public QObject
{
    Q_OBJECT

public:
    explicit CAlgoInvoker(QObject *parent = 0);

    bool detectPupilOfImg(const QString &_img_path, enAlgoVer _pupil_algo_ver, int _img_num, bool _is_calc_expo);           // 检测指定图像的瞳孔
    void detectPupilOfImgsOfDir(const QString &_dir_path, enAlgoVer _pupil_algo_ver, int &_count_succ, int &_count_fail);   // 检测指定文件夹中所有图像的瞳孔
    bool calcVisionOfImgs(QString _img_dir, enAlgoVer _pupil_algo_ver, enAlgoVer _calc_algo_ver, bool _is_single_thread,stVisionValue &vision,stVisionAbnormal &vision_abnormal);   // 计算指定目录的图集的屈光度

    bool calcAverageGrey(const QString &_dir_path);                                     // 计算平均灰度
    bool calcContrast(const QString &_file_path, double &_mean, double &_std_dev);      // 计算对比度

    void test1(const QString &_file_path);
    void test2(const QString &_file_path);
    void test3();

    void setHmode(bool flag);

    CAlgoIntf *m_algoIntf {nullptr};
signals:

public slots:
    void slotDetectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, bool _is_need_calc_expo);
    void slotShowResult(bool _is_succ, stVisionValue _vision, stVisionAbnormal _value_unnormal);

protected:
    // 检测瞳孔（算法4：二值化采用了自适应阈值，优化了结构）
    bool detectPupil(unsigned char *_img_data, int _img_idx, enAgeRange _age_range, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l,
                              bool _is_calc_vision, enSingleDualEyeMode _single_dual_eye);
    // 计算图像的曝光量信息，返回是否计算成功
    bool calcExposure(unsigned char *_img_data, int _img_idx, stPupilInfo &_pupil_info_r, stPupilInfo &_pupil_info_l, int &_avg, bool &_over_expo,
                             enSingleDualEyeMode _single_dual_eye);
    // 计算屈光度
    enCalcResultState calcVision(std::vector<unsigned char *> &_img_list, enSingleDualEyeMode _single_dual_eye,
                                 QString _img_dir_name,stVisionValue &vision,stVisionAbnormal &vision_abnormal);

    bool calcSucceeded;

};

#endif // ALGO_INVOKER_H
