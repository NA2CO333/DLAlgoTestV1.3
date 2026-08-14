#ifndef CBASEWIDGET_H
#define CBASEWIDGET_H

#include <QFrame>
#include <QMainWindow>
#include <QDialog>

// 主题类型     // TODO: 类似的类型集中放到一个文件？
enum enThemeType {
    themeType_Unknown   = -1,
    themeType_Black     = 1,
    themeType_White     = 2,

    themeType_Min = themeType_Black,
    themeType_Max = themeType_White,
};

//
class CBaseFormIntf     // 这个类不能继承自 QObject，否则成员函数重复，因此无法发射信号？
{
public:
    explicit CBaseFormIntf();
    bool isShowStatusBar = false;

    static void centerWidget(QWidget *_widget, QWidget *_parent = Q_NULLPTR);       // TODO: 这种工具函数与窗体接口没有必要关系，不应放这里？
    static bool changeAppStyleSheet(const QString &_file_path);         // 设置整个程序的样式表（文件路径可为资源路径）

    void callAfterShow(QObject *_parent);

protected:
    virtual void afterShow() {}

    // TODO: showedEvent, closedEvent ?

};

//
class CBaseWidget : public QFrame, public CBaseFormIntf         // NOTE: 基类不应该用 QWidget，而应该用 QFrame，因为 QWidget 不支持 StyleSheet
{
    Q_OBJECT
public:
    explicit CBaseWidget(QWidget *parent = 0, Qt::WindowFlags flags = 0);       // TODO: 默认值 -1，若为 -1 则设 parent 为全局 BaseForm

    void open();
    void done(int _code);

    bool isDialogMode();                // 当前处于对话框模式

Q_SIGNALS:
    void sigWindowTitleChanged();       // TODO: 连接当前窗体和状态栏，同步标题的修改
    void sigDialogFinished(int _dialog_code);
    void sigVisibleChanged(bool _is_visible);

protected:
    void showEvent(QShowEvent *_evt) override;
    void hideEvent(QHideEvent *_evt) override;

    bool m_isDialogMode = false;                                // 当前处于对话框模式
    QDialog::DialogCode m_dialogCode = QDialog::Rejected;       // 对话框返回码

};

//
class CBaseMainWindow : public QMainWindow, public CBaseFormIntf
{
    Q_OBJECT
public:
    explicit CBaseMainWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);

Q_SIGNALS:
    void sigWindowTitleChanged();

};

//
class CBaseDialog : public QDialog, public CBaseFormIntf
{
    Q_OBJECT
public:
    explicit CBaseDialog(QWidget *parent = 0, Qt::WindowFlags flags = 0);

Q_SIGNALS:
    void sigWindowTitleChanged();

};

#endif // CBASEWIDGET_H
