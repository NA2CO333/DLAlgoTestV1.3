#ifndef CLANDEVFINDERCLIENT_H
#define CLANDEVFINDERCLIENT_H

#include <string>

#include "landevfinderdefs.h"

/* ============================================================================
 * 客户端负责发送设备查询广播，并解析服务端的应答得到服务端的主业务通信地址和端口。
 * 用法：
 * 1、设置查询消息发送端口（可选，若不设置，则使用默认端口）。
 * 2、设置广播地址（若不设置，部分路由器下服务端会收不到广播数据）。
 * 3、设置查找完成后的回调（可选，若使用非阻塞模式，则不需设置）。
 * 4、调用“开始查找”函数。
 * 5、调用“获取查找结果”函数。
 */

//
typedef void (*funcFindFinishedCallback)();

// “LAN 设备发现”的客户端
class CLanDevFinderClient           // TODO: rename to CLanDevDiscoveryClient ?
{
public:
    static CLanDevFinderClient *getInstance();
    ~CLanDevFinderClient();

    // 设置请求消息的发送端口
    void setPortReq(int _port_req);

    // 设置广播地址
    void setBroadcastAddr(const std::string &_broadcast_addr);

    // 设置接收超时毫秒数（每次查找总时间的最大值约等于：每次接收超时毫秒数 * 重试次数）
    void setRecvTimeout(int _msecs);

    // 设置重试次数
    void setRetryTimes(int _times);

    // 设置通信消息关键字
    void setMessageKeyword(std::string _keyword);

    // 设置查找完成后的回调（若是希望阻塞调用，即查找完成后再返回，则不需要设置）。注意：这个回调是在子线程里调用的，非主线程，可能需要增加信号槽将后续处理转移到主线程执行。
    void setFindFinishedCallback(funcFindFinishedCallback _func);

    /**
     * @brief 开始查找
     * @param _is_block : 是否阻塞模式。若是阻塞，则本函数在查找完成后才返回，且不会执行回调。若非阻塞，则本函数立即返回，并在查找完成后执行回调。
     */
    void startFind(bool _is_block = true);

    /**
     * @brief 获取查找结果
     * @param _addr     【输出参数】通信地址（主业务的）
     * @param _port     【输出参数】通信端口（主业务的）
     * @param _index    索引号。若找到了多个，根据索引号指定获得第几个服务端连接参数
     * @return 是否成功（若此前最后一次查找成功，则成功，否则失败）
     */
    bool getFindResult(std::string &_addr, int &_port, unsigned int _index = 0);

    /**
     * @brief 获取查找结果的个数（支持在同一个局域网有多台服务端在运行的情景）
     * @return
     */
    int getResultCount();

protected:
    CLanDevFinderClient();

    static CLanDevFinderClient *instance;

    int portServer = LAN_DEV_FINDER_DEFAULT_SVR_RECV_PORT;      // 服务端监听端口
    int portClient = LAN_DEV_FINDER_DEFAULT_CLT_RECV_PORT;      // 客户端监听端口
    funcFindFinishedCallback callbackFindFinished = 0;
    pthread_t threadId = 0;
    bool isBlock = false;
    std::string broadcastAddr = "";
    std::string messageKeyword = LAN_DEV_FINDER_DEFAULT_KEY;
    std::string messageFormatRequst = LAN_DEV_FINDER_REQUEST_FORMAT;
    int recvTimeout = 0;        // 接收超时（毫秒）
    int retryTimes = 0;         // 重试次数

    void reset();
    static void *procRequest(void *);

};

#endif // CLANDEVFINDERCLIENT_H
