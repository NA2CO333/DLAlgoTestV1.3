#ifndef REMOTESERVICEDEFS_H
#define REMOTESERVICEDEFS_H

/* “设备文件远程浏览和上传”功能的设备端服务模块的共有类型定义
 */

//
namespace Net {
namespace Remote {

// Log类型
enum enLogType {
    logType_debug,
    logType_info,
    logType_warning,
    logType_error,
};

}   // namespace Remote
}   // namespace Net

#endif // REMOTESERVICEDEFS_H
