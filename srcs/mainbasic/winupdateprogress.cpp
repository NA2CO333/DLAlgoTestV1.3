//系统更新处理
#include "winupdateprogress.h"
#include "ui_winupdateprogress.h"

#include "statusbarform.h"
#include "windowsmanager.h"
#include "appsetting.h"
#include "global.h"
#include "update.h"
#include "winmanage.h"

//
WinUpdateProgress::WinUpdateProgress(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::WinUpdateProgress)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    //
    this->setGeometry(0, 26, 800, 454);

    ui->progressBarTotal->setValue(0);
    ui->progressBarCurrent->setValue(0);
    ui->labelCurrentFile->setText("");

}

WinUpdateProgress::~WinUpdateProgress()
{
    delete ui;
}

void WinUpdateProgress::showEvent(QShowEvent *)
{
    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 样式刷新
    updateTheme(getSysThemeType());

}

void WinUpdateProgress::updateTheme(enThemeType _theme)
{
    //
    //QPalette palette;
    if (themeType_Black == _theme) {
        // 自身的背景
        //palette.setBrush(this->backgroundRole(), QColor(qRgb(1, 1, 1)));

        //
        this->setStyleSheet(
R"(
QWidget {
    background-color: transparent;
    color: rgb(204, 204, 204);
}
QPushButton {
    background-color: rgb(28, 28, 30);
    border-radius: 3px;
}
QProgressBar {
    background-color: rgb(204, 204, 204);
    color: rgb(64, 64, 64);
}
#lblDownloadSpeed {
    color: rgb(64, 64, 64);
}
)"
                    );

    } else if (themeType_White == _theme) {
        // TODO:

    }
    //this->setPalette(palette);
    //this->setAutoFillBackground(true);

}

void WinUpdateProgress::on_pushButtonCancel_clicked()
{
    emit sigCancelUpdate();
}

void WinUpdateProgress::slotShowProgress(bool _is_shown)
{
    if (_is_shown) {
        if (this->isHidden()) {
            getWinManage()->showWindowByType(WIN_UPDATE_PROGRESS);
        }
    } else {
        if (!this->isHidden()) {
            getWinManage()->backToLastWidget();
        }
    }
}

void WinUpdateProgress::slotCurrentFileChanged(QString _file_path)
{
    ui->labelCurrentFile->setText(_file_path);
}

void WinUpdateProgress::slotProgressChanged(int _current, int _total)
{
    if (_current >= 0) {
        ui->progressBarCurrent->setValue(_current);
    }
    if (_total >= 0) {
        ui->progressBarTotal->setValue(_total);
    }
}

void WinUpdateProgress::slotSpeedChanged(double _speed, int _unit)
{
    QString unit_str = (0 == _unit ? "B/s" : (1 == _unit ? "KB/s" : (2 == _unit ? "MB/s" : "?")));
    QString speed_str = QString::number(_speed, 'f', 2) + " " + unit_str;
    ui->lblDownloadSpeed->setText(speed_str);
}
