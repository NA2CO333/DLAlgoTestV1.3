#ifndef LANDEVFINDERDEFS_H
#define LANDEVFINDERDEFS_H

/* =============================================================================================
 * 本实现方案中，局域网设备查找（自动寻址 Aoto-Address）的基本原理：UPD 直接广播 + 私有协议
 * 1、服务端实时侦听指定端口的 UDP 广播。
 * 2、客户端发送广播询问当前服务器的 IP 和端口。
 * 3、服务器收到询问后应答。
 *
 */

/* ---------------------------------------------------------------------------------------------
 * 局域网设备查找的通信协议：
 * 1、服务端和客户端收到 UDP 消息后，都是根据预先约定的相同的“消息关键字”（下文用“Keyword”表示）来判断该消息是否是来自对方，消息关键字区分大小写。
 *
 * 2、消息数据包的结构：
 *    服务端消息："{" + [Keyword] + "-Server" + ": " + 附加数据 + "}"
 *    客户端消息："{" + [Keyword] + "-Client" + ": " + 附加数据 + "}"
 *
 *    其中，[Keyword]表示上述的“消息关键字”，方括号只是表示该值是动态确定的，并不是指字符串里包含方括号。
 *
 *    由上可得：
 *        可通过在消息数据中搜索"{"确定数据包的第一个字节，搜索"}"确定数据包的最后一个字节。
 *        [Keyword]之后的 "-Server" 或 "-Client" 之后的 ": " 的后一个字符为附加数据的开始，"}" 的前一个字符为附加数据的结尾。
 *        服务端通过检查消息是否包含“[Keyword]-Client”来判断该消息是否来自客户端，而客户端则通过检查消息是否包含“[Keyword]-Server”来判断该消息是否来自服务端。
 *
 * 3、上述“附加数据”一般是 CSV （逗号分隔值）格式，具体字段值可视需求而约定。
 *    服务端消息的附加数据：如果只是需要查找IP和端口，格式可为："[IP],[port]"
 *    客户端消息的附加数据：非必有
 *
 * ------------------------------------------------------------------------------------------ */

// 版本日期（最后修改日期）
#define LAN_DEV_FINDER_VER_DATE "20230804"

// 默认 服务端接收 端口
#define LAN_DEV_FINDER_DEFAULT_SVR_RECV_PORT    51365

// 默认 客户端接收 端口（客户端和服务端使用不同的监听端口，是为了方便在同一台电脑上调试，而且对于客户端来说，发送数据和侦听数据的端口相同，形成回环，不确定是否有问题）
#define LAN_DEV_FINDER_DEFAULT_CLT_RECV_PORT    51367

// 默认的消息关键字
#define LAN_DEV_FINDER_DEFAULT_KEY  "Lan-Dev-Finder"

// 消息关键字后缀（服务端）
#define LAN_DEV_FINDER_KEY_SERVER   "-Server"

// 消息关键字后缀（客户端）
#define LAN_DEV_FINDER_KEY_CLIENT   "-Client"

// 服务端应答消息格式：前缀 + ip + 端口，逗号分隔，且前后端用花括弧包起来
#define LAN_DEV_FINDER_RESPONSE_FORMAT  ("{" LAN_DEV_FINDER_DEFAULT_KEY LAN_DEV_FINDER_KEY_SERVER ": %s,%d}")
// 服务端应答消息的最大长度
#define LAN_DEV_FINDER_RESPONSE_LEN     256+128

// 客户端请求消息格式：前缀 + 当前询问次数（非必有），逗号分隔，且前后端用花括弧包起来
#define LAN_DEV_FINDER_REQUEST_FORMAT   ("{" LAN_DEV_FINDER_DEFAULT_KEY LAN_DEV_FINDER_KEY_CLIENT ": %d}")
// 客户端请求消息的最大长度
#define LAN_DEV_FINDER_REQUEST_LEN      256


#endif // LANDEVFINDERDEFS_H
