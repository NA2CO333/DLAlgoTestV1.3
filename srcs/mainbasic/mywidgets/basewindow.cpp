#include "basewindow.h"
#include "global.h"

#include <QApplication>

#include "winmanage.h"

//
CBaseWindow *CBaseWindow::instance = Q_NULLPTR;

CBaseWindow::CBaseWindow(QWidget *parent, Qt::WindowFlags flags) : CBaseMainWindow(parent, flags)
{
    this->setObjectName("BaseWindow");
    this->setWindowTitle("Screener");

#if (2 == OS_TYPE)
    this->setWindowFlag(Qt::FramelessWindowHint, false);
#endif

}

CBaseWindow *CBaseWindow::getInstance()
{
    if (!instance) {
        instance = new CBaseWindow;
    }
    return instance;
}

void CBaseWindow::setScreenSize(int _width, int _height)
{
    this->screenWidth = _width;
    this->screenHeight = _height;

    //
    this->setGeometry(0, 0, screenWidth, screenHeight);
    CBaseFormIntf::centerWidget(this);

}

void CBaseWindow::setTheme(enThemeType _theme)          // TODO: 应显示可区分于系统背景和黑屏的画面，易于分辨故障
{
    //
    themeType = _theme;

    //
    QPalette palette = this->palette();
    if (themeType_Black == _theme) {      //黑色主题
        palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));
        //palette.setColor(this->backgroundRole(), QColor(1, 1, 1));
    } else {
        palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));
        //palette.setColor(this->backgroundRole(), QColor(242, 242, 247));
    }
    this->setPalette(palette);
}

enThemeType CBaseWindow::getTheme()
{
    return themeType;
}

void CBaseWindow::showEvent(QShowEvent *_evt)
{
    //
    QMainWindow::showEvent(_evt);

    //
    assert(screenWidth > 0 && screenHeight > 0);

    //
    this->setTheme(getSysThemeType());       // TODO: Update 窗体未完成黑色样式，如果这里设置了黑色，Update 窗体的文字看不见了

}

void CBaseWindow::hideEvent(QHideEvent *_evt)
{
    //
    QMainWindow::hideEvent(_evt);

    // 关闭所有未关闭的子窗口          // TODO: 应该在 Base::hideEvent() 前执行？
    getWinManage()->hideAllChildren();
}

