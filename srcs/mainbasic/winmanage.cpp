#include "winmanage.h"

#include <QMessageBox>
#include <QTranslator>
#include <QApplication>

#include "camerainit.h"
#include "global.h"
#include "batterymonitor.h"
#include "runningstatus.h"
#include "windowsmanager.h"
#include "utilui.h"

//
const char CWinManage::OBJ_NAME_DIALOG_COVER[] = "DialogCover";     // 对话框蒙板的对象名

//
void CWinManage::initWindowList()
{
    windowList.insert(WIN_STATUS_BAR        , "WgtStatusBar"            );
    windowList.insert(WIN_HOME              , "MainWindow"              );
    windowList.insert(WIN_MEASURE           , "WinMeasure"              );
    windowList.insert(WIN_CLINIC            , "WinClinic"               );
    windowList.insert(WIN_PER_REC           , "WinPersonalRecord"       );
    windowList.insert(WIN_SCREEN            , "WinScreen"               );
    windowList.insert(WIN_TOOL              , "Tool"                    );
    windowList.insert(WIN_RESULT            , "Result"                  );
    windowList.insert(WIN_PER               , "PersonalInfos"           );
    windowList.insert(WIN_WIFI              , "WinWifi"                 );
    windowList.insert(WIN_LOG               , "loginWin"                );
    windowList.insert(WIN_SET               , "settings"                );
    windowList.insert(WIN_DATA              , "WinDataTrans"            );
    windowList.insert(WIN_PROGRESS          , "progressWin"             );
    windowList.insert(WIN_THEME             , "themebackground"         );
    windowList.insert(WIN_ABOUT             , "aboutdevice"             );
    windowList.insert(WIN_DATE              , "datePage"                );
    windowList.insert(WIN_PRINT             , "printerSetting"          );
    windowList.insert(WIN_EYESIGHT          , "eyesightstandard"        );
    windowList.insert(WIN_MUSIC             , "MusicSetting"            );
    windowList.insert(WIN_RUNSTATUS         , "RunningStatus"           );
    windowList.insert(WIN_IMAGE             , "previewimage"            );
    windowList.insert(WIN_ENGIN             , "engineerMode"            );
    windowList.insert(WIN_BT                , "WinBluetooth"            );
    windowList.insert(WIN_DISTCALI          , "WinDistCalibration"      );
    windowList.insert(WIN_LAMP_CALI         , "LampCalibrate"           );
    windowList.insert(WIN_DIAGNOSTIC        , "WinDiagnosticStandard"   );
    windowList.insert(WIN_SUGGESTION        , "WinDiagnosisSuggestion"  );
    windowList.insert(WIN_UPDATE_SET        , "WinUpdateSetup"          );
    windowList.insert(WIN_UPDATE_PROGRESS   , "WinUpdateProgress"       );
}

enWindowType CWinManage::getWindowTypeByName(QString _obj_name)
{
    for (auto it = windowList.begin(); it != windowList.end(); it++) {
        if (it.value() == _obj_name) {
            return it.key();
        }
    }
    return windowType_Unkown;
}

//
CWinManage::CWinManage(QObject *_parent) : QObject(_parent)
{
    ProgressWin = NULL;
    writingWin = NULL;
    msgWin = NULL;
    noticeWin = NULL;
    mKeyboard = NULL;
    editingWindow = NULL;

    QObject::connect(this, &CWinManage::sigShowWindow, this, &CWinManage::slotShowWindow, Qt::QueuedConnection);

    //
    suspensionPromptBox = CSuspensionPromptBox::getInstance();
    suspensionPromptBox->setScreenSize(SCREEN_WIDTH, SCREEN_HEIGHT);

    //
    m_winCover = new QWidget(getWinBase());
    const QRect &rect_base = getWinBase()->geometry();
    m_winCover->setGeometry(0, 0, rect_base.width(), rect_base.height());
    m_winCover->setStyleSheet("QWidget { background-color: rgb(200, 200, 200, 50); }");


    //
    initWindowList();

    //
    QObject::connect(this, &CWinManage::sigSuspensionPrompt, this, &CWinManage::slotSuspensionPrompt, Qt::QueuedConnection);

}

//返回到上一层窗口
void CWinManage::backToLastWidget()      // TODO: 不应有一个全局的“上层窗口”，否则若前后两个窗口返回时都调用该方法时将形成死循环。应每个窗口都保存自己的“上层窗口，该值由上层窗口指定或自动设定。
{
    QWidget *win_last = getLastWin();
    if (win_last) {
        enWindowType win_type = getWindowTypeByName(win_last->objectName());
        showWindowByType(win_type);
    } else {
#if (OS_TYPE == 2)
        showSuspensionPrompt("winMgr err: no pre win found!");
#endif
    }
}

//获取上一层窗口
QWidget *CWinManage::getLastWin()
{
    if (listOpenedWins.size() > 1) {
        return listOpenedWins.at(listOpenedWins.size() - 2);
    } else {
        return Q_NULLPTR;
    }
}

//获取现在窗口
QWidget *CWinManage::getCurrentWin()
{
    if (listOpenedWins.size() > 0) {
        return listOpenedWins.back();
    } else {
        return Q_NULLPTR;
    }
}

//添加窗口
void CWinManage::addWidget(QWidget *_widget)
{
    qDebug() << "add widget:" << _widget->objectName(); //2020.10.12  tao

    _widget->setVisible(false);

    WidgetVector.push_back(_widget);
}

void CWinManage::hideMsgWin(const int _msg_id, bool _is_hide_suspension_msg, QWidget *_win_shown)        // TODO: 这里隐藏所有消息框的逻辑是有问题的，特别是询问消息，应该只能隐藏自己显示的消息
{
    qDebug()<<"hideMsgWin";

    //
    if (_msg_id >= 0) {
        if (_msg_id != currentMsgId) {
            qDebug() << __PRETTY_FUNCTION__ << ": _msg_id != currentMsgId, hiding cancelled!";
            return;
        }
    }

    //
    if(msgWin != NULL)
    {
        msgWin->rejected();
        msgWin->hide();
        msgWin->deleteLater();
        msgWin = NULL;
    }

    //
    if(noticeWin != NULL)
    {
        noticeWin->rejected();
        noticeWin->hide();
        noticeWin->deleteLater();
        noticeWin = NULL;
    }

    // 隐藏浮动提示框
    CSuspensionPromptBox *prompt = CSuspensionPromptBox::getInstance();
    if (prompt->getIsShown()) {
        //
        if (_is_hide_suspension_msg) {
            prompt->hideMsgWin();
        }

        //
        if (_win_shown == prompt->parent()) {
            prompt->hideMsgWin();
        }
    }
}

bool CWinManage::isMsgShow()
{
    if(msgWin != NULL && msgWin->isVisible())
        return true;
    else
        return false;
}

int CWinManage::showMsgWin(QString _content, bool _has_button, QString _button_text, int _timeout_secs, bool _is_modal)
{
    //
    QWidget *curr_win = getCurrentWin();
    if (curr_win && curr_win->objectName() == getWindowNameByType(WIN_MEASURE)) {  // TODO: 测量窗体为什么禁止弹出提示框？
        showSuspensionPrompt(_content, -1);
        return -1;
    }

    // 若正在编辑
    if (editingWindow) {     /* 嵌入式 Linux 平台里，若是在编辑控件事件里弹出，会导致界面没触屏没响应，须用此方法弹出对话框。 */    // TODO: 因为弹出框占用焦点，但却被在后显示的窗口挡住？
        QMessageBox::information(Q_NULLPTR, "information", _content, QMessageBox::Ok);
        return -1;
    }

    //
    QWidget *win_parent = curr_win;
    if (!win_parent) {
        //QMessageBox::information(Q_NULLPTR, "information", _content, QMessageBox::Ok);
        //return;

        win_parent = getWinBase();
    }

    //
    if (msgWin != NULL)
    {
        msgWin->rejected();
        msgWin->hide();
        msgWin->deleteLater();
        msgWin = NULL;
    }

    msgWin = new MessageWin(win_parent);
    msgWin->setWindowModality(Qt::WindowModal);
    msgWin->setContent(_content);
    msgWin->setButtonEnable(_has_button);
    msgWin->setTimeout(_timeout_secs);
    if(_button_text != "OK")
        msgWin->setButtonText(_button_text);

    qDebug()<<sender()<<"--msgWIn:"<<msgWin<<"--"<<_content;
    if (!_is_modal) {
        msgWin->show();
    } else {
        msgWin->exec();
    }

    //
    int msg_id = ++currentMsgId;
    qDebug() << __PRETTY_FUNCTION__ << ": currentMsgId = " << currentMsgId;

    //
    return msg_id;
}

bool CWinManage::showNoticeWin(QString content, QString OKbuttonText, QString NobuttonText, int _timeout, bool _default_sel, bool _is_show_all_time)
{
    QWidget *curr_win = getCurrentWin();
    if (curr_win && curr_win->objectName() == getWindowNameByType(WIN_MEASURE) && !_is_show_all_time) {    // TODO: 测量窗体为什么禁止弹出提示框？
        showSuspensionPrompt(content, 5000);
        return false;
    }

    if (noticeWin != NULL) {
        noticeWin->rejected();
        noticeWin->hide();
        noticeWin->deleteLater();
        noticeWin = NULL;
    }

    //
    noticeWin = new NoticeWin(curr_win);
    noticeWin->setWindowModality(Qt::WindowModal);
    noticeWin->setContent(content);
    if(OKbuttonText != "Yes" || NobuttonText != "No")               // NOTE: 以此代码逻辑，此函数内的按钮文本默认值，和提示框的默认按钮文本，并不一定一致
        noticeWin->setButtonText(OKbuttonText,NobuttonText);
    noticeWin->timeout = _timeout;
    noticeWin->defaultSelect = _default_sel;

    if (noticeWin->exec() == QDialog::Accepted) {
        return true;
    } else {
        return false;
    }
}

void CWinManage::initKeyboard(QWidget *_parent)  //初始化键盘
{
    if (mKeyboard == NULL) {
        mKeyboard = new Keyboard(_parent);
        mKeyboard->setVisible(false);
    }
}

QWidget *CWinManage::findWindowByName(QString _obj_name)
{
    //
    int size = WidgetVector.size();
    for (int i = 0; i < size; i++) {
        QWidget *win = WidgetVector.at(i);
        if(win->objectName() == _obj_name) {
            return win;
        }
    }
    return Q_NULLPTR;
};

QWidget *CWinManage::findWindowByType(enWindowType _win_type)
{
    //
    QString obj_name = windowList.value(_win_type);

    //
    return findWindowByName(obj_name);
}

bool CWinManage::showWindow(QWidget *_win)
{
    //
    if (!_win) {
        showSuspensionPrompt("Program Error: _win is null!");
        return false;
    }

    //
    QString obj_name = _win->objectName();
    enWindowType win_type = getWindowTypeByName(obj_name);
    if (windowType_Unkown != win_type) {
        showWindowByType(win_type);                         // TODO: 逻辑优化：已经有窗口指针了，又经过对象名找窗口指针？
        return true;
    } else {
        logCritical(QString("Windows's objectName '%1' not found!").arg(obj_name), CGlobal::LOG_SYS);
        return false;
    }
}

void CWinManage::hideDialog(CBaseWidget *_dialog, QDialog::DialogCode _dialog_code)
{
    _dialog->done(_dialog_code);

    QObject::disconnect(m_conn_dialogFinished);

}

void CWinManage::showWidgetAsDialogView(CBaseWidget *_wgt)
{
    //
    if (!_wgt) {
        showSuspensionPrompt("LogicError: Dialog pointer is NULL!");
        return;
    }

    //
    if (_wgt->isVisible()) {
        logWarning(QString("%1: the showing window has been shown! ObjName = %2, WinTitle = %3").arg(__PRETTY_FUNCTION__).arg(_wgt->objectName()).arg(_wgt->windowTitle()));
        return;
    }

    //
    QWidget *dialog_cover = new QWidget(getWinBase());
    dialog_cover->setObjectName(OBJ_NAME_DIALOG_COVER);
    dialog_cover->setParent(getWinBase());
    dialog_cover->setGeometry(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    //
    QMetaObject::Connection *conn = new QMetaObject::Connection();

    // NOTE: 注意：被显示的窗口若重写了 showEvent() 事件，须显式调用基类的 showEvent() 方法，否则 sigVisibleChanged 事件不会被触发

    //
    *conn = QObject::connect(_wgt, &CBaseWidget::sigVisibleChanged, this, [dialog_cover, _wgt, conn] (bool _is_visible) {
        if (!_is_visible) {
            dialog_cover->hide();
            if (conn) {
                if (*conn) {
                    QObject::disconnect(*conn);     // TODO: 如果根窗口关闭时，这里弹出的对话框还未关闭，会发生异常？
                }
                delete conn;
            }
            _wgt->setParent(nullptr);
            dialog_cover->deleteLater();
        }
    });

    _wgt->setParent(dialog_cover);
    Util::Ui::centerWidget(_wgt);

    dialog_cover->show();
    _wgt->show();
}

// 显示窗体         // TODO: 逐步淘汰这个函数？改用 *win = CWinManage::getWindow(enWindowType) -> win->setXXX() -> CWinManage::::showWindow(win) 模式
void CWinManage::showWindowByType(enWindowType _win_type, QWidget *_old_win_not_keep)       /* 注意：（i.MX6Q系统？）使用信号槽切换窗口后，如果先关闭当前唯一窗体（包括对话框的 accept(), reject(), done() 等）再调新窗体，会导致程序退出！ */
{
    if (windowType_Unkown == _win_type) {
        showSuspensionPrompt(tr("显示窗口失败：类型未知"));    // "Showing window failed: type unknown"
        return;
    }

    //
#if QPA_PLATFORM_TYPE != 1
    emit sigShowWindow(_win_type, _old_win_not_keep);
#else
    slotShowWindow(_win_type, _old_win_not_keep);
    // TODO: 在 i.MX6Q 系统里用信号槽切换窗口，v1.4 曾出现过 Edit 窗体退出时整个进程退出（不是崩溃）？但是 1.5 在 rk3568 系统里没出现过此bug？
#endif
}

// 显示窗体
void CWinManage::slotShowWindow(enWindowType _win_type, QWidget *_old_win_not_keep)
{
    struct stShowCmdHistInfo {      // 显示命令历史信息
        enWindowType winType    = enWindowType::windowType_Unkown;
        QWidget *oldWinNotKeep  = nullptr;
    };

    static QVector<stShowCmdHistInfo> cmd_buff;     // 指令缓冲
    static bool is_showing = false;

    /* 这里采用队列先把要显示的窗体记录下来，然后循环显示，直到全部显示完。
     * 这样可以避免在信号队列中存在较近的两个显示窗体的信号时（比如用户在返回按钮位置快速点击两次，比如快速点击两次“工程调试模式”的返回按钮），
     * 就会导致窗体显示函数执行过程中，又执行了下一个窗体的显示槽函数，导致视图队列 listOpenedWins 的维护出错，从而导致获取当前窗体的函数出错。
     */

    //
    cmd_buff.append({_win_type, _old_win_not_keep});

    //
    if (is_showing) {
        return;
    }

    //
    is_showing = true;
    while (cmd_buff.size() > 0) {
        stShowCmdHistInfo cmd = cmd_buff.last();
        cmd_buff.removeLast();

        doShowWindow(cmd.winType, cmd.oldWinNotKeep);
    }
    is_showing = false;
}

void CWinManage::slotSuspensionPrompt(QString _msg, int _msecs)
{
    showSuspensionPrompt(_msg, _msecs);
}

void CWinManage::doShowWindow(enWindowType _win_type, QWidget *_old_win_not_keep)
{
#if (1 == OS_TYPE)
    static bool is_showing = false;     // TODO: 改用独占锁或消息队列支持多线程？
    if (is_showing) {       /* 如果在一个窗体的 showEent() 里再次调用本函数 show 另一个普通窗体，则会导致多出一个无法显示的对话框，且占用了输入焦点，导致触屏失效。 */
        //showMsgWin("Multiple showing the same window!");      // TODO: 这个有风险？如果后面再有弹出对话框，可能会导致屏幕无法点击？
        logCritical("Multiple showing the same window!");
        return;
    }
    is_showing = true;
#endif

    //
    do {
        //
        QWidget *win = findWindowByType(_win_type);
        if (win)
        {
            qDebug() << "show " << win->objectName();

            //
            hideMsgWin(-1, false, win);                 // TODO: 这里隐藏所有消息框，逻辑有问题？

            // 窗体的父对象须是底窗体
            if (win->parent() != getWinBase()) {
                win->setParent(getWinBase());
            }

            // 若基窗体未显示，则显示
            if (!getWinBase()->isVisible()) {
                getWinBase()->show();
            }

            // 显示需显示的窗体
            //CBaseFormIntf::centerWidget(win);
            if (win->isVisible()) {
                win->hide();
            }
            win->show();
            //qApp->processEvents();

            //
            //WgtStatusBar::getInstance()->setTitle(win->windowTitle());   /* 由窗体主动调用标题栏的更新方法，这里不必设置 */

            // 状态栏的父窗体切换
            CBaseFormIntf *base_form = dynamic_cast<CBaseFormIntf *>(win);
            if (base_form) {
                if (base_form->isShowStatusBar) {
                    //WgtStatusBar::setCurrParent(win);
                    if (!WgtStatusBar::instance()->isVisible()) {
                        WgtStatusBar::instance()->setVisible(true);
                    }
                } else {
                    //WgtStatusBar::setCurrParent(Q_NULLPTR);
                    if (WgtStatusBar::instance()->isVisible()) {
                        WgtStatusBar::instance()->setVisible(false);
                    }
                }
            } else {
                logCritical("cast to CBaseFormIntf failed when showing new win!");
                // TODO: 转换失败，说明代码里有不规范操作？
            }

            // 切换悬浮消息框的父窗体
            //if (CSuspensionPromptBox::getInstance()->getIsShown()) {
            //    CSuspensionPromptBox::getInstance()->changeParent(win);
            //}
            /* 浮动消息框的父窗口改为 g_WinBase 后，不需切换父窗口 */

            //
            QString obj_name = windowList.value(_win_type);

            // 隐藏其它窗体
            int size = WidgetVector.size();
            for (int i = 0; i < size; i++) {
                QWidget *item_win = WidgetVector.at(i);
                if (item_win->isVisible()) {
                    if (item_win->objectName() != obj_name) {
                        item_win->hide();
                        qDebug() << "hide " << item_win->objectName();
                    }
                }
            }

            // 维护“已打开的窗体列表”（窗口栈）
            if (_old_win_not_keep && !listOpenedWins.isEmpty()) {           // 从窗口栈里移除不需保留的旧窗口
                if (listOpenedWins.last() == _old_win_not_keep) {
                    listOpenedWins.removeLast();
                }
            }

            int idx_new = -1;
            for (int i = listOpenedWins.size() - 1; i >= 0; i--) {          // 从后往前查找正在打开的窗体指针
                if (listOpenedWins.at(i) == win) {
                    idx_new = i;
                    break;
                }
            }
            if (idx_new >= 0) {                                            // 若找到，则只需将列表中此后的窗体移除掉，使新窗口为最后窗口
                if (idx_new < listOpenedWins.size() - 1) {
                    for (int i = listOpenedWins.size() - 1; i > idx_new; i--) {
                        listOpenedWins.removeLast();
                    }
                }
            } else {
                listOpenedWins.append(win);                              // 否则，将正在打开的窗体指针添加到已打开窗体列表
            }

            // afterShow 事件
            if (base_form) {
                base_form->callAfterShow(win->parent());
            }

            // 若是试用机，且标题栏可见，则显示到期信息
            if (CAuthIntf::authType_Trial == CGlobal::authType && WgtStatusBar::instance()->isVisible()
                    && getWindowNameByType(WIN_MEASURE) != win->objectName()
                    //&& getWindowNameByType(WIN_TOOL) != win->objectName()
                    ) {
                //WgtStatusBar::instance()->setTrialDesc(CWinManage::getTrialDesc());
                WgtStatusBar::instance()->updateTitle();
            }

        }
    } while (false);

#if (1 == OS_TYPE)
    is_showing = false;
#endif
}

// 隐藏窗体
void CWinManage::HideWin(enWindowType _win_type)
{
    //
    QString obj_name = windowList.value(_win_type);

    //
    QWidget *win_item = findWindowByName(obj_name);
    if (win_item) {
        if (win_item->isVisible()) {
            win_item->hide();
        }
    }
}

ProgressWindow* CWinManage::instanceProgressWin()
{
    if(ProgressWin != NULL)
        return ProgressWin;

    qDebug()<<"init progresswin";
    ProgressWin = new ProgressWindow();
    ProgressWin->setWindowModality(Qt::ApplicationModal);
    return ProgressWin;

}

MessageWin* CWinManage::instanceMsg()
{
    if(writingWin != NULL)
        return writingWin;

    qDebug()<<"init writingWin";
    writingWin = new MessageWin(getCurrentWin());
    //writingWin->setWindowModality(Qt::WindowModal);
    writingWin->setWindowModality(Qt::ApplicationModal);
    QString text = tr("正在写入...");   // "Writing data..."
    writingWin->setContent(text);
    writingWin->setButtonEnable(false);
    return writingWin;
}

QString CWinManage::getWindowNameByType(enWindowType _win_type)
{
    return windowList.value(_win_type);
}

void CWinManage::showProgress(QString _text, int _percent)
{
//    qDebug()<<"CWinManage::showProgress:"<<num;

    if(_text == "writing")
    {
        writingWin = instanceMsg();
        writingWin->show();
        if(ProgressWin != NULL)
            ProgressWin->hide();
        return;
    }

    ProgressWin = instanceProgressWin();
    ProgressWin->setContext(_text);
    ProgressWin->setProgress(_percent);

    Util::Ui::centerWidget(ProgressWin);

    if (_text == "end")
    {
        if(writingWin != NULL)
        {
            writingWin->accept();
            writingWin->deleteLater();
            writingWin = NULL;
        }

        ProgressWin->hide();

        //
        MessageWin msg(getCurrentWin());
        QString text = tr("完    成！");   // "Complete!"
        msg.setContent(text);
        msg.setButtonEnable(false);
        //msg.setWindowModality(Qt::WindowModal);
        msg.setWindowModality(Qt::ApplicationModal);
        msg.show();
        QTime _time = QTime::currentTime().addMSecs(1500);
        while(QTime::currentTime() < _time)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1500);
        msg.hide();
        msg.accept();
        ProgressWin->deleteLater();
        ProgressWin = NULL;
        qDebug()<<"delete progressWin";
    }
    else if (_percent == 100)
    {
        ProgressWin->hide();
        ProgressWin->deleteLater();
        ProgressWin = NULL;
        qDebug()<<"delete progressWin";
    }
    else{
        ProgressWin->show();
        ProgressWin->update();
    }

}

void CWinManage::slotMessage(QString _msg)
{
    showSuspensionPrompt(_msg);
}

// 显示悬浮消息框（@param _msecs 0 表示默认显示时间，-1 表示一直显示）
int CWinManage::showSuspensionPrompt(QString _msg, int _msecs, QWidget *_parent_wgt)      // TODO: 改为通过信号槽调用，不易后面立即显示的窗体覆盖掉？
{
    return CSuspensionPromptBox::getInstance()->showMessage((_parent_wgt ? _parent_wgt : getWinBase()), _msg, _msecs);
}

void CWinManage::hideSuspensionPrompt(int _msg_id)
{
    CSuspensionPromptBox::getInstance()->hideMsgWin(_msg_id);
}

void CWinManage::asyncSuspensionPrompt(QString _msg, int _msecs)
{
    emit sigSuspensionPrompt(_msg, _msecs);
}

/* 规范：
 * 仅允许通过 CWinManage::openClinicMeasure() 或 CWinManage::openMeasureWin() 打开测量界面，不允许直接 show 测量界面（WIN_MEASURE, WinMeasure）。
 */
// 打开门诊测量
bool CWinManage::openClinicMeasure(enAgeRange _age_range)
{
    if (!globalService()->getIsStartupFinished()) {
        getWinManage()->showSuspensionPrompt(tr("程序启动中，请稍后..."));   // "program starting, please wait..."
        return false;
    }

    // 新建诊疗号
    QString num = getNewClinicNum();
    if (num.length() == 0) {
        QString msg = tr("新建编号失败！");    // "Creating new number failed!"
        getWinManage()->showMsgWin(msg);
        return false;
    }

    // 创建数据对象
    CPatient pat;
    pat.reset();
    pat.patientid = num;
    pat.setAgeRange(_age_range);

    // 打开测量窗口
    WinMeasure::setOperationMode(operationMode_NormalMeasure);

    openMeasureWin(pat, patientSource_Manual);

    //
    return true;
}

/* 规范：
 * 仅允许通过 CWinManage::openClinicMeasure() 或 CWinManage::openMeasureWin() 打开测量界面，不允许直接 show 测量界面（WIN_MEASURE, "WinMeasure"）。
 */
// 打开测量界面
bool CWinManage::openMeasureWin(const CPatient &_pat, const enPatientSource _patient_source, QWidget *_old_win_not_keep)
{
    // 未激活的设备，不可进入测量界面          // TODO: 通过设置操作锁定来实现？
    if (!CGlobal::isDevActivated) {
        //
        this->showMsgWin(tr("设备未激活，请先扫码激活。"), true, tr("确定"), -1, true);    // "The device is not activated, please activate it first."

        // 显示激活二维码对话框
        globalService()->showDevActivateDialog();

        //
        return false;
    }

    // 已锁定的设备，不可进入测量界面
    QString err_msg_locked;
    if (globalService()->isOperationLocked(err_msg_locked)) {
        // TODO:

    }

    // 检查试用权限
    if (CAuthIntf::authType_Trial == CGlobal::authType) {
        int days_remain = QDate::currentDate().daysTo(CGlobal::authExpiryDate);
        if (days_remain < 0) {
            QString msg = tr("试用期限已到，请联系对应的销售人员");  // "trial period expired, please \ncontact corresponding sales personnel"
            this->showSuspensionPrompt(msg, -1);
            return false;
        }
    }

    // 0 格电不能进入测量界面（见 BatteryMonitor::adToLevel_21700() 的注释）
    if (BatteryMonitor::getBattLevel() <= 0 && !BatteryMonitor::getIsCharging()) {    //
        QString message,buttonText;
        message = tr("电量过低，相机已不支持工作\n，请充电或更换电池!");  // "The battery is too low and the camera does not support work.\nPlease charge or replace the battery in time!"
        buttonText = tr("确认");  // "OK"

        MessageWin mess;
        mess.setContent(message);
        mess.setWindowModality(Qt::ApplicationModal);        // 阻塞msg以外的所以窗体
        mess.setButtonText(buttonText);
        if(mess.exec() == QDialog::Accepted) {
        }
        return false;
    }

    // 空间满，不能进入测量界面                         // TODO: 和 WindowsManagers::isStorageSpaceEnough() 重复了，去掉？
    if (RunningStatus::getIsStorageFull()) {
        QString tip_str = tr("存储空间不足，请先清理。");   // "Storage space not enough, \nplease clean up."
        this->showMsgWin(tip_str);

        return false;
    }

    // 检查存储空间是否足够
    //if (!WindowsManagers::isStorageSpaceEnough()) {
    //    QString text = tr("存储空间不足，请先删除部分或全部记录！");   // "Insufficient storage space, please delete some or all records first!"
    //    getWinManage()->showMsgWin(text);
    //    return false;
    //}

    // 检查并开启相机
    if (!g_CameraIntf->getIsOn()) {
        // 如果是开机初始化中，则等待，否则重上电
        if (g_CameraIntf->getIsIniting()) {
            //
            QString msg = tr("初始化中 ...");   // "Initializing ..."
            this->showSuspensionPrompt(msg, -1);
            qApp->processEvents();

            // 等待
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 10000 && !g_CameraIntf->getIsOn()) {
                Util::waitMs(300);
            }
        } else {
            //
            QString msg = tr("加载中 ...");    // "loading ..."
            this->showSuspensionPrompt(msg, -1);
            qApp->processEvents();

            // 相机重上电
            bool is_init_succ = CameraInitThread::cameraRePowerOn();
            if (!is_init_succ) {
                // TODO:
            }
        }

        this->hideMsgWin(-1, true);
        if (!g_CameraIntf->getIsOn()) {
            this->showMsgWin(tr("启动相机失败！"), true, "OK");  // "failed to start camera!"
            //
            return false;
        }
    }

    //
    CPatient pat_new;
    pat_new.cloneFrom(_pat);

    // 若是来自外部指令，则检查编号是否已存在
    bool is_by_cmd = (patientSource_Command == _patient_source);
    if (is_by_cmd) {
        MySQLitePatients *db_access = MySQLitePatients::getInstance();
        vector<CPatient> pats = db_access->findRecordByPatientid(pat_new.patientid);
        bool id_exists = (pats.size() > 0);
        if (id_exists) {                        // 若已存在，则保存时替换旧记录
            // 提示用户
            // TODO:


            //
            CPatient &pat = pats.at(0);
            pat_new.id = pat.id;
            //is_by_cmd = false;      // 若编号已存在，则使本次测量等效于从批量筛查界面开启测量    // TODO: 这样之后指令开启的测量结果的自动保存功能失效了？
            //pat_new.isBatch = true;
        }
    }

    // 年龄段合法性的检查处理
    enAgeRange age_range = pat_new.getAgeRange();
    if (age_range < ageRange_Min || age_range > ageRange_Max) {
        // 若年龄段不合法，但生日有效，则根据生日设置年龄段
        QDate birth_date = pat_new.getBirthDate();
        if (birth_date.isValid()) {
            age_range = CAgeRange::getAgeRangeFromBirthdate(birth_date);
            pat_new.setAgeRange(age_range);
        } else {
            // 若年龄段不合法，生日也无效，则取默认年龄段
            pat_new.setAgeRange(CGlobal::defaultAgeRange);
            getWinManage()->showSuspensionPrompt(tr("年龄段缺失，已设为缺省年龄段 %1")        // "Age range missing, set as default age range %1"
                                                 .arg(CAgeRange::getAgeRangeDesc(CGlobal::defaultAgeRange)));
        }
    }

    // 设置测量时间   /* 注意：这是必须的，因为后面文件保存时需要用到此时间。另外，须确保在后续的流程中，直到数据完全保存，此字段的值不被修改。 */
    pat_new.patienttesttime = QDateTime::currentDateTime().toString(CPatient::dateTimeFormat());

    // 若已显示，先隐藏
    if (g_WinMeasure->isVisible()) {
        g_WinMeasure->hide();
    }

    // 显示测量窗口
    g_WinMeasure->setPatientSource(_patient_source);
    g_WinMeasure->setPatient(pat_new);

    this->showWindowByType(WIN_MEASURE, _old_win_not_keep);

    //
    return true;
}

void CWinManage::showProgressWin()
{

}

void CWinManage::hideProgesswin()
{
    ProgressWin->hide();
    ProgressWin->deleteLater();
}

//显示键盘
void CWinManage::showKeyboard(myEditLine *_edit, QWidget *_editing_window, bool _is_modal)      // TODO: 支持 Modal 方式，阻塞调用窗口的线程？
{
    qDebug() << "editing window " << (_editing_window ? _editing_window->objectName() : "NULL");

    hideMsgWin();

    if(_edit == NULL){
        qDebug()<<"_currentEdit == NULL,return";
        return;
    }
    if (!_edit->isEnabled()) {
        return;
    }

    //if (_editing_window == NULL) {
    //    qDebug()<<"_activeWindow == NULL,set to current window";
    //    editingWindow = getCurrentWin();
    //} else {
    //    editingWindow = _editing_window;
    //}

    currentEdit = _edit;
    QObject::connect(mKeyboard, &Keyboard::sendText, currentEdit, &myEditLine::updateText, Qt::UniqueConnection);

    //if (_is_modal) {
    //    mKeyboard->setWindowModality(Qt::WindowModal);
    //}
    Q_UNUSED(_is_modal)

    //WgtStatusBar::setCurrParent(mKeyboard);
    isShowingKeyboard = true;
    mKeyboard->showKeyBoard(_edit->text());

    //
    //if (!m_isDialogMode && _editing_window != getWinBase()) {
    //    editingWindow->hide();          // TODO: 当前窗体没必要隐藏？否则又多了一次没有意义的隐藏-显示事件。    ：有必要，此时窗口确实隐藏了，而某些情况下窗口需要知道此消息
    //}
    // NOTE: (2025-05-31)禁用此动作，否则模态对话框无法弹出键盘

}

//隐藏键盘
void CWinManage::hideKeyboard()
{
    mKeyboard->clearText();
    QObject::disconnect(mKeyboard, &Keyboard::sendText, currentEdit, &myEditLine::updateText);

    //
    //if (editingWindow) {
    //    //WgtStatusBar::setCurrParent(editingWindow);
    //    editingWindow->show();
    //    editingWindow = NULL;
    //}
    // NOTE: (2025-05-31)禁用此动作，否则模态对话框无法弹出键盘

    //
    mKeyboard->hide();
    isShowingKeyboard = false;
}

bool CWinManage::getIsShowingKeyboard()
{
    return isShowingKeyboard;
}

QString CWinManage::getTrialDesc()
{
    if (CAuthIntf::authType_Trial == CGlobal::authType) {
        int days_remain = QDate::currentDate().daysTo(CGlobal::authExpiryDate);
        QString desc;
        if (days_remain >= 0) {
            desc = tr("【试用周期还剩: %1 天】").arg(days_remain);   // "[trial period left: %1 days]"
        } else {
            desc = tr("【试用期限已到】"); // "[trial period expired]"
        }
        return desc;
    } else {
        return "";
    }
}

void CWinManage::updateWindowTitle(QWidget *_widget, const QString &_title)
{
    _widget->setWindowTitle(_title);

    CBaseFormIntf *form_inft = dynamic_cast<CBaseFormIntf *>(_widget);
    if (form_inft) {
        if (form_inft->isShowStatusBar) {
            WgtStatusBar::instance()->setTitle(_title);
        }
    }
}

//QString CWinManage::getNextSerialNum()
//{
//    //
//    QString num;

//    //
//    bool succ = false;
//    QString last_num;
//    int last_id = 0;
//    int current_id = 1;

//    // 从文件中读取最后自建编号，然后 +1 作为当前自建编号
//    if (readCurrentIdFile(last_num)) {
//        last_id = currentIdStrToInt(last_num);
//    }

//    if (last_id < 0)
//        last_id = 0;
//    current_id = last_id + 1;
//    QString current_num = currentIdIntToStr(current_id);

//    // 在数据库中查询当前自建编号是否存在
//    MySQLitePatients *db_access = MySQLitePatients::getInstance();
//    vector<CPatient> mypats = db_access->findRecordByPatientid(current_num);
//    bool id_exists = (mypats.size() > 0);

//    // 若当前自建编号在数据库中存在，则查找数据库中的最大自建编号，然后 +1 作为当前自建编号
//    if (id_exists) {
//        // 尝试直接查询最大自建编号
//        bool succ_query = db_access->getMaxPatientId(last_num);
//        if (succ_query) {
//            last_id = currentIdStrToInt(last_num);
//            if (last_id >= 0 && currentIdIntToStr(last_id) == last_num) {
//                current_id = last_id + 1;
//                succ = true;
//            } else {
//                succ_query = false;     // 若编号转为 ID 再转为编号后，值改变了，说明查询到的编号不符合格式定义，要继续查询
//            }
//        }

//        // 查询疑似自建编号列表，从大到小逐个检查是否符合格式定义，找到真正的最大自建编号
//        if (!succ_query) {
//            QStringList list_num;
//            bool found = false;
//            int count = 0, limit = 100;
//            do {
//                list_num.clear();
//                succ_query = db_access->getPatientIdList(list_num, limit, count * limit);       // 逐批降序查询
//                if (succ_query) {
//                    if (list_num.count() > 0) {
//                        for (int i = 0; i < list_num.count(); i++) {
//                            last_num = list_num[i];
//                            last_id = currentIdStrToInt(last_num);
//                            if (last_id >= 0 && currentIdIntToStr(last_id) == last_num) {
//                                found = true;
//                                break;
//                            }
//                        }
//                    } else {
//                        break;
//                    }
//                    if (found) {
//                        current_id = last_id + 1;
//                        break;
//                    }
//                } else {
//                    break;
//                }

//                //
//                count++;
//            } while (!found);
//            succ = found;
//        }
//    } else {
//        succ = true;
//    }

//    // 设置当前被测者编号
//    if (succ) {
//        //
//        num = currentIdIntToStr(current_id);
//    }

//    //
//    return num;
//}

//QString CWinManage::currentIdIntToStr(int _id)
//{
//    QString num = QString("P%1").arg(_id, 5, 10, QLatin1Char('0'));
//    return num;
//}

//int CWinManage::currentIdStrToInt(QString _num)
//{
//    bool is_ok;
//    int id = _num.mid(1).toInt(&is_ok, 10);
//    if (is_ok) {
//        return id;
//    } else {
//        return -1;
//    }
//}

//bool CWinManage::readCurrentIdFile(QString &_num)
//{
//    bool succ = false;
//    _num = "";
//    QString file_path = QString("/media/currentId.txt");  // TODO: 并入配置文件？
//    QFile file(file_path);
//    if (file.exists()) {
//        if (file.open(QIODevice::ReadOnly)) {
//            QTextStream text_stream(&file);
//            text_stream >> _num;
//            file.close();
//            succ = true;
//        }
//    } else {
//        logWarning("MainWindow::readCurrentIdFile(): file currentId.txt missed!");
//    }
//    return succ;
//}

//bool CWinManage::writeCurrentIdFile(QString _num)
//{
//    QString file_path = QString("/media/currentId.txt");  // TODO: 并入配置文件？
//    QFile file(file_path);
//    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
//        QTextStream text_stream(&file);
//        text_stream << _num;
//        text_stream.flush();
//        file.flush();
//        file.close();

//        return true;
//    } else {
//        return false;
//    }
//}

//void CWinManage::resetCurrentIdFile()
//{
//    writeCurrentIdFile(currentIdIntToStr(0));
//}

QString CWinManage::getNewClinicNum()
{
    /* 编号规则：M + 设备编号 + 当前时间戳
     */

    //
    QString num;

    //
    do {
        // 设备编号必须存在
        QString dev_num = CGlobal::devNum;
        if (dev_num.length() == 0) {
            QString msg = tr("设备编号未设置！");   // "Device Number not set!"
            getWinManage()->showMsgWin(msg);
            break;
        }

        // 得到时间戳
        qint64 time_stamp = QDateTime::currentDateTime().toSecsSinceEpoch();

        //
        num = QString("M%1%2").arg(dev_num).arg(time_stamp);

    } while (false);

    //
    return num;
}

bool CWinManage::setTranslator(const QString &_language)
{
    static QTranslator translator;
    static QString last_language = "";

    qDebug() << __PRETTY_FUNCTION__ << ": loading language: " << _language;

    QString language = (G_LANGUAGE_DEFAULT == _language ? "" : _language);

    bool is_succ = false;
    if (language != last_language) {
        if (language.length() > 0) {
            //QString ts_path = QString("%1/%2.qm").arg(QApplication::applicationDirPath()).arg(language);
            QString ts_path = QString(":/resource/language/%1.qm").arg(language);

            bool is_load = translator.load(ts_path);
            if (is_load) {
                is_succ = qApp->installTranslator(&translator);
                last_language = language;
                qDebug() << __PRETTY_FUNCTION__ << ": language has been loaded: " << _language;
            } else {
                qCritical() << __PRETTY_FUNCTION__ << ": failed to load ts file \"" << ts_path << "\"";
                qApp->removeTranslator(&translator);
            }
        } else {
            qCritical() << __PRETTY_FUNCTION__ << ": language name is empty! loading failed!";
            is_succ = qApp->removeTranslator(&translator);
            last_language = language;
        }
    } else {
        qWarning() << __PRETTY_FUNCTION__ << ": language is same with last, no changing!";
        is_succ = true;
    }

    return is_succ;
}

void CWinManage::hideAllChildren()
{
    QList<QWidget*> childen = getWinBase()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : childen) {
        qDebug() << "ObjectName: " << child->objectName();
        if (child->objectName() != OBJ_NAME_DIALOG_COVER) {
            child->hide();
        } else {
            Util::Ui::hideAllChildren(child);
        }
    }
}

///=============================================================================================================
/// extern variable

CBaseWindow *g_WinBase = Q_NULLPTR;
CBaseWindow *getWinBase()
{
    if (!g_WinBase) {
        g_WinBase = CBaseWindow::getInstance();
        g_WinBase->setScreenSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    }
    return g_WinBase;
}

//
CWinManage *g_WinManage = Q_NULLPTR;

CWinManage *getWinManage()
{
    if (!g_WinManage) {
        g_WinManage = new CWinManage;
    }
    return g_WinManage;
}
