#include "dialoglanguage.h"
#include "ui_dialoglanguage.h"

#include "global.h"

//
DialogLanguage::DialogLanguage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogLanguage)
{
    ui->setupUi(this);

    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowFlag(Qt::FramelessWindowHint, true);

}

DialogLanguage::~DialogLanguage()
{
    delete ui;
}

void DialogLanguage::setLanguage(QString _language)
{
    m_language = _language;
}

QString DialogLanguage::getLanguage()
{
    return m_language;
}

void DialogLanguage::showEvent(QShowEvent *)
{
    // 根据当前语言设置界面
    if (G_LANGUAGE_ENGLISH == m_language) {
        ui->rbtnEnglish->setChecked(true);
    } else if (G_LANGUAGE_CHINESE == m_language) {
        ui->rbtnChinese->setChecked(true);
    } else if (G_LANGUAGE_GERMAN == m_language) {
        ui->rbtnGerman->setChecked(true);
    } else {
        ui->rbtnEnglish->setChecked(true);
    }
}

void DialogLanguage::on_btnCancel_clicked()
{
    reject();
}

void DialogLanguage::on_btnOk_clicked()
{
    // 获取用户选定的语言
    if (ui->rbtnEnglish->isChecked()) {
        m_language = G_LANGUAGE_ENGLISH;
    } else if (ui->rbtnChinese->isChecked()) {
        m_language = G_LANGUAGE_CHINESE;
    } else if (ui->rbtnGerman->isChecked()) {
        m_language = G_LANGUAGE_GERMAN;
    } else {
        //
    }
    accept();
}
