#ifndef BATCHUPLOADLIST_H
#define BATCHUPLOADLIST_H

#include <QDialog>
#include <QTableWidget>
#include <QCheckBox>
#include <QVector>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStringList>

#include "mysqlitepatients.h"
#include "baseform.h"

namespace Ui {
class batchUploadList;
}

class batchUploadList : public CBaseDialog
{
    Q_OBJECT

public:
    explicit batchUploadList(QWidget *parent = 0);
    ~batchUploadList();
    static QVector<QCheckBox*> batchUploadselect;
    static QStringList  batchList;

    void addCheckBox(QStringList);
    static bool ScreenBatchNo(QString); //return true if find the num
    static bool getBatchNo();
    static void clearList();

protected:
    void paintEvent(QPaintEvent *);

private slots:
    void on_buttonBox_accepted();

private:
    Ui::batchUploadList *ui;
    QTableWidget *tableWidget;
    static MySQLitePatients *mysql;
    static std::vector<CPatient> mLk;
};

#endif // BATCHUPLOADLIST_H
