#ifndef CDISTCALIBRATION_H
#define CDISTCALIBRATION_H

#include <QObject>
#include <QVector>

// 距离和清晰度
struct stDistClarity {
    float clarity;
    int dist;
};

// 距离校准
class CDistCalibration : public QObject
{
    Q_OBJECT
public:
    explicit CDistCalibration(QObject *parent = 0);

    static const int interval = 200;

    void reset();
    void putDistance(int _dist, float _clarity);
    int getCount();
    int getDistOfMaxClarity(QString *_msg = Q_NULLPTR);
    bool getIsStarted();
    void setIsStarted(bool _is_started);
    bool saveDataToFile(QString _file_path, QString *_msg = Q_NULLPTR);

signals:

public slots:

private:
    const int COUNT_MIN = 100;              // 最小样本量
    const int COUNT_MAX = 1000;             // 最大样本量

    QVector<stDistClarity> dataList;
    float distOfMaxClarity = -1;            // 最大清晰度时的距离
    bool isStarted = false;                 // 是否已完成标定

    void calcDistOfMaxClarity();

};

#endif // CDISTCALIBRATION_H
