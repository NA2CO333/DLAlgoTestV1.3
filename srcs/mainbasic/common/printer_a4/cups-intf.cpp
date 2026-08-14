#include "cups-intf.h"

#include <QDebug>
#include <QStringLiteral>
#include <QFile>
#include <QUrl>

#include "wifiintf.h"
#include "nettools.h"
#include "util-common.h"
#include "logger.h"

namespace Common {

//
static constexpr QChar CHAR_SPACE   = ' ';
static constexpr QChar CHAR_QUO     = '"';

/// ============================================================================================================
/// 其它函数
//TODO: 移到公用单元？

// 分离由多个字段的值拼合而成的字符串（如 CSV 格式），支持双引号包起含有空格的字段
void splitStrToFields(const QString &_str, QStringList &_list_str, const QChar &_sep = CHAR_SPACE)
{
    //
    _list_str.clear();

    //
    bool is_err = false;

    //
    QString sub_str;
    int pos_last = -1;
    int pos_curr = -1;
    bool is_quo_started = false;
    int pos_quo = -1;
    do {
        pos_last = pos_curr;
        pos_curr = _str.indexOf(_sep, pos_curr + 1);

        if (pos_curr >= 0) {    // 若找到分割符
            // 分割符之前的部分为当前字段值
            sub_str = _str.mid(pos_last + 1, pos_curr - (pos_last + 1));

            // 若有开双引号则特殊处理，否则添加当前字段值
            is_quo_started = sub_str.startsWith(CHAR_QUO);
            if (is_quo_started) {
                // 往后面查找收双引号，且将开、收双引号之间的部分作为当前字段值
                pos_quo = _str.indexOf(CHAR_QUO, pos_curr + 1);         // TODO: 支持双引号的转义？
                if (pos_quo >= 0) {
                    // 收双引号，须是最后一个字符，或者下一个字符是分割符，否则格式错误
                    if ((pos_quo == _str.length() - 1) || ((pos_quo < _str.length() - 1) && (_sep == _str[pos_quo + 1]))) {
                        sub_str = _str.mid(pos_last + 2, pos_quo - (pos_last + 2));

                        _list_str.append(sub_str);
                        pos_curr = pos_quo + 1;
                    } else {
                        qCritical() << __PRETTY_FUNCTION__ << ": format error: ending of quotation not last char and next char not separator!";
                        is_err = true;
                        break;
                    }
                } else {
                    // 若找不到收双引号，则格式错误
                    qCritical() << __PRETTY_FUNCTION__ << ": format error: ending of quotation not found!";
                    is_err = true;
                    break;
                }
            } else {
                _list_str.append(sub_str);
            }
        } else {                // 若找不到分割符
            // 从查找开始位置到字符串末尾，为当前字段值，并结束分割
            sub_str = _str.mid(pos_last + 1);

            if (sub_str.startsWith(CHAR_QUO)) {     // 去掉开双引号
                sub_str = sub_str.mid(1);
            }
            if (sub_str.endsWith(CHAR_QUO)) {       // 去掉收双引号
                sub_str = sub_str.left(sub_str.length() - 1);
            }

            _list_str.append(sub_str);

            break;
        }
    } while (pos_curr >= 0);

    //
    if (is_err) {
        qCritical() << "str which format error is: " << _str;
        _list_str.clear();
    }
}

/// ============================================================================================================
/// class CCupsIntf

// 已支持的 PPD
static constexpr char PPD_EPSON_L3250[]         = "/etc/ppd/Epson-L3250_Series-epson-escpr-en.ppd";
static constexpr char PPD_HP_LASER_MFP_13X[]    = "/etc/ppd/HP_Laser_MFP_13x_Series.ppd";
static constexpr char PPD_Lenovo_CS1821W[]      = "/etc/ppd/Lenovo_CS1821_CS1831_Series.ppd";

static constexpr char MAKES[3][16] = {
    "EPSON",
    "HP",
    "LENOVO",
};
// TODO:


//
CCupsIntf::CCupsIntf(QObject *parent)
{
    Q_UNUSED(parent)

    QObject::connect(this, &CCupsIntf::sigPrintFile, this, &CCupsIntf::slotPrintFile, Qt::QueuedConnection);

}

CCupsIntf::~CCupsIntf()
{
    // TODO: 删除已添加的打印机？

}

const QStringList &CCupsIntf::getSupportedMakes()
{
    static QStringList *list = nullptr;
    if (!list) {
        list = new QStringList;
        *list << MAKES[0] << MAKES[1] << MAKES[2];
    }

    // TODO:

    return *list;
}

const QStringList &CCupsIntf::getSupportedModels(const QString &_make)
{
    static QStringList *list = nullptr;
    if (!list) {
        list = new QStringList;
        //*list << ;
    }

    Q_UNUSED(_make)
    // TODO:

    return *list;
}

void CCupsIntf::searchPrinters(QList<stPrinterInfo> &_list_info)
{
    qDebug() << __PRETTY_FUNCTION__ << ": entered ...";

    // 先清空结果列表
    _list_info.clear();

    // 是否 WiFi 直连
    /* 经测试了 EPSON 和 LENOVO 打印机，若仪器通过打印机的WiFi直连功能连接打印机，搜索得到的IP是无效的，须换为本机的默认网关 */
    bool is_wifi_direct = CWifiIntf::instance()->isWifiDirect();
    QString default_gateway = "";
    if (is_wifi_direct) {
        default_gateway = Net::getDefaultGateway();
    }

    //
    QProcess process;

    //if (QProcess::NotRunning != process.state()) {
    //    // logWarning();
    //    process.waitForFinished();
    //    if (QProcess::NotRunning != process.state()) {
    //        // logCritical();
    //        process.kill();
    //    }
    //}

    // 用 snmp 查找网络打印机，得到品牌和型号
    {
        qDebug() << __PRETTY_FUNCTION__ << ": calling /usr/lib/cups/backend/snmp ...";

        process.setReadChannel(QProcess::StandardOutput);
        process.start("/usr/lib/cups/backend/snmp");
        process.waitForFinished(30000);

        //qDebug() << process.readAll();

        //if (QProcess::UnknownError != process.error()) {
        //
        //}

        QString str_line;
        QStringList list_fileds;
        int pos_protocol_end;
        QString make_model;
        int pos_1, pos_2;
        QString ip;
        QString key;
        while (!process.atEnd()) {
            str_line = QString::fromLatin1(process.readLine());
            str_line.remove(str_line.length() - 1, 1);  // 移除行末的换行符
            qDebug() << str_line;

            //
            if (str_line.startsWith("network ")) {                      // 只获取网络打印机（uri格式：protocol://）
                splitStrToFields(str_line, list_fileds, CHAR_SPACE);

                if (list_fileds.size() < 3) {
                    qWarning() << "logic error: size of list_fileds less then 3";
                    continue;
                }

                //
                stPrinterCupsInfo cups_info;

                // uri
                cups_info.uri = list_fileds[1];
                key = cups_info.uri;

                // protocol
                pos_protocol_end = cups_info.uri.indexOf("://");
                if (pos_protocol_end > 0) {
                    cups_info.protocol = cups_info.uri.left(pos_protocol_end);
                } else {
                    continue;       // 没有完整 URI 的，过滤掉
                }

                // make & model
                make_model = list_fileds[2];
                pos_1 = make_model.indexOf(' ');
                pos_2 = make_model.indexOf(' ', pos_1 + 1);
                if (pos_1 > 0 && pos_2 > 0) {
                    cups_info.make = make_model.left(pos_1);
                    cups_info.model = make_model.mid(pos_1 + 1, pos_2 - pos_1 - 1);
                } else {
                    cups_info.make = "";
                    cups_info.model = "";
                }

                cups_info.model.remove('(');
                cups_info.model.remove(')');

                // ip
                ip = "";
                pos_1 = cups_info.uri.indexOf("://");
                if (pos_1 > 0) {
                    ip = cups_info.uri.mid(pos_1 + 3);

                    pos_1 = ip.indexOf(':');
                    if (pos_1 > 0) {
                        ip = ip.left(pos_1);
                    }

                    pos_1 = ip.indexOf('/');
                    if (pos_1 > 0) {
                        ip = ip.left(pos_1);
                    }
                }
                cups_info.ip = ip;

                if (is_wifi_direct) {       // WiFi直连时，修正打印机的IP
                    cups_info.ip = default_gateway;
                    cups_info.uri.replace(ip, default_gateway);
                }

                // desc
                cups_info.name = QString("%1_%2_(%3)").arg(cups_info.make).arg(cups_info.model).arg(cups_info.ip);

                // 根据品牌和型号匹配 ppd
                if (QStringLiteral("lenovo") == cups_info.make.toLower()) {
                    cups_info.ppd = "/etc/ppd/Lenovo_CS1821_CS1831_Series.ppd";
                } else if (QStringLiteral("hp") == cups_info.make.toLower()) {
                    cups_info.ppd = "/etc/ppd/HP_Laser_MFP_13x_Series.ppd";
                } else if (QStringLiteral("epson") == cups_info.make.toLower()) {
                    cups_info.ppd = "/etc/ppd/Epson-L3250_Series-epson-escpr-en.ppd";
                }

                //
                m_listPrintersCpusInfo.insert(key, cups_info);
            }
        }
    }

    // 用 lpinfo 查找打印机，得到 uri        // TODO: 有了 snmp 的搜索，lpinfo 的搜索可以不需要？   // TODO: 两个搜索结果可能有差异？比如 snmp 有但 lpinfo 没有，或反之？
    {
        qDebug() << __PRETTY_FUNCTION__ << ": calling lpinfo -v ...";

        process.setReadChannel(QProcess::StandardOutput);
        process.start("lpinfo -v");
        process.waitForFinished(30000);

        //if (QProcess::UnknownError != process.error()) {
        //
        //}

        static constexpr char INFO_HEAD_NETWORK[]   = "network ";       // 打印机信息头 - 网络连接方式的
        static constexpr char INFO_HEAD_USB[]       = "direct ";        // 打印机信息头 - USB连接方式的

        QString str_line;
        QString uri;
        int pos_protocol_end;
        QString protocol;
        bool is_supported;
        while (!process.atEnd()) {
            str_line = QString::fromLatin1(process.readLine());
            str_line.remove(str_line.length() - 1, 1);  // 移除行末的换行符
            qDebug() << str_line;

            //
            if (str_line.startsWith(INFO_HEAD_USB)) {               // 获取 USB 连接方式的打印机（uri格式：“usb://VENDOR/MODEL?serial=XXX”）
                // uri
                uri = str_line.mid(strlen(INFO_HEAD_USB));

                // protocol
                pos_protocol_end = uri.indexOf("://");
                if (pos_protocol_end > 0) {
                    protocol = uri.left(pos_protocol_end);
                } else {
                    protocol = "";
                }

                // 若协议不是 usb，则过滤掉
                if (QStringLiteral("usb") != protocol.toLower()){
                    continue;
                }

                // vendor
                QString vendor = uri.mid(pos_protocol_end + strlen("://"));   // 去掉 protocol 部分
                vendor = vendor.left(vendor.indexOf('/'));
                if (vendor.isEmpty()) {
                    continue;
                }

                // model
                QString model = uri.mid(uri.indexOf(vendor) + vendor.size() + 1);
                if (model.isEmpty()) {
                    continue;
                }
                model = model.left(model.indexOf('?'));
                if (model.isEmpty()) {
                    continue;
                }
                model = QUrl::fromPercentEncoding(model.toLatin1());
                model.replace(' ', '-');
                if (model.isEmpty()) {
                    continue;
                }

                //
                stPrinterCupsInfo cups_info;
                cups_info.name = QString("%1_%2(USB)").arg(vendor).arg(model);
                cups_info.uri = uri;
                cups_info.make = vendor;
                cups_info.model = model;

                // 根据品牌和型号匹配 ppd
                is_supported = true;
                if (QStringLiteral("lenovo") == cups_info.make.toLower()) {
                    cups_info.ppd = "/etc/ppd/Lenovo_CS1821_CS1831_Series.ppd";
                } else if (QStringLiteral("hp") == cups_info.make.toLower()) {
                    cups_info.ppd = "/etc/ppd/HP_Laser_MFP_13x_Series.ppd";
                } else if (QStringLiteral("epson") == cups_info.make.toLower()) {
                    cups_info.ppd = "/etc/ppd/Epson-L3250_Series-epson-escpr-en.ppd";
                } else {
                    is_supported = false;
                    continue;
                }
                m_listPrintersCpusInfo.insert(cups_info.uri, cups_info);

                //
                stPrinterInfo printer_info;

                printer_info.uri = cups_info.uri;
                printer_info.name = cups_info.name;
                printer_info.isSupported = is_supported;

                _list_info.append(printer_info);
            } else if (str_line.startsWith(INFO_HEAD_NETWORK)) {    // 获取网络连接方式的打印机
                // uri
                uri = str_line.mid(strlen(INFO_HEAD_NETWORK));

                // protocol
                pos_protocol_end = uri.indexOf("://");
                if (pos_protocol_end > 0) {
                    protocol = uri.left(pos_protocol_end);
                } else {
                    protocol = "";
                }

                // 若协议不是 socket、lpd，则过滤掉
                if ((QStringLiteral("socket") != protocol.toLower()) && (QStringLiteral("lpd") != protocol.toLower()) ){
                    continue;
                }

                // cups_info
                stPrinterCupsInfo cups_info;
                if (!m_listPrintersCpusInfo.contains(uri)) {
                    //cups_info.uri = uri;
                    //cups_info.protocol = protocol;
                    //cups_info.name = QString("(unknown)_%1").arg(uri);
                    //m_listPrintersCpusInfo.insert(uri, cups_info);

                    continue;       // 若前面的 snmp 找不到该打印机，则过滤掉       // TODO: 有没问题？有没其它方法获得该打印机的品牌和型号？
                } else {
                    cups_info = m_listPrintersCpusInfo.value(uri);
                }

                is_supported = true;
                if (cups_info.ppd.isEmpty()) {      // 若没有 PPD，则未支持该系列打印机，过滤掉
                    is_supported = false;
                    if (!m_isShowNotSupported) {
                        continue;
                    }
                }

                //
                stPrinterInfo printer_info;
                printer_info.uri = cups_info.uri;       /* 这里取 snmp 搜索得到的而且可能修正过的 uri */
                printer_info.name = cups_info.name;
                printer_info.isSupported = is_supported;

                _list_info.append(printer_info);
            }
        }
    }

    //
    qDebug() << __PRETTY_FUNCTION__ << ": exited";
}

bool CCupsIntf::setDefaultPrinter(QString _uri, QString _name, QString _ppd)
{
    //
    stPrinterCupsInfo cups_info;
    if (!_name.isEmpty() && !_ppd.isEmpty()) {
        cups_info.name  = _name;
        cups_info.ppd   = _ppd;
        cups_info.uri   = _uri;
    } else if (m_listPrintersCpusInfo.contains(_uri)) {
        cups_info = m_listPrintersCpusInfo.value(_uri);
    } else {
        return false;
    }

    // 添加打印机        // TODO: 即使没有正式添加打印机，也可以直接使用设备 URI ？
    QString cmd = QString("lpadmin -p \"%1\" -E -v \"%2\" -i \"%3\"").arg(cups_info.name).arg(_uri).arg(cups_info.ppd);
    qDebug() << cmd;
    int ret = std::system(cmd.toLatin1().data());
    bool is_cmd_succ = Util::isSystemCmdSucc(ret);
#if OS_TYPE != 2
    if (!is_cmd_succ) {
        qDebug() << "Failed execute command " << cmd;
        return false;
        return true;
    }
#else
    Q_UNUSED(is_cmd_succ)
    // NOTE: 2025-09-10 Ubuntu 20 里用此命令添加打印机会出错：lpadmin: System V interface scripts are no longer supported for security reasons.
#endif

    // 设置默认打印机
    //QString cmd_set_default = QString("lpoptions -d %1").arg(cups_info.name);
    //qDebug() << cmd_set_default;
    //std::system(cmd_set_default.toLatin1().data());
    // TODO: 好像没必要？

    /* 设置上次连接的打印机后
     * 1、自动连接该打印机。若该打印机不存在，自动搜索。
     * 2、下发打印任务时，先尝试自动连接该打印机，若连接失败，才提示打印机未连接。
     */


    //
    if (_uri.isEmpty()) {
        logCritical(QString("%1: URI cannot be empty!").arg(__PRETTY_FUNCTION__));
        return false;
    }

    //




    //
    m_defaultPrinterName = cups_info.name;

    //
    return true;
}

bool CCupsIntf::setPaperSize(enPageSize _paper_size)
{
    if ((pageSize_A4 != _paper_size) && (pageSize_A5 != _paper_size)) {       // 暂定支持 A4、A5
        //logWarning();
        return false;
    }


    //
    return false;

    // TODO:

}

void CCupsIntf::printFile(QString _file_path, bool _is_async)
{
    if (_is_async) {
        emit sigPrintFile(_file_path);
    } else {
        doPrintFile(_file_path);
    }
}

void CCupsIntf::slotPrintFile(QString _file_path)
{
    doPrintFile(_file_path);
}

void CCupsIntf::doPrintFile(QString _file_path)
{
    if (_file_path.contains(CHAR_SPACE)) {
        _file_path = QString(CHAR_QUO) + _file_path + CHAR_QUO;
    }

    QString cmd = QString("lpr -o media=A4 -P \"%1\" \"%2\"").arg(m_defaultPrinterName).arg(_file_path);
    qDebug() << cmd;

    int ret = std::system(cmd.toLatin1().data());       // TODO: 通过 QProcess 执行命令，得到错误输出？
    if (!Util::isSystemCmdSucc(ret)) {
        qDebug() << "Failed to print file " << _file_path;
        emit sigPrintFileFailed(_file_path);
    }
}

int CCupsIntf::getJobsCount()
{
    QProcess process;

    //if (QProcess::NotRunning != process.state()) {
    //    // logWarning();
    //    process.waitForFinished();
    //    if (QProcess::NotRunning != process.state()) {
    //        // logCritical();
    //        process.kill();
    //    }
    //}

    process.setReadChannel(QProcess::StandardOutput);
    process.start("lpstat -o");
    process.waitForFinished(30000);

    //if (QProcess::UnknownError != process.error()) {
    //
    //}

    int count_job = 0;
    QString str_line;
    while (!process.atEnd()) {
        str_line = QString::fromLatin1(process.readLine());
        //qDebug() << str_line;

        if (str_line.length() > 10) {       // TODO: 判断逻辑的完善？
            count_job++;
        }
    }

    return count_job;
}

void CCupsIntf::cancelAllJobs()
{
    QString cmd = QString("cancel -a");
    qDebug() << cmd;
    std::system(cmd.toLatin1().data());
}

bool CCupsIntf::getIsPrinterReady()
{
    return (m_defaultPrinterName.length() > 0);   // TODO: 还要检查该打印机是否在线(lpstat -p <printername> ?)
}

enPrinterStatus CCupsIntf::getPrinterStatus()
{
    QProcess process;
    QString command = "lpstat";
    QStringList arguments;
    arguments << "-p" << m_defaultPrinterName;

    process.start(command, arguments);
    if (!process.waitForFinished(3000)) { // 等待3秒
        qWarning() << "命令执行超时！可能打印机掉线。";
        return enPrinterStatus::Offline;// 认为打印机状态不可用
    }

    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();

    if (!errorOutput.isEmpty()) {
        qWarning() << "错误输出:" << errorOutput;
    }
/*
    // 将输出按行分割
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    qDebug() << output;
    // 确保有第二行
    if (lines.size() >= 2) {
        output = lines.last(); // 提取最后一行非空行状态信息
    }
*/
    if (output.contains("unreachable", Qt::CaseInsensitive)) {
        qDebug() << "打印机不可访问。";
        return enPrinterStatus::Unreachable;
    }
    else if (output.contains("not responding", Qt::CaseInsensitive)) {
        qDebug() << "打印机无应答，请检查设备。";
        return enPrinterStatus::NoResponding;
    }
    else if (output.contains("waiting for printer to become available", Qt::CaseInsensitive)) {
        qDebug() << "等待打印机可用。";
        return enPrinterStatus::WaitingForavilable;
    }
    else if (output.contains("error", Qt::CaseInsensitive)) {
        qDebug() << "打印机出现错误，请检查设备。";
        return enPrinterStatus::Error;
    }
    else if (output.contains("paused", Qt::CaseInsensitive)) {
        qDebug() << "打印机已暂停。";
        return enPrinterStatus::Paused;
    }
    else if (output.contains("stopped", Qt::CaseInsensitive)) {
        qDebug() << "打印机已停止，请检查设备。";
        return enPrinterStatus::Stopped;
    }
    else if (output.contains("spooling job", Qt::CaseInsensitive)) {
        qDebug() << "任务正在排队或发送。";
        return enPrinterStatus::Spooling;
    }
    else if (output.contains("processing", Qt::CaseInsensitive)) {
        qDebug() << "打印机正在处理任务。";
        return enPrinterStatus::Processing;
    }
    else if (output.contains("printing", Qt::CaseInsensitive)) {
        qDebug() << "打印机正在打印。";
        return enPrinterStatus::Printing;
    }
    else if (output.contains("idle", Qt::CaseInsensitive)) {
        qDebug() << "打印机处于空闲状态，可以接收任务。";
        return enPrinterStatus::Idle;
    }
    else if (output.contains("offline", Qt::CaseInsensitive)) {
        qDebug() << "打印机已离线，请检查网络连接。";
        return enPrinterStatus::Offline;
    }
    else if (output.contains("held", Qt::CaseInsensitive)) {
        qDebug() << "打印任务已挂起。";
        return enPrinterStatus::Held;
    }
    else if (output.contains("waiting for authentication", Qt::CaseInsensitive)) {
        qDebug() << "打印机等待用户认证。";
        return enPrinterStatus::WaitingForAuth;
    }
    else {
        qDebug() << "未知打印机状态。";
        return enPrinterStatus::Unknown;
    }
}

}   // namespace Common
