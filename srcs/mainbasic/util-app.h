#ifndef UTIL_APP_H
#define UTIL_APP_H

/* 本程序内部使用的工具函数或类
 */

#include <QObject>

#include "data.h"
#include "globaltypes.h"

//
namespace UtilApp {

QString getPreviewImgPath(const CPatient &_patient);                    // 获取数据对象的预览图像路径
//QString getPdfFilePath(const CPatient &_patient);                       // 获取数据对象的 pdf 文件路径

bool deletePreviewImagesOfPatient(const CPatient &_pat);                // 删除指定数据对象的预览图像
bool deletePdfFilesOfPatient(const CPatient &_pat);                     // 删除指定数据对象的 pdf 文件

} // namespace AppUtils

#endif // UTIL_APP_H
