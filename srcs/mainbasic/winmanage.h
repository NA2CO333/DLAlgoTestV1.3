#ifndef CWINMANAGE_H
#define CWINMANAGE_H

#include <QObject>
#include <QMap>
#include <QVector>

#include "myeditline.h"
#include "progresswindow.h"
#include "messagewin.h"
#include "keyboard.h"
#include "noticewin.h"
#include "suspensionpromptbox.h"
#include "basewindow.h"
#include "data.h"
#include "DataTransmit.h"
#include "globaltypes.h"

// 窗口类型
enum enWindowType
{
    windowType_Unkown = -1,

    WIN_STATUS_BAR          ,
    WIN_HOME                ,
    WIN_MEASURE             ,
    WIN_CLINIC              ,
    WIN_PER_REC             ,
    WIN_SCREEN              ,
    WIN_TOOL                ,
    WIN_RESULT              ,
    WIN_PER                 ,
    WIN_WIFI                ,
    WIN_LOG                 ,
    WIN_SET                 ,
    WIN_DATA                ,
    WIN_PROGRESS            ,
    WIN_THEME               ,
    WIN_ABOUT               ,
    WIN_DATE                ,
    WIN_PRINT               ,
    WIN_EYESIGHT            ,
    WIN_MUSIC               ,
    WIN_RUNSTATUS           ,
    WIN_IMAGE               ,
    WIN_ENGIN               ,
    WIN_BT                  ,
    WIN_DISTCALI            ,
    WIN_LAMP_CALI           ,
    WIN_DIAGNOSTIC          ,
    WIN_SUGGESTION          ,
    WIN_UPDATE_SET          ,
    WIN_UPDATE_PROGRESS     ,
};

// 预声明
CBaseWindow *getWinBase();

// 视图管理器（本类只负责窗体的显示相关功能，其它全局功能应放到 class WindowsManagers、class CGlobal 等。）
class CWinManage : public QObject
{
    Q_OBJECT

public:
    explicit CWinManage(QObject *_parent = nullptr);

    //
    void initKeyboard(QWidget *_parent);

    bool showWindow(QWidget *_win);         // 显示指定窗口

    /* 在没有用到 QDialog 的情况下，实现对对话框的交互流程。 */
    template<typename ReceiverClass>
    bool showWidgetAsDialogProcess(CBaseWidget *_dialog, ReceiverClass *_sender, void (ReceiverClass::*_slot)(int))  // 以对话框的流程显示一个普通部件
    {
        //
        if (!_dialog) {
            showSuspensionPrompt("Program Error: _dialog is null!");
            return false;
        }
        if (!_sender) {
            showSuspensionPrompt("Program Error: _sender is null!");
            return false;
        }

        //
        //m_conn_dialogFinished = QObject::connect(_dialog, &QDialog::finished, _sender, _slot, Qt::UniqueConnection);
        m_conn_dialogFinished = QObject::connect(_dialog, &CBaseWidget::sigDialogFinished, _sender, _slot, Qt::UniqueConnection);

        //_dialog->setParent(getWinBase());
        _dialog->raise();
        _dialog->open();

        m_isDialogMode = true;

        return true;
    }
    void hideDialog(CBaseWidget *_dialog, QDialog::DialogCode _dialog_code);

    void showWidgetAsDialogView(CBaseWidget *_wgt);     // 以对话框的样式显示一个普通部件（父窗口为根窗口，覆盖全屏，但不像对话框的 exec() 方法那样阻塞 UI 线程）

    /**
     * @brief 显示指定窗口
     * @param _win_type
     * @param _old_win_not_keep 不需保留在窗口栈的当前窗口（须为当前窗口指针或空，若非空，则窗口管理模块会将此窗口在窗口栈中清掉，那么从下一个窗口返回时，就不是返回到此窗口，而是返回到窗口栈中里的上一个窗口）
     */
    void showWindowByType(enWindowType _win_type, QWidget *_old_win_not_keep = nullptr);
    // TODO: 逐步淘汰这个函数，改用 *win = CWinManage::getWindow<wintype>(enWindowType) -> win->setXXX() -> CWinManage::::showWindow(win) 模式？

    void HideWin(enWindowType _win_type);
    void addWidget(QWidget *_widget);
    void backToLastWidget();

    /**
     * @brief showMsgWin
     * @param _content
     * @param _has_button
     * @param _button_text
     * @param _timeout_secs 超时（秒数）自动隐藏，小于等于0表示无超时
     * @param _is_modal
     * @return
     */
    int showMsgWin(QString _content, bool _has_button = true, QString _button_text = "OK", int _timeout_secs = -1, bool _is_modal = false);

    /**
     * @brief 显示 Yes/No 选择提示框
     * @param content           提示的内容
     * @param OKbuttonText      Yes 按钮显示的文本
     * @param NobuttonText      No 按钮显示的文本
     * @param _timeout          超时时间（秒），若超时，则自动选择，0表示无超时
     * @param _default_sel      默认选项（超时后自动选择）
     * @param _is_show_all_time 任何情况下都显示，即取消当前窗口是测量窗口时改为显示浮动消息框等的限制
     * @return
     */
    bool showNoticeWin(QString content, QString OKbuttonText = "Yes", QString NobuttonText = "No", int _timeout = 0,
                       bool _default_sel = true, bool _is_show_all_time = false);

    /**
     * @brief 隐藏消息框
     * @param _is_hide_suspension_msg: 是否隐藏浮动消息框（因为出现过不当调用而导致本应显示的浮动提示被隐藏，如筛查超时提示，所以默认不隐藏浮动提示框）
     */
    // TODO: 待检查逻辑及对其的调用
    void hideMsgWin(const int _msg_id = -1, bool _is_hide_suspension_msg = false, QWidget *_win_shown = Q_NULLPTR);

    bool isMsgShow();

    QWidget* getLastWin();
    QWidget* getCurrentWin();       // 当前通过 showWindow() 函数显示出来的窗体

    void showProgressWin();
    void hideProgesswin();
    void showKeyboard(myEditLine *_edit, QWidget *_editing_window, bool _is_modal = false);
    void hideKeyboard();
    bool getIsShowingKeyboard();        // 是否正在显示键盘
    ProgressWindow* instanceProgressWin();
    MessageWin* instanceMsg();

    //
    template<typename T>
    T *getWindow(enWindowType _win_type)
    {
        QWidget *widget = findWindowByType(_win_type);
        T *win = dynamic_cast<T*>(widget);
        if (win) {
            return win;
        } else {
            T *win = new T;
            showSuspensionPrompt(QString("Internal error: failed to obtain inst of win type (%1)").arg(_win_type), -1);
            return win;
        }
    }

    //
    QString getWindowNameByType(enWindowType _win_type);
    enWindowType getWindowTypeByName(QString _obj_name);

    int showSuspensionPrompt(QString _msg, int _msecs = 0, QWidget *_parent_wgt = Q_NULLPTR);
    void hideSuspensionPrompt(int _msg_id);
    Keyboard *mKeyboard;

    void asyncSuspensionPrompt(QString _msg, int _msecs = 0);       // 异步悬浮提示框（跨线程调用）

    /* 规范：
     * 仅允许通过 CWinManage::openClinicMeasure() 或 CWinManage::openMeasureWin() 打开测量界面，不允许直接 show 测量界面（WIN_MEASURE, WinMeasure）。
     */
    bool openClinicMeasure(enAgeRange _age_range);                                          // 打开门诊测量
    bool openMeasureWin(const CPatient &_pat, const enPatientSource _patient_source, QWidget *_old_win_not_keep = nullptr);

    //
    static QString getTrialDesc();

    void updateWindowTitle(QWidget *_widget, const QString &_title);    // 同步更新窗体和系统状态栏的标题

    /* 流水编号相关函数 */                          /* 2023-10-27 旧代码所用，现新增门诊功能后，需弃用。 */
    //static QString getNextSerialNum();                  // 获取下一个流水编号（上一个流水号 + 1）
    //static QString currentIdIntToStr(int _id);
    //static int currentIdStrToInt(QString _num);
    //static bool readCurrentIdFile(QString &_num);
    //static bool writeCurrentIdFile(QString _num);
    //static void resetCurrentIdFile();

    static QString getNewClinicNum();                           // 获取新建的诊疗号

    static bool setTranslator(const QString &_language);        // 设置翻译器

    void hideAllChildren();                                     // 隐藏所有子窗口

public slots:
    void showProgress(QString _text, int _percent);
    void slotMessage(QString _msg);

signals:
    void sigShowWindow(enWindowType _win_type, QWidget *_old_win_not_keep);
    void sigSuspensionPrompt(QString _msg, int _msecs);

protected slots:
    void slotShowWindow(enWindowType _win_type, QWidget *_old_win_not_keep);
    void slotSuspensionPrompt(QString _msg, int _msecs);

protected:
    static const char OBJ_NAME_DIALOG_COVER[];      // 对话框蒙板的对象名
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    QVector<QWidget*> WidgetVector;
    QVector<QWidget *> listOpenedWins;      // 已打开的窗口列表

    QWidget *editingWindow = Q_NULLPTR;     // 调用键盘的窗体，即调用 showKeyboard() 时传入的窗体，在隐藏键盘后置为空
    myEditLine *currentEdit;

    ProgressWindow *ProgressWin;
    MessageWin *writingWin;
    MessageWin *msgWin;
    NoticeWin  *noticeWin;

    bool isShowingKeyboard = false;

    CSuspensionPromptBox *suspensionPromptBox = Q_NULLPTR;

    int currentMsgId = 0;       // 当前消息ID

    QMap<enWindowType, QString> windowList;             // 窗口对象名列表
    QWidget *m_winCover = Q_NULLPTR;                       // 窗口遮盖（显示时总是处于窗体顶层，顶层的对话框、提示信息，须是它的子窗体）

    void initWindowList();
    void doShowWindow(enWindowType _win_type, QWidget *_old_win_not_keep);

    QWidget* findWindowByType(enWindowType _win_type);
    QWidget *findWindowByName(QString _obj_name);

    QMetaObject::Connection m_conn_dialogFinished;      // 上一次打开对话框时的 finished 信号链接     // TODO: 代码结构完善？
    bool m_isDialogMode = false;                        // 当前是否处于对话框模式

};


///=============================================================================================================
/// extern variable

CBaseWindow *getWinBase();

CWinManage *getWinManage();         // 获取“窗口管理者”对象


#endif // CWINMANAGE_H
