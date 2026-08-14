#ifndef WINUNITTEST_H
#define WINUNITTEST_H

#include <QWidget>

namespace Ui {
class WinUnitTest;
}

class CWinUnitTest : public QWidget
{
    Q_OBJECT

public:
    explicit CWinUnitTest(QWidget *parent = nullptr);
    ~CWinUnitTest();

private slots:
    void on_btnClose_clicked();
    void on_btnTestBaseboardSerial_clicked();
    void on_btnTestRkWifiBt_clicked();

private:
    Ui::WinUnitTest *ui;
};

#endif // WINUNITTEST_H
