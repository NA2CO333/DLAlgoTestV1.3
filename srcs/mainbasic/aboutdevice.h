#ifndef ABOUTDEVICE_H
#define ABOUTDEVICE_H

#include <QWidget>

#include "baseform.h"
#include "statusbarform.h"
#include "util-common.h"
#include "remote-service/remoteservice.h"
#include "winlog.h"
#include "globalclass.h"

//
namespace Ui {
class aboutdevice;
}

class aboutdevice : public CBaseWidget
{
    Q_OBJECT

public:
    explicit aboutdevice(QWidget *parent = 0);
    ~aboutdevice();

    static QString getAppVerBase();         // 本程序的 基本版本号（即前三段）
    static QString getAppVerDate();         // 本程序的 版本日期
    static QString getAppVerFull();         // 本程序的 完整 版本号（基本版本号 + 版本日期）
    static QString getAppVerBuild();        // 本程序的 编译号
    static QString getAppVerAll();          // 本程序的 全部版本（完整版本号 + 编译号）
    static QString getVerStrForReg();       // 本程序的 产品注册所用格式的 版本号

    static void sendQueryStm32Version();                                                // 发送下位机版本查询指令

    static void setStm32Version(int _ver_major, int _ver_minor, int _ver_patch);        // 设置底板单片机程序版本号
    static void getStm32Version(int &_ver_major, int &_ver_minor, int &_ver_patch);     // 获得底板单片机程序版本号
    static const QString &getStm32VersionStr();                                         // 获得底板单片机程序版本号字符串

    static stVerInfoApp getAppVerInfoOfCode();                                          // 获得【代码中的版本信息】

    /**
     * @brief 版本字符串转为版本信息结构体
     * @param _ver_str
     * @param _ver_info
     * @param _fail_val : 若转换失败，得到的值。-1：无，0：0值，1：当前代码版本号
     * @return 是否成功
     */
    static bool strToVersionInfo(const QString &_ver_str, stVerInfoApp &_ver_info, int _fail_val = 0);
    static QString versionInfoToStr(const stVerInfoApp &_ver_info);
protected:
    void showEvent(QShowEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

    void setIsRemoteServiceOn(bool _is_on);                 // 设置远程服务开关状态
    void doSetIsRemoteServiceOn(bool _is_on, QString _svr_host, int _svr_port, QString _svr_socket_path, bool _is_https, QString _svr_upload_path, QString _dev_num);

    void setRemoteSvcLogVisible(bool _is_visible);

    Net::Remote::CRemoteService *remoteService = Q_NULLPTR;
    WinLog *winLog = Q_NULLPTR;

private slots:
    void on_pushButton_Back_clicked();
    void on_btnDataBackup_clicked();
    void on_btnDataRestore_clicked();
    void on_sbtnRemoteSvc_clicked();
    void on_btnRemoteSvcLogs_clicked();
private:
    Ui::aboutdevice *ui;
};

#endif // ABOUTDEVICE_H
