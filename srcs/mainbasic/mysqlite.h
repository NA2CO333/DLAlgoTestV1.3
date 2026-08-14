#ifndef MYSQLITE_H
#define MYSQLITE_H
#include <QSqlDatabase>
#include <QObject>
#include <QSqlQueryModel>

//
namespace SQL {

// SQL: 启用 WAL（支持读写并发），库级永久生效
inline constexpr char JOURNAL_MODE_WAL[]       = "PRAGMA journal_mode=WAL;";

// SQL: 设置磁盘同步策略为 NORMAL（性能平衡），连接级生效
inline constexpr char SYNCHRONOUS_NORMAL[]     = "PRAGMA synchronous=NORMAL;";

}   // namespace SQL

/* 
 * parent class 
 * function 
 * [1]打开或新建DB 
 * [2]关闭DB 
 * */  
class MySQLite:public QObject
{
public:  
    ~MySQLite();  

    //QSqlDatabase m_dbconn;    //2020.10.12屏蔽  tao
    static QSqlDatabase m_dbconn;

    bool openDatabase(QString &_err_msg);               // 打开数据库
    bool closeDatabase(QString &_err_msg);              // 关闭数据库

    QString databaseFilePath();                         // 数据库文件路径
    QString databaseFileName();                         // 数据库文件名

protected:
    MySQLite();
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    static QString DatabaseDir;     // 数据库目录

};

#endif
