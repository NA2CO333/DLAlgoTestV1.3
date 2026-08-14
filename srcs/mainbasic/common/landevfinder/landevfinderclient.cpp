#include "landevfinderclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <vector>
#include <sys/time.h>

//#include <thread>
#include <pthread.h>

// 默认的接收超时（毫秒）
#define RECV_TIMEOUT     500

// 默认的发送超时（毫秒）
#define SEND_TIMEOUT     1000

// 默认的查询的重试次数
#define SEND_TIMES       3

///====================================================================================
/// types

struct stLanDevInfo {
    std::string addr;
    int port;
};

std::vector<stLanDevInfo> results;

///====================================================================================
/// class CLanDevFinderClient

//
CLanDevFinderClient *CLanDevFinderClient::instance = nullptr;

//
CLanDevFinderClient::CLanDevFinderClient()
{
    recvTimeout = RECV_TIMEOUT;
    retryTimes = SEND_TIMES;

}

CLanDevFinderClient *CLanDevFinderClient::getInstance()
{
    if (!instance) {
        instance = new CLanDevFinderClient;
    }
    return instance;
}

CLanDevFinderClient::~CLanDevFinderClient()
{

}

void CLanDevFinderClient::setPortReq(int _port_req)
{
    portServer = _port_req;
}

void CLanDevFinderClient::setBroadcastAddr(const std::string &_broadcast_addr)
{
    broadcastAddr = _broadcast_addr;
}

void CLanDevFinderClient::setRecvTimeout(int _msecs)
{
    recvTimeout = _msecs;
}

void CLanDevFinderClient::setRetryTimes(int _times)
{
    retryTimes = _times;
}

void CLanDevFinderClient::setMessageKeyword(std::string _keyword)
{
    //
    messageKeyword = _keyword;

    //
    messageFormatRequst = LAN_DEV_FINDER_REQUEST_FORMAT;
    std::size_t pos_key = messageFormatRequst.find(LAN_DEV_FINDER_DEFAULT_KEY);
    messageFormatRequst.replace(pos_key, strlen(LAN_DEV_FINDER_DEFAULT_KEY), messageKeyword.c_str());

}

void CLanDevFinderClient::setFindFinishedCallback(funcFindFinishedCallback _func)
{
    callbackFindFinished = _func;
}

void CLanDevFinderClient::reset()
{

}

//
void CLanDevFinderClient::startFind(bool _is_block)
{
    //
    isBlock = _is_block;

    //
    if (isBlock) {
        procRequest(NULL);
    } else {
        //
        if (threadId) {
            pthread_cancel(threadId);       // TODO: 这样结束线程可能导致的问题？
            threadId = 0;
        }

        //
        //std::thread thread_request(&CLanDevFinderClient::procRequest, this);
        //thread_request.detach();

        pthread_create(&threadId, NULL, &CLanDevFinderClient::procRequest, NULL);

    }
}

bool CLanDevFinderClient::getFindResult(std::string &_addr, int &_port, unsigned int _index)
{
    if (_index >= 0 && _index < results.size()) {
        _addr = results.at(_index).addr;
        _port = results.at(_index).port;

        return true;
    } else {
        return false;
    }
}

int CLanDevFinderClient::getResultCount()
{
    return results.size();
}

//
void *CLanDevFinderClient::procRequest(void *)
{
    CLanDevFinderClient *client = CLanDevFinderClient::getInstance();

    //
    client->reset();

    results.clear();

    //
    if (0 == client->portServer) {
        client->portServer = LAN_DEV_FINDER_DEFAULT_SVR_RECV_PORT;
    }
    if (0 == client->portClient) {
        client->portClient = LAN_DEV_FINDER_DEFAULT_CLT_RECV_PORT;
    }

    //
    int sock = socket(PF_INET, SOCK_DGRAM, 0);

    // 设置 socket 得到广播权限
    int so_brd = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *)&so_brd, sizeof(so_brd));

    // 设置接收超时
    int rcv_timeout_secs = client->recvTimeout / 1000;
    int rcv_timeout_usecs = (client->recvTimeout % 1000) * 1000;
    timeval timeout_recv = {rcv_timeout_secs, rcv_timeout_usecs};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_recv, sizeof(timeval));

    // 设置发送超时
    int snd_timeout_secs = SEND_TIMEOUT / 1000;
    int snd_timeout_usecs = (SEND_TIMEOUT % 1000) * 1000;
    timeval timeout_send = {snd_timeout_secs, snd_timeout_usecs};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout_send, sizeof(timeval));

    // 接收和发送数据缓冲区
    char data_send[LAN_DEV_FINDER_REQUEST_LEN];
    char data_recv[LAN_DEV_FINDER_RESPONSE_LEN];

    // 确定广播地址
    in_addr_t b_addr = INADDR_BROADCAST;
    std::string b_addr_str = client->broadcastAddr;
    if (b_addr_str.length() > 0) {
        b_addr = inet_addr(b_addr_str.c_str());
    }

    // 广播地址定义（端口是服务端的接收端口）
    struct sockaddr_in addr_broad;
    memset(&addr_broad, 0, sizeof(addr_broad));
    addr_broad.sin_family = AF_INET;
    addr_broad.sin_addr.s_addr = b_addr;
    addr_broad.sin_port = htons(client->portServer);

    // 接收地址定义（客户端的）
    struct sockaddr_in addr_recv;
    memset(&addr_recv, 0, sizeof(addr_recv));
    addr_recv.sin_family = AF_INET;
    addr_recv.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_recv.sin_port = htons(client->portClient);

    //
    do {
        // 绑定接收地址和端口
        int ret_bind = bind(sock, (struct sockaddr *)&addr_recv, sizeof(addr_recv));
        if (-1 == ret_bind) {
            break;
        }

        //
        for (int i = 0; i < client->retryTimes; i++)
        {
            // 发送查找消息
            snprintf(data_send, LAN_DEV_FINDER_REQUEST_LEN, client->messageFormatRequst.c_str(), i + 1);
            sendto(sock, data_send, strlen(data_send), 0, (struct sockaddr *)&addr_broad, sizeof(addr_broad));

            // 接收应答数据并解析
            timeval tv_begin;
            int ret_tv_begin = gettimeofday(&tv_begin, NULL);       // TODO: 改用 clock_gettime() ？
            if (0 != ret_tv_begin) {
                break;
            }

            long usec_diff_max = client->recvTimeout * 1000;
            long usec_diff = 0;
            do {
                memset(data_recv, 0, LAN_DEV_FINDER_RESPONSE_LEN);
                socklen_t len_addr = sizeof(addr_recv);    // 这个须初始化
                int recv_len = recvfrom(sock, data_recv, LAN_DEV_FINDER_RESPONSE_LEN, 0, (sockaddr *)&addr_recv, &len_addr);

                //
                if (recv_len > 0) {
                    std::cout << "data received: src addr = " << inet_ntoa(addr_recv.sin_addr)
                              << ", src_port = " << ntohs(addr_recv.sin_port)
                              << ", data len = " << recv_len
                              << std::endl;

                    //
                    std::string str_recv = data_recv;

                    // 解析
                    std::string::size_type pos_key = str_recv.find(client->messageKeyword + LAN_DEV_FINDER_KEY_SERVER);
                    if (std::string::npos != pos_key) {
                        std::string::size_type pos_1 = str_recv.find(':');
                        if (std::string::npos != pos_1) {
                            std::string::size_type pos_2 = str_recv.find(',', pos_1 + 1);
                            if (std::string::npos != pos_2) {
                                std::string::size_type pos_3 = str_recv.find('}', pos_2 + 1);
                                if (std::string::npos != pos_3) {
                                    std::string str_addr  = str_recv.substr(pos_1 + 1, pos_2 - pos_1 - 1);
                                    if (' ' == str_addr[0]) {
                                        str_addr.erase(0, 1);
                                    }
                                    std::string addr = str_addr;

                                    std::string str_port = str_recv.substr(pos_2 + 1, pos_3 - pos_2 - 1);
                                    int port = atoi(str_port.c_str());

                                    bool is_exists = false;
                                    for (int i = results.size() - 1; i >= 0; i--) {
                                        if (results.at(i).addr == addr && results.at(i).port == port) {
                                            is_exists = true;
                                            break;
                                        }
                                    }

                                    if (!is_exists) {
                                        stLanDevInfo dev_info;
                                        dev_info.addr = addr;
                                        dev_info.port = port;

                                        results.push_back(dev_info);
                                    }
                                }
                            }
                        }
                    }
                }

                //
                timeval tv_curr;
                int ret_tv_curr = gettimeofday(&tv_curr, NULL);         // TODO: 改用 clock_gettime() ？
                if (0 != ret_tv_curr) {
                    break;
                }
                usec_diff = (tv_curr.tv_sec * 1000000 + tv_curr.tv_usec - tv_begin.tv_sec * 1000000 - tv_begin.tv_usec);
            } while (usec_diff < usec_diff_max);
        }
    } while (false);

    //
    close(sock);

    // 回调
    if (!client->isBlock) {
        if (client->callbackFindFinished) {
            client->callbackFindFinished();
        }
    }

    //
    return NULL;
}
