#ifndef PRINTERSETTING_H
#define PRINTERSETTING_H

#include <QSettings>
#include <QCheckBox>
#include <QList>
#include <QLabel>
#include <QMovie>

#include "baseform.h"
#include "myeditline.h"
#include "statusbarform.h"
#include "data.h"
#include "print-intf.h"
#include "waitingmovie.h"

namespace Ui {
class printerSetting;
}

// 小票打印类型
enum enTicketPrintConnType
{
    ticketPrintConnType_WiFi,       // WiFi 连接
    ticketPrintConnType_BT,         // 蓝牙连接
};

// “打印设置” 窗体的业务数据类
class CBusiDataPrintSetting : public CBusiData
{
public:
    bool isAutoPrintTicket;                         // 是否自动打印小票
    enTicketPrintConnType ticketPrintConnType;      // 小票打印连接类型
    QString wifiPrinterIP;                          // WiFi 小票打印机 IP
    int wifiPrinterPort;                            // WiFi 小票打印机 端口
    QString organizationName;                       // 机构名称（小票的）

    QString orgNameA4;                              // 机构名称（A4报告的）
    QString operatorName;                           // 操作者

    // 重置（还原）
    void reset() override;

    // 比较
    bool isEqualTo(const CBusiDataPrintSetting &_busi_data) const;

};

//
class printerSetting : public CBaseWidget
{
    Q_OBJECT

public:
    explicit printerSetting(QWidget *parent = 0);
    ~printerSetting();

    static QString getCfg_WifiPrinterIP();                                  // 获得 WiFi小票打印机 IP 的配置值
    static int getCfg_WifiPrinterPort();                                    // 获得 WiFi小票打印机端口 的配置值

protected:
    void showEvent(QShowEvent *) override;
    //void afterShow() override;

    myEditLine *ipEdit = Q_NULLPTR;
    myEditLine *portEdit = Q_NULLPTR;
    myEditLine *edtOrganizationName = Q_NULLPTR;
    myEditLine *edtOrgNameA4 = Q_NULLPTR;
    myEditLine *edtOperator = Q_NULLPTR;

    //
    CBusiDataPrintSetting busiDataOrigin;           // 原始业务数据

    QList<Common::stPrinterInfo> listPrinterInfo;

    QTimer timerRefreshJobCount;

    CWaitingMovie *lblWaiting = Q_NULLPTR;

    //
    void updateTheme(enThemeType _theme);                                   // 更新主题

    void configToBusiData(CBusiDataPrintSetting &_busi_data);               // 将配置文件里的配置设置到业务实体对象
    void saveBusiData(const CBusiDataPrintSetting &_busi_data);             // 保存业务数据

    void busiDataToUi(const CBusiDataPrintSetting &_busi_data);             // 将 数据 设置到 UI
    void uiToBusiData(CBusiDataPrintSetting &_busi_data);                   // 从 UI 取值到 数据

    //QString checkValues(const CBusiDataDataTrans &_busi_data);              // 检查各个值是否合法

    void askAndSave(const CBusiDataPrintSetting &_busi_data);               // 询问用户是否需要保存，若需要则保存

    //
    void startRefreshJobCount(int _interval);           // 开始刷新打印任务数
    bool importImgFromUdisk(const QString &_udisk_path, const QString &_file_name, QString &_msg);   // 从 U 盘导入 A4 报告图像文件

    void loadOrgLogoImg();
    void loadQrCodeImg();

    void updateLanguage();
private slots:
    void on_pushButton_Back_clicked();
    void on_pushButton_Home_clicked();
    void on_pushButton_Save_clicked();
    void on_checkBox_WifiPrint_clicked(bool checked);
    void on_checkBox_BtPrint_clicked(bool checked);
    void on_btnBtConn_clicked();
    void on_btnFindPrinterList_clicked();
    void on_cbbPrinterList_activated(int index);
    void on_btnCancelAllJobs_clicked();
    void on_btnUpdateImgs_clicked();
    void on_btnDiagnosticStandard_clicked();
    void on_btnDiagnosisSuggestion_clicked();
    void on_tabWidget_currentChanged(int index);

    void slot_printIntf_SearchFinished();
    void slot_timerRefreshJobCount_timeout();

private:
    Ui::printerSetting *ui;

};

#endif // PRINTERSETTING_H
