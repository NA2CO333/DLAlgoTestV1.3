#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QTimer>
#include <QLabel>

#include "baseform.h"
#include "initthread.h"
#include "statusbarform.h"
#include "themebackground.h"
#include "DataTransmit.h"

//
extern int batchhistory;        // 用于控制是进入历史纪录或批量筛查界面时是显示当前页还是显示最后一页？1,3：历史记录页面当前页和最后一页；2,4：批量筛查页面第一页和最后一页
// TODO: 不用这种方式，待优化

extern int beginreplace;
extern int endreplace;

//
namespace Ui {
class MainWindow;
}

//
class MainWindow : public CBaseWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void showLoading(bool);

private slots:
    void on_pushButton_6_12_clicked();
    void on_pushButton_12_36_clicked();
    void on_pushButton_3_6_clicked();
    void on_pushButton_6_20_clicked();
    void on_pushButton_20_100_clicked();

    void on_pushButton_historyrecord_clicked();
    void on_pushButton_tool_clicked();
    void on_pushButton_piliang_clicked();
    void barcodeHandle();

    void on_ckbIsDebugMode_clicked(bool checked);

public slots:
    void slotPhysicButtonPressed();         // 槽函数：物理按键被点击

protected:
    void showEvent(QShowEvent*);
    void hideEvent(QHideEvent *);
    void keyPressEvent(QKeyEvent *e);
    //void mousePressEvent(QMouseEvent *e);

signals:
    void sendSIGNAL(enSysSignal _sys_signal);
    //void sendTest(DataTrans::Client);

private:
    Ui::MainWindow *ui;

    QString barcodeData;
    bool barcodeMode;
    QTimer readBarcode;
    QLabel *loading;
    QMovie *mMovie;
};

#endif // MAINWINDOW_H
