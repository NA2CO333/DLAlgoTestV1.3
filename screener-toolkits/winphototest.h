#ifndef WINPHOTOTEST_H
#define WINPHOTOTEST_H

#include <QMainWindow>

namespace Ui {
class WinPhotoTest;
}

class WinPhotoTest : public QMainWindow
{
    Q_OBJECT

public:
    explicit WinPhotoTest(QWidget *parent = 0);
    ~WinPhotoTest();

protected:
    //void keyPressEvent(QKeyEvent *);

private slots:
    void on_btnStart_clicked();

    void on_btnPause_clicked();

    void on_btnGoBack_clicked();

private:
    Ui::WinPhotoTest *ui;

};

#endif // WINPHOTOTEST_H
