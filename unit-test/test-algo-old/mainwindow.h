#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>

#if APP_VER > 10309
#  include "appsetting.h"
#endif

namespace Ui {
class MainWindow;
}

//
class CVisionMeasure;

//
class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    CVisionMeasure *mVisionMeasure;

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void slotAboutToExit();

private slots:
    void on_btnSelPath_clicked();
    void on_btnDetectPupil_clicked();
    void on_btnSelDir_clicked();
    void on_btnCalcVision_clicked();
    void on_btnTest_clicked();
    void on_btnCalcAverageGrey_clicked();
    void on_btnLedPosiDetect_clicked();
    void on_btnCalcImgGrey_clicked();
    void on_btnCalcContrast_clicked();
    void on_btnSetContrast_clicked();
    void on_btnCalcDistDataPeak_clicked();
    void on_btnClose_clicked();
    void on_btnTest2_clicked();
    void on_btnFilterLog_clicked();
    void on_btnTest3_clicked();
    void on_btnDetectPupilAll_clicked();
    void on_rbtnCalcNew_clicked();
    void on_rbtnCalcOld_clicked();

    void on_btnTest_processPic0_clicked();

    void on_btnCalcVisionNew_clicked();

protected:
    void showEvent(QShowEvent *);

    appSetting *setting = Q_NULLPTR;

    void saveEdtVal(QLineEdit *_edt);
    void loadEdtVal(QLineEdit *_edt);

    void saveCbbVal(QComboBox *_cbb);
    void loadCbbVal(QComboBox *_cbb);

    void saveCkbVal(QCheckBox *_ckb);
    void loadCkbVal(QCheckBox *_ckb);

    void saveRdbVal(QRadioButton *_rdb);
    void loadRdbVal(QRadioButton *_rdb);

private:
    Ui::MainWindow *ui;

};

#endif // MAINWINDOW_H
