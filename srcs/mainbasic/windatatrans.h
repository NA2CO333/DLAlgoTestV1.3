#ifndef DATATRANS_H
#define DATATRANS_H

#include <QHBoxLayout>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QSettings>
#include <QCheckBox>

#include "baseform.h"
#include "myeditline.h"
#include "DataTransmit.h"
#include "qrcodeinput.h"
#include "statusbarform.h"
#include "landevfinderclientqt.h"
#include "waitingmovie.h"
#include "websocket.h"
#include "data-intf-guanxin.h"

namespace Ui {
class WinDataTrans;
}

// 数据接口配置
// NOTE: 遍历方法：从 min 到 max 且不在 dataInterfaceCfg_Invalid_xx 内
enum enDataInterfaceCfg {
    dataInterfaceCfg_Unknown            = -1,
    dataInterfaceCfg_ManylinksCloud     = 0,                                // 万灵云端
    dataInterfaceCfg_PcSoftware         = 2,                                // 视筛 PC 端      // NOTE: 不连续值，兼容旧版
    dataInterfaceCfg_Http               ,                                   // 自定义 http
    dataInterfaceCfg_Bluetooth          ,                                   // 蓝牙
    dataInterfaceCfg_UsbUart            ,                                   // USB 转 UART
    dataInterfaceCfg_Uart               ,                                   // UART
    dataInterfaceCfg_GuanXin            ,                                   // 新疆冠新

    dataInterfaceCfg_Min = dataInterfaceCfg_ManylinksCloud,                 // 最小值
    dataInterfaceCfg_Max = dataInterfaceCfg_GuanXin,                        // 最大值
    dataInterfaceCfg_Invalid_01 = 1,                                        // 无效值 01
};
QString enumToText_DataInterfaceCfg(enDataInterfaceCfg _intf_cfg);                   // 枚举值转文本：接口数据配置
DataTrans::enConnMode DataInterfaceCfg_to_ConnMode(enDataInterfaceCfg _intf_cfg);       // 获取【数据接口配置】对应的连接方式
void getDataInterfaceCfgItems(const QVector<int> *&_values, const QVector<QString> *&_captions);    // 获取【数据接口配置】选项列表（包括各个有效的枚举值和对应的描述文本）

// Http 接口配置
struct stHttpIntfCfg
{
    QString receiverAddr;               // 接收端地址
    int receiverPort;                   // 接收端端口

    QString pathData;                   // 筛查结果接口路径
    QString pathClient;                 // 受测者信息接口路径
    QString pathClientList;             // 受测者批量信息路径
    QString pathAuth;                   // 授权接口路径
    QString pathImage;                  // 图像接口路径

    QString authUserName;               // 身份验证用户名
    QString authPassword;               // 身份验证密码
    QString authUserType;               // 身份验证用户标识

    bool isUseHttps;                    // 是否使用 Https 通信
    bool isNeedAuth;                    // 接收端是否需要身份验证

    //
    void reset();

    //
    bool isEqualTo(const stHttpIntfCfg &_obj) const;

};

// 万灵云端-门诊 接口配置
const stHttpIntfCfg DATATRANS_CFG_CLOUD_OUTPATIENT = {
    "opt.manylinksmed.com",             // 接收端地址
    0,                                  // 接收端端口

    "/api-v1/data",                     // 筛查结果接口路径
    "",                                 // 受测者信息接口路径
    "/api-v1/batch",                    // 受测者批量信息路径
    "/api-v1/auth",                     // 授权接口路径
    "/api-v1/image",                    // 图像接口路径

    "",                                 // 身份验证用户名
    "",                                 // 身份验证密码
    "",                                 // 身份验证用户标识

    true,                               // 是否使用 Https 通信
    true,                               // 接收端是否需要身份验证
};

// 万灵云端-学校 接口配置
const stHttpIntfCfg DATATRANS_CFG_CLOUD_SCHOOL = {
    "s.manylinksmed.com",               // 接收端地址
    0,                                  // 接收端端口

    "/api-school/data",                 // 筛查结果接口路径
    "",                                 // 受测者信息接口路径
    "/api-school/batch",                // 受测者批量信息路径
    "/api-school/auth",                 // 授权接口路径
    "/api-school/image",                // 图像接口路径

    "",                                 // 身份验证用户名
    "",                                 // 身份验证密码
    "",                                 // 身份验证用户标识

    true,                               // 是否使用 Https 通信
    true,                               // 接收端是否需要身份验证
};

// 万灵视筛PC端 接口配置
const stHttpIntfCfg DATATRANS_CFG_PC_TERMINAL = {
    "192.168.1.",                       // 接收端地址
    8081,                               // 接收端端口

    "/data",                            // 筛查结果接口路径
    "/client",                          // 受测者信息接口路径
    "/batch",                           // 受测者批量信息路径
    "/auth",                            // 授权接口路径
    "/image",                           // 图像接口路径

    "test",                             // 身份验证用户名
    "123",                              // 身份验证密码
    "ssi",                              // 身份验证用户标识

    false,                              // 是否使用 Https 通信
    false,                              // 接收端是否需要身份验证
};

// 本窗体的业务数据
class CBusiDataDataTrans : public CBusiData
{
public:
    enDataInterfaceCfg intfType;            // 接口配置类型

    stHttpIntfCfg httpIntf;                 // http 接口

    bool isPostImmediately;                 // 是否自动上传
    bool isUploadImage;                     // 是否上传图像
    bool isAutoQuerySubject;                // 是否自动查询被测者信息

    bool isExternalControl;                 // 是否外部控制（即 v1.3、1.4的“受控版”）
    bool isAutoTurnLampWhenExternalControl; // 自控模式时是否自动转灯

    int serialBaud;                         // 串口波特率

    QString devCode;                        // 设备编号     // NOTE: 数据上传接口的“设备编号”是独立设置的，可能与实际编号不一致（因为以前对接的客户系统接收端做了不恰当的前缀过滤）

    stGuanXinIntfCfg guanXinCfg;            // 新疆冠新接口配置

    //
    CBusiDataDataTrans()
    {
        reset();
    }

    //
    void reset() override;

    //
    bool isEqualTo(const CBusiDataDataTrans &_busi_data) const;

};

//
class WinDataTrans : public CBaseWidget
{
    Q_OBJECT

public:
    explicit WinDataTrans(QWidget *parent = 0);
    ~WinDataTrans();

    static const int DEFAULT_USB_SERIAL_BARD = (int)QSerialPort::Baud115200;

    static QVector<int> *getSerialBaudList();
    static bool checkSerialBaud(int &_baud);

    bool checkNetwork();

    static void syncToDataTransmiter(const CBusiDataDataTrans &_busi_data);     // 将参数同步到数据传输接口

    static enDataInterfaceCfg getCfg_intfType();        // 获得【数据传输的接口配置类型】的配置值

    static bool isManylinksDataIntf();                  // 判断当前配置的接口是否万灵云端
    static bool isManylinksProtocal;                    // 是否使用万灵云端协议（仅调试用）

    static void configToBusiData(CBusiDataDataTrans &_busi_data);               // 将配置文件里的配置设置到业务实体对象
    static void busiDataToConfig(const CBusiDataDataTrans &_busi_data);         // 保存业务数据到配置文件

    static void getGuanXinIntfCfg(stGuanXinIntfCfg &_cfg);                      // 获取“新疆冠新”接口配置
    static void setGuanXinIntfCfg(const stGuanXinIntfCfg &_cfg);                // 保存“新疆冠新”接口配置

Q_SIGNALS:
    /**
     * @brief 结果上传
     * @param _list_upload_ids  需要上传的记录的 id 列表
     */
    void sigUpLoadData(QVector<int> _list_upload_ids);
    //
    void sigVerifyAuth();

public Q_SLOTS:
    void slot_mproSysPushSvc_ConnStatChanged(Net::Remote::CWebSocket::enConnStat _curr_stat);

protected Q_SLOTS:
    void slot_testFeedback(int _test_type, bool _is_succ, QString _msg);       // 参数 _test_type：0-鉴权测试，1-数据测试
    void slot_receiveQrCodeData(QString);
    void slotFinderFinished();

protected:
    void showEvent(QShowEvent*);

    static QVector<int> *serialBaudList;

    //
    void setLineEditCheck(bool check);

    bool check_cbbConnMode(DataTrans::enConnMode _conn_mode, bool _is_show_msg = false);    // 修改后的数据检查及校正：连接方式
    void setCtrlsStatByIntfType(const enDataInterfaceCfg _intf_cfg_type);                   // （在“接口配置类型”的选项改变后）根据选项设置控件可见、有效状态，等

    //
    void updateTheme(enThemeType _theme);                                   // 更新主题
    void updateLanguage();                                                  // 更新语言

    void busiDataToUi(const CBusiDataDataTrans &_busi_data);                // 将 数据 设置到 UI
    void uiToBusiData(CBusiDataDataTrans &_busi_data);                      // 从 UI 取值到 数据

    QString checkValues(const CBusiDataDataTrans &_busi_data);              // 检查各个值是否合法

    bool askAndSave(const CBusiDataDataTrans &_busi_data);                  // 询问用户是否需要保存，若需要则保存

    //
    void setHttpIntfDefaultCfgToUi(enDataInterfaceCfg _intf_type);          // 将 http 接口的默认配置设置到 UI 中

    //
    CBusiDataDataTrans busiDataOrigin;

    qrcodeInput *scanInput {nullptr};

    CLanDevFinderClientQt *devFinderClient = Q_NULLPTR;

    CWaitingMovie *lblWaiting = Q_NULLPTR;

    myEditLine *edtCloudAddr = Q_NULLPTR;

    QString m_currMProPushConnStatDesc;         // 当前 MPro 推送连接状态描述

    const QVector<int> *m_intfItemValues {nullptr};             // 接口选项列表 - 枚举值
    const QVector<QString> *m_intfItemCaptions {nullptr};       // 接口选项列表 - 描述文本

private slots:
    void on_btnBack_clicked();
    void on_btnHome_clicked();
    void on_btnSave_clicked();

    void on_btnTestData_clicked();
    void on_btnTestAuth_clicked();
    void on_btnScanInput_clicked();
    void on_btnFindServer_clicked();

    void on_cbbIntfConfigType_currentIndexChanged(int _index);
    void on_cbbCloudAddr_activated(const QString &arg1);

    void on_btnWebSocketTest_clicked();

    void on_ckbManylinksProtocal_clicked(bool _checked);
private:
    Ui::WinDataTrans *ui;
};

#endif // DATATRANS_H
