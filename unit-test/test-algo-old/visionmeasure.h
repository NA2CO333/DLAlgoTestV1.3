#ifndef VISIONMEASURE_H
#define VISIONMEASURE_H

#include <QObject>
#include <QThread>

#include "algoobj.h"

//
class CVisionMeasure : public QObject
{
    Q_OBJECT

private:
    CAlgoObj mAlgo;
    QThread mAlgoThread;
    bool calcSucceeded;

public:
    explicit CVisionMeasure(QObject *parent = 0);

    bool detectPupilOfImg(const QString &_img_path, enAlgoVerAll _pupil_algo_ver, int _img_num, bool _is_calc_expo);
    void detectPupilOfImgsOfDir(const QString &_dir_path, enAlgoVerAll _pupil_algo_ver, int &_count_succ, int &_count_fail);
    bool calcVisionOfImgs(QString _img_dir, enAlgoVerAll _pupil_algo_ver, enAlgoVerAll _calc_algo_ver, bool _is_single_thread);
    bool ledPosiDetect(QString _img_dir, enAlgoVerAll _detect_algo_ver, enAlgoVerAll _calc_algo_ver);
    bool test_Runtask_processPic0(const QString &_img_path, int _img_num, enAlgoVerAll _pupil_algo_ver);

signals:

public slots:
    void slotShowResult(bool _is_succ, stVisionValue _vision, stVisionAbnormal _value_unnormal);

protected:

};

#endif // VISIONMEASURE_H
