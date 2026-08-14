#include "keyboard.h"
#include "ui_keyboard.h"

#include <QScroller>
#include <QRegExp>
#include <QStringList>

#include "winmanage.h"
#include "global.h"

//
Keyboard::Keyboard(QWidget *_parent) :
    CBaseDialog(_parent),
    ui(new Ui::Keyboard)
{
    ui->setupUi(this);

    //
    inputMethodIntf = CInputMethodIntf::instance();

    //
    isShowStatusBar = true;

    LanguageItem = 0;   //默认为中文输入状态

    ui->lineEdit_Input->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ChineseWidget = new MyListWidget;
    ChineseWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui->horizontalLayout_Chinese->addWidget(ChineseWidget);

    //画板
    hwBoard = new HandWriteBoard(ui->page_Hand);
    hwBoard->setGeometry(ui->label_Hand->geometry());
    connect(hwBoard, SIGNAL(recognizeResult(QStringList)),this,SLOT(recognizeResultSlot(QStringList)));
    connect(hwBoard, SIGNAL(SendClearState()),this,SLOT(Hand_Reset_clicked()));
    ui->label_Hand->hide();
    hwBoard->inputMethodIntf = inputMethodIntf;
    inputMethodIntf->setHandWriteSize(ui->label_Hand->width(), ui->label_Hand->height());

    //连接进入符号键盘按钮
    connect(ui->pushButton_Pinyin_Symbol,SIGNAL(clicked()),this,SLOT(Slot_Enter_into_Symbol()));
    connect(ui->pushButton_Hand_Symbol,SIGNAL(clicked()),this,SLOT(Slot_Enter_into_Symbol()));
    connect(ui->pushButton_Strokes_Symbol,SIGNAL(clicked()),this,SLOT(Slot_Enter_into_Symbol()));
    //连接进入数字键盘按钮
    connect(ui->pushButton_Pinyin_Num,SIGNAL(clicked()),this,SLOT(Slot_Enter_into_Num()));
    connect(ui->pushButton_Hand_Num,SIGNAL(clicked()),this,SLOT(Slot_Enter_into_Num()));
    connect(ui->pushButton_Strokes_Num,SIGNAL(clicked()),this,SLOT(Slot_Enter_into_Num()));
    //删除键点击信号
    connect(ui->pushButton_Pinyin_Backspace,SIGNAL(pressed()),this,SLOT(Slot_Backspace_pressed_State()));
    connect(ui->pushButton_Hand_Backspace,SIGNAL(pressed()),this,SLOT(Slot_Backspace_pressed_State()));
    connect(ui->pushButton_Strokes_Backspace,SIGNAL(pressed()),this,SLOT(Slot_Backspace_pressed_State()));
    connect(ui->pushButton_Num_Backspace,SIGNAL(pressed()),this,SLOT(Slot_Backspace_pressed_State()));
    //删除键松开信号
    connect(ui->pushButton_Pinyin_Backspace,SIGNAL(released()),this,SLOT(Slot_Backspace_released_State()));
    connect(ui->pushButton_Hand_Backspace,SIGNAL(released()),this,SLOT(Slot_Backspace_released_State()));
    connect(ui->pushButton_Strokes_Backspace,SIGNAL(released()),this,SLOT(Slot_Backspace_released_State()));
    connect(ui->pushButton_Num_Backspace,SIGNAL(released()),this,SLOT(Slot_Backspace_released_State()));
    TimerDel = new QTimer;
    connect(TimerDel,SIGNAL(timeout()),this,SLOT(Slot_Delete_Char()));      //用于连续删除文本
    TimerDel->stop();
    //连接Enter回车按钮
    connect(ui->pushButton_Pinyin_Enter,SIGNAL(clicked()),this,SLOT(Slot_Back_Enter_clicked()));
    connect(ui->pushButton_Hand_Enter,SIGNAL(clicked()),this,SLOT(Slot_Back_Enter_clicked()));
    connect(ui->pushButton_Strokes_Enter,SIGNAL(clicked()),this,SLOT(Slot_Back_Enter_clicked()));
    connect(ui->pushButton_Num_Enter,SIGNAL(clicked()),this,SLOT(Slot_Back_Enter_clicked()));

    connect(ChineseWidget,SIGNAL(pressedChanged(const int &,const QString &)),this,SLOT(onKeyPressed(const int &, const QString &)));    //发送选择的文本到LineEdit
    connect(ChineseWidget,SIGNAL(pressedChanged(const int &,const QString &)),this,SLOT(clearBufferText()));  //发送完后清空候选栏

    // 初始化
    int ret = inputMethodIntf->openInputMethon(0);		// 拼音初始化
    if(ret == 0)    // 成功
    {
        qDebug() << "init ok" << endl;
    }
    else            // 出错
        qDebug() << "init error: " << ret << endl;

    //
    PrevIndexWidget = ui->stackedWidget_Key->currentIndex();    // 记录当前页面

    // 统一处理按键事件         /* 经实测，当快速点击按钮时，eventFilter() 被调用的次数会少于实际调用次数，而 on_PushButton_clicked() 却不会，所以改为使用信号槽连接 */
    //QList<QPushButton *> list_PushButton = findChildren<QPushButton *>();
    //foreach (QPushButton *p, list_PushButton) {
    //    p->installEventFilter(this);    //总事件过滤器关联按钮
    //}

    // 统一处理按键事件
    QList<QPushButton *> list_PushButton = findChildren<QPushButton *>();
    QPushButton *p;
    for (int i = 0; i < list_PushButton.size(); i++) {
        p = list_PushButton.at(i);

        QObject::connect(p, &QPushButton::clicked, this, &Keyboard::slot_key_clicked, Qt::QueuedConnection);
    }

    //
#ifndef USE_DWIME
//    ui->pushButton_HandMode->setVisible(false);
    ui->pushButton_StrokesMode->setVisible(false);
#endif

}

Keyboard::~Keyboard()
{
    delete ui;

    inputMethodIntf->close();

    delete hwBoard;
    hwBoard = nullptr;

    delete ChineseWidget;
    ChineseWidget = nullptr;

}

void Keyboard::showEvent(QShowEvent *)
{
    //ui->lineEdit_Input->setFocus();

    // 若当前语言为非汉语，则自动切换到英文状态
    if (G_LANGUAGE_CHINESE != CGlobal::language) {
        on_pushButton_English_clicked();
    }

    //
    /* page_Pinyin页面 */
    ui->pushButton_Pinyin_Space->setText(tr("空格"));         // "space"
    ui->pushButton_Pinyin_Symbol->setText(tr("符号"));        // "#@$"
    /* page_Symbol符号页面 */
    if (isDBC)
        ui->pushButton_Symbol_SBC->setText(tr("全角"));       // "DBC"
    else
        ui->pushButton_Symbol_SBC->setText(tr("半角"));       // "SBC"
    ui->pushButton_Symbol_Back->setText(tr("返回"));          // "back"
    /* stackedWidget_Key */
    ui->pushButton_Num_More->setText(tr("更多.."));           // "more.."
    ui->pushButton_Num_Space->setText(tr("空格"));            // "space"
    ui->pushButton_Num_Back->setText(tr("返回"));             // "back"

    // 语言
    if(G_LANGUAGE_CHINESE == CGlobal::language)
    {
        //英文版本屏蔽以下内容
        ui->pushButton_English->setVisible(true);
        ui->pushButton_HandMode->setVisible(true);
        ui->pushButton_PinyinMode->setVisible(true);
#ifdef USE_DWIME
        ui->pushButton_StrokesMode->setVisible(true);
#endif
    }
    else
    {
        //英文版本屏蔽以下内容
        ui->pushButton_English->setVisible(false);
        ui->pushButton_HandMode->setVisible(false);
        ui->pushButton_PinyinMode->setVisible(false);
#ifdef USE_DWIME
        ui->pushButton_StrokesMode->setVisible(false);
#endif

        ui->pushButton_Pinyin_Language->setText("En");
    }

    // 使底窗口的样式临时转为白色（因本窗体的样式未实现，只支持白色样式）      // TODO: 待优化
    oldTheme = CBaseWindow::getInstance()->getTheme();
    if (themeType_White != oldTheme) {
        CBaseWindow::getInstance()->setTheme(themeType_White);
    }

    // 窗体背景
    QPalette palette;
    palette.setBrush(this->backgroundRole(),QColor(240,240,242));
    this->setAutoFillBackground(true);
    this->setPalette(palette);

    //设置样式
    ChineseWidget->setStyleSheet(
                R"(
                QListWidget { outline: none; border:1px solid #00000000; color: black; }
                QListWidget::Item { width: 50px; height: 50px; }
                QListWidget::Item:hover { background: #4395ff; color: white; }
                QListWidget::item:selected { background: #4395ff; color: black; }
                QListWidget::item:selected:!active { background: #00000000; color: black; }
                )");

    ui->lineEdit_Input->setStyleSheet("QLineEdit { border-style: none; padding: 3px; border-radius: 5px; border: 1px solid #dce5ec; font-size: 30px; } ");

    hwBoard->setStyleSheet(ui->label_Hand->styleSheet());

    QList<QPushButton *> list_PushButton = findChildren<QPushButton *>();
    foreach(QPushButton *p,list_PushButton)
    {
        p->setStyleSheet("QPushButton{border-radius:3px; background-color:rgb(180,180,180); color:rgb(1,1,1);}");
    }

    updateLanguageButtonStyle();

}

void Keyboard::hideEvent(QHideEvent *)
{
    // 退出时，使底窗口的样式从临时样式变为原有的样式      // TODO: 待优化
    if (CBaseWindow::getInstance()->getTheme() != oldTheme) {
        CBaseWindow::getInstance()->setTheme(oldTheme);
    }

}

//总事件过滤器        /* 经实测，当快速点击按钮时，eventFilter() 被调用的次数会少于实际调用次数，而 on_PushButton_clicked() 却不会，所以改为使用信号槽连接 */
//bool Keyboard::eventFilter(QObject *obj, QEvent *_event)
//{
//    //键盘事件处理
//    //qDebug()<<"-----event3:"<<_event;
//    if(_event->type() == QEvent::MouseButtonPress)   //点击按键
//    {
//        if(PushButton_Filtration(obj->objectName())) {
//            return QWidget::eventFilter(obj,_event);
//        }
//        QPushButton *button = dynamic_cast<QPushButton *>(obj);
//        if (button) {
//            QString text = button->text();
//            if(ui->stackedWidget_Key->currentIndex()==2)   //笔画界面
//            {
//                if(obj->objectName()=="pushButton_Strokes_Heng")
//                    text = "1";     //传"一"
//                else if(obj->objectName()=="pushButton_Strokes_Shu")
//                    text = "2";     //传"丨"
//                else if(obj->objectName()=="pushButton_Strokes_Pie")
//                    text = "3";     //传"丿"
//                else if(obj->objectName()=="pushButton_Strokes_Dian")
//                    text = "4";     //传"、"
//                else if(obj->objectName()=="pushButton_Strokes_Hengpie")
//                    text = "5";     //传"フ"
//                else if(obj->objectName()=="pushButton_Strokes_Globbing")
//                    text = "6";     //传"通配"
//                else if(obj->objectName()=="pushButton_Strokes_Participle")
//                    text = "7";     //传"分词"键
//            }
//            if(obj->objectName()=="pushButton_Pinyin_Space" || obj->objectName()=="pushButton_Hand_Space" || obj->objectName()=="pushButton_Strokes_Space" || obj->objectName()=="pushButton_Num_Space")
//            {
//                text = " ";
//                keyInput(text);  //传空格键
//                return QWidget::eventFilter(obj,_event);
//            }
//            keyInput(text);
//        }
//    }
//    return QWidget::eventFilter(obj,_event);
//}

void Keyboard::slot_key_clicked()
{
    // 键盘事件处理
    QPushButton *button = dynamic_cast<QPushButton *>(sender());
    if (button) {
        //qDebug()<<"-----event3:"<<_event;

        //
        if (PushButton_Filtration(button->objectName())) {
            return;
        }

        //
        QString text = button->text();
        if (ui->stackedWidget_Key->currentIndex()==2) {         //笔画界面
            if(button->objectName()=="pushButton_Strokes_Heng")
                text = "1";     //传"一"
            else if(button->objectName()=="pushButton_Strokes_Shu")
                text = "2";     //传"丨"
            else if(button->objectName()=="pushButton_Strokes_Pie")
                text = "3";     //传"丿"
            else if(button->objectName()=="pushButton_Strokes_Dian")
                text = "4";     //传"、"
            else if(button->objectName()=="pushButton_Strokes_Hengpie")
                text = "5";     //传"フ"
            else if(button->objectName()=="pushButton_Strokes_Globbing")
                text = "6";     //传"通配"
            else if(button->objectName()=="pushButton_Strokes_Participle")
                text = "7";     //传"分词"键
        }

        //
        if (button->objectName()=="pushButton_Pinyin_Space" || button->objectName()=="pushButton_Hand_Space"
                || button->objectName()=="pushButton_Strokes_Space" || button->objectName()=="pushButton_Num_Space")
        {
            text = " ";
            keyInput(text);  //传空格键
            return;
        }

        //
        keyInput(text);
    } else {
        qDebug() << __PRETTY_FUNCTION__ << ": logic error: dynamic_cast failed!";
    }
}

//过滤功能按钮
bool Keyboard::PushButton_Filtration(QString ObjectName)
{
    if(ObjectName=="pushButton_English" || ObjectName=="pushButton_PinyinMode" || ObjectName=="pushButton_HandMode" || ObjectName=="pushButton_StrokesMode")
        return true;
    else if(ObjectName=="pushButton_Pinyin_Cap" || ObjectName=="pushButton_Pinyin_Backspace" || ObjectName=="pushButton_Pinyin_Symbol" || \
            ObjectName=="pushButton_Pinyin_Language" || ObjectName=="pushButton_Pinyin_Num" || ObjectName=="pushButton_Pinyin_Enter")
        return true;
    else if(ObjectName=="pushButton_Hand_Symbol" || ObjectName=="pushButton_Hand_Language" || ObjectName=="pushButton_Hand_Backspace" || \
            ObjectName=="pushButton_Hand_Num"  || ObjectName=="pushButton_Hand_Enter")
        return true;
    else if(ObjectName=="pushButton_Strokes_Backspace" ||  ObjectName=="pushButton_Strokes_Rewrite" || ObjectName=="pushButton_Strokes_Symbol" || \
            ObjectName=="pushButton_Strokes_Language" || ObjectName=="pushButton_Strokes_Num" || ObjectName=="pushButton_Strokes_Enter")
        return true;
    else if(ObjectName=="pushButton_Symbol_Back" || ObjectName=="pushButton_Symbol_SBC" || ObjectName=="pushButton_Num_Backspace" || \
            ObjectName=="pushButton_Num_More" || ObjectName=="pushButton_Num_Back" || ObjectName=="pushButton_Num_Enter")
        return true;
    else
        return false;
}

void Keyboard::PushButton_Language_Text(QString Str)
{
    ui->pushButton_Pinyin_Language->setText(Str);
    ui->pushButton_Hand_Language->setText(Str);
    ui->pushButton_Strokes_Language->setText(Str);
}

void Keyboard::on_pushButton_English_clicked()
{
    // 按下“拼音”
    on_pushButton_PinyinMode_clicked();

    // 切换到“英文”
    if (0 == LanguageItem) {
        on_pushButton_Pinyin_Language_clicked();
    }

    // 语言按钮样式
    updateLanguageButtonStyle();

}

void Keyboard::on_pushButton_PinyinMode_clicked()
{
    if(!inputMethodIntf->openInputMethon(0))    //拼音初始化
    {
        qDebug() << "Pinyin Init Ok" << endl;
    }
    ui->stackedWidget_Key->setCurrentIndex(0);  //进入拼音页面
    if(LanguageItem == 0)
        PushButton_Language_Text("中文");
    else if(LanguageItem == 1)
        PushButton_Language_Text("En");
    clearBufferText();

    // 切换到“中文”
    if (1 == LanguageItem) {
        on_pushButton_Pinyin_Language_clicked();
    }

    // 语言按钮样式
    updateLanguageButtonStyle();

}

void Keyboard::on_pushButton_HandMode_clicked()
{
    ui->stackedWidget_Key->setCurrentIndex(1);  //进入手写页面
    if(LanguageItem == 0)
    {
        PushButton_Language_Text("中文");
        hwBoard->setLanguage(HandWriteBoard::Chinese);
    }
    else if(LanguageItem == 1)
    {
        PushButton_Language_Text("En");
        hwBoard->setLanguage(HandWriteBoard::English);
    }
    clearBufferText();

    // 语言按钮样式
    updateLanguageButtonStyle();

}

void Keyboard::on_pushButton_StrokesMode_clicked()
{
    int ret = inputMethodIntf->openInputMethon(1);
    if(ret == 0)
        qDebug()<<"-----Strokes Init Ok";
    ui->stackedWidget_Key->setCurrentIndex(2);  //进入笔画页面
    if(LanguageItem == 0)
        PushButton_Language_Text("中文");
    else if(LanguageItem == 1)
        PushButton_Language_Text("En");
    clearBufferText();
}

//void Keyboard::on_button_clicked(QString txt)
//{
//    qDebug()<<"-----txt1:"<<txt;
//}

/****************************** 拼音键盘 **********************************/
void Keyboard::on_pushButton_Pinyin_Language_clicked()
{
    //
    if (ui->pushButton_Pinyin_Language->text() == "中文" || G_LANGUAGE_CHINESE != CGlobal::language)  // NOTE: 非中文语言时，禁用切换到中文输入法的功能
    {
        // 切换到英文输入状态
        LanguageItem = 1;
        ui->pushButton_Pinyin_Language->setText("En");
        ui->pushButton_Pinyin_Comma->setText(",");
        ui->pushButton_Pinyin_Period->setText(".");
    }
    else if(ui->pushButton_Pinyin_Language->text() == "En")
    {
        // 切换到拼音输入法状态
        LanguageItem = 0;
        ui->pushButton_Pinyin_Language->setText("中文");
        ui->pushButton_Pinyin_Comma->setText("，");
        ui->pushButton_Pinyin_Period->setText("。");
    }

    // 英文输入状态下，自动切换到半角符号输入状态
    if (1 == LanguageItem) {
        isDBC = true;
        on_pushButton_Symbol_SBC_clicked();
    }

    // 清空候选列表
    ChineseWidget->clear();
    ui->label_PinYin->clear();
}

// 切换大小写
void Keyboard::on_pushButton_Pinyin_Cap_clicked()
{
    if (!Cap_Aa_Flag)
    {
        Cap_Aa_Flag = true;
        ui->pushButton_Pinyin_Q->setText("Q");
        ui->pushButton_Pinyin_W->setText("W");
        ui->pushButton_Pinyin_E->setText("E");
        ui->pushButton_Pinyin_R->setText("R");
        ui->pushButton_Pinyin_T->setText("T");
        ui->pushButton_Pinyin_Y->setText("Y");
        ui->pushButton_Pinyin_U->setText("U");
        ui->pushButton_Pinyin_I->setText("I");
        ui->pushButton_Pinyin_O->setText("O");
        ui->pushButton_Pinyin_P->setText("P");
        ui->pushButton_Pinyin_A->setText("A");
        ui->pushButton_Pinyin_S->setText("S");
        ui->pushButton_Pinyin_D->setText("D");
        ui->pushButton_Pinyin_F->setText("F");
        ui->pushButton_Pinyin_G->setText("G");
        ui->pushButton_Pinyin_H->setText("H");
        ui->pushButton_Pinyin_J->setText("J");
        ui->pushButton_Pinyin_K->setText("K");
        ui->pushButton_Pinyin_L->setText("L");
        ui->pushButton_Pinyin_Z->setText("Z");
        ui->pushButton_Pinyin_X->setText("X");
        ui->pushButton_Pinyin_C->setText("C");
        ui->pushButton_Pinyin_V->setText("V");
        ui->pushButton_Pinyin_B->setText("B");
        ui->pushButton_Pinyin_N->setText("N");
        ui->pushButton_Pinyin_M->setText("M");
    }
    else
    {
        Cap_Aa_Flag = false;
        ui->pushButton_Pinyin_Q->setText("q");
        ui->pushButton_Pinyin_W->setText("w");
        ui->pushButton_Pinyin_E->setText("e");
        ui->pushButton_Pinyin_R->setText("r");
        ui->pushButton_Pinyin_T->setText("t");
        ui->pushButton_Pinyin_Y->setText("y");
        ui->pushButton_Pinyin_U->setText("u");
        ui->pushButton_Pinyin_I->setText("i");
        ui->pushButton_Pinyin_O->setText("o");
        ui->pushButton_Pinyin_P->setText("p");
        ui->pushButton_Pinyin_A->setText("a");
        ui->pushButton_Pinyin_S->setText("s");
        ui->pushButton_Pinyin_D->setText("d");
        ui->pushButton_Pinyin_F->setText("f");
        ui->pushButton_Pinyin_G->setText("g");
        ui->pushButton_Pinyin_H->setText("h");
        ui->pushButton_Pinyin_J->setText("j");
        ui->pushButton_Pinyin_K->setText("k");
        ui->pushButton_Pinyin_L->setText("l");
        ui->pushButton_Pinyin_Z->setText("z");
        ui->pushButton_Pinyin_X->setText("x");
        ui->pushButton_Pinyin_C->setText("c");
        ui->pushButton_Pinyin_V->setText("v");
        ui->pushButton_Pinyin_B->setText("b");
        ui->pushButton_Pinyin_N->setText("n");
        ui->pushButton_Pinyin_M->setText("m");
    }
}

/****************************** 手写键盘 **********************************/
void Keyboard::on_pushButton_Hand_Language_clicked()
{
    if(ui->pushButton_Hand_Language->text() == "中文" || G_LANGUAGE_CHINESE != CGlobal::language)  // NOTE: 非中文语言时，禁用切换到中文输入法的功能
    {
        // 切换到英文输入状态
        LanguageItem = 1;
        ui->pushButton_Hand_Language->setText("En");
        ui->pushButton_Hand_Comma->setText(",");
        ui->pushButton_Hand_Period->setText(".");
        hwBoard->setLanguage(HandWriteBoard::English);
    }
    else if(ui->pushButton_Hand_Language->text() == "En")
    {
        // 切换到中文输入状态
        LanguageItem = 0;
        ui->pushButton_Hand_Language->setText("中文");
        ui->pushButton_Hand_Comma->setText("，");
        ui->pushButton_Hand_Period->setText("。");
        hwBoard->setLanguage(HandWriteBoard::Chinese);
    }
}

void Keyboard::Hand_Reset_clicked()
{
    if(!inputMethodIntf->reset())
        ui->label_PinYin->clear();
}

void Keyboard::recognizeResultSlot(const QStringList &list)
{
    //qDebug() << "got candidate_list.count() = " << list.count();
    //显示手写结果
    ChineseWidget->setText("");     //添加之前先清空
    for(int i = 0; i < list.count(); i++)
    {
        //bts[i]->setText(list.at(i));
        ChineseWidget->addOneItem(list.at(i));    //添加到候选栏
        //qDebug() << list.at(i);
    }
}

void Keyboard::updateLanguageButtonStyle()
{
    static const QString STYLE_DEFAULT = "QPushButton { border-radius: 3px; background-color: rgb(180,180,180); color: rgb(1,1,1); } ";
    static const QString STYLE_SELECTED = "QPushButton { border-radius: 3px; background-color:rgb(200,200,200); color:rgb(1,1,1); border: 2px solid rgb(85, 87, 83); } ";
    static QFont font = ui->pushButton_English->font();
    static QFont font_bold = ui->pushButton_English->font();
    font_bold.setBold(true);

    int language_stat = (NumKeyboardFlag ? PrevIndexWidget : ui->stackedWidget_Key->currentIndex());

    ui->pushButton_English->setStyleSheet((0 == language_stat && 1 == LanguageItem) ? STYLE_SELECTED : STYLE_DEFAULT);
    ui->pushButton_English->setFont((0 == language_stat && 1 == LanguageItem) ? font_bold : font);

    ui->pushButton_PinyinMode->setStyleSheet((0 == language_stat && 0 == LanguageItem) ? STYLE_SELECTED : STYLE_DEFAULT);
    ui->pushButton_PinyinMode->setFont((0 == language_stat && 0 == LanguageItem) ? font_bold : font);

    ui->pushButton_HandMode->setStyleSheet((1 == language_stat) ? STYLE_SELECTED : STYLE_DEFAULT);
    ui->pushButton_HandMode->setFont((1 == language_stat) ? font_bold : font);

}

/****************************** 笔画键盘 **********************************/
void Keyboard::on_pushButton_Strokes_Language_clicked()
{
    if(ui->pushButton_Strokes_Language->text() == "中文" || G_LANGUAGE_CHINESE != CGlobal::language)  // NOTE: 非中文语言时，禁用切换到中文输入法的功能
    {
        // 切换到英文输入状态
        LanguageItem = 1;
        ui->pushButton_Strokes_Language->setText("En");
        ui->pushButton_Strokes_Comma->setText(",");
        ui->pushButton_Strokes_Period->setText(".");
        ui->pushButton_Strokes_Colon->setText(":");
        ui->pushButton_Semicolon->setText(";");
    }
    else if(ui->pushButton_Strokes_Language->text() == "En")
    {
        // 切换到中文输入状态
        LanguageItem = 0;
        ui->pushButton_Strokes_Language->setText("中文");
        ui->pushButton_Strokes_Comma->setText("，");
        ui->pushButton_Strokes_Period->setText("。");
        ui->pushButton_Strokes_Colon->setText("：");
        ui->pushButton_Semicolon->setText("；");
    }
}

//重输
void Keyboard::on_pushButton_Strokes_Rewrite_clicked()
{
    if(!inputMethodIntf->reset())
    {
        ui->label_PinYin->clear();
        //qDebug()<<"-----Reset ok";
    }
}

/****************************** 标点键盘 **********************************/
//全角、半角
void Keyboard::on_pushButton_Symbol_SBC_clicked()
{
    if (isDBC || G_LANGUAGE_CHINESE != CGlobal::language)  // NOTE: 非中文语言时，禁用切换到全角符号的功能
    {
        // 切换到半角符号输入状态
        isDBC = false;

        ui->pushButton_Symbol_SBC->setText(tr("半角"));   // "SBC" ("Single Byte Character" ? or "Half-width Character" ?)
        ui->pushButton_Symbol_1->setText(",");
        ui->pushButton_Symbol_2->setText(".");
        ui->pushButton_Symbol_3->setText("!");
        ui->pushButton_Symbol_4->setText("?");
        ui->pushButton_Symbol_5->setText(":");
        ui->pushButton_Symbol_6->setText(";");
        ui->pushButton_Symbol_7->setText("'");
        ui->pushButton_Symbol_8->setText("\"");
        ui->pushButton_Symbol_9->setText("[");
        ui->pushButton_Symbol_10->setText("]");
        ui->pushButton_Symbol_13->setText("<");
        ui->pushButton_Symbol_14->setText(">");
        ui->pushButton_Symbol_15->setText("/");
        ui->pushButton_Symbol_20->setText("$");
        ui->pushButton_Symbol_22->setText("^");
        ui->pushButton_Symbol_24->setText("*");
        ui->pushButton_Symbol_25->setText("(");
        ui->pushButton_Symbol_26->setText(")");
        ui->pushButton_Symbol_27->setText("_");

    }
    else
    {
        // 切换到全角符号输入状态
        isDBC = true;

        ui->pushButton_Symbol_SBC->setText(tr("全角"));   // "DBC" ("Double Byte Character" ? or "Full-width Character" ?)
        ui->pushButton_Symbol_1->setText("，");
        ui->pushButton_Symbol_2->setText("。");
        ui->pushButton_Symbol_3->setText("！");
        ui->pushButton_Symbol_4->setText("？");
        ui->pushButton_Symbol_5->setText("：");
        ui->pushButton_Symbol_6->setText("；");
        ui->pushButton_Symbol_7->setText("‘’");
        ui->pushButton_Symbol_8->setText("“”");
        ui->pushButton_Symbol_9->setText("【");
        ui->pushButton_Symbol_10->setText("】");
        ui->pushButton_Symbol_13->setText("《");
        ui->pushButton_Symbol_14->setText("》");
        ui->pushButton_Symbol_15->setText("\\");
        ui->pushButton_Symbol_20->setText("¥");
        ui->pushButton_Symbol_22->setText("……");
        ui->pushButton_Symbol_24->setText("×");
        ui->pushButton_Symbol_25->setText("（");
        ui->pushButton_Symbol_26->setText("）");
        ui->pushButton_Symbol_27->setText("--");
    }
}

//标点键盘返回
void Keyboard::on_pushButton_Symbol_Back_clicked()
{
    if(NumKeyboardFlag)
    {
        ui->stackedWidget_Key->setCurrentIndex(3);  //进入数字键盘页面
        return;
    }
    ui->stackedWidget_Key->setCurrentIndex(PrevIndexWidget);  //返回上一层页面
}


/****************************** 数字键盘 **********************************/
void Keyboard::on_pushButton_Num_More_clicked()
{
    ui->stackedWidget_Key->setCurrentIndex(4);  //进入标点符号页面
}

//数字键盘返回
void Keyboard::on_pushButton_Num_Back_clicked()
{
    NumKeyboardFlag = false;    //退出数字键盘标志
    ui->stackedWidget_Key->setCurrentIndex(PrevIndexWidget);  //返回上一层页面
}

//公共槽函数(进入符号键盘)
void Keyboard::Slot_Enter_into_Symbol()
{
    PrevIndexWidget = ui->stackedWidget_Key->currentIndex(); //记录当前页面
    ui->stackedWidget_Key->setCurrentIndex(4);  //进入标点符号页面
}

//公共槽函数(进入数字键盘)
void Keyboard::Slot_Enter_into_Num()
{
    //
    NumKeyboardFlag = true;     //进入数字键盘标志
    PrevIndexWidget = ui->stackedWidget_Key->currentIndex(); //记录当前页面
    ui->stackedWidget_Key->setCurrentIndex(3);  //进入数字键盘页面

    // 清空候选列表
    ChineseWidget->clear();
    ui->label_PinYin->clear();
}

void Keyboard::Slot_Backspace_pressed_State()
{
    //qDebug()<<"-----Delete";
    if(inputMethodIntf->getInputs().length() > 0)
    {
        inputMethodIntf->inputKey(inputKey_Back);
        QStringList cand_list;
        inputMethodIntf->getCandidates(cand_list);
        updateCand(cand_list);
    }
    else /*if(ui->lineEdit_Input->hasFocus())*/ {
        ui->lineEdit_Input->backspace();
    }
    DeleteCharCount = 0;
    TimerDel->start(200);
}

void Keyboard::Slot_Backspace_released_State()
{
    TimerDel->stop();
}

void Keyboard::Slot_Delete_Char()
{
    DeleteCharCount++;
    if(DeleteCharCount == 5)
        TimerDel->start(100);
    if(inputMethodIntf->getInputs().length() > 0)
    {
        inputMethodIntf->inputKey(inputKey_Back);
        QStringList cand_list;
        inputMethodIntf->getCandidates(cand_list);
        updateCand(cand_list);
    }
    else /*if(ui->lineEdit_Input->hasFocus())*/ {
        ui->lineEdit_Input->backspace();
    }
}

//公共槽函数(回车按钮)
void Keyboard::Slot_Back_Enter_clicked()
{
    // 通常是输出输入串
    //...

    // 后重置
    inputMethodIntf->inputKey(inputKey_Back);
    QStringList cand_list;
    inputMethodIntf->getCandidates(cand_list);
    updateCand(cand_list);

    emitSendText();
    //this->parentWidget()->show();
    //this->hide();
}

/************************************************************************/
//更新候选词
void Keyboard::updateCand(QStringList &_candidate_list)
{
    //QPushButton * bts[] = {ui->cand1, ui->cand2, ui->cand3, ui->cand4, ui->cand5, ui->cand6, ui->cand7, ui->cand8, ui->cand9, ui->cand10};
    ChineseWidget->clear();

    int count = _candidate_list.count();
    for(int i = 0; i < count; i++) {
        ChineseWidget->addOneItem(_candidate_list[i]);    //添加到候选栏
    }

//    for(i; i < count; i++)
//    {
//        //bts[i]->setText("");
//        ChineseWidget->addOneItem("");    //添加到候选栏
//    }

    QString input_str = inputMethodIntf->getInputs();
    if (input_str.length() > 0)    //查询当前是否有按键输入到引擎里
    {
        ui->label_PinYin->setText(inputMethodIntf->getInputs());
    }
    else
    {
        ui->label_PinYin->setText("");
    }
}

void Keyboard::selCand(int i)
{
    QStringList cand_list;
    inputMethodIntf->selectCandidate(i, &cand_list);

    ui->lineEdit_Input->setText(inputMethodIntf->getInputs());

    updateCand(cand_list);
}

//拼音获取
void Keyboard::keyInput(QString &s)
{
    if(ui->stackedWidget_Key->currentIndex()==0)    //键盘界面pushButton text获取
    {
        if(LanguageItem == 0)       //输入状态为中文
        {
            if (s[0] >= 'a' && s[0] <= 'z')
            {
                inputMethodIntf->inputKey((enInputKey)s[0].unicode());
                QStringList cand_list;
                inputMethodIntf->getCandidates(cand_list);
                updateCand(cand_list);
            }
            else if(s[0] >= 'A' && s[0] <= 'Z')
            {
                QString ch = s.toLower();
                inputMethodIntf->inputKey((enInputKey)ch[0].unicode());
                QStringList cand_list;
                inputMethodIntf->getCandidates(cand_list);
                updateCand(cand_list);
            }
            else if(s=="，" || s=="。" || s==" ")   //中文状态下,键盘的两个符号和空格不能过滤
                ui->lineEdit_Input->insert(s);
            else if(s == "分词")
            {
                inputMethodIntf->inputKey(inputKey_SEPARATOR);
                QStringList cand_list;
                inputMethodIntf->getCandidates(cand_list);
                updateCand(cand_list);
            }
        }
        else if(LanguageItem == 1)  //输入状态为英文
            ui->lineEdit_Input->insert(s);
    }
    else if(ui->stackedWidget_Key->currentIndex()==1)   //手写界面pushButton text获取
    {
        if(LanguageItem == 0)       //输入状态为中文
        {
            if(s=="，" || s=="。" || s==" ")
                ui->lineEdit_Input->insert(s);
        }
        else if(LanguageItem == 1)  //输入状态为英文
            ui->lineEdit_Input->insert(s);
    }
    else if(ui->stackedWidget_Key->currentIndex()==2)   //笔画界面pushButton text获取
    {
        if(LanguageItem == 0)       //输入状态为中文
        {
            if(s=="，" || s=="。" || s=="？" || s=="：" || s=="；" || s=="0" || s==" ")
                ui->lineEdit_Input->insert(s);
            if(s>="1" && s<="5")    //笔画
            {
                inputMethodIntf->inputKey((enInputKey)s[0].unicode());
                QStringList cand_list;
                inputMethodIntf->getCandidates(cand_list);
                updateCand(cand_list);
            }
            else if(s >= "6")
            {
                if(s == "6")         //通配符
                    inputMethodIntf->inputKey(inputKey_WILDCHAR);
                else if(s == "7")    //通配符
                    inputMethodIntf->inputKey(inputKey_SEPARATOR);
                QStringList cand_list;
                inputMethodIntf->getCandidates(cand_list);
                updateCand(cand_list);
            }
        }
        else if(LanguageItem == 1)  //输入状态为英文
            ui->lineEdit_Input->insert(s);
    }
    else    //为字符,数字界面
    {
        if(s == "&&")
            ui->lineEdit_Input->insert("&");
        else
            ui->lineEdit_Input->insert(s);
    }
}

void Keyboard::onKeyPressed(int key, QString value)
{
    qDebug() << "key: " << key << "Value: " << value;
    ui->lineEdit_Input->insert(value);
}

//清空拼音
void Keyboard::clearBufferText()
{
    if(!inputMethodIntf->reset())
    {
        ui->label_PinYin->clear();
        ChineseWidget->setText("");
    }
}


/*------------------------------------------------ QLineEdit -----------------------------------------------------*/
//发送信号
void Keyboard::emitSendText()
{
    QString text = ui->lineEdit_Input->text();
    qDebug()<<"sendTextSlot,sendtext:"<<text;

    emit sendText(text);
}

void Keyboard::setInputFocus()
{

}

void Keyboard::showKeyBoard(QString &&_old_str)
{
    ui->lineEdit_Input->setText(_old_str);

    CBaseFormIntf::centerWidget(this);
    //this->show();
    //this->raise();
    this->exec();
}

void Keyboard::clearText()
{
    ui->lineEdit_Input->clear();
    ui->label_PinYin->clear();
    ChineseWidget->setText("");
    inputMethodIntf->reset();   //退出界面重置输入法
}

QString Keyboard::getText()
{
    return ui->lineEdit_Input->text();
}


/*------------------------------------------------ QListWidget -----------------------------------------------------*/
MyListWidget::MyListWidget(QWidget *parent) :
    QListWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setViewMode(QListView::ListMode);   //设置为列表显示模式
    setFlow(QListView::LeftToRight);    //从左往右排列
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   //屏蔽水平滑动条
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);     //屏蔽垂直滑动条
    setHorizontalScrollMode(QListWidget::ScrollPerPixel);   //设置为像素滚动
    QScroller::grabGesture(this->viewport(), QScroller::LeftMouseButtonGesture);    //设置鼠标左键拖动

    connect(this, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(onItemClicked(QListWidgetItem *)));
}

//2020.7.29  tao
void MyListWidget::setText(const QString &text)
{
    int cnt=0;
    for (int i = 0; i < count(); i++) {
        QListWidgetItem *item = takeItem(i);
        delete item;
        item = NULL;
    }

    clear();

    addOneItem(text);
    //qDebug()<<"--text:"<<text;
    if (! m_data.contains(text.left(1))) {
        return;
    }
    if(text.size()==1)  //打第一个字母情况下查找本地词库
    {
        /* 通过获取首字母索引词库内容，用于加快匹配词(组)。 */
        const QList<QPair<QString, QString>> &tmp = m_data[text.left(1)];
        for(const QPair<QString, QString> &each : tmp){
            if (each.first.left(text.count()) != text)  //模糊匹配
                continue;
            addOneItem(each.second);    //添加到候选栏
            cnt++;
            if(cnt>1000) //这里加限制,不然全部遍历词库需要时间,1000是根据最多汉字的字母s而定(s开头简体字有940个)
                break;
        }
    }
    else    //组合或全拼情况下的查找本地词库
    {
        /* 通过获取首字母索引词库内容，用于加快匹配词(组)。 */
        const QList<QPair<QString, QString>> &tmp = m_data[text.left(1)];
        for(const QPair<QString, QString> &each : tmp){
            if (each.first.left(text.count()) != text)  //模糊匹配
                continue;
            addOneItem(each.second);    //添加到候选栏
        }
    }
}

void MyListWidget::onItemClicked(QListWidgetItem *item)
{
    emit pressedChanged(-1, item->text());
    setText("");
}

void MyListWidget::addOneItem(const QString &text)
{
    QListWidgetItem *item = new QListWidgetItem(text, this);
    QFont font;
    font.setPointSize(18);
    font.setBold(true);
    font.setWeight(50);
    item->setFont(font);

    item->setTextAlignment(Qt::AlignCenter);      //设置文字居中
    bool is_chinese = QRegExp("^[\u4E00-\u9FA5]+").indexIn(text.left(1)) != -1;  //检测Item项的文字查是否为中文

    int width = font.pointSize();       //width:18
    if (is_chinese)
        width += text.count()*font.pointSize()*1.5;
    else{
        width += text.count()*14;
        //qDebug()<<"--width:"<<width<<"  count:"<<text.count();
    }

    item->setSizeHint(QSize(width, 50));
    addItem(item);
}

void Keyboard::on_lineEdit_Input_returnPressed()
{
    Slot_Back_Enter_clicked();
}
