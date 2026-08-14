#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <QObject>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <asm/types.h>
#include <netinet/ether.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdlib.h> 
//#include <error.h>
#include <net/route.h>
#include <dirent.h>

#include <QDebug>
#include <QObject>
#define READ_BUF_SIZE   1024

struct NetworkInfo{
    QString Name;
    QString Ip;
    QString Mac;
    QString NetMask;
    QString Gateway;
    QString Dns1;
    QString Dns2;
    void clear(){
        Ip = "";
        Mac = "";
        NetMask = "";
        Gateway = "";
        Dns1 = "";
        Dns2 = "";
    }
};

#define BUFSIZE 8192
struct route_info{
    struct in_addr dstAddr;
    struct in_addr  srcAddr;
    struct in_addr  gateWay;
    char ifName[IF_NAMESIZE];
};

static QString hexToQString(quint8 *dsc, int len)
{
    QString buf;
    for(int i = 0; i < len; i++){
        if((dsc[i] >> 4) >= 0 && (dsc[i] >> 4) <= 9)
            buf.append(((dsc[i] >> 4)&0x0f) + '0');
        else buf.append(((dsc[i] >> 4) &0x0f )+ 'a' - 10  );
        if((dsc[i] & 0x0f) >= 0 && (dsc[i] & 0x0f) <= 9)
            buf.append((dsc[i] & 0x0f) + '0');
        else buf.append((dsc[i] & 0x0f) + 'a' - 10);
        if(i < len -1)buf.append(':');
    }
    return buf;
}

static void charToHex(char *src, quint8 *dsc, int len)
{
    for(int i = 0; i < len; i++){
        if(src[i*2] >= '0' && src[i*2] <= '9'){
            dsc[i] = dsc[i] & 0x00;
            dsc[i] += (src[i*2]-'0')<<4;
        }else{
            dsc[i] = dsc[i] & 0x00;
            dsc[i] += (src[i*2]-'a' + 10)<<4;
        }
        if(src[i*2+1] >= '0' && src[i*2+1] <= '9'){
            dsc[i] += (src[i*2+1]-'0');
        }else{
            dsc[i] += (src[i*2+1]-'a' + 10 );
        }
    }
}

static int readNlSock(int sockFd, char *bufPtr, int seqNum, int pId)
{
    struct nlmsghdr *nlHdr;
    int readLen = 0, msgLen = 0;
    do
    {

        if((readLen = recv(sockFd, bufPtr, BUFSIZE - msgLen, 0)) < 0){
            perror("SOCK READ: ");
            return -1;
        }

        nlHdr = (struct nlmsghdr *)bufPtr;

        if((NLMSG_OK(nlHdr, readLen) == 0) || (nlHdr->nlmsg_type == NLMSG_ERROR))
        {
            perror("Error in recieved packet");
            return -1;
        }


        if(nlHdr->nlmsg_type == NLMSG_DONE)
        {
            break;
        }
        else
        {

            bufPtr += readLen;
            msgLen += readLen;
        }

        if((nlHdr->nlmsg_flags & NLM_F_MULTI) == 0)
        {

            break;
        }
    } while((nlHdr->nlmsg_seq != seqNum) || (nlHdr->nlmsg_pid != pId));
    return msgLen;
}

static void parseRoutes(struct nlmsghdr *nlHdr, struct route_info *rtInfo,char *gateway)
{
    struct rtmsg *rtMsg;
    struct rtattr *rtAttr;
    int rtLen;
    char *tempBuf = NULL;
    tempBuf = (char *)malloc(100);
    rtMsg = (struct rtmsg *)NLMSG_DATA(nlHdr);

    if((rtMsg->rtm_family != AF_INET) || (rtMsg->rtm_table != RT_TABLE_MAIN))
        return;

    rtAttr = (struct rtattr *)RTM_RTA(rtMsg);
    rtLen = RTM_PAYLOAD(nlHdr);
    for(;RTA_OK(rtAttr,rtLen);rtAttr = RTA_NEXT(rtAttr,rtLen)){
        switch(rtAttr->rta_type) {
        case RTA_OIF:
            if_indextoname(*(int *)RTA_DATA(rtAttr), rtInfo->ifName);
            break;
        case RTA_GATEWAY:
            rtInfo->gateWay = *(struct in_addr *)RTA_DATA(rtAttr);
            break;
        case RTA_PREFSRC:
            rtInfo->srcAddr = *(struct in_addr *)RTA_DATA(rtAttr);
            break;
        case RTA_DST:
            rtInfo->dstAddr = *(struct in_addr *)RTA_DATA(rtAttr);
            break;
        }
    }
    if (strstr((char *)inet_ntoa(rtInfo->dstAddr), "0.0.0.0"))
        sprintf(gateway, (char *)inet_ntoa(rtInfo->gateWay));
    free(tempBuf);
    return;
}

static int get_gateway(char *gateway)
{
    struct nlmsghdr *nlMsg;
    struct rtmsg *rtMsg;
    struct route_info *rtInfo;
    char msgBuf[BUFSIZE];

    int sock, len, msgSeq = 0;
    //char buff[1024];

    if((sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0)
        perror("Socket Creation: ");


    memset(msgBuf, 0, BUFSIZE);


    nlMsg = (struct nlmsghdr *)msgBuf;
    rtMsg = (struct rtmsg *)NLMSG_DATA(nlMsg);


    nlMsg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)); // Length of message.
    nlMsg->nlmsg_type = RTM_GETROUTE; // Get the routes from kernel routing table .

    nlMsg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST; // The message is a request for dump.
    nlMsg->nlmsg_seq = msgSeq++; // Sequence of the message packet.

    nlMsg->nlmsg_pid = getpid(); // PID of process sending the request.
    //nlMsg->nlmsg_pid = 0;     // TODO: 若用 pid，若别的地方也这么写，则冲突

    if(send(sock, nlMsg, nlMsg->nlmsg_len, 0) < 0){
        printf("Write To Socket Failed...\n");
        return -1;
    }


    if((len = readNlSock(sock, msgBuf, msgSeq, getpid())) < 0) {
        printf("Read From Socket Failed...\n");
        return -1;
    }

    rtInfo = (struct route_info *)malloc(sizeof(struct route_info));
    // ADDED BY BOB

    for(;NLMSG_OK(nlMsg,len);nlMsg = NLMSG_NEXT(nlMsg,len)){
        memset(rtInfo, 0, sizeof(struct route_info));
        parseRoutes(nlMsg, rtInfo,gateway);
    }
    free(rtInfo);
    close(sock);
    return 0;
}

static bool getLocalInfo(NetworkInfo *network)
{
    bool flag = false;
    int fd;
    int interfaceNum = 0;
    struct ifreq buf[16];
    struct ifconf ifc;
    struct ifreq ifrcopy;
    char mac[24] = {0};
    char ip[32] = {0};
    char gateway[255]={0};
    char subnetMask[32] = {0};
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        // close(fd);
        return flag;
    }
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t)buf;

    if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
    {
        interfaceNum = ifc.ifc_len / sizeof(struct ifreq);
        while (interfaceNum-- > 0)
        {
            QString name = buf[interfaceNum].ifr_ifrn.ifrn_name;
            if(!name.contains(network->Name))continue;
            ifrcopy = buf[interfaceNum];
            if (ioctl(fd, SIOCGIFFLAGS, &ifrcopy))
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return flag;
            }
            //get the mac of this interface
            if (!ioctl(fd, SIOCGIFHWADDR, (char *)(&buf[interfaceNum])))
            {
                memset(mac, 0, sizeof(mac));
                network->Mac = hexToQString((quint8 *)buf[interfaceNum].ifr_hwaddr.sa_data, 6);
            }
            else
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return flag;
            }
            //get the IP of this interface
            if (!ioctl(fd, SIOCGIFADDR, (char *)&buf[interfaceNum]))
            {
                snprintf(ip, sizeof(ip), "%s",
                         (char *)inet_ntoa(((struct sockaddr_in *)&(buf[interfaceNum].ifr_addr))->sin_addr));
                //printf("device ip: %s\n", ip);        //屏蔽,不然查询会一直打印IP 2020.6.3
                network->Ip = ip;
            }
            else            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return flag;
            }
            //get the subnet mask of this interface
            if (!ioctl(fd, SIOCGIFNETMASK, &buf[interfaceNum]))            {
                snprintf(subnetMask, sizeof(subnetMask), "%s",
                         (char *)inet_ntoa(((struct sockaddr_in *)&(buf[interfaceNum].ifr_netmask))->sin_addr));
                network->NetMask = subnetMask;
                flag = true;
            }            else
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                return flag;
            }
        }
    }
    else
    {
        printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
        close(fd);
        return flag;
    }
    close(fd);
    if(get_gateway(gateway) == 0){
        network->Gateway = gateway;
    }
    return flag;
}

static bool setLocalInfo(NetworkInfo *network){
    bool flag = false;
    int fd;
    int interfaceNum = 0;
    struct ifreq buf[16];
    struct ifconf ifc;
    struct ifreq ifrcopy;
    struct sockaddr_in *sin_net_mask;
    char mac[24] = {0};
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        // close(fd);
        return flag;
    }
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t)buf;

    if (!ioctl(fd, SIOCGIFCONF, (char *)&ifc))
    {
        interfaceNum = ifc.ifc_len / sizeof(struct ifreq);
        while (interfaceNum-- > 0)
        {
            QString name = buf[interfaceNum].ifr_ifrn.ifrn_name;
            if(!name.contains(network->Name))continue;
            ifrcopy = buf[interfaceNum];
            if (ioctl(fd, SIOCGIFFLAGS, &ifrcopy))
            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return flag;
            }
            //get the mac of this interface
            memset(&buf[interfaceNum], 0, sizeof(buf[interfaceNum]));
            strncpy(buf[interfaceNum].ifr_name, "eth0", sizeof(buf[interfaceNum].ifr_name )-1);
            sin_net_mask = (struct sockaddr_in *)&buf[interfaceNum].ifr_ifru.ifru_hwaddr;
            sin_net_mask -> sin_family = ARPHRD_ETHER;
            memset(mac, 0, sizeof(mac));
            QString str = network->Mac.replace(':', "");
            charToHex(str.toLatin1().data(), (quint8 *)buf[interfaceNum].ifr_hwaddr.sa_data, 6 );
            if (!ioctl(fd, SIOCSIFHWADDR, (char *)(&buf[interfaceNum])))            {
                //qDebug()<<"true";
            }            else            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return flag;
            }
            memset(&buf[interfaceNum], 0, sizeof(buf[interfaceNum]));
            strncpy(buf[interfaceNum].ifr_name, "eth0", sizeof(buf[interfaceNum].ifr_name )-1);
            sin_net_mask = (struct sockaddr_in *)&buf[interfaceNum].ifr_ifru.ifru_addr;
            sin_net_mask -> sin_family = AF_INET;
            inet_aton(network->Ip.toStdString().c_str(),&(sin_net_mask->sin_addr));
            //get the IP of this interface
            if (!ioctl(fd, SIOCSIFADDR, (char *)&buf[interfaceNum]))            {
            }            else            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                close(fd);
                return flag;
            }
            //get the subnet mask of this interface
            memset(&buf[interfaceNum], 0, sizeof(buf[interfaceNum]));
            strncpy(buf[interfaceNum].ifr_name, "eth0", sizeof(buf[interfaceNum].ifr_name )-1);
            sin_net_mask = (struct sockaddr_in *)&buf[interfaceNum].ifr_addr;
            sin_net_mask -> sin_family = AF_INET;
            inet_pton(AF_INET, network->NetMask.toLatin1().data(), &sin_net_mask ->sin_addr);
            //memcpy((void *)((struct sockaddr_in *)&(buf[interfaceNum].ifr_ifru.ifru_netmask).sa_data), network->NetMask.toLatin1().data(), strlen(network->NetMask.toLatin1().data()));
            if (!ioctl(fd, SIOCSIFNETMASK, &buf[interfaceNum]))            {
                //qDebug()<<"true";
            }            else            {
                printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
                return flag;
            }
        }
    }    else    {
        printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
        close(fd);
        return flag;
    }
    close(fd);
    return flag;
}

static bool delNullRoute( QString host)
{
    int fd = socket( PF_INET, SOCK_DGRAM, IPPROTO_IP );

    struct rtentry route;
    memset( &route, 0, sizeof( route ) );

    struct sockaddr_in *addr = (struct sockaddr_in *)&route.rt_gateway;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = 0;
    inet_aton(host.toStdString().c_str(),&(addr->sin_addr));
    addr = (struct sockaddr_in*) &route.rt_dst;
    addr->sin_family = AF_INET;
    //addr->sin_addr.s_addr = htonl(host);
    addr->sin_addr.s_addr = 0;
    addr = (struct sockaddr_in*) &route.rt_genmask;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = 0x00000000;

    route.rt_flags = RTF_UP | RTF_HOST | RTF_REJECT;
    route.rt_metric = 0;

    // this time we are deleting the route:
    if ( ioctl( fd, SIOCDELRT, &route ) )
    {
        close( fd );
        printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
        qDebug()<<"delroute false";
        return false;
    }

    close( fd );
    return true;
}

static int is_valid_ip(QString ipaddr)
{
   int ret = 0;
   struct in_addr inp;
   ret = inet_aton(ipaddr.toLatin1().data(), &inp);
   if (0 == ret)
   {
       return -1;
   }
   else
   {
       printf("inet_aton:ip=%u\n",ntohl(inp.s_addr));
   }

   return 0;
}

static int set_gateway(QString ip)
{
   int sockFd;
   struct sockaddr_in sockaddr;
   struct rtentry rt;

   if (is_valid_ip(ip) < 0)
   {
       printf("gateway was invalid!\n");
       return -1;
   }

   sockFd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockFd < 0)
   {
       perror("Socket create error.\n");
       return -1;
   }

   memset(&rt, 0, sizeof(struct rtentry));
   memset(&sockaddr, 0, sizeof(struct sockaddr_in));
   sockaddr.sin_family = AF_INET;
   sockaddr.sin_port = 0;
   if(inet_aton(ip.toLatin1().data(), &sockaddr.sin_addr)<0)
   {
       perror("inet_aton error\n" );
       close(sockFd);
       return -1;
   }

   memcpy ( &rt.rt_gateway, &sockaddr, sizeof(struct sockaddr_in));
   ((struct sockaddr_in *)&rt.rt_dst)->sin_family=AF_INET;
   ((struct sockaddr_in *)&rt.rt_genmask)->sin_family=AF_INET;
   rt.rt_flags = RTF_GATEWAY;
   if (ioctl(sockFd, SIOCADDRT, &rt)<0)
   {
       perror("ioctl(SIOCADDRT) error in set_default_route\n");
       close(sockFd);
       return -1;
   }

   return 0;
}

static int del_gateway(QString ip)
{
   int sockFd;
   struct sockaddr_in sockaddr;
   struct rtentry rt;

   if (is_valid_ip(ip) < 0)
   {
       printf("gateway was invalid!\n");
       return -1;
   }

   sockFd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockFd < 0)
   {
       perror("Socket create error.\n");
       return -1;
   }

   memset(&rt, 0, sizeof(struct rtentry));
   memset(&sockaddr, 0, sizeof(struct sockaddr_in));
   sockaddr.sin_family = AF_INET;
   sockaddr.sin_port = 0;
   if(inet_aton(ip.toLatin1().data(), &sockaddr.sin_addr)<0)
   {
       perror("inet_aton error\n" );
       close(sockFd);
       return -1;
   }

   memcpy ( &rt.rt_gateway, &sockaddr, sizeof(struct sockaddr_in));
   ((struct sockaddr_in *)&rt.rt_dst)->sin_family=AF_INET;
   ((struct sockaddr_in *)&rt.rt_genmask)->sin_family=AF_INET;
   rt.rt_flags = RTF_GATEWAY;
   if (ioctl(sockFd, SIOCDELRT, &rt)<0)
   {
       perror("ioctl(SIOCDELRT) error in set_default_route\n");
       close(sockFd);
       return -1;
   }

   return 0;
}

static bool addNullRoute( QString host )
{
    // create the control socket.
    int fd = socket( PF_INET, SOCK_DGRAM, IPPROTO_IP );

    struct rtentry route;
    memset( &route, 0, sizeof( route ) );

    // set the gateway to 0.
    struct sockaddr_in *addr = (struct sockaddr_in *)&route.rt_gateway;
    addr->sin_family = AF_INET;

    inet_aton(host.toStdString().c_str(),&(addr->sin_addr));
    // set the host we are rejecting.
    addr = (struct sockaddr_in*) &route.rt_dst;
    addr->sin_family = AF_INET;
    //addr->sin_addr.s_addr = htonl(host);
    addr->sin_addr.s_addr = 0;

    // Set the mask. In this case we are using 255.255.255.255, to block a single
    // IP. But you could use a less restrictive mask to block a range of IPs.
    // To block and entire C block you would use 255.255.255.0, or 0x00FFFFFFF
    addr = (struct sockaddr_in*) &route.rt_genmask;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = 0x00000000;

    // These flags mean: this route is created "up", or active
    // The blocked entity is a "host" as opposed to a "gateway"
    // The packets should be rejected. On BSD there is a flag RTF_BLACKHOLE
    // that causes packets to be dropped silently. We would use that if Linux
    // had it. RTF_REJECT will cause the network interface to signal that the
    // packets are being actively rejected.
    route.rt_flags = RTF_UP | RTF_HOST | RTF_REJECT;
    route.rt_metric = 0;

    // this is where the magic happens..
    if ( ioctl( fd, SIOCADDRT, &route ) )
    {
        close( fd );
        printf("ioctl: %s [%s:%d]\n", strerror(errno), __FILE__, __LINE__);
        qDebug()<<"addroute false";
        return false;
    }

    // remember to close the socket lest you leak handles.
    close( fd );
    return true;
}

static //存在返回1，不存在返回0
int judge_pid_exist(char* pidName, char* pid){
    DIR *dir;
    struct dirent *next;
    FILE *status;
    char buffer[READ_BUF_SIZE];
    char name[READ_BUF_SIZE];
    //proc中包括当前的进程信息,读取该目录
    dir = opendir("/proc");
    if (!dir){
        printf("Cannot open /proc\n");
        return 0;
    }
    //遍历
    while ((next = readdir(dir)) != NULL){
        //跳过"."和".."两个文件名
        if ((strcmp(next->d_name, "..") == 0) || (strcmp(next->d_name, ".") == 0))  {
            continue;
        }
        //如果文件名不是数字则跳过
        if (!isdigit(*next->d_name)){
            continue;
        }
        //判断是否能打开状态文件
        sprintf(buffer,"/proc/%s/status",next->d_name);
        if (!(status = fopen(buffer,"r")))
        {
            continue;
        }
        //读取状态文件
        if (fgets(buffer,READ_BUF_SIZE,status) == NULL)   {
            fclose(status);
            continue;
        }
        fclose(status);
        //读取PID对应的程序名，格式为Name:  程序名
        sscanf(buffer,"%*s %s",name);
        //判断程序名是否符合预期
        if (strcmp(name,pidName) == 0){
            //符合
            closedir(dir);
            memcpy(pid, next->d_name, sizeof( next->d_name));
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

static void process(int signo){

    printf("signo:%d,pid:%d\n",signo,getpid());

}

#endif // FUNCTIONS_H
