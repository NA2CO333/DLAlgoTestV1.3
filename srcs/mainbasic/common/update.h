#ifndef CUPDATE_H
#define CUPDATE_H

#include <QObject>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QTimer>
#include <QEventLoop>
#include <QSettings>
#include <QThread>

//
#if (OS_TYPE == 1)
// i.MX6Q 平台
#  define DEFAULT_UPDATE_PATH "http://download.manylinksmed.com/screener/1.5"
#else
// rk3568 平台 和 PC 桌面
#  define DEFAULT_UPDATE_PATH "http://download.manylinksmed.com/screener2/1.5"
#endif

// 前置声明
class QNetworkAccessManager;

//
enum CheckState {
    UpdateAvailable,            // 有可用更新
    NoUpdateAvailable,          // 无可用更新
    BadServer,
    networkUnreachable,
    NoNetwork,
    HostNotFoundError,
    InfoFileFormatError,
};

//
class UpdateOperation : public QObject
{
    Q_OBJECT

public:
    explicit UpdateOperation(QNetworkAccessManager *_net_manager, QObject *_parent = 0);
    ~UpdateOperation();
    //bool isNetworkReachable();

public slots:
    void slotCheckUpdate(QString _server);

signals:
    void sigCheckUpdateResult(CheckState);              // 检查更新的结果状态

private:
    QNetworkAccessManager *m_netManager {nullptr};
    QEventLoop *m_eventLoop {nullptr};
    QTimer *m_timer {nullptr};
    bool m_isInstantiate {false};     // 是否已实例化
};

// 软件升级模块
class CUpdate : public QObject
{
    Q_OBJECT

public:
    explicit CUpdate(QObject *_parent = nullptr);
    ~CUpdate();

    static void setNetworkAccessManager(QNetworkAccessManager *_net_manager);

    void updateLater();                             // 定时 5ms 后执行更新     // TODO: 这函数没用？
    bool isSuccess();                               // 更新是否成功           // TODO: 这函数没用？

signals:
    void sigShowProgress(bool _is_shown);                   // 显示进度窗口       // TODO: 改为别的意义？比如开始和结束？
    void sigCurrentFileChanged(QString _file_path);         // 当前文件改变事件
    void sigProgressChanged(int _current, int _total);      // 进度改变事件，-1 表示不变
    void sigSpeedChanged(double _speed, int _unit);         // 下载速度事件，单位：0 B/s，1 KB/s，2 MB/s
    void sigRebootSystem();                                 // 重启系统

    void setAutoCheckUpdate(bool _auto_check);      // TODO: 结构优化，判断及设置是否自动检查的代码不应该在本模块内部

    /* 私有 */
    void sigCheckUpdate(QString _service_addr);             // 检查更新（指定地址）

public slots:
    void slotUpdateAddressChanged(QString _new_addr);       // “更新地址”改变事件
    void slotAutoCheckUpdateChanged(bool _is_auto_check);   // “是否自动检查更新”改变事件
    void slotCheckUpdate();                                 // “检查更新”槽函数
    void slotCancelUpdate();                                // “取消更新”槽函数

protected slots:
    void slot_autoUpdateTimer_timeout();

    void slotCheckUpdateResult(CheckState);                 // “检查更新的结果状态”槽函数
    void slotRunUpdate();                                   // （完整的更新过程，不含检查）

protected:
    bool runUpdate();                                       // 执行更新（在当前线程）

    static QNetworkAccessManager *s_netManager;

    QTimer *m_timer;                          // 事件循环定时器
    QEventLoop *m_eventLoop;
    QTimer *m_autoUpdateTimer;                // 定时检查更新所用定时器

    QString m_server;
    bool m_updateState = false;       // 更新的执行成功状态
    bool m_isCancel = false;

    UpdateOperation *m_operation {nullptr};
    QThread *m_workThread {nullptr};

    enum UpdateAction
    {
        AutoCheck,
        ManualCheck
    };

    UpdateAction m_action;    /* 区分是程序自动检查更新还是用户手动检查更新，以确定是否需要弹出提示 */

};

///=============================================================================================================
/// extern variable

extern CUpdate *g_update;

extern bool update_flag;   //检测更新按键标志，防止出现重复弹窗     // TODO: 清掉

#endif // CUPDATE_H
