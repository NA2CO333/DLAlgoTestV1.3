#include "commandhandler.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QProcess>

#include "remoteservice.h"
#include "websocketconn.h"
#include "logger.h"

namespace Net {
namespace Remote {

// JSON 格式
//static const QJsonDocument::JsonFormat JSON_FORMAT = QJsonDocument::Indented;
//static const QJsonDocument::JsonFormat JSON_FORMAT = QJsonDocument::Compact;

///=============================================================================================================
/// 数据类

bool CCommunicMessage::fromJson(const QString &_json_str)
{
    QJsonParseError json_err;
    QJsonDocument json_doc = QJsonDocument::fromJson(_json_str.toUtf8().data(), &json_err);
    if (QJsonParseError::NoError == json_err.error) {
        if (json_doc.isNull()) {
            qDebug() << __PRETTY_FUNCTION__ << ": json doc is null";
            return false;
        }
        if (!json_doc.isObject()) {
             qDebug() << __PRETTY_FUNCTION__ << ": doc is not object";
             return false;
        }

        QJsonObject json_obj = json_doc.object();

        {QJsonValue val_command = json_obj.value("command");    if (!val_command.isUndefined() && !val_command.isNull())    { command = val_command.toString(); } }
        {QJsonValue val_stat    = json_obj.value("stat");       if (!val_stat.isUndefined() && !val_stat.isNull())          { stat = val_stat.toString(); } }
        {QJsonValue val_data    = json_obj.value("data");       if (!val_data.isUndefined() && !val_data.isNull())          { data = val_data.toString(); } }
        {QJsonValue val_msg     = json_obj.value("msg");        if (!val_msg.isUndefined() && !val_msg.isNull())            { msg = val_msg.toString(); } }
        {QJsonValue val_version = json_obj.value("version");    if (!val_version.isUndefined() && !val_version.isNull())    { version = val_version.toString(); } }
        {QJsonValue val_sender  = json_obj.value("sender");     if (!val_sender.isUndefined() && !val_sender.isNull())      { sender = val_sender.toString(); } }

        //
        return true;
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "json parse err: err = " << json_err.error << ", " << json_err.errorString();
        return false;
    }
}

QString CCommunicMessage::toJson()
{
    QJsonObject json_obj;

    json_obj.insert("command",  QJsonValue(command));
    json_obj.insert("stat",     QJsonValue(stat));
    json_obj.insert("data",     QJsonValue(data));
    json_obj.insert("msg",      QJsonValue(msg));
    json_obj.insert("version",  QJsonValue(version));
    json_obj.insert("sender",   QJsonValue(sender));

    QJsonDocument json_doc;
    json_doc.setObject(json_obj);

    return json_doc.toJson(QJsonDocument::Indented);
}

bool CReqDirList::fromJson(const QString &_json_str)
{
    QJsonParseError json_err;
    QJsonDocument json_doc = QJsonDocument::fromJson(_json_str.toUtf8().data(), &json_err);
    if (QJsonParseError::NoError == json_err.error) {
        if (json_doc.isNull()) {
            qDebug() << __PRETTY_FUNCTION__ << ": json doc is null";
            return false;
        }
        if (!json_doc.isObject()) {
             qDebug() << __PRETTY_FUNCTION__ << ": doc is not object";
             return false;
        }

        QJsonObject json_obj = json_doc.object();

        {QJsonValue val_dirPath     = json_obj.value("dirPath");    if (!val_dirPath.isUndefined() && !val_dirPath.isNull())    { dirPath = val_dirPath.toString(); } }
        {QJsonValue val_currPath    = json_obj.value("currPath");   if (!val_currPath.isUndefined() && !val_currPath.isNull())  { currPath = val_currPath.toString(); } }

        //
        return true;
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "json parse err: err = " << json_err.error << ", " << json_err.errorString();
        return false;
    }
}

QString CFileInfo::toJson()
{
    QJsonObject json_obj;

    this->toJsonObj(json_obj);

    QJsonDocument json_doc;
    json_doc.setObject(json_obj);

    return json_doc.toJson(QJsonDocument::Indented);
}

void CFileInfo::toJsonObj(QJsonObject &_json_obj) const
{
    _json_obj.insert("name",    QJsonValue(name));
    _json_obj.insert("isDir",   QJsonValue(isDir));
    _json_obj.insert("size",    QJsonValue(size));
    _json_obj.insert("date",    QJsonValue(date));
}

QString CRespDirList::toJson()
{
    QJsonObject json_obj;

    json_obj.insert("dirPath",  QJsonValue(dirPath));
    json_obj.insert("count",    QJsonValue(count));

    QJsonArray json_arr;
    for (int i = 0; i < fileList.size(); i++) {
        const CFileInfo &info = fileList.at(i);
        QJsonObject obj;
        info.toJsonObj(obj);
        json_arr.append(QJsonValue(obj));
    }
    json_obj.insert("fileList", QJsonValue(json_arr));

    QJsonDocument json_doc;
    json_doc.setObject(json_obj);

    return json_doc.toJson(QJsonDocument::Compact);     /* 这个对象的数据量可能比较大，最好压缩 */
}

bool CReqUploadFile::fromJson(const QString &_json_str)
{
    QJsonParseError json_err;
    QJsonDocument json_doc = QJsonDocument::fromJson(_json_str.toUtf8().data(), &json_err);
    if (QJsonParseError::NoError == json_err.error) {
        if (json_doc.isNull()) {
            qDebug() << __PRETTY_FUNCTION__ << ": json doc is null";
            return false;
        }
        if (!json_doc.isObject()) {
             qDebug() << __PRETTY_FUNCTION__ << ": doc is not object";
             return false;
        }

        QJsonObject json_obj = json_doc.object();

        {QJsonValue val_dirPath     = json_obj.value("dirPath");    if (!val_dirPath.isUndefined() && !val_dirPath.isNull())            { dirPath = val_dirPath.toString(); } }
        {QJsonValue val_name        = json_obj.value("name");       if (!val_name.isUndefined() && !val_name.isNull())                  { name = val_name.toString(); } }
        {QJsonValue val_bytes_pseg  = json_obj.value("bytesPSeg");  if (!val_bytes_pseg.isUndefined() && !val_bytes_pseg.isNull())      { bytesPSeg = val_bytes_pseg.toBool(); } }
        {QJsonValue val_seg_begin   = json_obj.value("segBegin");   if (!val_seg_begin.isUndefined() && !val_seg_begin.isNull())        { segBegin = val_seg_begin.toBool(); } }
        {QJsonValue val_byte_begin  = json_obj.value("byteBegin");  if (!val_byte_begin.isUndefined() && !val_byte_begin.isNull())      { byteBegin = val_byte_begin.toBool(); } }
        {QJsonValue val_timeout     = json_obj.value("timeout");    if (!val_timeout.isUndefined() && !val_timeout.isNull())            { timeout = val_timeout.toBool(); } }
        {QJsonValue val_compress    = json_obj.value("compress");   if (!val_compress.isUndefined() && !val_compress.isNull())          { compress = val_compress.toBool(); } }
        {QJsonValue val_format      = json_obj.value("format");     if (!val_format.isUndefined() && !val_format.isNull())              { format = val_format.toString(); } }
        {QJsonValue val_need_check  = json_obj.value("needCheck");  if (!val_need_check.isUndefined() && !val_need_check.isNull())      { needCheck = val_need_check.toBool(); } }
        {QJsonValue val_verifi_algo = json_obj.value("verifiAlgo"); if (!val_verifi_algo.isUndefined() && !val_verifi_algo.isNull())    { verifiAlgo = val_verifi_algo.toBool(); } }

        //
        return true;
    } else {
        qDebug() << __PRETTY_FUNCTION__ << "json parse err: err = " << json_err.error << ", " << json_err.errorString();
        return false;
    }
}

QString CPostFileInfo::toJson()
{
    QJsonObject json_obj;

    json_obj.insert("dirPath",  QJsonValue(dirPath));
    json_obj.insert("name",     QJsonValue(name));
    json_obj.insert("size",     QJsonValue(size));

    QJsonDocument json_doc;
    json_doc.setObject(json_obj);

    return json_doc.toJson(QJsonDocument::Indented);
}

QString CPostFileEnd::toJson()
{
    QJsonObject json_obj;

    json_obj.insert("dirPath",  QJsonValue(dirPath));
    json_obj.insert("name",     QJsonValue(name));
    json_obj.insert("size",     QJsonValue(size));
    json_obj.insert("isSucc",   QJsonValue(isSucc));
    json_obj.insert("checksum", QJsonValue(checksum));

    QJsonDocument json_doc;
    json_doc.setObject(json_obj);

    return json_doc.toJson(QJsonDocument::Indented);
}

///=============================================================================================================
/// class CCommandHandler

CCommandHandler::CCommandHandler(CRemoteService *_service, QObject *_parent) : QObject(_parent)
  , service(_service)
{
    //
    setObjectName(TYPE_NAME_DIR_LIST);

    //
    cmdProcessFuncs = new QMap<QString, funcCmdPrecess>;

}

void CCommandHandler::getSupportedCmds(QStringList &_cmd_list)
{
    _cmd_list = cmdProcessFuncs->keys();
}

bool CCommandHandler::processCmd(const QString &_cmd, const QString &_data_json_str, QString &_err_msg)
{
    //
    if (!cmdProcessFuncs->contains(_cmd)) {
        _err_msg = (QString(__PRETTY_FUNCTION__) + ": logic error! not suport command '%1'").arg(_cmd);
        return false;
    }

    //
    funcCmdPrecess func_check_pkg = cmdProcessFuncs->value(_cmd);
    if (func_check_pkg) {
        bool is_succ = func_check_pkg(_data_json_str, _err_msg);
        return is_succ;
    } else {
        _err_msg = QString("program error: callback function of %1 not found!").arg(_cmd);
        return false;
    }
}

///=============================================================================================================
/// class CDirListHandler

const QString TYPE_NAME_DIR_LIST = "CDirListHandler";

const QString CMD_REQU_DIR_LIST         = "dir-list";               // “目录查询”请求指令码
const QString CMD_REQU_UPLOAD_FILE      = "upload-file";            // “文件上传”请求指令码

const QString CMD_RESP_POST_FILE_INFO   = "post-file-info";         // “文件信息”应答指令码

//
CDirListHandler::CDirListHandler(CRemoteService *_service, QObject *_parent) :
    CCommandHandler(_service, _parent)
{
    // 设置 “指令-处理函数” 映射表
    cmdProcessFuncs->insert(CMD_REQU_DIR_LIST,       std::bind(&CDirListHandler::processCmd_DirList, this, std::placeholders::_1, std::placeholders::_2));
    cmdProcessFuncs->insert(CMD_REQU_UPLOAD_FILE,    std::bind(&CDirListHandler::processCmd_UploadFile, this, std::placeholders::_1, std::placeholders::_2));

    //
    qRegisterMetaType<CReqUploadFile>("CReqUploadFile");

    // 构造文件上传对象
    fileUpload = new CFileUpload(_service, nullptr);

    threadUploadFile = new QThread;

    fileUpload->moveToThread(threadUploadFile);
    threadUploadFile->start();

    QObject::connect(this, &CDirListHandler::sigUploadFile, fileUpload, &CFileUpload::slotUploadFile, Qt::QueuedConnection);
    QObject::connect(fileUpload, &CFileUpload::sigUploadAborted, this, &CDirListHandler::slot_fileUpload_UploadError, Qt::QueuedConnection);
    QObject::connect(fileUpload, &CFileUpload::sigMessage, this, &CDirListHandler::slot_fileUpload_Message, Qt::QueuedConnection);

}

void CDirListHandler::addTopDir(QString _dir_path)
{
    if (!topDirs.contains(_dir_path)) {
        // 若路径不以 "/" 结束，则补上
        if (!_dir_path.endsWith(QDir::separator())) {
            _dir_path += QDir::separator();
        }

        // 检查路径的有效性
        bool is_valid = true;
        if (!QFile::exists(_dir_path)) {
            qDebug() << "path " << _dir_path << "not exists!";
            return;
        } else if (!QFileInfo(_dir_path).isDir()) {
            qDebug() << "path " << _dir_path << "is not dir!";
            return;
        }

        //
        if (is_valid) {
            topDirs.append(_dir_path);
        }
    } else {
        qWarning() << __FUNCTION__ << "dir path is already appended";
    }
}

void CDirListHandler::setUploadSvcUrl(QUrl _url)
{
    fileUpload->uploadSvcUrl = _url;
}

QUrl CDirListHandler::getUploadSvcUrl()
{
    return fileUpload->uploadSvcUrl;
}

void CDirListHandler::slot_fileUpload_UploadError(QString _err_msg)
{
    service->sendErrorResponse(CMD_REQU_UPLOAD_FILE, _err_msg);
}

void CDirListHandler::slot_fileUpload_Message(enLogType _log_type, QString _msg)
{
    service->emitLog(_log_type, _msg);
}

bool CDirListHandler::processCmd_DirList(const QString &_data_json_str, QString &_err_msg)
{
    static const QString FILE_DATE_FORMAT = QStringLiteral("yyyy-MM-dd hh:mm:ss");

    bool is_succ = true;
    _err_msg.clear();

    QString dir_path = "";      // 目标路径

    // 得到目标路径
    do {
        // 得到数据对象
        CReqDirList req_dir_list;
        bool succ_from_json = req_dir_list.fromJson(_data_json_str);
        if (!succ_from_json) {
            //_err_msg = "Parsing JSON string failed! Destination direction changed to root directory.";
            _err_msg = "解析 JSON 字符串失败! 目标目录已改为顶层目录。";
            service->emitLog(logType_error, _err_msg);
            dir_path = "/";                                         /* 不管发生什么错误，stat 都返回 true，并自动跳到顶层目录，不过要设置提示信息。后面也是一样规则。 */
            break;
        }

        //
        dir_path = req_dir_list.dirPath;
        //service->emitLog(logType_info, "dest dir of command = \"" + dir_path + "\"");
        service->emitLog(logType_info, "收到的指令中的目标文件夹 = \"" + dir_path + "\"");

        // 若目标文件夹路径是 ".."，获取真正的目标路径
        if (dir_path == "..") {
            //
            if (req_dir_list.currPath.length() == 0) {
                //_err_msg = "currPath can't be empty when list parent dir! Destination direction turn to \"/\"";
                _err_msg = "查询父目录时，currPath 参数不可为空！目标目录已改为顶层目录。";
                service->emitLog(logType_error, _err_msg);
                dir_path = "/";
                break;
            }
            int idx = req_dir_list.currPath.lastIndexOf(QDir::separator());
            if (idx >= 0) {
                dir_path = req_dir_list.currPath.left(idx);
            } else {
                _err_msg = "\"/\" not found in currPath, getting parent path failed!  Destination direction turn to \"/\"";
                _err_msg = "currPath 参数中未找到 \"/\"，获取父文件夹路径失败！目标目录已改为顶层目录。";
                service->emitLog(logType_error, _err_msg);
                dir_path = "/";
                break;
            }
        }

        // 若目标路径是文件，则取其所在文件夹路径
        if (QFileInfo(dir_path).isFile()) {
            int idx = dir_path.lastIndexOf(QDir::separator());
            if (idx == 0) {
                dir_path = "/";
            } else if (idx > 0) {
                dir_path = dir_path.left(idx);
            } else {
                //_err_msg = "Destination path is a file, and failed to get it's parent dir!  Destination direction turn to \"/\"";
                _err_msg = "目标路径是个文件，且获取其所在文件夹失败！目标目录已改为顶层目录。";
                service->emitLog(logType_error, _err_msg);
                dir_path = "/";
                break;
            }
        }

        // 若目标路径不存在，则设为顶层目录
        if (!QFile::exists(dir_path)) {
            _err_msg = "Logic error: destination path \"" + dir_path + "\" not found!  Destination direction turn to \"/\"";
            _err_msg = "逻辑错误：目标目录 \"" + dir_path + "\" 不存在！目标目录已改为顶层目录。";
            service->emitLog(logType_error, _err_msg);
            dir_path = "/";
            break;
        }

    } while (false);

    // 检查目标路径是否在顶层目录内
    bool is_permitted = isInTopDirs(dir_path);
    if (!is_permitted) {
        //_err_msg = "Path \"" + dir_path + "\" is not permitted!";
        _err_msg = "路径 \"" + dir_path + "\" 不在顶层目录范围内";
        service->emitLog(logType_error, _err_msg);
        return false;
    }

    //
    CRespDirList resp_dir_list;                 // 应答所用的数据对象
    resp_dir_list.dirPath = dir_path;

    // 列举目标路径下的文件信息
    QVector<CFileInfo> &file_list = resp_dir_list.fileList;

    if ((dir_path == "/") && (topDirs.size() > 1)) {       // 若目标目录是顶层目录，且顶层目录有多个，则列举各个顶层目录
        for (int i = 0; i < topDirs.size(); i++) {
            QString path = topDirs.at(i);

            //if (path.endsWith(QDir::separator())) {
            //    path = path.left(path.length() - 1);        // 因为要通过 QFileInfo::fileName() 读取文件夹的名称，路径末尾不能有 '/'
            //}
            QFileInfo file_info(path);

            CFileInfo info_obj;
            info_obj.name = path;
            info_obj.isDir = true;
            info_obj.size = "";
            info_obj.date = file_info.fileTime(QFileDevice::FileModificationTime).toString(FILE_DATE_FORMAT);

            file_list.append(info_obj);
        }
    } else {                                                // 否则，列举单一顶层目录下的文件和子目录
        QDir dir(dir_path);
        QFileInfoList file_info_list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (int i = 0; i < file_info_list.size(); i++) {
            QFileInfo file_info = file_info_list.at(i);

            CFileInfo info_obj;
            info_obj.name = file_info.fileName();
            info_obj.isDir = file_info.isDir();
            info_obj.size = (file_info.isFile() ? (QString("%1 B").arg(file_info.size())) : QString(""));
            info_obj.date = file_info.fileTime(QFileDevice::FileModificationTime).toString(FILE_DATE_FORMAT);

            file_list.append(info_obj);
        }
    }

    resp_dir_list.count = file_list.size();

    //
    CCommunicMessage communic_msg;
    communic_msg.command = QString("r_") + CMD_REQU_DIR_LIST;
    communic_msg.stat = (is_succ ? STAT_SUCC : STAT_FAIL);
    communic_msg.data = resp_dir_list.toJson();
    communic_msg.msg = _err_msg;
    communic_msg.version = Protocol;
    communic_msg.sender = service->getSenderNum();
    QString json_str = communic_msg.toJson();

    //
    //service->emitLog(logType_info, "sending text message:\n" + json_str);
    service->emitLog(logType_info, "正在发送文本消息:\n" + json_str);

    //
    qint64 n = service->getWebSocket()->sendTextMessage(json_str);
    qDebug() << n << " bytes sended";

    //
    return is_succ;
}

bool CDirListHandler::processCmd_UploadFile(const QString &_data_json_str, QString &_err_msg)
{
    //
    _err_msg.clear();

    //
    do {
        // 得到数据对象
        CReqUploadFile req_upload_file;
        bool succ_from_json = req_upload_file.fromJson(_data_json_str);
        if (!succ_from_json) {
            _err_msg = "parsing JSON failed!";
            service->emitLog(logType_error, _err_msg);
            return false;
        }

        // 检查目标路径是否在顶层目录内
        bool is_permitted = isInTopDirs(req_upload_file.dirPath);
        if (!is_permitted) {
            _err_msg = "Path \"" + req_upload_file.dirPath + "\" is not permitted!";
            service->emitLog(logType_error, _err_msg);
            return false;
        }

        // 检查文件是否存在
        QString file_path = req_upload_file.dirPath;
        if (!file_path.endsWith(QDir::separator())) {
            file_path += QDir::separator();
        }
        file_path += req_upload_file.name;

        if (!QFile::exists(file_path)) {
            _err_msg = QString("File '%1' not found!").arg(file_path);
            service->emitLog(logType_error, _err_msg);
            return false;
        }

        //
        //uploadFile(req_upload_file);         // TODO: 这种方式，无法用另一个线程上传文件？因为无法共用 websocket ？

        emit sigUploadFile(req_upload_file);

    } while (false);

    //
    return true;
}

bool CDirListHandler::isInTopDirs(const QString &_dst_path)
{
    //
    if (_dst_path == QDir::separator()) {
        return true;
    }

    //
    bool is_in_top_dir = false;
    for (int i = 0; i < topDirs.size(); i++) {
        if (_dst_path.startsWith(topDirs.at(i))) {
            is_in_top_dir = true;
            break;
        }
    }
    return is_in_top_dir;
}

void CDirListHandler::uploadFile(const CReqUploadFile &_req_upload_file)
{
    QString err_msg;

    //
    QString file_path = _req_upload_file.dirPath;
    if (!file_path.endsWith(QDir::separator())) {
        file_path += QDir::separator();
    }
    file_path += _req_upload_file.name;

    //
    QFileInfo file_info(file_path);

    // 如果是文件夹，则压缩
    bool succ_tar = false;
    if (file_info.isDir()) {
        succ_tar = fileUpload->tarDir(file_path, file_path, err_msg);
        if (!succ_tar) {
            err_msg = QString("packaging file \"%1\" failed! err: ").arg(file_path) + err_msg;
            service->sendErrorResponse(CMD_REQU_UPLOAD_FILE, err_msg);
            return;
        }
    }

    // 打开文件
    QFile file(file_path);
    bool is_open_succ = file.open(QFile::ReadOnly);
    if (!is_open_succ) {
        err_msg = QString("opening file \"%1\" failed!").arg(file_path);
        service->sendErrorResponse(CMD_REQU_UPLOAD_FILE, err_msg);
        return;
    }

    // 发送文件信息
    int file_size = file_info.size();

    CPostFileInfo obj_post_begin;
    obj_post_begin.dirPath = _req_upload_file.dirPath;
    obj_post_begin.name = file_info.fileName();
    obj_post_begin.size = file_size;

    CCommunicMessage communic_msg_begin;
    communic_msg_begin.command = CMD_RESP_POST_FILE_INFO;
    communic_msg_begin.data = obj_post_begin.toJson();
    communic_msg_begin.version = Protocol;
    communic_msg_begin.sender = service->getSenderNum();
    QString begin_json_str = communic_msg_begin.toJson();

    service->emitLog(logType_info, "sending text message:\n" + begin_json_str);

    qint64 n_begin = service->getWebSocket()->sendTextMessage(begin_json_str);
    qDebug() << n_begin << "bytes sended";

    //
    service->emitLog(logType_info, "sending file binary data ...");

    // 逐帧传送
    int bytes_pseg = _req_upload_file.bytesPSeg;
    int total = 0;
    int sended;
    do {
        QByteArray buffer = file.read(bytes_pseg);
        sended = service->getWebSocket()->sendBinaryMessage(buffer);
        total += sended;
        if (buffer.size() != sended) {
            err_msg = "error on sending file: size of buffer and sended not equal!";
            break;
        }
    } while (!file.atEnd());
    bool is_succ = (total == file_size);

    QString checksum = "";

    if (is_succ) {
        // TODO: 有没必要计算？


    } else {
        if (err_msg.length() == 0) {
            err_msg = "error on sending file: size of file and sended not equal!";
        }
    }

    // 发送文件上传结束消息
    CPostFileEnd obj_post_end;
    obj_post_end.dirPath = _req_upload_file.dirPath;
    obj_post_end.name = file_info.fileName();
    obj_post_end.size = total;
    obj_post_end.isSucc = is_succ;
    obj_post_end.checksum = checksum;

    CCommunicMessage communic_msg_end;
    communic_msg_end.command = "r_" + CMD_REQU_UPLOAD_FILE;
    communic_msg_end.data = obj_post_end.toJson();
    communic_msg_end.version = Protocol;
    communic_msg_end.sender = service->getSenderNum();
    QString end_json_str = communic_msg_end.toJson();

    service->emitLog(logType_info, "sending text message:\n" + end_json_str);

    qint64 n_end = service->getWebSocket()->sendTextMessage(end_json_str);
    qDebug() << n_end << "bytes sended";

    // 删除打包文件夹产生的临时文件
    if (succ_tar) {
        bool succ_del = QFile::remove(file_path);
        if (!succ_del) {
            QString msg = "delete temp file \"" + file_path + "\" failed!";
            //logWarning(msg);
            service->emitLog(logType_warning, msg);
        }
    }

}

///=============================================================================================================
/// class CFileUpload
///
QNetworkAccessManager *CFileUpload::s_netManager {nullptr};

CFileUpload::CFileUpload(CRemoteService *_service, QObject *_parent) :
    QObject(_parent),
    service(_service)
{
    if (!s_netManager) {
        logWarning(QString("%1: QNetworkAccessManager not been setted! Created internal!").arg(__PRETTY_FUNCTION__));
        s_netManager = new QNetworkAccessManager();
    }
}

void CFileUpload::setNetworkAccessManager(QNetworkAccessManager *_net_manager)
{
    s_netManager = _net_manager;
}

void CFileUpload::slotUploadFile(CReqUploadFile _req_upload_file)
{
    uploadFile(_req_upload_file);          /* （协议改为通过 http 文件上传方法上传到通用的文件上传接口） */

}

bool CFileUpload::tarDir(const QString &_dir_path, QString &_tar_path, QString &_err_msg)
{
    // 去掉路径最后的 "/"
    QString dst_path = _dir_path;
    if (dst_path.endsWith(QDir::separator())) {
        dst_path = dst_path.left(dst_path.length() - 1);
    }

    // 文件夹路径须存在，且是文件夹
    QFileInfo info(dst_path);
    if (!info.exists()) {
        _err_msg = "src path is not exists!";
        return false;
    }
    if (!info.isDir()) {
        _err_msg = "src path is not a directory!";
        return false;
    }

    // 得到 tar 文件路径
    _tar_path = dst_path + ".tar";
    if (QFile::exists(_tar_path)) {             // 若目标文件已存在，则删除
        bool is_del_succ = QFile::remove(_tar_path);
        if (!is_del_succ) {
            qWarning() << "remove the existing tar file \"" << "\" failed";
        }
    }

    // 得到父文件夹路径
    int idx_last_sep = dst_path.lastIndexOf(QDir::separator());
    if (idx_last_sep < 0) {
        _err_msg = "program err: getting parent dir failed!";
        return false;
    }
    QString parent_dir = dst_path.left(idx_last_sep);

    // 构造归档命令的参数
    QStringList arguments;
    arguments << "-cvf" << _tar_path;

    // 执行归档命令
    QProcess proc_tar;
    proc_tar.setWorkingDirectory(parent_dir);      // 将当前路径转到根目录
    proc_tar.start("tar", arguments);
    /*bool is_tar_succ =*/ proc_tar.waitForFinished(-1);        // TODO: waitForFinished() 的返回值、exitCode()、exitStatus()，该怎么严谨判断进程的执行状态？

    if (proc_tar.exitCode() != 0) {
        _err_msg = "tar command failed: " + proc_tar.errorString();
        return false;
    }

    //
    return true;
}

bool CFileUpload::zipFileOrDir(QString _file_path, QString &_zip_path, QString &_err_msg)
{
    //
    QFileInfo file_info(_file_path);

    bool is_dir = file_info.isDir();

    // 去掉路径最后的 '/'
    if (_file_path.endsWith(QDir::separator())) {
        _file_path = _file_path.left(_file_path.length() - 1);
    }

    // 得到父目录路径
    int idx_last_sep = _file_path.lastIndexOf(QDir::separator());
    if (idx_last_sep < 0) {
        _err_msg = "压缩时查找目标路径所在父目录失败！";
        return false;
    }

    // 得到目标路径所在文件夹路径，及文件名
    QString parent_dir = _file_path.left(idx_last_sep);
    QString file_name = _file_path.mid(idx_last_sep + 1);

    // 得到压缩文件夹的文件名
    QString zip_name = QString(file_name).replace('.', '_') + ".zip";           // 不管是文件还是文件夹，将文件名里的 '.' 替换为 '_'

    // 构造归档命令的参数
    QStringList arguments;

    if (is_dir) {
        arguments << "-r";
    }
    arguments << "-v" << zip_name << file_name;

    // 执行归档命令
    QProcess proc_zip;
    proc_zip.setWorkingDirectory(parent_dir);      // 将当前路径转到根目录
    proc_zip.start("zip", arguments);
    bool is_zip_succ = proc_zip.waitForFinished(-1);        // TODO: waitForFinished() 的返回值、exitCode()、exitStatus()，该怎么严谨判断进程的执行状态？
    if (!is_zip_succ) {
        // TODO: ?
    }

    if (proc_zip.exitCode() != 0) {
        _err_msg = "zip command failed: " + proc_zip.errorString();
        return false;
    }

    //
    _zip_path = parent_dir + QDir::separator() + zip_name;

    //
    return true;
}

void CFileUpload::uploadFile(const CReqUploadFile &_req_upload_file)
{
    /* 目前万灵云端的文件上传服务，文件是保存到阿里云的。
     * 文件保存到阿里云时，文件名会被一串类似 UUID 的字符串替代，但是扩展名可以保留，且扩展名是后端程序指定的。
     * 目前后端支持的扩展名有：tar, tar.gz, zip
     */

    //
    QString err_msg;

    //
    QString file_path = _req_upload_file.dirPath;
    if (!file_path.endsWith(QDir::separator())) {
        file_path += QDir::separator();
    }
    if (file_path != QDir::separator()) {
        file_path += _req_upload_file.name;
    } else {
        file_path = _req_upload_file.name;
    }

    //
    bool is_succ = false;
    do {
        // 检查文件上传服务地址是否有效
        if (uploadSvcUrl.isEmpty() || !uploadSvcUrl.isValid()) {
            err_msg = "Internal error: upload service url is not valid!";
            emit sigUploadAborted(err_msg);
            break;
        }

        // 不管是上传文件，还是上传文件夹，都压缩为 zip 格式
        bool succ_compress = zipFileOrDir(file_path, file_path, err_msg);
        if (!succ_compress) {
            //err_msg = QString("packaging file/dir \"%1\" failed! err: ").arg(file_path) + err_msg;
            err_msg = QString("压缩文件/文件夹 \"%1\" 失败！错误: ").arg(file_path) + err_msg;
            emit sigUploadAborted(err_msg);
            break;
        }

        //
        QFileInfo file_info(file_path);
        emit sigMessage(logType_info, QString("file size = %1").arg(file_info.size()));

        //
        uploadSvcUrl.setQuery("equiment=" + service->getSenderNum());

        //
        int http_status = 0;
        QByteArray body;
        qint64 bytes_sent = 0;
        qint64 bytes_total = 0;

        QNetworkReply::NetworkError err_reply = uploadHttpMultipartFile(uploadSvcUrl, file_path, http_status, body, bytes_sent, bytes_total);

        //
        emit sigMessage(logType_info, "response body: " + QString::fromUtf8(body));

        // 超时或逻辑异常错误检查
        if (err_reply < 0) {
            if (-1 == (int)err_reply) {
                err_msg = QString("upload failed! error: timeout! bytes_sent = %1, total = %2").arg(bytes_sent).arg(bytes_total);
                emit sigUploadAborted(err_msg);
                break;
            } else {
                err_msg = QString("upload failed! error: program logic error! bytes_sent = %1, total = %2").arg(bytes_sent).arg(bytes_total);
                emit sigUploadAborted(err_msg);
                break;
            }
        }

        // 若应答出错
        if (QNetworkReply::NoError != err_reply) {
            err_msg = QString("Upload failed! reply error: %1, bytes_sent = %2, total = %3").arg(QVariant::fromValue(err_reply).toString()).arg(bytes_sent).arg(bytes_total);
            emit sigUploadAborted(err_msg);
            break;
        }

        // http 状态码须为 200
        if (200 != http_status) {
            err_msg = "http status code of upload interface is not 200!";
            emit sigUploadAborted(err_msg);
            break;
        }

        // 应答 json 的解析及状态码字段的值的检查
        QString key_str = "\"code\":";
        int idx1 = body.indexOf(key_str);
        if (idx1 < 0) {
            err_msg = "parsing json of reply of upload interface failed(\"code\" not found)!";
            emit sigUploadAborted(err_msg);
            break;
        }
        int idx2 = body.indexOf(",", idx1);
        if (idx2 < 0) {
            err_msg = "parsing json of reply of upload interface failed(\",\" not found)!";
            emit sigUploadAborted(err_msg);
            break;
        }

        QString code_str = QString::fromUtf8(body.mid(idx1 + key_str.length(), idx2 - idx1 - key_str.length()));
        int code = code_str.toInt();
        if (200 != code) {
            err_msg = "\"code\" of json of reply of upload interface is not 200!";
            emit sigUploadAborted(err_msg);
            break;
        }

        //
        is_succ = true;

        // 删除打包文件夹产生的临时文件
        if (succ_compress) {
            bool succ_del = QFile::remove(file_path);
            if (!succ_del) {
                QString msg = "delete temp file \"" + file_path + "\" failed!";
                //logWarning(msg);
                emit sigMessage(logType_warning, msg);
            }
        }

    } while (false);

    //
    if (is_succ) {
        emit sigMessage(logType_info, "文件 \"" + file_path + "\" 上传成功");
    } else {
        emit sigMessage(logType_info, "文件 \"" + file_path + "\" 上传失败！err: " + err_msg);
    }
}

QNetworkReply::NetworkError CFileUpload::uploadHttpMultipartFile(const QUrl &_url, const QString &_file_path, int &_http_stat, QByteArray &_body,
                                                                 qint64 &_bytes_sent, qint64 &_bytes_total)
{
    // 文件上传
    /* 接口定义：http://120.25.254.38:8080/swagger-ui.html#!/216442599120214303402550921475/uploadEquipmentRunInfoFileUsingPOST
     * 上传完即可，不需要继续发送文件上传的应答。
     * 结果检查逻辑：若 http 状态码为 200，则按接口 json 格式解析获取 code 字段的值，若不为 200，则表示发生错误。
     */

    QNetworkReply::NetworkError err_reply = (QNetworkReply::NetworkError)-100;      // 错误码初始值，设为小于 0 的未有规定其意义的任意值即可

    QNetworkReply *reply = Q_NULLPTR;

    do {
        // 打开文件
        QFile *file = new QFile(_file_path);
        bool is_open_succ = file->open(QFile::ReadOnly);
        if (!is_open_succ) {
            emit sigUploadAborted(QString("opening file \"%1\" failed!").arg(_file_path));
            break;
        }

        // HTTP 请求
        QNetworkRequest request(_url);      // TODO: 改用 Common::Net::sendHttpRequest()？
        //request.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data");     // TODO: 为什么加上这句后，服务端发生 “java.io.IOException: Stream closed” 异常？

        // 文件上传的表单数据
        QHttpPart part_file;
        QString part_file_header = QString("form-data; name=\"multipartFile\"; filename=\"%1\"").arg(file->fileName());
        part_file.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(part_file_header));
        part_file.setBodyDevice(file);

        QHttpMultiPart *multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        multi_part->append(part_file);

        // 发送 HTTP POST 请求
        service->emitLog(logType_info, "正在上传文件 \"" + _file_path + "\"");
        reply = s_netManager->post(request, multi_part);

        // 资源释放的依赖
        file->setParent(multi_part);
        multi_part->setParent(reply);   // NOTE: multi_part 的 parent 应该选 request，但 request 不是 QObject 的派生类

        //
        static const int TIMEOUT_RESPONSE = 30 * 1000;              // 超时时长（毫秒），指连续无数据传输的时长

        QElapsedTimer elapsedTimer;                                 // 超时的计时
        elapsedTimer.start();

        bool is_timeout = false;
        QObject::connect(reply, &QNetworkReply::uploadProgress, [&](qint64 __bytes_sent, qint64 __bytes_total) {     // 通过上传进度信号检测超时
            if (__bytes_sent > 0) {                                  // 如果有数据传输，重置计时器
                _bytes_sent = __bytes_sent;
                _bytes_total = __bytes_total;
                elapsedTimer.restart();
            }
            if (elapsedTimer.elapsed() > TIMEOUT_RESPONSE) {        // 检查是否超时
                _body = reply->readAll();       // abort() 后无法再读取，所以先读取
                reply->abort();                 // 若超时，中断请求
                is_timeout = true;
            }   // TODO: 这里检测不到超时？因为如果有 uploadProgress 事件，是否有必要超时？若没 uploadProgress 事件，这里不会被执行？改为用定时器定时检查？
        });

        // 通过事件循环等待请求执行结束           // TODO: 这里好像没必要用事件循环来阻塞？可通过信号槽
        QEventLoop event_loop;

        QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
        event_loop.exec(QEventLoop::ExcludeUserInputEvents);

        // http 应答 body
        if (reply->isOpen()) {
            _body = reply->readAll();
        } else {
            emit sigMessage(logType_error, "request connection is closed, can't read body!");
        }

        // 超时检查
        if (is_timeout) {
            err_reply = (QNetworkReply::NetworkError)-1;
            break;
        }

        // 网络错误码
        err_reply = reply->error();
        if (QNetworkReply::NoError != err_reply) {
            break;
        }

        // http 应答状态码
        QVariant stat_code_var = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);        // http 状态码检查
        _http_stat = stat_code_var.toInt();
        if (200 != _http_stat) {
            break;
        }

    } while (false);

    //
    if (reply) {
        reply->deleteLater();       // NOTE: QNetworkReply 须由调用者释放。
    }

    //
    return err_reply;
}

}   // namespace Remote
}   // namespace Net
