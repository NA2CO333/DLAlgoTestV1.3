#ifndef WINUPDATEPROGRESS_H
#define WINUPDATEPROGRESS_H

#include "baseform.h"

namespace Ui {
class WinUpdateProgress;
}

//
class WinUpdateProgress : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinUpdateProgress(QWidget *parent = 0);
    ~WinUpdateProgress();

public slots:
    void slotShowProgress(bool _is_shown);                   // 显示进度窗口       // TODO: 改为别的意义？比如开始和结束？
    void slotCurrentFileChanged(QString _file_path);         // 当前文件改变事件
    void slotProgressChanged(int _current, int _total);      // 进度改变事件，-1 表示不变
    void slotSpeedChanged(double _speed, int _unit);         // 下载速度事件，单位：0 B/s，1 KB/s，2 MB/s

protected slots:
    void on_pushButtonCancel_clicked();

signals:
    void sigCancelUpdate();

protected:
    void showEvent(QShowEvent *) override;

    void updateTheme(enThemeType _theme);

private:
    Ui::WinUpdateProgress *ui;
};

#endif // WINUPDATEPROGRESS_H
