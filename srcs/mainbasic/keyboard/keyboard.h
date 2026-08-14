#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <QWidget>
#include <QPushButton>
#include <QDebug>
#include <QtCore>
#include <QHash>
#include <QPair>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QCoreApplication>
#include <QTimer>

#include "baseform.h"

#include "inputmethodintf.h"
#include "handwriteboard.h"

//用于存放候选词
class MyListWidget : public QListWidget {
    Q_OBJECT
public:
    MyListWidget(QWidget *parent = NULL);
    void setText(const QString &text);
    void addOneItem(const QString &text);

signals:
    void pressedChanged(const int &code, const QString &text);

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    QMap<QString, QList<QPair<QString, QString>> > m_data;
};

//键盘类
namespace Ui {
class Keyboard;
}

class Keyboard : public CBaseDialog
{
    Q_OBJECT

public:
    explicit Keyboard(QWidget *_parent = 0);
    ~Keyboard();

    bool PushButton_Filtration(QString ObjectName);
    void keyInput(QString &s);
    void updateCand(QStringList &_candidate_list);
    void selCand(int i);
    void showKeyBoard(QString &&_old_str = "");
    void setInputFocus();
    void clearText();
    QString getText();
    void PushButton_Language_Text(QString Str);

public slots:
    void Slot_Enter_into_Symbol();
    void Slot_Enter_into_Num();
    void Slot_Delete_Char();
    void Slot_Back_Enter_clicked();
//    void on_button_clicked(QString txt);
    void emitSendText();
    void onKeyPressed(int key, QString value);
    void clearBufferText();
    void Hand_Reset_clicked();
    void Slot_Backspace_pressed_State();
    void Slot_Backspace_released_State();

signals:
    void sendText(QString text);

protected slots:
    void recognizeResultSlot(const QStringList &list);
    void updateLanguageButtonStyle();       // 更新语言切换按钮的样式
    void slot_key_clicked();

protected:
    //bool eventFilter(QObject *_obj, QEvent *_event) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

    int PrevIndexWidget;                                    // 用于保存上一层页面
    bool Cap_Aa_Flag = false;                               // 大小写切换标志（大写为 true）
    bool NumKeyboardFlag = false;                           // 是否是从数字键盘进入的符号界面
    int LanguageItem = 0;                                   // 语言类型  0:中文  1:英文 ...
    MyListWidget *ChineseWidget = Q_NULLPTR;                // 存放候选词
    QTimer *TimerDel = Q_NULLPTR;                           // 用于连续删除文字
    int DeleteCharCount = 0;                                // 删除文字计数
    HandWriteBoard *hwBoard = Q_NULLPTR;                    // 实例化画板类

    CInputMethodIntf *inputMethodIntf = Q_NULLPTR;

    enThemeType oldTheme;   // 记录下本窗体显示之前的主题（因本窗体的样式未实现，只支持白色样式，退出时须使底窗口的样式从临时样式变为原有的样式）      // TODO: 待优化

    bool isDBC = false;     // 是否全角（否则为半角）

private slots:
    void on_pushButton_English_clicked();
    void on_pushButton_PinyinMode_clicked();
    void on_pushButton_HandMode_clicked();
    void on_pushButton_StrokesMode_clicked();
    void on_pushButton_Pinyin_Language_clicked();
    void on_pushButton_Pinyin_Cap_clicked();
    void on_pushButton_Num_More_clicked();
    void on_pushButton_Num_Back_clicked();
    void on_pushButton_Symbol_SBC_clicked();
    void on_pushButton_Symbol_Back_clicked();
    void on_pushButton_Hand_Language_clicked();
    void on_pushButton_Strokes_Language_clicked();
    void on_pushButton_Strokes_Rewrite_clicked();
    void on_lineEdit_Input_returnPressed();

private:
    Ui::Keyboard *ui;
};

#endif // KEYBOARD_H
