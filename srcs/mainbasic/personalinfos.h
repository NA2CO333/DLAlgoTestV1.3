#ifndef PERSONALINFOS_H
#define PERSONALINFOS_H

#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

#include "baseform.h"
#include "winscreen.h"
#include "mysqlitepatients.h"

#include "DataTransmit.h"
#include "myeditline.h"
#include "statusbarform.h"

namespace Ui {
class PersonalInfos;
}

//
class PersonalInfos : public CBaseWidget
{
    Q_OBJECT

public:
    explicit PersonalInfos(QWidget *parent = 0);
    ~PersonalInfos();

    /** 被测者信息窗体的显示情景(getWinManage()->showWindowByType(WIN_PER))：
     *   1、在门诊记录页面点“新增”后弹出；
     *   2、在测量结果页面点击“编辑”按钮；
     *   3、批量筛查页面点击未测者，查看/编辑，然后可接着测量；
     *   4、扫码后弹出，或收到外部的“开启测量”指令后弹出；
     */
    // 本窗体的功能模式                         // TODO: 支持 or 合并？业务逻辑梳理？
    enum enModeFlag {
        modeFlag_ViewOnly           ,       // 信息查看
        modeFlag_New                ,       // 新增
        modeFlag_EditToEntity       ,       // 编辑到本窗体的实体对象
        modeFlag_EditAndSave        ,       // 编辑且保存（保存到数据库）
        modeFlag_EditAndTest        ,       // 编辑和测量
        modeFlag_FromBarcode        ,       // 筛查二维码输入后的处理（只支持筛查模式，非门诊模式，所以编号不重复）
        modeFlag_FromCommand        ,       // 外部系统的指令
    };

    /**
     * @brief 根据参数显示“被测者信息”页面
     * @param _mode_flag
     * @param _patient_source
     * @param _patient
     * @return
     */
    static PersonalInfos *getPersonalInfoWin(enModeFlag _mode_flag, enPatientSource _patient_source, const CPatient *_patient = nullptr, const QString &_barcode_data = "");
    static bool showPersonalInfo(enModeFlag _mode_flag, enPatientSource _patient_source, const CPatient *_patient = nullptr, const QString &_barcode_data = "");

    //
    CPatient &getPatient();

    static bool checkIsDataChanged(const CPatient &_old_pat, const CPatient &_new_pat,
                                   bool *_is_modi_patientid = nullptr, bool *_is_modi_patientname = nullptr,
                                   bool *_is_modi_patientsex = nullptr, bool *_is_modi_patientdate = nullptr);  // 检查窗体的数值是否有修改
    static void cloneEditingValues(const CPatient &_src_pat, CPatient &_dst_pat);               // 编辑字段的拷贝（从源数据对象将本窗体所修改的字段的值克隆到目标数据对象）

    /**
     * @brief 二维码字符串转实体对象
     * @param _code_raw     二维码字符串（原始内容）
     * @param _code_decoded 【输出参数】解码后的二维码内容（有的二维码内容可能经过 URL Encode）
     * @param _pat          【输出参数】实体对象
     * @return
     */
    static enQrCodeType barcodeDataToEntity(QString _code_raw, QString &_code_decoded, CPatient &_pat, QString &_err_msg);

    // 从源数据对象将本 UI 涉及的字段拷贝到目标数据对象
    static void cloneDataObjOfUiFields(const CPatient &_pat_src, CPatient &_pat_dst);

    /**
     * @brief 修改指定编号的被测者信息与指定实体一致（因被测者信息和测量记录未分表，避免出现分组字段不一致）
     * @param _number   被测者编号(须是旧的)
     * @return
     */
    static bool editTesteeInfoOfNumber(const QString &_number, const CPatient &_pat);

    static bool pinToTop(const QString &_number);           // 置顶指定的受检者编号

    void doOnReceivedHuayiQrCode(const QByteArray &_line_bytes);    // 接收华谊二维码后的处理

signals:
    void sendSIGNAL(enSysSignal _sys_signal);
    void sigQueryPatientInfo(QString _num);

public slots:
    void slotPhysicButtonPressed();         // 槽函数：物理按键被点击

protected Q_SLOTS:
    void slot_uploadThread_ReceivedPatientInfo(bool _is_succ, DataTrans::Client _client, QString _err_msg);    // 【查询被测者信息的应答数据】信号的槽函数
    void slot_dataIntfHuaYi_ReceivedPatientInfo(bool _is_succ, QString _err_msg, QDate _birthday, QString _business, QString _name, QString _pid, int _age);

protected:
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);
    void keyPressEvent(QKeyEvent *);

    // 保存的层级            // NOTE: 此状态值可简化保存的逻辑，不必在模块末尾再对“工作模式”做判断，降低“工作模式”复杂化的扩散
    enum enSavingLevel {
        //savingLevel_Unknown     = -1,       // 未知
        savingLevel_No          = 0,        // 不保存
        savingLevel_Entity,                 // 应用本窗体的实体对象（规则约定：须保存到实体对象，才能保存到后面的层级）
        savingLevel_Database,               // 保存到数据库
    };

    //
    void setModeFlag(enModeFlag _mode_flags);
    void setPatient(const CPatient &_pat);

    void setPatientSource(enPatientSource _patient_source) { m_patientSource = _patient_source; }

    void showWaitForRequestWin();
    void hideWaitForRequestWin();

    /**
     * @brief 得到扫码解析后的实体后的处理
     * @param _pat
     * @param _code_type
     * @return 是否需要继续后续的显示流程（比如若是需要等待被测者查询，或需要跳转到其它窗口，则不必执行后续显示过程）
     */
    bool doAfterGetEntityFromBarcode(CPatient &_pat, const enQrCodeType _code_type);

    void doAfterGetEntity();        // 获得了或更新了实体对象后需做的事情

    //
    void entityToUi(const CPatient &_pat);              // 从 实体对象 设置到 UI（无检查）
    void uiToEntity(CPatient &_pat);                    // 将 UI 的值设置到 实体对象（无检查）

    // 检查当前 UI 的数据是否合法
    static bool checkUiValues(Ui::PersonalInfos *_ui, const CPatient &_origin_pat, PersonalInfos::enModeFlag _mode_flag, QString &_err_msg);

    // 保存指定的实体对象到数据库
    bool saveEnityToDB(CPatient &_pat_new, QString &_err_msg,
                       const bool &_is_modi_patientid, const bool &_is_modi_patientname,
                       const bool &_is_modi_patientsex, const bool &_is_modi_patientdate);

    // “返回”操作之前需要做的工作。返回值：是否已完成
    bool doBeforeGoBack(QString &_err_msg);

    /**
     * @brief “保存”操作的过程（保存到 savingLevelMax 层级）
     * @param _is_manual    是否手动的操作（若是，则不询问是否保存，提示没有变更，反之则反过来）
     * @param _saving_level 保存层级
     * @param _err_msg
     * @return 是否已完成（未必是已保存，比如用户选择了不保存）
     */
    bool doSave(const bool _is_manual, const enSavingLevel _saving_level, QString &_err_msg);

    // 进入下一步（如本窗体的“测量”按钮等）操作之前需要做的工作。返回值：是否已完成
    bool doBeforeNextStep(QString &_err_msg);

    // 应用窗体的编辑并开启测量（“测量按钮”或“测量”物理按键所执行的功能）
    void applyAndStartMeasure();

    //
    void updateStyleSheet();          // 更新样式和语言，以及可见性、可编辑性等
    void updateLanguage(bool language);

    void setButtonsEnabledByMode();             // 根据工作模式设置各个功能按钮的有效性

    //
    QString m_barcodeData;
    bool m_isBarcodeValid = true;     // 条码内容是否合法     // TODO: 待清理

    //
    MySQLitePatients *m_mysql = Q_NULLPTR;
    MessageWin m_msgWin;

    CPatient m_patient;                                         // 当前窗体的业务数据对象（打开窗体前设置）
    enModeFlag m_modeFlag = modeFlag_ViewOnly;                  // 本窗体的功能模式
    enSavingLevel m_savingLevelSelf = savingLevel_No;           // 本窗体的保存层级

    enPatientSource m_patientSource = patientSource_Unknown;

private slots:
    void on_pushButton_Home_clicked();
    void on_pushButton_back_clicked();
    void on_pushButton_Save_clicked();
    void on_pushButton_test_clicked();
    void on_btnGenNum_clicked();
private:
    Ui::PersonalInfos *ui;
};

#endif // PERSONALINFOS_H
