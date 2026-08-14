#ifndef THREADMODEL_H
#define THREADMODEL_H

#include <QThread>
#include <QObject>
#include <mysqlitepatients.h>
#include <QDir>

// 年级和班级分隔符
#define GRADE_CLASS_SEPARATOR " "

//
class ThreadModel : public QObject
{
    Q_OBJECT
public:
    explicit ThreadModel(QObject *parent = 0);
    ~ThreadModel();
    void copyDir(QDir src,QDir dest);//sun
    void showProgress(int msecs);//wim
    bool readExcel(QString _path, std::vector<CPatient> &_list_pats, QString _process_msg);
    QString getCellName(int row,int col);
    //QString getGender(QString data);

    static QString mergeGradeAndClass(const QString &_grade, const QString &_class);                        // 合并年级和班级
    static void splitGradeAndClass(const QString &_grade_and_class, QString &_grade, QString &_class);      // 分离年级和班级

signals:
    void sigRefresh();                  // TODO: 这些信号和槽的命名、逻辑都比较乱，待整理！
    void sigProcessEnd();
    void sigEnableViewObject(bool enable);
    void sigWarningMsg(QString _msg);
    void progressSig(QString, int);

private:
    MySQLitePatients *mysql;

public slots:
    void slotImportBatch(QStringList);
    void slotExportData(QVector<int> _selected_ids, QString _udisk_path, bool _is_batch, bool _is_sheet, bool _is_pdf, bool _is_template);

};

#endif // THREADMODEL_H

