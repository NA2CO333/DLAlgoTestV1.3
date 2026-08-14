#ifndef DATATRANSMIT_H
#define DATATRANSMIT_H

#include <QVector>

#include <iostream>
#include <string>
#include <functional>
#include <ctime>
#include <vector>

#include "mongoose-6.7/mongoose.h"
#include "opencv2/core/core_c.h"
#include "opencv2/core/core.hpp"
#include "rapidjson-1.1.0/document.h"
#include "rapidjson-1.1.0/writer.h"
#include "rapidjson-1.1.0/stringbuffer.h"

#include "mysqlitepatients.h"
#include "bluetoothintf.h"
#include "serialdatatrans.h"

//
//using std::string;
//using std::vector;

//
namespace DataTrans
{
    // 通信数据类
    class MLMCommunic
    {
    public:
        MLMCommunic();

        //std::string  module;    // 模块码；【必有】
        std::string  func;      // 功能码；【必有】
        std::string  stat;      // 状态码
        std::string  data;      // 数据JSON字符串（不同的模块名对应不同的数据类）
        std::string  check;     // data 字符串的 MD5 值
        std::string  msg;       // 文本描述
        std::string  version;   // 数据格式/协议版本号
        std::string  stamp;     // 序列号
        std::string  lang;      // 语言代码（遵循 BCP 47（RFC 5646）规范。基本语言需求："zh-CN": 中国大陆简体中文，"en-US": 美国英语。为了兼容历史版本，缺省时为中文）

        std::string softwareVersion;    // 软件版本（仅对接万灵云端时存在）
        std::string hardwareVersion;    // 硬件版本（仅对接万灵云端时存在）

        void ToJson(std::string& _json_str);
        bool FromJson(std::string& _json_str);
    protected:
        void Clear();
    };

    const std::string PROTOCOL_VERSION  = "1.2.9";      // 协议版本

    const std::string LANGUAGE_CHINESE  = "zh-CN";      // 语言代码 - 中国大陆简体中文
    const std::string LANGUAGE_ENGLISH  = "en-US";      // 语言代码 - 美国英语

    //// 模块码
    //const std::string MODULE_MEASURE = "SightScreen.MeasureData";    // 筛查结果数据
    //const std::string MODULE_CLIENT  = "SightScreen.Client";         // 受测者身份信息
    //const std::string MODULE_AUTH    = "Auth";                       // 授权

    // 功能码
    const std::string FUNC_QUERY                = "query";
    const std::string FUNC_NEW                  = "new";
    const std::string FUNC_UPDATE               = "update";
    const std::string FUNC_DEL                  = "delete";
    const std::string FUNC_DISTANCE             = "distance";               // distance
    const std::string FUNC_START                = "start";                  // 进入测量状态
    const std::string FUNC_STOP                 = "stop";                   // 退出测量状态
    const std::string FUNC_GRAB_FRAME           = "grab_frame";             // 开始转灯及屈光计算
    const std::string FUNC_DEV_STAT             = "dev_stat";               // 设备状态
    const std::string FUNC_RUN_STAT             = "run_stat";               // 拍摄中状态
    const std::string FUN_DEL                   = "delete";                 // 删除数据库
    const std::string UNAVAILABLE               = "unavailable";            // 不在运行状态

    const std::string FUNC_NEW_SUBJECT          = "new_subject";            // 新增被测者指令
    const std::string FUNC_POWER_OFF            = "power_off";              // 关机
    const std::string FUNC_OPERATION_LOCKED     = "operation_locked";       // 操作锁定

    // 状态码
    const std::string STAT_SUCC  = "succ";   // 成功
    const std::string STAT_FAIL  = "fail";   // 失败

    const int LEN_NYSTAGMUS = 24;

    // 受测者身份信息      // TODO: 类名改为“Subject”？
    class Client
    {
    public:
        std::string  Num;           // 受测者编号                            /* 在视筛系统中，此字段实际意义变成了“筛查码”，见 data.h “2023-10-20 需求变更” */
        std::string  Name;          // 姓名
        std::string  Sex;           // 性别
        std::string  BirthDate;     // 出生日期（格式示例“2000-01-01”）
        std::string  Class;         // 班级       -> （2023-10-26）若是门诊档案，则为“籍贯”
        std::string  Tel;           // 电话
        std::string  WeChat;        // 微信号      -> （2023-10-31）若是门诊档案，则为“民族”
        std::string  Address;       // 地址
        std::string  comment1;      // 外部id
        std::string  Comment2;      // 备用2

        virtual void Clear();
        void ToJson(std::string& _json_str);
        void ToJson(rapidjson::Writer<rapidjson::StringBuffer> *_writer);
        //void CloneFrom(Client& _client);
        virtual bool FromJson(const std::string& _json_str);
        void FromJson(rapidjson::Value & _document);
        void FromObj(Client &_client);
    };
    // 多个受测者身份信息        // TODO: 类名改为“Subjects”？
    class Clients
    {
    public:
        Clients();
        Clients(int _len);
        ~Clients();
        Client *Items;     // 受测者身份信息数组
        static Client *mItems;     // 受测者身份信息数组
        static int mFlen;

        int Len();
        bool FromJson(std::string& _json_str);    // TODO: 用运算符重载等方式简化
        static bool FromJsons(std::string& _json_str);    // TODO: 用运算符重载等方式简化
        void Clear();

    protected:
        int FLen;
        void Init(int _len);
    };

    // 筛查任务受测者
    class ClientOfBatch : public Client
    {
    public:
        std::string BatchNo;        // 批次编号（任务编号）  // 2023-10-26 如果是万灵云端，这个字段是“诊疗号”
        std::string ClinicNo;       // 诊疗号（每个患者一个号，不重复） /* 2023-10-26 本来“诊疗号”应该对应旧协议中的“被测者编号”的，但是旧协议的“被测者编号”实为筛查号，因此新增此字段，与万灵云端的生测接口一样 */

        std::string Barcode;

        void Clear() override;
        enQrCodeType FromBarcode(const std::string &_str, std::string &_msg);
        void FromObj(ClientOfBatch &_client);
        void FromObj(Client &_client, bool _clear = true);

    protected:
        bool FromJson(const std::string &_json_str) override;
        bool FromCSV(const std::string& _csv_str, std::string *_msg = NULL);
    };

    // 性别码
    const std::string SEX_MALE      = "M";      // 男
    const std::string SEX_FEMALE    = "F";      // 女

    // 固视眼位超标
    class GazeExceed {
    public:
        bool VerticalGaze = false;      // 垂直凝视
        bool NasalGaze = false;         // 鼻侧凝视
        bool BitemporalGaze = false;    // 颞侧凝视
        bool GazeAsymmetry = false;     // 凝视不对称

        void ToJson(rapidjson::Writer<rapidjson::StringBuffer> *_writer);
    protected:
        bool IsExceed();    // 是否存在超标
    };

    // 单个受测者的筛查结果
    class MeasureResult
    {
    public:
        std::string DevType;        // 设备类型     // TODO: 设备型号？
        std::string DevCode;        // 设备编号
        std::string BatchNo;        // 筛查批次号
        std::string RecordID;       // 检查记录ID
        std::string AgeRange;       // 年龄段
        std::string ClientNum;      // 被测者编号        /* 在视筛系统中，此字段实际意义变成了“筛查码”，见 data.h “2023-10-20 需求变更” */
        bool        IsClinic;       // 是否门诊记录（若否，则为筛查记录）        // TODO: “门诊”的英文术语不应该用 Clinic（诊所/门诊部）而应该用 Outpatient（门诊病人/门诊服务）？
        std::string Barcode;        // 受测者二维码（如果测量前扫描受测者二维码）
        std::string Time;           // 筛查的日期时间；格式示例“2018-05-01 09:09:00”
        int         PD;             // 瞳距；单位：“mm”；【必有】
        std::string LPupil;         // 左眼瞳孔大小；单位：“mm”，保留1位小数；【必有】
        std::string LSE;            // 左眼等效球镜度；单位：“D”，保留2位小数；【必有】
        std::string LDS;            // 左眼球镜度；单位：“D”，保留2位小数；【必有】
        std::string LDC;            // 左眼柱镜度；单位：“D”，保留2位小数；【必有】                         // TODO: 这个可能是空？因为可能出异常值。
        int         LAxis;          // 左眼轴位；单位：“°”，值范围 0~180，0表示轴位为空（柱镜度为0时无轴位）
        std::string LVision;        // 左眼“参考视力”
        int         LGazeH;         // 左眼水平固视；单位“°”
        int         LGazeV;         // 左眼垂直固视；单位“°”
        bool        LPtosis;        // 左眼上睑下垂；值为“true”或“false”
        //2020.10.12  tao
        std::string LRedReflex;     // 左眼红光反射亮度
        std::string LRedReflexPer;  // 左眼红光反射百分比，单位“%”
        int         LNystagmus[LEN_NYSTAGMUS];      // 左眼眼球震颤坐标,12组坐标，x0,y0,x1,y1…

        GazeExceed  LGazeExceed;    // 左眼固视眼位超标

        std::string RPupil;         // 右眼瞳孔大小；单位：“mm”，保留1位小数；【必有】
        std::string RSE;            // 右眼等效球镜度；单位：“D”，保留2位小数；【必有】
        std::string RDS;            // 右眼球镜度；单位：“D”，保留2位小数；【必有】
        std::string RDC;            // 右眼柱镜度；单位：“D”，保留2位小数；【必有】
        int         RAxis;          // 右眼轴位；单位：“°”
        std::string RVision;        // 右眼“参考视力”
        int         RGazeH;         // 右眼水平固视；单位“°”
        int         RGazeV;         // 右眼垂直固视；单位“°”
        bool        RPtosis;        // 右眼上睑下垂
        //2020.10.12  tao
        std::string RRedReflex;     // 右眼红光反射亮度
        std::string RRedReflexPer;  // 右眼红光反射百分，单位“%”
        int         RNystagmus[LEN_NYSTAGMUS];      // 右眼眼球震颤坐标,12组坐标，x0,y0,x1,y1…

        GazeExceed  RGazeExceed;    // 右眼固视眼位超标

        bool        HasImage;       // 视筛上传的数据中是否包含测量时拍摄的图像
        Client      ClientInfo;     // 受测者信息
        std::string	Image;	        // base64 编码后的图片数据   //2020.10.12  tao
        std::string UserName;       // 用户名（即鉴权接口的账号名）

        void ToJson(std::string& _json_str);
        void ToJson(rapidjson::Writer<rapidjson::StringBuffer> *_writer);
    };

    // 日期时间格式
    const QString DATE_TIME_FORMAT = "yyyy-MM-dd HH:mm:ss";

    // 批量受测者的筛查结果
    class MeasureResults
    {
    public:
        //MeasureResult Items[];     // “单个受测者检测结果”数组

        //MeasureResults();
        MeasureResults(int _len);
        ~MeasureResults();

        MeasureResult *Items;
        int Len();

        void ToJson(std::string& _json_str);
    protected:
        int FLen = 0;
        void Init(int _len);
        void Clear();
    };

    const std::string DEVICE_TYPE   = "SSI";    // 设备类型

    // 权限验证信息
    class Auth
    {
    public:
        std::string Name = "";      // 用户名
        std::string Pwd  = "";      // 密码
        std::string Type = "";      // 账号类别，可用于支持接收端对账号进行分类
        std::string DevType;        // 设备类型
        std::string DevCode;        // 设备编号

        void ToJson(std::string& _json_str);
    };

    // Token 信息
    class Token
    {
    public:
        std::string  TokenStr;      // token字符串
        std::string  Expire;        // token到期时间戳（Unix时间，单位毫秒），0表示无超时       // TODO: 这个为什么不定义为整型？因为毫秒单位的 Unix 时间有可能需要长整型，而长整型不是标准类型？

        bool FromJson(std::string& _json_str);
    protected:
        void Clear();
    };

    // =========================================================================

    // 数据发送过程错误码
    enum enDataSendingError
    {
        dataSendingError_No = 0,                // 无错误
        dataSendingError_CannotConnect,         // 无法连接
        dataSendingError_ServerClosed,          // 接收端掉线
        dataSendingError_RequireResend,         // 接收端要求重发
        dataSendingError_AuthInvalid,           // 身份检验不通过
        dataSendingError_ResponseDataInvalid,   // 应答数据非法
        dataSendingError_FailedOrRejected,      // 服务器返回失败或拒绝
        dataSendingError_TransmissionTimeout,   // 传输超时
    };

    //// 数据对接接口设置值
    //struct IntfConfigValue
    //{

    //};

    ///***
    //筛查结果发送完成事件。
    //参数“_client_num” ：受测者编号
    //参数“_succ”       ：是否成功
    //参数“_msg”        ：若有错误，则为错误消息，否则为零长度字符串
    //*/
    //typedef void (*HdlSendReportFinished)(const std::string _client_num, const bool _succ, std::string _msg);

    /***
      类型：回调函数
      参数“_is_succ”  ：是否成功
      参数“_data”      ：相关数据，若失败，则为错误消息
    */
    typedef void(*HttpReqCallback)(enDataSendingError _err_code, std::string& _data);

    // 数据发送任务
    struct DataSendingTask
    {
        MeasureResults& Data;
        std::string& Msg;
    };

    //
    class HttpClient
    {
    public:
        static void SendReq(const std::string& url, const std::string& _extra_header,
                            const char *_body, HttpReqCallback req_callback, int _timeout, int _body_len = 0);
        static void OnHttpEvent(mg_connection *connection, int event_type, void *event_data);
        static int s_exit_flag;
        static HttpReqCallback s_req_callback;
    };

    // 类型：收到被测者信息的回调函数
    typedef void (*fCallback_GetNewSubject)(Client &_client);
    typedef void (*fCallback_OperationLocked)(bool _locked, QString _msg);

    // 通信连接方式
    enum enConnMode {
        connMode_Unknown        = -1,
        connMode_Http           = 0,
        connMode_Bluetooth,
        connMode_UsbUart,
        connMode_Uart,

        connMode_Min = connMode_Http,
        connMode_Max = connMode_Uart,
    };

    // 数据传输接口           // TODO: 不要完全静态？
    class DataTransmiter
    {
    public:
        static std::string DevCode;         // 设备编号     // NOTE: 数据上传接口的“设备编号”是独立设置的，可能与实际编号不一致（因为以前对接的客户系统接收端做了不恰当的前缀过滤）

        static std::string ReceiverAddr;    // 接收端地址
        static int ReceiverPort;            // 接收端端口

        static std::string PathData;        // 筛查结果接口路径
        static std::string PathClient;      // 受测者信息接口路径
        static std::string PathClientList;  // 受测者批量信息路径
        static std::string PathAuth;        // 授权接口路径
        static std::string PathImage;       // 图像接口路径

        static std::string AuthUserName;    // 身份验证用户名
        static std::string AuthPassword;    // 身份验证密码
        static std::string AuthUserType;    // 身份验证用户标识

        static bool IsNeedAuth;             // 接收端是否需要身份验证
        static bool IsUseHttps;             // 是否使用 Https 通信      // TODO: 兼容旧协议，暂时保留

        static enConnMode ConnMode;         // 连接方式

        static bool IsPostImmediately;      // 是否即时上传       // TODO: 这个设置逻辑上不属于这个模块
        static bool IsUploadImage;          // 是否上传图像

        static fCallback_GetNewSubject Callback_GetNewSubject;          // 收到开启测量指令后的回调     // TODO: 改用 Qt 的信号槽连接？
        static fCallback_OperationLocked Callback_OperationLocked;      // 收到操作锁定之后的回调
        static CBtConnection *BtConn;       // TODO: 因需实现请求应答模式，如何确保数据传输模块的发送和接收工作于不同线程？
        static CSerialDatatrans *serialDatatrans;

        static std::string getDevCode();    // 获取设备编号

        //static HdlSendReportFinished OnSendReportFinished; // 检测结果数据发送完成事件

        /***
         获取授权。
         参数“_msg”       ：若有错误，则为错误消息，否则为零长度字符串
         返回：是否成功
        */
        static bool GetAuthToken(std::string& _msg);

        /***
         查询受测者身份信息。
         参数“_msg”           ：错误消息
         参数“_nums”          ：受测者编号，支持逗号分隔的多个编号
         参数“_clients”       ：批量受测者对象，若执行成功，可从该对象读取服务端返回的数据
         返回：是否成功
        */
        static bool GetClientInfo(QString &_msg, std::string _nums, Clients& _clients);

        /***
         发送筛查结果。
         参数“_msg”       ：【输出参数】若有错误，则为错误消息，否则为零长度字符串
         参数"_list_fail" ：【输出参数】若失败，则失败的 id 将被存入此容器中
         参数“_patients”  ：筛查结果数组
         返回：是否成功
        */
        static bool SendMeasureData(QString &_msg, QVector<int> &_fail_ids, std::vector<CPatient>& _patients);

        /***
        get client list info
        参数“_msg”           ：错误消息
        参数“_batchNo”       : 批次号
        */
        static bool GetClientListInfo(QString &_msg, QString _batchNo = "");

        /***
         发送筛查图像。
         参数“_msg”       ：若有错误，则为错误消息，否则为零长度字符串
         参数“_client_num”：受测者编号（字段名"ClientNum"）
         参数“_test_time” ：测量时间（字段名"TestTime"）（格式"yyyy-MM-dd hh:mm:ss"，指 4 位 year，2 位 month，2 位 day，2 位 hour，2 位 minute，2 位 second，位数不足时前面补0）
         参数“_batch_no”  ：批次号（字段名"BatchNo"）
         参数“_barcode”   ：受测者二维码（字段名"Barcode"）
         参数“_image”     ：测量时所拍摄图像（字段名"File"）
         返回：是否成功
        */
        static bool SendMeasureImage(std::string& _msg, const std::string& _client_num, const std::string & _test_time,
            const std::string& _batch_no, const std::string& _barcode, const IplImage& _image);

        /***
         解析数据对接接口设置CSV文本，并设置到自身属性。
         参数“_msg”       ：若有错误，则为错误消息，否则为零长度字符串
         参数“_csv_str”   ：CSV文本
         //参数“_conf_value”：解析得到的设置值
         返回：是否成功
        */
        static bool SetFromCSV(std::string& _msg, const std::string& _csv_str/*,
            IntfConfigValue& _conf_value*/);

        /***
         * 处理接收到的通信数据包。
         *
         */
        static bool processReceivedDataPkg(QString &_msg, QByteArray &_data);

        // Vector 里的检测结果数据赋值到 MeasureResults 对象（内部类转接口类）
        static void DataVectorToObj(std::vector<CPatient>& _vector, MeasureResults& _results);

    protected:
        static std::string FTokenStr;   // token 字符串
        static time_t FTokenExpire;     // token 超时时间（格林威治时间）

        static std::string FProcMsg;
        static bool FProcResult;
        static bool FNeedResend;
        static Clients *FClients;
        static QVector<int> *FListFail;

        static bool mIsWaitingBtResp;
        static HttpReqCallback mRequestCallback;        // TODO: 如果多线程同时调用，可能造成数据混乱？

        static char* GetUrl(char* _url, int _len, std::string _path, bool _add_token = true);

        static void CallbackData(enDataSendingError _err_code, std::string& _data);
        static void CallbackAuth(enDataSendingError _err_code, std::string& _data);
        static void CallbackClient(enDataSendingError _err_code, std::string& _data);
        static void CallbackClientList(enDataSendingError _err_code, std::string& _data);
        static void CallbackImage(enDataSendingError _err_code, std::string& _data);

        static void GetAuthJson(std::string& _json_str);
        static bool DoSendMeasureData(DataSendingTask& _task);
        static bool CheckAndGetAuth(std::string& _str_msg);
        static void Clear();

        static void Callback_BtRequest(QByteArray &_resp_data);

        static bool sendBtRequest(std::string &_json_str, HttpReqCallback _request_callback, int _timeout_sec);

    };

    //string TrimRN(string _str);
    //bool CSVGetNextField(const string& _csv_str, int& _prior_start, int& _prior_end, string& _val_str);
    //int StrToInt(string _str);
    //long StrToLong(string _str);
    //double StrToFloat(string _str);
    //bool StrToBool(string _str);
    //void IntToStr(int& _i, string& _s);
    //char* url_encode(const char* s, int len, int* new_length);

    void client2Patient(Client &_client, CPatient &_patient);
    void batchClient2Patient(ClientOfBatch &_batch_client, CPatient &_patient);

}

#endif // DATATRANSMIT_H
