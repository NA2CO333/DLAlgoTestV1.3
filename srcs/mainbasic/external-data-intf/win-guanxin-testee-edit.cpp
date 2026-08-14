#include "win-guanxin-testee-edit.h"
#include "ui_win-guanxin-testee-edit.h"

#include "winmanage.h"

//
WinGuanXinTesteeEdit::WinGuanXinTesteeEdit(QWidget *_parent) :
    CBaseDialog(_parent),
    ui(new Ui::WinGuanXinTesteeEdit)
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

}

WinGuanXinTesteeEdit::~WinGuanXinTesteeEdit()
{
    delete ui;
}

void WinGuanXinTesteeEdit::setData(QVector<stAreaInfo> &_area_list, int _idx)
{
    m_areaList = &_area_list;
    m_idx = _idx;

    m_isChanged = false;
}

void WinGuanXinTesteeEdit::setData()
{
    m_areaList = nullptr;
    m_idx = -1;

    m_isChanged = false;
}

int WinGuanXinTesteeEdit::currentIndex()
{
    return ui->cbbAreaCode->currentIndex();
}

QString WinGuanXinTesteeEdit::currentName()
{
    return ui->edtName->text();
}

QString WinGuanXinTesteeEdit::currentCode()
{
    return ui->edtCode->text();
}

void WinGuanXinTesteeEdit::showEvent(QShowEvent *)
{
    //
    m_workMode = (m_areaList ? enWorkMode::edit : enWorkMode::add);

    //
    if (enWorkMode::edit == m_workMode) {
        //
        reloadAreaList(m_idx);

        //
        if (m_idx >= 0) {
            const stAreaInfo &info = m_areaList->at(m_idx);
            ui->edtCode->setText(info.code);
            ui->edtName->setText(info.name);
        }
    } else if (enWorkMode::add == m_workMode) {
        //
        this->layout()->invalidate();

        ui->wgtItems->setVisible(false);
        ui->wgtOperateBtns->setVisible(false);

        this->layout()->activate();

        //
        ui->edtCode->setText("");
        ui->edtName->setText("");
    } else {
        getWinManage()->showMsgWin("ProgramError: WorkMode not valid!");
    }
}

void WinGuanXinTesteeEdit::reloadAreaList(int _curr_idx)
{
    ui->cbbAreaCode->clear();
    for (int i = 0; i < m_areaList->size(); i++) {
        const stAreaInfo &area_info = m_areaList->at(i);
        ui->cbbAreaCode->addItem(area_info.name, area_info.code);
    }
    ui->cbbAreaCode->setCurrentIndex(_curr_idx);
    showAreaInfoByIndex(_curr_idx);
}

void WinGuanXinTesteeEdit::showAreaInfoByIndex(int _idx)
{
    if (_idx >= 0 && _idx < m_areaList->size()) {
        const stAreaInfo &info = m_areaList->at(_idx);
        ui->edtCode->setText(info.code);
        ui->edtName->setText(info.name);
    } else {
        ui->edtCode->setText("");
        ui->edtName->setText("");
    }
}

bool WinGuanXinTesteeEdit::checkDataValid(QString &_err_msg)
{
    if (enWorkMode::edit == m_workMode) {
        // 区域列表中所有项的名称和代码都不可为空
        for (int i = 0; i < m_areaList->size(); i++) {
            const stAreaInfo &info = m_areaList->at(i);
            if (info.code.isEmpty()) {
                _err_msg = tr("第 %1 项的区域号为空！").arg(i + 1);      // "The AreaCode of the %1th item is empty!"
                return false;
            }
            if (info.name.isEmpty()) {
                _err_msg = tr("第 %1 项的结构名称为空！").arg(i + 1);     // "The OrganizationName of the %1th item is empty!"
                return false;
            }
        }

        // 各项的名称和代码都不可有重复
        for (int i = 0; i < m_areaList->size() - 1; i++) {
            const stAreaInfo &info_1 = m_areaList->at(i);
            for (int j = i + 1; j < m_areaList->size(); j++) {
                const stAreaInfo &info_2 = m_areaList->at(j);

                if (info_1.code == info_2.code) {
                    _err_msg = tr("第 %1 项和第 %2 项的区域号相同！").arg(i + 1).arg(j + 1);      // "The AreaCode of the %1th and %2th items is the same!"
                    return false;
                }
                if (info_1.name == info_2.name) {
                    _err_msg = tr("第 %1 项和第 %2 项的机构名称相同！").arg(i + 1).arg(j + 1);      // "The OrganizationName of the %1th and %2th items is the same!"
                    return false;
                }
            }
        }

        //
        return true;
    } else if (enWorkMode::add == m_workMode) {
        const QString code_curr = ui->edtCode->text();
        const QString name_curr = ui->edtName->text();

        // 当前名称和代码编辑框内容都不可为空
        if (code_curr.isEmpty()) {
            _err_msg = tr("区域号不可为空！");      // "The AreaCode cannot be empty!"
            return false;
        }
        if (name_curr.isEmpty()) {
            _err_msg = tr("结构名称不可为空！");     // "The OrganizationName cannot be empty!"
            return false;
        }

        // 名称、代码和现有项的值不可重复
        QVector<stAreaInfo> &area_list = CDataIntfGuanXin::areaList();
        for (int i = 0; i < area_list.size(); i++) {
            const stAreaInfo &info = area_list.at(i);

            if (code_curr == info.code) {
                _err_msg = tr("区域号和已有的第 %1 项相同！").arg(i + 1);      // "The AreaCode is the same as the existing %1th item!"
                return false;
            }
            if (name_curr == info.name) {
                _err_msg = tr("机构名称和已有的第 %1 项相同！").arg(i + 1);      // "The OrganizationName is the same as the existing %1th item!"
                return false;
            }
        }

        //
        return true;
    } else {
        getWinManage()->showMsgWin("ProgramError: WorkMode not valid!");
        return false;
    }
}

void WinGuanXinTesteeEdit::on_cbbAreaCode_activated(int _index)
{
    showAreaInfoByIndex(_index);
}

void WinGuanXinTesteeEdit::on_edtCode_textEdited(const QString &_arg1)
{
    //
    int idx = ui->cbbAreaCode->currentIndex();
    if (idx >= 0) {
        stAreaInfo &info = (*m_areaList)[idx];

        info.code = _arg1;

        //
        m_isChanged = true;
        reloadAreaList(idx);
    }
}

void WinGuanXinTesteeEdit::on_edtName_textEdited(const QString &_arg1)
{
    //
    int idx = ui->cbbAreaCode->currentIndex();
    if (idx >= 0) {
        stAreaInfo &info = (*m_areaList)[idx];

        info.name = _arg1;

        //
        m_isChanged = true;
        reloadAreaList(idx);
    }
}

void WinGuanXinTesteeEdit::on_btnDel_clicked()
{
    int idx = ui->cbbAreaCode->currentIndex();
    if (idx >= 0) {
        //
        m_areaList->removeAt(idx);

        //
        if (idx > m_areaList->size() - 1) {     // 若当前行为最后一行，则索引号减1
            idx = m_areaList->size() - 1;
        }

        //
        m_isChanged = true;
        reloadAreaList(idx);        // NOTE: 此函数已处理了列表为空的情况
    }
}

void WinGuanXinTesteeEdit::on_btnOK_clicked()
{
    if (enWorkMode::edit == m_workMode) {
        //
        if (m_isChanged) {
            //
            QString err_msg;
            bool is_valid = checkDataValid(err_msg);
            if (!is_valid) {
                getWinManage()->showMsgWin(err_msg);
                return;
            }

            //
            CDataIntfGuanXin::saveConfig();
        }

        //
        this->accept();
    } else if (enWorkMode::add == m_workMode) {
        //
        QString err_msg;
        bool is_valid = checkDataValid(err_msg);
        if (!is_valid) {
            getWinManage()->showMsgWin(err_msg);
            return;
        }

        //
        CDataIntfGuanXin::areaList().append(stAreaInfo(currentCode(), currentName()));
        CDataIntfGuanXin::saveConfig();

        //
        this->accept();
    } else {
        getWinManage()->showMsgWin("ProgramError: WorkMode not valid!");
    }
}

void WinGuanXinTesteeEdit::on_btnCancel_clicked()
{
    // 若取消，且已被编辑过，则从配置文件重新载入区域列表
    if (m_isChanged) {
        CDataIntfGuanXin::loadConfig();
    }

    //
    this->reject();
}
