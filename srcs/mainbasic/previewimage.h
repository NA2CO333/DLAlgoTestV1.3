#ifndef PREVIEWIMAGE_H
#define PREVIEWIMAGE_H

#include <QWidget>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QLabel>
#include <QThread>
#include <QList>
#include <QMessageBox>
#include <QImage>
#include <QGraphicsScene>
#include <QObject>
#include <QWidget>

#include "baseform.h"
#include "statusbarform.h"
#include "mysqlitepatients.h"
#include "imagewidget.h"

namespace Ui {
class previewimage;
}

//
class previewimage : public CBaseWidget
{
    Q_OBJECT

public:
    explicit previewimage(QWidget *parent = 0);
    ~previewimage();

signals:
    void sigChangeZoom(int _zoom);

protected Q_SLOTS:
    void slot_ImageWidget_UserTurnPage(bool _is_toward_left);               // 用户翻页事件（待程序执行），_is_toward_left: 是否向左翻页（即页数+1）
    void slot_zoomTimer_timeout();

protected:
    void showEvent(QShowEvent*);
    void closeEvent(QCloseEvent *);

    void showImage(int _page, bool _is_equalize_hist = false);

    int press_x = 0;  //鼠标按下时的位置
    int press_y = 0;
    int relea_x = 0;  //鼠标释放时的位置
    int relea_y = 0;
    int zoomValue = 0;
    bool m_isAnimating;//动画正在运行
    bool bigfalse;
    bool smallflag;
//    QGraphicsScene *qgraphicsScene;
//    QGraphicsScene *qgraphicsScene1;
//    QGraphicsScene *qgraphicsScene2;
    ImageWidget *m_ImageWidget = Q_NULLPTR;
    QTimer *zoomTimer = Q_NULLPTR;    //用于连续缩放

private slots:
    void on_pushButton_Back_clicked();
    void on_pushButton_big_pressed();
    void on_pushButton_big_released();
    void on_pushButton_big_clicked();
    void on_pushButton_small_pressed();
    void on_pushButton_small_released();
    void on_pushButton_small_clicked();
    void on_verticalSlider_valueChanged(int value);
    void on_btnOriginSize_clicked();
    void on_btnHistotram_clicked(bool _checked);
private:
    Ui::previewimage *ui;
};

#endif // PREVIEWIMAGE_H
