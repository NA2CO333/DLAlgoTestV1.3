#ifndef WINLEDPOSIDETECT_H
#define WINLEDPOSIDETECT_H

#include <QMainWindow>

namespace Ui {
class CWinLedPosiDetect;
}

class CWinLedPosiDetect : public QMainWindow
{
    Q_OBJECT

public:
    explicit CWinLedPosiDetect(QWidget *parent = 0);
    ~CWinLedPosiDetect();

private:
    Ui::CWinLedPosiDetect *ui;
};

#endif // WINLEDPOSIDETECT_H
