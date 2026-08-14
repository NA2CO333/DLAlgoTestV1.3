#ifndef DIALOGLANGUAGE_H
#define DIALOGLANGUAGE_H

#include <QDialog>

namespace Ui {
class DialogLanguage;
}

class DialogLanguage : public QDialog
{
    Q_OBJECT
public:
    explicit DialogLanguage(QWidget *parent = nullptr);
    ~DialogLanguage();

    void setLanguage(QString _language);
    QString getLanguage();

protected:
    void showEvent(QShowEvent *) override;

    QString m_language;

private slots:
    void on_btnCancel_clicked();
    void on_btnOk_clicked();
private:
    Ui::DialogLanguage *ui;
};

#endif // DIALOGLANGUAGE_H
