#ifndef WINMULTIRESULT_H
#define WINMULTIRESULT_H

#include <QWidget>

#include "baseform.h"
#include "data.h"

namespace Ui {
class WinMultiResults;
}

// 多次测量的显示窗口
class WinMultiResults : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinMultiResults(QWidget *_parent = nullptr);
    ~WinMultiResults();

    void setData(const CPatient &_pat, bool _has_right, bool _has_left, bool _is_cyl_negative);

protected:
    void showEvent(QShowEvent *);

    void updateTheme(enThemeType _theme);                               // 更新主题样式

private:
    Ui::WinMultiResults *ui;
};

#endif // WINMULTIRESULT_H
