#ifndef OTAUPDATEDEFS_H
#define OTAUPDATEDEFS_H

/* OTA 升级的基本常量、函数定义。 */

/* ota-update 程序的部署：
 * 2023/08/14
 *
 * 1、在 "/ota" 目录下部署程序文件 ota-update 。
 *
 * 2、在 "/ota" 目录下创建系统配置备份模板文件 backup-internal 和 用户数据备份模板文件 backup-user（见 databackup.cpp 的宏定义 BACKUP_TEMPLATE_INTERNAL 和 BACKUP_TEMPLATE_USER）。
 *
 *   备份模板 backup-internal 和 backup-user 是纯文本文件，其中每一行都是一个需备份的文件或文件夹的绝对路径。
 *
 * 3、ota 状态文件："/ota/ota-stat" 的部署规则：
 *（1）如果是制作用于 OTA 更新的固件镜像，则在制作 rootfs 分区镜像时，须删除该文件（"rm /ota/ota-stat"），否则 OTA 更新完第一次重启时不会自动执行文件还原。
 *（2）其它情况下，比如制作用于使用烧录工具烧录的固件镜像，或者是部署网络升级程序，则须创建 ota 状态文件（"echo '' > /ota/ota-stat"），
 *    否则重启后系统将误以为当前状态是 ota 更新后的第一次启动，自动启动数据还原。
 *
 */

/* 固件更新包的格式及路径要求：
 * 2023/08/14
 *
 * 1、固件更新包须放在 U 盘根目录，不能放在文件夹内。
 *
 * 2、固件更新包的格式是："*.tar.gz"。
 *
 * 3、固件更新包内包含以下文件（不可包含文件夹）：
 *    固件镜像文件：   "<固件包文件主名>.img"；
 *    镜像文件的校验码："<固件包文件主名>.md5"；
 *    固件信息附注文件："*.txt"（非必须，仅供用户查看，程序不会访问）。
 *
 *    .tar.gz 文件、.img 文件和 .md5 文件的文件主名（不含扩展名）必须相同。
 *
 * 4、md5 文件的创建 ：
 *    md5sum <固件包文件主名>.img > <固件包文件主名>.md5
 *
 *   上述命令所得 md5 文件为纯文本文件，文件内只有一行内容，以一段十六进制表示的md5校验和开头，后接空格，再接文件名。
 *
 */


// 程序板本
#define APP_VER_MAJOR   1
#define APP_VER_MINOR   1
#define APP_VER_PATCH   1
#define APP_VER_DATE    "20230921"

//#define APP_VER_BUILD   1

// OTA 命令参数：显示版本号
#define OTA_PARAM_VERSION           "-v"

// OTA 命令参数：固件更新
#define OTA_PARAM_OTA_UPDATE        "--ota_update"

// OTA 命令参数：OTA 数据还原
#define OTA_PARAM_OTA_RESTORE       "--ota_restore"

// OTA 命令参数：用户 数据还原
#define OTA_PARAM_USER_RESTORE      "--user_restore"

// OTA 命令参数：用户 数据备份
#define OTA_PARAM_USER_BACKUP       "--user_backup"

// OTA 命令参数：语言（缺省时表示语言为中文，后接参数 e 时表示语言为英文）
#define OTA_PARAM_LANGUAGE      "--language"

// OTA 程序目录
#define OTA_APP_DIR             "/ota"

// OTA 程序名
#define OTA_APP_NAME            "ota-update"

// OTA 状态文件名
#define OTA_STAT_FILE_NAME      "ota-stat"

//
#include <QFile>
#include <QString>
#include <QStringLiteral>
#include <QDir>

// OTA 处理结果状态
enum enOtaProcessStat
{
    otaProcessStat_Succ,            // 成功
    otaProcessStat_Fail,            // 失败，后续采用通过过程
    otaProcessStat_FailHandled,     // 失败，但内部已做处理
};

//
namespace OtaUpdate
{
    // 判断是否需要进行 OTA 更新后的数据还原
    bool isNeedOtaRestore();

    // 标记 OTA 更新后的数据还原已执行
    void markOtaRestoreExecuted();

    // 启动 OTA 升级
    bool runOtaUpdate();

    // 启动 OTA 数据还原
    bool runOtaRestore();

    // 启动 用户 数据备份
    bool runUserBackup();

    // 启动 用户 数据还原
    bool runUserRestore();


    // 设置【语言是否为中文】
    void setIsChinese(bool _is_chinese);

    // 设置【备份文件前缀】
    void setBackupFilePrefix(const QString &_prefix);

    // 获取【备份文件前缀】
    QString getBackupFilePrefix();

    // 得到磁盘剩余空间（传入磁盘根路径或磁盘中任一文件的路径。若返回值小于 0，则表示失败）
    float getDiskRemainSpace(QString _udisk_path);

    // 得到文件备份要求空间
    float getBackupRequireSpace();

};

#endif // OTAUPDATEDEFS_H
