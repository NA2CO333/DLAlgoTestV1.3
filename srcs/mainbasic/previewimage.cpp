//显示图片(图片预览)
#include "previewimage.h"
#include "ui_previewimage.h"

#include "windowsmanager.h"
#include "global.h"

//
extern QString PhotoPath1; //结果图片2路径
extern QString PhotoPath2; //结果图片3路径
bool m_bStarted = false;

//
previewimage::previewimage(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::previewimage)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    bigfalse = true;    //结果图片放大标志
    smallflag = true;   //结果图片缩小标志

    ui->graphicsView_1->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); //取消水平滚动条
    ui->graphicsView_1->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   //取消垂直滚动条
    ui->graphicsView_2->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView_2->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    //
    m_ImageWidget = new ImageWidget;  //实例化类ImageWidget的对象，该类继承自QGraphicsItem，是自己写的类
    QObject::connect(this, &previewimage::sigChangeZoom, m_ImageWidget, &ImageWidget::slotChangeZoom);
    QObject::connect(m_ImageWidget, &ImageWidget::sigUserTurnPage, this, &previewimage::slot_ImageWidget_UserTurnPage);

    //
    zoomTimer = new QTimer;
    QObject::connect(zoomTimer, &QTimer::timeout, this, &previewimage::slot_zoomTimer_timeout);      //用于连续缩放
    zoomTimer->stop();

    //
    ui->verticalSlider->hide();

    // 将“100%”按钮移到放大和缩小按钮中间
    QRect rect_origin(ui->btnOriginSize->geometry());
    rect_origin.moveTo(ui->pushButton_big->x(), (ui->pushButton_big->y() + ui->pushButton_small->y()) / 2);
    ui->btnOriginSize->setGeometry(rect_origin);

}

previewimage::~previewimage()
{
    delete ui;
}

void previewimage::showEvent(QShowEvent *)
{
    //更新主题
    //QPalette palette;
    if(themeType_Black == getSysThemeType()){
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
    }
    else{
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
    }
    //this->setPalette(palette);

    //if (language) {
    //    ui->label_Back->setText("返回");
    //} else {
    //    ui->label_Back->setText("Back");
    //}

    getWinManage()->updateWindowTitle(this, tr("图像预览"));    // "Image Preview"

    ui->stackedWidget->setCurrentIndex(0);  //图片预览切换到界面0
    m_bStarted = true;
    showImage(0);
    m_ImageWidget->ResetItemPos();

}

void previewimage::closeEvent(QCloseEvent *)
{
    m_bStarted = false;

}

void previewimage::slot_ImageWidget_UserTurnPage(bool _is_toward_left)
{
    qDebug() << "主界面滑页: _is_toward_left = " << Util::bool2str(_is_toward_left);
    int page = -1;
    //判断滑动方向（右滑，页数减1）
    if (!_is_toward_left)    //x轴滑动距离大于50像素,y轴滑动距离小于100像素
    {
        int current_page = ui->stackedWidget->currentIndex();   //获取stackedWidget当前窗口索引
        if(current_page == 1)
        {
            page = 0;
            ui->stackedWidget->setCurrentIndex(page);  //切换界面
            QPropertyAnimation *animation = new QPropertyAnimation(ui->stackedWidget->currentWidget(),"geometry");     //捕获当前界面并绘制到stackedWidget上
            animation->setDuration(300);    //设置动画时间为400毫秒
            animation->setStartValue(QRect(-this->width()*2,0,this->width(),this->height()));
            animation->setEndValue(QRect(0,0,this->width(),this->height()));
            QParallelAnimationGroup *group = new QParallelAnimationGroup;  //动画容器
            group->addAnimation(animation);
            group->start();
        }
    }
    //判断滑动方向（左滑，页数加1）
    else
    {
        int current_page = ui->stackedWidget->currentIndex();
        if(current_page == 0)
        {
            page = 1;
            ui->stackedWidget->setCurrentIndex(page);
            QPropertyAnimation *animation = new QPropertyAnimation(ui->stackedWidget->currentWidget(),"geometry");
            animation->setDuration(300);
            animation->setStartValue(QRect(this->width()*2,0,this->width(),this->height()));
            animation->setEndValue(QRect(0,0,this->width(),this->height()));
            QParallelAnimationGroup *group = new QParallelAnimationGroup;
            group->addAnimation(animation);
            group->start();
        }
    }
    if (page >= 0) {
        showImage(page);
    }

    //
    ui->btnHistotram->setChecked(false);
}

void previewimage::showImage(int _page, bool _is_equalize_hist)
{
    if (_page == 0 && Result::ultimateDirCount > 0 && Result::ultimateFileCount >= 1) {       //显示第二张图片
        if (!_is_equalize_hist) {
            QImage image(PhotoPath1);
            QPixmap ConvertPixmap=QPixmap::fromImage(image);//QPixmap类是一个可以用作绘制设备的屏幕外图像表示
            m_ImageWidget->setPixmap(&ConvertPixmap);
        } else {
            QPixmap pixmap = CAlgoInvoker::readAndEqualizeHistFromFileToPixmap(PhotoPath1);
            m_ImageWidget->setPixmap(&pixmap);
        }
        int nwith = ui->graphicsView_1->width();//获取界面控件Graphics View的宽度
        int nheight = ui->graphicsView_1->height();//获取界面控件Graphics View的高度
        m_ImageWidget->setQGraphicsViewWH(nwith,nheight);//将界面控件Graphics View的width和height传进 ImageWidget 类 中
        QGraphicsScene *qgraphicsScene1 = new QGraphicsScene;
        qgraphicsScene1->addItem(m_ImageWidget);//将QGraphicsItem类对象放进QGraphicsScene1中
        ui->graphicsView_1->setSceneRect(QRectF(-(nwith/2),-(nheight/2),nwith,nheight));//使视窗的大小固定在原始大小，不会随图片的放大而放大（默认状态下图片放大的时候视窗两边会自动出现滚动条，并且视窗内的视野会变大），防止图片放大后重新缩小的时候视窗太大而不方便观察图片
        ui->graphicsView_1->setScene(qgraphicsScene1);//将当前场景设置为场景。如果场景已经被查看，这个函数不做任何事情。
        ui->graphicsView_1->setFocus();   //将界面的焦点设置到当前Graphics View控件
    }
    else if (_page == 1 && Result::ultimateDirCount > 0 && Result::ultimateFileCount >= 2) {       //显示第三张图片
        if (!_is_equalize_hist) {
            QImage image(PhotoPath2);
            QPixmap ConvertPixmap=QPixmap::fromImage(image);//QPixmap类是一个可以用作绘制设备的屏幕外图像表示
            m_ImageWidget->setPixmap(&ConvertPixmap);
        } else {
            cv::Mat mat = cv::imread(PhotoPath1.toStdString(), cv::IMREAD_GRAYSCALE);
            cv::Mat mat_2;
            cv::equalizeHist(mat, mat_2);
            QPixmap pixmap = CAlgoInvoker::matToPixmap(mat_2);
            m_ImageWidget->setPixmap(&pixmap);
        }
        int nwith = ui->graphicsView_2->width();//获取界面控件Graphics View的宽度
        int nheight = ui->graphicsView_2->height();//获取界面控件Graphics View的高度
        m_ImageWidget->setQGraphicsViewWH(nwith,nheight);//将界面控件Graphics View的width和height传进 ImageWidget 类中
        QGraphicsScene *qgraphicsScene2 = new QGraphicsScene;
        qgraphicsScene2->addItem(m_ImageWidget);//将QGraphicsItem类对象放进QGraphicsScene2中
        ui->graphicsView_2->setSceneRect(QRectF(-(nwith/2),-(nheight/2),nwith,nheight));//使视窗的大小固定在原始大小，不会随图片的放大而放大（默认状态下图片放大的时候视窗两边会自动出现滚动条，并且视窗内的视野会变大），防止图片放大后重新缩小的时候视窗太大而不方便观察图片
        ui->graphicsView_2->setScene(qgraphicsScene2);//将当前场景设置为场景。如果场景已经被查看，这个函数不做任何事情。
        ui->graphicsView_2->setFocus();   //将界面的焦点设置到当前Graphics View控件
    }
}

// 放大点击信号
void previewimage::on_pushButton_big_pressed()
{
    zoomValue = 120;
    zoomTimer->start(150);
}

void previewimage::on_pushButton_big_released()
{
    zoomTimer->stop();
}

void previewimage::on_pushButton_big_clicked()
{
    emit sigChangeZoom(zoomValue);
}

// 缩小点击信号
void previewimage::on_pushButton_small_pressed()
{
    zoomValue = -120;
    zoomTimer->start(150);
}

void previewimage::on_pushButton_small_released()
{
    zoomTimer->stop();
}

void previewimage::on_pushButton_small_clicked()
{
    emit sigChangeZoom(zoomValue);
}

// “100%”按钮点击
void previewimage::on_btnOriginSize_clicked()
{
    // 恢复图像视图为原始尺寸
    //emit sigChangeZoom(0);
    m_ImageWidget->ResetItemPos();
}

//连续缩放
void previewimage::slot_zoomTimer_timeout()
{
    emit sigChangeZoom(zoomValue);
}

//返回
void previewimage::on_pushButton_Back_clicked()
{
    getWinManage()->backToLastWidget();
}

//未用到
void previewimage::on_verticalSlider_valueChanged(int value)
{
    //qDebug()<<"-----value:"<<value;
    emit sigChangeZoom(value);
}

void previewimage::on_btnHistotram_clicked(bool _checked)
{
    int current_page = ui->stackedWidget->currentIndex();
    showImage(current_page, _checked);
}
