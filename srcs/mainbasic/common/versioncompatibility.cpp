#include "versioncompatibility.h"

#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include <QApplication>

#include "logger.h"
#include "mysqlitepatients.h"
#include "global.h"
#include "sysinfo.h"
#include "util-app.h"

//
QSettings *g_appConfig = Q_NULLPTR;
QSettings *getAppConfig()
{
    const static QString FILE_PATH_APP_INFO = qApp->applicationDirPath() + QDir::separator() + qApp->applicationName() + "_app.ini";

    //
    if (!g_appConfig) {
        g_appConfig = new QSettings(FILE_PATH_APP_INFO, QSettings::IniFormat);
    }
    return g_appConfig;
}

//
CVersionCompatibility::CVersionCompatibility(QObject *_parent) : QObject(_parent)
{
    /* 注意：版本号要升序排序 */

    // 2023-05-06 09:39 提交的代码将历史记录表 HistoryPatients 的表名改为 HistoryPatients
    listVerAndProcess.append(stVerAndProcess {
                                 stVerInfoApp {stVerInfo {1, 5, 5}, QDate(2023, 7, 31), 0},
                                 &CVersionCompatibility::afterUpdate_20230731
                             });

    // 2023-09-04 19:25 提交的代码，合并了最新的算法代码，需要将 opencv 库由 2.4 升级到 3.4
    //listVerAndProcess.append({{1, 5, 7,     QDate(2023, 9, 8), 0},      &CVersionCompatibility::afterUpdate_20230908});   /* 这个没法在这里做，因为在旧版 opencv 环境中根本无法启动本程序 */

    // 2023-10-20 开始，版本号升级到 v1.5.9，新增了对门诊的需求，编号允许重复（一般情况下，本次升级应该递增次版本号，但是由于网络更新的的路径与版本号挂钩，且版本选择机制未完善，不便修改次版本号，所以暂不递增次版本号）
    listVerAndProcess.append(stVerAndProcess {
                                 stVerInfoApp {stVerInfo {1, 5, 9}, QDate(2023, 10, 20), 0},
                                 &CVersionCompatibility::afterUpdate_20231020
                             });

}

void CVersionCompatibility::processAfterUpdate()
{
    // 得到配置版本号（指配置文件中记录的版本号）
    stVerInfoApp ver_info_update = CVersionCompatibility::getAppVerInfoOfUpdate();

    // 在列表中找出所有大于配置版本号的“升级版本号”
    const int NOT_FOUND = -100;
    int index_from = NOT_FOUND;
    for (int i = listVerAndProcess.count() - 1; i >= 0; i--) {
        const stVerInfoApp &ver_info = listVerAndProcess[i].verInfo;
        if (ver_info.compareWith(ver_info_update) <= 0) {   /* 第一个小于等于配置版本号的元素的下一个元素之后的所有元素，满足条件 */
            index_from = i + 1;
            break;
        }
    }
    if (NOT_FOUND == index_from) {      // 若找不到小于等于配置版本号的“升级版本号”，则全部都需处理
        index_from = 0;
    }

    // 若所有“升级版本号”都小于等于配置版本号，则不需处理
    if (index_from > listVerAndProcess.count() - 1) {       /* 前面的遍历过程，若所有“升级版本号”都小于等于配置版本号，则 index_from = listVerAndProcess.count() - 1 + 1 */
        return;
    }

    // 显示等待信息
    MessageWin win_msg;
    win_msg.setWindowModality(Qt::WindowModal);
    win_msg.setContent(tr("正在进行程序更新后的配置..."));  // "Configuring after program updated..."
    win_msg.setButtonEnable(false);
    win_msg.show();

    Util::waitMs(1500);     // 使本过程明显化，避免异常重复执行而一直未被发现

    //
    for (int i = index_from; i < listVerAndProcess.count(); i++) {
        listVerAndProcess[i].process();
    }

    // 隐藏等待消息
    win_msg.hide();

    // 记录最后一个“升级版本号”到配置文件
    stVerInfoApp ver_last_upgrade = listVerAndProcess[listVerAndProcess.size() - 1].verInfo;
    CVersionCompatibility::setAppVerInfoOfUpdate(ver_last_upgrade);

}

stVerInfoApp CVersionCompatibility::getAppVerInfoOfUpdate()
{
    //getInstance()->mutex->lock();
    QString ver_str = getAppConfig()->value("/app/currentSupportVer").toString();
    //getInstance()->mutex->unlock();

    //
    if (ver_str.length() == 0) {
        ver_str = CSysInfo::getAppVerOrigin();
    }

    //
    stVerInfoApp ver_info;
    aboutdevice::strToVersionInfo(ver_str, ver_info, 0);
    return ver_info;
}

void CVersionCompatibility::setAppVerInfoOfUpdate(const stVerInfoApp &_ver_info)
{
    QString ver_str = aboutdevice::versionInfoToStr(_ver_info);

    //
    //getInstance()->mutex->lock();
    getAppConfig()->setValue("/app/currentSupportVer", ver_str);
    getAppConfig()->sync();
    //getInstance()->mutex->unlock();
}

bool CVersionCompatibility::afterUpdate_20230731()
{
    //
    MySQLitePatients *mysqlite = MySQLitePatients::getInstance();
    QSqlQuery query(mysqlite->m_dbconn);

    // 先检查确认数据库版本不是最新的
    QString sql_count_old_table = "select count(*) from sqlite_master where name='HistroyPatients' ";

    if (query.prepare(sql_count_old_table)) {
        if (query.exec()) {
            if (query.first() && query.value(0).toInt() == 0) {
                logWarning(QString(__PRETTY_FUNCTION__) + ": 旧表名找不到，跳过处理过程");
                return true;     // 若旧表名不存在，则不需处理
            }
        } else {
            logCritical(QString(__PRETTY_FUNCTION__) + ": 数据库异常：准本表名查询失败");
            return false;
        }
    } else {
        logCritical(QString(__PRETTY_FUNCTION__) + ": 数据库异常：执行表名查询失败");
        return false;
    }

    // 先备份数据库文件
    QString file_path_fmt = MySQLitePatients::getDatabaseDir() + QDir::separator() + "patients%1.db";
    QString file_path_origin = file_path_fmt.arg("");
    //QString file_path_backup = file_path_fmt.arg(QString("_") + QString::number(aboutdevice::getAppVerInfoOfUpdate().verBuild));
    QString file_path_backup = file_path_fmt.arg(QDateTime::currentDateTime().toString("_yyyyMMdd_hhmm"));
    if (QFile::exists(file_path_origin) /*&& !QFile::exists(file_path_backup)*/) {
        QFile file(file_path_origin);
        file.copy(file_path_backup);
        file.close();
    }

    // 数据库兼容处理
    /* 2023-05-06 09:39 提交的代码将历史记录表 HistoryPatients 的表名改为 HistoryPatients */
    QString sql_upgrade;
    bool is_db_err = false;

    sql_upgrade = "begin transaction ; ";
    if (!is_db_err && !MySQLitePatients::execSql(query, sql_upgrade)) {
        logCritical(QString(__PRETTY_FUNCTION__) + ": upgrade database failed: " + sql_upgrade);
        is_db_err = true;
    }
    sql_upgrade =
            "insert into HistoryPatients (patientid, patientname, patientagerange, patientsex, patientdate, patientlefteyesph, patientlefteyecyl, patientlefteyeax, patientleftse, patientleftpd, patientleftptosis, patientlefthorizontalstrabismushz, patientleftverticalstrabismus, patientrighteyesph, patientrighteyecyl, patientrighteyeax, patientrightse, patientrightpd, patientrightptosis, patientrighthorizontalstrabismus, patientrightverticalstrabismus, patientpd, patientstuclass, patienttesttime, patientphone, patientaddress, patientwechat, barcodedata, batchNo, comment1, comment2, istest, isbatch, isneedupload, isuploaded, isneedimage, isuploadedimage, creattime ) "
            "select patientid, patientname, patientagerange, patientsex, patientdate, patientlefteyesph, patientlefteyecyl, patientlefteyeax, patientleftse, patientleftpd, patientleftptosis, patientlefthorizontalstrabismushz, patientleftverticalstrabismus, patientrighteyesph, patientrighteyecyl, patientrighteyeax, patientrightse, patientrightpd, patientrightptosis, patientrighthorizontalstrabismus, patientrightverticalstrabismus, patientpd, patientstuclass, patienttesttime, patientphone, patientaddress, patientwechat, barcodedata, batchNo, comment1, comment2, istest, isbatch, isneedupload, isuploaded, isneedimage, isuploadedimage, creattime "
            "  from HistroyPatients ";
    //qDebug() << sql_upgrade;
    if (!is_db_err && !MySQLitePatients::execSql(query, sql_upgrade)) {
        logCritical(QString(__PRETTY_FUNCTION__) + ": upgrade database failed: " + sql_upgrade);
        is_db_err = true;
    }
    sql_upgrade = "drop table HistroyPatients ; ";
    if (!is_db_err && !MySQLitePatients::execSql(query, sql_upgrade)) {
        logCritical(QString(__PRETTY_FUNCTION__) + ": upgrade database failed: " + sql_upgrade);
        is_db_err = true;
    }
    sql_upgrade = "commit transaction ; ";
    if (!is_db_err && !MySQLitePatients::execSql(query, sql_upgrade)) {
        logCritical(QString(__PRETTY_FUNCTION__) + ": upgrade database failed: " + sql_upgrade);
        is_db_err = true;
    }

    if (is_db_err) {
        sql_upgrade = "rollback ; ";
        MySQLitePatients::execSql(query, sql_upgrade);
        return false;
    }

    // 配置文件兼容处理
    /* 第一个 rk3568 固件日期是 2023-04-15，即 rk3568 机器的程序版本日期至少大于此日期。
     * 对比了 v1.5.4_20230322_96 和最新发布版 v1.5.5_20230728 生成的配置文件，得到本过程需要兼容的字段：
     * data/isIgnoreDist                -> ? （略过，这个配置没意义）
     * tool/btprint, tool/wifiprint     -> tool/ticketPrintConnType     （也略过，这个常用配置应该问题不大）
     *
     */


    //
    return true;
}

bool CVersionCompatibility::afterUpdate_20230908()
{
    /* 2023-09-04 19:25 提交的代码，合并了最新的算法代码，需要将 opencv 库由 2.4 升级到 3.4 */
    /* 处理过程：执行脚本“change_opencv_lib_3.4”，即可。
     */

    // 执行创建链接的脚本
    QString cmd_create_ln = "/root/change_opencv_lib_3.4";
    QString std_out;
    bool is_sh_succ = Util::executeLinuxCmd(cmd_create_ln, &std_out);
    if (!is_sh_succ) {
        //

    }

    //
    return true;
}

bool CVersionCompatibility::afterUpdate_20231020()
{
    do {
        // 1、旧的非筛查记录新增一条作为门诊档案      // TODO: 这种单表数据结构应优化？
        // TODO:
        /* 不做也问题不大，略过 */

        //
        MySQLitePatients *mysqlite = MySQLitePatients::getInstance();
        std::vector<CPatient> pats = mysqlite->findTableInfo(-1);

        // 2、测试时间加上秒
        for (size_t i = 0; i < pats.size(); i++) {
            CPatient &pat = pats.at(i);

            if (pat.patienttesttime.length() == 16) {
                pat.patienttesttime += ":00";
            }
        }
        mysqlite->TableModify(pats);

        // 3、旧的存图按照新的命名规则改名，并删掉报表文件（之后用到时会自动重建）
        for (size_t i = 0; i < pats.size(); i++) {
            CPatient &pat = pats.at(i);

            if (!pat.isTest) {
                continue;
            }

            // 存图文件夹重命名
            {
                QString tpl_path_photo = QString("/media/photo/%1");
                QString path_photo = tpl_path_photo.arg(pat.patientid);
                if (QFile::exists(path_photo)) {
                    QString img_dir_name = pat.getImgDirName();
                    QString path_photo_new = tpl_path_photo.arg(img_dir_name);
                    QString cmd = QString("mv %1 %2").arg(path_photo).arg(path_photo_new);
                    system(cmd.toLatin1().data());
                }
            }

            // 预览图重命名
            {
                QString tpl_path_preview = QString("/media/pdfPreviewImg/%1_pdfPreview.jpg");
                QString path_preview = tpl_path_preview.arg(pat.patientid);
                if (QFile::exists(path_preview)) {
                    QString img_dir_name = pat.getImgDirName();
                    QString path_preview_new = tpl_path_preview.arg(img_dir_name);
                    QString cmd = QString("mv %1 %2").arg(path_preview).arg(path_preview_new);
                    system(cmd.toLatin1().data());
                }
            }

            // 删除报表
            {
                QString tpl_path_report = QString("/media/reports/%1_result.pdf");
                QString path_report = tpl_path_report.arg(pat.patientid);
                if (QFile::exists(path_report)) {
                    QString cmd = QString("rm %1").arg(path_report);
                    system(cmd.toLatin1().data());
                }
            }
        }

        //
        return true;
    } while (false);

    //
    return false;
}
