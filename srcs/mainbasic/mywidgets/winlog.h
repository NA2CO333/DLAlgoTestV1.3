#ifndef WINLOG_H
#define WINLOG_H

#include <QWidget>

#include "remoteservicedefs.h"

namespace Net {
namespace Remote {
class CRemoteService;
}
}

namespace Ui {
class WinLog;
}

class WinLog : public QWidget
{
    Q_OBJECT

public:
    explicit WinLog(QWidget *parent = nullptr);
    ~WinLog();

public slots:
    void slot_remoteService_Log(Net::Remote::enLogType _log_type, QString _log_msg);

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private slots:
    void on_btnClose_clicked();

    void on_btnClear_clicked();

private:
    Ui::WinLog *ui;
};

#endif // WINLOG_H
