#include "util-common.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QDateTime>
#include <QImage>
#include <QProcess>
#include <QApplication>
#include <QTime>
#include <QLocale>
#include <QCollator>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>

#include <sys/statfs.h>
#include <execinfo.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <sys/time.h>
#include <math.h>

/// namespace Util begin ================================

namespace Util {

//void initialize()
//{

//}

//void uninitialize()
//{

//}

/// =============================================================================================================================
/// class CScreenerImgsData

constexpr char CScreenerImgsData::C_FILE_EXT[];

CScreenerImgsData::CScreenerImgsData(QObject *parent) :
    QObject(parent)
{
    imgsDir = QCoreApplication::applicationDirPath() + QDir::separator() + "simulate-imgs";

    QObject::connect(this, &CScreenerImgsData::sigFinishImgSaving, this, &CScreenerImgsData::slotFinishImgSaving, Qt::QueuedConnection);
    QObject::connect(this, &CScreenerImgsData::sigSaveSimulateImgToFile, this, &CScreenerImgsData::slotSaveSimulateImgToFile, Qt::QueuedConnection);

    isSaveToFile = true;

    this->moveToThread(&mWorkThread);
    mWorkThread.start(QThread::NormalPriority);

}

CScreenerImgsData::~CScreenerImgsData()
{
    QObject::disconnect(this, &CScreenerImgsData::sigFinishImgSaving, this, &CScreenerImgsData::slotFinishImgSaving);
    QObject::disconnect(this, &CScreenerImgsData::sigSaveSimulateImgToFile, this, &CScreenerImgsData::slotSaveSimulateImgToFile);

    mWorkThread.exit();
    mWorkThread.wait(10000);
    if (mWorkThread.isRunning()) {
        mWorkThread.terminate();
    }

    return;
}

void CScreenerImgsData::setImgSize(int _img_width, int _img_height)
{
    imgWidth = _img_width;
    imgHeight = _img_height;
}

void CScreenerImgsData::saveSimulateImageToFile(unsigned char* _data, QString _name)
{
    unsigned char* data = (unsigned char*)malloc(getImgDataLen());
    memcpy(data, _data, getImgDataLen());
    struct stImgData img_file_buff = {data, _name};
    mImgFilesBuff.append(img_file_buff);
}

void CScreenerImgsData::slotFinishImgSaving()
{
    QDir dir(imgsDir);
    if (!dir.exists())
        if (!dir.mkpath(imgsDir))
            return;

    for (int i = 0; i < mImgFilesBuff.size(); i++) {
        unsigned char* data = mImgFilesBuff.at(i).data;
        QString name = mImgFilesBuff.at(i).name;

        printf("doSaveBufferToFiles() begin : _name=%s\n", name.toLatin1().data());

        QString file_path = imgsDir + QDir::separator() + name;
        QFile file(file_path);
        bool open_succ = file.open(QFile::WriteOnly | QFile::Truncate);
        if (open_succ) {
            file.write((char*)data, getImgDataLen());
            file.flush();
            file.close();
        } else {
            std::cout << "CScreenerImgsData::slotFinishImgSaving(): open file failed!" << std::endl;
            // TODO:
        }

        if (isSaveToFile) {
            Util::saveImgDataToImgFile(data, imgWidth, imgHeight, file_path + C_FILE_EXT);
        }

        free(data);
        data = nullptr;
    }
    mImgFilesBuff.clear();
}

bool CScreenerImgsData::readSimulateImageFromFile(unsigned char* _data, int _idx)
{
    printf("readSimulateImageFromFile() begin : _idx=%d\n", _idx);

    QString name;
    if (!getFileNameByIndex(_idx, name)) {
        if (0 == _idx && !isNeedZeroth)
            return true;
        else
            return false;
    }

    bool is_succ = false;
    QString file_path = imgsDir + QDir::separator() + name;
    QFile file(file_path);
    if (file.exists()) {
        if (imgFileType_ImgData == fileType) {
            file.open(QFile::ReadOnly);
            file.read((char*)_data, getImgDataLen());
            file.close();

            return true;
        } else if (imgFileType_Bmp == fileType) {
            IplImage *img = cvLoadImage(file_path.toLatin1().data(), CV_LOAD_IMAGE_GRAYSCALE);
            if (img && img->imageData) {
                memcpy(_data, img->imageData, getImgDataLen());
                is_succ = true;
            }
            cvReleaseImage(&img);
        }
    }
    return is_succ;
}

bool CScreenerImgsData::getFileNameByIndex(int _idx, QString &_file_name)
{
    _file_name = "";
    if (mFileNames.size() > 0) {
        for (stFileNameInfo file_name_info : mFileNames) {
            if (file_name_info.index == _idx) {
                _file_name = file_name_info.name;
                break;
            }
        }
    }
    if (_file_name.length() == 0) {
        if (_idx < 0) {
            _file_name = QString("PRE") + C_FILE_EXT;
        } else if (isNeedZeroth) {
            _file_name = QString("%1%2").arg(_idx, 2, 10, QLatin1Char('0')).arg(C_FILE_EXT);
        }
    }
    return (_file_name.length() > 0);
}

void CScreenerImgsData::addFileName(int _idx, QString &_file_name)
{
    stFileNameInfo file_name_info;
    file_name_info.index = _idx;
    file_name_info.name = _file_name;
    mFileNames.append(file_name_info);
}

void CScreenerImgsData::slotSaveSimulateImgToFile()
{
    for (int i = 0; i < mImgToFileBuff.size(); i++)
    {
        unsigned char* data = mImgToFileBuff.at(i).data;
        QString name = mImgToFileBuff.at(i).name;

        printf("doOnSigSaveSimulateImgToFile() begin : _name=%s\n", name.toLatin1().data());

        QString file_path = imgsDir + QDir::separator() + name;
        Util::saveImgDataToImgFile(data, imgWidth, imgHeight, file_path + C_FILE_EXT);

        free(data);
        data = nullptr;
    }
    mImgToFileBuff.clear();
}

void CScreenerImgsData::saveSimulateImgToFile(unsigned char* _data, QString _name)
{
    if (!isSaveToFile)
        return;

    // memory copy
    unsigned char* data = (unsigned char*)malloc(getImgDataLen());
    memcpy(data, _data, getImgDataLen());
    struct stImgData img_buf = {data, _name};
    mImgToFileBuff.append(img_buf);

    // signal emit
    emit sigSaveSimulateImgToFile();
}

void CScreenerImgsData::saveSimulateImage(unsigned char* _data, int _idx)
{
    printf("saveSimulateImage() begin : _i=%d\n", _idx);

    QString name;
    if (-1 == _idx) {
        // 保存预测量图像到文件
        if (getFileNameByIndex(-1, name))
            saveSimulateImageToFile(_data, name);
        return;
    }

    if ((_idx < 0) || (_idx > 23 - 1))
        return;

    if (getFileNameByIndex(_idx, name))
        saveSimulateImageToFile(_data, name);

    //printf("saveSimulateImage() end : _i=%d\n", _i);
}

unsigned char* CScreenerImgsData::getImage(int _idx)
{
    //printf("getSimulateImage() begin : _i=%d\n", _i);

    if (!isSimulateImgsInitted)
        loadImgFiles();

    if (-1 == _idx) {
        return m_pPreImgData;

        //return m_vSimulageImgs.at(curr_pre);
        //curr_pre++;
        //if (curr_pre > 23 - 1)
        //    curr_pre = 0;
    }
    else {
        //curr_pre = 0;

        if (_idx < 0)
            _idx = 0;
        if (_idx > 23 - 1)
            _idx = 23 - 1;
        return mImgDataVec.at(_idx);
    }
}

void CScreenerImgsData::finishImgSaving()
{
    emit sigFinishImgSaving();
}

int CScreenerImgsData::getImgDataLen()
{
    return imgWidth * imgHeight;
}

void CScreenerImgsData::clearFileNames()
{
    mFileNames.clear();;
}

bool CScreenerImgsData::loadImgFiles(bool _force)      // 返回值:是否载入模拟抓图数据成功; _force: 是否强制重新从磁盘读取
{
    if (isSimulateImgsInitted && (!_force))
        return true;

    printf("CScreenerImgsData::loadImgFiles() begin : _force=%s\n", bool2str(_force));

    //
    clearFileNames();
    QDir dir_imgs(imgsDir);
    QFileInfoList file_info_list = dir_imgs.entryInfoList();
    for (QFileInfo file_info : file_info_list) {
        QString file_name = file_info.fileName();
        if (file_name.endsWith(C_FILE_EXT)) {           // 命名规则：文件名最靠前的连续的数字字符作为图像索引号，文件类型为 bmp
            int idx = getIndexFromFileName(file_name);
            addFileName(idx, file_name);
        }
    }

    //
    if (!m_pPreImgData && isLoadPreImg)
    {
        m_pPreImgData = (unsigned char*)malloc(getImgDataLen());
        memset(m_pPreImgData, 0, getImgDataLen());
    }
    if (0 == mImgDataVec.size())
    {
        for (int i = 0; i < 23; i++)        // 即使没有文件，也初始化内存
        {
            unsigned char* buff = (unsigned char*)malloc(getImgDataLen());   // TODO: free ?
            memset(buff, 0, getImgDataLen());
            mImgDataVec.append(buff);
        }
    }

    isSimulateImgsInitted = true;

    if (isLoadPreImg) {
        if (!readSimulateImageFromFile(m_pPreImgData, -1))
            return false;
    }
    for (int i = 0; i < 23; i++)
    {
        if (!readSimulateImageFromFile(mImgDataVec.at(i), i))
            return false;
    }

    return true;
}

int CScreenerImgsData::getIndexFromFileName(QString _file_name)
{
    QString idx_str;
    for (int i = 0; i < _file_name.length(); i++) {
        if (_file_name[i].isNumber()) {
            idx_str += _file_name[i];
        } else if (idx_str.length() > 0) {
            break;
        }
    }
    int idx = idx_str.toInt();
    return idx;
}

/// ================================================================================================
/// use or save simulate image

Util::CScreenerImgsData *g_simulateCapture = NULL;

Util::CScreenerImgsData *getSimulateCaptureInst()
{
    if (!g_simulateCapture) {
        g_simulateCapture = new Util::CScreenerImgsData();          // TODO: 按照  screener-unit-test_desktop.pro -> CVisionMeasure::calcVisionOfImgs() 的用法优化
        g_simulateCapture->isLoadPreImg = false;
        g_simulateCapture->fileType = Util::imgFileType_Bmp;
        g_simulateCapture->isNeedZeroth = false;
        g_simulateCapture->useSimulateImage = false;
    }
    return g_simulateCapture;
}

void setSaveSimulateImg(bool _is_save, int _img_width, int _img_height)
{
    if (getSaveSimulateImg() == _is_save) {
        return;
    }

    //
    getSimulateCaptureInst()->isSaveSimulateImage = _is_save;
    if (_is_save) {
        getSimulateCaptureInst()->useSimulateImage = false;
        getSimulateCaptureInst()->setImgSize(_img_width, _img_height);

        //
        getSimulateCaptureInst()->loadImgFiles();
    }
}

bool setUseSimulateImg(bool _is_use, int _img_width, int _img_height)
{
    if (getUseSimulateImg() == _is_use) {
        return _is_use;
    }

    //
    getSimulateCaptureInst()->useSimulateImage = _is_use;
    if (_is_use) {
        getSimulateCaptureInst()->isSaveSimulateImage = false;
        getSimulateCaptureInst()->setImgSize(_img_width, _img_height);

        // 载入模拟抓图数据
        if (!getSimulateCaptureInst()->loadImgFiles(true))
        {
            getSimulateCaptureInst()->useSimulateImage = false;

            QMessageBox::information(Q_NULLPTR, "error", "载入模拟抓图数据失败!\n请先保存模拟抓图数据");

            _is_use = false;
        }
    }

    return _is_use;
}

bool getSaveSimulateImg()
{
    return getSimulateCaptureInst()->isSaveSimulateImage;
}

bool getUseSimulateImg()
{
    return getSimulateCaptureInst()->useSimulateImage;
}

void saveSimulateImg(uchar *_img_data, int _turnlamp_buff_size)
{
    if (getSimulateCaptureInst()->isSaveSimulateImage)
    {
        //unsigned long tick_save_simu_img = clock();       // TODO: clock() 仅适用于单核的设备，应改用 clock_gettime() ？

        if (!getSimulateCaptureInst()->isSimulateImgsInitted)
            getSimulateCaptureInst()->loadImgFiles();

        if (_turnlamp_buff_size > 0) {      // 抓图期
            if (_turnlamp_buff_size == 1) {      // 抓第1张图时保存通过检查的预检图像
                getSimulateCaptureInst()->saveSimulateImage(getSimulateCaptureInst()->m_pPreImgData, -1);
            }

            getSimulateCaptureInst()->saveSimulateImage(_img_data, _turnlamp_buff_size - 1);
        } else {    // 复制预检期图像到内存
            memcpy(getSimulateCaptureInst()->m_pPreImgData, _img_data, getSimulateCaptureInst()->getImgDataLen());
        }

        //logDebug(QString::asprintf("SaveSimulateImage time consuming : %lu", clock() - tick_save_simu_img));
    }
}

void finishSimulateImgSaving()
{
    getSimulateCaptureInst()->finishImgSaving();
}

void useSimulateImg(uchar *_img_data, int _turnlamp_buff_size)
{
    if (getSimulateCaptureInst()->useSimulateImage) {
        //unsigned long tick_use_simu_img = clock();        // TODO: clock() 仅适用于单核的设备，应改用 clock_gettime() ？

        if (!getSimulateCaptureInst()->isSimulateImgsInitted)
        {
            //logCritical(QString::asprintf("warning: isSimulateImgsInitted = false before calling getImage()"));
            getSimulateCaptureInst()->loadImgFiles();
        }

        if (_turnlamp_buff_size > 0) {  // 抓图期
            int i = _turnlamp_buff_size + 1;
            i = (i >= 23) ? 23 - 1 : i - 1;
            unsigned char *simulate_img = getSimulateCaptureInst()->getImage(i);
            memcpy(_img_data, simulate_img, getSimulateCaptureInst()->getImgDataLen());
        }
        else {  // 预检期
            unsigned char *simulate_img = getSimulateCaptureInst()->getImage(-1);
            memcpy(_img_data, simulate_img, getSimulateCaptureInst()->getImgDataLen());
        }

        //logDebug(QString::asprintf("UseSimulateImage time consuming : %lu", clock() - tick_use_simu_img));
    }
}

/// =============================================================================================================================
/// class CUDisk

QString CUDisk::s_path = "";

CUDisk::CUDisk(QObject *parent) : QObject(parent)
{

}

CUDisk::~CUDisk()
{

}

void CUDisk::remount()
{
#if (1 == OS_TYPE)
    system("umount /run/media/sda1");
    system("mount -t auto -o iocharset=utf8 /dev/sda1 /run/media/sda1");
    //system("mount -t auto -o iocharset=gbk /dev/sda1 /run/media/sda1");
#endif
}

QString CUDisk::getPath()
{
    s_path = "";

#if (OS_TYPE != 2)
#  if (OS_TYPE == 1)
    QString base_dir = "/run/media";
    QDir mydir;
    mydir.setPath(base_dir);
    mydir.setFilter(QDir::Dirs);
    //mydir.setSorting(QDir::Name);
    QFileInfoList list = mydir.entryInfoList();
    int size = list.length();
    if (size > 2) {
        QFileInfo fileInfo;
        QString base_name;
        for (int i = 0; i < list.length(); i++) {
            fileInfo = list.at(i);
            base_name = fileInfo.baseName();
            if (base_name.startsWith("sd",Qt::CaseSensitive)&&
                    fileInfo.isReadable() &&fileInfo.isWritable()) {
                s_path = base_dir + QDir::separator() + base_name;
                break;
            }
        }
    }
#  else
    /* 思路：“mount -l”，搜索“/media/usb* ”，最后核验该目录是否存在 */

    const QString UDISK_PATH_PRE = "/media/usb";

    s_path = "";
    QString mount_info;
    Util::executeLinuxCmd("mount -l", &mount_info);
    int idx1 = mount_info.indexOf(UDISK_PATH_PRE);
    int idx2 = mount_info.indexOf(" ", idx1);
    if (idx1 >= 0 && idx2 >= 0) {
        s_path = mount_info.mid(idx1, idx2 - idx1);
        qDebug() << "got udisk path from mount info: " << s_path;
    } else {
        qDebug() << "udisk path not found from mount info!";
    }
    if (!QFile::exists(s_path)) {
        qDebug() << "udisk path not exists?! setted to empty.";
        s_path = "";
    }
#  endif
#else
    s_path = QDir::homePath() + "/test/udisk/";           // TODO: 检查 U 盘文件系统格式，并赋值到 format 变量
    if (!QFile::exists(s_path))
        s_path = "";
#endif

    return s_path;
}

bool CUDisk::sync()
{
    int ret = system("sync");
    return isSystemCmdSucc(ret);
}

bool CUDisk::umount(bool _del_path)
{
#if (OS_TYPE != 2)
    int ret = system((QString("umount ") + s_path).toLocal8Bit().data());
    //qDebug() << "ret = " << ret;
    bool succ = isSystemCmdSucc(ret);
    //qDebug() << "succ = " << succ;
#if 1 == OS_TYPE
    // 删除U盘目录，防止卸载后目录还在，而程序根据目录是否存在的方法判断 U 盘是否已插入时判断错误而导致保存文件到 U 盘时实际上保存到了本地磁盘，且界面又没有任何提示，导致用户得不到本要保存到 U 盘的文件（2022-03-15）
    if (_del_path) {                                    // 注意：在 rk3568 平台，U盘路径不可删除，否则导致以后没有这个目录导致U盘自动挂载失败
        if (succ)
        {
            if (isUmounted()) {
                system(QString("rmdir %1").arg(path).toLocal8Bit().data());
            }
        }
    }
#else
    Q_UNUSED(_del_path)
#endif
    return succ;
#else
    Q_UNUSED(_del_path)
    return true;
#endif
}

bool CUDisk::isUmounted()
{
    if (QFile::exists(s_path)) {
        struct stat file_info, file_info_parent;
        stat(s_path.toLocal8Bit().data(), &file_info);
        stat((s_path + (s_path[s_path.length() - 1] == '/' ? ".." : "/..")).toLocal8Bit().data(), &file_info_parent);
        bool is_device_same = (file_info.st_dev == file_info_parent.st_dev);
        //qDebug() << s_path << " is_device_same = " << is_device_same;
        return is_device_same;
    } else {
        //qDebug() << "file " << s_path << "not exists";
        return true;
    }
}

/// =============================================================================================================================
/// 其它函数

bool isSystemCmdSucc(int _status)
{
    //
    if (_status != -1) {                // 检查是否无法启动子进程（例如内存不足或 shell 不可用）
        if (WIFEXITED(_status)){        // 检查进程是否正常退出（通过 exit 或 return）
            int exit_status = WEXITSTATUS(_status);     // 提取进程的退出状态
            if (exit_status == 0) {
                return true;
            } else {
                printf("Command failed with code %d.\n", exit_status);
                return false;
            }
       } else {
           printf("Process was terminated by signal or stopped.");
           return false;
       }
    } else {
       perror("Failed to invoke the shell");
       return false;
    }
}

int compDouble(const double _d1, const double _d2)
{
    double d = _d1 - _d2;
    if (qFuzzyIsNull(d))
        return 0;
    else
        return (d < 0 ? -1 : 1);
}

const char * bool2str(bool _bool)
{
    return (_bool ? "true" : "false");
}

QString boolToYesNo(bool _bool)
{
    return (_bool ? QCoreApplication::translate("util.cpp", "是") : QCoreApplication::translate("util.cpp", "否"));  // "Yes"  "No"
}

bool qstr2bool(QString _str)
{
    return (_str == QString("true"));
}

int strToInt(const QString &_str, int _default)
{
    bool ok;
    int i = _str.toInt(&ok, 10);
    return (ok ? i : _default);
}

int variantToInt(const QVariant &_variant, int _default)
{
    bool ok;
    int i = _variant.toInt(&ok);
    return (ok ? i : _default);
}

bool isStrEmpty(const char *_str)
{
    return ((!_str) || (strlen(_str) == 0));
}

QRect cvrect2qrect(CvRect _cv_rect)
{
    return QRect(_cv_rect.x, _cv_rect.y, _cv_rect.width, _cv_rect.height);
}

uint randomInt()
{
    static bool inited = false;
    if (!inited) {      // 只需且只能初始化一次，否则每次 rand() 返回的值都一样
        srand(time(NULL));
        inited = true;
    }
    return rand();
}

void IplImageToQImage(IplImage &_ipl_img, QImage &_q_img)
{
    // TODO:
}

bool executeLinuxCmd(const QString &_cmd, QString *_std_out)
{
    QProcess proc;
    proc.start("bash", QStringList() << "-c" << _cmd);
    proc.waitForFinished(10000);

    if (_std_out) {
        proc.setReadChannel(QProcess::StandardOutput);
        *_std_out = proc.readAllStandardOutput();
    }

    return ((QProcess::NormalExit == proc.exitStatus()) && (EXIT_SUCCESS == proc.exitCode()));
}

// 从指定位置开始，从给定的字符串中读取第一行的内容（含换行符），返回换行符之后下一个字符的索引。若无换行符，返回0。
int readLine(QByteArray & _str, QByteArray &_line_str, int _from)
{
    /* unix 系统的换行符为 "\n"，windows 系统的换行符为 "\r\n"，都是以 "\n" 结尾。 */
    int i = _str.indexOf("\n", _from);
    if (i >= 0)
        i += 1;
    else
        i = 0;

    _line_str = (i > _from) ? _str.mid(_from, i - _from) : "";

//    if (_trim) {
//        int n = _line_str.length() - 1;
//        if (_line_str.length() && '\n' == _line_str[n]) {
//            _line_str.truncate(n);
//            n--;
//        }
//        if (_line_str.length() && '\r' == _line_str[n]) {
//            _line_str.truncate(n);
//            n--;
//        }
//    }

    return i;
}

int readLine(QString & _str, QString & _line_str, int _from)        // TODO: 用模板实现？
{
    QByteArray bytes_str = _str.toLatin1();
    QByteArray bytes_line;
    int r = readLine(bytes_str, bytes_line, _from);
    _line_str = bytes_line;
    return r;
}

int calcMonospacePrintingLength(const QString &_str)
{
    int length = 0;
    char ch_ascii;
    for (QChar ch : _str) {
        if (ch.isPrint()) {     // 判断是否可打印
            // 判断字符的 Unicode 范围，ASCII 字符为半角，其余一般为全角
            if (ch.unicode() < 0x80) {
                length += 1;    // 半角字符
            } else {
                length += 2;    // 全角字符
            }
        } else {
            if (ch.unicode() < 0x80) {
                ch_ascii = static_cast<char>(ch.unicode());
                switch (ch_ascii) {
                case '\t':      // Tab符（水平制表符）
                    length += 8;
                    break;
                }
            }
        }
    }
    return length;
}

bool sleepUntil(bool *_cond, bool _val, long _time_out_usec)
{
    const int INTERVAL_USEC = 1000;  // TODO: <bits/time.h> CLOCKS_PER_SEC 的值（=1000000），不同平台并不同？

    //clock_t tip_begin = clock();        // TODO: clock() 仅适用于单核的设备，应改用 clock_gettime() ？
    //while (clock() - tip_begin < _time_out_usec) {
    //    usleep(INTERVAL_USEC);
    //    if (_cond && (_val == *_cond)) {
    //        return true;
    //    }
    //}

    timeval tv_begin;
    int err_code = gettimeofday(&tv_begin, NULL);               // TODO: 若计时失败，本函数没有意义？改为休眠“超时时间”？      // TODO: 改用 clock_gettime() ？
    if (0 != err_code) {
        qCritical() << "gettimeofday() returns error?! code = " << err_code;
        return false;
    }

    __suseconds_t usec_begin = tv_begin.tv_sec * 1000000 + tv_begin.tv_usec;
    timeval tv_curr;
    do {
        err_code = gettimeofday(&tv_curr, NULL);            // TODO: 改用 clock_gettime() ？
        if (0 != err_code) {
            qCritical() << "gettimeofday() returns error?! code = " << err_code;
            return false;
        }

        usleep(INTERVAL_USEC);

        if (_cond && (_val == *_cond)) {
            return true;
        }

    } while ((tv_curr.tv_sec * 1000000 + tv_curr.tv_usec) - usec_begin < _time_out_usec);

    //
    return false;
}

// 让当前线程等待指定的毫秒数，且期间处理系统消息
bool waitMs(int _ms, int *_cond, int _val)
{
    int timeout = 20;

    if (_ms < timeout)
        timeout = _ms;

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < _ms) {
        if (NULL != _cond && *_cond == _val)
            return true;

        qApp->processEvents(QEventLoop::AllEvents, timeout);    // TODO: 如果当前线程不是 UI 线程，事件循环能执行吗？
        // TODO: 注意：这里调用了事件处理函数，如果是在按钮的点击、窗体的 showEvent() 等事件过程中调用本函数，可能导致事件代码执行次序的错乱？
    }

    return false;
}

// 计算crc（直接计算法 CRC-32/MPEG-2）
unsigned int getCrc32(unsigned char *ptr, int len)
{
    unsigned int i;
    unsigned int crc = 0xFFFFFFFF;

    while(len--)
    {
        crc ^= (unsigned int)(*ptr++) << 24;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ 0x04C11DB7;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// 用指定字符分隔字符串，回传字符两侧的字符串，返回分隔字符的索引
int splitStr(const QString &_str, const QString &_sep, QString &_sub_str1, QString &_sub_str2, int _from)
{
    int idx = _str.indexOf(_sep, _from);
    if (idx >= 0) {
        _sub_str1 = _str.left(idx);
        _sub_str2 = _str.mid(idx + 1);

        return idx;
    } else {
        return -1;
    }
}

// 从指定位置开始，截取两个给定的分隔符之间的部分，并返回第二个分隔符的索引
bool getSeparatedStr(QString _str, QString _sepa1, QString _sepa2, QString &_sub_str, int _from)
{
    _sub_str = "";

    QString str1, str2, str3;
    bool found = false;
    if (Util::splitStr(_str, _sepa1, str1, str2, _from) >= 0) {
        if (Util::splitStr(str2, _sepa2, str1, str3, _from) >= 0) {
            _sub_str = str1;
            found = true;
        }
    }

    return found;
}

//
bool separateStr(const QString &_str, QChar _separator, int _n, QString &_sub_str1, QString &_sub_str2)
{
    int pos = -1;

    if (-1 == _n) {
        pos = _str.lastIndexOf(_separator);         // TODO: 用 QString::.section() + QString::count(QChar) ？
    } else if (_n >= 0) {
        int from = 0;
        for (int i = 0; i <= _n; i++) {
            pos = _str.indexOf(_separator, from);
            if (pos >= 0) {
                from = pos + 1;
            } else {
                return false;
            }
        }
    }

    if (pos >= 0) {
        _sub_str1 = _str.mid(0, pos);
        _sub_str2 = _str.mid(pos + 1);
        return true;
    } else {
        return false;
    }
}

void splitStrToFields(const QString &_str, QStringList &_list_str, const QChar &_sep)
{
    static constexpr QChar CHAR_QUO     = '"';

    //
    _list_str.clear();

    //
    bool is_err = false;

    //
    QString sub_str;
    int pos_last = -1;
    int pos_curr = -1;
    bool is_quo_started = false;
    int pos_quo = -1;
    do {
        pos_last = pos_curr;
        pos_curr = _str.indexOf(_sep, pos_curr + 1);

        if (pos_curr >= 0) {    // 若找到分割符
            // 分割符之前的部分为当前字段值
            sub_str = _str.mid(pos_last + 1, pos_curr - (pos_last + 1));

            // 若有开双引号则特殊处理，否则添加当前字段值
            is_quo_started = sub_str.startsWith(CHAR_QUO);
            if (is_quo_started) {
                // 往后面查找收双引号，且将开、收双引号之间的部分作为当前字段值
                pos_quo = _str.indexOf(CHAR_QUO, pos_curr + 1);         // TODO: 支持双引号的转义？
                if (pos_quo >= 0) {
                    // 收双引号，须是最后一个字符，或者下一个字符是分割符，否则格式错误
                    if ((pos_quo == _str.length() - 1) || ((pos_quo < _str.length() - 1) && (_sep == _str[pos_quo + 1]))) {
                        sub_str = _str.mid(pos_last + 2, pos_quo - (pos_last + 2));

                        _list_str.append(sub_str);
                        pos_curr = pos_quo + 1;
                    } else {
                        qCritical() << __PRETTY_FUNCTION__ << ": format error: ending of quotation not last char and next char not separator!";
                        is_err = true;
                        break;
                    }
                } else {
                    // 若找不到收双引号，则格式错误
                    qCritical() << __PRETTY_FUNCTION__ << ": format error: ending of quotation not found!";
                    is_err = true;
                    break;
                }
            } else {
                _list_str.append(sub_str);
            }
        } else {                // 若找不到分割符
            // 从查找开始位置到字符串末尾，为当前字段值，并结束分割
            sub_str = _str.mid(pos_last + 1);

            if (sub_str.startsWith(CHAR_QUO)) {     // 去掉开双引号
                sub_str = sub_str.mid(1);
            }
            if (sub_str.endsWith(CHAR_QUO)) {       // 去掉收双引号
                sub_str = sub_str.left(sub_str.length() - 1);
            }

            _list_str.append(sub_str);

            break;
        }
    } while (pos_curr >= 0);

    //
    if (is_err) {
        qCritical() << "str which format error is: " << _str;
        _list_str.clear();
    }
}

// 对字符串列表进行排序，支持中文
void sortStringList(QStringList &_str_list)     //TODO: QDir::entryInfoList(, QDir::SortFlag) 怎么实现的？
{
    QLocale locale(QLocale::Chinese);   // TODO: 视筛仪的嵌入式 Linux 系统未支持中文语言，只支持 ASCII 码排序？
    QCollator collator(locale);
    std::sort(_str_list.begin(), _str_list.end(), collator);
}

// 改变浮点数的精度
double roundDouble(double _double, double _precision)
{
    if (compDouble(_double, 0) != 0) {
        return round(_double / _precision) * _precision;
    } else {
        return _double;
    }
}

// 改变整数的精度
int roundInt(int _int, int _precision, bool _is_ceil)
{
    if (_precision > 1) {
        int r = _int % _precision;
        return (_is_ceil ? _int + r : _int - r);
    } else {
        return _int;
    }
}

//
QDate strToDate(const QString &_str)
{
    QDate date;

    if (_str.contains("/")) {
        date = QDate::fromString(_str, "yyyy/M/d");
    } else if (_str.contains("-")) {
        date = QDate::fromString(_str, "yyyy-M-d");
    } else if (_str.contains(".")) {
        date = QDate::fromString(_str, "yyyy.M.d");
    } else if (_str.length() == 8) {
        date = QDate::fromString(_str, "yyyyMMdd");
    }

    return date;
}

// 转换日期字符串的格式
QString formatDateStr(QString _date_str, QString _format)
{
    QDate date = strToDate(_date_str);
    return (date.isValid() ? date.toString(_format) : "");
}

bool strsToDate(const QString &_year_str, const QString &_month_str, const QString &_day_str, QDate &_date)
{
    do {
        bool ok;
        int year = _year_str.toUInt(&ok, 10);
        if (!ok)
            break;
        int month = _month_str.toUInt(&ok, 10);
        if (!ok)
            break;
        int day = _day_str.toUInt(&ok, 10);
        if (!ok)
            break;
        _date.setDate(year, month, day);
        return _date.isValid();
    } while (false);
    return false;
}

// 计算字符串的宽度，单位为字符个数（每个中文字符算2个，每个英文字符算1个）
int calcTextCharWidth(QString _str)
{
    int w = 0;
    for (int i = _str.size() - 1; i >= 0; i--) {
        w += (_str[i].unicode() < 256 ? 1 : 2);
    }
    return w;
}

double max3(double _v1, double _v2, double _v3)
{
    double ret = (_v1 > _v2 ? _v1 : _v2);
    ret = (ret > _v3 ? ret : _v3);
    return ret;
}

// 保存图像数据到图像文件 ( use cvSaveImage(), surport jpg quality setting )
bool saveImgDataToImgFile(uchar *_img_data, int _img_width, int _img_height, QString _file_path, int _channels)
{
    QString dir_path = getDirOfPath(_file_path);
    QDir dir(dir_path);
    if (!dir.exists()) {
        dir.mkpath(dir_path);
    }

    int *params = Q_NULLPTR;
    if (_file_path.endsWith(".jpg")) {
        int p[3];
        p[0] = CV_IMWRITE_JPEG_QUALITY;
        p[1] = 20;
        p[2] = 0;
        params = p;
    }

    IplImage *img = cvCreateImageHeader(cvSize(_img_width, _img_height), IPL_DEPTH_8U, _channels);
    cvSetData(img, _img_data, _img_width);      // TODO: 如果是 3 通道，这里会出错？（CFrameDrawer::updateFrame() 的调用）
    bool succ = cvSaveImage(_file_path.toLocal8Bit().data(), img, params);
    cvReleaseImageHeader(&img);

    return succ;
}

// 保存图像数据到图像文件 ( use cv::imwrite() )
bool saveImgDataToImgFile2(uchar *_img_data, int _img_width, int _img_height, QString _file_path, int _channels)
{
    QString dir_path = getDirOfPath(_file_path);
    QDir dir(dir_path);
    if (!dir.exists()) {
        dir.mkpath(dir_path);
    }

    cv::Mat img(_img_height, _img_width, CV_MAKETYPE(CV_8U, _channels), _img_data);     // TODO: 这个 cv::Mat 释放后会不会把 image data 也释放了吧？
    bool succ = cv::imwrite(_file_path.toStdString(), img);
    return succ;
}

QString getDirOfPath(QString _file_path)
{
    int idx_last_sep = _file_path.lastIndexOf(QDir::separator());
    QString dir_str = _file_path.mid(0, idx_last_sep);
    return dir_str;
}

bool makePath(QString _dir_path)
{
    if (!QFile::exists(_dir_path)) {
        QDir dir(_dir_path);
        return dir.mkpath(_dir_path);
    } else {
        return true;
    }
}

// 选择文件夹
QString selectDir(QWidget *_parent, QString _default_dir)
{
    QFileDialog dlg(_parent);
    //dlg.setModal(true);
    //dlg.setDirectory(QDir::currentPath());
    dlg.setDirectory(_default_dir);
    dlg.setViewMode(QFileDialog::List);
    dlg.setFileMode(QFileDialog::DirectoryOnly);
    if (dlg.exec()) {
        QStringList file_list = dlg.selectedFiles();
        return file_list[0];
    }
    return "";
}

// 选择文件
QStringList selectFile(QWidget *_parent, QString _filter, bool _is_multi, QString _default_dir)
{
    QFileDialog dlg(_parent);
    //dlg.setModal(true);
    //dlg.setDirectory(QDir::currentPath());
    dlg.setDirectory(_default_dir);
    //dlg.setNameFilter("BMP File, JPG File, .csv (*.bmp *.jpg *.csv)");
    dlg.setNameFilter(_filter);
    dlg.setViewMode(QFileDialog::List);
    dlg.setFileMode(_is_multi ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
    if (dlg.exec()) {
        return dlg.selectedFiles();
    } else {
        return QStringList();
    }
}

// 计算平均灰度（单通道，略的）
float getAverageGrayRough(QString _file_path)
{
    cv::Mat img = cv::imread(_file_path.toStdString(), cv::IMREAD_GRAYSCALE);
    return getAverageGrayRough(img);
}

// 计算平均灰度（单通道，粗略的）
float getAverageGrayRough(cv::Mat _img)      // TODO: average = cv::sum(_mat)[0] / _mat.total(); ？
{
    float average_grey = 0;
    int rows = _img.rows;
    int cols = _img.cols;
    float total_grey = 0;
    int row = rows / 2;
    uchar *p;
    for (int col = 0; col < cols; col++) {
        p = _img.ptr<uchar>(row, col);
        total_grey += p[0];
    }
    average_grey = (float)total_grey / cols;

    float average_grey_2 = cv::sum(_img)[0] / _img.total();
    qDebug() << "average_grey = " << average_grey_2;

    return average_grey;
}

// 计算平均灰度（单通道）
float getAverageGray(cv::Mat _img)
{
    float average_grey = cv::sum(_img)[0] / _img.total();
    qDebug() << "average_grey = " << average_grey;

    //
    return average_grey;
}

// 计算标准差
float calcStdDev(std::vector<int> &_list, int _from, int _to)
{
    //
    int from = 0, to = _list.size() - 1;
    if (_from >= 0) {
        if (_from <= to) {
            from = _from;
        } else {
            return -1;
        }
    }
    if (_to >= 0) {
        if (_to <= to) {
            to = _to;
        } else {
            return -1;
        }
    }
    if (from > to) {
        return -1;
    }

    //
    float sum = 0;
    for (int i = from; i <= to; i++) {
        sum += _list[i];
    }
    float avg = sum / (to - from + 1);

    //
    sum = 0;
    for (int i = from; i <= to; i++) {
        sum += (_list[i] - avg) * (_list[i] - avg);
    }
    avg = sum / (to - from + 1);

    //
    return sqrt(avg);
}

void setCurrentDirToHome()
{
#ifdef Q_OS_LINUX
    QString home_path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);     // 获得主目录
    QDir::setCurrent(home_path);
#endif
}

bool saveMemToFile(const char *_data, int _len, QString _file_path)
{
    QFile file(_file_path);
    bool open_succ = file.open(QFile::WriteOnly | QFile::Truncate);
    if (open_succ) {
        file.write((char*)_data, _len);
        file.flush();
        file.close();

        return true;
    } else {
        std::cout << "CScreenerImgsData::slotFinishImgSaving(): open file failed!" << std::endl;
        // TODO:


        //
        return false;
    }
}

long getTvDiffUsec(timeval _tv_end, timeval _tv_begin)
{
    return _tv_end.tv_sec * 1000000 + _tv_end.tv_usec - _tv_begin.tv_sec * 1000000 - _tv_begin.tv_usec;
}

//
bool separateFilePath(const QString &_file_path, QString &_dir_path, QString &_file_name)
{
    return separateStr(_file_path, QDir::separator(), -1, _dir_path, _file_name);
}

//
bool separateFileName(const QString &_full_name, QString &_file_name, QString &_exten_name)
{
    return separateStr(_full_name, '.', -1, _file_name, _exten_name);
}

// 转义字符解析
void parseEscapeChar(QByteArray &_data)
{
    if (_data.length() < 2) {
        return;
    }

    char *c1, *c2, *c3;
    bool escaped;
    for (int i = _data.length() - 3; i >= -1; i--) {
        c1 = (i >= 0 ? _data.data() + i : 0);
        c2 = _data.data() + i + 1;
        c3 = _data.data() + i + 2;
        if ((!c1 || '\\' != c1[0]) && ('\\' == c2[0])) {
            escaped = true;
            switch (c3[0]) {
            case 'r': c3[0] = '\r'; break;
            case 'n': c3[0] = '\n'; break;
            case '0': c3[0] = '\0'; break;
            case '\\': break;
            default: escaped = false; break;
            }
            if (escaped) {
                _data.remove(i + 1, 1);
            }
        }
    }
}

///=============================================================================================
/// class CEventDelayFilter

CEventDelayFilter *CEventDelayFilter::instance = Q_NULLPTR;

CEventDelayFilter::CEventDelayFilter()
{

}

CEventDelayFilter::~CEventDelayFilter()
{

}

CEventDelayFilter *CEventDelayFilter::getInstance()
{
    if (!instance) {
        instance = new CEventDelayFilter();
    }
    return instance;
}

void CEventDelayFilter::releaseInstance()
{
    delete instance;
    instance = Q_NULLPTR;
}

// 注册延时过滤器
int CEventDelayFilter::registerDelayFilter(int _delay_ms)                      /* 注意：每次调用本函数都会申请内存，且在运行期间不会释放，不可重复调用！ */
{
    int id = map.size();
    stEventDelayFilterInfo *info = new stEventDelayFilterInfo();
    memset(info, 0, sizeof(stEventDelayFilterInfo));
    info->delayMs = _delay_ms;
    info->timer = new QElapsedTimer();
    map.insert(id, info);

    return id;
}

// 调用延时过滤器（过滤掉延时期间收到的事件），返回延时期间内收到的事件次数
int CEventDelayFilter::invokeDelayFilter(int _filter_id)
{
    if (!map.contains(_filter_id)) {
        // TODO: log ?
        return false;
    }

    stEventDelayFilterInfo *info = map[_filter_id];

    if (!info->timer->isValid() || info->timer->elapsed() > info->delayMs) {
        info->timer->start();
        info->countEvent = 1;
    } else {
        info->countEvent += 1;
    }

    return info->countEvent;
}

QString setComboBoxIndex(QComboBox &_cbb, int _idx)         // TODO: 移出 Util 模块，放到与 UI 相关的工具模块中（本模块是最底层的模块）
{
    QString err_str = "";
    if (_idx < -1 || _idx > _cbb.count() - 1) {
        err_str = QString("Index of ComboBox '") + _cbb.objectName() + "' is out of range!";
        _idx = -1;
    }
    _cbb.setCurrentIndex(_idx);

    return err_str;
}

///=============================================================================================
/// class CIntArray

CIntArray::CIntArray(int _count, ...)       // 注意：可变参数的个数不可错
{
    va_list arg_ptr;
    va_start(arg_ptr, _count);

    size = _count;
    data = (int *)malloc(sizeof(int) * (_count + 1));

    for (int i = 0; i < _count; i++) {
        data[i] = va_arg(arg_ptr, int);
    }

    va_end(arg_ptr);
}

CIntArray::~CIntArray()
{
    delete data;
    data = Q_NULLPTR;
}

bool CIntArray::contains(const int _v) const
{
    bool ret = false;
    for (int i = 0; i < size; i++) {
        if (_v == data[i]) {
            ret = true;
            break;
        }
    }
    return ret;
}

const int &CIntArray::operator[](int i) const
{
    return data[i];
}

int getDirSpace(const char* dir_path, ulong* dir_space)     // TODO: 和 RunningStatus::GetMemory() 统一？
{
    if (dir_path == nullptr)
    {
        *dir_space = 0;
        qDebug()<<"dir_path is nullptr";
        return -1;
    }

    struct statfs diskInfo;
    int result = statfs(dir_path, &diskInfo);
    if (result != 0)
    {
        return result;
    }

    unsigned long long blocksize = diskInfo.f_bsize;    //每个block里包含的字节数
//    unsigned long long totalsize = blocksize * diskInfo.f_blocks;     //总的字节数，f_blocks为block的数目
//    qDebug("Total_size = %llu B = %llu KB = %llu MB = %llu GB\n",
//        totalsize, totalsize>>10, totalsize>>20, totalsize>>30);
    //unsigned long long freeDisk = diskInfo.f_bfree * blocksize;    //剩余空间的大小
    unsigned long long availableDisk = diskInfo.f_bavail * blocksize;     //可用空间大小
    *dir_space = availableDisk>>20;
//    qDebug("Disk_free = %llu MB = %llu GB\nDisk_available = %llu MB = %llu GB\n",
//        freeDisk>>20, freeDisk>>30, availableDisk>>20, availableDisk>>30);
    qDebug()<<QString("dir_space:%1 MB").arg(*dir_space);
    return 0;
}

//
void fillStr(QString &_str, char _c, int _len, bool _is_left)
{
    int len = _len - _str.size();
    QString str(len, _c);
    _str.insert((_is_left ? 0 : _str.size()), str);
}

//
void fillStrExt(QString &_str, char _c, int _len, bool _is_left)
{
    int len = _len - _str.size();
    for (int i = _str.size() - 1; i >= 0; i--) {
        if (_str[i] > 0xFF) {
            len--;
        }
    }

    QString str(len, _c);
    _str.insert((_is_left ? 0 : _str.size()), str);
}

void rectAdaptParent(QRect &_rect_child, const QRect &_rect_parent, int _margin)
{
    double w_h_ratio_child = (double)_rect_child.width() / _rect_child.height();        // 子部件的宽高比
    double w_h_ratio_parent = (double)_rect_parent.width() / _rect_parent.height();     // 父部件的宽高比
    if (w_h_ratio_child > w_h_ratio_parent) {                           // 若子部件的宽高比大于父部件的宽高比，则以父部件的宽度设置子部件宽度，然后以子部件的宽高比计算子部件的高度
        int width = _rect_parent.width() - _margin * 2;
        int height = width / w_h_ratio_child;
        _rect_child.setRect(_margin, (_rect_parent.height() - height) / 2, width, height);
    } else {                                                            // 否则，反过来
        int height = _rect_parent.height() - _margin * 2;
        int width = height * w_h_ratio_child;
        _rect_child.setRect((_rect_parent.width() - width) / 2, _margin, width, height);
    }
}

QString doubleToQStr(double _d, int _precision, bool _is_signed)
{
    QString str = QString::number(_d, 'f', _precision);
    if (_is_signed && (_d > M_FLOAT_PRECISION)) {
        //str.prepend(QByteArrayLiteral("+"));
        str.prepend('+');
    }
    return str;
}

bool readFileToBytesArray(const QString &_file_path, QByteArray &_bytes)
{
    _bytes.clear();

    bool is_succ = false;
    QFile file(_file_path);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            _bytes = file.readAll();
            file.close();
            is_succ = true;
        }
    }

    return is_succ;
}

bool readFileToQStr(const QString &_file_path, QString &_str, const char *_codec_name)
{
    QByteArray bytes;
    bool is_succ = Util::readFileToBytesArray(_file_path, bytes);
    if (is_succ) {
        QTextStream stream(bytes);
        stream.setCodec(_codec_name);

        _str = stream.readAll();
    }
    return is_succ;
}

bool readFileToQStrList(const QString &_file_path, QStringList &_lines)
{
    _lines.clear();

    QString lines_str;
    bool is_succ = readFileToQStr(_file_path, lines_str);
    if (is_succ) {
        lines_str.replace("\r\n", "\n");
        _lines = lines_str.split("\n");
    }

    return is_succ;
}

bool writeQStrToFile(const QString &_str, const QString &_file_path, const char *_codec_name)
{
    // 确保目录存在
    QFileInfo file_info(_file_path);
    QString path_dir = file_info.absolutePath();
    if (!QFile::exists(path_dir)) {
        QDir dir(path_dir);
        bool succ_mkdir = dir.mkpath(path_dir);
        if (!succ_mkdir) {
            return false;
        }
    }

    //
    QFile file(_file_path);
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream stream(&file);
        stream.setCodec(_codec_name);
        stream << _str;
        stream.flush();
        file.flush();
        file.close();

        return true;
    } else {
        // TODO: log?

        return false;
    }
}

bool writeQStrListToFile(const QStringList &_str_list, const QString &_file_path, const char *_codec_name)
{
    QFile file(_file_path);
    if (file.open(QFile::WriteOnly | QFile::Truncate)) {
        QTextStream stream(&file);
        stream.setCodec(_codec_name);
        for (int i = 0; i < _str_list.size(); i++) {
            stream << _str_list.at(i) << '\n';
        }
        stream.flush();
        file.flush();
        file.close();

        return true;
    } else {
        // TODO: log?

        return false;
    }
}

void qstrToStringList(const QString &_text, QStringList &_str_list)
{
    _str_list.clear();

    //
    if (_text.length() == 0) {
        return;
    }

    //
    int pos_begin = 0, pos_end = -1;
    QString str_line;
    do {
        if (pos_begin > _text.length() - 1) {
            break;
        }

        pos_end = _text.indexOf('\n', pos_begin);
        if (pos_end >= 0) {
            str_line = _text.mid(pos_begin, pos_end - pos_begin + 1);
            pos_begin = pos_end + 1;
        } else {
            str_line = _text.mid(pos_begin);
            pos_begin = _text.length();
        }

        str_line.replace("\r\n", "");
        str_line.replace('\n', "");
        _str_list.append(str_line);
    } while (true);
}

void stringListToQStr(const QStringList &_str_list, QString &_text)
{
    _text.clear();

    for (int i = 0; i < _str_list.size(); i++) {
        _text.append(_str_list.at(i));
        _text.append('\n');
    }
}

void appendLine(QString &_text, const QString &_new_line)
{
    if (!_text.isEmpty() && !_text.endsWith('\n')) {
        _text.append('\n');
    }
    _text.append(_new_line);
}

double clamp(const double _value, const double _min, const double _max)
{
    double value = _value;
    if (compDouble(value, _min) < 0) {
        value = _min;
    } else if (compDouble(value, _max) > 0) {
        value = _max;
    }
    return value;
}

bool executeTarExtract(const QString &_src_file_path, const QString _work_dir_path)
{
    //
    QProcess proc_tar;
    proc_tar.setWorkingDirectory(_work_dir_path);      // 设置工作目录，即提取时以该目录为根目录

    QString cmd_tar = QString("tar -xvf %1").arg(_src_file_path);
    proc_tar.setReadChannel(QProcess::StandardOutput);
    proc_tar.start(cmd_tar);

    bool is_finished = false;
    QString msg;
    QByteArray msg_bytes;
    while (!is_finished) {
        while (proc_tar.canReadLine()) {
            msg_bytes = proc_tar.readLine();
            msg = msg_bytes;
            qDebug() << msg;
        }

        //
        is_finished = proc_tar.waitForFinished(300);
    }

    while (proc_tar.canReadLine()) {            // TODO: 前面的命令执行完后，还有输出没读完？
        msg_bytes = proc_tar.readLine();
        msg = msg_bytes;
        qDebug() << msg;
    }

    //
    bool is_tar_succ = ((QProcess::NormalExit == proc_tar.exitStatus()) && (EXIT_SUCCESS == proc_tar.exitCode()));

    //
    return is_tar_succ;
}

bool executeGzipDecompress(const QString &_src_file_path, bool _is_reserve_src, const QString &_tar_file_path)
{
    if (_is_reserve_src && _src_file_path.length() == 0) {
        qCritical() << __PRETTY_FUNCTION__ << ": params invalid: reserve source file is true, but no target path!";
        return false;
    }

    //
    QString cmd_gzip;
    if (!_is_reserve_src) {
        cmd_gzip = QString("gzip -dvf %1").arg(_src_file_path);
    } else {
        cmd_gzip = QString("gzip -dcvf %1 > %2").arg(_src_file_path).arg(_tar_file_path);
    }

    QProcess proc_gzip;
    proc_gzip.setReadChannel(QProcess::StandardOutput);
    proc_gzip.start("bash", QStringList() << "-c" << cmd_gzip);     /* 命令中的重定向操作须 bash 来执行 */

    bool is_finished = false;
    QString msg;
    QByteArray msg_bytes;
    while (!is_finished) {
        while (proc_gzip.canReadLine()) {
            msg_bytes = proc_gzip.readLine();
            msg = msg_bytes;
            qDebug() << msg;
        }

        //
        is_finished = proc_gzip.waitForFinished(300);
    }

    while (proc_gzip.canReadLine()) {            // TODO: 前面的命令执行完后，还有输出没读完？
        msg_bytes = proc_gzip.readLine();
        msg = msg_bytes;
        qDebug() << msg;
    }

    //
    bool is_gzip_succ = ((QProcess::NormalExit == proc_gzip.exitStatus()) && (EXIT_SUCCESS == proc_gzip.exitCode()));

    //
    return is_gzip_succ;
}

uint calcPrintWidthOfStr(const QString &_str)
{
    uint width = 0;

    for (const QChar &ch : _str) {
        if (ch.unicode() < 256) {   // ASCII 范围，英文字符
            width += 1;
        } else {                    // 中文字符及其他非 ASCII 字符
            width += 2;
        }
    }

    return width;
}

/* 引用 unicode/ucsdet.h 头文件后，需要加载 libicuuc.so 和 libicudata.so 这两个库文件 */
//#LIBS += -L/usr/lib/x86_64-linux-gnu -licuuc -licudata
//#include <unicode/ucsdet.h>
//bool isChineseTextGarbled(const QString& _str)
//{
//    /* 源自“零声教学AI助手” */
//
//    QByteArray bytes = _str.toUtf8();  // 转换为字节数组（使用UTF-8编码）
//
//    UErrorCode err_dode = U_ZERO_ERROR;
//    UCharsetDetector* detector = ucsdet_open(&err_dode);
//
//    if (U_SUCCESS(err_dode))
//    {
//        ucsdet_setText(detector, bytes.constData(), bytes.length(), &err_dode);
//
//        const UCharsetMatch* match = ucsdet_detect(detector, &err_dode);
//
//        if (match != nullptr)
//        {
//            const char* charsetName = ucsdet_getName(match, &err_dode);
//
//            // 检查字符集是否与期望的编码相符
//            if (charsetName != nullptr && strcmp(charsetName, "UTF-8") != 0)
//            {
//                qDebug() << "Detected encoding:" << charsetName;
//                return true;  // 中文可能是乱码
//            }
//        }
//    }
//
//    ucsdet_close(detector);
//
//    return false;  // 中文不是乱码
//}

///=============================================================================================
///


}

/// namespace Util end ================================

