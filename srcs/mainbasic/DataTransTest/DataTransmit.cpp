
// TODO: 没有独占锁，全局数据可能错乱？
// TODO: 超时断线（Linux）；字符串内存空间；字符转码处理；使用反射序列化反序列化；

#ifndef _DEBUGGING
//#define _DEBUGGING
#endif // !_DEBUGGING

#include "DataTransmit.h"

#include <stdio.h>
#include <ctime>
#include <exception>
#include <sstream>
//#include <locale>
//#include <codecvt>

#include <QDebug>
#include <QDate>
#include <QTextCodec>
#include <QThread>
#include <QCoreApplication>

//#include "guid.hpp"
//#include "opencv2\core\core.hpp"
//#include "opencv2\highgui\highgui.hpp"
#include "opencv2/highgui/highgui.hpp"

//#include "encodings.h"
// RapidJson 内置的转码器：http://rapidjson.org/zh-cn/md_doc_encoding_8zh-cn.html

#include "mysqlitepatients.h"
#include "eyesightstandard.h"
#include "global.h"
#include "globalclass.h"
#include "settings/settings.h"
#include "windatatrans.h"
#include "aboutdevice.h"
#include "sysinfo.h"

//
using namespace std;
using namespace rapidjson;

#define TR_CONTEXT "DataTransmit.cpp"
#define TR(str) QCoreApplication::translate(TR_CONTEXT, str)

//
namespace DataTrans
{
//const int MAX_SENDING_COUNT             = 3;        // 结果数据发送重试次数

const double TIME_OUT_HTTP_DEFAULT      = 10;       // http 缺省 应答超时（秒）
const double TIME_OUT_HTTP_AUTH         = 10;       // http 鉴权 应答超时（秒）
const double TIME_OUT_HTTP_DATA_BASE    = 10;       // http 数据上传 应答超时（秒），基本超时时间，还要加上与数据量等比的增量
const double TIME_OUT_HTTP_DATA_PER     = 1;        // http 数据上传 应答超时（秒），每条数据的增量，总时间 = 基本时间 + 每条增量 * 条数
const double TIME_OUT_HTTP_CLIENT       = 10;       // http 单个被测者查询 超时时间（秒）
const double TIME_OUT_HTTP_LIST         = 60;       // http 批量被测者查询 超时时间（秒）     // TODO: 如果一直无数据，不应等待这么长时间
const double TIME_OUT_HTTP_IMAGE        = 30;       // http 图像上传 超时时间（秒）

const double TIME_OUT_BT_DATA_BASE      = 2;        // 数据上传 蓝牙 应答超时（秒）（基本）              // TODO: 蓝牙不应该需要应答？
const double TIME_OUT_BT_DATA_PER       = 1;        // 数据上传 蓝牙 应答超时（秒）（每条结果增加量）

const int URL_LEN = 400;

std::string DataTransmiter::DevCode = "";

std::string DataTransmiter::ReceiverAddr = "";
int DataTransmiter::ReceiverPort = 0;

std::string DataTransmiter::PathData = "";
std::string DataTransmiter::PathClient = "";
std::string DataTransmiter::PathClientList = "";
std::string DataTransmiter::PathAuth = "";
std::string DataTransmiter::PathImage = "";

std::string DataTransmiter::AuthUserName = "";
std::string DataTransmiter::AuthPassword = "";
std::string DataTransmiter::AuthUserType = "";

bool DataTransmiter::IsNeedAuth = false;
bool DataTransmiter::IsUseHttps = false;
enConnMode DataTransmiter::ConnMode = connMode_Http;

bool DataTransmiter::IsPostImmediately = false;
bool DataTransmiter::IsUploadImage = false;

fCallback_GetNewSubject DataTransmiter::Callback_GetNewSubject = Q_NULLPTR;
fCallback_OperationLocked DataTransmiter::Callback_OperationLocked = Q_NULLPTR;

HttpReqCallback DataTransmiter::mRequestCallback = Q_NULLPTR;

//
std::string DataTransmiter::FTokenStr = "";
time_t DataTransmiter::FTokenExpire = 0;

std::string DataTransmiter::FProcMsg = "";
bool DataTransmiter::FProcResult = false;

bool DataTransmiter::FNeedResend = false;

Clients* DataTransmiter::FClients = NULL;
QVector<int> *DataTransmiter::FListFail = Q_NULLPTR;

bool DataTransmiter::mIsWaitingBtResp = false;
CBtConnection *DataTransmiter::BtConn = Q_NULLPTR;
CSerialDatatrans *DataTransmiter::serialDatatrans = Q_NULLPTR;

//HdlSendReportFinished DataTransmiter::OnSendReportFinished = NULL;

// =============================================================================

string TrimRN(string _str)
{
    string str = _str;
    int len = str.length();
    if (('\r' == str[len - 1]) || ('\n' == str[len - 1]))
        str.erase(len - 1);
    if (('\r' == str[len - 2]) || ('\n' == str[len - 2]))
        str.erase(len - 2);
    return str;
}

bool CSVGetNextField(const string& _csv_str, string::size_type& _prior_start, string::size_type & _prior_end, string& _val_str)
{
    _val_str = "";
    _prior_start = _prior_end + 1;
    if ((_csv_str.length()) == _prior_start)
        return true;
    else if ((_csv_str.length()) < _prior_start)
        return false;
    _prior_end = _csv_str.find(',', _prior_start);
    if (string::npos != _prior_end)
    {
        _val_str = _csv_str.substr(_prior_start, _prior_end - _prior_start);
        return true;
    }
    else
    {
        if (_prior_start < _csv_str.length())
        {
            _val_str = _csv_str.substr(_prior_start, _csv_str.length() - _prior_start);
            _prior_end = _csv_str.length();
            return true;
        }
        else
            return false;
    }
}

int StrToInt(string _str)
{
    int result = 0;
    try
    {
        result = std::atoi(_str.c_str());
    }
    catch (...)
    {
        result = 0;
    }
    return result;
}

long StrToLong(string _str)
{
    long result = 0;
    try
    {
        result = std::atol(_str.c_str());
    }
    catch (...)
    {
        result = 0;
    }
    return result;
}

double StrToFloat(string _str)
{
    double result = 0;
    try
    {
        result = std::atof(_str.c_str());
    }
    catch (...)
    {
        result = 0;
    }
    return result;
}

bool StrToBool(string _str)
{
    bool result = false;
    try
    {
        //std::istringstream(_str) >> std::boolalpha >> result;

        if ((_str == "true") || (_str == "True") || (_str == "1"))
            result = true;
    }
    catch (...)
    {
        result = false;
    }
    return result;
}

void IntToStr(int& _i, string& _s)
{
    stringstream ss;
    ss << _i;
    ss >> _s;
}

static unsigned char hexchars[] = "0123456789ABCDEF";

char* url_encode(const char* s, int len, int* new_length)
{
    unsigned char c;
    unsigned char *to, *start;
    unsigned char const *from, *end;

    from = (unsigned char *)s;
    end = (unsigned char *)s + len;
    start = to = (unsigned char *)calloc(1, 3 * len + 1);

    while (from < end)
    {
        c = *from++;

        if (c == ' ')
        {
            *to++ = '+';
        }
        else if ((c < '0' && c != '-' && c != '.') ||
                 (c < 'A' && c > '9') ||
                 (c > 'Z' && c < 'a' && c != '_') ||
                 (c > 'z'))
        {
            to[0] = '%';
            to[1] = hexchars[c >> 4];
            to[2] = hexchars[c & 15];
            to += 3;
        }
        else
        {
            *to++ = c;
        }
    }
    *to = 0;
    if (new_length)
    {
        *new_length = to - start;
    }
    return (char *)start;
}

//
const char* newGUID()
{
    static bool is_init = false;

    if (!is_init) {
        srand((unsigned int)time(NULL));
        is_init = true;
    }

    static char buf[64] = { 0 };
    snprintf(buf, sizeof(buf),
             //"{%08X-%04X-%04X-%04X-%04X%04X%04X}",
             "%04X%04X%04X%04X%04X%04X%04X",
             //rand() & 0xffffffff,
             rand() & 0xffff,
             rand() & 0xffff,
             rand() & 0xffff,
             rand() & 0xffff,
             rand() & 0xffff, rand() & 0xffff, rand() & 0xffff
             );
    return (const char*)buf;
}

// ============================================================================================

// 初始化client静态变量
int HttpClient::s_exit_flag = 0;
HttpReqCallback HttpClient::s_req_callback = NULL;

// 客户端的网络请求响应
void HttpClient::OnHttpEvent(mg_connection *connection, int event_type, void *event_data)
{
    http_message *hm = (struct http_message *)event_data;

    switch (event_type)
    {
        case MG_EV_CONNECT:     // 连接事件，成功或失败
        {
            int connect_status = *((int *)event_data);
            /* MG_EV_CONNECT 的 error code:
             * （来源：montoose.c 搜 "MG_EV_CONNECT"；所有调用 mg_if_connect_cb() 的地方；
             * -1 : resolve_cb()，   "cannot schedule DNS lookup"?
             * 错误码转描述的写法，参见“mongoose.h”有关 MG_EV_CONNECT 的备注。
             */

            if (0 == connect_status)    // NOTE: 0 表示无错误
            {
                // do nothing
            }
            else
            {
                mg_set_timer(connection, 0);  // Clear connect timer

                printf("connect() error: %s(%d)\n", strerror(connect_status), connect_status);
                s_exit_flag = 1;

                //
                if (s_req_callback) {
                    string s_stat;
                    IntToStr(connect_status, s_stat);
                    string msg = string("Failed to connect: ") + s_stat + "(" + strerror(connect_status) + ")";
                    s_req_callback(dataSendingError_CannotConnect, msg);
                }
            }
        }
            break;
        //case MG_EV_RECV:      // TODO: 这个事件只是一次接收事件，并不意味着应答已完整接收？
        case MG_EV_HTTP_REPLY:  // HTTP 应答事件 (HTTP回复已到,解析的回复struct http_message通过处理程序的void *ev_data 指针传递)
        {
            //            printf("Got reply:\n%.*s\n", (int)hm->body.len, hm->body.p);
            std::string rsp = std::string(hm->body.p, hm->body.len);
            connection->flags |= MG_F_SEND_AND_CLOSE;
            s_exit_flag = 1; // 每次收到请求后关闭本次连接，重置标记

            // 回调
            if (s_req_callback)
                s_req_callback(dataSendingError_No, rsp);
        }
            break;
        case MG_EV_TIMER:   //连接超时事件    // TODO: 这个是超时事件，而不是只是定时器事件，还需判断是否连接上、是否正在传输数据，才能判断是否超时？
        {
            printf("Connect timeout");
            connection->flags |= MG_F_CLOSE_IMMEDIATELY;
            s_exit_flag = 1;

            // 回调
            if (s_req_callback)
            {
                string msg = "timeout";
                s_req_callback(dataSendingError_TransmissionTimeout, msg);
            }
        }
            break;
        case MG_EV_CLOSE:   // 连接关闭事件
        {
            if (s_exit_flag == 0)
            {
                printf("Server closed connection\n");
                s_exit_flag = 1;

                //
                if (s_req_callback)
                {
                    string msg = "server closed connection";
                    s_req_callback(dataSendingError_ServerClosed, msg);
                }
            }
        }
            break;
        default:
            // TODO: ？MG_EV_POLL ?
            break;
    }
}

//发送一次请求，并回调处理，然后关闭本次连接
void HttpClient::SendReq(const std::string& url, const std::string& _extra_header,
                         const char *_body, HttpReqCallback req_callback, int _timeout, int _body_len)
{
    if (_timeout <= 0) {
        _timeout = TIME_OUT_HTTP_DEFAULT;
    }

    //
    s_exit_flag = 0;
    //time_t t_start = time(NULL);

    //给回调函数赋值
    s_req_callback = req_callback;      // TODO: 并行错乱风险？
    mg_mgr mgr;     //创建一个连接管理器
    mg_mgr_init(&mgr, NULL);    //对结构体内的数据做一些赋值操作，创建网络接口以及ssl初始化等

    mg_connection *connection = NULL;   //新建一个监听连接，绑定端口和回调函数，加入到事件管理器中
    //进入循环，轮询所有连接，如果有事件发生，则进行处理
    //创建出站链接
    if (0 == _body_len)
        connection = mg_connect_http(&mgr, OnHttpEvent, url.c_str(), _extra_header.c_str(), _body);
    else
        connection = mg_connect_http_ext(&mgr, OnHttpEvent, url.c_str(), _extra_header.c_str(), _body, _body_len);

    if (connection) {
        //struct mg_connect_opts opts;
        //memset(&opts, 0, sizeof(opts));
        //opts.flags |= ;
        //mg_connection* connection = mg_connect_http_opt(&mgr, OnHttpEvent, opts, url.c_str(), extra_header, _json_str.c_str());

#ifndef _DEBUGGING
        mg_set_timer(connection, mg_time() + _timeout);   // 发送改事件给改连接
#endif // _DEBUGGING

        mg_set_protocol_http_websocket(connection);

        printf("Http request url: %s\n", url.c_str());

        // loop
        //time_t t_now;
        while (s_exit_flag == 0)
        {
            //time(&t_now);
            //if (t_now - t_start > _timeout)
            //{
            //    char* msg = "connection timeout";
            //    s_req_callback(dataSendingError_TransmissionTimeout, msg);

            //    s_exit_flag = 1;
            //    break;
            //}
            // TODO: mg_mgr_poll()时，如果服务端不响应，在 Windows8 里测试不会阻塞，但在视筛Linux里会

            mg_mgr_poll(&mgr, 500);
        }
    } else {
        std::string err_msg = "failed to create http connection!";
        logCritical(err_msg.c_str(), CGlobal::LOG_DATATRANS);
        s_req_callback(dataSendingError_CannotConnect, err_msg);
    }
    mg_mgr_free(&mgr);
}

// ============================================================================================
// <-- DataTransmiter

bool DataTransmiter::sendBtRequest(std::string &_json_str, HttpReqCallback _request_callback, int _timeout_sec)
{
    qDebug() << __PRETTY_FUNCTION__ << ": currentThreadId = " << reinterpret_cast<quintptr>(QThread::currentThreadId());

    //
    QByteArray str_resp = QByteArray(_json_str.c_str());
    str_resp = QByteArray("\n\n") + str_resp + "\n\n";

    if (BtConn) {
        mRequestCallback = _request_callback;
        mIsWaitingBtResp = true;

        BtConn->pushSendingData(str_resp.data());
    } else {
        return false;
    }

    // 阻塞等待应答
    if (_timeout_sec > 0) {
        time_t now = time(NULL);
        bool is_timeout = false;
        while (mIsWaitingBtResp) {
            QThread::msleep(10);

            if (time(NULL) - now >= _timeout_sec) {
                FProcMsg = "timeout";
                is_timeout = true;

                //
                break;
            }
        }

        // 若超时，重置等待状态值
        if (is_timeout) {
            logWarning("Bluetooth Request waiting response timeout!", CGlobal::LOG_DATATRANS);
            mIsWaitingBtResp = false;
        }
    } else {
        mIsWaitingBtResp = false;
    }

    //
    //return (!is_timeout);       /* 因大部分对接的客户都没有做应答，若超时当作失败，体验不好，所以这里超时时也当作是成功 */

    //
    FProcResult = true;
    FProcMsg = "";

    // TODO: 检查确保 FListFail 不为空，或 FListFail 改为对象？
    //

    FListFail->clear();
    //FListFail = NULL;

    return true;
}

void DataTransmiter::Callback_BtRequest(QByteArray &_resp_data)
{
    // 应答数据处理完后，重置状态值
    mIsWaitingBtResp = false;

    // 应答数据处理
    std::string resp_data(_resp_data.data());
    mRequestCallback(dataSendingError_No, resp_data);
}

// 发送数据
bool DataTransmiter::SendMeasureData(QString& _msg, QVector<int> &_fail_ids, vector<CPatient>& _patients)
{
    // 失败列表初始化为全部编号
    for (int i = _patients.size() - 1; i >= 0; i--) {
        _fail_ids.append(_patients.at(i).id);
    }
    FListFail = &_fail_ids;

    //
    if ((connMode_Bluetooth == ConnMode) && !BtConn->getIsConnected()) {
        _msg = QCoreApplication::translate("DataTransmit.cpp", "未找到已连接的“数据传输”类别的蓝牙设备");  // "No 'Transmission' category bluetooth device connected been found!"
        return false;
    }

    // 默认参数

    // 参数检查
    if (ReceiverAddr.empty()) {
        _msg = TR("“服务器地址”未设置！");    // "The 'Server Address' has not been set up!"
        return false;
    }

    if (PathData.empty()) {
        _msg = TR("“数据接收接口”未设置！");    // "The 'Data Receiving Interface' has not been set up!"
        return false;
    }

    // 默认返回值

    //
    _msg = "";

    FProcMsg = "";
    FProcResult = false;
    FNeedResend = false;

    string msg_task;
    try
    {
        MeasureResults results(_patients.size());   //批量受测者结果
        DataVectorToObj(_patients, results);        //单个受测者结果数据转换
        DataSendingTask task{results, msg_task };   //数据发送任务

        // 检查授权
        string str_auth;
        if (!CheckAndGetAuth(str_auth))
        {
            _msg = QCoreApplication::translate("DataTransmit.cpp", "获取授权失败：") + QString::fromStdString(str_auth);   // "Obtaining authorization failed: "
            return false;
        }

        // 尝试发送数据
        //for (int send_count = 1; send_count <= MAX_SENDING_COUNT; send_count++)
        //{
        //}
        DoSendMeasureData(task);    //上传数据
        if(FNeedResend)
            DoSendMeasureData(task);

        //回调
        //if(OnSendReportFinished)
        //    OnSendReportFinished();
    }
    catch (exception& ex)
    {
        FProcMsg = "exception on sending data : " + string(ex.what());
        FProcResult = false;
    }
    catch (...)
    {
        // TODO: log()
        //if (task.Msg.length > 0)
        //    msg_task = task.Msg + "\r\n";
        FProcMsg = "exception on sending data : unknown error";
        FProcResult = false;
    }

    _msg = QString::fromStdString(FProcMsg);
    if(!FProcResult)
    {
        FTokenExpire = 0;
        FTokenStr = "";
    }
    return FProcResult;
}

string DataTransmiter::getDevCode()
{
    enDataInterfaceCfg intf_type = WinDataTrans::getCfg_intfType();
    if (dataInterfaceCfg_Http == intf_type ||
            dataInterfaceCfg_Bluetooth == intf_type ||
            dataInterfaceCfg_UsbUart == intf_type ||
            dataInterfaceCfg_Uart == intf_type
            ) {
        //
        if (!DataTransmiter::DevCode.empty()) {
            return DataTransmiter::DevCode;
        } else {
            return CGlobal::devNum.toStdString();
        }
    } else {
        //
        return CGlobal::devNum.toStdString();
    }
}

bool DataTransmiter::GetAuthToken(std::string& _msg)
{
    // 默认参数

    // 参数检查
    if (PathAuth.empty()) {
        _msg = TR("“登陆接口”未设置！").toStdString();    // "The 'Login Interface' has not been set up!"
        return false;
    }

    // 目前只有 http 支持鉴权
    if (connMode_Http != ConnMode) {
        logWarning("DataTransmiter::GetAuthToken(): bluetooth conn mode not support get auth!", CGlobal::LOG_DATATRANS);
        return true;
    }

    // 默认返回值
    _msg = "";
    //FTokenStr = "";       // 不应清掉，之前的值还可能有用
    //FTokenExpire = 0;

    FProcMsg = "";
    FProcResult = false;

    try
    {
        string auth_json_str = "";
        GetAuthJson(auth_json_str);     //打包用户名,密码,账号类别等

        MLMCommunic communic;
        communic.func = FUNC_NEW;
        communic.data = auth_json_str;
        communic.version = PROTOCOL_VERSION;

        string json_communic_str = "";
        communic.ToJson(json_communic_str);
        //std::cout << json_communic_str << std::endl;

        //
        char url[URL_LEN];
        GetUrl(url, URL_LEN, PathAuth, false);

        string extra_header = string("Content-Type:application/json; charset=utf-8\r\n") +
                "Accept-Charset: utf-8\r\n" + "Connection: close\r\n";

        int timeout_sec = TIME_OUT_HTTP_AUTH;

        HttpClient::SendReq(url/*_encoded*/, extra_header, json_communic_str.c_str(), &DataTransmiter::CallbackAuth, timeout_sec);    //CallbackAuth:回调函数

        //free(url_encoded);

    }
    catch (exception& ex)
    {
        FProcMsg = "exception on get token : " + string(ex.what());
        FProcResult = false;
    }
    catch (...)
    {
        // TODO: log()

        //
        FProcMsg = "exception on get token : nuknown error";
        FProcResult = false;
    }

    //
    _msg = FProcMsg;
    return FProcResult;     //真值在回调函数CallbackAuth中有设置
}

//查询受测者信息
bool DataTransmiter::GetClientInfo(QString &_msg, std::string _nums, Clients& _clients)
{
    if ((connMode_Bluetooth == ConnMode) && !BtConn->getIsConnected()) {
        _msg = QCoreApplication::translate("DataTransmit.cpp", "未找到已连接的“数据传输”类别的蓝牙设备");  // "No 'Transmission' category bluetooth device connected been found!"
        return false;
    }

    // 默认参数

    // 参数检查
    if (ReceiverAddr.empty()) {
        _msg = TR("“服务器地址”未设置！");    // "The 'Server Address' has not been set up!"
        return false;
    }

    if (PathClient.empty()) {
        _msg = TR("“被测者信息接口”未设置！");    // "The 'Subject Query Path' has not been set up!"
        return false;
    }

    // 默认返回值
    _msg = "";
    FClients = &_clients;

    FProcMsg = "";
    FProcResult = false;

    //
    try
    {
        // 检查授权
        string str_auth;
        if (!CheckAndGetAuth(str_auth))
        {
            _msg = QCoreApplication::translate("DataTransmit.cpp", "获取授权失败：") + QString::fromStdString(str_auth);   // "Obtaining authorization failed: "
            return false;
        }

        //
        MLMCommunic communic;
        communic.func = FUNC_QUERY;
        communic.data = _nums;
        communic.version = PROTOCOL_VERSION;

        string json_communic_str = "";
        communic.ToJson(json_communic_str);

        //qDebug()<<"send request json str:"<<QString::fromStdString(json_communic_str);

        //
        if (connMode_Http == DataTransmiter::ConnMode) {
            char url[URL_LEN];
            GetUrl(url, URL_LEN, PathClient);

            string extra_header = string("Content-Type:application/json; charset=utf-8\r\n") +
                    "Accept-Charset: utf-8\r\n" + "Connection: close\r\n";
            if (IsNeedAuth)
                extra_header += "token:" + FTokenStr + "\r\n";

            //            printf("clientInfo request--header:%s,body:%s\n",extra_header.c_str(),json_communic_str.c_str());

            int timeout_sec = TIME_OUT_HTTP_CLIENT;

            HttpClient::SendReq(url/*_encoded*/, extra_header, json_communic_str.c_str(), &DataTransmiter::CallbackClient, timeout_sec);
            //free(url_encoded);

            //FProcResult = true;
        } else if (connMode_Bluetooth == DataTransmiter::ConnMode) {
            logWarning("DataTransmiter::GetClientInfo(): BlueTooth not supported!", CGlobal::LOG_DATATRANS);
            // TODO:
        } else if (connMode_UsbUart == DataTransmiter::ConnMode || connMode_Uart == DataTransmiter::ConnMode) {
            FProcMsg = "ConnMode not supported!";
            FProcResult = false;
            logWarning(QString("DataTransmiter::GetClientInfo(): ") + QString::fromStdString(FProcMsg), CGlobal::LOG_DATATRANS);
            // TODO:

        }
    }
    catch (exception& ex)
    {
        FProcMsg = "exception on get client info : " + string(ex.what());
        FProcResult = false;
    }
    catch (...)
    {
        // TODO: log()

        //
        FProcMsg = "exception on get client info : nuknown error";
        FProcResult = false;
    }

    //
    _msg = QString::fromStdString(FProcMsg);
    return FProcResult;     //真值在回调函数CallbackClient中有设置
}

//数据导入
bool DataTransmiter::GetClientListInfo(QString &_msg, QString _batchNo)
{
    if ((connMode_Bluetooth == ConnMode) && !BtConn->getIsConnected()) {
        _msg = QCoreApplication::translate("DataTransmit.cpp", "未找到已连接的“数据传输”类别的蓝牙设备");  // "No 'Transmission' category bluetooth device connected been found!"
        return false;
    }

    // 默认参数

    // 参数检查
    if (ReceiverAddr.empty()) {
        _msg = TR("“服务器地址”未设置！");    // "The 'Server Address' has not been set up!"
        return false;
    }

    if (PathClientList.empty()) {
        _msg = TR("“批量名单接口”未设置！");    // "The 'Batch List Query' has not been set up!"
        return false;
    }

    // 默认返回值
    _msg = "";
    //FClients = &_clients;
    FProcMsg = "";
    FProcResult = false;

    try
    {
        // 检查授权
        string str_auth;
        if (!CheckAndGetAuth(str_auth))
        {
            _msg = QCoreApplication::translate("DataTransmit.cpp", "获取授权失败：") + QString::fromStdString(str_auth); // "Obtaining authorization failed: "
            return false;
        }

        MLMCommunic communic;
        communic.func       = FUNC_QUERY;       //请求
        communic.data       = "all_data";       //导入所以数据
        communic.version    = PROTOCOL_VERSION; //版本号v1.2
        communic.msg        = DataTransmiter::getDevCode();  //设备号

        string json_communic_str = "";
        communic.ToJson(json_communic_str);     //转换为JSON格式字符串
        qDebug()<<"send request json str:"<<QString::fromStdString(json_communic_str);

        //
        if (connMode_Http == DataTransmiter::ConnMode) {
            char url[URL_LEN];
            GetUrl(url, URL_LEN, PathClientList);   //打包接收端入口地址(批量接口)

            string extra_header = string("Content-Type:application/json; charset=utf-8\r\n") +
                    "Accept-Charset: utf-8\r\n" + "Connection: close\r\n";
            if (IsNeedAuth)     //身份验证
                extra_header += "token:" + FTokenStr + "\r\n";
            qDebug()<<"-----Recevie request url:"<<QString::fromLatin1(url);    //例:http://192.168.5.36:8081/clientlist

            int timeout_sec = TIME_OUT_HTTP_LIST;

            HttpClient::SendReq(url/*_encoded*/, extra_header, json_communic_str.c_str(), &DataTransmiter::CallbackClientList, timeout_sec);  //CallbackClientList:http请求回调函数
            //free(url_encoded);

            //FProcResult = true;
        } else if (connMode_Bluetooth == DataTransmiter::ConnMode) {
            logWarning("DataTransmiter::GetClientListInfo(): BlueTooth not supported!", CGlobal::LOG_DATATRANS);
            // TODO:
        } else if (connMode_UsbUart == DataTransmiter::ConnMode || connMode_Uart == DataTransmiter::ConnMode) {
            FProcMsg = "ConnMode not supported!";
            FProcResult = false;
            logWarning(QString("DataTransmiter::GetClientListInfo(): ") + QString::fromStdString(FProcMsg), CGlobal::LOG_DATATRANS);
            // TODO:

        }
    }
    catch (exception& ex)
    {
        FProcMsg = "exception on get client info : " + string(ex.what());
        FProcResult = false;
    }
    catch (...)
    {
        // TODO: log()
        FProcMsg = "exception on get client info : nuknown error";
        FProcResult = false;
    }
    _msg = QString::fromStdString(FProcMsg);

    return FProcResult;     //真值在回调函数CallbackClientList中有设置
}

//发送图片  2020.8.28改  tao
bool DataTransmiter::SendMeasureImage(std::string & _msg, const std::string & _client_num, const std::string & _test_time,
                                      const std::string& _batch_no, const std::string& _barcode, const IplImage & _image)
{
    // 默认参数

    // 参数检查
    if (ReceiverAddr.empty()) {
        _msg = TR("“服务器地址”未设置！").toStdString();    // "The 'Server Address' has not been set up!"
        return false;
    }

    if (PathImage.empty()) {
        _msg = TR("“图像接收接口”未设置！").toStdString();    // "The 'Image Receiving Interface' has not been set up!"
        return false;
    }

    // 目前只有 http 支持发送图像
    if (connMode_Http != ConnMode) {
        logWarning("DataTransmiter::GetAuthToken(): Conncting method not support sending image!", CGlobal::LOG_DATATRANS);
        return true;
    }

    // 默认返回值
    _msg = "";

    FProcMsg = "";
    FProcResult = false;

    try
    {
        // 检查授权
        string str_auth;
        if (!CheckAndGetAuth(str_auth))
        {
            _msg = QCoreApplication::translate("DataTransmit.cpp", "获取授权失败：").toStdString() + str_auth;  // "Obtaining authorization failed: "
            return false;
        }

        string boundary_str = "------FormBoundary";
        //boundary_str += xg::newGuid().str();
        boundary_str += newGUID();
        int len_boundary = boundary_str.length();

        cv::Mat tmp_mat = cv::cvarrToMat(&_image);
        vector<uchar> jpg_buff;
        cv::imencode(".jpg", tmp_mat, jpg_buff);
        int len_jpg = jpg_buff.size();

        bool is_save_img = false;
        if (is_save_img) {
            // 保存 jpg 到本地文件
            FILE *fp_jpg = fopen("debug_upload-img.jpg", "w+b");
            for (int i = 0; i < len_jpg; i++)
            {
                fwrite(&jpg_buff[i], 1, 1, fp_jpg);
            }
            fclose(fp_jpg);
        }

        //1.打包编号
        //int len_str_client_num = 150 + _client_num.length();
        //char* str_client_num = new char[len_str_client_num];
        //snprintf(str_client_num, len_str_client_num,
        //         "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n",
        //         boundary_str.c_str(), "ClientNum", _client_num.c_str());
        //1.打包编号和时间   2020.8.28  tao
        //int len_str_client_num = 200 + _client_num.length() + _test_time.length();
        //char* str_client_num = new char[len_str_client_num];
        //snprintf(str_client_num, len_str_client_num,
        //         "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n%s\r\n%s\r\n",
        //         boundary_str.c_str(), "ClientNum", _client_num.c_str(), "TestTime",_test_time.c_str());

        // 1. 打包编号   2021.03.08  tao
        int len_str_client_num = 200 + _client_num.length() /*+ _test_time.length()*/;
        char* str_client_num = new char[len_str_client_num];
        //if(MySQLitePatients:: getIsBatch(QString::fromStdString(_client_num))) //批量导入的数据就发送原编号
        {
            snprintf(str_client_num, len_str_client_num,
                     "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n",
                     boundary_str.c_str(), "ClientNum", _client_num.c_str());   //编号
        }
        //else    //本地生成的数据就发送处理过的编号        /* 这种逻辑不合理，应由接收端判断该编号是否来自己方 */  // TODO: PC 端改进（和 DataTransmiter::DataVectorToObj() 同步修改）
        //{
        //    QString qstr = QString::fromStdString(_test_time);  //String转换QString
        //    qstr = qstr.replace(QRegExp("\\-"), "");    //正则表达式,"-"转""
        //    qstr = qstr.remove(QRegExp("\\s"));         //删除所有空格
        //    qstr = qstr.replace(QRegExp("\\:"), "");    //正则表达式,":"转""
        //    string test_time = qstr.toStdString();      //QString转换String
        //    snprintf(str_client_num, len_str_client_num,
        //             "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s_%s_%s\r\n",
        //             boundary_str.c_str(),"ClientNum",DevCode.c_str(),test_time.c_str(), _client_num.c_str());  //设备号_时间_编号
        //}
        //qDebug()<<"-----JpgName:"<<QString::fromStdString(str_client_num);

        // 2. 测量时间
        int len_str_testtime = 0;
        char empty_str[] = "";
        char *str_testtime = empty_str;
        bool is_add_test_time = false;
        if (WinDataTrans::isManylinksDataIntf() || is_add_test_time || CGlobal::isDebugMode || WinDataTrans::isManylinksProtocal) {  // 目前只有万灵云端需要测量时间，若是全部加上，可能会导致其它对接端接收失败
            len_str_testtime = 150 + _test_time.length();
            str_testtime = new char[len_str_testtime];
            snprintf(str_testtime, len_str_testtime,
                     "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n",
                     boundary_str.c_str(), "TestTime", _test_time.c_str());
        }

        // 3. 打包批次号
        int len_str_batchno = 150 + _batch_no.length();
        char* str_batchno = new char[len_str_batchno];
        snprintf(str_batchno, len_str_batchno,
                 "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n",
                 boundary_str.c_str(), "BatchNo", _batch_no.c_str());
        // 4. 打包扫码数据
        int len_str_barcode = 150 + _barcode.length();
        char* str_barcode = new char[len_str_barcode];
        snprintf(str_barcode, len_str_barcode,
                 "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n",
                 boundary_str.c_str(), "Barcode", _barcode.c_str());
        // 5. 打包图片名
        int len_str_file = 200 + _client_num.length();
        char* str_file = new char[len_str_file];
        snprintf(str_file, len_str_file,
                 "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n%s\r\n\r\n",
                 boundary_str.c_str(), "File", (_client_num + ".jpg").c_str(), "Content-Type: image/jpeg");
        // 6. 计算总数据长度
        int len_body = strlen(str_client_num) + strlen(str_testtime) + strlen(str_batchno) + strlen(str_barcode) + strlen(str_file) + (len_jpg + 2) + (2 + len_boundary + 4);
        // 7. 字符拼接
        char* str_body = new char[len_body + 1];
        snprintf(str_body, len_body, "%s%s%s%s%s", str_client_num, str_testtime, str_batchno, str_barcode, str_file);
        // 8. 打包图片数据
        int len_curr = strlen(str_body);
        for (int i = 0; i < len_jpg; i++)
        {
            str_body[len_curr] = (char)jpg_buff[i];
            len_curr++;
        }
        str_body[len_curr] = '\r';
        len_curr++;
        str_body[len_curr] = '\n';
        len_curr++;

        string str_image_tail = "--" + boundary_str + "--\r\n";
        int len_tail = str_image_tail.length();
        memcpy(str_body + len_curr, str_image_tail.c_str(), len_tail);

        bool is_save_body = false;
        if (is_save_body) {
            // 保存 str_body 到本地文件
            FILE *fp_body = fopen("debug_upload-img-body.tmp", "w+b");
            fwrite(str_body, 1, len_body, fp_body);
            fclose(fp_body);
        }

        //
        char url[URL_LEN];
        GetUrl(url, URL_LEN, PathImage);

        string extra_header = "Content-Type: multipart/form-data; charset=utf-8; boundary=" + boundary_str + "\r\n" +
                "Accept-Charset: utf-8\r\n" + "Connection: close\r\n";
        if (IsNeedAuth)
            extra_header += "token:" + FTokenStr + "\r\n";

        int timeout_sec = TIME_OUT_HTTP_IMAGE;

        HttpClient::SendReq(url/*_encoded*/, extra_header, str_body, &DataTransmiter::CallbackImage, timeout_sec, len_body);

        //            printf("upload img header:%s\nbody:%s\n",extra_header.c_str(),str_body);
        //free(url_encoded);    // TODO: ？

        delete []str_client_num;
        str_client_num = nullptr;

        if (len_str_testtime > 0) {
            delete []str_testtime;
            str_testtime = nullptr;
        }

        delete []str_batchno;
        str_batchno = nullptr;

        delete []str_barcode;
        str_barcode = nullptr;

        delete []str_file;
        str_file = nullptr;

        delete []str_body;
        str_body = nullptr;
    }
    catch (exception& ex)
    {
        FProcMsg = "exception on uploading image : " + string(ex.what());
        FProcResult = false;
    }
    catch (...)
    {
        // TODO: log()

        //
        FProcMsg = "exception on uploading image : nuknown error";
        FProcResult = false;
    }


    //
    _msg = FProcMsg;
    return FProcResult;
}

//解析二维码数据(扫码设置参数)
bool DataTransmiter::SetFromCSV(std::string & _msg, const std::string & _csv_str/*,IntfConfigValue & _conf_value*/)
{
    Clear();

    string str_csv = TrimRN(_csv_str);

    string::size_type prior_start = 0, prior_end = -1;
    string val_str;
    //对应格式为:"192.168.5.36,8081,/data,/image,False,False,False,/auth,/client,test,123,ssi,False,/batch,/nv,/clientlist"
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        ReceiverAddr = val_str;
    else { _msg = "解析“服务端地址”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        ReceiverPort = StrToInt(val_str);
    else { _msg = "解析“服务端口”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        PathData = val_str;
    else { _msg = "解析“数据接收路径”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        PathImage = val_str;
    else { _msg = "解析“图像接收路径”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        IsPostImmediately = StrToBool(val_str);
    else { _msg = "解析“即时上传”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        IsUploadImage = StrToBool(val_str);
    else { _msg = "解析“上传图像”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        IsNeedAuth = StrToBool(val_str);
    else { _msg = "解析“身份验证”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        PathAuth = val_str;
    else { _msg = "解析“鉴权接口路径”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        PathClient = val_str;
    else { _msg = "解析“客户接口路径”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        AuthUserName = val_str;
    else { _msg = "解析“用户名”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        AuthPassword = val_str;
    else { _msg = "解析“密码”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        AuthUserType = val_str;
    else { _msg = "解析“账号类别”字段失败"; return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
        IsUseHttps = StrToBool(val_str);
    else { _msg = "解析“使用HTTPS”字段失败"; return false; }

    //2020.9.22  tao
    if (prior_end > str_csv.size()) {
        if (CSVGetNextField(str_csv, prior_start, prior_end, val_str))
            PathClientList = val_str;
        else { _msg = "解析“批量接口路径”字段失败"; return false; }
    }

    return true;
}

// 处理接收到的通信数据包
bool DataTransmiter::processReceivedDataPkg(QString &_msg, QByteArray &_data)
{
    // 调用这个函数时，连接方式不应是 http/https
    // ASSERT(connMode_Http != ConnMode);
    if (connMode_Http == ConnMode) {
        logCritical("DataTransmiter::processCommunicPkg(): ConnMode is http/https, should not call this function!", CGlobal::LOG_DATATRANS);
        return false;
    }

    //
    bool ret = false;

    MLMCommunic communic_resp;
    std::string str_json(_data.data());
    bool is_json_valid_communic = communic_resp.FromJson(str_json);
    if (is_json_valid_communic) {
        // 检测数据包是否可能是应答数据包
        bool is_response = mIsWaitingBtResp;
        if (FUNC_NEW_SUBJECT == communic_resp.func) {
            is_response = false;
        }

        //
        if (is_response) {       // 正在等待应答
            logDebug("current stat is waiting bluetooth response, checking response data", CGlobal::LOG_BLUETOOTH);

            QString err_str = "";
            try {
                Callback_BtRequest(_data);      // TODO: 为什么捕获异常了，还是崩溃？json错误：Items属性误直接设为一个数组
            } catch (exception& ex) {
                err_str = ex.what();
            } catch (...) {
                err_str = strerror(errno);
            }

            if (err_str.length() > 0) {
                logWarning(QString("DataTransmiter::processReceivedDataPkg() -> Callback_BtRequest() exception: ") + err_str, CGlobal::LOG_DATATRANS);
                _msg = err_str;
            }

            mIsWaitingBtResp = false;
        } else {                    // 被动接收的数据包
            if (FUNC_NEW_SUBJECT == communic_resp.func) {                   // 新增被测者
                // TODO: 应答？


                //
                Clients clients(1);
                bool is_json_valid_client = clients.FromJson(communic_resp.data);
                if (is_json_valid_client) {
                    // 返回被测者基本信息
                    //Client client;
                    //client.CloneFrom(clients.Items[0]);

                    if (NULL != Callback_GetNewSubject) {
                        Callback_GetNewSubject(clients.Items[0]);
                    } else {
                        logCritical(QString(__PRETTY_FUNCTION__) + ": Callback_GetNewSubject not setted!", CGlobal::LOG_DATATRANS);
                    }

                    //
                    ret = true;
                } else {
                    _msg = "Parse JSON by class Clients failed!";
                }
            } else if (FUNC_OPERATION_LOCKED == communic_resp.func) {       // 操作锁定
                bool is_valid = true;
                bool locked = false;
                if (communic_resp.data.length() == 0 || communic_resp.data == "true") {
                    locked = true;
                } else if (communic_resp.data == "false") {
                    locked = false;
                } else {
                    is_valid = false;
                }

                if (is_valid) {
                    if (NULL != Callback_OperationLocked) {
                        Callback_OperationLocked(locked, QString::fromStdString(communic_resp.msg));
                    } else {
                        logCritical(QString(__PRETTY_FUNCTION__) + ": Callback_OperationLocked not setted!", CGlobal::LOG_DATATRANS);
                    }
                } else {
                    logCritical(QString(__PRETTY_FUNCTION__) + ": data not valid!", CGlobal::LOG_DATATRANS);
                }

                //
                ret = true;
            } else {
                _msg = QString::asprintf("Value of \"func\" is \"%s\", not supported!", communic_resp.func.c_str());
            }
        }
    } else {
        _msg = "Parse JSON by class MLMCommunic failed!";
        logWarning(QString(_data), CGlobal::LOG_DATATRANS);
    }

    return ret;
}

// ============================================================================================
// 私有函数

//数据上传
bool DataTransmiter::DoSendMeasureData(DataSendingTask& _task)
{
    // 构造数据
    string data_json_str;
    _task.Data.ToJson(data_json_str);

    MLMCommunic communic;
    communic.func = FUNC_NEW;
    communic.data = data_json_str;
    communic.version = PROTOCOL_VERSION;

    string json_communic_str = "";
    communic.ToJson(json_communic_str);
    std::cout << json_communic_str << std::endl;

    //
    if (connMode_Http == DataTransmiter::ConnMode) {
        //send data (发送)
        char url[URL_LEN];  //url[400]
        GetUrl(url, URL_LEN, PathData);
        //QString qstr2 = QString::fromStdString(PathData);
        string extra_header = string("Content-Type:application/json; charset=utf-8\r\n") + "Accept-Charset: utf-8\r\n" + "Connection: close\r\n";
        if (IsNeedAuth)
            extra_header += "token:" + FTokenStr + "\r\n";
        qDebug()<<"-----Send request url:"<<QString::fromLatin1(url);

        int timeout_sec = TIME_OUT_HTTP_DATA_BASE + TIME_OUT_HTTP_DATA_PER * _task.Data.Len();

        HttpClient::SendReq(url/*_encoded*/, extra_header, json_communic_str.c_str(), &DataTransmiter::CallbackData, timeout_sec);    //检测接口路径

        //free(url_encoded);

        //
        return (0 == _task.Msg.length());     // TODO:
    } else if (connMode_Bluetooth == DataTransmiter::ConnMode) {
        int timeout_sec = TIME_OUT_BT_DATA_BASE + TIME_OUT_BT_DATA_PER * std::ceil((double)_task.Data.Len() / 1000);

        return sendBtRequest(json_communic_str, &DataTransmiter::CallbackData, timeout_sec);
    } else if (connMode_UsbUart == DataTransmiter::ConnMode || connMode_Uart == DataTransmiter::ConnMode) {   // TODO: 这里没有应答，导致调用端无限等待？
        FProcMsg = "ConnMode not supported!";
        FProcResult = false;
        logWarning(QString("DataTransmiter::DoSendMeasureData(): ") + QString::fromStdString(FProcMsg), CGlobal::LOG_DATATRANS);
        // TODO:

    }

    return false;
}

// Vector 里的检测结果数据赋值到 MeasureResults 对象（内部类转接口类）
void DataTransmiter::DataVectorToObj(vector<CPatient>& _vector, MeasureResults& _results)       // TODO: 与 UpLoadThread::dataVectorToObj() 共用？
{
    int len = _vector.size();
    for (int i = 0; i < len; i++)
    {
        MeasureResult& result = _results.Items[i];
        CPatient& patient = _vector[i];

        result.DevType = DataTrans::DEVICE_TYPE;
        result.DevCode = DataTrans::DataTransmiter::getDevCode();
        result.BatchNo = patient.batchNo.toStdString();
        result.RecordID = QString::number(patient.id).toStdString();
        result.AgeRange = std::to_string((int)patient.getAgeRange());

        //if (MySQLitePatients::getIsBatch(patient.patientid) || !(connMode_Http == ConnMode)) {   // 和视筛 PC 端对接
        //    //本地生成的数据就发送处理过的编号      //2021.03.08  tao
        //    /* 严谨的逻辑应该是所有由外部传入的编号都原样回传，但是由于外部传入的被测者都被设为 isBatch = true，所以可以这样写。 */    // TODO: 这个逻辑不合理，应完善 PC 端（和 DataTransmiter::SendMeasureImage() 同步修改）
        //    QString qstr = patient.patienttesttime;
        //    qstr = qstr.replace(QRegExp("\\-"), "");    //正则表达式,"-"转""
        //    qstr = qstr.remove(QRegExp("\\s"));         //删除所有空格
        //    qstr = qstr.replace(QRegExp("\\:"), "");    //正则表达式,":"转""
        //    qstr = QString::fromStdString(DevCode) + "_" + qstr + "_" + patient.patientid;  //设备号_时间_编号
        //    result.ClientNum = qstr.toStdString();      //QString转换String
        //} else
        {
            result.ClientNum = patient.patientid.toStdString();
        }
        result.IsClinic = (!patient.isBatch);

        result.Barcode = patient.barcodeData.toStdString();

        QDateTime measure_time = QDateTime::fromString(patient.patienttesttime, CPatient::dateTimeFormat());
        result.Time = measure_time.toString(DATE_TIME_FORMAT).toStdString();

        result.PD = patient.patientpd.toInt();

        result.LPupil = patient.patientleftpd.toStdString().c_str();
        result.LSE = patient.patientleftse.toStdString().c_str();
        result.LDS = patient.patientlefteyesph.toStdString().c_str();
        result.LDC = patient.patientlefteyecyl.toStdString().c_str();
        result.LAxis = patient.patientlefteyeax.toInt();
        result.LVision = CAlgoInvoker::diopterToVision(patient.patientlefteyesph, patient.patientlefteyecyl, CGlobal::visionNotation.getValue()).toStdString();
        result.LGazeH = patient.patientlefths.toInt();
        result.LGazeV = patient.patientleftvs.toInt();
        result.LPtosis = patient.patientleftptosis;
        //2020.10.12  tao
        result.LRedReflex = "";
        result.LRedReflexPer = "";
        for(int i = 0; i < LEN_NYSTAGMUS; i++)
            result.LNystagmus[i] = 0;

        result.RPupil = patient.patientrightpd.toStdString().c_str();
        result.RSE = patient.patientrightse.toStdString().c_str();
        result.RDS = patient.patientrighteyesph.toStdString().c_str();
        result.RDC = patient.patientrighteyecyl.toStdString().c_str();
        result.RAxis = patient.patientrighteyeax.toInt();
        result.RVision = CAlgoInvoker::diopterToVision(patient.patientrighteyesph, patient.patientrighteyecyl, CGlobal::visionNotation.getValue()).toStdString();
        result.RGazeH = patient.patientrighths.toInt();
        result.RGazeV = patient.patientrightvs.toInt();
        result.RPtosis = patient.patientrightptosis;

        result.RRedReflex = "";             //2020.10.12  tao
        result.RRedReflexPer = "";

        for (int i = 0; i < LEN_NYSTAGMUS; i++) {
            result.RNystagmus[i] = 0;
        }

        result.UserName = ((WinDataTrans::isManylinksDataIntf() || CGlobal::isDebugMode || WinDataTrans::isManylinksProtocal) ? DataTransmiter::AuthUserName : "");

        result.ClientInfo.Num = patient.patientid.toStdString();
        result.ClientInfo.Name = patient.patientname.toStdString();
        result.ClientInfo.Sex = patient.patientsex.toStdString();
        result.ClientInfo.BirthDate = patient.getBirthDateStr().toStdString();
        result.ClientInfo.Tel = patient.patientPhone.toStdString();
        result.ClientInfo.WeChat = patient.patientWechat.toStdString();
        result.ClientInfo.Class = patient.patientstuclass.toStdString();
        result.ClientInfo.Address = patient.patientAddress.toStdString();
        result.ClientInfo.comment1 = patient.comment1.toStdString();
        result.ClientInfo.Comment2 = patient.Comment2.toStdString();
        result.HasImage = patient.isNeedImage;
        result.Image = ""/*UpLoadThread::imageBase64Encode(getPreviewImgPath(patient))*/;

        stVisionJudgementRst standard_rst_right, standard_rst_left;
        eyesightstandard::standardCompare(patient, &standard_rst_right, &standard_rst_left);
        result.LGazeExceed.VerticalGaze     = standard_rst_right.verticalGaze || standard_rst_left.verticalGaze;
        result.LGazeExceed.NasalGaze        = standard_rst_right.nasalGaze || standard_rst_left.nasalGaze;
        result.LGazeExceed.BitemporalGaze   = standard_rst_right.bitemporalGaze || standard_rst_left.bitemporalGaze;
        result.LGazeExceed.GazeAsymmetry    = standard_rst_right.gazeAsymmetry || standard_rst_left.gazeAsymmetry;

    }
}

//
void DataTransmiter::CallbackData(enDataSendingError _err_code, std::string& _data)
{
    logDebug(QString("DataTransmiter::CallbackData(): _data = ") + _data.c_str(), CGlobal::LOG_DATATRANS);

    FNeedResend = false;
    if (dataSendingError_No == _err_code)
    {
        // 字符串由 UTF-8 转 本地编码（如果不同）
        // TODO: 暂不处理，Linux 默认使用 UTF-8
        //char* coding = getenv("LANG");

        //
        MLMCommunic response;
        bool parse_succ = response.FromJson(_data);
        if (parse_succ)
        {
            // 检验 MD5
            // TODO:

            //
            FNeedResend = false;

            // 根据成功或失败状态构造返回值
            /*
             * 2021-11-11 协议变更，要兼容两种情况：
             * 旧协议（v1.2.1,v1.2.2）：返回值表示整批数据上传成功或失败。
             * 新协议（v1.2.3）：对于批量数据上传，若全部处理成功，则返回成功，否则返回失败，并将处理失败的被测者编号存入 data 字段里。
             * 2023-10-26 协议变更：
             * 新协议（v1.2.8）：对于批量数据上传，若全部处理成功，则返回成功，否则返回失败，并将处理失败的被测者的 RecordID 存入 data 字段里。
             */
            if (STAT_SUCC == response.stat)
            {
                // 全部成功，则清空失败列表
                if (FListFail) {
                    FListFail->clear();
                } else {
                    logCritical(QString(__PRETTY_FUNCTION__) + ": logic error! FListFail is null", CGlobal::LOG_DATATRANS);
                }

                //
                FProcResult = true;
                FProcMsg = "";
            }
            else if (STAT_FAIL == response.stat)
            {
                /* 若 data 字段的数据符合新协议，则失败列表同步为 data 字段的值，否则认为全部失败，不需处理（失败列表已在发送数据时被初始化为等于上传列表） */
                bool is_new_protocol = false;
                QVector<int> fail_ids;

                // 判断 data 字段数据是否符合新协议，并得到新协议返回的失败列表
                if (response.data.length() > 0) {
                    QString fail_ids_str = QString::fromStdString(response.data);
                    QStringList fail_id_strs = fail_ids_str.split(",");
                    for (auto num : fail_id_strs) {
                        num.replace("\"", "");      // 编号可能被用双引号包起来，需去掉
                        num.replace(" ", "");       // 逗号之后可能有空格，需去掉
                    }
                    if (fail_id_strs.count() > 0) {
                        is_new_protocol = true;       // 到了这里，除非编号有不存在的情况，否则数据合规

                        // TODO: 检查确保 FListFail 不为空，或 FListFail 改为对象？
                        //

                        // 检查应答数据中的失败编号是否存在于上传编号中，若否，则数据格式与协议不符，判断为旧协议（整批失败或成功）
                        int idx;
                        int id;
                        for (int i = 0; i < fail_id_strs.count(); i++) {
                            id = fail_id_strs[i].toInt();
                            idx = FListFail->indexOf(id);

                            // 只要有一个编号对不上，就认为格式不符，属于旧协议（整批失败或成功）
                            if (idx < 0) {
                                is_new_protocol = false;
                                break;
                            }

                            //
                            fail_ids.append(id);
                        }
                    }
                }

                // TODO: 检查确保 FListFail 不为空，或 FListFail 改为对象？
                //

                // 若数据符合新协议，则同步失败列表
                if (is_new_protocol) {
                    FListFail->clear();
                    for (auto id : fail_ids) {
                        FListFail->append(id);
                    }
                }

                //FListFail = NULL;

                //
                FProcResult = false;
                FProcMsg = "Error from server : " + response.msg;
            }
            else
            {
                qDebug() << "sendResult return obj.'stat' not valid, resp_str = \n" << _data.c_str();

                // 全部失败（失败列表已在发送数据时被初始化为等于上传列表，不需处理）

                //
                static constexpr int MAX_LEN = 250;
                QString err_msg = "\"stat\" field in JSON not valid! source:\n";
                if (_data.length() <= MAX_LEN) {
                    err_msg += QString::fromStdString(_data);
                } else {
                    err_msg += QString::fromStdString(_data.substr(0, MAX_LEN)) + "...";
                }
                FProcMsg = err_msg.toStdString();
                FProcResult = false;
            }
        }
        else
        {
            qDebug() << "sendResult resp_str JSON parse failed, resp_str = \n" << _data.c_str();

            static constexpr int MAX_LEN = 250;
            QString err_msg = "Failed to parse JSON in HTTP response! source:\n";
            if (_data.length() <= MAX_LEN) {
                err_msg += QString::fromStdString(_data);
            } else {
                err_msg += QString::fromStdString(_data.substr(0, MAX_LEN)) + "...";
            }
            FProcMsg = err_msg.toStdString();
            FProcResult = false;
        }

        //FProcMsg = _data + "\r\n" + FProcMsg;
    }
    else
    {
        qDebug() << "sendResult return http error: code = " << _err_code << ", resp_str = \n" << _data.c_str();

        FProcMsg = "unknown error(" + std::to_string(_err_code) + "), data = \"" + _data + "\"";
        FProcResult = false;
    }
}

void DataTransmiter::CallbackAuth(enDataSendingError _err_code, std::string& _data)
{
    //logDebug(QString("DataTransmiter::CallbackAuth(): _data = ") + _data.c_str());

    //QTextCodec *codec = QTextCodec::codecForName("GBK");
    //QTextCodec::setCodecForLocale(codec);

    std::cout << "callbackauth:" << _data << std::endl;
    FNeedResend = false;
    if (dataSendingError_No == _err_code)
    {
        // 字符串由 UTF-8 转 本地编码（如果不同）
        // TODO: 暂不处理，Linux 默认使用 UTF-8
        //char* coding = getenv("LANG");

        //
        MLMCommunic response;
        bool parse_succ = response.FromJson(_data);
        if (parse_succ)
        {
            if (STAT_SUCC == response.stat)
            {
                // 读取 Token
                if (!response.data.empty()) {
                    Token token;
                    bool parse_succ = token.FromJson(response.data);
                    if (parse_succ)
                    {
                        FTokenStr = token.TokenStr;
                        FTokenExpire = StrToFloat(token.Expire) / 1000;
                        FProcMsg = "";
                    } else {
                        qWarning() << __PRETTY_FUNCTION__ << ": parse JSON of Token object failed!";
                        FTokenStr = "TockenReplyDataInvalid(stat_is_succ_but_data_parsing_failed)";
                        FTokenExpire = 0;       // 若解析 Token 对象的 JSON 失败，但鉴权接口返回 true，则认为 token 永久有效
                        if (!WinDataTrans::isManylinksDataIntf()) {    // NOTE: 若是万灵云端，则判定成功（万灵云端不使用 tocken 来鉴权，需额外传用户名和密码）
                            FProcMsg = "parsing JSON of Token object failed";
                        }
                    }
                } else {
                    FTokenStr = "(null)";
                    FTokenExpire = 0;       // 若解析 Token 对象的 JSON 失败，但鉴权接口返回 true，则认为 token 永久有效
                    if (!WinDataTrans::isManylinksDataIntf()) {    // NOTE: 若是万灵云端，则判定成功（万灵云端不使用 tocken 来鉴权，需额外传用户名和密码）
                        FProcMsg = "parsing JSON of Token object failed";
                    }
                }

                //
                FProcResult = true;
            }
            else if (STAT_FAIL == response.stat)
            {
                FProcResult = false;
                FProcMsg = "Error from server : " + response.msg;
            }
            else
            {
                FProcResult = false;
                FProcMsg = "response data stat error";
            }
        }
        else
        {
            FProcResult = false;
            FProcMsg = (response.msg.length() > 0) ? response.msg : "response data format error";
        }
    }
    else
    {
        FProcResult = false;
        FProcMsg = _data;
    }
}

void importClientList(int *_count_repeated)     // TODO: 数据的保存不应该放到这个模块
{
    MySQLitePatients *mysql = MySQLitePatients::getInstance();
    std::vector<CPatient> addPats;
    addPats.clear();

    for (int i = 0; i < Clients::mFlen; i++) {
        Client &client = Clients::mItems[i];
        CPatient pat;
        pat.reset();

        pat.patientid       = QString::fromStdString(client.Num);
        pat.patientname     = QString::fromStdString(client.Name);
        pat.patientsex      = QString::fromStdString(client.Sex);
        pat.setBirthDate(Util::strToDate(QString::fromStdString(client.BirthDate)));
        pat.patientstuclass = QString::fromStdString(client.Class);
        pat.patientPhone    = QString::fromStdString(client.Tel);
        pat.patientWechat   = QString::fromStdString(client.WeChat);
        pat.patientAddress  = QString::fromStdString(client.Address);
        pat.comment1        = QString::fromStdString(client.comment1);
        pat.Comment2        = QString::fromStdString(client.Comment2);

        pat.isBatch         = true;     // 从外部导入的被测者，全部算是“批量筛查”的被测者
        //pat.setAgeRange(CAgeRange::getAgeRangeFromBirthdateStr(pat.getBirthDateStr()));

        addPats.push_back(pat);
    }

    mysql->TableBatchAdd(addPats, _count_repeated);

}

//http请求回调函数
void DataTransmiter::CallbackClientList(enDataSendingError _err_code, std::string& _data)
{
    //QTextCodec *codec = QTextCodec::codecForName("GBK");
    //QTextCodec::setCodecForLocale(codec);

    qDebug()<<"-- DataTransmiter::CallbackClient _data: " << _data.data();
    std::cout << _data << std::endl;        // TODO: 在 QtCreator 调试时，用 std::cout 输入 _data 正常，但是用 qDebug() 输出时中文是乱码，为什么？

    FNeedResend = false;
    if (dataSendingError_No == _err_code)
    {
        // 字符串由 UTF-8 转 本地编码（如果不同）
        // TODO: 暂不处理，Linux 默认使用 UTF-8
        //char* coding = getenv("LANG");

        MLMCommunic response;
        bool parse_succ = response.FromJson(_data);     // TODO: 未捕获异常导致若数据格式不合法则程序崩溃？
        if (parse_succ)
        {
            FProcMsg = "";

            if (STAT_SUCC == response.stat) //succ  成功
            {
                // 解析数据
                bool parse_succ = Clients::FromJsons(response.data);

                if (parse_succ)
                {
                    int count_repeated = 0;
                    importClientList(&count_repeated);
                    if (count_repeated > 0) {
                        FProcMsg += (std::string("repeated: ") + std::to_string(count_repeated));
                    }
                }
                else
                {

                }

                FProcResult = true;
            }
            else if (STAT_FAIL == response.stat)    //fail  失败
            {
                FProcResult = false;
                FProcMsg = "Error from server : " + response.msg;
            }
            else
            {
                FProcResult = false;
                FProcMsg = "response data stat error";
            }
        }
        else
        {
            FProcResult = false;
            FProcMsg = (response.msg.length() > 0) ? response.msg : "response data format error";
        }

        FProcMsg = _data + "\r\n" + FProcMsg;
    }
    else
    {
        qDebug()<<"eeee";
        FProcMsg = _data;
        FProcResult = false;
    }
}

void DataTransmiter::CallbackClient(enDataSendingError _err_code, std::string& _data)
{
    logDebug(__PRETTY_FUNCTION__ + QString(": entered ..."), CGlobal::LOG_DATATRANS);

    //QTextCodec *codec = QTextCodec::codecForName("GBK");
    //QTextCodec::setCodecForLocale(codec);

    FNeedResend = false;
    if (dataSendingError_No == _err_code)
    {
        std::cout << "_data: " << _data << std::endl;       // TODO: 为什么 logDebug(QString::fromStdString(_data)) 在 Qt Creator 里输出的中文为乱码？

        // 字符串由 UTF-8 转 本地编码（如果不同）
        // TODO: 暂不处理，Linux 默认使用 UTF-8
        //char* coding = getenv("LANG");

        //
        MLMCommunic response;
        bool parse_succ = response.FromJson(_data);
        if (parse_succ)
        {
            if (STAT_SUCC == response.stat)
            {
                if(FClients==NULL){
                    qDebug()<<"FClients==NULL";
                    return;
                }
                // 解析数据
                bool parse_succ = FClients->FromJson(response.data);

                if (parse_succ)
                {

                }
                else
                {
                    FClients->Clear();
                }

                //
                FProcResult = true;
                FProcMsg = "";
            }
            else if (STAT_FAIL == response.stat)
            {
                FProcResult = false;
                FProcMsg = "Error from server : " + response.msg;
            }
            else
            {
                FProcResult = false;
                FProcMsg = "response data stat error";
            }
        }
        else
        {
            FProcResult = false;
            FProcMsg = (response.msg.length() > 0) ? response.msg : "response data format error";
        }

        FProcMsg = _data + "\r\n" + FProcMsg;
    }
    else
    {
        qDebug()<<"eeee";
        FProcMsg = _data;
        FProcResult = false;
    }
}

void DataTransmiter::CallbackImage(enDataSendingError _err_code, std::string& _data)
{
    FNeedResend = false;
    if (dataSendingError_No == _err_code)
    {
        // 字符串由 UTF-8 转 本地编码（如果不同）
        // TODO: 暂不处理，Linux 默认使用 UTF-8
        //char* coding = getenv("LANG");

        //
        MLMCommunic response;
        bool parse_succ = response.FromJson(_data);
        if (parse_succ)
        {
            if (STAT_SUCC == response.stat)
            {
                //
                FProcResult = true;
                FProcMsg = "";
            }
            else if (STAT_FAIL == response.stat)
            {
                FProcResult = false;
                FProcMsg = "Error from server : " + response.msg;
            }
            else
            {
                FProcResult = false;
                FProcMsg = "response data stat error";
            }
        }
        else
        {
            FProcResult = false;
            FProcMsg = (response.msg.length() > 0) ? response.msg : "response data format error";
        }

        FProcMsg = _data + "\r\n" + FProcMsg;
    }
    else
    {
        FProcMsg = _data;
        FProcResult = false;
    }
}

char * DataTransmiter::GetUrl(char* _url, int _len, std::string _path, bool _add_token)
{
    char* url = _url;
    memset(url, 0, _len);

    const char* protol = (IsUseHttps ? "https://" : "http://");   //是否使用HTTPS
    strcat(url, protol);                    //拼接http或https
    strcat(url, ReceiverAddr.c_str());      //拼接ip或域名

    if (0 != ReceiverPort)
    {
        string port_str = "";
        IntToStr(ReceiverPort, port_str);   //端口
        strcat(url, ":");
        strcat(url, port_str.c_str());      //拼接端口
    }

    if(_path.at(0) != '/')                  //接口路径格式
        _path.insert(_path.begin(),'/');

    strcat(url, _path.c_str());             //拼接接口路径

    if (_add_token && IsNeedAuth)
    {
        string token_param = "?token=" + FTokenStr;
        strcat(url, token_param.c_str());
    }

    // TODO: 有的 token 长度达 200 多个字节，如妇幼协会的视筛系统
    // TODO: 未 UrlEncode，不支持中文和特殊字符
    //int encoded_len;
    //char* url_encoded = url_encode(url, strlen(url), &encoded_len);

    //free(url_encoded);

    return url;
}

// 检查并确保获得合法的鉴权（token）
bool DataTransmiter::CheckAndGetAuth(std::string& _str_msg)
{
    if (connMode_Bluetooth == ConnMode)
        return true;

    //
    bool auth_pass = true;
    if (IsNeedAuth) //身份验证
    {
        if (FTokenExpire > 0)
        {
            time_t now = time(NULL);
            if (now + 120  > FTokenExpire){
                auth_pass = GetAuthToken(_str_msg);
            }

        }
        else if (0 == FTokenExpire)
        {
            if (FTokenStr.length() == 0)
                auth_pass = GetAuthToken(_str_msg);
        }
        else
        {
            auth_pass = GetAuthToken(_str_msg);
        }
    }
    return auth_pass;
}

void DataTransmiter::Clear()
{
    ReceiverAddr = "";
    ReceiverPort = 0;

    PathData = "";
    PathClient = "";
    PathAuth = "";
    PathImage = "";
    IsPostImmediately = false;
    IsUploadImage = false;

    IsNeedAuth = false;

    AuthUserName = "";
    AuthPassword = "";
    AuthUserType = "";
}

// 得到授权对象的 JSON 字符串
void DataTransmiter::GetAuthJson(std::string& _json_str)
{
    Auth auth;
    auth.Name = AuthUserName;   //用户名
    auth.Pwd = AuthPassword;    //密码
    auth.Type = AuthUserType;   //账号类别
    auth.DevType = DataTrans::DEVICE_TYPE;  //设备类型:SSI
    auth.DevCode = DataTrans::DataTransmiter::getDevCode();  //设备编号

    auth.ToJson(_json_str);
}

// DataTransmiter -->
// =============================================================================

MLMCommunic::MLMCommunic()
{
    this->version = PROTOCOL_VERSION;
    this->lang = CGlobal::langCode().toStdString();
    this->softwareVersion = QString("App: %1, Firmware: %2").arg(aboutdevice::getAppVerFull()).arg(CSysInfo::firmwareVersion()).toStdString();
    this->hardwareVersion = QString("MCU: %1").arg(aboutdevice::getStm32VersionStr()).toStdString();
}

//转换为Json格式
void MLMCommunic::ToJson(std::string& _json_str)
{
    StringBuffer str_buf;
    Writer<StringBuffer> writer(str_buf);

    writer.StartObject();

    writer.Key("func");
    writer.String(this->func.c_str());
    writer.Key("stat");
    writer.String(this->stat.c_str());
    writer.Key("data");
    writer.String(this->data.c_str());
    writer.Key("check");
    writer.String(this->check.c_str());
    writer.Key("msg");
    writer.String(this->msg.c_str());
    writer.Key("version");
    writer.String(this->version.c_str());
    writer.Key("stamp");
    writer.String(this->stamp.c_str());

    if (WinDataTrans::isManylinksDataIntf() || CGlobal::isDebugMode || WinDataTrans::isManylinksProtocal)    // NOTE: 目前仅万灵云端支持了此字段，为了避免影响已对接的系统，加此限制
    {
        writer.Key("lang");
        writer.String(this->lang.c_str());
        writer.Key("softwareVersion");
        writer.String(this->softwareVersion.c_str());
        writer.Key("hardwareVersion");
        writer.String(this->hardwareVersion.c_str());
    }

    writer.EndObject();

    _json_str = str_buf.GetString();
}

bool MLMCommunic::FromJson(std::string& _json_str)
{
    this->Clear();

    bool result = false;

    try
    {
        Document d;
        d.Parse<0>(_json_str.c_str());
        ParseErrorCode err_code = d.GetParseError();    //return 0  成功
        if (kParseErrorNone == err_code)
        {
            if (d.HasMember("func") && !d["func"].IsNull() && d["func"].IsString())
                this->func = d["func"].GetString();
            if (d.HasMember("stat") && !d["stat"].IsNull() && d["stat"].IsString())
                this->stat = d["stat"].GetString();
            if (d.HasMember("data") && !d["data"].IsNull() && d["data"].IsString())
                this->data = d["data"].GetString();
            if (d.HasMember("check") && !d["check"].IsNull() && d["check"].IsString())
                this->check = d["check"].GetString();
            if (d.HasMember("msg") && !d["msg"].IsNull() && d["msg"].IsString())
                this->msg = d["msg"].GetString();
            if (d.HasMember("version") && !d["version"].IsNull() && d["version"].IsString())
                this->version = d["version"].GetString();
            if (d.HasMember("stamp") && !d["stamp"].IsNull() && d["stamp"].IsString())
                this->stamp = d["stamp"].GetString();
            if (d.HasMember("lang") && !d["lang"].IsNull() && d["lang"].IsString())
                this->lang = d["lang"].GetString();

            result = true;
        }
        else{
            cout<<"*******\njson analy failed，err_code"<<err_code<<endl;
        }
    }
    catch (exception& ex)
    {
        QString msg = QString("MLMCommunic::FromJson() exception: ") + ex.what();
        logWarning(msg, CGlobal::LOG_DATATRANS);
    }
    catch (...)
    {
        logWarning(QString("MLMCommunic::FromJson() unknown exception: ") + strerror(errno), CGlobal::LOG_DATATRANS);
    }

    return result;
}

void MLMCommunic::Clear()
{
    this->func = "";
    this->stat = "";
    this->data = "";
    this->check = "";
    this->msg = "";
    this->version = "";
    this->stamp = "";
}

void MeasureResult::ToJson(std::string & _json_str)
{
    StringBuffer str_buf;
    Writer<StringBuffer> writer(str_buf);

    this->ToJson(&writer);

    _json_str = str_buf.GetString();
}

void MeasureResult::ToJson(rapidjson::Writer<rapidjson::StringBuffer> *_writer)
{
    _writer->StartObject();

    _writer->Key("DevType");
    _writer->String(this->DevType.c_str());

    _writer->Key("DevCode");
    _writer->String(this->DevCode.c_str());

    _writer->Key("BatchNo");
    _writer->String(this->BatchNo.c_str());

    _writer->Key("RecordID");
    _writer->String(this->RecordID.c_str());

    _writer->Key("AgeRange");
    _writer->String(this->AgeRange.c_str());

    _writer->Key("ClientNum");
    _writer->String(this->ClientNum.c_str());

    _writer->Key("IsClinic");
    _writer->Bool(this->IsClinic);

    _writer->Key("Barcode");
    _writer->String(this->Barcode.c_str());

    _writer->Key("Time");
    _writer->String(this->Time.c_str());    // TODO: 日期格式转换？

    _writer->Key("PD");
    _writer->Int(this->PD);

    _writer->Key("LPupil");
    _writer->String(this->LPupil.c_str());

    _writer->Key("LSE");
    _writer->String(this->LSE.c_str());

    _writer->Key("LDS");
    _writer->String(this->LDS.c_str());

    _writer->Key("LDC");
    _writer->String(this->LDC.c_str());

    _writer->Key("LAxis");
    _writer->Int(this->LAxis);

    _writer->Key("LVision");
    _writer->String(this->LVision.c_str());

    _writer->Key("LGazeH");
    _writer->Int(this->LGazeH);

    _writer->Key("LGazeV");
    _writer->Int(this->LGazeV);

    _writer->Key("LPtosis");
    _writer->Bool(this->LPtosis);

    //-----start 2020.10.12  tao
    if (CGlobal::getIsExternalControl())     //极视互联版
    {
        _writer->Key("LRedReflex");
        _writer->String(this->LRedReflex.c_str());

        _writer->Key("LRedReflexPer");
        _writer->String(this->LRedReflexPer.c_str());

        _writer->Key("LNystagmus");
        _writer->StartArray();
        for(int i = 0; i < LEN_NYSTAGMUS; i++)
            _writer->Int(this->LNystagmus[i]);
        _writer->EndArray();
    }
    //-----end

    _writer->Key("LGazeExceed");
    this->LGazeExceed.ToJson(_writer);

    _writer->Key("RPupil");
    _writer->String(this->RPupil.c_str());

    _writer->Key("RSE");
    _writer->String(this->RSE.c_str());

    _writer->Key("RDS");
    _writer->String(this->RDS.c_str());

    _writer->Key("RDC");
    _writer->String(this->RDC.c_str());

    _writer->Key("RAxis");
    _writer->Int(this->RAxis);

    _writer->Key("RVision");
    _writer->String(this->RVision.c_str());

    _writer->Key("RGazeH");
    _writer->Int(this->RGazeH);

    _writer->Key("RGazeV");
    _writer->Int(this->RGazeV);

    _writer->Key("RPtosis");
    _writer->Bool(this->RPtosis);

    //-----start 2020.10.12  tao
    if (CGlobal::getIsExternalControl())     //极视互联版
    {
        _writer->Key("RRedReflex");
        _writer->String(this->RRedReflex.c_str());

        _writer->Key("RRedReflexPer");
        _writer->String(this->RRedReflexPer.c_str());

        _writer->Key("RNystagmus");
        _writer->StartArray();
        for(int i = 0; i < LEN_NYSTAGMUS; i++)
            _writer->Int(this->RNystagmus[i]);
        _writer->EndArray();
    }
    //-----end

    _writer->Key("RGazeExceed");
    this->RGazeExceed.ToJson(_writer);

    _writer->Key("HasImage");
    _writer->Bool(this->HasImage);

    _writer->Key("ClientInfo");
    this->ClientInfo.ToJson(_writer);

    if (CGlobal::getIsExternalControl())     //极视互联版
    {
        _writer->Key("Image");  //2020.10.12  tao
        _writer->String(this->Image.c_str());   //2020.10.12  tao
    }

    //if (this->UserName.length() > 0 && WinDataTrans::isManylinksDataIntf() || CGlobal::isDebugMode || WinDataTrans::isManylinksProtocal)    // 20240924 版增加的 UserName 长度检查不合理？字段多了不一定有影响，少了反而可能导致某些已对接的用了此字段的或简单解析文本获取数据的对接端出错？(20250514)
    {
        _writer->Key("UserName");                       // 20231026 版增加了此字段，万灵云端需求
        _writer->String(this->UserName.c_str());
    }

    _writer->EndObject();
}

//MeasureResults::MeasureResults()
//{
//    Init(0);
//}

MeasureResults::MeasureResults(int _len)
{
    Init(_len);
}

MeasureResults::~MeasureResults()
{
    Clear();
}

void MeasureResults::Init(int _len)
{
    FLen = _len;
    if (_len > 0)
        Items = new MeasureResult[FLen];
}

void MeasureResults::Clear()
{
    delete []Items;
    Items = NULL;
    FLen = 0;
}

int MeasureResults::Len()
{
    return FLen;
}

void MeasureResults::ToJson(std::string& _json_str)
{
    StringBuffer str_buf;
    Writer<StringBuffer> writer(str_buf);

    writer.StartObject();

    writer.Key("Items");
    writer.StartArray();
    int len = this->Len();
    for (int i = 0; i < len; i++)
    {
        MeasureResult &measure_result = this->Items[i];
        measure_result.ToJson(&writer);
    }
    writer.EndArray();
    writer.EndObject();

    _json_str = str_buf.GetString();
}

void Client::ToJson(std::string & _json_str)
{
    StringBuffer str_buf;
    Writer<StringBuffer> writer(str_buf);

    this->ToJson(&writer);

    _json_str = str_buf.GetString();
}

void Client::ToJson(rapidjson::Writer<rapidjson::StringBuffer>* _writer)
{
    _writer->StartObject();
    _writer->Key("Num");
    _writer->String(this->Num.c_str());
    _writer->Key("Name");
    _writer->String(this->Name.c_str());
    _writer->Key("Sex");
    _writer->String(this->Sex.c_str());
    _writer->Key("BirthDate");
    _writer->String(this->BirthDate.c_str());
    _writer->Key("Tel");
    _writer->String(this->Tel.c_str());
    _writer->Key("WeChat");
    _writer->String(this->WeChat.c_str());
    _writer->Key("Class");
    _writer->String(this->Class.c_str());
    _writer->Key("Address");
    _writer->String(this->Address.c_str());
    _writer->Key("comment1");
    _writer->String(this->comment1.c_str());
    _writer->Key("Comment2");
    _writer->String(this->Comment2.c_str());
    _writer->EndObject();
}

bool Client::FromJson(const string &_json_str)
{
    this->Clear();

    bool result = false;

    Document d;
    d.Parse<0>(_json_str.c_str());
    ParseErrorCode err_code = d.GetParseError();
    if (kParseErrorNone == err_code)
    {
        FromJson(d);

        result = true;
    }

    return result;
}

void Client::FromJson(rapidjson::Value & _document)
{
    if (_document.HasMember("Num") && !_document["Num"].IsNull() && _document["Num"].IsString())
        this->Num = _document["Num"].GetString();
    if (_document.HasMember("Name") && !_document["Name"].IsNull() && _document["Name"].IsString())
        this->Name = _document["Name"].GetString();
    if (_document.HasMember("Sex") && !_document["Sex"].IsNull() && _document["Sex"].IsString())
        this->Sex = _document["Sex"].GetString();
    if (_document.HasMember("BirthDate") && !_document["BirthDate"].IsNull() && _document["BirthDate"].IsString())
        this->BirthDate = _document["BirthDate"].GetString();
    if (_document.HasMember("Tel") && !_document["Tel"].IsNull() && _document["Tel"].IsString())
        this->Tel = _document["Tel"].GetString();
    if (_document.HasMember("WeChat") && !_document["WeChat"].IsNull() && _document["WeChat"].IsString())
        this->WeChat = _document["WeChat"].GetString();
    if (_document.HasMember("Class") && !_document["Class"].IsNull() && _document["Class"].IsString())
        this->Class = _document["Class"].GetString();
    if (_document.HasMember("Address") && !_document["Address"].IsNull() && _document["Address"].IsString())
        this->Address = _document["Address"].GetString();
    if (_document.HasMember("comment1") && !_document["comment1"].IsNull() && _document["comment1"].IsString())
        this->comment1 = _document["comment1"].GetString();
    if (_document.HasMember("Comment2") && !_document["Comment2"].IsNull() && _document["Comment2"].IsString())
        this->Comment2 = _document["Comment2"].GetString();
}

void Client::FromObj(Client &_client)
{
    this->Num = _client.Num;
    this->Name = _client.Name;
    this->Sex = _client.Sex;
    this->BirthDate = _client.BirthDate;
    this->Class = _client.Class;
    this->Tel = _client.Tel;
    this->WeChat = _client.WeChat;
    this->Address = _client.Address;
    this->comment1 = _client.comment1;
    this->Comment2 = _client.Comment2;
}

void Client::Clear()
{
    this->Num = "";
    this->Name = "";
    this->Sex = "";
    this->BirthDate = "";
    this->Tel = "";
    this->WeChat = "";
    this->Class = "";
    this->Address = "";
    this->comment1 = "";
    this->Comment2 = "";
}

//void Client::CloneFrom(Client & _client)
//{
//    this->Num = _client.Num;
//    this->Name = _client.Name;
//    this->Sex = _client.Sex;
//    this->BirthDate = _client.BirthDate;
//    this->Tel = _client.Tel;
//    this->WeChat = _client.WeChat;
//    this->Class = _client.Class;
//    this->Address = _client.Address;
//    this->comment1 = _client.comment1;
//    this->Comment2 = _client.Comment2;
//}

Client *Clients::mItems = NULL;     // 受测者身份信息数组
int Clients::mFlen = 0;
Clients::Clients()
{
    Init(0);
}


Clients::Clients(int _len)
{
    Init(_len);
}

Clients::~Clients()
{
    Clear();
}

void Clients::Init(int _len)
{
    FLen = _len;
    if (_len > 0)
        Items = new Client[FLen];
    else
        Items = NULL;
}

void Clients::Clear()
{
    if (Items) {
        delete []Items;
        Items = NULL;
    }
    FLen = 0;
}

int Clients::Len()
{
    return FLen;
}

//单个Items项
bool Clients::FromJson(std::string & _json_str)
{
    this->Clear();
    bool result = false;

    Document d;
    d.Parse<0>(_json_str.c_str());
    ParseErrorCode err_code = d.GetParseError();    //获取接口数据
    if (kParseErrorNone == err_code)
    {
        if (d.HasMember("Items") && !d["Items"].IsNull() && d["Items"].IsArray())
        {
            Value &items = d["Items"];
            if (items.Size() > 0)
            {
                FLen = items.Size();
                Items = new Client[FLen];
                for (int i = 0; i < FLen; i++)
                {
                    Value &v_client = items[i];
                    assert(v_client.IsObject());
                    Client &client = Items[i];      //引用
                    client.FromJson(v_client);      //Client成员赋值
                    qDebug()<<"get client num:"<<QString::fromStdString(client.Num);
                }
            }
        }

        result = true;
    }
    return result;
}

//多个Items项（批量导入所用）
bool Clients::FromJsons(std::string & _json_str)
{
    //
    bool result = false;

    Document d;
    d.Parse<0>(_json_str.c_str());
    ParseErrorCode err_code = d.GetParseError();    //获取接口数据
    if (kParseErrorNone == err_code)
    {
        if (d.IsObject() && d.HasMember("Items") && !d["Items"].IsNull() && d["Items"].IsArray())
        {
            Value &items = d["Items"];
            if (items.Size() > 0)
            {
                Clients::mFlen = items.Size();
                qDebug()<<"--get item size:"<<Clients::mFlen;
                mItems = new Client[Clients::mFlen];    //实例化受测者信息数组
                for (int i = 0; i < Clients::mFlen; i++)
                {
                    Value &v_client = items[i];
                    assert(v_client.IsObject());
                    Client &client = mItems[i];     //引用
                    client.FromJson(v_client);      //Client成员赋值
                    qDebug()<<"get client num:"<<QString::fromStdString(client.Num)<<"  name"<<QString::fromStdString(client.Name)<<"  sex"<<QString::fromStdString(client.Sex);
                }
            }

            result = true;
        } else {
            result = false;
        }
    }

    //
    return result;
}

bool ClientOfBatch::FromCSV(const string &_csv_str, std::string *_msg)
{
    this->Clear();

    string str_csv = TrimRN(_csv_str);

    string::size_type prior_start = 0, prior_end = -1;
    string val_str;

    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->Num            = val_str;
    else { if (NULL != _msg) _msg->assign("解析“筛查号”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->Name           = val_str;
    else { if (NULL != _msg) _msg->assign("解析“姓名”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->Sex            = val_str;
    else { if (NULL != _msg) _msg->assign("解析“性别”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->BirthDate      = val_str;
    else { if (NULL != _msg) _msg->assign("解析“生日”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->Class          = val_str;
    else { if (NULL != _msg) _msg->assign("解析“班级/籍贯”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->Tel            = val_str;
    else { if (NULL != _msg) _msg->assign("解析“电话”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->WeChat         = val_str;
    else { if (NULL != _msg) _msg->assign("解析“微信/民族”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->Address        = val_str;
    else { if (NULL != _msg) _msg->assign("解析“地址”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->comment1       = val_str;
    else { if (NULL != _msg) _msg->assign("解析“备注1”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->Comment2       = val_str;
    else { if (NULL != _msg) _msg->assign("解析“备注2”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->BatchNo        = val_str;
    else { if (NULL != _msg) _msg->assign("解析“批次编号”字段失败"); return false; }
    if (CSVGetNextField(str_csv, prior_start, prior_end, val_str)) this->ClinicNo       = val_str;
    else { if (NULL != _msg) _msg->assign("解析“诊疗号”字段失败"); return false; }

    //
    return true;
}

void ClientOfBatch::FromObj(ClientOfBatch &_client)
{
    Client::FromObj(_client);
    this->BatchNo = _client.BatchNo;
    this->ClinicNo = _client.ClinicNo;
    this->Barcode = _client.Barcode;
}

void ClientOfBatch::FromObj(Client &_client, bool _clear)
{
    if (_clear)
        Clear();
    Client::FromObj(_client);
}

void ClientOfBatch::Clear()
{
    Client::Clear();
    this->BatchNo = "";
    this->ClinicNo = "";
    this->Barcode = "";
}

/* 解析二维码的数据，并将其中的数据赋值给自身。
 * 返回二维码类型：1-JSON, 2-CSV, 3-仅被测者编号；返回 -1 表示解析失败。
 */
enQrCodeType ClientOfBatch::FromBarcode(const string &_str, string &_msg)
{
    this->Clear();

    _msg = "";
    enQrCodeType code_type = qrCodeType_Unknown;
    try
    {
        if ('{' == _str.at(0)) {
            this->FromJson(_str);
            code_type = qrCodeType_JSON;
        } else if (string::npos != _str.find_first_of(',', 0)) {
            this->FromCSV(_str);
            code_type = qrCodeType_CSV;
        } else {
            this->Num = _str;
            code_type = qrCodeType_Number;
        }
    }
    catch (exception& ex)
    {
        _msg = "exception: " + string(ex.what());
        code_type = qrCodeType_Unknown;
    }

    return code_type;
}

bool ClientOfBatch::FromJson(const std::string & _json_str)
{
    this->Clear();

    bool result = false;

    Document d;
    d.Parse<0>(_json_str.c_str());
    ParseErrorCode err_code = d.GetParseError();
    if (kParseErrorNone == err_code)
    {
        Client::FromJson(d);

        if (d.HasMember("BatchNo") && !d["BatchNo"].IsNull() && d["BatchNo"].IsString())
            this->BatchNo = d["BatchNo"].GetString();
        if (d.HasMember("ClinicNo") && !d["ClinicNo"].IsNull() && d["ClinicNo"].IsString())
            this->ClinicNo = d["ClinicNo"].GetString();

        result = true;
    }

    return result;
}

void Auth::ToJson(std::string& _json_str)
{
    StringBuffer str_buf;
    Writer<StringBuffer> writer(str_buf);

    writer.StartObject();

    writer.Key("Name");
    writer.String(this->Name.c_str());
    writer.Key("Pwd");
    writer.String(this->Pwd.c_str());
    writer.Key("Type");
    writer.String(this->Type.c_str());
    writer.Key("DevType");
    writer.String(this->DevType.c_str());
    writer.Key("DevCode");
    writer.String(this->DevCode.c_str());

    writer.EndObject();

    _json_str = str_buf.GetString();
}

bool Token::FromJson(std::string & _json_str)
{
    this->Clear();

    bool result = false;

    Document d;
    d.Parse<0>(_json_str.c_str());
    ParseErrorCode err_code = d.GetParseError();
    if (kParseErrorNone == err_code)
    {
        if (d.HasMember("TokenStr") && !d["TokenStr"].IsNull() && d["TokenStr"].IsString())
            this->TokenStr = d["TokenStr"].GetString();
        if (d.HasMember("Expire") && !d["Expire"].IsNull() && d["Expire"].IsString())
            this->Expire = d["Expire"].GetString();

        result = true;
    }

    return result;
}

void Token::Clear()
{
    this->TokenStr = "";
    this->Expire = "";
}

bool GazeExceed::IsExceed()
{
    return this->VerticalGaze || this->NasalGaze || this->BitemporalGaze || this->GazeAsymmetry;
}

void GazeExceed::ToJson(rapidjson::Writer<StringBuffer> *_writer)
{
    _writer->StartObject();
    _writer->Key("VerticalGaze");
    _writer->Bool(this->VerticalGaze);
    _writer->Key("NasalGaze");
    _writer->Bool(this->NasalGaze);

    // 20240524 变更，之前的字段名： _writer->Key("BitamporalGaze");
    {
        _writer->Key("BitemporalGaze");
        _writer->Bool(this->BitemporalGaze);
    }

    _writer->Key("GazeAsymmetry");
    _writer->Bool(this->GazeAsymmetry);
    _writer->EndObject();
}

// 数据拷贝，从 Client 到 CPatient
void client2Patient(Client &_client, CPatient &_patient)
{
    //
    _patient.patientid = QString::fromStdString(_client.Num);
    _patient.patientname = QString::fromStdString(_client.Name);
    QString sex = QString::fromStdString(_client.Sex);
    if(sex == "M")
        _patient.patientsex = "M";
    else if(sex == "F")
        _patient.patientsex = "F";
    else
        _patient.patientsex = "";

    _patient.setAgeRange(ageRange_Invalid);
    _patient.setBirthDateStr(QString::fromStdString(_client.BirthDate));
    _patient.patientstuclass = QString::fromStdString(_client.Class);
    _patient.patientPhone = QString::fromStdString(_client.Tel);
    _patient.patientWechat = QString::fromStdString(_client.WeChat);
    _patient.patientAddress = QString::fromStdString(_client.Address);
    _patient.comment1 = QString::fromStdString(_client.comment1);
    _patient.Comment2 = QString::fromStdString(_client.Comment2);

    _patient.isBatch = true;

    // 若生日有效，则根据生日设置年龄段
    QDate birth_date = _patient.getBirthDate();
    if (birth_date.isValid()) {
        enAgeRange age_range = CAgeRange::getAgeRangeFromBirthdate(birth_date);
        _patient.setAgeRange(age_range);
    }
}

// 数据拷贝，从 ClientOfBatch 到 CPatient
void batchClient2Patient(ClientOfBatch &_batch_client, CPatient &_patient)
{
    //
    client2Patient((Client &)_batch_client, _patient);
    _patient.barcodeData = QString::fromStdString(_batch_client.Barcode);
    _patient.batchNo = QString::fromStdString(_batch_client.BatchNo);

    //
    _patient.isBatch = (_batch_client.ClinicNo.length() == 0);
    if (!_patient.isBatch) {
        _patient.patientid = QString::fromStdString(_batch_client.ClinicNo);
    }
}

}
