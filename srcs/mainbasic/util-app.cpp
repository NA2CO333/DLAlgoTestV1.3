#include "util-app.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

#include "logger.h"
#include "winmanage.h"

//
namespace UtilApp {

QString getPreviewImgPath(const CPatient &_patient)
{
    //
    QString img_dir_name = _patient.getImgDirName();

    //
    //QString path = QString("/media/pdfPreviewImg/%1_pdfPreview.jpg");
    QString path_tpl = "/media/photo/%1/temp12.jpg";    // NOTE: 2025-06-04 为降低图像占用空间，xxx_pdfPreview.jpg 不再保存，预览图只有 CAlgoInvoker::calcVision() 保存的 12、18 号转灯图
    QString path = path_tpl.arg(img_dir_name);

    //
    if (!QFile::exists(path)) {                 // 兼容旧版本，若 .jpg 不存在，则尝试 .bmp
        QString path_old = QString("/media/photo/%1/temp12.bmp").arg(img_dir_name);
        if (QFile::exists(path_old)) {
            path = path_old;
        }
    }

    //
    return path;
}

//QString getPdfFilePath(const CPatient &_patient)
//{
//    QString tpl_path = QString("%1/%2_result.pdf");
//    QString img_dir_name = _patient.getImgDirName();
//    return tpl_path.arg(PDF_REPORT_DIR).arg(img_dir_name);
//}

bool deletePreviewImagesOfPatient(const CPatient &_pat)
{
    if (!_pat.isTest) {
        return true;
    }

    QString file_path = getPreviewImgPath(_pat);
    if (QFile::exists(file_path)) {
        QString cmd = "rm " + file_path;
        system(cmd.toLatin1().data());
        //qDebug()<<cmd;

        // 删除该测量的所有存图
        QString img_dir_name = _pat.getImgDirName();
        QString dir_path = QString("/media/photo/%1").arg(img_dir_name);
        //qDebug() << dir_path;
        if (QFile::exists(dir_path)) {
            QString cmd = "rm -r " + dir_path;
            system(cmd.toLatin1().data());
        }
        return true;
    } else {
        logWarning(QString("file %1 not found!, delete preview image failed!").arg(file_path));
        return false;
    }
}

bool deletePdfFilesOfPatient(const CPatient &_pat)
{
    if (!_pat.isTest) {
        return true;
    }

    //QString file_path = getPdfFilePath(_pat);
    //if (QFile::exists(file_path))
    //{
    //    QString cmd = "rm " + file_path;
    //    system(cmd.toLatin1().data());
    //    //qDebug()<<cmd;
    //    return true;
    //} else {
    //    logWarning(QString("file %1 not found!, delete pdf file failed!").arg(file_path));
    //    return false;
    //}
}

} // namespace AppUtils
