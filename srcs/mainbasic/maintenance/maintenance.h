#ifndef CMAINTENANCE_H
#define CMAINTENANCE_H

#include <QObject>

// 维护相关功能封装（非正常功能需求）
class CMaintenance : public QObject
{
    Q_OBJECT
public:
    explicit CMaintenance(QObject *parent = nullptr);
    ~CMaintenance();

public Q_SLOTS:
    void slotConfigLoaded();

};

#endif // CMAINTENANCE_H
