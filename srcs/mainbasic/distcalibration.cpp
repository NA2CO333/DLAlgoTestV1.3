#include "distcalibration.h"

#include <QFile>
#include <QTextStream>

#include <iostream>

#include "util-common.h"
#include "global.h"

//
CDistCalibration::CDistCalibration(QObject *parent) : QObject(parent)
{
    reset();
}

// 重置
void CDistCalibration::reset()
{
    dataList.clear();
    distOfMaxClarity = -1;
    isStarted = false;
}

// 添加距离值及其对应的清晰度值
void CDistCalibration::putDistance(int _dist, float _clarity)
{
    //
    if (!isStarted)
        return;

    //
    stDistClarity dist_clarity = {_clarity, _dist};
    dataList.append(dist_clarity);

}

// 由采样数据集找出清晰度最高时的距离值
void CDistCalibration::calcDistOfMaxClarity()
{
    int data_size = dataList.count();
    if (data_size < COUNT_MIN)
        return;

    //
    const float COUNT_PEAK_RATIO = 0.05;        // “波峰样本”个数比例（相对于总样本数）
    const float COUNT_PEAK_MIN = 1;             // 最小波峰样本个数

    //std::cout << std::endl;

    // 移除前列过多的样本数据
    if (data_size > COUNT_MAX) {
        dataList.remove(0, data_size - COUNT_MAX);
        data_size = dataList.count();
    }

    // 找出最大的若干个值（“波峰样本”）
    int len_bigger = round(data_size * COUNT_PEAK_RATIO);
    if (len_bigger < COUNT_PEAK_MIN)
        len_bigger = COUNT_PEAK_MIN;

    //std::cout << "copy to data_bigger[]" << std::endl;
    stDistClarity data_bigger[len_bigger + 1];      /* 数组长度冗余一个元素防止下标越界。 */
    memset(data_bigger, 0, sizeof(stDistClarity) * (len_bigger + 1));
    for (int i = 0; i < len_bigger; i++) {      // “波峰样本”初始化：从样本集前列逐个拷贝
        data_bigger[i] = dataList[i];
        //std::cout << "data_bigger[" << i << "].clarity = " << data_bigger[i].clarity << ", .dist = " << data_bigger[i].dist << std::endl;
    }

    //std::cout << "sort" << std::endl;
    stDistClarity temp;
    for (int num_bubbled = 0; num_bubbled < len_bigger - 1; num_bubbled++) {    // 将“波峰样本”降序排序（冒泡法）
        for (int i = 0; i < len_bigger - num_bubbled - 1; i++) {
            assert(i >= 0 && i + 1 < len_bigger);

            if (Util::compDouble(data_bigger[i].clarity, data_bigger[i + 1].clarity) < 0) {
                temp = data_bigger[i];
                data_bigger[i] = data_bigger[i + 1];
                data_bigger[i + 1] = temp;
            }
        }
    }
    //for (int i = 0; i < len_bigger; i++) {
    //    std::cout << "data_bigger[" << i << "].clarity = " << data_bigger[i].clarity << ", .dist = " << data_bigger[i].dist << std::endl;
    //}

    if (data_size > len_bigger) {
        stDistClarity picked;
        for (int i = data_size - 1; i >= len_bigger; i--) {      // 将“样本集”内未被拷贝的元素逐个捡出，与“波峰样本”比较
            picked = dataList[i];

            // 在“波峰样本”中从小到大比较，找到第一个大于被比较数的元素的位置
            int pos = -1;
            for (int i = len_bigger - 1; i >= 0; i--) {
                if (data_bigger[i].clarity > picked.clarity) {
                    pos = i;
                    break;
                }
            }

            // 将被比较数插入到“波峰样本”中
            //std::cout << "pos = " << pos << std::endl;
            if (pos < len_bigger - 1)
            {
                for (int k = len_bigger - 1; k > pos + 1; k--) {        // 逐个后移一位，最后一位移除
                    data_bigger[k] = data_bigger[k - 1];
                }
                //std::cout << "copy " << "dataList[" << i << "] " << picked.dist << " to data_bigger[" << pos + 1 << "] " << data_bigger[pos + 1].dist << std::endl;
                data_bigger[pos + 1] = picked;
            }
        }
    }

    // TODO: 剔除异常值？


    // 求平均值
    //std::cout << "calc average" << std::endl;
    int total = 0;
    for (int i = 0; i < len_bigger; i++) {
        total += data_bigger[i].dist;
        //std::cout << "data_bigger[" << i << "].clarity = " << data_bigger[i].clarity << ", .dist = " << data_bigger[i].dist << std::endl;
    }
    int ave = total / len_bigger;
    //std::cout << "ave = " << ave << std::endl;

    //
    distOfMaxClarity = ave;
}

int CDistCalibration::getCount()
{
    return dataList.count();
}

// 获取最清晰时的距离值（若返回 -1 则表示失败）
int CDistCalibration::getDistOfMaxClarity(QString *_msg)
{
    if (dataList.count() < COUNT_MIN) {
        if (_msg)
            *_msg = QString("采样数至少要 %1 个！").arg(COUNT_MIN);
        return -1;
    }
    return round(distOfMaxClarity);
}

bool CDistCalibration::getIsStarted()
{
    return isStarted;
}

bool CDistCalibration::saveDataToFile(QString _file_path, QString *_msg)
{
    QString msg = (_msg ? *_msg : QString(""));
    QFile file(_file_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream streem(&file);
        for (int i = 0; i < dataList.count(); i++) {
            streem << QString::asprintf("%.2f, %d\r\n", dataList[i].clarity, dataList[i].dist);
        }
        streem.flush();
        file.flush();
        file.close();
        return true;
    } else {
        msg = "failed to open file!";
        return false;
    }
}

void CDistCalibration::setIsStarted(bool _is_started)
{
    bool need_calc = (isStarted && !_is_started);

    isStarted = _is_started;

    if (need_calc) {
        calcDistOfMaxClarity();
    }
}
