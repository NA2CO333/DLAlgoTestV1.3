#include <QPainter>
#include <QDebug>

#include "handwriteboard.h"

// 手写识别范围
#define HW_RANGE_NUMBER				0x1         /* 识别范围：数字     */
#define HW_RANGE_LOWER_CHAR			0x2         /* 识别范围：小写字母 */
#define HW_RANGE_UPPER_CHAR			0x4         /* 识别范围：大写字母 */
#define HW_RANGE_ASC_PUNCTUATION		0x8         /* 识别范围：半角标点符号 */

#define HW_RANGE_CN					0x8600      /* 识别范围：汉字 */
#define HW_RANGE_CHN_PUNCTUATION		0x800       /* 识别范围：中文标点符号 */
#define HW_RANGE_CONTROL_CHAR			0x2000      /* 识别范围：空格、回车以及删除等三个控制字符 */

//
HandWriteBoard::HandWriteBoard(QWidget *parent, Qt::WindowFlags f) : QLabel(parent,f)
{
    init();
    HandClear = new QTimer;
    connect(HandClear,SIGNAL(timeout()),this,SLOT(SlotHandWritingClear()),Qt::QueuedConnection);
    HandClear->stop();
}

HandWriteBoard::HandWriteBoard(const QString &text, QWidget *parent, Qt::WindowFlags f) : QLabel(text,parent,f)
{
    init();
}

HandWriteBoard::~HandWriteBoard()
{
    //inputMethodIntf->close();
}

void HandWriteBoard::init()
{
    isWriting = false;
    lang = Chinese;
    memset(mResultStr, 0, sizeof(mResultStr));

    qDebug() << "sizeof" << sizeof(MyPoint);
}

void HandWriteBoard::clear()
{
    track.clear();
    update();
}

void HandWriteBoard::setLanguage(InputLanguage lang)
{
    this->lang = lang;
}

void HandWriteBoard::mousePressEvent(QMouseEvent *ev)
{
    track.append(MyPoint(ev->x(),ev->y()));
    isWriting = true;
}

void HandWriteBoard::mouseMoveEvent(QMouseEvent *ev)
{
    track.append(MyPoint(ev->x(),ev->y()));
    update();
}

void HandWriteBoard::mouseReleaseEvent(QMouseEvent *)
{
    // 加入笔划结束点
    track.append(MyPoint(-1,0));
    // 加入笔迹结束点
    track.append(MyPoint(-1,-1));

    // 识别
    const int MAX_RECOGNIZE_CNT = 20;

    int reg_cnt;
    if(lang == Chinese) {
        reg_cnt = inputMethodIntf->inputHandWrite((short *)track.data(), track.size(), mResultStr, MAX_RECOGNIZE_CNT, HW_RANGE_CN);
    } else {
        reg_cnt = inputMethodIntf->inputHandWrite((short *)track.data(), track.size(), mResultStr, MAX_RECOGNIZE_CNT, HW_RANGE_LOWER_CHAR | HW_RANGE_UPPER_CHAR);
    }

    // 显示结果
    QStringList list;
    for(int i=0; i < reg_cnt; i++)
    {
        const wchar_t ss[] = {mResultStr[i], 0};
        list.append(QString::fromWCharArray(ss));
    }
    //qDebug() << "candidate_list.count() = " << list.count();
    emit recognizeResult(list);

    // 弹出笔迹结束点
    track.pop_back();
    isWriting = false;

    HandClear->start(500);
    ClearCount = 2;     //计数2次(1s)后清空画板(实际上会慢的多)
}

void HandWriteBoard::paintEvent(QPaintEvent *)
{
    if(track.size() < 2) return;

    QPainter painter;
    painter.begin(this);
    QPen pen = painter.pen();
    pen.setWidth(2);
    pen.setColor(Qt::black);
    painter.setPen(pen);
    MyPoint prev = track.first();
    MyPoint curr;
    QVector<MyPoint>::iterator it = track.begin();
    for(it++; it!=track.end(); it++)
    {
        curr = *it;
        if(prev.x()!=-1 && curr.x()!=-1)
            painter.drawLine(prev.x(), prev.y(), curr.x(), curr.y());
        prev = curr;
    }
    painter.end();
}

void HandWriteBoard::SlotHandWritingClear()
{
    ClearCount--;   //计数
    if(ClearCount <= 0)
    {
        HandClear->stop();
        emit SendClearState();
        clear();
    }
}
