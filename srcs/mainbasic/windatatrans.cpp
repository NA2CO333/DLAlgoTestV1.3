//数据传输界面
#include "windatatrans.h"
#include "ui_windatatrans.h"

#include <QSpacerItem>
#include <QtPrintSupport/QPrinter>
//#include <QNetworkConfigurationManager>

#include "DataTransmit.h"
#include "messagewin.h"
#include "noticewin.h"
#include "windowsmanager.h"
#include "appsetting.h"
#include "global.h"

using namespace DataTrans;

// 筛查仪PC端软件查找消息关键字（区分大小写）
#define SCREENER_PC_SOFTWARE_FINDER_KEYWORD "ScreenerPcSoftwareFind-7eef637fe921"

///=============================================================================================
///

QString enumToText_DataInterfaceCfg(enDataInterfaceCfg _intf_cfg)
{
    switch (_intf_cfg) {
    case dataInterfaceCfg_Unknown           : return "Unknown";
    case dataInterfaceCfg_ManylinksCloud    : return WinDataTrans::tr("万灵云端");          // "ManyLinks Cloud"
    case dataInterfaceCfg_PcSoftware        : return WinDataTrans::tr("万灵视筛PC端");      // "ManyLinks PC Software"
    case dataInterfaceCfg_Http              : return WinDataTrans::tr("使用 http");         // "HTTP"
    case dataInterfaceCfg_Bluetooth         : return WinDataTrans::tr("使用 蓝牙");         // "Bluetooth"
    case dataInterfaceCfg_UsbUart           : return WinDataTrans::tr("使用 USB-串口");     // "USB-Serial"
    case dataInterfaceCfg_Uart              : return WinDataTrans::tr("使用 UART");         // "UART"
    case dataInterfaceCfg_GuanXin           : return WinDataTrans::tr("新疆冠新");          // "Xinjiang GuanXin"
    default                                 : return "???";
    }
}

enConnMode DataInterfaceCfg_to_ConnMode(enDataInterfaceCfg _intf_cfg)
{
    switch (_intf_cfg) {
    case dataInterfaceCfg_Unknown           : return DataTrans::connMode_Unknown;
    case dataInterfaceCfg_ManylinksCloud    : return DataTrans::connMode_Http;
    case dataInterfaceCfg_PcSoftware        : return DataTrans::connMode_Http;
    case dataInterfaceCfg_Http              : return DataTrans::connMode_Http;
    case dataInterfaceCfg_Bluetooth         : return DataTrans::connMode_Bluetooth;
    case dataInterfaceCfg_UsbUart           : return DataTrans::connMode_UsbUart;
    case dataInterfaceCfg_Uart              : return DataTrans::connMode_Uart;
    case dataInterfaceCfg_GuanXin           : return DataTrans::connMode_Http;
    default                                 : return DataTrans::connMode_Unknown;
    }
}

void getDataInterfaceCfgItems(const QVector<int> *&_values, const QVector<QString> *&_captions)
{
    //
    static QVector<int> values;
    static QVector<QString> captions;

    //
    if (values.isEmpty()) {
        enDataInterfaceCfg intf_type;
        bool is_intf_type_valid;
        for (int i = dataInterfaceCfg_Min; i <= dataInterfaceCfg_Max; i++) {
            intf_type = (enDataInterfaceCfg)i;
            is_intf_type_valid = !(intf_type < dataInterfaceCfg_Min || intf_type > dataInterfaceCfg_Max
                                   || intf_type == dataInterfaceCfg_Invalid_01);
            if (is_intf_type_valid) {
                values.append((int)intf_type);
                captions.append(enumToText_DataInterfaceCfg(intf_type));
            }
        }
    }

    //
    _values = &values;
    _captions = &captions;
}

///=============================================================================================
/// struct stHttpIntfCfg

//
void stHttpIntfCfg::reset()
{
    receiverAddr    = "";
    receiverPort    = 0;

    pathData        = "";
    pathClient      = "";
    pathClientList  = "";
    pathAuth        = "";
    pathImage       = "";

    authUserName    = "";
    authPassword    = "";
    authUserType    = "";

    isUseHttps      = false;
    isNeedAuth      = false;
}

//
bool stHttpIntfCfg::isEqualTo(const stHttpIntfCfg &_obj) const
{
    return (true &&
            _obj.receiverAddr   == this->receiverAddr   &&
            _obj.receiverPort   == this->receiverPort   &&

            _obj.pathData       == this->pathData       &&
            _obj.pathClient     == this->pathClient     &&
            _obj.pathClientList == this->pathClientList &&
            _obj.pathAuth       == this->pathAuth       &&
            _obj.pathImage      == this->pathImage      &&

            _obj.authUserName   == this->authUserName   &&
            _obj.authPassword   == this->authPassword   &&
            _obj.authUserType   == this->authUserType   &&

            _obj.isUseHttps     == this->isUseHttps     &&
            _obj.isNeedAuth     == this->isNeedAuth     &&
            true
            );
}

///=============================================================================================
/// struct stBusiDataDataTrans

void CBusiDataDataTrans::reset()
{
    intfType = dataInterfaceCfg_Unknown;

    httpIntf.reset();

    isPostImmediately                   = false;
    isUploadImage                       = false;
    isAutoQuerySubject                  = false;

    isExternalControl                   = false;
    isAutoTurnLampWhenExternalControl   = false;

    serialBaud                          = WinDataTrans::getSerialBaudList()->at(0);

    guanXinCfg.ip = "";
    guanXinCfg.port = 0;

    devCode = "";

}

bool CBusiDataDataTrans::isEqualTo(const CBusiDataDataTrans &_busi_data) const
{
    return (true &&
            _busi_data.intfType                             == this->intfType                               &&

            _busi_data.httpIntf.isEqualTo(this->httpIntf) &&

            _busi_data.isPostImmediately                    == this->isPostImmediately                      &&
            _busi_data.isUploadImage                        == this->isUploadImage                          &&
            _busi_data.isAutoQuerySubject                   == this->isAutoQuerySubject                     &&

            _busi_data.isExternalControl                    == this->isExternalControl                      &&
            _busi_data.isAutoTurnLampWhenExternalControl    == this->isAutoTurnLampWhenExternalControl      &&

            _busi_data.serialBaud                           == this->serialBaud                             &&

            _busi_data.devCode                              == this->devCode                                &&

            _busi_data.guanXinCfg.isEqualTo(this->guanXinCfg)                                               &&

            true);
}

///=============================================================================================
/// class WinDataTrans

//
QVector<int> *WinDataTrans::serialBaudList = Q_NULLPTR;
bool WinDataTrans::isManylinksProtocal = false;             // 是否使用万灵云端协议（仅调试用）

//
WinDataTrans::WinDataTrans(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::WinDataTrans)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    //
    connect(this, &WinDataTrans::sigUpLoadData, g_uploadThread, &UpLoadThread::slotUploadData);
    connect(g_uploadThread, &UpLoadThread::testFeedback, this, &WinDataTrans::slot_testFeedback);
    connect(this, &WinDataTrans::sigVerifyAuth, g_uploadThread, &UpLoadThread::verifyAuth);

    //
    scanInput = NULL;

    // 初始化下拉选项框 “接口配置类别”
    for (int i = 0; i < getSerialBaudList()->size(); i++) {
        ui->cbbSerialBaud->addItem(QString::number(getSerialBaudList()->at(i)), getSerialBaudList()->at(i));
    }

    // 清掉设计期间设置的 StyleSheet
    QList<QWidget *> list_childs = this->findChildren<QWidget *>();
    for (int i = 0; i < list_childs.size(); i++) {
        list_childs[i]->setStyleSheet("");
    }

    //
    getDataInterfaceCfgItems(m_intfItemValues, m_intfItemCaptions);

    // 【数据接口配置】选择框的初始化
    ui->cbbIntfConfigType->clear();
    for (int i = 0; i < m_intfItemCaptions->size(); i++) {
        ui->cbbIntfConfigType->addItem(m_intfItemCaptions->at(i));
    }
    ui->cbbIntfConfigType->setCurrentIndex(-1);

    // 设备查找客户端的初始化
    devFinderClient = new CLanDevFinderClientQt;
    devFinderClient->setMessageKeyword(SCREENER_PC_SOFTWARE_FINDER_KEYWORD);
    QObject::connect(devFinderClient, &CLanDevFinderClientQt::sigFinderFinished, this, &WinDataTrans::slotFinderFinished, Qt::QueuedConnection);

    // 等待动画
    lblWaiting = new CWaitingMovie((QWidget *)(ui->btnFindServer->parent()), ":/resource/uploading.gif", 32);
    lblWaiting->move(ui->btnFindServer->x() + (ui->btnFindServer->width() - lblWaiting->width()) / 2,
                     ui->btnFindServer->y() + (ui->btnFindServer->height() - lblWaiting->height()) / 2
                     );

    //
    edtCloudAddr = new myEditLine;
    ui->cbbCloudAddr->setLineEdit(edtCloudAddr);

}

WinDataTrans::~WinDataTrans()
{
    qDebug() << "WinDataTrans::~dataTrans()";
    delete ui;
}

void WinDataTrans::updateLanguage()
{
    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("数据传输"));    // "Data Transmission"

    //
    //if (language) {
    //    // TODO: 改为用全局字符串列表来设置？
    //    ;
    //
    //    //
    //    ui->cbbIntfConfigType->setItemText(0, "万灵云端-门诊");
    //    ui->cbbIntfConfigType->setItemText(1, "万灵云端-学校");
    //    ui->cbbIntfConfigType->setItemText(2, "万灵视筛PC端");
    //    ui->cbbIntfConfigType->setItemText(3, "使用 http");
    //    ui->cbbIntfConfigType->setItemText(4, "使用 蓝牙");
    //    ui->cbbIntfConfigType->setItemText(5, "使用 USB串口");
    //    ui->cbbIntfConfigType->setItemText(6, "使用 串口(UART)");
    //
    //    //
    //    ui->ckbAutoUpload->setText("自动上传");
    //    ui->ckbIsUploadImage->setText("上传图像");
    //    ui->ckbIsAutoGetInfo->setText("查询被测者信息");
    //
    //    //
    //    ui->lblHttpAddr->setText("IP/域名：");
    //    ui->lblHttpPort->setText("端口：");
    //    ui->lblPathData->setText("数据接收接口：");
    //    ui->lblPathImage->setText("图像接收接口：");
    //    ui->lblPathSubject->setText("被测者信息接口：");
    //    ui->lblPathSubjectList->setText("批量名单接口：");
    //    ui->lblPathAuth->setText("登录接口：");
    //    ui->lblUser->setText("用户名：");
    //    ui->lblPassword->setText("密码：");
    //    ui->lblAccountTag->setText("账号标识：");
    //
    //    ui->ckbIsNeedAuth->setText("需要登录");
    //    ui->ckbIsUseHttps->setText("使用 https");
    //
    //    //
    //    ui->lblCloudAddr->setText("IP/域名：");
    //    ui->lblCloudPort->setText("端口：");
    //    ui->lblCloudUser->setText("用户名：");
    //    ui->lblCloudPwd->setText("密码：");
    //
    //    ui->btnFindServer->setText("查找");
    //
    //    //
    //    ui->ckbIsExternalControl->setText("受控模式");
    //    ui->ckbAutoTurnLamp->setText("受控时自动转灯");
    //
    //    ui->lblBaudRate->setText("波特率");
    //
    //    //
    //    ui->lblHome->setText("主页");
    //    ui->lblSave->setText("保存");
    //    ui->lblBack->setText("返回");
    //    ui->lblTestData->setText("数据测试");
    //    ui->lblTestAuth->setText("登录测试");
    //    ui->lblScanInput->setText("扫码设置参数");
    //} else {
    //    //
    //    ui->cbbIntfConfigType->setItemText(0, "ManyLinks-Outpatient");
    //    ui->cbbIntfConfigType->setItemText(1, "ManyLinks-School");
    //    ui->cbbIntfConfigType->setItemText(2, "ManyLinks PC Software");
    //    ui->cbbIntfConfigType->setItemText(3, "use http");
    //    ui->cbbIntfConfigType->setItemText(4, "use bluetooth");
    //    ui->cbbIntfConfigType->setItemText(5, "use USB-Serial");
    //    ui->cbbIntfConfigType->setItemText(6, "use SerialPort3");
    //
    //    //
    //    ui->ckbAutoUpload->setText("Auto Upload");
    //    ui->ckbIsUploadImage->setText("Upload Image");
    //    ui->ckbIsAutoGetInfo->setText("Query Subject Info.");
    //
    //    //
    //    ui->lblHttpAddr->setText("IP/DomainName: ");
    //    ui->lblHttpPort->setText("Port: ");
    //    ui->lblPathData->setText("Data Recv Path: ");
    //    ui->lblPathImage->setText("Image Upload Path: ");
    //    ui->lblPathSubject->setText("Subject Query path: ");
    //    ui->lblPathSubjectList->setText("Batch List Query: ");
    //    ui->lblPathAuth->setText("Auth path: ");
    //    ui->lblUser->setText("User name: ");
    //    ui->lblPassword->setText("Password: ");
    //    ui->lblAccountTag->setText("Account Tag: ");
    //
    //    ui->ckbIsNeedAuth->setText("NeedAuth");
    //    ui->ckbIsUseHttps->setText("use https");
    //
    //    //
    //    ui->lblCloudAddr->setText("IP/DomainName: ");
    //    ui->lblCloudPort->setText("Port: ");
    //    ui->lblCloudUser->setText("User: ");
    //    ui->lblCloudPwd->setText("Password：");
    //
    //    ui->btnFindServer->setText("Find");
    //
    //    //
    //    ui->ckbIsExternalControl->setText("ControlledMode");
    //    ui->ckbAutoTurnLamp->setText("AutoTurnLampWhenControlled");
    //
    //    ui->lblBaudRate->setText("BaudRate: ");
    //
    //    //
    //    ui->lblHome->setText("Home");
    //    ui->lblSave->setText("Save");
    //    ui->lblBack->setText("Back");
    //    ui->lblTestData->setText("Data Test");
    //    ui->lblTestAuth->setText("Auth Test");
    //    ui->lblScanInput->setText("Scan Input");
    //}
}

void WinDataTrans::showEvent(QShowEvent *)
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

    // （在“接口配置类型”的选项改变后）根据选项设置控件可见、有效状态，等
    setCtrlsStatByIntfType(busiDataOrigin.intfType);

    // 其它动态更新的 UI 部件文本
    ui->lblMProPushStat->setText(m_currMProPushConnStatDesc);

    // 控件【可见/可用】状态
    ui->wgtWebSocketTest->setVisible(CGlobal::isDebugMode);

    ui->edtGuanXin_PathUpload->setReadOnly(!CGlobal::isDebugMode);
    ui->edtGuanXin_PathQuery->setReadOnly(!CGlobal::isDebugMode);

    ui->ckbManylinksProtocal->setVisible(CGlobal::isDebugMode);

}

void WinDataTrans::updateTheme(enThemeType _theme)
{
    static QString form_style_black;

    static bool form_style_black_read = false;
    if (!form_style_black_read) {
        Util::readFileToQStr(":/resource/qss/windatatrans.qss", form_style_black);
        form_style_black_read = true;
    }

    //
    if (themeType_Black == _theme) {
        this->setStyleSheet(form_style_black);

        //

    } else if (themeType_White == _theme) {
        // TODO:
    }

}

QVector<int> *WinDataTrans::getSerialBaudList()
{
    if (!serialBaudList) {
        serialBaudList = new QVector<int>();

        serialBaudList->push_back(QSerialPort::Baud19200);
        serialBaudList->push_back(QSerialPort::Baud38400);
        serialBaudList->push_back(QSerialPort::Baud57600);
        serialBaudList->push_back(QSerialPort::Baud115200);
    }
    return serialBaudList;
}

bool WinDataTrans::checkNetwork()
{
//    QNetworkConfigurationManager mgr;
//    if(mgr.isOnline()){
//        qDebug()<<"checkNetwork::device is online";
//        return true;
//    }
//    else{
//        qDebug()<<"checkNetwork::device is offline";
//        return false;
//    }
    return g_WifiIntf->getIsConnected();
}

void WinDataTrans::slot_testFeedback(int _test_type, bool _is_succ, QString _msg)
{
    ui->btnTestData->setEnabled(true);
    ui->btnTestAuth->setEnabled(true);
    setLineEditCheck(true);

    QString text = (0 == _test_type ? tr("登录") : tr("数据"));  // "Authority "  "Data "
    text += (_is_succ ? tr("测试成功！") : tr("测试失败！"));   // "test success!"   "test failed!"
    if (_msg.length() > 0) {
        text += ("\n" + _msg);
    }

    qDebug() << (_is_succ ? "test sucess!" : "test failed!");
    getWinManage()->showMsgWin(text);
}

void WinDataTrans::on_btnTestData_clicked()
{
    // 先保存
    CBusiDataDataTrans busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        if (!askAndSave(busi_data)) {
            return;
        }
    }

    //
    if(!checkNetwork())
    {
        qDebug() << "network is disconnect!";
        QString text = tr("网络未连接!");    // "Network is disconnect!"
        getWinManage()->showMsgWin(text);
        return;
    }

    //
    if (DataTransmiter::ReceiverAddr == "")
    {
        QString text = tr("请输入接收端ip地址!");   // "Please enter IP!"
        getWinManage()->showMsgWin(text);

        return;
    }
//    if(DataTransmiter::ReceiverPort==""){
//        MessageWin msg;
//        if(language)
//            msg.setContent("请输入接收端端口号!");
//        else
//            msg.setContent("Please input port num!");

//        msg.exec();
//        return;
//    }

//    QString log;
//    bool ret = true;
//    if(usrVerifyState){
//        ret = DataTransmiter::GetAuthToken(log);
//        if(ret){
//            qDebug()<<"GetAuth  success:"<<QString::fromStdString(log);;
//        }
//        else
//            qDebug()<<"GetAuth  failed:"<<QString::fromStdString(log);;

//    }

    if(true)
    {
        QVector<int> list;
        list.clear();
        list.push_back(-1);     /* id = -1 表示测试 */
        emit sigUpLoadData(list);                       // TODO: 如果后面逻辑不严谨没有信号回传，这里将不可恢复，看起来像是卡死？
        ui->btnTestData->setEnabled(false);
        setLineEditCheck(false);
        /*
        if(DataTransmiter::SendMeasureData(log, Pat)){
            qDebug()<<"test sucess!";
            MessageWin msg;
            if(language)
                msg.setContent("数据测试成功!");
            else
                msg.setContent("test success!");
            msg.exec();
            return;
        }
        else{
            qDebug()<<"test failed:"<<QString::fromStdString(log);
            MessageWin msg;
            if(language)
                msg.setContent("数据测试失败!");
            else
                msg.setContent("test failed!");
            msg.exec();
            return;
        }
        */
    }
}

void WinDataTrans::on_btnTestAuth_clicked()// test auth
{
    // 先保存
    CBusiDataDataTrans busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        if (!askAndSave(busi_data)) {
            return;
        }
    }

    //
    if(!checkNetwork())
    {
        qDebug() << "network is disconnect!";
        QString text = tr("网络未连接!");    // "Network is disconnect!"
        getWinManage()->showMsgWin(text);
        return;
    }

    //
    qDebug() << "ip:" << QString::fromStdString(DataTransmiter::ReceiverAddr);
    if (DataTransmiter::ReceiverAddr == "")
    {
        MessageWin msg;
        msg.setContent(tr("请输入接收端ip地址!"));  // "Please input ip!"

        msg.exec();
        return;
    }
//    if(DataTransmiter::ReceiverPort==""){
//        MessageWin msg;
//        if(language)
//            msg.setContent("请输入接收端端口号!");
//        else
//            msg.setContent("Please input port num!");

//        msg.exec();
//        return;
//    }

    ui->btnTestAuth->setEnabled(false);
    emit sigVerifyAuth();
    setLineEditCheck(false);
}


void WinDataTrans::on_btnBack_clicked()
{
    CBusiDataDataTrans busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    getWinManage()->backToLastWidget();
}

void WinDataTrans::on_btnHome_clicked()
{
    CBusiDataDataTrans busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    }

    //
    getWinManage()->showWindowByType(WIN_HOME);
}

void WinDataTrans::on_btnSave_clicked()
{
    CBusiDataDataTrans busi_data;
    uiToBusiData(busi_data);
    if (!busi_data.isEqualTo(busiDataOrigin)) {
        askAndSave(busi_data);
    } else {
        getWinManage()->showSuspensionPrompt(tr("数据未被修改")); // "Data not modified"
    }
}

void WinDataTrans::on_btnScanInput_clicked()
{
    scanInput = new qrcodeInput;    //实例化一个二维码接收的类
    connect(scanInput, &qrcodeInput::sendQRcodedata, this, &WinDataTrans::slot_receiveQrCodeData);    //发送扫码参数

    scanInput->exec();

    disconnect(scanInput, &qrcodeInput::sendQRcodedata, this, &WinDataTrans::slot_receiveQrCodeData);
    delete scanInput;
    scanInput = nullptr;
}

void WinDataTrans::slot_receiveQrCodeData(QString data)
{
    qDebug() << "WinDataTrans::receiveQRcodeData:" << data;

    //
    if (data.length() == 0 || data.at(0) == '\n') {
        getWinManage()->showSuspensionPrompt(tr("扫码得到的内容为空"));  // "The content obtained by scanning the code is empty"
        return;
    }

    //
    string msg;

    if (DataTransmiter::SetFromCSV(msg, data.toStdString())) {
        qDebug() << "get csv success! ConnMode=" << DataTransmiter::ConnMode;

        //
        enDataInterfaceCfg intf_type = dataInterfaceCfg_Http;

        //
        ui->cbbIntfConfigType->setCurrentIndex(m_intfItemValues->indexOf((int)intf_type));

        //
        ui->edtHttpAddr->setText(QString::fromStdString(DataTransmiter::ReceiverAddr));
        ui->edtHttpPort->setText(DataTransmiter::ReceiverPort != 0 ? QString::number(DataTransmiter::ReceiverPort, 10) : "");
        ui->edtPathData->setText(QString::fromStdString(DataTransmiter::PathData));
        ui->edtPathImage->setText(QString::fromStdString(DataTransmiter::PathImage));
        ui->ckbAutoUpload->setChecked(DataTransmiter::IsPostImmediately);
        ui->ckbIsUploadImage->setChecked(DataTransmiter::IsUploadImage);
        ui->ckbIsNeedAuth->setChecked(DataTransmiter::IsNeedAuth);
        ui->edtPathAuth->setText(QString::fromStdString(DataTransmiter::PathAuth));
        ui->edtPathSubject->setText(QString::fromStdString(DataTransmiter::PathClient));
        ui->edtUser->setText(QString::fromStdString(DataTransmiter::AuthUserName));
        ui->edtPassword->setText(QString::fromStdString(DataTransmiter::AuthPassword));
        ui->edtAccountTag->setText(QString::fromStdString(DataTransmiter::AuthUserType));
        ui->ckbIsUseHttps->setChecked(DataTransmiter::IsUseHttps);
        ui->edtPathSubjectList->setText(QString::fromStdString(DataTransmiter::PathClientList));   //批量接口路径  2020.9.22  tao
    } else {
        getWinManage()->showSuspensionPrompt(tr("二维码解析失败！"), -1);   // "Failed to parse the QR code!"
    }

    qDebug() << "SetFromCSV log:" << QString::fromStdString(msg);
}

void WinDataTrans::setLineEditCheck(bool check)
{
    ui->edtHttpAddr->setEnabled(check);
    ui->edtHttpPort->setEnabled(check);
    ui->edtPathData->setEnabled(check);
    ui->edtPathAuth->setEnabled(check);
    ui->edtPathSubject->setEnabled(check);
    ui->edtPathSubjectList->setEnabled(check);
    ui->edtUser->setEnabled(check);
    ui->edtPassword->setEnabled(check);
    ui->edtAccountTag->setEnabled(check);
    ui->edtPathImage->setEnabled(check);
}

void WinDataTrans::configToBusiData(CBusiDataDataTrans &_busi_data)
{
    stHttpIntfCfg &http_intf = _busi_data.httpIntf;

    //
    _busi_data.intfType             = getCfg_intfType();

    //
    http_intf.receiverAddr      = appSetting::value("/data/ip").toString();
    http_intf.receiverPort      = appSetting::value("/data/port").toInt();

    http_intf.pathData          = appSetting::value("/data/datapath").toString();
    http_intf.pathAuth          = appSetting::value("/data/authpath").toString();
    http_intf.pathClient        = appSetting::value("/data/usrpath").toString();
    http_intf.pathClientList    = appSetting::value("/data/clientlistpath").toString();
    http_intf.pathImage         = appSetting::value("/data/imagepath").toString();

    http_intf.authUserName      = appSetting::value("/data/usr").toString();
    http_intf.authPassword      = appSetting::value("/data/code").toString();
    http_intf.authUserType      = appSetting::value("/data/authId").toString();

    http_intf.isUseHttps        = appSetting::value("/data/isHttps").toBool();
    http_intf.isNeedAuth        = appSetting::value("/data/usrverify").toBool();

    //
    _busi_data.isUploadImage        = appSetting::value("/data/uploadimage").toBool();
    _busi_data.isPostImmediately    = appSetting::value("/data/upload").toBool();
    _busi_data.isAutoQuerySubject   = appSetting::value("/data/autoGetInfo").toBool();

    _busi_data.devCode              = appSetting::value("/data/devCode").toString();

    _busi_data.isExternalControl                    = CGlobal::getIsExternalControl(true);
    _busi_data.isAutoTurnLampWhenExternalControl    = CGlobal::isAutoTurnLampWhenExternalControl;

    _busi_data.serialBaud           = CGlobal::dataTransSerialBaud;

    getGuanXinIntfCfg(_busi_data.guanXinCfg);

}

void WinDataTrans::busiDataToConfig(const CBusiDataDataTrans &_busi_data)
{
    const stHttpIntfCfg &http_intf = _busi_data.httpIntf;

    // 保存到配置文件
    appSetting::setValue("/data/intfConfigType",    _busi_data.intfType);

    //
    appSetting::setValue("/data/ip",                http_intf.receiverAddr);
    appSetting::setValue("/data/port",              http_intf.receiverPort);

    appSetting::setValue("/data/datapath",          http_intf.pathData);
    appSetting::setValue("/data/imagepath",         http_intf.pathImage);
    appSetting::setValue("/data/authpath",          http_intf.pathAuth);
    appSetting::setValue("/data/usrpath",           http_intf.pathClient);
    appSetting::setValue("/data/clientlistpath",    http_intf.pathClientList);

    appSetting::setValue("/data/usr",               http_intf.authUserName);
    appSetting::setValue("/data/code",              http_intf.authPassword);
    appSetting::setValue("/data/authId",            http_intf.authUserType);

    appSetting::setValue("/data/isHttps",           http_intf.isUseHttps);
    appSetting::setValue("/data/usrverify",         http_intf.isNeedAuth);

    //
    appSetting::setValue("/data/uploadimage",       _busi_data.isUploadImage        );
    appSetting::setValue("/data/upload",            _busi_data.isPostImmediately    );
    appSetting::setValue("/data/autoGetInfo",       _busi_data.isAutoQuerySubject   );

    appSetting::setValue("/data/devCode",           _busi_data.devCode              );

    // 同步到全局变量
    syncToDataTransmiter(_busi_data);

    //CGlobal::isAutoQuerySubject                 = _busi_data.isAutoQuerySubject;            // TODO: 增加这个全局变量，避免其它模块直接从配置文件读取？

    CGlobal::setIsExternalControl(_busi_data.isExternalControl);
    CGlobal::isAutoTurnLampWhenExternalControl  = _busi_data.isAutoTurnLampWhenExternalControl;

    CGlobal::dataTransSerialBaud    = _busi_data.serialBaud;

    CGlobal::saveConfs();

    setGuanXinIntfCfg(_busi_data.guanXinCfg);

    // 同步到全局对象
    //if (is_datatrans_serialport_ok)   // 根据选项改变数据传输串口模块的参数、开关状态等          // TODO: 每必要在此处就打开？
    {
        QString msg;
        g_SerialDatatrans->checkAndSet(&msg);
        if (msg.length() > 0) {
            getWinManage()->showSuspensionPrompt(msg, 3000);
        }
    }

}

void WinDataTrans::getGuanXinIntfCfg(stGuanXinIntfCfg &_cfg)
{
    static stGuanXinIntfCfg DEFAULT_CFG;

    _cfg.ip         = appSetting::value("/DataIntf_GuanXin/ip"          , DEFAULT_CFG.ip            ).toString();
    _cfg.port       = appSetting::value("/DataIntf_GuanXin/port"        , DEFAULT_CFG.port          ).toUInt();
    _cfg.pathUpload = appSetting::value("/DataIntf_GuanXin/pathUpload"  , DEFAULT_CFG.pathUpload    ).toString();
    _cfg.pathQuery  = appSetting::value("/DataIntf_GuanXin/pathQuery"   , DEFAULT_CFG.pathQuery     ).toString();
}

void WinDataTrans::setGuanXinIntfCfg(const stGuanXinIntfCfg &_cfg)
{
    appSetting::setValue("/DataIntf_GuanXin/ip"         , _cfg.ip           );
    appSetting::setValue("/DataIntf_GuanXin/port"       , _cfg.port         );
    appSetting::setValue("/DataIntf_GuanXin/pathUpload" , _cfg.pathUpload   );
    appSetting::setValue("/DataIntf_GuanXin/pathQuery"  , _cfg.pathQuery    );
}

void WinDataTrans::busiDataToUi(const CBusiDataDataTrans &_busi_data)
{
    const stHttpIntfCfg &http_intf = _busi_data.httpIntf;

    //
    ui->cbbIntfConfigType->setCurrentIndex(m_intfItemValues->indexOf((int)_busi_data.intfType));

    //
    ui->edtHttpAddr->setText(http_intf.receiverAddr);
    ui->edtHttpPort->setText(http_intf.receiverPort != 0 ? QString::number(http_intf.receiverPort) : "");

    ui->edtPathImage->setText(http_intf.pathImage);
    ui->edtPathAuth->setText(http_intf.pathAuth);
    ui->edtPathData->setText(http_intf.pathData);
    ui->edtPathSubject->setText(http_intf.pathClient);
    ui->edtPathSubjectList->setText(http_intf.pathClientList);

    ui->edtUser->setText(http_intf.authUserName);
    ui->edtPassword->setText(http_intf.authPassword);
    ui->edtAccountTag->setText(http_intf.authUserType);

    ui->ckbIsNeedAuth->setChecked(http_intf.isNeedAuth);
    ui->ckbIsUseHttps->setChecked(http_intf.isUseHttps);

    //
    ui->ckbAutoUpload->setChecked(_busi_data.isPostImmediately);
    ui->ckbIsUploadImage->setChecked(_busi_data.isUploadImage);
    ui->ckbIsAutoGetInfo->setChecked(_busi_data.isAutoQuerySubject);

    ui->ckbIsExternalControl->setChecked(_busi_data.isExternalControl);
    ui->ckbAutoTurnLamp->setChecked(CGlobal::isAutoTurnLampWhenExternalControl);

    ui->cbbSerialBaud->setCurrentIndex(getSerialBaudList()->indexOf(CGlobal::dataTransSerialBaud));

    //
    if (connMode_Http == DataInterfaceCfg_to_ConnMode(_busi_data.intfType))
    {
        edtCloudAddr->setText(http_intf.receiverAddr);
        ui->edtCloudPort->setText(QString::number(http_intf.receiverPort));
        ui->edtCloudUser->setText(http_intf.authUserName);
        ui->edtCloudPwd->setText(http_intf.authPassword);
    }

    ui->edtGuanXin_IP->setText(                     _busi_data.guanXinCfg.ip            );
    ui->edtGuanXin_Port->setText(QString::number(   _busi_data.guanXinCfg.port          ));
    ui->edtGuanXin_PathUpload->setText(             _busi_data.guanXinCfg.pathUpload    );
    ui->edtGuanXin_PathQuery->setText(              _busi_data.guanXinCfg.pathQuery     );



    if (dataInterfaceCfg_Http == _busi_data.intfType) {
        ui->edtDevCode_Http->setText(_busi_data.devCode);
    } else if (dataInterfaceCfg_Bluetooth == _busi_data.intfType ||
               dataInterfaceCfg_UsbUart == _busi_data.intfType ||
               dataInterfaceCfg_Uart == _busi_data.intfType
               ) {
        ui->edtDevCode_Serial->setText(_busi_data.devCode);
    }

}

void WinDataTrans::uiToBusiData(CBusiDataDataTrans &_busi_data)
{
    stHttpIntfCfg &http_intf = _busi_data.httpIntf;

    //
    int intf_type_idx = ui->cbbIntfConfigType->currentIndex();
    _busi_data.intfType = (intf_type_idx >= 0 ? (enDataInterfaceCfg)m_intfItemValues->at(intf_type_idx) : dataInterfaceCfg_Unknown);

    //
    http_intf.receiverAddr      = ui->edtHttpAddr->text();
    http_intf.receiverPort      = ui->edtHttpPort->text().toUInt();

    http_intf.pathImage         = ui->edtPathImage->text();
    http_intf.pathAuth          = ui->edtPathAuth->text();
    http_intf.pathData          = ui->edtPathData->text();
    http_intf.pathClient        = ui->edtPathSubject->text();
    http_intf.pathClientList    = ui->edtPathSubjectList->text();

    http_intf.authUserName  = ui->edtUser->text();
    http_intf.authPassword  = ui->edtPassword->text();
    http_intf.authUserType  = ui->edtAccountTag->text();

    http_intf.isNeedAuth    = ui->ckbIsNeedAuth->isChecked();
    http_intf.isUseHttps    = ui->ckbIsUseHttps->isChecked();

    if (dataInterfaceCfg_ManylinksCloud == _busi_data.intfType
            || dataInterfaceCfg_PcSoftware == _busi_data.intfType)
    {
        http_intf.authUserName = ui->edtCloudUser->text();
        http_intf.authPassword = ui->edtCloudPwd->text();
        http_intf.receiverAddr = edtCloudAddr->text();
        http_intf.receiverPort = ui->edtCloudPort->text().toInt();

        http_intf.isNeedAuth = (http_intf.authUserName.length() > 0);
    }

    //
    _busi_data.isPostImmediately    = ui->ckbAutoUpload->isChecked();
    _busi_data.isUploadImage        = ui->ckbIsUploadImage->isChecked();
    _busi_data.isAutoQuerySubject   = ui->ckbIsAutoGetInfo->isChecked();

    _busi_data.isExternalControl                    = ui->ckbIsExternalControl->isChecked();
    _busi_data.isAutoTurnLampWhenExternalControl    = ui->ckbAutoTurnLamp->isChecked();

    _busi_data.serialBaud           = getSerialBaudList()->at(ui->cbbSerialBaud->currentIndex());

    _busi_data.guanXinCfg.ip            = ui->edtGuanXin_IP->text();
    _busi_data.guanXinCfg.port          = ui->edtGuanXin_Port->text().toUInt();
    _busi_data.guanXinCfg.pathUpload    = ui->edtGuanXin_PathUpload->text();
    _busi_data.guanXinCfg.pathQuery     = ui->edtGuanXin_PathQuery->text();

    if (dataInterfaceCfg_Http == _busi_data.intfType) {
        _busi_data.devCode          = ui->edtDevCode_Http->text();
    } else if (dataInterfaceCfg_Bluetooth == _busi_data.intfType ||
               dataInterfaceCfg_UsbUart == _busi_data.intfType ||
               dataInterfaceCfg_Uart == _busi_data.intfType
               ) {
        _busi_data.devCode          = ui->edtDevCode_Serial->text();
    }

}

QString WinDataTrans::checkValues(const CBusiDataDataTrans &_busi_data)
{
    //
    QString msg = "";

    //
    if (_busi_data.intfType < 0) {
        return (tr("请选择连接方式")); // "Please select connection method"
    }
    enConnMode conn_mode = DataInterfaceCfg_to_ConnMode(_busi_data.intfType);

    // 检查：如果 “连接方式” 选了 USB 串口或 串口(UART)，要做相应检查和提示
    /*bool is_datatrans_serialport_ok =*/ check_cbbConnMode(conn_mode, /*_show_msg*/true);
    //if (!is_datatrans_serialport_ok) {
    //    msg += ;
    //}

    //
    if (_busi_data.isPostImmediately)
    {
        if(((_busi_data.httpIntf.receiverAddr.length() < 3 || _busi_data.httpIntf.receiverAddr.contains(" ")) &&
            (edtCloudAddr->text().length() < 3 || edtCloudAddr->text().contains(" "))
            ) && (connMode_Http == conn_mode))
        {
            qDebug() << "ReceiverAddr is invalid!";
            if (msg.length() > 0)
                msg += "\r\n";
            msg += tr("请设置正确的接收地址！");   // "Please set correct IP!"
        }
    }

    if (connMode_UsbUart == conn_mode || connMode_Uart == conn_mode) {
        if (ui->cbbSerialBaud->currentIndex() < 0) {
            if (msg.length() > 0)
                msg += "\r\n";
            msg += tr("请选择波特率！");   // "Please select BaudRate!"
        }
    }

    // TODO: 网址、服务路径等字段的非法字符检查？


    //
    return msg;
}

bool WinDataTrans::askAndSave(const CBusiDataDataTrans &_busi_data)
{
    enDataInterfaceCfg intf_type_old = busiDataOrigin.intfType;

    //
    QString text = tr("是否保存修改?");   // "Save the modifications?"
    bool ret = getWinManage()->showNoticeWin(text);
    QString msg_check;
    if (ret) {
        msg_check = checkValues(_busi_data);
        if (msg_check.length() > 0) {
            getWinManage()->showSuspensionPrompt(msg_check, 3000);
        } else {
            //
            busiDataToConfig(_busi_data);

            // 同步到本模块的数据对象
            busiDataOrigin = _busi_data;

            // 若接口类型改变了，重新设置
            if (intf_type_old != _busi_data.intfType) {
                globalService()->autoSetStatOfMProSysPushSvcCommunic(g_WifiIntf->getIsConnected());
            }
        }
    }

    //
    return (ret ? (msg_check.length() == 0) : false);
}

void WinDataTrans::setHttpIntfDefaultCfgToUi(enDataInterfaceCfg _intf_type)
{
    if (dataInterfaceCfg_ManylinksCloud == _intf_type)          // 万灵云端
    {
        //if (!ui->wgtCloudAddr->isVisible())
        {
            edtCloudAddr->setText(          DATATRANS_CFG_CLOUD_OUTPATIENT.receiverAddr);
            ui->edtCloudPort->setText(      QString::number(DATATRANS_CFG_CLOUD_OUTPATIENT.receiverPort));
        }

        ui->edtPathData->setText(           DATATRANS_CFG_CLOUD_OUTPATIENT.pathData);
        ui->edtPathAuth->setText(           DATATRANS_CFG_CLOUD_OUTPATIENT.pathAuth);
        ui->edtPathSubject->setText(        DATATRANS_CFG_CLOUD_OUTPATIENT.pathClient);
        ui->edtPathSubjectList->setText(    DATATRANS_CFG_CLOUD_OUTPATIENT.pathClientList);
        ui->edtPathImage->setText(          DATATRANS_CFG_CLOUD_OUTPATIENT.pathImage);

        //ui->edtCloudUser->setText(          DATATRANS_CFG_CLOUD_OUTPATIENT.authUserName);
        //ui->edtCloudPwd->setText(           DATATRANS_CFG_CLOUD_OUTPATIENT.authPassword);
        ui->edtAccountTag->setText(         DATATRANS_CFG_CLOUD_OUTPATIENT.authUserType);

        ui->ckbIsNeedAuth->setChecked(      DATATRANS_CFG_CLOUD_OUTPATIENT.isNeedAuth);
        ui->ckbIsUseHttps->setChecked(      DATATRANS_CFG_CLOUD_OUTPATIENT.isUseHttps);
    }
    else if (dataInterfaceCfg_PcSoftware == _intf_type)        // 万灵视筛仪 PC 软件
    {
        //if (!ui->wgtCloudAddr->isVisible())
        {
            edtCloudAddr->setText(          DATATRANS_CFG_PC_TERMINAL.receiverAddr);
            ui->edtCloudPort->setText(      QString::number(DATATRANS_CFG_PC_TERMINAL.receiverPort));
        }

        ui->edtPathData->setText(           DATATRANS_CFG_PC_TERMINAL.pathData);
        ui->edtPathAuth->setText(           DATATRANS_CFG_PC_TERMINAL.pathAuth);
        ui->edtPathSubject->setText(        DATATRANS_CFG_PC_TERMINAL.pathClient);
        ui->edtPathSubjectList->setText(    DATATRANS_CFG_PC_TERMINAL.pathClientList);
        ui->edtPathImage->setText(          DATATRANS_CFG_PC_TERMINAL.pathImage);

        //ui->edtCloudUser->setText(          DATATRANS_CFG_PC_TERMINAL.authUserName);      /* 这个用户名和密码保留用户填写的信息，体验更好？ */
        //ui->edtCloudPwd->setText(           DATATRANS_CFG_PC_TERMINAL.authPassword);
        ui->edtAccountTag->setText(         DATATRANS_CFG_PC_TERMINAL.authUserType);

        ui->ckbIsNeedAuth->setChecked(      DATATRANS_CFG_PC_TERMINAL.isNeedAuth);
        ui->ckbIsUseHttps->setChecked(      DATATRANS_CFG_PC_TERMINAL.isUseHttps);
    }
}

void WinDataTrans::syncToDataTransmiter(const CBusiDataDataTrans &_busi_data)
{
    const stHttpIntfCfg &http_intf = _busi_data.httpIntf;

    DataTransmiter::DevCode             = _busi_data.devCode.toStdString();

    DataTransmiter::ReceiverAddr        = http_intf.receiverAddr.toStdString();
    DataTransmiter::ReceiverPort        = http_intf.receiverPort;

    DataTransmiter::PathData            = http_intf.pathData.toStdString();
    DataTransmiter::PathAuth            = http_intf.pathAuth.toStdString();
    DataTransmiter::PathClient          = http_intf.pathClient.toStdString();
    DataTransmiter::PathClientList      = http_intf.pathClientList.toStdString();
    DataTransmiter::PathImage           = http_intf.pathImage.toStdString();

    DataTransmiter::AuthUserName        = http_intf.authUserName.toStdString();
    DataTransmiter::AuthPassword        = http_intf.authPassword.toStdString();
    DataTransmiter::AuthUserType        = http_intf.authUserType.toStdString();

    DataTransmiter::IsNeedAuth          = http_intf.isNeedAuth;
    DataTransmiter::IsUseHttps          = http_intf.isUseHttps;

    DataTransmiter::ConnMode            = DataInterfaceCfg_to_ConnMode(_busi_data.intfType);

    DataTransmiter::IsPostImmediately   = _busi_data.isPostImmediately;
    DataTransmiter::IsUploadImage       = _busi_data.isUploadImage;

}

enDataInterfaceCfg WinDataTrans::getCfg_intfType()
{
    enDataInterfaceCfg intf_type = (enDataInterfaceCfg)appSetting::value("/data/intfConfigType").toInt();
    return intf_type;
}

bool WinDataTrans::isManylinksDataIntf()
{
    bool ret = (false
                || (dataInterfaceCfg_ManylinksCloud == WinDataTrans::getCfg_intfType())
                || (
                    (dataInterfaceCfg_Http == WinDataTrans::getCfg_intfType())
                    && (
                        (DataTrans::DataTransmiter::ReceiverAddr.find("manylinksmed.com") != std::string::npos)
                        || (DataTrans::DataTransmiter::ReceiverAddr.find("120.25.254.38") != std::string::npos)
                        || (DataTrans::DataTransmiter::ReceiverAddr.find("120.79.74.94") != std::string::npos)
                       )
                   )
                );
    return ret;
}

// 检查 “连接方式” 选项的正确性
bool WinDataTrans::check_cbbConnMode(enConnMode _conn_mode, bool _is_show_msg)
{
    if (!this->isVisible() && _is_show_msg) {
        //logger.warning_c();
        _is_show_msg = false;
    }

    if (connMode_UsbUart == _conn_mode) {                 // 如果选了 USB 串口，则检查是否存在
        if (CSerialDatatrans::getUsbSerialPortPath().length() == 0) {
            if (_is_show_msg) {
                QString msg = tr("未发现USB串口，请另选");   // "USB Serial Port not found, please select another"
                getWinManage()->showSuspensionPrompt(msg, 3000);
            }
            return false;
        }
    } else if (connMode_Uart == _conn_mode) {          // 如果选了串口(UART)，则检查是否被测距模块占用
        if (CDistanceDetect::isSensorNeedPort3(/*CGlobal::distSensorType*/ g_WinMeasure->distanceDetect()->sensorType())) {
            if (_is_show_msg) {
                QString msg = tr("串口(UART)已被占用，请另选");   // "Port3 is occupied by ranging module, please select another"
                getWinManage()->showSuspensionPrompt(msg, 3000);
            }
            return false;
        }
    }

    //
    return true;
}

void WinDataTrans::on_cbbIntfConfigType_currentIndexChanged(int _index)
{
    if (!this->isVisible()) {
        return;
    }
    // 得到当前选定的接口配置类别
    int intf_type_idx = _index;
    enDataInterfaceCfg intf_type = (intf_type_idx >= 0 ? (enDataInterfaceCfg)m_intfItemValues->at(intf_type_idx) : dataInterfaceCfg_Unknown);

    if (intf_type < dataInterfaceCfg_Min || intf_type > dataInterfaceCfg_Max
            || intf_type == dataInterfaceCfg_Invalid_01) {
        return;
    }

    //
    enConnMode conn_mode = DataInterfaceCfg_to_ConnMode(intf_type);

    // 选项检查：如果选了串口(UART)，则检查是否被占用
    check_cbbConnMode(conn_mode, true);

    // （在“接口配置类型”的选项改变后）根据选项设置控件可见、有效状态，等
    setCtrlsStatByIntfType(intf_type);

    // 将 http 接口的默认配置设置到 UI 中
    setHttpIntfDefaultCfgToUi(intf_type);

//    //
//    g_SerialDatatrans->isChinese = language;
//    QString msg;
//    bool is_need_open = (connMode_UsbUart == conn_mode);
//    bool succ_open = g_SerialDatatrans->setIsOpened(is_need_open, msg);
//    if (!succ_open) {
//        logDebug(QString::asprintf("CSerialDatatrans::on_cbbConnMode_currentIndexChanged(): open datatrans serialport failed: %s", msg.toUtf8().data()), CGlobal::LOG_DATATRANS);
//        if (this->isVisible())      // TODO: 在视筛仪里运行时，若在窗体构造时调用了下面这句弹出对话框，则会发生"Segmentation fault"异常？
//            getWinManage()->showMsgWin(msg);
//    } else {
//        logDebug(QString::asprintf("CSerialDatatrans::on_cbbConnMode_currentIndexChanged(): is_need_open =  %s", Util::bool2str(is_need_open)), CGlobal::LOG_DATATRANS);
//    }

}

//
void WinDataTrans::setCtrlsStatByIntfType(const enDataInterfaceCfg _intf_cfg_type)
{
    if (_intf_cfg_type < dataInterfaceCfg_Min || _intf_cfg_type > dataInterfaceCfg_Max
            || _intf_cfg_type == dataInterfaceCfg_Invalid_01) {
        return;
    }

    //
    enConnMode conn_mode = DataInterfaceCfg_to_ConnMode((enDataInterfaceCfg)_intf_cfg_type);
    bool is_http_mode = (connMode_Http == conn_mode);

    // 配置控件页切换
    switch (_intf_cfg_type) {
    case dataInterfaceCfg_ManylinksCloud:               // 万灵云端接口
        ui->stackedWidget->setCurrentWidget(ui->pageCloud);

        ui->wgtCloudAddr->setVisible(CGlobal::isDebugMode);

        break;
    case dataInterfaceCfg_PcSoftware:                   // 万灵视筛 PC 端接口
        ui->stackedWidget->setCurrentWidget(ui->pageCloud);

        ui->wgtCloudAddr->setVisible(true);

        break;
    case dataInterfaceCfg_Http:                         // 自定义 HTTP 接口
        ui->stackedWidget->setCurrentWidget(ui->pageHttp);

        break;
    case dataInterfaceCfg_GuanXin:                      // 新疆冠新接口
        ui->stackedWidget->setCurrentWidget(ui->pageGuanXin);

        break;
    default:                                            // 其它接口
        ui->stackedWidget->setCurrentWidget(ui->pageSerial);
    }

    // 设置控件有效性（或可见性）
    ui->ckbIsUploadImage->setVisible(is_http_mode && !(dataInterfaceCfg_GuanXin == _intf_cfg_type
                                                       ));

    ui->ckbIsAutoGetInfo->setVisible(is_http_mode && !(dataInterfaceCfg_ManylinksCloud == _intf_cfg_type
                                                       || dataInterfaceCfg_PcSoftware == _intf_cfg_type
                                                       || dataInterfaceCfg_GuanXin == _intf_cfg_type
                                                       ));

    ui->ckbIsExternalControl->setVisible(!is_http_mode);        // 只有“蓝牙”和“串口”连接方式，才能选择“受控模式”

    ui->ckbIsNeedAuth->setVisible(is_http_mode);

    ui->btnTestData->setVisible(is_http_mode && !(dataInterfaceCfg_GuanXin == _intf_cfg_type
                                                  ));
    ui->lblTestData->setVisible(is_http_mode && !(dataInterfaceCfg_GuanXin == _intf_cfg_type
                                                  ));
    ui->btnTestAuth->setVisible(is_http_mode && !(dataInterfaceCfg_GuanXin == _intf_cfg_type
                                                  ));
    ui->lblTestAuth->setVisible(is_http_mode && !(dataInterfaceCfg_GuanXin == _intf_cfg_type
                                                  ));

    ui->btnScanInput->setVisible(is_http_mode && !(dataInterfaceCfg_ManylinksCloud == _intf_cfg_type
                                                   || dataInterfaceCfg_PcSoftware == _intf_cfg_type
                                                   || dataInterfaceCfg_GuanXin == _intf_cfg_type
                                                   ));
    ui->lblScanInput->setVisible(is_http_mode && !(dataInterfaceCfg_ManylinksCloud == _intf_cfg_type
                                                   || dataInterfaceCfg_PcSoftware == _intf_cfg_type
                                                   || dataInterfaceCfg_GuanXin == _intf_cfg_type
                                                   ));

    ui->frmSerialBaud->setVisible(connMode_UsbUart == conn_mode || connMode_Uart == conn_mode);

}

bool WinDataTrans::checkSerialBaud(int &_baud)
{
    if (getSerialBaudList()->indexOf(_baud) >= 0) {
        return true;
    } else {
        _baud = DEFAULT_USB_SERIAL_BARD;
        return false;
    }
}

void WinDataTrans::on_btnFindServer_clicked()
{
    // 清空现有选项
    ui->cbbCloudAddr->clear();

    // 禁用按钮
    ui->btnFindServer->setEnabled(false);

    // 显示等待动画
    lblWaiting->setIsPlaying(true);

    //
    devFinderClient->startFind(false);

}

void WinDataTrans::slotFinderFinished()
{
    // 停止等待动画
    lblWaiting->setIsPlaying(false);

    // 启用按钮
    ui->btnFindServer->setEnabled(true);

    //
    QString addr = "";
    int port = 0;
    bool is_succ = false;
    int count = devFinderClient->getResultCount();
    for (int i = 0; i < count; i++) {
        is_succ = devFinderClient->getFindResult(addr, port, i);
        if (is_succ) {
            ui->cbbCloudAddr->addItem(QString("%1:%2").arg(addr).arg(port));

            if (0 == count) {
                edtCloudAddr->setText(addr);
                ui->edtCloudPort->setText(QString::number(port));
            }
        } else {
            break;
        }
    }

    if (count > 1) {
        getWinManage()->showSuspensionPrompt(tr("找到的服务器数量大于1，请选择需要的服务器"));  // "The number of servers found is greater than 1,\nplease select the desired server"
    } else if (1 == count) {
        on_cbbCloudAddr_activated(ui->cbbCloudAddr->itemText(0));
        getWinManage()->showSuspensionPrompt(tr("查找成功"));   // "Finding succeeded"
    } else {
        getWinManage()->showSuspensionPrompt(tr("查找失败"));   // "Finding failed"
    }
}

void WinDataTrans::slot_mproSysPushSvc_ConnStatChanged(Net::Remote::CWebSocket::enConnStat _curr_stat)
{
    //
    m_currMProPushConnStatDesc = Net::Remote::CWebSocket::enumToStr_TimerStat(_curr_stat);

    //
    if (this->isVisible()) {
        ui->lblMProPushStat->setText(m_currMProPushConnStatDesc);
    }
}

void WinDataTrans::on_cbbCloudAddr_activated(const QString &arg1)
{
    if (arg1.contains(":")) {
        QStringList list_values = arg1.split(":");
        if (list_values.count() >= 2) {
            edtCloudAddr->setText(list_values[0]);
            ui->edtCloudPort->setText(list_values[1]);
        }
    }
}

void WinDataTrans::on_btnWebSocketTest_clicked()
{
    QString msg = ui->edtWebSocketTest->text();
    globalService()->mproSysPushSvcCommunic()->webSocket()->sendTextMessage(msg);
    getWinManage()->showSuspensionPrompt("Text Message Sent:\n" + msg);
}

void WinDataTrans::on_ckbManylinksProtocal_clicked(bool _checked)
{
    WinDataTrans::isManylinksProtocal = _checked;
}
