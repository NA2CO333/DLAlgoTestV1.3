#ifndef MYSQLITEPATIENTS_H
#define MYSQLITEPATIENTS_H
#include "mysqlite.h"

#include <vector>

#include <QString>
#include <QVector>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QReadWriteLock>
#include <QCoreApplication>

#include "data.h"

// 数据表的最大记录数
#if (1 == OS_TYPE)
// i.MX6Q 平台
static const int MAX_RECORD_COUNT = 10000;
#else
// 其它平台（PC，rk3568）
static const int MAX_RECORD_COUNT = 30000;
#endif

extern const QString TABLE_NAME_HISTORY;        // 数据库表名：历史记录

//
struct PatientStudent{
    QString patientid;
    QString patientname;
    QString patientagerange;
    QString patientstuclass;
    QString patienttesttime;
};

// 排列方式
enum class enSortType {
    Unknown     = -1,   // 未知
    ByTimeAsc   = 0,    // "Ascending by Time"
    ByTimeDsc   = 1,    // "Descending by Time"
    ByNumberAsc = 2,    // "Ascending by Num"
    ByNumberDsc = 3,    // "Descending by Num"
};

//
class MySQLitePatients : public MySQLite
{
    Q_OBJECT
public:
    static MySQLitePatients* getInstance();
    ~MySQLitePatients();

    static bool initDatabase();     // 初始化数据库
    static bool delAllData();   //2020.10.12  tao
    static bool getIsBatch(QString vecId); //2021.03.08  tao
    static void dataRowToStruct(QSqlQuery &_sql_query, struct CPatient &_patient);

    static QString getDatabaseDir();    // 数据库目录

    std::vector<CPatient> GetTableInfo();

    /**
     * @brief 插入新记录
     * @param _pat          新插入的记录
     * @param _get_new_id   【输出参数】是否获取新生成的自增 id，若是，则获取并设置到 _pat.id
     * @return 是否成功
     */
    bool insertHistory(CPatient &_pat, bool _get_new_id);

    void TableDelete(QVector<int> _ids);
    void TableDeleteByNum(QVector<QString> _nums);

    void TableBatchAdd(std::vector<CPatient> _pats, int *_count_repeated);          /* 注意：仅批量导入时使用，会对筛查号检查并去重 */        // TODO: 结构应还需优化？

    void TableModify(std::vector<CPatient> vecModify);
    void recordeModify(const CPatient &_pat);

    // 参数 _batch_type: 档案类型 -1 不限，0 门诊，1 筛查；
    // 参数 _sort_type: 排序方法 0:按创建时间升序，1:按创建时间降序，2:按编号升序，3:按编号降序，其它:不排序
    std::vector<CPatient> findTableInfo(int _batch_type, QString _search_str = "", enSortType _sort_type = enSortType::Unknown);    // 查询指定类型的记录（增加门诊功能后，此查询仅适用于筛查记录的查询）
    std::vector<CPatient> getInfoForBatch(bool _is_batch, enSortType _sort_type = enSortType::Unknown);

    std::vector<CPatient> getInfoForClinic(enSortType = enSortType::Unknown, QString _search_str = "");                  // 查出所有门诊档案记录（需要 group by）
    // TODO: 不分表，而通过 group by 获取档案，这样做可能更费事，还容易出错！还是要分表才行。

    bool getInfoForClinicByPatientId(const QString &_patient_id, CPatient &_pat);           // 根据 patientid 查询一条门诊档案

    std::vector<CPatient> findRecordById(int _id);
    std::vector<CPatient> findRecordByIdList(QVector<int> _id_list);
    bool getPatientById(CPatient &_pat);         // 根据 id 载入记录

    /**
     * @brief 按编号查询
     * @param _patientid    编号
     * @param _istest       是否已测，-1 表示不限，0 表示未测，1 表示已测
     * @param _sort_type    排序方法，0 按测量时间升序，1 按测量时间降序，2 按编号升序，3 按编号降序，其它 不排序
     * @param _limit        限制查询结果的最大记录数，0 表示不限制
     * @return
     */
    std::vector<CPatient> findRecordByPatientid(QString _patientid, int _istest = -1, enSortType _sort_type = enSortType::Unknown, int _limit = 0);
    // 按编号列表查询，其它参数同上
    std::vector<CPatient> findRecordByPatientidList(QVector<QString> _patientid_list, int _istest = -1, enSortType _sort_type = enSortType::Unknown, int _limit = 0);

    //
    std::vector<CPatient> findTableInfobyBatchNo(QString BatchNo);

    bool getIdsByNums(const QVector<QString> &_num_list, QVector<int> &_id_list, int _istest = -1);         // _istest: 是否已测，-1 表示不限，0 表示未测，1 表示已测

    //bool getMaxPatientId(QString &_num);            // 查询数据库中的最大自建编号
    //bool getPatientIdList(QStringList &_list_num, int _limit, int _offset, QString _min = "", QString _max = "");   // 查询数据库中指定范围的编号


    static bool execSql(QSqlQuery &_query, const QString &_sql_str);

    static CPatient *getPatientFromListById(std::vector<CPatient> &_records, int _id);
    static int findPatientFromListById(std::vector<CPatient> &_records, int _id);
    static void getPatientFromListByIdList(std::vector<CPatient> &_records, QVector<CPatient *> &_pats, const QVector<int> _id_list);

    int getMeasureRecordsCount();            // 获取测量记录的条数

    bool deleteBeforeDate(const QDate &_date_earliest);         // 删除指定创建日期之后创建的记录

signals:
    void progressSig(QString, int);

protected:
    MySQLitePatients();

    static bool addFieldToTable(const QString &_field_name, const QString &_field_type, bool _is_nullable,
                                const QString &_table_name, QSqlQuery &_query);             // 向指定的表中增加指定的字段

    static MySQLitePatients* mySqlPatients;
    inline static const char * const S_CLASS_NAME = staticMetaObject.className();     // 本类的类名

    static QString databaseVersion;
    static QMutex mutex;                           // TODO: 如果内部嵌套调用，可能造成死锁？方案：公开函数（可加统一前缀如“call”方便区分私有函数）全部加此锁，而公开函数里只是加锁和调用具体实现的私有函数，而私有函数禁止调用公开函数

};

#endif


