//数据库的操作,数据表创建,数据查询,数据插入及数据删除
#include "mysqlitepatients.h"

#include <QDebug>
#include <QSqlQuery>
#include <QStringList>
#include <QTime>
#include <QSqlError>

#include "global.h"

//
const char *KEYWORD_NON_UPLOADED_CN    = "未上传";                   // 搜索关键字：未上传（中文 Chinese
const char *KEYWORD_NON_UPLOADED_EN    = "not uploaded";            // 搜索关键字：未上传（英文 English）
const char *KEYWORD_NON_UPLOADED_DE    = "nicht hochgeladen";       // 搜索关键字：未上传（德文 Deutsch）

const QString TABLE_NAME_HISTORY = "HistoryPatients";       // 数据库表名：历史记录

// 建表 sql   // TODO: 放到独立的文件方便管理版本变更
const static QString SQL_CREATE_TABLE = QString() +
        "CREATE TABLE " + TABLE_NAME_HISTORY + " ( " +
        "id INTEGER PRIMARY KEY AUTOINCREMENT " +
        ", patientid varchar(32) "+
        ", patientname varchar(32) " +
        ", patientagerange varchar(32) "  +
        ", patientsex varchar(32) " +
        ", patientdate varchar(32) " +
        ", patientlefteyesph varchar(32) " +
        ", patientlefteyecyl varchar(32) " +
        ", patientlefteyeax varchar(32) " +
        ", patientleftse vatchar(32) " +
        ", patientleftpd varchar(32) " +
        ", patientleftptosis int " +                            //上睑下垂
        ", patientlefthorizontalstrabismushz varchar(32) " +    //斜视
        ", patientleftverticalstrabismus varchar(32) " +        //斜视
        ", patientrighteyesph varchar(32) " +
        ", patientrighteyecyl varchar(32) " +
        ", patientrighteyeax varchar(32) " +
        ", patientrightse varchar(32) "+                        //edit by sun 20180808
        ", patientrightpd varchar(32) " +
        ", patientrightptosis int " +                           //上睑下垂
        ", patientrighthorizontalstrabismus varchar(32) " +     //斜视
        ", patientrightverticalstrabismus varchar(32) " +       //斜视
        ", patientpd varchar(32) " +
        ", patientstuclass varchar(32) " +
        ", patienttesttime varchar(32) " +
        ", patientphone varchar(32) "+
        ", patientaddress varchar(32) "+
        ", patientwechat varchar(32) "+
        ", barcodedata varchar(32) "+
        ", batchNo int "+
        ", comment1 varchar(32) "+
        ", comment2 varchar(32) "+
        ", istest int " +
        ", isbatch int " +
        ", isneedupload int " +
        ", isuploaded int "+
        ", isneedimage int "+
        ", isuploadedimage int "+
        ", creattime varchar(32) " +                            //创建时间  2020.11.30  tao
        //", source int " +                           // 被测者基本信息的来源：0 本地创建，1 外部传入
        //", grade varchar(32) " +                                // 年级
        ", IS_MULTI         REAL    NULL " +
        ", RESULT_1_R_SPH   REAL    NULL " +
        ", RESULT_1_R_CYL   REAL    NULL " +
        ", RESULT_1_R_AX    REAL    NULL " +
        ", RESULT_1_L_SPH   REAL    NULL " +
        ", RESULT_1_L_CYL   REAL    NULL " +
        ", RESULT_1_L_AX    REAL    NULL " +
        ", RESULT_2_R_SPH   REAL    NULL " +
        ", RESULT_2_R_CYL   REAL    NULL " +
        ", RESULT_2_R_AX    REAL    NULL " +
        ", RESULT_2_L_SPH   REAL    NULL " +
        ", RESULT_2_L_CYL   REAL    NULL " +
        ", RESULT_2_L_AX    REAL    NULL " +
        ", RESULT_3_R_SPH   REAL    NULL " +
        ", RESULT_3_R_CYL   REAL    NULL " +
        ", RESULT_3_R_AX    REAL    NULL " +
        ", RESULT_3_L_SPH   REAL    NULL " +
        ", RESULT_3_L_CYL   REAL    NULL " +
        ", RESULT_3_L_AX    REAL    NULL " +
        ");";

QString MySQLitePatients::databaseVersion = "1.0";      // NOTE: 初始化为最开始的版本号，程序启动后检查数据库时再更新版本号

QMutex MySQLitePatients::mutex;

//
MySQLitePatients* MySQLitePatients::mySqlPatients = Q_NULLPTR;

/*
* pengliqiang
* 创建表以及增删改查
*/
MySQLitePatients::MySQLitePatients() : MySQLite()
{
    initDatabase();
}

MySQLitePatients::~MySQLitePatients()
{

}

bool MySQLitePatients::initDatabase()
{
    /* SQLITE 3 的 4 种字段类型：
     * INTEGER：用于存储整数。SQLite 支持多种整数大小，包括 1、2、3、4、6 或 8 字节。
     * REAL：用于存储浮点数（双精度），通常以 8 字节的 IEEE 浮点格式存储。
     * TEXT：用于存储文本字符串，采用 UTF-8 编码，可以保存任意长度的字符串。
     * BLOB：表示二进制大对象（Binary Large Object），可以存储任何数据，包括图像和文件等。
     */

    //
    QSqlQuery query(m_dbconn);

    // 数据表检查处理
    QStringList list_tables = m_dbconn.tables();
    if (list_tables.contains(TABLE_NAME_HISTORY)) {
        // 旧版数据表更新
        QString sql_tables_info = QString("select * from sqlite_master where name='%1' ").arg(TABLE_NAME_HISTORY);
        query.prepare(sql_tables_info);
        if (query.exec() && query.first()) {
            QString sql_scripts = query.value("sql").toString();

            do {
                // v1.1: 增加了 "creattime"（创建时间）字段
                {
                    // 若没有 "creattime"（创建时间）字段，则添加
                    static const QString FIELD_CREATTIME = "creattime";
                    bool has_field = sql_scripts.contains(" " + FIELD_CREATTIME + " ");
                    if (!has_field) {
                        if (addFieldToTable(FIELD_CREATTIME, "TEXT", true, TABLE_NAME_HISTORY, query)) {
                            logDebug(QString("added field '%1' to table '%2'").arg(FIELD_CREATTIME).arg(TABLE_NAME_HISTORY),
                                     CGlobal::LOG_DATABASE);
                        } else {
                            logCritical(QString("field to add field '%1' to table '%2'!").arg(FIELD_CREATTIME).arg(TABLE_NAME_HISTORY),
                                        CGlobal::LOG_DATABASE);
                            break;
                        }
                    }
                    databaseVersion = "1.1";
                }

                // v1.2: 增加了 "grade"（年级）字段
                //{
                //    // 若没有 "grade"（年级）字段，则添加
                //    static const QString FIELD_GRADE = "grade";
                //    bool has_field = sql_scripts.contains(" " + FIELD_GRADE + " ");
                //    if (!has_field) {
                //        if (addFieldToTable(FIELD_GRADE, "TEXT", true, TABLE_NAME_HISTORY, query)) {
                //            logDebug(QString("added field '%1' to table '%2'").arg(FIELD_GRADE).arg(TABLE_NAME_HISTORY),
                //                     CGlobal::LOG_DATABASE);
                //        } else {
                //            logCritical(QString("field to add field '%1' to table '%2'!").arg(FIELD_GRADE).arg(TABLE_NAME_HISTORY),
                //                        CGlobal::LOG_DATABASE);
                //            break;
                //        }
                //    }
                //    databaseVersion = "1.2";
                //}
                /*  年级和班别分开的工作量不小，暂停。其实视筛仪里并不一定需要分开，因为这里只需要查看，没必要非要从这里导出。 2021-12-17 */

                // v1.3: 增加了 "IS_MULTI"（是否多次测量）字段，及3次测量结果的左、右眼的 SPH,CYL, AX 字段
                {
                    // 若没有 "IS_MULTI"（是否多次测量）等字段，则添加
                    static const QStringList FIELD_NAMES = QStringList()
                            << "IS_MULTI"

                            << "RESULT_1_R_SPH"
                            << "RESULT_1_R_CYL"
                            << "RESULT_1_R_AX"
                            << "RESULT_1_L_SPH"
                            << "RESULT_1_L_CYL"
                            << "RESULT_1_L_AX"

                            << "RESULT_2_R_SPH"
                            << "RESULT_2_R_CYL"
                            << "RESULT_2_R_AX"
                            << "RESULT_2_L_SPH"
                            << "RESULT_2_L_CYL"
                            << "RESULT_2_L_AX"

                            << "RESULT_3_R_SPH"
                            << "RESULT_3_R_CYL"
                            << "RESULT_3_R_AX"
                            << "RESULT_3_L_SPH"
                            << "RESULT_3_L_CYL"
                            << "RESULT_3_L_AX"
                               ;

                    QString field_name;
                    bool has_field;
                    for (int i = 0; i < FIELD_NAMES.size(); i++) {
                        field_name = FIELD_NAMES.at(i);
                        has_field = sql_scripts.contains(" " + field_name + " ");
                        if (!has_field) {       // 若没有 "IS_MULTI"（是否多次测量）字段，则添加
                            if (addFieldToTable(field_name, "REAL", true, TABLE_NAME_HISTORY, query)) {
                                logDebug(QString("added field '%1' to table '%2'").arg(field_name).arg(TABLE_NAME_HISTORY),
                                         CGlobal::LOG_DATABASE);
                            } else {
                                logCritical(QString("field to add field '%1' to table '%2'!").arg(field_name).arg(TABLE_NAME_HISTORY),
                                            CGlobal::LOG_DATABASE);
                                break;
                            }
                        }
                    }
                    databaseVersion = "1.3";
                }

                //
                return true;
            } while (false);
        } else {
            logCritical("MySQLitePatients::initTable(): Query tables info failed!", CGlobal::LOG_DATABASE);
            // TODO: 提示错误，机器无法使用？
        }
        qDebug() << "-----table exists";
    } else {                                                // 若不包含历史记录表，则创建
        qDebug() << "-----table does not exist";
        if (query.exec(SQL_CREATE_TABLE)) {
            qDebug()<<"-----Create Table Success!\n";
            return true;
        } else {
            qDebug()<<"-----Create Table failed!\n";
        }
    }

    // 执行数据库级配置
    bool succ_wal = query.exec(SQL::JOURNAL_MODE_WAL);
    if (!succ_wal) {
        qCritical() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): Failed to exec sql '" << SQL::JOURNAL_MODE_WAL
                    << "', err: " << query.lastError().text();
    }

    //
    return false;
}

bool MySQLitePatients::addFieldToTable(const QString &_field_name, const QString &_field_type, bool _is_nullable,
                                       const QString &_table_name, QSqlQuery &_query)
{
    QString sql_str = QString("ALTER TABLE %1 ADD %2 %3 %4")
            .arg(_table_name).arg(_field_name).arg(_field_type)
            .arg(_is_nullable ? "NULL" : "NOT NULL");
    _query.prepare(sql_str);
    if (_query.exec()) {
        logDebug("Succeeded to add field \"" + _field_name + "\".", CGlobal::LOG_DATABASE);
        return true;
    } else {
        logCritical("Failed to execute sql: " + sql_str, CGlobal::LOG_DATABASE);
        return false;
    }
}

MySQLitePatients* MySQLitePatients::getInstance()
{
    if (!mySqlPatients) {
        mySqlPatients = new MySQLitePatients;
    }

    return mySqlPatients;
}

//2020.10.12  tao
bool MySQLitePatients::delAllData()
{
    qDebug() << "start clear history record ...";

    QSqlQuery query(m_dbconn);
    QString DropTable ="DROP TABLE HistoryPatients";
    if(query.exec(DropTable)){
        qDebug() << "drop table HistoryPatients!";
    }
    else
        qDebug() << "drop table HistoryPatients failed!";

    initDatabase();

    return true;
}

// 将数据集的行（含所有字段且顺序为原始顺序）数据拷贝到结构体
void MySQLitePatients::dataRowToStruct(QSqlQuery &_sql_query, CPatient &_patient)       /** 表字段查询：PRAGMA table_info([HistoryPatients]) */
{
    _patient.id                 = _sql_query.value(0).toInt();

    _patient.patientid          = _sql_query.value(1).toString();
    _patient.patientname        = _sql_query.value(2).toString();
    _patient.setAgeRange((enAgeRange)_sql_query.value(3).toInt());
    _patient.patientsex         = _sql_query.value(4).toString();
    _patient.setBirthDateStr(_sql_query.value(5).toString());
    _patient.patientlefteyesph  = _sql_query.value(6).toString();
    _patient.patientlefteyecyl  = _sql_query.value(7).toString();
    _patient.patientlefteyeax   = _sql_query.value(8).toString();
    _patient.patientleftse      = _sql_query.value(9).toString();                // TODO: 数据库保存球镜度柱镜度未精确处理的最原始值，查询时根据SE公式和精度即时计算
    _patient.patientleftpd      = _sql_query.value(10).toString();
    _patient.patientleftptosis  = _sql_query.value(11).toBool();
    _patient.patientlefths      = _sql_query.value(12).toString();
    _patient.patientleftvs      = _sql_query.value(13).toString();
    _patient.patientrighteyesph = _sql_query.value(14).toString();
    _patient.patientrighteyecyl = _sql_query.value(15).toString();
    _patient.patientrighteyeax  = _sql_query.value(16).toString();
    _patient.patientrightse     = _sql_query.value(17).toString();
    _patient.patientrightpd     = _sql_query.value(18).toString();
    _patient.patientrightptosis = _sql_query.value(19).toBool();
    _patient.patientrighths     = _sql_query.value(20).toString();
    _patient.patientrightvs     = _sql_query.value(21).toString();
    _patient.patientpd          = _sql_query.value(22).toString();
    _patient.patientstuclass    = _sql_query.value(23).toString();
    _patient.patienttesttime    = _sql_query.value(24).toString();
    _patient.patientPhone       = _sql_query.value(25).toString();
    _patient.patientAddress     = _sql_query.value(26).toString();
    _patient.patientWechat      = _sql_query.value(27).toString();
    _patient.barcodeData        = _sql_query.value(28).toString();
    _patient.batchNo            = _sql_query.value(29).toString();
    _patient.comment1           = _sql_query.value(30).toString();
    _patient.Comment2           = _sql_query.value(31).toString();
    _patient.isTest             = _sql_query.value(32).toBool();
    _patient.isBatch            = _sql_query.value(33).toBool();
    _patient.isNeedUpload       = _sql_query.value(34).toBool();
    _patient.isUploaded         = _sql_query.value(35).toBool();
    _patient.isNeedImage        = _sql_query.value(36).toBool();
    _patient.isUploadedImage    = _sql_query.value(37).toBool();
    _patient.creattime          = _sql_query.value(38).toString();

    _patient.IS_MULTI           = _sql_query.value(39).toBool();

    _patient.RESULT_1_R_SPH     = _sql_query.value(40).toDouble();
    _patient.RESULT_1_R_CYL     = _sql_query.value(41).toDouble();
    _patient.RESULT_1_R_AX      = _sql_query.value(42).toDouble();
    _patient.RESULT_1_L_SPH     = _sql_query.value(43).toDouble();
    _patient.RESULT_1_L_CYL     = _sql_query.value(44).toDouble();
    _patient.RESULT_1_L_AX      = _sql_query.value(45).toDouble();

    _patient.RESULT_2_R_SPH     = _sql_query.value(46).toDouble();
    _patient.RESULT_2_R_CYL     = _sql_query.value(47).toDouble();
    _patient.RESULT_2_R_AX      = _sql_query.value(48).toDouble();
    _patient.RESULT_2_L_SPH     = _sql_query.value(49).toDouble();
    _patient.RESULT_2_L_CYL     = _sql_query.value(50).toDouble();
    _patient.RESULT_2_L_AX      = _sql_query.value(51).toDouble();

    _patient.RESULT_3_R_SPH     = _sql_query.value(52).toDouble();
    _patient.RESULT_3_R_CYL     = _sql_query.value(53).toDouble();
    _patient.RESULT_3_R_AX      = _sql_query.value(54).toDouble();
    _patient.RESULT_3_L_SPH     = _sql_query.value(55).toDouble();
    _patient.RESULT_3_L_CYL     = _sql_query.value(56).toDouble();
    _patient.RESULT_3_L_AX      = _sql_query.value(57).toDouble();
}

QString MySQLitePatients::getDatabaseDir()
{
    MySQLitePatients *inst = MySQLitePatients::getInstance();
    return inst->DatabaseDir;
}

std::vector<CPatient> MySQLitePatients::GetTableInfo()
{
    std::vector<CPatient> vecR;

//    QString sql = "select * from HistoryPatients order by patientid";
    QString sql = "select * from HistoryPatients order by id";

    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(sql);
    if(sql_query.exec())
    {
        while(sql_query.next())
        {
            CPatient rec;
            dataRowToStruct(sql_query, rec);
            vecR.push_back(rec);
        }
    } else {
        qDebug()<<"get table info fail";
    }
    return vecR;
}

void MySQLitePatients::TableDelete(QVector<int> _ids)
{
    QSqlQuery sql_delete(m_dbconn);

    bool is_has_null = false;

    //
    sql_delete.prepare("delete from HistoryPatients where id=?");
    for (int i = 0; i < _ids.size();i ++)
    {
        int id = _ids.at(i);
        if (0 == id) {
            is_has_null = true;
        }
        sql_delete.addBindValue(id);
        if (!sql_delete.exec()) {
            //qDebug()<<"delete record ["<<vecid.at(i)<<"] in HistoryPatients table failed!";
        } else {
            qDebug()<<"delete success";
        }

    }

    //
    if (is_has_null) {
        sql_delete.prepare("delete from HistoryPatients where id is null");
        if (!sql_delete.exec()) {
            qWarning() << "delete null id in HistoryPatients table failed!";
        } else {
            //qDebug()<<"delete success";
        }
    }
}

void MySQLitePatients::TableDeleteByNum(QVector<QString> _nums)
{
    QSqlQuery sql_delete(m_dbconn);

    bool is_has_null = false;

    //
    sql_delete.prepare("delete from HistoryPatients where patientid=?");
    for (int i = 0; i < _nums.size(); i++) {
        QString num = _nums.at(i);
        if (num.length() == 0) {
            is_has_null = true;
        }
        sql_delete.addBindValue(num);
        if (!sql_delete.exec()) {
            qCritical() << "delete record [" << num << "] in HistoryPatients table failed!";
        } else {
            //qDebug()<<"delete success";
        }
    }

    //
    if (is_has_null) {
        sql_delete.prepare("delete from HistoryPatients where patientid is null");
        if (!sql_delete.exec()) {
            qWarning() << "delete null patientid in HistoryPatients table failed!";
        } else {
            //qDebug()<<"delete success";
        }
    }
}

std::vector<CPatient> MySQLitePatients::findTableInfo(int _batch_type, QString _search_str, enSortType _sort_type)
{
    // 搜索未上传的

    //
    QMutexLocker locker(&mutex);

    QString sql_search = "";
    QString keyword = "";
    if ((KEYWORD_NON_UPLOADED_CN == _search_str) || (KEYWORD_NON_UPLOADED_EN == _search_str) || (KEYWORD_NON_UPLOADED_DE == _search_str)) {
        sql_search = "   and not isuploaded ";
    } else if (_search_str.length() > 0) {
        sql_search = "   and (patientid like ? or patientname like ? or patientstuclass like ?) ";
        keyword = _search_str;
    }

    QString sql_batch = "";
    if (0 == _batch_type) {
        sql_batch = "   and not isbatch ";
    } else if (1 == _batch_type) {
        sql_batch = "   and isbatch ";
    }

    QString sql_order = "";
    switch (_sort_type) {
    case enSortType::ByTimeAsc:
        sql_order = " order by creattime asc";
        break;
    case enSortType::ByTimeDsc:
        sql_order = " order by creattime desc";
        break;
    case enSortType::ByNumberAsc:
        sql_order = " order by patientid asc";
        break;
    case enSortType::ByNumberDsc:
        sql_order = " order by patientid desc";
        break;
    default:
        //order_str = " order by id asc";
        break;
    }

    std::vector<CPatient> vecR;
    QString sql = QString("select * from HistoryPatients ")
            + " where 1 "
            + sql_search
            + sql_batch
            + sql_order;

    qDebug() << sql;

    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(sql);

    if (keyword.length() > 0) {
        sql_query.addBindValue("%" + keyword + "%");
        sql_query.addBindValue("%" + keyword + "%");
        sql_query.addBindValue("%" + keyword + "%");
    }

    if(sql_query.exec())
    {
        while(sql_query.next())
        {
            CPatient rec;
            dataRowToStruct(sql_query, rec);
            vecR.push_back(rec);
        }
    } else {
        qDebug()<<"find id from table info fail";
    }
    return vecR;
}

std::vector<CPatient> MySQLitePatients::findRecordById(int _id)
{
    static QVector<int> id_list;
    id_list.clear();
    id_list.push_back(_id);
    return findRecordByIdList(id_list);
}

std::vector<CPatient> MySQLitePatients::findRecordByIdList(QVector<int> _id_list)
{
    QMutexLocker locker(&mutex);
    std::vector<CPatient> vecR;
    QString sql = "select * from HistoryPatients where id=?";
    QSqlQuery sql_query(m_dbconn);
    for (int i = 0; i < _id_list.size(); i++) {
        sql_query.clear();
        sql_query.prepare(sql);
        sql_query.addBindValue(_id_list.at(i));

        if(sql_query.exec())
        {
            while(sql_query.next())
            {
                CPatient rec;
                dataRowToStruct(sql_query, rec);
                vecR.push_back(rec);
            }
        } else {
            qDebug()<<"find id from table info fail";
        }

    }
    return vecR;
}

bool MySQLitePatients::getPatientById(CPatient &_pat)
{
    if (_pat.id <= 0) {
        return false;
    }
    std::vector<CPatient> pats = findRecordById(_pat.id);
    if (pats.size() == 1) {
        _pat.cloneFrom(pats.at(0));
        return true;
    } else {
        return false;
    }
}

std::vector<CPatient> MySQLitePatients::findRecordByPatientid(QString _patientid, int _istest, enSortType _sort_type, int _limit)
{
    static QVector<QString> patientid_list;
    patientid_list.clear();
    patientid_list.push_back(_patientid);
    return findRecordByPatientidList(patientid_list, _istest, _sort_type, _limit);
}

std::vector<CPatient> MySQLitePatients::findRecordByPatientidList(QVector<QString> _patientid_list, int _istest, enSortType _sort_type, int _limit)
{
    QMutexLocker locker(&mutex);

    QString order_str = "";
    switch (_sort_type) {
    case enSortType::ByTimeAsc:
        order_str = " order by patienttesttime asc";
        break;
    case enSortType::ByTimeDsc:
        order_str = " order by patienttesttime desc";
        break;
    case enSortType::ByNumberAsc:
        order_str = " order by patientid asc";
        break;
    case enSortType::ByNumberDsc:
        order_str = " order by patientid desc";
        break;
    default:
        //order_str = " order by id asc";
        break;
    }

    QString limit_str;
    if (_limit > 0) {
        limit_str = QString(" limit %1").arg(_limit);
    }

    std::vector<CPatient> vecR;
    QString sql = "select * from HistoryPatients where patientid=?";

    QString cond_str = (-1 == _istest ? "" : (0 == _istest ? " and istest=0" : " and istest=1"));
    sql += cond_str;
    sql += order_str;
    sql += limit_str;

    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(sql);
    for (int i = 0; i < _patientid_list.size(); i++) {            // TODO: 把循环多次查询改为在 sql 里添加多个编号？
        sql_query.clear();
        sql_query.prepare(sql);
        sql_query.addBindValue(_patientid_list.at(i));

        if(sql_query.exec())
        {
            while(sql_query.next())
            {
                CPatient rec;
                dataRowToStruct(sql_query, rec);
                vecR.push_back(rec);
            }
        } else {
            qDebug()<<"find patientid from table info fail";
        }

    }
    return vecR;
}

std::vector<CPatient> MySQLitePatients::findTableInfobyBatchNo(QString BatchNo)
{
    qDebug()<<" MySQLitePatients::findTableInfobyBatchNo--"<<BatchNo;
    QMutexLocker locker(&mutex);
    std::vector<CPatient> vecR;
    QString sql = "select * from HistoryPatients where batchNo=?";
    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(sql);
    sql_query.addBindValue(BatchNo);
    if(sql_query.exec())
    {
        while(sql_query.next())
        {
            CPatient rec;
            dataRowToStruct(sql_query, rec);
            vecR.push_back(rec);
            qDebug()<<"find id:"<<rec.patientid<<"where batchNo="<<BatchNo;
        }
    } else {
        qDebug()<<"find id from table info fail";
    }
    qDebug()<<"vecR.size="<<vecR.size();
    return vecR;
}

bool MySQLitePatients::insertHistory(CPatient &_pat, bool _get_new_id)
{
    _pat.creattime = QDateTime::currentDateTime().toString(CPatient::dateTimeFormat());

    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(QString("insert into HistoryPatients values(NULL ") +
                      ",?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?" +
                      ",?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?" +
                      ")"
                      );    // TODO: 列出所有设置了新值的字段名，避免数据表字段变动后 insert into 语句执行失败？

    sql_query.addBindValue(_pat.patientid);
    sql_query.addBindValue(_pat.patientname);
    sql_query.addBindValue(QString::number((int)_pat.getAgeRange()));
    sql_query.addBindValue(_pat.patientsex);
    sql_query.addBindValue(_pat.getBirthDateStr());
    sql_query.addBindValue(_pat.patientlefteyesph);
    sql_query.addBindValue(_pat.patientlefteyecyl);
    sql_query.addBindValue(_pat.patientlefteyeax);
    sql_query.addBindValue(_pat.patientleftse);
    sql_query.addBindValue(_pat.patientleftpd);
    sql_query.addBindValue(_pat.patientleftptosis);
    sql_query.addBindValue(_pat.patientlefths);
    sql_query.addBindValue(_pat.patientleftvs);
    sql_query.addBindValue(_pat.patientrighteyesph);
    sql_query.addBindValue(_pat.patientrighteyecyl);
    sql_query.addBindValue(_pat.patientrighteyeax);
    sql_query.addBindValue(_pat.patientrightse);
    sql_query.addBindValue(_pat.patientrightpd);
    sql_query.addBindValue(_pat.patientrightptosis);
    sql_query.addBindValue(_pat.patientrighths);
    sql_query.addBindValue(_pat.patientrightvs);
    sql_query.addBindValue(_pat.patientpd);
    sql_query.addBindValue(_pat.patientstuclass);
    sql_query.addBindValue(_pat.patienttesttime);
    sql_query.addBindValue(_pat.patientPhone);
    sql_query.addBindValue(_pat.patientAddress);
    sql_query.addBindValue(_pat.patientWechat);
    sql_query.addBindValue(_pat.barcodeData);
    sql_query.addBindValue(_pat.batchNo);
    sql_query.addBindValue(_pat.comment1);
    sql_query.addBindValue(_pat.Comment2);
    sql_query.addBindValue(_pat.isTest);
    sql_query.addBindValue(_pat.isBatch);
    sql_query.addBindValue(_pat.isNeedUpload);
    sql_query.addBindValue(_pat.isUploaded);
    sql_query.addBindValue(_pat.isNeedImage);
    sql_query.addBindValue(_pat.isUploadedImage);
    sql_query.addBindValue(_pat.creattime);

    sql_query.addBindValue(_pat.IS_MULTI        );
    sql_query.addBindValue(_pat.RESULT_1_R_SPH  );
    sql_query.addBindValue(_pat.RESULT_1_R_CYL  );
    sql_query.addBindValue(_pat.RESULT_1_R_AX   );
    sql_query.addBindValue(_pat.RESULT_1_L_SPH  );
    sql_query.addBindValue(_pat.RESULT_1_L_CYL  );
    sql_query.addBindValue(_pat.RESULT_1_L_AX   );
    sql_query.addBindValue(_pat.RESULT_2_R_SPH  );
    sql_query.addBindValue(_pat.RESULT_2_R_CYL  );
    sql_query.addBindValue(_pat.RESULT_2_R_AX   );
    sql_query.addBindValue(_pat.RESULT_2_L_SPH  );
    sql_query.addBindValue(_pat.RESULT_2_L_CYL  );
    sql_query.addBindValue(_pat.RESULT_2_L_AX   );
    sql_query.addBindValue(_pat.RESULT_3_R_SPH  );
    sql_query.addBindValue(_pat.RESULT_3_R_CYL  );
    sql_query.addBindValue(_pat.RESULT_3_R_AX   );
    sql_query.addBindValue(_pat.RESULT_3_L_SPH  );
    sql_query.addBindValue(_pat.RESULT_3_L_CYL  );
    sql_query.addBindValue(_pat.RESULT_3_L_AX   );

    bool is_succ = sql_query.exec();
    if (!is_succ) {
        qDebug() << "insert record '" << _pat.patientid << "' into HistoryPatients table failed!";
        qCritical() << "\nSQL Error:\n" << sql_query.lastError().text();
        return false;
    }

    // 获取新记录的自增id
    if (_get_new_id) {
        //sql_query.prepare("select last_insert_rowid()");
        ////sql_query.prepare("select seq from sqlite_sequence where name = 'HistoryPatients'");
        //if (sql_query.exec()) {
        //    if (sql_query.first()) {
        //        _pat.id = sql_query.value(0).toInt();
        //        if (!(_pat.id > 0)) {
        //            logWarning(QString("%1: logic err: query new rowid not great than 0?").arg(__PRETTY_FUNCTION__), CGlobal::LOG_DATABASE);
        //            return false;
        //        }
        //        if (sql_query.next()) {
        //            logWarning(QString("%1: logic err: query new rowid, row count great than 1?").arg(__PRETTY_FUNCTION__), CGlobal::LOG_DATABASE);
        //            return false;
        //        }
        //    } else {
        //        logWarning(QString("%1: sqlquery->first() failed!!").arg(__PRETTY_FUNCTION__), CGlobal::LOG_DATABASE);
        //        return false;
        //    }
        //} else {
        //    logWarning(QString("%1: query new rowid failed?").arg(__PRETTY_FUNCTION__), CGlobal::LOG_DATABASE);
        //    return false;
        //}

        QVariant last_id_var = sql_query.lastInsertId();
        if (last_id_var.isValid()) {
            int id = last_id_var.toInt();
            _pat.id = id;
            qDebug() << "lastInsertId() = " << id;
        } else {
            logWarning(QString("%1: Failed to get lastInsertId()!").arg(__PRETTY_FUNCTION__), CGlobal::LOG_DATABASE);
            is_succ = false;
        }
    }

    //
    return is_succ;
}

void MySQLitePatients::TableBatchAdd(std::vector<CPatient> _pats, int *_count_repeated)
{
    bool proState = _pats.size() > 1 ? true : false;

    qDebug()<<"TableBatchAdd...";
    QString text = tr("正在存入数据");    // "Importing data"
    double totalSize = _pats.size();
    int cpNum = 1;

    if (proState) {
        emit progressSig(text, 0);
    }

    if (_count_repeated) {
        *_count_repeated = 0;
    }

    for (unsigned int i = 0; i < totalSize; i++)
    {
        QString text = tr("正在存入数据..."); // "Importing data..."

        if (_count_repeated) {
            std::vector<CPatient> fVector = findRecordByPatientid(_pats[i].patientid);
            if (fVector.size() > 0)
            {
                qDebug()<<"-----编号已存在："<<text;

                text = tr("编号已存在：");    // "Duplicate number："
                text.append(_pats[i].patientid);

                (*_count_repeated) ++;

                //
                continue;
            }
        }

        //
        bool is_succ = insertHistory(_pats[i], false);
        if (!is_succ) {
            //qDebug()<<"insert record ["<<_pats[i].patientid<<"] in HistoryPatients table failed!";
        }

        //
        if (proState)
        {
            int pNum = (cpNum++) / totalSize * 100;
            emit progressSig(text, pNum);

            QTime _time = QTime::currentTime().addMSecs(1);
            while(QTime::currentTime() < _time)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
    }

    //
    emit progressSig(text, 100);
}

std::vector<CPatient> MySQLitePatients::getInfoForBatch(bool _is_batch, enSortType _sort_type)
{
    QMutexLocker locker(&mutex);
    QString order_str = "";
    switch (_sort_type) {
    case enSortType::ByTimeAsc:
        order_str = " order by creattime asc";
        break;
    case enSortType::ByTimeDsc:
        order_str = " order by creattime desc";
        break;
    case enSortType::ByNumberAsc:
        order_str = " order by patientid asc";
        break;
    case enSortType::ByNumberDsc:
        order_str = " order by patientid desc";
        break;
    default:
        //order_str = " order by id asc";
        break;
    }

    QString sql = "select * from HistoryPatients where isbatch=? " + order_str;

    std::vector<CPatient> vecR;
    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(sql);

    int val = _is_batch ? 1 : 0;
    sql_query.addBindValue(val);

    if(sql_query.exec())
    {
        while(sql_query.next())
        {
            CPatient rec;
            dataRowToStruct(sql_query, rec);
            vecR.push_back(rec);
        }
    } else {
        qDebug()<<"find id from table batch info fail";
    }
    return vecR;
}

std::vector<CPatient> MySQLitePatients::getInfoForClinic(enSortType _sort_type, QString _search_str)
{
    QMutexLocker locker(&mutex);

    QString sql_search = "";
    QString keyword = "";
    if ((KEYWORD_NON_UPLOADED_CN == _search_str) || (KEYWORD_NON_UPLOADED_EN == _search_str) || (KEYWORD_NON_UPLOADED_DE == _search_str)) {
        sql_search = "   and not isuploaded ";
    } else if (_search_str.length() > 0) {
        sql_search = "   and (patientid like ? or patientname like ? or patientstuclass like ?) ";
        keyword = _search_str;
    }

    QString sql_order = "";
    switch (_sort_type) {
    case enSortType::ByTimeAsc:
        sql_order = " order by creattime asc";
        break;
    case enSortType::ByTimeDsc:
        sql_order = " order by creattime desc";
        break;
    case enSortType::ByNumberAsc:
        sql_order = " order by patientid asc";
        break;
    case enSortType::ByNumberDsc:
        sql_order = " order by patientid desc";
        break;
    default:
        //sql_order = " order by id asc";
        break;
    }

    QString sql = QString("select patientid, patientname, patientsex, patientdate from HistoryPatients ")
            + " where isbatch=? "
            + sql_search
            + " group by patientid, patientname, patientsex, patientdate "      // TODO: 还有 patientagerange ？
            + sql_order;

    qDebug() << sql;

    std::vector<CPatient> vecR;
    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(sql);

    bool is_batch = false;
    int val = is_batch ? 1 : 0;

    sql_query.addBindValue(val);

    if (keyword.length() > 0) {
        sql_query.addBindValue("%" + keyword + "%");
        sql_query.addBindValue("%" + keyword + "%");
        sql_query.addBindValue("%" + keyword + "%");
    }

    if(sql_query.exec())
    {
        while(sql_query.next())
        {
            CPatient pat;

            pat.patientid   = sql_query.value(0).toString();
            pat.patientname = sql_query.value(1).toString();
            pat.patientsex  = sql_query.value(2).toString();
            pat.setBirthDateStr(sql_query.value(3).toString());

            vecR.push_back(pat);
        }
    } else {
        qDebug()<<"find id from table batch info fail";
    }
    return vecR;
}

bool MySQLitePatients::getInfoForClinicByPatientId(const QString &_patient_id, CPatient &_pat)
{
    QMutexLocker locker(&mutex);

    QString sql = QString("select patientid, patientname, patientsex, patientdate from HistoryPatients ")
            + " where isbatch=0 "
            + "   and patientid=? "
            + " group by patientid, patientname, patientsex, patientdate ";      // TODO: 还有 patientagerange ？

    std::vector<CPatient> vecR;
    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(sql);

    sql_query.addBindValue(_patient_id);

    if (sql_query.exec()) {
        if (sql_query.next()) {
            _pat.patientid   = sql_query.value(0).toString();
            _pat.patientname = sql_query.value(1).toString();
            _pat.patientsex  = sql_query.value(2).toString();
            _pat.setBirthDateStr(sql_query.value(3).toString());
        } else {
            return false;
        }

        // 如果记录数多余 1，则认为失败
        if (sql_query.next()) {
            logCritical(__PRETTY_FUNCTION__ + QString(": rowcount more than 1, logic error!"), CGlobal::LOG_DATABASE);
            return false;
        }
    } else {
        qDebug()<<"find id from table batch info fail";
    }

    return true;
}

bool MySQLitePatients::getIdsByNums(const QVector<QString> &_num_list, QVector<int> &_id_list, int _istest)
{
    QMutexLocker locker(&mutex);

    _id_list.clear();

    QSqlQuery sql_query(m_dbconn);
    QString sql = "select id from HistoryPatients where patientid in ( %1 ) ";

    QString cond_str = (-1 == _istest ? "" : (0 == _istest ? " and istest=0" : " and istest=1"));
    sql += cond_str;

    QString nums_str;
    for (int i = 0; i < _num_list.size(); i++) {
        if (nums_str.length()) {
            nums_str += ", ";
        }
        nums_str += QString("'%1'").arg(_num_list.at(i));
    }

    sql = sql.arg(nums_str);

    sql_query.prepare(sql);

    if (sql_query.exec()) {
        int id;
        while (sql_query.next()) {
            id = sql_query.value(0).toInt();
            _id_list.push_back(id);
        }
        return true;
    } else {
        logCritical(QString("sql query failed: \"%1\", \nerror: %2").arg(sql).arg(sql_query.lastError().text()), CGlobal::LOG_DATABASE);
        return false;
    }
}

bool MySQLitePatients::getIsBatch(QString vecId)
{
    QString sql = "select * from HistoryPatients where patientid=?";
    QSqlQuery sql_query(m_dbconn);
    sql_query.prepare(sql);
    sql_query.addBindValue(vecId);
    if(sql_query.exec())
    {
        while(sql_query.next())
        {
            if(sql_query.value(33).toBool())
                return true;
        }
    }
    else
        qDebug()<<"find id from table info fail2";
    return false;
}

void MySQLitePatients::TableModify(std::vector<CPatient> vecModify)
{
    for (unsigned int i=0; i<vecModify.size(); i++)
    {
        recordeModify(vecModify[i]);
    }//end for
}

void MySQLitePatients::recordeModify(const CPatient &_pat)
{
    QSqlQuery sql_modify(m_dbconn);
    QString sql_str = QString("update HistoryPatients set patientname=? ") +
            " , patientagerange=? , patientsex=? , patientdate=? " +
            " , patientlefteyesph=? , patientlefteyecyl=? " +
            " , patientlefteyeax=? , patientleftse=? , patientleftpd=? , patientleftptosis=? " +
            " , patientlefthorizontalstrabismushz=? , patientleftverticalstrabismus=? " +
            " , patientrighteyesph=? , patientrighteyecyl=? " +
            " , patientrighteyeax=? , patientrightse=? , patientrightpd=? , patientrightptosis=? " +
            " , patientrighthorizontalstrabismus=? , patientrightverticalstrabismus=? " +
            " , patientpd=? , patientstuclass=? , patienttesttime=? " +
            " , patientphone=? , patientaddress=? , patientwechat=? , barcodedata=? , batchNo=? , comment1=? , comment2=? " +
            " , istest=? , isbatch=? , isneedupload=? , isuploaded=? , isneedimage=? , isuploadedimage=? , patientid=? , creattime=? " +
            " , IS_MULTI        =? " +
            " , RESULT_1_R_SPH  =? " +
            " , RESULT_1_R_CYL  =? " +
            " , RESULT_1_R_AX   =? " +
            " , RESULT_1_L_SPH  =? " +
            " , RESULT_1_L_CYL  =? " +
            " , RESULT_1_L_AX   =? " +
            " , RESULT_2_R_SPH  =? " +
            " , RESULT_2_R_CYL  =? " +
            " , RESULT_2_R_AX   =? " +
            " , RESULT_2_L_SPH  =? " +
            " , RESULT_2_L_CYL  =? " +
            " , RESULT_2_L_AX   =? " +
            " , RESULT_3_R_SPH  =? " +
            " , RESULT_3_R_CYL  =? " +
            " , RESULT_3_R_AX   =? " +
            " , RESULT_3_L_SPH  =? " +
            " , RESULT_3_L_CYL  =? " +
            " , RESULT_3_L_AX   =? " +
            "   where id=?";
    //qDebug() << sql_str;

    sql_modify.prepare(sql_str);

    sql_modify.addBindValue(_pat.patientname);
    sql_modify.addBindValue(QString::number((int)_pat.getAgeRange()));
    sql_modify.addBindValue(_pat.patientsex);
    sql_modify.addBindValue(_pat.getBirthDateStr());

    sql_modify.addBindValue(_pat.patientlefteyesph);
    sql_modify.addBindValue(_pat.patientlefteyecyl);
    sql_modify.addBindValue(_pat.patientlefteyeax);
    sql_modify.addBindValue(_pat.patientleftse);
    sql_modify.addBindValue(_pat.patientleftpd);
    sql_modify.addBindValue(_pat.patientleftptosis);
    sql_modify.addBindValue(_pat.patientlefths);
    sql_modify.addBindValue(_pat.patientleftvs);

    sql_modify.addBindValue(_pat.patientrighteyesph);
    sql_modify.addBindValue(_pat.patientrighteyecyl);
    sql_modify.addBindValue(_pat.patientrighteyeax);
    sql_modify.addBindValue(_pat.patientrightse);
    sql_modify.addBindValue(_pat.patientrightpd);
    sql_modify.addBindValue(_pat.patientrightptosis);
    sql_modify.addBindValue(_pat.patientrighths);
    sql_modify.addBindValue(_pat.patientrightvs);

    sql_modify.addBindValue(_pat.patientpd);
    sql_modify.addBindValue(_pat.patientstuclass);
    sql_modify.addBindValue(_pat.patienttesttime);
    sql_modify.addBindValue(_pat.patientPhone);
    sql_modify.addBindValue(_pat.patientAddress);
    sql_modify.addBindValue(_pat.patientWechat);
    sql_modify.addBindValue(_pat.barcodeData);
    sql_modify.addBindValue(_pat.batchNo);
    sql_modify.addBindValue(_pat.comment1);
    sql_modify.addBindValue(_pat.Comment2);

    sql_modify.addBindValue(_pat.isTest);
    sql_modify.addBindValue(_pat.isBatch);
    sql_modify.addBindValue(_pat.isNeedUpload);
    sql_modify.addBindValue(_pat.isUploaded);
    sql_modify.addBindValue(_pat.isNeedImage);
    sql_modify.addBindValue(_pat.isUploadedImage);
    sql_modify.addBindValue(_pat.patientid);
    sql_modify.addBindValue(_pat.creattime);

    sql_modify.addBindValue(_pat.IS_MULTI       );
    sql_modify.addBindValue(_pat.RESULT_1_R_SPH );
    sql_modify.addBindValue(_pat.RESULT_1_R_CYL );
    sql_modify.addBindValue(_pat.RESULT_1_R_AX  );
    sql_modify.addBindValue(_pat.RESULT_1_L_SPH );
    sql_modify.addBindValue(_pat.RESULT_1_L_CYL );
    sql_modify.addBindValue(_pat.RESULT_1_L_AX  );
    sql_modify.addBindValue(_pat.RESULT_2_R_SPH );
    sql_modify.addBindValue(_pat.RESULT_2_R_CYL );
    sql_modify.addBindValue(_pat.RESULT_2_R_AX  );
    sql_modify.addBindValue(_pat.RESULT_2_L_SPH );
    sql_modify.addBindValue(_pat.RESULT_2_L_CYL );
    sql_modify.addBindValue(_pat.RESULT_2_L_AX  );
    sql_modify.addBindValue(_pat.RESULT_3_R_SPH );
    sql_modify.addBindValue(_pat.RESULT_3_R_CYL );
    sql_modify.addBindValue(_pat.RESULT_3_R_AX  );
    sql_modify.addBindValue(_pat.RESULT_3_L_SPH );
    sql_modify.addBindValue(_pat.RESULT_3_L_CYL );
    sql_modify.addBindValue(_pat.RESULT_3_L_AX  );

    sql_modify.addBindValue(_pat.id);

    if(!sql_modify.exec())
    {
        //qDebug() << sql_modify.lastQuery();
        qDebug() << "modify record,id:[" << _pat.id << "] in HistoryPatients table failed!";
    } else {
        qDebug() << "modify success,id:" << _pat.id << ",isUpload=" << _pat.isUploaded << ",isTest=" << _pat.isTest;
    }//end if
}

//bool MySQLitePatients::getMaxPatientId(QString &_num)
//{
//    QString sql_str = "select max(patientid) from HistoryPatients where patientid like 'P%' and length(patientid) == 6";

//    QSqlQuery sql_query(m_dbconn);
//    sql_query.prepare(sql_str);

//    _num = "";
//    bool succ = false;
//    if(sql_query.exec())
//    {
//        if (sql_query.next())
//        {
//            _num = sql_query.value(0).toString();
//            succ = true;
//        }
//    }
//    return succ;
//}

//bool MySQLitePatients::getPatientIdList(QStringList &_list_num, int _limit, int _offset, QString _min, QString _max)
//{
//    QString sql_str =
//            "select patientid "
//            "  from HistoryPatients "
//            " where patientid like 'P%' "
//            "   and length(patientid) == 6 ";

//    if (_min.length() > 0)
//        sql_str +=
//            "   and patientid >= '?' ";

//    if (_max.length() > 0)
//        sql_str +=
//            "   and patientid <= '?' ";

//    sql_str +=
//            " order by patientid desc "
//            " limit ? offset ? ";

//    QSqlQuery sql_query(m_dbconn);
//    sql_query.prepare(sql_str);

//    if (_min.length() > 0)
//        sql_query.addBindValue(_min);

//    if (_max.length() > 0)
//        sql_query.addBindValue(_max);

//    sql_query.addBindValue(_limit);
//    sql_query.addBindValue(_offset);

//    _list_num.clear();
//    bool succ = false;
//    if(sql_query.exec())
//    {
//        while (sql_query.next())
//        {
//            _list_num.append(sql_query.value(0).toString());
//        }
//        succ = true;
//    }
//    return succ;
//}

bool MySQLitePatients::execSql(QSqlQuery &_query, const QString &_sql_str)
{
    if (_query.prepare(_sql_str)) {
        if (_query.exec()) {
            return true;
        } else {
            logCritical(QString(__PRETTY_FUNCTION__) + ": exec \"" + _sql_str + "\" failed." + _query.lastError().text());
            return false;
        }
    } else {
        logCritical(QString(__PRETTY_FUNCTION__) + ": prepare \"" + _sql_str + "\" failed." + _query.lastError().text());
        return false;
    }
}

CPatient *MySQLitePatients::getPatientFromListById(std::vector<CPatient> &_records, int _id)
{
    int idx = findPatientFromListById(_records, _id);
    if (idx >= 0) {
        return &(_records[idx]);
    } else {
        return Q_NULLPTR;
    }
}

int MySQLitePatients::findPatientFromListById(std::vector<CPatient> &_records, int _id)
{
    for (size_t i = 0; i < _records.size(); i++) {
        if (_id == _records[i].id) {
            return i;
        }
    }
    return -1;
}

void MySQLitePatients::getPatientFromListByIdList(std::vector<CPatient> &_records, QVector<CPatient *> &_pats, const QVector<int> _id_list)
{
    _pats.clear();
    for (size_t idx_recs = 0; idx_recs < _records.size(); idx_recs++) {
        for (int idx_id = 0; idx_id < _id_list.size(); idx_id++) {
            if (_id_list.at(idx_id) == _records.at(idx_recs).id) {
                _pats.push_back(&(_records[idx_recs]));
            }
        }
    }
}

int MySQLitePatients::getMeasureRecordsCount()
{
    QMutexLocker locker(&mutex);

    int ret = -1;

    //
    QString sql = "select count(1) from HistoryPatients ";
    QSqlQuery sql_query(m_dbconn);
    if (!sql_query.prepare(sql)) {
        return ret;
    }

    if (sql_query.exec()) {
        int count = 0;
        while (sql_query.next()) {
            count++;
            if (count <= 1) {
                ret = sql_query.value(0).toInt();
            } else {
                ret = -1;
                logCritical(QString("%1: logic error: return dataset count greater than 1").arg(__PRETTY_FUNCTION__), CGlobal::LOG_DATABASE);
                break;
            }
        }
    } else {
        return ret;
    }

    //
    return ret;
}

bool MySQLitePatients::deleteBeforeDate(const QDate &_date_earliest)
{
    QMutexLocker locker(&mutex);

    //
    QSqlQuery query(MySQLite::m_dbconn);

    QString sql_del = "delete from HistoryPatients where creattime < :CreateTime ";
    query.prepare(sql_del);
    QString date_str = _date_earliest.toString("yyyy-MM-dd");
    query.bindValue(":CreateTime", date_str);
    if (query.exec()) {
        qDebug() << "clean records succeeded!";
        return true;
    } else {
        QString err_msg = query.lastError().text();
        qDebug() << "clean records failed! error: " << err_msg.toLocal8Bit().data();
        return false;
    }
}
