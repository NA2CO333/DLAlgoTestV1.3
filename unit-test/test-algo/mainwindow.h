#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>

namespace Ui {
class MainWindow;
}

//
class CAlgoInvoker;

//
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void slotAboutToExit();

protected:
    void showEvent(QShowEvent *);

    void saveEdtVal(QLineEdit *_edt);
    void loadEdtVal(QLineEdit *_edt);

    void saveCbbVal(QComboBox *_cbb);
    void loadCbbVal(QComboBox *_cbb);

    void saveCkbVal(QCheckBox *_ckb);
    void loadCkbVal(QCheckBox *_ckb);

    void saveRdbVal(QRadioButton *_rdb);
    void loadRdbVal(QRadioButton *_rdb);

    CAlgoInvoker *m_algoInvoker {nullptr};

private slots:
    void on_btnClose_clicked();
    void on_btnSelFile_clicked();
    void on_btnSelDir_clicked();
    void on_btnDetectPupil_clicked();
    void on_btnDetectPupilAll_clicked();
    void on_rbtnCalcNew_clicked();
    void on_rbtnCalcOld_clicked();
    void on_btnCalcVision_clicked();
    void on_btnCalcAverageGrey_clicked();
    void on_btnCalcImgGrey_clicked();
    void on_btnCalcContrast_clicked();
    void on_btnFilterLog_clicked();
    void on_btnTest2_clicked();
    void on_btnTest3_clicked();
    void on_btnCalAll_clicked();

    void on_chkHmode_stateChanged(int arg1);

    void on_streamCalcBtn_clicked();

    void on_refStrategyButton_clicked();

    void on_stableTestBtn_clicked();

    void on_calibBtn_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
