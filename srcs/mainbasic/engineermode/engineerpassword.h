#ifndef ENGINEERPASSWORD_H
#define ENGINEERPASSWORD_H

#include <QWidget>

#include "myeditline.h"
#include "engineermode/engineermode.h"
#include "baseform.h"

namespace Ui {
class engineerpassword;
}

class engineerpassword : public CBaseWidget
{
    Q_OBJECT

public:
    explicit engineerpassword(QWidget *parent = 0);
    ~engineerpassword();

    static qint64 elapsedChecked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *_evt) override;

    static QElapsedTimer s_elapsedChecked;

private slots:
    void on_pushButton_Ok_clicked();
    void on_pushButton_Alter_clicked();
    void on_pushButton_Cancel_clicked();
private:
    Ui::engineerpassword *ui;
    myEditLine *rootPwd;
    myEditLine *engineerPwd;
    bool showPwdEdit;
};

#endif // ENGINEERPASSWORD_H
