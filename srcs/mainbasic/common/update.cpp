#include "update.h"

#include <QApplication>
#include <QtNetwork/QNetworkAccessManager>
#include <QProcess>

#include "wifiintf.h"

#include "noticewin.h"
#include "messagewin.h"
#include "updatedialog.h"
#include "global.h"

#include "winmeasure.h"
#include "batterymonitor.h"

//
#define ALL_PERMISSIONS   QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner \
    | QFile::ReadUser  | QFile::WriteUser  | QFile::ExeUser  \
    | QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup \
    | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther

//
bool update_flag = false;
CUpdate *g_update = nullptr;

//
const int timeWait = 20000;
const int timeEachLoop = 500;

namespace  {
constexpr int C_DOWNLOWN_TIMEOUT_MS = 30 * 1000;       // 下载超时（ms）（连续无数据传输的时间）
}

/* 这里的定义和 update-gen 项目的 update_gen.cpp 一致。 */
//
#define UPDATE_INFO_FILE_NAME "update-info.txt"
#define UPDATE_INFO_FILE_NAME_OLD "md5"

const QString FIELD_SEP = "\t";
const QString LINE_TAIL = "\r\n";
const QString LINE_TAIL_OLD = "\n";

//
QNetworkAccessManager *CUpdate::s_netManager {nullptr};

CUpdate::CUpdate(QObject *_parent) : QObject(_parent)
{
    //
    if (!s_netManager) {
        logWarning(QString("%1: QNetworkAccessManager not been setted! Created internal!").arg(__PRETTY_FUNCTION__));
        s_netManager = new QNetworkAccessManager();
    }

    s_netManager->setNetworkAccessible(QNetworkAccessManager::Accessible);

    //
    m_timer = new QTimer;
    m_timer->setSingleShot(true);

    //
    m_eventLoop = new QEventLoop;

    // finish to quit
    QObject::connect(s_netManager, &QNetworkAccessManager::finished, m_eventLoop, &QEventLoop::quit);
    QObject::connect(m_timer, &QTimer::timeout, m_eventLoop, &QEventLoop::quit);

    //
    m_autoUpdateTimer = new QTimer;
    m_autoUpdateTimer->setInterval(30000);

    QObject::connect(m_autoUpdateTimer, &QTimer::timeout, this, &CUpdate::slot_autoUpdateTimer_timeout);

    //
    m_operation = new UpdateOperation(s_netManager);
    m_workThread = new QThread;

    QObject::connect(m_operation, &UpdateOperation::sigCheckUpdateResult, this, &CUpdate::slotCheckUpdateResult,Qt::QueuedConnection);
    QObject::connect(this, &CUpdate::sigCheckUpdate, m_operation, &UpdateOperation::slotCheckUpdate,Qt::QueuedConnection);

    m_operation->moveToThread(m_workThread);
    m_workThread->start();

    //
    m_action = AutoCheck;

}

CUpdate::~CUpdate()
{
    delete m_timer;
    m_timer = nullptr;
    delete m_eventLoop;
    m_eventLoop = nullptr;
    delete m_autoUpdateTimer;
    m_autoUpdateTimer = nullptr;
    delete m_operation;
    m_operation = nullptr;
    delete m_workThread;
    m_workThread = nullptr;
}

void CUpdate::setNetworkAccessManager(QNetworkAccessManager *_net_manager)
{
    s_netManager = _net_manager;
}

// 设置获取更新的服务器
void CUpdate::slotUpdateAddressChanged(QString _server)
{
    this->m_server = _server;
}

// 5毫秒后开始更新,适合在main函数里不能直接调用runUpdate的情况
void CUpdate::updateLater()
{
    QTimer::singleShot(5, this, [this]() {
        this->slotRunUpdate();
    });
}

void CUpdate::slot_autoUpdateTimer_timeout()
{
    static int checkTimes = 0;
    if(checkTimes >= 10)
    {
        m_autoUpdateTimer->stop();
        return;
    }
    checkTimes++;

    if (WinMeasure::isOpened()) {
        qDebug() << ": camera is visible";
        return;
    }

    qDebug() << "update check attempt " << checkTimes;

    emit sigCheckUpdate(m_server);
}

void CUpdate::slotAutoCheckUpdateChanged(bool _is_auto_check)
{
    if (_is_auto_check) {
        if (!m_autoUpdateTimer->isActive()) {
            m_autoUpdateTimer->start();
        }
    } else {
        if (m_autoUpdateTimer->isActive()) {
            m_autoUpdateTimer->stop();
        }
    }
}

void CUpdate::slotCheckUpdate()
{
    //
    m_autoUpdateTimer->stop();
    m_action = ManualCheck;

    //
#if (2 == OS_TYPE)
    if (CGlobal::isDebugMode) {
        update_flag = false;
        emit sigShowProgress(true);       // 桌面调试样式，只显示窗体，不执行更新
        return;
    }
#endif

    //
    emit sigCheckUpdate(m_server);
}

void CUpdate::slotRunUpdate()
{
    //
    emit sigShowProgress(true);

    // 执行更新
    bool is_update_succ = runUpdate();

    //
    if (is_update_succ)
    {
        qDebug() << "auto update succeeded.";

        // save a backup of main
#if (OS_TYPE == 1)
        system("cp /oldVersion/bin/main /bin/mainBackup");
#else
        system("cp /oldVersion/bin/screener /bin/screenerBackup");      // 部分文件更新失败后的恢复？
#endif

        // if need to keep old version, commit this
        system("rm /oldVersion -r");

        //
        //appSetting::sync();     // TODO: 之前未加上这句，据反馈有时配置文件会损坏？如何确保不会有信息丢失？

        system("sync");     /* 之前（20231013）的代码，如果下面的“是否重启”提示框，用户点了“取消”，就不同步，导致文件丢失。 */

        //
        MessageWin msg_win;
        msg_win.setWindowModality(Qt::ApplicationModal);     // 模式显示
        msg_win.setContent(tr("更新成功，即将重启。"));    // "Update success,\ntake effect after restart"
        msg_win.setButtonEnable(false);
        msg_win.setTimeout(3);
        msg_win.exec();                 // TODO: 把这些涉及 UI 的代码全都移到模块之外？

        //
        emit sigRebootSystem();
    }
    else
    {
        qDebug() << "auto update failed.";

        system("cp /oldVersion/* / -r");
        system("rm /oldVersion -r");

        //
        //appSetting::sync();     // TODO: 之前未加上这句，据反馈有时配置文件会损坏？如何确保不会有信息丢失？

        system("sync");     /* 之前（20231013）的代码，如果下面的“是否重启”提示框，用户点了“取消”，就不同步，导致文件丢失。 */

        //
        MessageWin mess;
        mess.setContent(tr("更新失败, 请稍后重试")); // "Update fail,retry later"
        mess.setButtonText("确认");   // "OK"
        mess.exec();
    }

    //
    emit sigShowProgress(false);

}

// 执行更新
bool CUpdate::runUpdate()
{
    if(m_server.isEmpty())
    {
        qDebug() << "set server first";
        m_updateState = false;
        return false;
    }

    //检测内存可用容量,小于100M则不更新   2020.7.17
    qDebug() << "checking free space ...";

    QProcess p;
    p.start("df");
    p.waitForFinished();
    QStringList list = QString::fromLocal8Bit(p.readAllStandardOutput()).simplified().split("\n");
    for (QString s : list){
        if(s.section(" ",0,0) == "/dev/root"){
            int memLeft = s.section(" ",3,3).toInt();
            if(memLeft < 100*1024)
                return false;
        }
    }

    //
    QObject::connect(s_netManager, &QNetworkAccessManager::finished, m_eventLoop, &QEventLoop::quit, (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));

    /******************************************* start *******************************************/
    //2021.03.09  tao  获取服务器记录的版本号
    if(1)
    {
        qDebug() << "donwloading version info file ...";

        //设置url
        QString url = m_server + QDir::separator() + "version.txt";
        QNetworkRequest requestInfo;                // TODO: 改用 Common::Net::sendHttpRequest()？
        requestInfo.setUrl(QUrl(url));

        //添加事件循环机制，返回后再运行后面的
        QNetworkReply *reply =  s_netManager->get(requestInfo);
        m_timer->start(timeWait);
        m_eventLoop->exec();

        m_timer->stop();

        //错误处理
        if (reply->error() == QNetworkReply::NoError)
        {
            //请求返回的结果
            while(!reply->atEnd())      //文件一直读到末尾
            {
                QByteArray responseByte = reply->readLine();    //获取文件一行内容
                qDebug()<<"-----reply->readAll:"<<responseByte;
            }
        }
        else
        {
            qDebug()<<"request protobufHttp handle errors here";
            QVariant statusCodeV = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);  //statusCodeV是HTTP服务器的相应码，reply->error()是Qt定义的错误码，可以参考QT的文档
            qDebug( "request protobufHttp found error ....code: %d %d\n", statusCodeV.toInt(), (int)reply->error());
            qDebug() << qPrintable(reply->errorString());
        }

        //
        if (reply) {
            reply->deleteLater();       // NOTE: QNetworkReply 须由调用者释放。
        }
    }
    /******************************************* end *******************************************/

    // 创建一个请求
    QNetworkRequest request;
    QString url_str = m_server + QDir::separator() + UPDATE_INFO_FILE_NAME;
    request.setUrl(QUrl(url_str));

    qDebug() << QTime::currentTime().toString("hh:mm:ss zzz: ") << "donwloading file info file from " << url_str << " ...";

    // 发送GET请求
    QNetworkReply *reply = s_netManager->get(request);

    // 设置超时
    m_timer->start(timeWait);

    // 等待GET请求完成
    m_eventLoop->exec(QEventLoop::ExcludeUserInputEvents);

    m_timer->stop();

    if (!reply->isFinished()) {
        //
        reply->abort();

        //
        if(m_timer->remainingTime() < 0) {    // 定时器剩余时间小于0，代表已超时
            qDebug() << "http method get timeout";
        } else {
            qDebug() << "unknown error";
        }

        //
        reply->deleteLater();
        return false;
    }

    int totalSize = reply->size();

    qDebug() << QTime::currentTime().toString("hh:mm:ss zzz: ") << "file info file size = " << totalSize;

    // 读取文件md5中的每一行
    while(!reply->atEnd())
    {
        if(m_isCancel)
        {
            m_isCancel = false;
            m_updateState = false;
            reply->deleteLater();
            return false;
        }

        // 格式:[完整路径文件名]\t[32字符md5值]\r\n
        QString file_info_str(reply->readLine().trimmed());
        if (file_info_str.length() == 0)
            break;

        QString file_path;
        QByteArray md5_value;
        QStringList file_info_list = file_info_str.split(FIELD_SEP);
        if (file_info_list.length() == 2) {
            file_path = file_info_list.at(0);
            md5_value = (file_info_list.at(1)).toLocal8Bit();
        } else {
            //
            reply->deleteLater();
            return false;
        }

        //
        emit sigCurrentFileChanged(file_path);

        // 更新标志位
        bool isUpdateFile;

        QFile file(file_path);
        if(file.exists())
        {
            // 文件存在并且可以正常打开
            if(file.open(QFile::ReadOnly))
            {
                // 计算出本地文件md5值,与服务器上该文件md5值进行对比
                QByteArray md5 = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5).toHex();
                if(md5_value == md5)
                {
                    // qDebug() << "the same md5 value " << md5;
                    isUpdateFile = false;
                }
                else
                {
                    // qDebug() << "diffrent md5 value " << md5 << " & " << md5Value;
                    isUpdateFile = true;
                }
                file.close();
            }
            else
            {
                qDebug() << "open file " << file_path << " failed:" << file.errorString();
                isUpdateFile = false;
            }
        }
        else
        {
            // 文件不存在,则创建完整路径,以便新建文件
            QFileInfo info(file);
            QDir dir(info.absolutePath());
            if(!dir.exists())
                dir.mkpath(dir.absolutePath());

            isUpdateFile = true;
        }

        if(isUpdateFile)
        {
            qDebug() << "updating file " << file_path << " ...";

            // 创建并发送GET请求,获取服务器上的文件
            QNetworkRequest fileRequest;
            fileRequest.setUrl(QUrl(m_server + file_path));
            QNetworkReply *fileReply = s_netManager->get(fileRequest);

            int downloadedSize = 0;

            int count_timeout = 0;
            qint64 size_last = 0;
            while (!m_isCancel) {
                int fileSize = fileReply->header(QNetworkRequest::ContentLengthHeader).toInt();
                if (fileSize != 0)
                    emit sigProgressChanged(fileReply->size() * 100 / fileSize, -1);
                else
                    emit sigProgressChanged(0, -1);

                qApp->processEvents();

                //
                if(fileReply->isFinished())
                {
                    break;
                }

                //
                m_timer->start(timeEachLoop);
                m_eventLoop->exec(QEventLoop::ExcludeUserInputEvents);
                m_timer->stop();

                // 超时检查
                qint64 size_curr = fileReply->size();
                int size_diff = size_curr - size_last;
                size_last = size_curr;
                if (0 == size_diff) {
                    count_timeout++;
                    //qDebug() << "count_timeout = " << count_timeout;
                } else {
                    qDebug() << "count_timeout = " << count_timeout;
                    //count_timeout = 0;
                }
                if (count_timeout * timeEachLoop > C_DOWNLOWN_TIMEOUT_MS) {
                    qDebug() << "file download timeout!";
                    fileReply->deleteLater();
                    reply->deleteLater();
                    return false;
                }

                // calculate download speed b/s
                int speed = (size_curr - downloadedSize) * 1000 / timeEachLoop;
                downloadedSize = fileReply->size();
                if(speed < 1024)
                {
                    emit sigSpeedChanged(speed, 0);
                }
                else
                {
                    int speedInKb = speed/1024;
                    if(speedInKb < 1024)
                    {
                        emit sigSpeedChanged(speedInKb, 1);
                    }
                    else
                    {
                        float speedInMb = (float)speedInKb/1024;
                        emit sigSpeedChanged(speedInMb, 2);
                    }
                }
            }

            emit sigSpeedChanged(0, 0);

            if (m_isCancel)
            {
                m_isCancel = false;
                m_updateState = false;
                emit sigCurrentFileChanged("");
                emit sigProgressChanged(0, 0);
                fileReply->deleteLater();
                reply->deleteLater();
                return false;
            }

            // check new file
            QByteArray newFile = fileReply->readAll();
            QByteArray newMd5 = QCryptographicHash::hash(newFile, QCryptographicHash::Md5).toHex();
            if(md5_value != newMd5)
            {
                qDebug() << "new file md5 is diffrent to record, skip";
                fileReply->deleteLater();
                reply->deleteLater();
                return false;
            }

            QFile::Permissions permission;

            if(file.exists())
            {
                permission = file.permissions();

                QFileInfo info(QString("/oldVersion%1").arg(file.fileName()));
                QDir dir(info.absoluteDir());
                dir.mkpath(dir.absolutePath());
                if(!file.copy(QString("/oldVersion%1").arg(file.fileName())))
                    qDebug() << "copy failed: " << file.errorString();
                if(!file.remove())
                    qDebug() << "remove failed: " << file.errorString();
            }
            else
            {
                permission |= ALL_PERMISSIONS;
            }

            // 打开本地文件并写入
            if(file.open(QFile::WriteOnly|QFile::Truncate)|QFile::Unbuffered)
            {
                file.write(newFile);
                file.close();
                if(!file.setPermissions(permission))
                    qDebug() << "set permissions failed: " << file.errorString();

                // run script
                if(file_path.contains("RunScript"))
                {
                    system(file_path.toStdString().data());
                }
            }
            else
            {
                qDebug() << file_path << " open fail:" << file.errorString();
                fileReply->deleteLater();
                reply->deleteLater();
                return false;
            }

            //
            fileReply->deleteLater();       // NOTE: QNetworkReply 须由调用者释放。
        }
        else
        {
            qDebug() << file_path << " is newest or can't update";
        }

        emit sigProgressChanged(-1, (totalSize-reply->size()) * 100 / totalSize);

        qApp->processEvents();
    }

    //
    reply->deleteLater();       // NOTE: QNetworkReply 须由调用者释放。

    m_updateState = true;
    return true;
}

bool CUpdate::isSuccess()
{
    return m_updateState;
}

void CUpdate::slotCancelUpdate()
{
    m_isCancel = true;

#if (2 == OS_TYPE)
    emit sigShowProgress(false);
#endif
}

void CUpdate::slotCheckUpdateResult(CheckState state)
{
    if(WinMeasure::isOpened())
        return;

    qDebug() << "action: " << m_action;
    qDebug() << "state: " << state;
    if(m_action == AutoCheck)
    {
        if(state == NoUpdateAvailable)
        {
            m_autoUpdateTimer->stop();
            return;
        }
        if(state != UpdateAvailable)
        {
            return;
        }

        m_autoUpdateTimer->stop();
        UpdateDialog updateDialog;
        int r = updateDialog.exec();
        if(r == UpdateDialog::Update)
        {
            // check battery
#if (OS_TYPE != 2)
            bool is_battery_ok = (BatteryMonitor::getBattLevel() >= 1 || BatteryMonitor::getIsCharging());
#else
            bool is_battery_ok = true;
#endif
            if (is_battery_ok) {
                // 执行更新
                slotRunUpdate();
            } else {
                MessageWin mess;
                mess.setContent(tr("电量低,充电后重试"));   // "Low battery,retry after charge"
                mess.setButtonText("确认");   // "OK"
                mess.exec();
            }
        }
        else if(r == UpdateDialog::Remind)
        {
            //
        }
        else //(r == UpdateDialog::Ignore)
        {
            emit setAutoCheckUpdate(false);
        }
    }
    else
    {
        bool updateAvailabe = false;
        QString messageContent;
        QString buttonText;

        buttonText = tr("确认"); // "OK"

        if (state == NoUpdateAvailable) {
            messageContent = tr("暂无可用更新");  // "No update available now"
        } else if(state == NoNetwork) {
            messageContent = tr("网络未连接");   // "Network disconnect"
        } else if(state == networkUnreachable) {
            messageContent = tr("当前网络不可用"); // "Network is unreachable"
        } else if(state == BadServer) {
            messageContent = tr("更新服务器设置错误");   // "Update server settings error"
        } else if(state == HostNotFoundError) {
            messageContent = tr("更新服务器无法访问");   // "Update server is unreachable"
        } else if(state == InfoFileFormatError) {
            messageContent = tr("信息文件格式错误");    // "InfoFile format error"
        } else {
            updateAvailabe = true;
        }

        if(updateAvailabe)
        {
            UpdateDialog updateDialog;
            int r = updateDialog.exec();
            if(r == UpdateDialog::Update)
            {
                // check battery
#if (OS_TYPE != 2)
                bool is_battery_ok = (BatteryMonitor::getBattLevel() >= 1 || BatteryMonitor::getIsCharging());
#else
                bool is_battery_ok = true;
#endif
                if(is_battery_ok) {
                    // 执行更新
                    slotRunUpdate();
                } else {
                    MessageWin mess;
                    mess.setContent(tr("电量低,充电后重试"));   // "Low battery,retry after charging"
                    mess.setButtonText(tr("确认"));   // "OK"
                    mess.exec();
                }
            }
            else if(r == UpdateDialog::Remind)
            {
                //
            }
            else //if(r == UpdateDialog::Ignore)
            {
                emit setAutoCheckUpdate(false);
            }
            update_flag = false;
        }
        else
        {
            MessageWin mess;
            mess.setContent(messageContent);
            mess.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
            mess.setButtonText(buttonText);
            if(mess.exec() == QDialog::Accepted){}
            else{}
            update_flag = false;
        }
    }
}

UpdateOperation::UpdateOperation(QNetworkAccessManager *_net_manager, QObject *_parent) : QObject(_parent), m_netManager(_net_manager)
{
    m_isInstantiate = false;
}

UpdateOperation::~UpdateOperation()
{
    if(m_isInstantiate)
    {
        delete m_eventLoop;
        delete m_timer;
    }
}

/* 114 ping 不通了，导致所有以前出厂的机器都无法网络升级。这个判断并不保险（无法确保所 ping IP 或域名永久有效），且必要性也不高，不如不要 */
//bool UpdateOperation::isNetworkReachable()
//{
//    //FILE* f = popen("ping 114.114.114.114 -w 1 -c 1", "r");               /* 实际使用中遇到有的地方这个 IP 被屏蔽 */
//    FILE* f = popen("ping www.baidu.com -w 1 -c 1", "r");
//    char str[512] = {0};
//    fread(str, 1, 512, f);
//    pclose(f);
//
//    bool is_net_ok = QString(str).contains("time");         // TODO: ping 不通也会包含“time”？
//
//    if (!is_net_ok) {
//        qDebug() << "network is not available:\n";
//        qDebug() << str;
//    }
//
//    return is_net_ok;
//}

void UpdateOperation::slotCheckUpdate(QString _server)
{
    if(!m_isInstantiate)
    {
        m_isInstantiate = true;

        //
        m_netManager->setNetworkAccessible(QNetworkAccessManager::Accessible);

        m_eventLoop = new QEventLoop;

        m_timer = new QTimer;
        m_timer->setSingleShot(true);

        // finish to quit
        QObject::connect(m_netManager, SIGNAL(finished(QNetworkReply*)), m_eventLoop, SLOT(quit()), Qt::QueuedConnection);

        // 超时退出阻塞
        QObject::connect(m_timer, SIGNAL(timeout()), m_eventLoop, SLOT(quit()));

    }

    CWifiIntf *wifi_intf = CWifiIntf::instance();
    if (!wifi_intf->getIsConnected())
    {
        qDebug() << "no network";
        emit sigCheckUpdateResult(NoNetwork);
        return;
    }
    //else if(!isNetworkReachable())
    //{
    //    qDebug() << "network is unreachable";
    //    emit sigCheckUpdateResult(networkUnreachable);
    //    return;
    //}
    else
    {
        if(_server.isEmpty())
        {
            qDebug() << "set server first";
            emit sigCheckUpdateResult(BadServer);
            return;
        }

        // 创建一个请求
        QNetworkRequest request;    // TODO: 改用 Common::Net::sendHttpRequest()？
        QUrl url(_server + QDir::separator() + UPDATE_INFO_FILE_NAME);
        request.setUrl(url);

        // 发送GET请求
        QNetworkReply *reply = m_netManager->get(request);

        // 设置超时
        m_timer->start(timeWait);

        // 等待GET请求完成
        m_eventLoop->exec(QEventLoop::ExcludeUserInputEvents);

        m_timer->stop();

        if (!reply->isFinished()) {
            if (m_timer->remainingTime() < 0) {
                qDebug() << "http method get timeout";
            } else {
                qDebug() << "unknown error";
            }

            reply->deleteLater();
            emit sigCheckUpdateResult(BadServer);
            return;
        }

        QNetworkReply::NetworkError err = reply->error();
        if (QNetworkReply::NoError == err) {        // TODO: 否则，服务器无法连接时也是提示“无可用更新”
            // 读取文件md5中的每一行
            while(!reply->atEnd())
            {
                // 格式:[完整路径文件名]\t[16字节md5值]\r\n
                QString file_info_str(reply->readLine().trimmed());
                if (file_info_str.length() == 0)
                    continue;

                QString file_path;
                QByteArray md5_value;
                QStringList file_info_list = file_info_str.split(FIELD_SEP);
                if (file_info_list.length() == 2) {
                    file_path = file_info_list.at(0);
                    md5_value = (file_info_list.at(1)).toLocal8Bit();
                } else {
                    //
                    qDebug() << "info file format error";
                    reply->deleteLater();
                    emit sigCheckUpdateResult(BadServer);
                    return;
                }
                qDebug() << file_path << " md5 = " << md5_value;

                //
                if(file_path.toStdString().data()[0] == '<')
                {
                    qDebug() << "server error";
                    reply->deleteLater();
                    emit sigCheckUpdateResult(BadServer);
                    return;
                }

                QFile file(file_path);
                if(file.exists())
                {
                    // 文件存在并且可以正常打开
                    if(file.open(QFile::ReadOnly))
                    {
                        // 计算出本地文件md5值,与服务器上该文件md5值进行对比
                        QByteArray md5 = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5).toHex();
                        qDebug() << "local file md5 = " << md5;
                        if (md5_value != md5)
                        {
                            qDebug() << file_path << " has diffrent md5 value " << md5 << " -> " << md5_value;
                            reply->deleteLater();
                            emit sigCheckUpdateResult(UpdateAvailable);
                            return;
                        }
                        file.close();
                    }
                }
                // 文件不存在
                else
                {
                    qDebug() << file_path << " not exists";
                    reply->deleteLater();
                    emit sigCheckUpdateResult(UpdateAvailable);
                    return;
                }
            }
        } else {
            //logWarning(QString::asprintf("UpdateOperation::slotCheckUpdate(): error = %d", (int)err));
            qDebug() << QString::asprintf("UpdateOperation::slotCheckUpdate(): error = %d", (int)err);
            qDebug() << url.toString();

            switch (err) {
            case QNetworkReply::HostNotFoundError:
                emit sigCheckUpdateResult(HostNotFoundError);
                break;
            default:
                emit sigCheckUpdateResult(HostNotFoundError);
                break;
            }

            reply->deleteLater();
            return;
        }

        reply->deleteLater();
        emit sigCheckUpdateResult(NoUpdateAvailable);
        return;
    }
}
