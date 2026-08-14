#ifndef THEMEBACKGROUND_H
#define THEMEBACKGROUND_H

#include <QWidget>

#include "baseform.h"
#include "statusbarform.h"

namespace Ui {
class themebackground;
}

//
class themebackground : public CBaseWidget
{
    Q_OBJECT

public:
    explicit themebackground(QWidget *parent = 0);
    ~themebackground();

private slots:
    void on_pushButton_Back_clicked();
    void on_pushButton_Save_clicked();
    void on_pushButton_Home_clicked();
    void on_radioButton_White_clicked();
    void on_radioButton_Black_clicked();

protected:
    void showEvent(QShowEvent *);

    void updateTheme(enThemeType _theme_type);
    void Save_prompt_dialog();

private:
    bool theme_w_flag,theme_b_flag;
    Ui::themebackground *ui;
};

#endif // THEMEBACKGROUND_H
