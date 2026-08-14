#ifndef CBASEWINDOW_H
#define CBASEWINDOW_H

#include "baseform.h"

// 底窗体
class CBaseWindow : public CBaseMainWindow          // TODO: 须确保所有窗口都在此窗口内？比如弹出模式的密码对话框，再显示键盘窗口，键盘窗口就被密码修改框遮住
{
    Q_OBJECT
public:
    static CBaseWindow *getInstance();

    void setScreenSize(int _width, int _height);

    void setTheme(enThemeType _theme);
    enThemeType getTheme();

protected:
    void showEvent(QShowEvent *_evt) override;
    void hideEvent(QHideEvent *_evt) override;

    static CBaseWindow *instance;

    int screenWidth = -1;
    int screenHeight = -1;

    enThemeType themeType = themeType_Black;

    CBaseWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);

};

#endif // CBASEWINDOW_H
