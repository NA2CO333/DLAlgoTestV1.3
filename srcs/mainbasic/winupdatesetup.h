#ifndef WINUPDATESETUP_H
#define WINUPDATESETUP_H

#include <QWidget>

#include "baseform.h"
#include "data.h"

namespace Ui {
class WinUpdateSetup;
}

// 程序更新的设置
class WinUpdateSetup : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinUpdateSetup(QWidget *parent = nullptr);
    ~WinUpdateSetup();

    static QString getCfg_updateAddress();                      // 获取【更新服务的地址】的配置值
    static void setCfg_updateAddress(QString _url_str);         // 设置【更新服务的地址】的配置值

    static bool getCfg_autoCheckUpdate();                       // 获取【是否自动检查更新】的配置值
    static void setCfg_autoCheckUpdate(bool _auto_check);       // 获取【是否自动检查更新】的配置值

signals:
    void sigUpdateAddressChanged(QString _new_addr);            // 更新地址改变事件
    void sigAutoCheckUpdateChanged(bool _is_auto_check);        // 设置自动检查更新是否已启用
    void sigCheckUpdate();                                      // 检查更新一次

public slots:
    void setAutoCheckUpdate(bool _auto_check);

protected:
    void showEvent(QShowEvent *) override;

    enum UpdateAction {AutoCheck, ManualCheck};

    void updateUiValues();                                      // 更新 UI 控件的值
    void updateTheme(enThemeType _theme);                       // 更新主题

private slots:
    void on_sbtnIsAutoCheck_clicked();
    void on_btnNetworkUpdate_clicked();
    void on_btnHome_clicked();
    void on_btnBack_clicked();
    void on_edtServer_textChanged(const QString &arg1);
    void on_btnUDiskUpdate_clicked();
    void on_btnOtaUpdate_clicked();
private:
    Ui::WinUpdateSetup *ui;
};

#endif // WINUPDATESETUP_H
