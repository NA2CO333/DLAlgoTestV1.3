//数据库的创建
#include "mysqlite.h"

#include <QSqlQuery>
#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QSqlError>

#include "logger.h"

//
QSqlDatabase MySQLite::m_dbconn;

QString MySQLite::DatabaseDir;                      // 数据库目录
const QString DATABASE_NAME = "patients.db";        // 数据库文件名

//
MySQLite::MySQLite()
{
    //
    if (DatabaseDir.length() == 0) {
        DatabaseDir = qApp->applicationDirPath();
    }

    //
    qDebug()<<"MySQLite creating QSqlDatabase";
    QString err_msg;
    bool succ_close = openDatabase(err_msg);
    if (!succ_close) {
        qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): Failed to open database: " << err_msg;
    }
}

MySQLite::~MySQLite()
{
    qDebug() << "~MySQLite into";
    QString err_msg;
    bool succ_close = closeDatabase(err_msg);
    if (!succ_close) {
        qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): Failed to close database: " << err_msg;
    }
    qDebug() << "MySQLite::~MySQLite() out";
}

bool MySQLite::openDatabase(QString &_err_msg)
{
    logDebug(QString("%1::%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    QString file_path = databaseFilePath();
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        qDebug()<<"MySQLite  not connect";
        m_dbconn = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        qDebug()<<"MySQLite start connect";
        m_dbconn = QSqlDatabase::addDatabase("QSQLITE");    //添加SQLite数据库驱动
        m_dbconn.setDatabaseName(file_path);  //在工程目录新建一个patients.db的文件
    }
    if(!m_dbconn.open())    {
        qDebug()<<"open "<<file_path<<" failed!";
        _err_msg = m_dbconn.lastError().text();
    }

    // 执行连接级配置
    QSqlQuery query(m_dbconn);
    bool succ_wal = query.exec(SQL::SYNCHRONOUS_NORMAL);
    if (!succ_wal) {
        qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): Failed to exec SQL '" << SQL::SYNCHRONOUS_NORMAL
                    << "', err: " << query.lastError().text();
    }

    //
    return true;
}

bool MySQLite::closeDatabase(QString &_err_msg)
{
    logDebug(QString("%1::%2(): entered ...").arg(S_CLASS_NAME).arg(__FUNCTION__));

    Q_UNUSED(_err_msg)

    m_dbconn.close();
    QString conn_name = (QLatin1String(m_dbconn.defaultConnection));
    QSqlDatabase::removeDatabase(conn_name);

    return true;
}

QString MySQLite::databaseFilePath()
{
    return (DatabaseDir + QDir::separator() + DATABASE_NAME);
}

QString MySQLite::databaseFileName()
{
    return DATABASE_NAME;
}
