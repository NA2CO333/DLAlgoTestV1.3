#ifndef IMPORT_H
#define IMPORT_H

#include <QThread>

#include "mysqlitepatients.h"
#include "threadmodel.h"
#include "baseform.h"

namespace Ui {
class import;
}

// 导入、导出总对话框
class import : public CBaseDialog
{
    Q_OBJECT

public:
    explicit import(QWidget *parent, QVector<int> _selectedIds, QString _udisk_path, bool _is_batch);
    ~import();

signals:
//    void senderSignalIndex();
    void clicked();
    void sigExport(QVector<int> _selected_ids, QString _udisk_path, bool _is_batch, bool _is_sheet, bool _is_pdf, bool _is_template);
    void sigImportBatch(QStringList);

protected slots:
    void exportScreenData(bool _is_batch);
    void slotProcessEnd();

    void on_back_pushButton_clicked();
    void on_btnSelectExport_clicked();
    void on_btnImportBatch_clicked();
    void on_btnExportHistory_clicked();
    void on_btnExportBatch_clicked();

protected:
    void showEvent(QShowEvent *);
    void paintEvent(QPaintEvent *event);
    void keyPressEvent(QKeyEvent*);

    QVector<int> selectedIds;

    MySQLitePatients *mysql;
    std::vector<CPatient> mLk;
    int size;

    //QThread *thread;
    //ThreadModel *work;

    ThreadModel *model;
    QThread *hard;

    QString udiskPath;

private:
    Ui::import *ui;
};

#endif // IMPORT_H
