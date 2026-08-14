#ifndef EYESIGHTSTANDARD_H
#define EYESIGHTSTANDARD_H

#include <QSettings>
#include <QDebug>

#include "baseform.h"
#include "myeditline.h"
#include "statusbarform.h"
#include "mysqlitepatients.h"

namespace Ui {
class eyesightstandard;
}

// 视力判断结果。false 未超标，true 超标。
struct stVisionJudgementRst {
    bool anisometropia = false;         // 屈光参差
    bool astigmatism = false;           // 散光
    bool myopia = false;                // 近视
    bool hyperopia = false;             // 远视
    bool unequalInPupilSize = false;    // 瞳孔大小不等
    bool verticalGaze = false;          // 垂直凝视
    bool nasalGaze = false;             // 鼻侧凝视
    bool bitemporalGaze = false;        // 颞侧凝视
    bool gazeAsymmetry = false;         // 凝视不对称

    bool dataAbnormal = false;          // 数据异常

    void clear() {
        anisometropia = false;
        astigmatism = false;
        myopia = false;
        hyperopia = false;
        unequalInPupilSize = false;
        verticalGaze = false;
        nasalGaze = false;
        bitemporalGaze = false;
        gazeAsymmetry = false;

        dataAbnormal = false;
    }

    bool isNormal();

};

class eyesightstandard : public CBaseWidget
{
    Q_OBJECT

public:
    explicit eyesightstandard(QWidget *parent = 0);
    ~eyesightstandard();
    void getStandardValue();
    void Save_prompt_dialog();
    void setLineEditCheck(bool check);
    void setEditLineValue();
    void setDefaultParams();
    bool Correctness_of_judgment();

    // 将传入的检查结果和视力标准做对比，如果只想对比单眼，可传入另一眼 _result_? 参数为 null
    static void standardCompare(const CPatient &_patient, stVisionJudgementRst * _result_right, stVisionJudgementRst * _result_left);

private slots:
    void on_pushButton_Back_clicked();
    void on_pushButton_Home_clicked();
    void on_pushButton_Save_clicked();
    void on_pushButton_Recover_clicked();

protected:
    void showEvent(QShowEvent *) override;

private:
    Ui::eyesightstandard *ui;

    QStringList paraList;   //用来保存本地配置文件散光,近视,远视
};

#endif // EYESIGHTSTANDARD_H
