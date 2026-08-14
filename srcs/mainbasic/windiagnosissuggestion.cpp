#include "windiagnosissuggestion.h"
#include "ui_windiagnosissuggestion.h"

#include "winmanage.h"
#include "global.h"
#include "report.h"
#include "util-common.h"

/// ====================================================================================================
/// class CBusiDataSuggestion
///

void CBusiDataSuggestion::reset()
{
    suggestion.clear();
}

bool CBusiDataSuggestion::isEqualTo(const CBusiDataSuggestion &_busi_data) const
{
    bool is_same = true;

    if (_busi_data.suggestion != suggestion) {
        is_same = false;
    }

    return is_same;
}

void CBusiDataSuggestion::copyFrom(CBusiDataSuggestion &_busi_data)
{
    this->suggestion = _busi_data.suggestion;
}

/// ====================================================================================================
/// class WinDiagnosisSuggestion
///

//
WinDiagnosisSuggestion::WinDiagnosisSuggestion(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::WinDiagnosisSuggestion)
{
    ui->setupUi(this);
    isShowStatusBar = true;

    // 清掉设计期间设置的 StyleSheet
    QList<QWidget *> list_childs = this->findChildren<QWidget *>();
    for (int i = 0; i < list_childs.size(); i++) {
        list_childs[i]->setStyleSheet("");
    }

}

WinDiagnosisSuggestion::~WinDiagnosisSuggestion()
{
    delete ui;
}

void WinDiagnosisSuggestion::showEvent(QShowEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 获得业务数据
    configToBusiData(busiDataOrigin);

    // 将业务数据设置到 UI
    busiDataToUi(busiDataOrigin);

    // 更新语言
    updateLanguage();

    // 更新主题
    updateTheme(getSysThemeType());

}

void WinDiagnosisSuggestion::updateTheme(enThemeType _theme)
{
    static QString form_style_black;

    static bool form_style_black_read = false;
    if (!form_style_black_read) {
        Util::readFileToQStr(":/resource/qss/windiagnosissuggestion.qss", form_style_black);
        form_style_black_read = true;
    }

    //
    if (themeType_Black == _theme) {
        this->setStyleSheet(form_style_black);

    } else if (themeType_White == _theme) {
        // TODO:

    }

}

void WinDiagnosisSuggestion::updateLanguage()
{
    getWinManage()->updateWindowTitle(this, tr("诊治建议"));    // "Diagnosis suggestion setting"

    ui->lblHome->setText(tr("主页")); // "Home"
    ui->lblSave->setText(tr("保存")); // "Save"
    ui->lblBack->setText(tr("返回")); // "Back"

    ui->btnLoad->setText(tr("从U盘导入"));  // "Import from U-Disk"
    ui->lblFileNameTip->setText(tr("（文件名：“suggestion.txt”，UTF-8编码）"));  // "(File Name: \"suggestion.txt\", UTF-8 Encoding)"

}

void WinDiagnosisSuggestion::on_btnHome_clicked()
{
    CBusiDataSuggestion busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    getWinManage()->showWindowByType(WIN_HOME);
}

void WinDiagnosisSuggestion::on_btnBack_clicked()
{
    CBusiDataSuggestion busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    //getWinManage()->showWindowByType(WIN_TOOL);
    getWinManage()->backToLastWidget();
}

void WinDiagnosisSuggestion::on_btnSave_clicked()
{
    CBusiDataSuggestion busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    } else {
        getWinManage()->showSuspensionPrompt(tr("数据未被修改")); // "Data not modified"
    }
}

void WinDiagnosisSuggestion::configToBusiData(CBusiDataSuggestion &_busi_data)
{
    _busi_data.suggestion = CReport::getSuggestion(true);
}

void WinDiagnosisSuggestion::saveBusiData(CBusiDataSuggestion &_busi_data)
{
    // 保存到永久存储
    QString err_msg;
    bool succ_save_to_file = CReport::saveSuggestion(_busi_data.suggestion, err_msg);
    if (!succ_save_to_file) {
        getWinManage()->showMsgWin(err_msg, -1);
    }

    // 保存到内存变量
    busiDataOrigin.copyFrom(_busi_data);
}

void WinDiagnosisSuggestion::busiDataToUi(const CBusiDataSuggestion &_busi_data)
{
    ui->txtSuggestion->setText(_busi_data.suggestion);
}

void WinDiagnosisSuggestion::uiToBusiData(CBusiDataSuggestion &_busi_data)
{
    QString text = ui->txtSuggestion->toPlainText();
    _busi_data.suggestion = text;
}

void WinDiagnosisSuggestion::askAndSave(CBusiDataSuggestion &_busi_data)
{
    QString text = tr("是否保存修改?");   // "Save the modifications?"
    bool ret = getWinManage()->showNoticeWin(text);
    if (ret) {
        saveBusiData(_busi_data);
    }
}

void WinDiagnosisSuggestion::on_btnLoad_clicked()
{
    // 检查 U 盘是否存在
    QString path_udisk = Util::CUDisk::getPath();
    if (path_udisk.length() == 0) {
        getWinManage()->showMsgWin(tr("未找到U盘"));    // "U-Disk not found"
        return;
    }

    // 检查诊治建议文件是否存在
    QString path_file = path_udisk + QDir::separator() + REPORT_SUGGESTION_FILE_NAME;
    if (!QFile::exists(path_file)) {
        getWinManage()->showMsgWin(tr("在U盘里未找到文件“%1”").arg(REPORT_SUGGESTION_FILE_NAME));   // "File \"%1\" not found in U-Disk"
        return;
    }

    // 载入诊治建议到临时业务数据对象
    CBusiDataSuggestion busi_data;
    QString text;
    bool is_succ_read = Util::readFileToQStr(path_file, text, "UTF-8");
    if (is_succ_read) {
        busi_data.suggestion = text;    // TODO: 根据字数、报表中该文字的字体及行宽，来自动分行？
    } else {
        // TODO: log?

        getWinManage()->showMsgWin(tr("文件读入时出错"));  // "Error when file read in"
        return;
    }

    // 将临时业务数据设置到 UI
    busiDataToUi(busi_data);

    // 状况提示
    getWinManage()->showSuspensionPrompt(tr("导入成功"));   // "Import succeeded"

}
