//上传数据处理
#include "uploadthread.h"

#include <QDebug>
#include <QFile>
#include <QBuffer>

#include "opencv2/highgui/highgui.hpp"

#include "mysqlitepatients.h"
#include "windatatrans.h"
#include "personalinfos.h"
#include "mainwindow.h"
#include "result.h"
#include "global.h"
#include "settings/settings.h"
#include "windowsmanager.h"
#include "util-app.h"
#include "nettools.h"
#include "global_intf.h"

//
using namespace DataTrans;

//
UpLoadThread::UpLoadThread(QObject *parent) : QObject(parent)
{
    //
    DataTrans::DataTransmiter::Callback_GetNewSubject = this->callbackDataTransmiterGetNewSubject;
    DataTrans::DataTransmiter::Callback_OperationLocked = this->callbackDataTransmiterOperationLocked;

}

UpLoadThread::~UpLoadThread()
{
    qDebug() << "~UpLoadThread()";
}

/* TODO: 若这里没有回传结果信号就退出，会导致前面的等待过程一直无法结束，看起来像是卡死了？
 * TODO: 且回传信号要判断是用 testFeedback() 还是 sigUpLoadDataFeedback() ？若没调用对，也会导致前面的过程卡死？这不安全。
*/
void UpLoadThread::slotUploadData(QVector<int> _list_upload_ids)       // TODO: 传输结果时没必要再查询？
{
    qDebug() << "UpLoadThread::upLoadData";

    logDebug(QString::asprintf("UpLoadThread::upLoadData(): ThreadId = %lld", (qintptr)QThread::currentThreadId()), CGlobal::LOG_DATATRANS);

    //
    if(!checkNetwork() && (connMode_Http == DataTransmiter::ConnMode))
    {
        QString msg = "newwork is disconnected,upload failed";
        qDebug() << msg;
        QVector<int> succ_ids;
        emit sigUpLoadDataFeedback(_list_upload_ids.size(), 0, 0, msg);
        return;
    }

    //
    if (_list_upload_ids.size() == 0) {
        logWarning(QString("%1: dataset is enpty!").arg(__PRETTY_FUNCTION__), CGlobal::LOG_DATATRANS);
        emit sigUpLoadDataFeedback(_list_upload_ids.size(), 0, 0, tr("上传失败：没有传入数据"));  // "upload failed: no data"
        return;
    }

    //
    bool is_test = (_list_upload_ids.at(0) == -1);      /* id = -1 表示测试 */

    //
    MySQLitePatients *mysql = MySQLitePatients::getInstance();
    vector<CPatient> pats;

    if (is_test)       // 数据测试
    {
        pats.clear();
        CPatient test;
        test.patientid = "TEST001";
        test.patientname = "test";
        test.setAgeRange(ageRange_4_20_100_YEAE);
        test.patientsex = "M";
        test.setBirthDate(Util::strToDate("19980215"));
        test.patientlefteyesph = "-2.50";
        test.patientlefteyecyl = "-1.00";
        test.patientlefteyeax = "173.00";
        test.patientleftse = "-3.00";
        test.patientleftpd = "3.83";
        test.patientleftptosis = false;
        test.patientlefths = "10";
        test.patientleftvs = "-25";
        test.patientrighteyesph = "-3.00";
        test.patientrighteyecyl = "-0.25";
        test.patientrighteyeax = "60.00";
        test.patientrightse = "-3.25";
        test.patientrightpd = "3.81";
        test.patientrightptosis = true;
        test.patientrighths = "0";
        test.patientrightvs = "0";
        test.patientpd = "70.21";
        test.patientstuclass = "三年二班";
        test.patienttesttime = QDateTime::currentDateTime().toString(CPatient::dateTimeFormat());
        test.patientPhone = "1358867123";
        test.patientPhone = "Guangzhou hualelu Rishendasha";
        test.patientWechat = "15820588510";
        test.barcodeData = "";
        test.isTest = true;
        test.isBatch = false;
        test.isNeedUpload = true;
        test.isUploaded = true;

        pats.push_back(test);
    }
    else        //查找单个数据
    {
        pats = mysql->findRecordByIdList(_list_upload_ids);
        qDebug() << "pats.size=" << pats.size();
    }

    // 过滤掉未测量的数据
    for (int i = pats.size() - 1; i >= 0; i--) {
        auto it = pats.begin() + i;
        if (!it->isTest) {
            pats.erase(it);
        }
    }

    // 如果是万灵云端，必填字段检查
    //if (WinDataTrans::isManylinksDataIntf()) {
        // TODO:


    //}

    // 得到真正上传的 id list
    QVector<int> list_upload_ids_final;
    foreach (auto pat, pats) {
        list_upload_ids_final.append(pat.id);
    }

    //
    if (pats.size() == 0)
    {
        QVector<int> succ_ids;
        emit sigUpLoadDataFeedback(_list_upload_ids.size(), 0, 0, tr("没有可上传的数据"));  // "No data to upload"
        return;
    }

    // 若是受控模式
    if (CGlobal::getIsExternalControl()) {      /* 注意：若这里没有回传结果信号就退出，会导致前面的等待过程一直无法结束，看起来像是卡死了。*/
        sendBlueToothMeasureResults(pats);
        int count_0 = _list_upload_ids.size();
        int count_1 = pats.size();
        if (is_test) {                          // TODO: 这里没有真正判断是否发送成功？
            emit testFeedback(count_1, true, "");
        } else {
            emit sigUpLoadDataFeedback(count_0, count_1, count_1, "");
        }
        return;
    }

    // 裁减版处理
    Result::reduceResult(pats);

    // 旧代码先发送数据，再发送图像，但 PC 端自动打印功能需要先发送图像，再发送数据
    bool upload_ret = false;
    QVector<int> list_succ = list_upload_ids_final;     // 缺省全部成功，然后在成功列表中移除失败列表的编号
    QVector<int> list_fail;
    enDataInterfaceCfg data_intf = WinDataTrans::getCfg_intfType();
    enConnMode conn_mode = DataInterfaceCfg_to_ConnMode(data_intf);
    QString err_msg;
    if (connMode_Http == conn_mode) {                               // http 连接方式
        if (dataInterfaceCfg_ManylinksCloud == data_intf) {     // 万灵云端
            /* 万灵云端结果上传数据中受检者必有字段：编号，生日，？
             */

            // 接口选择
            bool is_batch = pats.at(0).isBatch;
            for (int i = 1; i < (int)pats.size(); i++) {        // 未支持门诊记录和筛查记录混合上传
                if (pats.at(i).isBatch != is_batch) {
                    emit sigUpLoadDataFeedback(_list_upload_ids.size(), 0, 0, tr("不支持门诊和筛查记录一起上传！"));  // "Cannot upload outpatient and screening records together!"
                    return;
                }
            }

            // 万灵云端的接口，根据默认值重设接口参数
            if (is_batch) {
                DataTransmiter::ReceiverAddr    = DATATRANS_CFG_CLOUD_SCHOOL.receiverAddr.toStdString();
                DataTransmiter::ReceiverPort    = DATATRANS_CFG_CLOUD_SCHOOL.receiverPort;

                DataTransmiter::PathData        = DATATRANS_CFG_CLOUD_SCHOOL.pathData.toStdString();
                DataTransmiter::PathAuth        = DATATRANS_CFG_CLOUD_SCHOOL.pathAuth.toStdString();
                DataTransmiter::PathClient      = DATATRANS_CFG_CLOUD_SCHOOL.pathClient.toStdString();
                DataTransmiter::PathClientList  = DATATRANS_CFG_CLOUD_SCHOOL.pathClientList.toStdString();
                DataTransmiter::PathImage       = DATATRANS_CFG_CLOUD_SCHOOL.pathImage.toStdString();
            } else {
                DataTransmiter::ReceiverAddr    = DATATRANS_CFG_CLOUD_OUTPATIENT.receiverAddr.toStdString();
                DataTransmiter::ReceiverPort    = DATATRANS_CFG_CLOUD_OUTPATIENT.receiverPort;

                DataTransmiter::PathData        = DATATRANS_CFG_CLOUD_OUTPATIENT.pathData.toStdString();
                DataTransmiter::PathAuth        = DATATRANS_CFG_CLOUD_OUTPATIENT.pathAuth.toStdString();
                DataTransmiter::PathClient      = DATATRANS_CFG_CLOUD_OUTPATIENT.pathClient.toStdString();
                DataTransmiter::PathClientList  = DATATRANS_CFG_CLOUD_OUTPATIENT.pathClientList.toStdString();
                DataTransmiter::PathImage       = DATATRANS_CFG_CLOUD_OUTPATIENT.pathImage.toStdString();
            }

            // 发送数据
            upload_ret = sendData(pats, list_succ, list_fail, err_msg);

            // 发送图像             // NOTE: (2025-12-11)须先上传记录，再上传图像，否则云端需第二次上传才能看到图像？
            if (upload_ret) {
                /*upload_ret =*/ sendImage(pats);
            }
        } else if (dataInterfaceCfg_PcSoftware == data_intf) {  // PC 端
            // 发送图像                         // NOTE: (2026-08-07) PC 端要先发送图像，再发送数据，否则“立即打印”功能打印的报告无图？
            if (upload_ret) {
                /*upload_ret =*/ sendImage(pats);
            }

            // 发送数据
            upload_ret = sendData(pats, list_succ, list_fail, err_msg);
        } else if (dataInterfaceCfg_GuanXin == data_intf) {     // 新疆冠新
            //
            upload_ret = sendThroughGuanXin(pats, list_succ, err_msg);
        } else {                                                // 其它 http 连接方式
            // 发送数据
            upload_ret = sendData(pats, list_succ, list_fail, err_msg);

            // 发送图像             // NOTE: 应先上传记录，再上传图像，否则接收端处理可能出错？
            if (upload_ret) {
                /*upload_ret =*/ sendImage(pats);
            }
        }
    } else {                                                        // 其它连接方式
        // 发送数据
        upload_ret = sendData(pats, list_succ, list_fail, err_msg);
    }

    // 反馈给用户
    if (is_test) {
        emit testFeedback(1, upload_ret, err_msg);
        qDebug() << "emit testFeedback(" << upload_ret << ");";
    } else {
        emit sigUpLoadDataFeedback(_list_upload_ids.size(), list_upload_ids_final.size(), list_succ.size(), err_msg);
        qDebug() << "upload " << (upload_ret ? "success" : "failed") << "! succ = " << list_succ.count() << ", fail = " << list_fail.count();
    }
}

bool UpLoadThread::sendData(std::vector<CPatient> &_pats, QVector<int> &_list_succ, QVector<int> &_list_fail, QString &_msg)
{
    bool upload_ret = DataTransmiter::SendMeasureData(_msg, _list_fail, _pats);
    logDebug(QString::asprintf("UpLoadThread::upLoadData(): upload_ret = %s, list_fail.count() = %d",
                               Util::bool2str(upload_ret), _list_fail.count()), CGlobal::LOG_DATATRANS);

    // 构造上传成功 id 列表
    if (_list_fail.count() > 0) {
        int id;
        for (int i = _list_succ.count() - 1; i >= 0; i--) {
            id = _list_succ.at(i);
            if (_list_fail.indexOf(id) >= 0)
                _list_succ.removeAt(i);
        }
    }

    // 设置到数据库       // TODO: 结果分3中情况：应答为成功，应答为失败，无应答（相当多的对接端并无应答，尤其是蓝牙）
    if (_list_succ.count() > 0) {
        setIsUpload(_list_succ, true);
    }
    if (_list_fail.count() > 0) {
        setIsUpload(_list_fail, false);
    }

    //
    return upload_ret;
}

bool UpLoadThread::sendImage(std::vector<CPatient> &_pats)
{
    // TODO: 大批量上传时，没次上传的数据个数应做限制；需上传图片时，应数据和图片小批间隔上传，不应全部分开上传，也不应先上传图片。


    // 批量上传时，禁止上传图像文件   /* 因为没有完备的传输任务管理，大量的文件传送会导致数据传输通道堵塞。 */
    if (_pats.size() > 5 && !Common::Net::isLanIP(QString::fromStdString(DataTransmiter::ReceiverAddr))) {
        QString msg = tr("大批量上传暂不支持上传图像！");     // "Uploading images in bulk is currently not supported!"
        qWarning() << msg.toLocal8Bit().constData();
        globalIntf()->asyncSuspensionPrompt(msg, -1);
    } else {
        string patient_id, testtime, barcode, batchNo;
        std::string msg;

        if (DataTransmiter::IsUploadImage && (connMode_Http == DataTransmiter::ConnMode))    // 只有 http/https 协议支持图像的传送
        {
            for (unsigned int i = 0; i < _pats.size(); i++)
            {
                testtime = _pats.at(i).patienttesttime.toStdString();  //测试时间
                patient_id = _pats.at(i).patientid.toStdString();      //编号
                batchNo = _pats.at(0).batchNo.toStdString();           //批次编号
                barcode = _pats.at(i).barcodeData.toStdString();       //扫码数据

                QString img_path = UtilApp::getPreviewImgPath(_pats.at(i));
                if (QFile::exists(img_path)) {
                    IplImage *img_preview = CAlgoInvoker::readAndEqualizeHistFromFileToIplImage(img_path);
                    if (!img_preview || !img_preview->imageData) {
                        logCritical(__PRETTY_FUNCTION__ + QString(": upload image failed! failed to load image \"%1\"").arg(img_path), CGlobal::LOG_DATATRANS);
                        continue;
                    }
                    qDebug() << "load image";
                    if (!DataTransmiter::SendMeasureImage(msg, patient_id, testtime, batchNo, barcode, *img_preview)) {
                        qWarning() << "upload image failed:" << QString::fromStdString(patient_id) << ",log:" << QString::fromStdString(msg);
                    } else {
                        qDebug() << "upload image success:" << img_path;
                    }

                    if (img_preview != NULL)
                    {
                        cvReleaseImage(&img_preview);
                    }
                } else {
                    qWarning() << "upload image failed! file not found:" << img_path;
                }
            }
        }
    }

    //
    return true;    // TODO: 逻辑完善
}

bool UpLoadThread::sendThroughGuanXin(const std::vector<CPatient> &_pats, QVector<int> &_list_succ, QString &_err_msg)
{
    //
    bool succ_all = true;
    QVector<int> fail_list;

    // 新疆冠新接口
    CDataIntfGuanXin data_intf(networkManager(), nullptr);
    stGuanXinIntfCfg cfg;
    WinDataTrans::getGuanXinIntfCfg(cfg);
    data_intf.setConfig(cfg);

    for (int i = 0; i < (int)_pats.size(); i++) {
        const CPatient &pat = _pats.at(i);

        bool has_right, has_left;
        Result::judgeSingleDualEyeMode(pat, &has_right, &has_left);
        stVisionJudgementRst right_comp, left_comp;
        Result::getVisionJudgementRst(pat, has_right, has_left, right_comp, left_comp);
        QString desc = Result::getVisionJudgementDesc(pat, has_right, has_left, right_comp, left_comp).toStr(false);

        Entity::EResultRequest result_request;
        result_request.testno = pat.patientid;

        if (has_right && has_left) {
            result_request.addResultItem(CResultItemNames::TongJu           , pat.patientpd             );
        }

        if (has_left) {
            result_request.addResultItem(CResultItemNames::Ltongkongdaxiao  , pat.patientleftpd         );
            result_request.addResultItem(CResultItemNames::Laxialview       , pat.patientlefteyeax      );
            result_request.addResultItem(CResultItemNames::LZhuJing         , pat.patientlefteyecyl     );
            result_request.addResultItem(CResultItemNames::LYanQiuJing      , pat.patientlefteyesph     );
        }

        if (has_right) {
            result_request.addResultItem(CResultItemNames::Rtongkongdaxiao  , pat.patientrightpd        );
            result_request.addResultItem(CResultItemNames::Raxialview       , pat.patientrighteyeax     );
            result_request.addResultItem(CResultItemNames::RZhuJing         , pat.patientrighteyecyl    );
            result_request.addResultItem(CResultItemNames::RYanQiuJing      , pat.patientrighteyesph    );
        }

        {
            result_request.addResultItem(CResultItemNames::ChuShaiZhenDuan  , desc                      );
        }

        Entity::EResultResponse result_response;
        bool succ_curr = data_intf.uploadResult(pat.comment1, result_request, result_response, _err_msg);
        if (!succ_curr) {
            fail_list.append(i);
            succ_all = false;
        }
    }

    for (int i = fail_list.size() - 1; i >= 0; i--) {
        const int &idx = fail_list.at(i);
        _list_succ.removeAt(idx);
    }

    // 上传结束状态设置
    setIsUpload(_list_succ, true);

    //
    return succ_all;
}

void UpLoadThread::verifyAuth()
{

    std::string log;
    bool ret = DataTransmiter::GetAuthToken(log);
    if(ret)
    {
        qDebug() << "GetAuth test sucess!";
        emit testFeedback(0, true, QString::fromStdString(log));
    }
    else
    {
        qDebug() << "GetAuth test failed:" << QString::fromStdString(log);
        emit testFeedback(0, false, QString::fromStdString(log));
    }

}

void UpLoadThread::setIsUpload(QVector<int> list, bool state)
{
    MySQLitePatients *mysql = MySQLitePatients::getInstance();
    std::vector<CPatient> mypats;
    std::vector<CPatient> modify;
    modify.clear();

    mypats.clear();
    mypats = mysql->findRecordByIdList(list);

    for (size_t i = 0; i < mypats.size(); i++)
    {
        (mypats.at(i)).isNeedUpload = true;
        (mypats.at(i)).isUploaded = state;
        modify.push_back(mypats.at(i));
        qDebug() << (mypats.at(i)).patientid << "set isupload:" << (mypats.at(i)).isUploaded;
    }

    mysql->TableModify(modify);
}

//
bool UpLoadThread::checkNetwork()
{
    return g_WifiIntf->getIsConnected();
}

//
void UpLoadThread::slotQueryPatientInfo(QString _num)
{
    DataTrans::Client client;
    client.Clear();

    if (connMode_Http == DataTransmiter::ConnMode) {
        if (DataTransmiter::ReceiverAddr.length() < 3)
        {
            emit requestClientInfoFeedback(false, client, tr("Interface path not valid!"));     // "接口路径无效！"
            return;
        }
    }

    qDebug() << "UpLoadThread::slotQueryPatientInfo = "/*<<QString::fromStdString(PersonalInfos::pClient.Num)*/;
    QString err_msg;
    Clients clients;
    if(DataTransmiter::GetClientInfo(err_msg, _num.toStdString(), clients))
    {
        if(clients.Items == NULL)
        {
            qDebug() << "get clientInfo error!";
            emit requestClientInfoFeedback(false, client, err_msg);
            return;
        }
        else
        {
            qDebug() << "clients.Items!=NULL";
            qDebug() << "get clientInfo num:" << QString::fromStdString(clients.Items[0].Num);
        }
        client.FromObj(clients.Items[0]);
        qDebug() << "get client info success!,age=" << client.BirthDate.c_str();
        emit requestClientInfoFeedback(true, client, err_msg);
    }
    else
    {
        qDebug() << "get client info failed:" << err_msg;
        emit requestClientInfoFeedback(false, client, err_msg);
    }
}

void UpLoadThread::clientListRequest()
{
    qDebug() << "--UpLoadThread::clientListRequest()";
    if (connMode_Http == DataTransmiter::ConnMode)
    {
        if(DataTransmiter::ReceiverAddr.length() < 3)
        {
            QString log = tr("请设置接收端IP!");  // "Please set reciecer IP!"
            emit requestClientListFeedback(log);

            return;
        }
        if(DataTransmiter::PathClientList.length() < 1)
        {
            QString log = tr("请设置批量接口路径!"); // "Please set clientlist path!"
            emit requestClientListFeedback(log);

            return;
        }
    }

    QString log, batNo = "";
    if (DataTransmiter::GetClientListInfo(log, batNo))
    {
        qDebug() << "emit requestClientListFeedback:succ";
        emit requestClientListFeedback("succ");

    }
    else
    {
        qDebug() << "emit requestClientListFeedback:fail";
        emit requestClientListFeedback(log);
    }
}

/************************* 2020.10.12增  tao *************************/
string UpLoadThread::imageBase64Encode(QString _img_path)              // TODO: 这个不应该定义在这里，应该放到数据传输模块
{
    string imgString = "";
    QFile img(_img_path);
    if (!img.exists()) {
        qDebug() << "img do not existed!" << _img_path;
        return imgString;
    } else {
        QImage picJpg(_img_path);
        QByteArray ba;
        QBuffer buf(&ba);
        picJpg.save(&buf, "JPG", 50);
        QByteArray pic = ba.toBase64();
        qDebug() << "image Base64 size:" << pic.size();
        return pic.toStdString();
    }
}

void UpLoadThread::setBtConnection(CBtConnection *_bt_conn)
{
    logDebug(QString::asprintf("UpLoadThread::setBtConnection(): ThreadId = %lld", (qintptr)QThread::currentThreadId()), CGlobal::LOG_BLUETOOTH);

    btConnection = _bt_conn;

    /* 这里不使用队列连接方式，因为目前的代码，若用队列连接，发送和接收处理过程将工作于同一个线程。
     * 因为蓝牙传输要实现“请求-应答”模式通信过程，见 DataTransmiter::sendBtRequest()。
     * 请求发起后当前线程将处于轮询/阻塞等待应答数据的状态，因此蓝牙的数据接收处理过程不可与通信流程的发起代码工作在同一线程。
     */
    QObject::connect(btConnection, &CBtConnection::sigReceivedData, this, &UpLoadThread::slot_btConnection_ReceivedData, Qt::DirectConnection);

    DataTrans::DataTransmiter::BtConn = btConnection;
}

void UpLoadThread::setSerialDatatrans(CSerialDatatrans *_serial_datatrans)
{
    serialDatatrans = _serial_datatrans;
    QObject::connect(serialDatatrans, &CSerialDatatrans::sigReceivedData, this, &UpLoadThread::slot_btConnection_ReceivedData, Qt::DirectConnection);
    DataTrans::DataTransmiter::serialDatatrans = _serial_datatrans;
}

// 发送【设备运行状态】到外部对接系统（旧的外部控制协议，打算弃用）
void UpLoadThread::sendRunStat(const std::string _stat)
{
    if (connMode_Bluetooth == DataTransmiter::ConnMode) {
        if (!g_uploadThread->btConnection->getIsConnected()) {
            logWarning("bluetooth not connected, cancel sending RunStat!", CGlobal::LOG_DATATRANS);
            return;
        }
    }

    //
    MLMCommunic stat_data;
    stat_data.func = DataTrans::FUNC_RUN_STAT;
    stat_data.data = _stat;
    std::string json = "";
    stat_data.ToJson(json);

    g_uploadThread->doSendBlueToothData(json);
}

// 发送【JSON数据包】到外部对接系统（旧的外部控制协议，打算弃用）
void UpLoadThread::doSendBlueToothData(std::string _json)
{
    QByteArray data = QString::fromStdString(_json).toLatin1();

    int len = data.length();
    //qDebug()<<"get bt json len:"<<len;

    QByteArray lenStr = QByteArray::number(len,10);
    if(lenStr.length() < 8){
        int i = 8 - lenStr.length();
        for(int k=0;k < i;k++){
            lenStr.prepend("0");
        }
    }
    QByteArray bt_data = "";
    bt_data.append("@@");
    bt_data.append(lenStr);
    bt_data.append(data);
    bt_data.append("##");
    //qDebug()<<"total bt_data length:"<<bt_data.length();
    //qDebug()<<"final bt_data :"<<bt_data;   //

    bt_data.append("\r\n");

    //emit writeBlueToothData(bt_data);
    if (connMode_Bluetooth == DataTransmiter::ConnMode) {
        btConnection->pushSendingData(bt_data);
    } else if (connMode_UsbUart == DataTransmiter::ConnMode || connMode_Uart == DataTransmiter::ConnMode) {
        serialDatatrans->writeData(bt_data, true);      // TODO: 这里若没 true 参数（不等待发送完成），则发送的数据可能堵塞？待验证……
    } else {
        logCritical("UpLoadThread::doSendBlueToothData(): ConnMode not supported!", CGlobal::LOG_DATATRANS);
    }
}

bool UpLoadThread::checkIsConnected(QString &_err_msg)
{
    _err_msg.clear();

    if (connMode_Http == DataTransmiter::ConnMode) {
        if (g_WifiIntf->getIsConnected()) {
            return true;
        } else {
            _err_msg = tr("WiFi 未连接！");    // ""
            return false;
        }
    } else if (connMode_Bluetooth == DataTransmiter::ConnMode) {
        if (btConnection->getIsConnected()) {
            return true;
        } else {
            _err_msg = tr("蓝牙数据传输未连接！");    // ""
            return false;
        }
    } else if (connMode_UsbUart == DataTransmiter::ConnMode || connMode_Uart == DataTransmiter::ConnMode) {
        if (serialDatatrans->isOpened()) {
            return true;
        } else {
            //_err_msg = tr("USB-UART and UART connection mode not supported!");    // "USB-UART and UART connection mode not supported!"
            _err_msg = tr("USB-UART 或 UART 未连接！");      // ""
            return false;
        }
    } else {
        _err_msg = tr("数据接口的连接方式配置错误！");    // ""
        return false;
    }
}

// 通过蓝牙方式发送测量结果
void UpLoadThread::sendBlueToothMeasureResults(std::vector<CPatient> _patients)
{
    string data_json_str;

    MeasureResults results(_patients.size());
    DataTransmiter::DataVectorToObj(_patients,results);
    results.ToJson(data_json_str);

    MLMCommunic communic;
    communic.func = FUNC_NEW;
    communic.data = data_json_str;
    communic.version = PROTOCOL_VERSION;

    string json_communic_str = "";
    communic.ToJson(json_communic_str);

    QTime _time = QTime::currentTime().addMSecs(500);
    while(QTime::currentTime() < _time){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 500);
    }
    qDebug()<<"--delay finished";

    doSendBlueToothData(json_communic_str);
}

// 蓝牙数据接收事件（数据对接功能）
/***
 * 注意：因为蓝牙的通信要实现“请求-应答”模式的通信，所以该模式通信的发起、等待代码不能与蓝牙的数据接收处理过程工作在同一线程。
 *      参见 UpLoadThread::setBtConnection() 的信号槽连接的注释。
 * 目前（v1.3.12.4），通信发起、等待过程的代码工作在“uploadthread”模块的工作线程，而数据接收处理过程工作在蓝牙模块的接收槽函数的工作线程。
 */
// TODO: 在数据传输模块的内部确保发送和接收工作于不同线程，不依赖外部逻辑和机构
void UpLoadThread::slot_btConnection_ReceivedData(QByteArray _data)
{
    static QByteArray buffer = "";
    const int MAX_BUFFER_SIZE = 256 * 1024;    // 缓冲区最大长度（暂定 256k bytes）

    logDebug(QString::asprintf("UpLoadThread::slot_btConnection_ReceivedData(): ThreadId = %lld", (qintptr)QThread::currentThreadId()), CGlobal::LOG_BLUETOOTH);

    // 存到缓冲区
    buffer.append(_data);

    // 数据分包
    if (CGlobal::getIsExternalControl()) {          // 原有的定制的北京九辰协议
        splitPackages_JiuChen(buffer);
    } else {                            // 通用的标准协议
        splitPackages(buffer);
    }

    // 缓冲区长度检查，防止异常导致占用内存过多
    if (buffer.length() > MAX_BUFFER_SIZE) {
        logCritical("UpLoadThread::slot_btConnection_ReceivedData(): buffer too long! data abnormal?", CGlobal::LOG_BLUETOOTH);

        buffer.clear();
    }
}

// 数据分包（北京九辰版协议）：从数据接收缓冲区中识别出数据包，并调用解包函数做后续处理
void UpLoadThread::splitPackages_JiuChen(QByteArray &_buffer)
{
    bool found_pkg;
    bool checked_pkg;
    int begin;
    int end;
    QByteArray pkg;

    do {
        found_pkg = false;
        checked_pkg = false;
        pkg = "";
        begin = _buffer.indexOf("@@", 0);
        end = -1;

        if(begin >= 0)
        {
            end = _buffer.indexOf("##", begin);

            found_pkg = (end >= 0);
        } else {
            found_pkg = false;
        }

        if (found_pkg) {
            pkg = _buffer.mid(begin, end + 2 - begin);

            // 确认数据包格式（粗略检查）
            QRegExp re("@@[0-9]{8}\\{[\\s\\S]+\\}##");
            checked_pkg = (re.indexIn(pkg) >= 0);
        }

        if (checked_pkg) {
            _buffer.remove(0, end + 2);      // 从缓冲区中移除已检出的数据包（丢弃了 begin 前面的数据）

            unpackAndProcessPkg_JiuChen(pkg);
        }
    } while (found_pkg);
}

// 数据分包（通用，蓝牙通信）
void UpLoadThread::splitPackages(QByteArray &_buffer)
{
    const char *PKG_HEAD = "\r\n\r\n{";
    const char *PKG_TAIL = "}\r\n\r\n";

    const char *PKG_HEAD_1 = "\n\n{";
    const char *PKG_TAIL_1 = "}\n\n";

    // 同时支持 unix 风格和 Windows 风格的换行符，即搜索替换，"\n\n{" -> "\r\n\r\n{"，"\n\n}" -> "\r\n\r\n}"
    _buffer.replace(QByteArray(PKG_HEAD_1), QByteArray(PKG_HEAD));      // TODO: 应该反过来？
    _buffer.replace(QByteArray(PKG_TAIL_1), QByteArray(PKG_TAIL));

    //
    bool found_pkg;
    bool checked_pkg;
    int begin;
    int end;
    QByteArray pkg;

    do {
        found_pkg = false;
        checked_pkg = false;
        pkg = "";
        begin = _buffer.indexOf(PKG_HEAD, 0);
        end = -1;

        if(begin >= 0)
        {
            end = _buffer.indexOf(PKG_TAIL, begin);

            found_pkg = (end >= 0);
        } else {
            found_pkg = false;
        }

        if (found_pkg) {
            pkg = _buffer.mid(begin, end + 5 - begin);

            // 确认数据包格式（粗略检查）
            QRegExp reg_exp("\r{0,1}\n\r{0,1}\n\\{[\\s\\S]+\\}\r{0,1}\n\r{0,1}\n");         // TODO: 这个有必要吗？或者只用这种方法即可？
            checked_pkg = (reg_exp.indexIn(pkg) >= 0);
        }

        if (checked_pkg) {
            _buffer.remove(0, end + 5);      // 从缓冲区中移除已检出的数据包（丢弃了 begin 前面的数据）

            unpackAndProcessPkg(pkg);
        } else {
            if (found_pkg) {
                logWarning("UpLoadThread::splitPackages(): Package found but RegExp failed!", CGlobal::LOG_DATATRANS);
                found_pkg = false;
            }
        }
    } while (found_pkg);
}

// 解包和处理蓝牙通信数据包（北京九辰版协议）
bool UpLoadThread::unpackAndProcessPkg_JiuChen(QByteArray &_data)
{
    if (!CGlobal::getIsExternalControl()) {
        logCritical("UpLoadThread::unpackAndProcessPkg_JiuChen(): Received JiuChen data package but not JiuChen version!", CGlobal::LOG_DATATRANS);
        return true;
    }

    //
    if(_data.startsWith("@@") && _data.endsWith("##")) {
        if(_data.size() < 12){
            qDebug()<<"btData size() < 12,return";
            //to do error feedback
            return false;
        }

        int len = _data.mid(2,8).toInt();
        if(len < (_data.size()-12)){
            qDebug()<<"jason len<=0,return";
            //to do error feedback
            return false;
        }
        //get json data
        _data.remove(0,10);
        std::string json_data = _data.mid(0,len).toStdString();

        MLMCommunic bt_data;
        bool ret = bt_data.FromJson(json_data);
        if(!ret){
            qDebug()<<"get json failed";
            return false;
        }

        qDebug()<<"func = "<< QString::fromStdString(bt_data.func);           
        if(bt_data.func == FUNC_START){
            Client cInfo;
            cInfo.FromJson(bt_data.data);
            QString num = QString::fromStdString(cInfo.Num);

            int age_range = QString::fromStdString(cInfo.BirthDate).toInt();
            qDebug()<<"num:"<<num<<",age:"<<age_range;
            if(age_range > 5)       // “开始测量”指令中的年龄段，值范围是 1~5，而不是 0～4
                age_range = 5;
            if(age_range < 1)
                age_range = 1;

            age_range -= 1;              // 对方系统的年龄段，从 1 开始，而我方系统的年龄段，从 0 开始

            //set test info
            if(num=="")
                num = "NULL";

            CPatient patient;
            patient.patientid = num;
            patient.setAgeRange((enAgeRange)age_range);
            g_WinMeasure->setPatient(patient);

            bt_data.stat = "succ";
            string feedbackJson = "";
            bt_data.ToJson(feedbackJson);
            doSendBlueToothData(feedbackJson);
            emit cameraCtl(FUNC_START);
        }
        else if(bt_data.func == FUNC_STOP){
            bt_data.stat = "succ";
            string feedbackJson = "";

            bt_data.ToJson(feedbackJson);
            doSendBlueToothData(feedbackJson);
            emit cameraCtl(FUNC_STOP);
        }
        else if(bt_data.func == FUNC_GRAB_FRAME)
        {
            bt_data.stat = "succ";
            string feedbackJson = "";
            bt_data.ToJson(feedbackJson);
            doSendBlueToothData(feedbackJson);
            emit cameraCtl(FUNC_GRAB_FRAME);
        }
        else if(bt_data.func == FUNC_DEV_STAT){
            string dev_stat;
            WinMeasure::getDevStat(dev_stat);
            qDebug()<<"dev_stat="<<QString::fromStdString(dev_stat);
            bt_data.data = dev_stat;
            bt_data.stat = STAT_SUCC;

            string json = "";
            bt_data.ToJson(json);
            doSendBlueToothData(json);
        }
        else if(bt_data.func == FUNC_RUN_STAT){
            string dev_stat;
            string run_stat;
            WinMeasure::getDevStat(dev_stat);
            if(dev_stat == BUSY)
                WinMeasure::getRunStat(run_stat);
            else
                run_stat = NOTRUNNING;

            qDebug()<<"get run_stat="<<QString::fromStdString(run_stat);
            bt_data.data = run_stat;
            bt_data.stat = STAT_SUCC;

            string json = "";
            bt_data.ToJson(json);
            doSendBlueToothData(json);
        }
        else if(bt_data.func == FUNC_DEL){
            qDebug()<<"fun:delete";
            bool ret = MySQLitePatients::delAllData();

            bt_data.stat = ret ? STAT_SUCC : STAT_FAIL;

            string json = "";
            bt_data.ToJson(json);
            doSendBlueToothData(json);
        }
        else if(bt_data.func == FUNC_POWER_OFF) {
            bt_data.stat = "succ";
            string feedbackJson = "";
            bt_data.ToJson(feedbackJson);
            doSendBlueToothData(feedbackJson);
            emit cameraCtl(FUNC_POWER_OFF);
        }
        else {
            logCritical(QString::asprintf("UpLoadThread::unpachAndProcessBtPkg(): func '%s' is not supported!", bt_data.func.c_str()), CGlobal::LOG_BLUETOOTH);
        }

        return true;
    } else {
        //
        logCritical(QString("UpLoadThread::unpachAndProcessBtPkg(): data format error, _data = ") + _data, CGlobal::LOG_BLUETOOTH);
        return false;
    }
}

// 解包和包数据处理（通用的蓝牙数据接口）
bool UpLoadThread::unpackAndProcessPkg(QByteArray &_data)
{
    if (CGlobal::getIsExternalControl()) {
        logCritical("UpLoadThread::unpackAndProcessPkg(): received non JiuChen data package but is JiuChen version!", CGlobal::LOG_DATATRANS);
        return true;
    }
    logDebug(QString(__PRETTY_FUNCTION__) + ": received data pkg : \n" + _data, CGlobal::LOG_BLUETOOTH);

    // 蓝牙数据接口的蓝牙连接收到数据包后，交由数据通信模块处理
    QString msg;
    bool succ = DataTrans::DataTransmiter::processReceivedDataPkg(msg, _data);
    if (succ) {
        //
    } else {
        logWarning(QString("UpLoadThread::unpackAndProcessPkg(): process failed! msg = ") + msg, CGlobal::LOG_DATATRANS);
    }
    return succ;
}

void UpLoadThread::callbackDataTransmiterGetNewSubject(DataTrans::Client &_client)
{
    // 发送信号
    emit g_uploadThread->sigDataTransmiterGetNewSubject(_client);
}

void UpLoadThread::callbackDataTransmiterOperationLocked(bool _locked, QString _msg)
{
    // 发送信号
    emit g_uploadThread->sigDataTransmiterOperationLocked(_locked, _msg);
}

void UpLoadThread::handleBlueToothCmd(QString _data)       // 处理本应用内其它模块（抓图模块）发来的蓝牙指令
{
    qDebug()<<"handleBlueToothCmd:" << _data;

    MLMCommunic bt_data;
    if(_data.toStdString() == MEASURE_SUCC || _data.toStdString() == MEASURE_FAIL){
        bt_data.func = FUNC_RUN_STAT;
        bt_data.data = _data.toStdString();
        string json = "";
        bt_data.ToJson(json);
        doSendBlueToothData(json);
    }
    else if(_data.startsWith(QString::fromStdString(FUNC_DEV_STAT))){
        string dev_stat;
        WinMeasure::getDevStat(dev_stat);
        qDebug()<<"query feedback dev_stat="<<QString::fromStdString(dev_stat);

        bt_data.func = _data.toStdString();
        bt_data.stat = "succ";
        bt_data.data = dev_stat;
        string json = "";
        bt_data.ToJson(json);
        doSendBlueToothData(json);
    }
    else if(_data.startsWith(QString::fromStdString(FUNC_RUN_STAT))){
        string run_stat;
        WinMeasure::getRunStat(run_stat);
        qDebug()<<"query feedback run_stat="<<QString::fromStdString(run_stat);

        bt_data.func = _data.toStdString();
        bt_data.stat = "succ";
        bt_data.data = run_stat;
        string json = "";
        bt_data.ToJson(json);
        doSendBlueToothData(json);
    }
    else if(_data.startsWith(QString::fromStdString(FUNC_DISTANCE))){
        string distance = _data.section(':',1,1).toStdString();
        cout <<"-----------send distance:"<<distance<<"\n"<<endl;

        bt_data.func = FUNC_DISTANCE;
        bt_data.stat = "";
        bt_data.data = distance;
        string json = "";
        bt_data.ToJson(json);
        doSendBlueToothData(json);
    }

    qDebug()<<"send FUNC:"<<QString::fromStdString(bt_data.func)<<",STAT:"<<QString::fromStdString(bt_data.data);
}
