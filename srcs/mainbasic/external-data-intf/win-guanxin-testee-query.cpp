#include "win-guanxin-testee-query.h"
#include "ui_win-guanxin-testee-query.h"

#include <QDateTime>

#include "win-guanxin-testee-edit.h"
#include "winmanage.h"
#include "utilui.h"

//
WinGuanXinTesteeQuery::WinGuanXinTesteeQuery(QWidget *_parent) :
    CBaseDialog(_parent),
    ui(new Ui::WinGuanXinTesteeQuery)
{
    ui->setupUi(this);

    //
    this->setStyleSheet(R"DELIMITER(
QWidget {
    background-color:rgb(51,56,62);
    color:rgb(204,204,204)
}
QLineEdit {
    background-color: rgb(28,28,30);
}
)DELIMITER"
                        );

    //
    ui->cbbAreaCode->clear();

}

WinGuanXinTesteeQuery::~WinGuanXinTesteeQuery()
{
    delete ui;
}

void WinGuanXinTesteeQuery::showEvent(QShowEvent *)
{
    //
    ui->edtMemo->setEnabled(false);

    //
    if (ui->cbbAreaCode->count() == 0) {
        reloadAreaList(0);
    }

    //
    if (ui->edtBeginYear->text().isEmpty()) {
        QDate date_end = QDate::currentDate();
        QDate date_begin = date_end.addDays(-1);

        ui->edtBeginYear->setText(  date_begin.toString("yyyy"));
        ui->edtBeginMonth->setText( date_begin.toString("MM"));
        ui->edtBeginDay->setText(   date_begin.toString("dd"));

        ui->edtEndYear->setText(    date_end.toString("yyyy"));
        ui->edtEndMonth->setText(   date_end.toString("MM"));
        ui->edtEndDay->setText(     date_end.toString("dd"));

    }
}

void WinGuanXinTesteeQuery::reloadAreaList(int _curr_idx)
{
    ui->cbbAreaCode->clear();
    QVector<stAreaInfo> &area_list = CDataIntfGuanXin::areaList();
    for (int i = 0; i < area_list.size(); i++) {
        const stAreaInfo &area_info = area_list.at(i);
        ui->cbbAreaCode->addItem(area_info.name, area_info.code);
    }
    ui->cbbAreaCode->setCurrentIndex(_curr_idx);
}

bool WinGuanXinTesteeQuery::checkValues(QString &_err_msg)
{
    // 日期的合法性
    QDate begin_date = QDate(ui->edtBeginYear->text().toInt(), ui->edtBeginMonth->text().toInt(), ui->edtBeginDay->text().toInt());
    QDate end_date   = QDate(ui->edtEndYear->text().toInt(), ui->edtEndMonth->text().toInt(), ui->edtEndDay->text().toInt());

    if (!begin_date.isValid()) {
        _err_msg = tr("“开始日期”不合法！");    // "'Start Date' is illegal!"
        return false;
    }

    if (!end_date.isValid()) {
        _err_msg = tr("“结束日期”不合法！");    // "'End Date' is illegal!"
        return false;
    }

    // 开始日期不能大于结束日期
    if (begin_date > end_date) {
        _err_msg = tr("“开始日期”不能大于“结束日期”！");     // "'Start Date' cannot greater then 'Date End'!"
        return false;
    }

    // 开始日期和结束日期的跨度不能过大
    static const int MAX_INTERVAL = 90;     // 最大日期跨度（天数）
    if (begin_date.daysTo(end_date) > MAX_INTERVAL) {
        _err_msg = tr("“开始日期”和“结束日期”的跨度不能过大！");     // "The interval between 'Start Date' and 'End Date' must not be too long!"
        return false;
    }

    //
    return true;
}

void WinGuanXinTesteeQuery::on_btnCancel_clicked()
{
    this->reject();
}

void WinGuanXinTesteeQuery::on_btnOK_clicked()
{
    //
    QString err_msg;
    bool succ = checkValues(err_msg);
    if (!succ) {
        getWinManage()->showMsgWin(err_msg);
        return;
    }

    //
    this->accept();
}

void WinGuanXinTesteeQuery::getUiData(Entity::ETesteeQueryRequest &_entity)
{
    _entity.areaCode    = ui->cbbAreaCode->currentData().toString();

    _entity.beginTime   = QDate(ui->edtBeginYear->text().toInt(), ui->edtBeginMonth->text().toInt(), ui->edtBeginDay->text().toInt())
            .toString(CDataIntfGuanXin::DATE_FORMAT);
    _entity.endTime     = QDate(ui->edtEndYear->text().toInt(), ui->edtEndMonth->text().toInt(), ui->edtEndDay->text().toInt())
            .toString(CDataIntfGuanXin::DATE_FORMAT);

    _entity.memo        = ui->edtMemo->text();

    //_entity.areaCode    = "ea2798b2-b871-44b7-a8b4-662f875d616c";
    //_entity.beginTime   = "2025-05-01";
    //_entity.endTime     = "2025-05-29";
    //_entity.memo        = "SLSC";

}

void WinGuanXinTesteeQuery::on_btnEditArea_clicked()
{
    //
    WinGuanXinTesteeEdit *win_edit = nullptr;
    if (!win_edit) {
        win_edit = new WinGuanXinTesteeEdit(this->parentWidget());
        win_edit->setObjectName("WinGuanXinTesteeEdit");
        win_edit->setParent(this->parentWidget());
        Util::Ui::centerWidget(win_edit);
    }

    //
    win_edit->setData(CDataIntfGuanXin::areaList(), ui->cbbAreaCode->currentIndex());
    int ret = win_edit->exec();
    if (QDialog::Accepted == ret) {
        if (win_edit->isChanged()) {
            int idx = win_edit->currentIndex();
            reloadAreaList(idx);
        }
    }
}

void WinGuanXinTesteeQuery::on_btnAdd_clicked()
{
    //
    WinGuanXinTesteeEdit *win_add = nullptr;
    if (!win_add) {
        win_add = new WinGuanXinTesteeEdit(this->parentWidget());
        win_add->setObjectName("WinGuanXinTesteeAdd");
        win_add->setParent(this->parentWidget());
        Util::Ui::centerWidget(win_add);
    }

    //
    win_add->setData();
    int ret = win_add->exec();
    if (QDialog::Accepted == ret) {
        reloadAreaList(CDataIntfGuanXin::areaList().size() - 1);
    }
}
