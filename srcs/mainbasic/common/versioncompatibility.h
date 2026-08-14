#ifndef CVERSIONCOMPATIBILITYP_H
#define CVERSIONCOMPATIBILITYP_H

/**************************
 * 版本兼容处理的原理：
 * （1）程序每次启动时，都执行一次版本兼容检查及处理过程（下称“兼容处理过程”）。
 * （2）“代码版本号”：代码中的版本号，即本程序的当前版本号，是固化在代码中的。
 * （3）“配置版本号”：保存在配置文件中的版本号，由“兼容处理过程”在执行完后设置，表示当前系统环境已经兼容到此版本。
 * （4）“升级版本号”：表示从低版本升级到该版本时须执行“兼容处理过程”的一个“临界”版本号。
 * （5）“兼容处理函数”：与“升级版本号”一一对应，表示由低版本升级到该版本时须进行必要处理。
 * （6）“兼容处理列表”：多个“升级版本号”和其对应的“兼容处理函数”一一对应构成的列表。
 * （7）“兼容处理过程”所做的事情：在“兼容处理列表”中找到高于（不包含等于）“配置版本号”的各个“升级版本号”，并从低到高逐个执行它所对应的“兼容处理函数”。
 * （8）“兼容处理过程”在其执行完后，设置了“配置版本号”为当前的“代码版本号”，表示当前系统环境已经兼容此版本。
 *
 * 新增版本兼容处理的方法：
 * （1）在上述“兼容处理列表”中新增“升级版本号”（可用实际最低临界版本号，或当前版本号），以及对应的“兼容处理函数”。
 * （2）实现新增的“兼容处理函数”。
 */

// NOTE: 数据库的表结构更新，在 MySQLitePatients::initDatabase() 里有执行。
// TODO: 有没有必要合并 MySQLitePatients::initDatabase() 里的操作到本类？

//
#include <QSqlQuery>

#include "aboutdevice.h"

// 版本兼容处理
class CVersionCompatibility : public QObject
{
    Q_OBJECT
public:
    explicit CVersionCompatibility(QObject *_parent = nullptr);

    void processAfterUpdate();          // “兼容处理过程”，检查并执行程序升级后必要执行的兼容处理过程       // TODO: 执行失败后的处理或版本回退？

    static stVerInfoApp getAppVerInfoOfUpdate();                        // 获得【APP的升级版本号信息】
    static void setAppVerInfoOfUpdate(const stVerInfoApp &_ver_info);   // 设置【APP的升级版本号信息】

protected:
    typedef bool (*funcProcess)();

    struct stVerAndProcess {        // “升级版本号”及其“兼容处理函数”
        stVerInfoApp verInfo;       // “升级版本号”（表示从低版本升级到该版本时须执行“兼容处理过程”的一个“临界”版本号）
        funcProcess process;        // “兼容处理函数”（从低版本往上升级时需执行的过程）
    };

    QList<stVerAndProcess> listVerAndProcess;   // “兼容处理列表”，即“升级版本号”和对应的处理过程列表（须按版本号升序排序）

    static bool afterUpdate_20230731();
    static bool afterUpdate_20230908();
    static bool afterUpdate_20231020();

};

#endif // CVERSIONCOMPATIBILITYP_H
