#ifndef CLANDEVFINDERCLIENTQT_H
#define CLANDEVFINDERCLIENTQT_H

#include <QObject>

#include "landevfinderclient.h"

// CLanDevFinderClient 类的 Qt 调用封装
class CLanDevFinderClientQt : public QObject
{
    Q_OBJECT
public:
    explicit CLanDevFinderClientQt(QObject *parent = nullptr);
    ~CLanDevFinderClientQt();

    void startFind(bool _is_block = true);
    bool getFindResult(QString &_addr, int &_port, unsigned int _index = 0);
    void setMessageKeyword(QString _keyword);
    int getResultCount();

signals:
    void sigFinderFinished();

protected:
    static CLanDevFinderClientQt *thisQtObj;
    CLanDevFinderClient *devFinderClient = Q_NULLPTR;

    static void callbackFinderFinished();
    void emitFinderFinished();

};

#endif // CLANDEVFINDERCLIENTQT_H
