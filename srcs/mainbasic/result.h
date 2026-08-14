#ifndef RESULT_H
#define RESULT_H

#include <QMouseEvent>
#include <QLabel>
#include <QThread>
#include <QList>
#include <QMessageBox>
#include <QImage>
#include <QObject>
#include <QWidget>
#include <QPixmap>

#include "baseform.h"
#include "statusbarform.h"
#include "mysqlitepatients.h"
#include "eyesightstandard.h"
#include "report.h"
#include "globaltypes.h"
#include "winmultiresults.h"

using namespace std;

// 眼位数据
class Strabismus
{
public:
    void setDirection(bool rightUpDown,bool rightLeftRight,bool leftUpDown,bool leftLeftRight);
    void setValue(double rightUpDown,double rightLeftRight,double leftUpDown,double leftLeftRight);
    void getDirection(bool &rightUpDown,bool &rightLeftRight,bool &leftUpDown,bool &leftLeftRight);
    void getValue(double &rightUpDown,double &rightLeftRight,double &leftUpDown,double &leftLeftRight);
    void clear();
    void setEnable(bool);
    bool getEnable();
    void setRightEyeState(bool);
    bool getRightEyeState();
    void setLeftEyeState(bool);
    bool getLeftEyeState();
private:
    bool rightUpDown,rightLeftRight,leftUpDown,leftLeftRight;
    double rightUpDownVal,rightLeftRightVal,leftUpDownVal,leftLeftRightVal;
    bool enable;
    bool rightEyeState, leftEyeState;       // 是否显示凝视提醒图形
};

// 视力判断描述
struct stVisionJudgementDesc {
    QString R;          // 右眼
    QString L;          // 左眼
    QString Both;       // 双眼

    // 转为字符串（@param _is_format : 是否整理格式）
    QString toStr(bool _is_format = false);

};

//
namespace Ui {
class Result;
}

//
class Result : public CBaseWidget
{
    Q_OBJECT

public:
    explicit Result(QWidget *parent = 0);
    ~Result();

    // 模式标识 （支持 or 合并）
    //enum enModeFlag {
    //    modeFlag_newResult,
    //    modeFlag_oldResult,
    //};

    //void setModeFlags(enModeFlag _work_mode);

    //
    static int ultimateDirCount;
    static int ultimateFileCount;

    void setPatientSource(enPatientSource _patient_source) { m_patientSource = _patient_source; }

    void setIsNeedSave(bool _is_need_save);             // 根据是否需要保存进行相关设置

    void setPatient(const CPatient &_pat);

    void setHistoryListPtr(const std::vector<CPatient> *_ptr);                  // 设置“历史记录列表的指针”
    bool getNextResult(int _curr_id, CPatient& _pat, QString &_err_msg);        // 获取下一条结果记录
    bool getPrevResult(int _curr_id, CPatient& _pat, QString &_err_msg);

    void setIsReliable(bool _is_reliable);
    bool getIsReliable();

    void loadDataToUi(const CPatient &_pat);        // 载入业务数据对象中的值到 UI
    void refreshTitle(const CPatient &_pat);
    bool saveResult(CPatient &_pat, QString &_err_msg);
    //static void generatePdf(QString);
    void saveNotice(const CPatient &_pat);          // 检查是否需要保存，若是，则询问用户是否保存，若选是，则保存
    bool checkIsSaved(const CPatient &_pat);        // 检查数据是否已保存
    static void upLoadCallback(std::string info);
    void show_theme_state();//
    void ShowMessageWin();  //弹框:未找到图片
    bool GetDirList();      //获取目录列表
    int GetDirFile();       //获取目录文件

    bool updateEditingValues(const CPatient &_pat);     // 更新正在编辑的值（从编辑窗体返回时调用），返回：数据是否已被修改

    static stVisionJudgementDesc getVisionJudgementDesc(const CPatient &_pat, bool _has_right, bool _has_left,
                                                        stVisionJudgementRst _right_comp, stVisionJudgementRst _left_comp);
    static void getVisionJudgementRst(const CPatient &_pat, bool _has_right, bool _has_left,
                                      stVisionJudgementRst & _right_comp, stVisionJudgementRst & _left_comp);
    static enSingleDualEyeMode judgeSingleDualEyeMode(CPatient _pat, bool *_has_right = nullptr, bool *_has_left = nullptr);            // 获取单双眼模式（通过瞳孔直径数据）
    static void reduceResult(CPatient &_pat);
    static void reduceResult(std::vector<CPatient> &_pats);

    static void patientToReportData(const CPatient &_pat, const stVisionJudgementDesc &_judgement_desc, stReportData &_report_data);      // CPatient 数据转为报表数据
    static bool printA4Report(const CPatient &_pat);                                        // 打印 A4 报告

    static bool isCylNegative();        // 是否负散光

    static QString savePdfReport(const CPatient &_pat);

    void beforeBack();                      // “返回”操作前须执行的过程

signals:
    void sendSIGNAL(enSysSignal _sys_signal);
    void sigUpLoadData(QVector<int> _ids);
    void printSig(CPatient,QString);
    void sigSaveResult();
    void sigResultAbnormal();

public slots:
    void receieve();
    void batchPrint(std::vector<CPatient> pats);
    void slot_printTrans_DataSendFinished(bool _is_succ, QString _err_msg);

    void slotPhysicButtonPressed();         // 槽函数：物理按键被点击
    void slotUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg);

protected slots:
    //void barcodeHandle();
    //void slotKbReaderGetline(std::string _line_str);
    void slot_timerAutoTest_timeout();
    void slot_this_SaveResult();
    void slot_personInfo_finished(int _dialog_code);

    /**
     * @brief 结果异常处理
     * @param result:true:保存，false：重测
     */
    void slotResultAbnormal();

protected:
    void showEvent(QShowEvent*);
    void hideEvent(QHideEvent*);
    void keyPressEvent(QKeyEvent *event);
    //void mousePressEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *);

    void uploadResultData(const CPatient &_pat);
    void adjustWidgetsByVisionNotation(bool _is_refer_vision_shown);

    static bool callback_QrCodeScanned();       // 扫码事件回调

    CPatient patient;           // 当前被测者的结果数据

    enPatientSource m_patientSource = patientSource_Unknown;

    vector<CPatient> rPats;
    QString m_judgementDescStr;           // 检查结果判断描述文本
    QString barcodeData;
    bool barcodeMode;
    QTimer readBarcode;
    QTimer timerAutoTest;
    Strabismus mStrabismus;
    bool keypressState;         // 是否执行 keyPress 事件

    QLabel *rUD_label;
    QLabel *rLR_label;
    QLabel *lUD_label;
    QLabel *lLR_label;

    WinMultiResults *m_winMultiResults = nullptr;

    QString ultimateDir;//结果图片2,3d的目录

    QPixmap imgBg;

    static bool s_isCylNegative;    // 是否负散光    // NOTE: 正散光的配置未永久保存，但影响所有结果数据的输出，包括结果界面、小票打印、报告打印、结果上传等

    int x_lblSeCaptionR = 0;
    int x_lblSeCaptionL = 0;
    int x_lblSeR = 0;
    int x_lblSeL = 0;

    bool isReliable = true;

    bool isNeedSave = true;     // 是否需要保存（窗体显示前由外部模块标记是否需要保存，点击本窗体保存按钮或取消保存后，重置为 false）

    const std::vector<CPatient> * historyListPtr = Q_NULLPTR;   // 历史记录列表的指针（若非空，则可切换显示上一个下一个结果）

    int m_msgIdOfWaitingBtUpload = 0;                           // 等待蓝牙上传的提示框的 id，-1 表示没有

    bool isBatchScreen();                                       // 判断是否批量筛查状态（若是，则在状态栏显示下一个被测者，且可按物理按键开始测量下一个）
    bool printTicket(const CPatient &_pat, bool _is_check_conn = false, QString *_err_msg = nullptr);
    void setStrabismus(const CPatient &_pat, bool _has_right, bool _has_left, stVisionJudgementRst _right_comp, stVisionJudgementRst _left_comp);
    void showVisionJudgementDesc(const CPatient &_pat, bool _right, bool _left, stVisionJudgementRst _right_comp, stVisionJudgementRst _left_comp);

    /**
     * @brief 将屈光数据的正负散光符号设置为与当前的正负散光设置一致，并打印小票
     * @param _pat          病人结果信息
     * @param _result_str   结果描述
     */
    void switchCylSignAndPrintTicket(CPatient _pat, QString _judgement_desc);

    void updateView_SwitchSaveAndBackButton(bool _is_show_save);        // 切换“保存”和“返回”两个按钮的显示或隐藏状态（两个按钮在同一个位置，只能显示其一）

    void languageChange(const CPatient &_pat);

private Q_SLOTS:
    void on_pushButton_back_clicked();
    void on_pushButton_Home_clicked();
    void on_pushButton_Save_clicked();
    void on_pushButton_edit_clicked();
    void on_btnPrintReceipt_clicked();
    void on_btnPrintA4_clicked();
    void on_pushButton_preview_clicked();
    void on_pushButton_chongxin_clicked();
    void on_pushButton_PrevPage_clicked();
    void on_pushButton_NextPage_clicked();
    void on_btnSwichCylSign_clicked();
    void on_btnShowMultiResults_toggled(bool checked);
private:
    Ui::Result *ui;
};

#endif // RESULT_H
