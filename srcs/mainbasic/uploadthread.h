#ifndef UPLOADTHREAD_H
#define UPLOADTHREAD_H

#include "DataTransmit.h"

#include <QObject>

#include <vector>
#include <string>

#include "bluetoothintf.h"
#include "serialdatatrans.h"

//
class UpLoadThread : public QObject
{
    Q_OBJECT
public:
    explicit UpLoadThread(QObject *parent = 0);
    ~UpLoadThread();

    static void sendRunStat(const std::string _stat);

    void setIsUpload(QVector<int>, bool);
    bool checkNetwork();
    static std::string imageBase64Encode(QString _img_path);
    void setBtConnection(CBtConnection *_bt_conn);
    void setSerialDatatrans(CSerialDatatrans *_serial_datatrans);

    void doSendBlueToothData(std::string);

    bool checkIsConnected(QString &_err_msg);       // 检查数据传输的连接是否已就绪

protected:
    CBtConnection *btConnection = Q_NULLPTR;
    CSerialDatatrans *serialDatatrans = Q_NULLPTR;

    void sendBlueToothMeasureResults(std::vector<CPatient> Pats);

    void splitPackages_JiuChen(QByteArray &_buffer);            // 九辰版的通信协议分包
    bool unpackAndProcessPkg_JiuChen(QByteArray &_data);        // 九辰版的通信协议解包和处理
    void splitPackages(QByteArray &_buffer);
    bool unpackAndProcessPkg(QByteArray &_data);
    static void callbackDataTransmiterGetNewSubject(DataTrans::Client &_client);        // DataTransmiter 收到开启测量指令后的回调（因为 DataTransmiter 未支持 Qt 信号槽，所以提供其回调，然后在此发送信号）
    static void callbackDataTransmiterOperationLocked(bool _locked, QString _msg);      // DataTransmiter 收到操作锁定之后的回调

    bool sendData(std::vector<CPatient> &_pats, QVector<int> &_list_succ, QVector<int> &_list_fail, QString &_msg);     // 发送数据
    bool sendImage(std::vector<CPatient> &_pats);                                                                       // 发送图像
    bool sendThroughGuanXin(const std::vector<CPatient> &_pats, QVector<int> &_list_succ, QString &_err_msg);           // 通过新疆冠新接口上传数据

signals:
    /**
     * @brief 结果上传的反馈
     * @param _count_upload         用户选定的上传记录数
     * @param _count_upload_final   真正上传的记录数，0 表示因出错而没有真正执行上传
     * @param _count_succ           上传成功记录数
     * @param _msg                  错误消息
     */
    void sigUpLoadDataFeedback(int _count_upload, int _count_upload_final, int _count_succ, QString _msg);              // TODO: 这个信号有多个侦听者，增加调用者参数？
    void testFeedback(int _test_type, bool _is_succ, QString _msg);       // 参数 _test_type：0-鉴权测试，1-数据测试
    void requestClientInfoFeedback(bool _is_succ, DataTrans::Client _client, QString _err_msg);   // 【查询被测者信息的应答数据】信号
    void requestClientListFeedback(QString log);
    //2020.10.12  tao
    void cameraCtl(const std::string cmd);
    // void writeBlueToothData(QByteArray data);
    void sigDataTransmiterGetNewSubject(DataTrans::Client _client);         // 【DataTransmiter 收到开启测量指令】信号
    void sigDataTransmiterOperationLocked(bool _locked, QString _msg);      // 【DataTransmiter 收到操作锁定】信号

public slots:
    void slotUploadData(QVector<int> _list_upload_ids);         //上传结果
    void verifyAuth();                          //鉴权，获取token
    void slotQueryPatientInfo(QString _num);                   //查询受测者信息
    void clientListRequest();                   //批量查询受测者信息
    void handleBlueToothCmd(QString _data);          //2020.10.12  tao
    void slot_btConnection_ReceivedData(QByteArray _data);

};

#endif // UPLOADTHREAD_H
