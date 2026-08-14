#ifndef UTIL_H
#define UTIL_H

#include <execinfo.h>

#include <QVector>
#include <QThread>
#include <QRect>
#include <QElapsedTimer>
#include <QMap>
#include <QComboBox>
#include <QDebug>

// TODO: 将 OpenCV 库从普通业务模块剥离？
#include "opencv2/core/core.hpp"
#include "opencv2/core/core_c.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgproc/imgproc_c.h"

#include "globaltypes.h"

//
namespace Util {

#define M_FLOAT_PRECISION 0.00001

//void initialize();
//void uninitialize();

enum enImgFileType {
    imgFileType_ImgData,
    imgFileType_Bmp,
};

// 视筛仪图像数据
class CScreenerImgsData : public QObject        // TODO: 移到 UtilApp
{
    Q_OBJECT
public:
    explicit CScreenerImgsData(QObject *parent=0);
    ~CScreenerImgsData();

    QString imgsDir;

    bool useSimulateImage = false;          // 使用模拟图像数据，用于恒温箱测试
    bool isSaveSimulateImage = false;       // 保存恒温测试所需的模拟数据
    bool isSimulateImgsInitted = false;     // 是否已初始化存放虚拟抓图图像数据的内存区

    unsigned char* m_pPreImgData = NULL;

    bool isSaveToFile = false;
    bool isLoadPreImg = true;
    enImgFileType fileType = imgFileType_ImgData;

    bool isNeedZeroth = true;       // 是否需要第 0 张图

    void setImgSize(int _img_width, int _img_height);
    bool loadImgFiles(bool _force = false);
    void saveSimulateImage(unsigned char* _data, int _idx);
    unsigned char* getImage(int _idx);
    void saveSimulateImgToFile(unsigned char* _data, QString _name);
    void finishImgSaving();
    int getImgDataLen();
    void clearFileNames();
    void addFileName(int _idx, QString &_file_name);

    static int getIndexFromFileName(QString _file_name);

signals:
    void sigFinishImgSaving();
    void sigSaveSimulateImgToFile();

private slots:
    void slotSaveSimulateImgToFile();
    void slotFinishImgSaving();

private:
    struct stImgData {
        unsigned char* data;
        QString name;
    };
    struct stFileNameInfo {
        int index;
        QString name;
    };

    static constexpr char C_FILE_EXT[] = ".jpg";

    int imgWidth = -1;
    int imgHeight = -1;

    QVector<unsigned char*> mImgDataVec;
    QVector<stImgData> mImgFilesBuff;
    QVector<stImgData> mImgToFileBuff;
    //int curr_pre = 0;
    QThread mWorkThread;
    QVector<stFileNameInfo> mFileNames;

    void saveSimulateImageToFile(unsigned char* _data, QString _name);
    bool readSimulateImageFromFile(unsigned char* _data, int _idx);
    bool getFileNameByIndex(int _idx, QString &_file_name);

};

/// ================================================================================================
/// use or save simulate image

//
Util::CScreenerImgsData *getSimulateCaptureInst();

void setSaveSimulateImg(bool _is_save, int _img_width, int _img_height);
bool setUseSimulateImg(bool _is_use, int _img_width, int _img_height);
bool getSaveSimulateImg();
bool getUseSimulateImg();

void saveSimulateImg(uchar *_img_data, int _turnlamp_buff_size);
void finishSimulateImgSaving();
void useSimulateImg(uchar *_img_data, int _turnlamp_buff_size);

/// ================================================================================================
///

// U盘操作封装
class CUDisk : public QObject
{
    Q_OBJECT

public:
    explicit CUDisk(QObject *parent=0);
    ~CUDisk();

    static void remount();      // TODO: 临时解决方法（i.Mx6q 平台不能识别中文目录的问题），使系统支持U盘的中文文件名
    static QString getPath();
    static bool sync();
    static bool umount(bool _del_path = true);
    static bool isUmounted();

protected:
    static QString s_path;

};

///=================================================================================================
/// class CCircularQueue（声明）

// 队列元素信息
template <typename T> struct stQueueElementInfo {
    unsigned int    idx;        // 索引号
    T               *data;      // 数据数组地址
    time_t          time;       // 时间
};

// 环形缓冲队列（Circular Buffer Queue）
template <typename T> class CCircularQueue
{
public:
    CCircularQueue(int _queue_len, int _element_len);
    ~CCircularQueue();

    void zeroAll();
    T *nextOne();
    stQueueElementInfo<T> &nextOneInfo();
    bool getOneAt(unsigned int _sn, T *&_data);

protected:
    int queueLen;
    int elementLen;

    QVector<stQueueElementInfo<T>> queue;
    int currentIndex;      // 当前元素索引号

};

///=================================================================================================
/// template CCircularQueue（实现）

//
// @_queue_len: 队列元素个数
// @_element_len: 每个元素中指定数据类型的个数
template <typename T> CCircularQueue<T>::CCircularQueue(int _queue_len, int _element_len) :
    queueLen(_queue_len),
    elementLen(_element_len)
{
    // 队列内存申请
    for (int i = 0; i < queueLen; i++) {
        stQueueElementInfo<T> element_info;
        memset(&element_info, 0, sizeof(stQueueElementInfo<T>));

        element_info.idx = i;
        element_info.data = (T *)malloc(elementLen * sizeof(T));

        queue.append(element_info);
    }

    //
    zeroAll();

}

template <typename T> CCircularQueue<T>::~CCircularQueue()
{
    // 队列内存释放
    for (stQueueElementInfo<T> element_info : queue) {
        free(element_info.data);
        element_info.data = nullptr;
    }
    queue.clear();

}

// 队列置零
template <typename T> void CCircularQueue<T>::zeroAll()
{
    // 擦除所有缓存帧图像数据
    for (stQueueElementInfo<T> element_info : queue) {
        memset(element_info.data, 0, elementLen);
    }

    //
    currentIndex = 0;
}

// 队列指针后移一位，并返回后移之后的地址
template <typename T> T *CCircularQueue<T>::nextOne()
{
    stQueueElementInfo<T> &element_info = nextOneInfo();
    return element_info.data;
}

template <typename T> stQueueElementInfo<T> &CCircularQueue<T>::nextOneInfo()
{
    currentIndex++;
    if (!(currentIndex < queueLen)) {
        currentIndex = 0;
    }

    stQueueElementInfo<T> &element_info = queue[currentIndex];

    element_info.idx = currentIndex;
    //element_info.time = time(NULL);

    return element_info;
}

template <typename T> bool CCircularQueue<T>::getOneAt(unsigned int _sn, T *&_data)
{
    if (_sn >= 0 && _sn < queueLen) {
        _data = queue.at(_sn).data;
        return true;
    } else {
        return false;
    }
}

/// ====================================================================================================================
/// 其它函数

// 由 system() 的返回值判断是否执行成功
bool isSystemCmdSucc(int _status);

// 比较 double 类型的值，若前者小于后者则返回 -1，若相等则返回 0，若前者大于后者则返回 1
int compDouble(const double _d1, const double _d2);

// 比较两个值（值的类型需支持大于等于小于运算符，且没有类似浮点类型的精度问题）。返回：若前者小于后者则返回 -1，若相等则返回 0，若前者大于后者则返回 1
template <typename T>
int compValue(const T _v1, const T _v2)
{
    return (_v1 > _v2 ? 1 : (_v1 == _v2 ? 0 : -1));
}

//
bool qstr2bool(QString _str);
//
int strToInt(const QString &_str, int _default = 0);
//
int variantToInt(const QVariant &_variant, int _default = -1);

/**
 * @brief 解析日期字符串。支持的格式："yyyy-MM-dd"，"yyyyMMdd"，"yyyy/MM/dd"，设计目的是兼容 U 盘导入文件中的多种日期格式，UI 输入的兼容也可以使用。
 * @param _str
 * @return QDate 类型，调用者可通过 date.isValid() 判断是否成功。
 */
QDate strToDate(const QString &_str);

// 改变日期字符串的格式
QString formatDateStr(QString _date_str, QString _format = "yyyy-MM-dd");

/**
 * @brief 由年、月、日字符串得到对应的日期类型对象
 * @param _year_str     年
 * @param _month_str    月
 * @param _day_str      日
 * @param _date         【输出参数】日期
 * @return  是否成功
 */
bool strsToDate(const QString &_year_str, const QString &_month_str, const QString &_day_str, QDate &_date);

//
const char * bool2str(bool _bool);

/**
 * @brief 浮点数转字符串
 * @param _d
 * @param _precision    精度，即保留的小数位数
 * @param _is_signed    是否包含符号（值为 0 时不加符号）
 * @return
 */
QString doubleToQStr(double _d, int _precision = 2, bool _is_signed = true);

//
QString boolToYesNo(bool _bool);

//
bool isStrEmpty(const char *_str);
//
QRect cvrect2qrect(CvRect _cv_rect);

// 获得随机整数
uint randomInt();

//
void IplImageToQImage(IplImage &_ipl_img, QImage &_q_img);

// 执行 shell 指令，返回标准输出， 及是否成功
bool executeLinuxCmd(const QString &_cmd, QString *_std_out = nullptr);

// 从字符串中指定位置开始读取一行内容
int readLine(QByteArray & _str, QByteArray & _line_str, int _from = 0);
//
int readLine(QString & _str, QString & _line_str, int _from = 0);

// 计算字符串的等宽字体打印长度（单位为半角字符的宽度，即全角字符宽度计为2）
int calcMonospacePrintingLength(const QString &_str);

/**
 * @brief 使当前线程等待，直到指定的条件变量的值与指定值相等或超时为止
 * @param _cond             条件变量
 * @param _val              满足条件的值
 * @param _time_out_usec    超时时间（微妙）
 * @return                  条件变量是否已等于指定的值
 */
bool sleepUntil(bool *_cond, bool _val, long _time_out_usec);
//
bool waitMs(int _ms, int *_cond = NULL, int _val = 1);
//
unsigned int getCrc32(unsigned char *ptr, int len);
//
int splitStr(const QString &_str, const QString &_sep, QString &_sub_str1, QString &_sub_str2, int _from = 0);
//
bool getSeparatedStr(QString _str, QString _sepa1, QString _sepa2, QString &_sub_str, int _from = 0);
// 据分隔符及位置分隔字符串为两段。@param _n : -1 表示最后一个，大于等于 0 表示从头往尾搜索找到分隔符的次数
bool separateStr(const QString &_str, QChar _separator, int _n, QString &_sub_str1, QString &_sub_str2);

// 分离由多个字段的值拼合而成的字符串（如 CSV 格式），支持双引号包起含有空格的字段
void splitStrToFields(const QString &_str, QStringList &_list_str, const QChar &_sep = ' ');

//
void sortStringList(QStringList &_str_list);

// 改变浮点数的精度
double roundDouble(double _double, double _precision);
// 改变整数的精度
int roundInt(int _int, int _precision = 1, bool _is_ceil = false);

// 返回将指定数值限制在指定区间内之后的值
double clamp(const double _value, const double _min, const double _max);

// 计算字符串的宽度，单位为字符个数（每个中文字符算2个，每个英文字符算1个）
int calcTextCharWidth(QString _str);
//
double max3(double _v1, double _v2, double _v3);
//
bool saveImgDataToImgFile(uchar *_img_data, int _img_width, int _img_height, QString _file_path, int _channels = 1);
bool saveImgDataToImgFile2(uchar *_img_data, int _img_width, int _img_height, QString _file_path, int _channels = 1);
//
QString getDirOfPath(QString _file_path);
//
bool makePath(QString _dir_path);
//
QString selectDir(QWidget *_parent, QString _default_dir = "");
QStringList selectFile(QWidget *_parent, QString _filter = "AllFiles (*.*)", bool _is_multi = false, QString _default_dir = "");
//
float getAverageGrayRough(cv::Mat _img);
//
float getAverageGrayRough(QString _file_path);
//
float getAverageGray(cv::Mat _img);
//
float calcStdDev(std::vector<int> &_list, int _from = -1, int _to = -1);

//
void setCurrentDirToHome();
//
bool saveMemToFile(const char *_data, int _len, QString _file_path);

//
long getTvDiffUsec(timeval _tv_end, timeval _tv_begin);

// 将文件路径拆分为路径（末尾不含路径分隔符）和文件名
bool separateFilePath(const QString &_file_path, QString &_dir_path, QString &_file_name);

// 将完整文件名（不含路径）拆分为文件名和扩展名
bool separateFileName(const QString &_full_name, QString &_file_name, QString &_exten_name);

// 转义字符解析
void parseEscapeChar(QByteArray &_data);

/**
 * @brief 获取指定路径的剩余空间
 * @param dir_path：路径
 * @param dir_space：剩余空间，单位MB
 */
int getDirSpace(const char* dir_path, ulong* dir_space);

// 用指定的字符在指定的位置（左或右）填充字符串到指定长度（以 QChar 为单位，即每个中文或英文的字符的长度都视为 1）     // TODO: 用 QString 的 leftJustified() 或 rightJustified() 即可？
void fillStr(QString &_str, char _c, int _len, bool _is_left = true);
// 用指定的字符在指定的位置（左或右）填充字符串到指定长度（每个中文长度视为 2，每个英文的字符的长度视为 1）
void fillStrExt(QString &_str, char _c, int _len, bool _is_left = true);

// 使子 rect 适应父 rect，即在父 rect 内达到最大，且维持宽高比不变
void rectAdaptParent(QRect &_rect_child, const QRect &_rect_parent, int _margin = 0);

// 从文件中读出数据到 QByteArray
bool readFileToBytesArray(const QString &_file_path, QByteArray &_bytes);
// 从文件中读出数据到 QString
bool readFileToQStr(const QString &_file_path, QString &_str, const char *_codec_name = "UTF-8");
// 从文件中读出数据到 QStringList
bool readFileToQStrList(const QString &_file_path, QStringList &_lines);

// 将 QString 写入到文件
bool writeQStrToFile(const QString &_str, const QString &_file_path, const char *_codec_name = "UTF-8");
// 将 QStringList 写入到文件
bool writeQStrListToFile(const QStringList &_str_list, const QString &_file_path, const char *_codec_name = "UTF-8");

// QString 转 QStringList，以换行符为分割标识，且把换行符去掉
void qstrToStringList(const QString &_text, QStringList &_str_list);
// QStringList 转 QString，以 '\n' 分隔
void stringListToQStr(const QStringList &_str_list, QString &_text);

// 在文本末端添加一行（自动判断是否需要插入换行符，若需要则插入）
void appendLine(QString &_text, const QString &_new_line);

/**
 * @brief executeTarCreate
 * @return
 */
//bool executeTarCreate();

/**
 * @brief 用 tar 命令提取归档文件
 * @param _src_file_path    源文件路径
 * @param _work_dir_path    工作目录路径
 * @return  是否成功
 */
bool executeTarExtract(const QString &_src_file_path, const QString _work_dir_path);

/**
 * @brief executeGzipCompress
 * @return
 */
//bool executeGzipCompress();

/**
 * @brief 用 gz 命令解压文件
 * @param _src_file_path    源文件路径
 * @param _tar_file_path    目标文件路径（若需要保留源文件，须指定目标文件路径）
 * @param _is_reserve_src   是否保留源文件
 * @return  是否成功
 */
bool executeGzipDecompress(const QString &_src_file_path, bool _is_reserve_src = false, const QString &_tar_file_path = "");

/**
 * @brief 检测 QString 里是否包含乱码
 * @param _str
 * @return
 */
//bool isChineseTextGarbled(const QString& _str);

uint calcPrintWidthOfStr(const QString &_str);      // 根据“英文字符占宽为1，中文字符占宽为2”的规则计算字符串的占位宽度

/// ====================================================================================================================
/// class CEventDelayFilter

// 延时过滤器的信息数据
struct stEventDelayFilterInfo {
    int delayMs;                    // 抖动过滤时间
    QElapsedTimer *timer;           // 上次触发后的计时
    int countEvent = 0;             // 上次触发后收到的事件计数
};

// 事件延时过滤器
class CEventDelayFilter
{
public:
    static CEventDelayFilter *getInstance();
    static void releaseInstance();

    int registerDelayFilter(int _delay_ms);         // 注册
    int invokeDelayFilter(int _filter_id);          // 调用

protected:
    CEventDelayFilter();
    ~CEventDelayFilter();

    static CEventDelayFilter *instance;
    QMap<int, stEventDelayFilterInfo *> map;
};

/// ====================================================================================================================
/// class CIntArray

//
class CIntArray
{
public:
    CIntArray(int _count, ...);
    ~CIntArray();

    bool contains(const int _v) const;

    const int &operator[](int i) const;

protected:
    int size = 0;
    int *data = Q_NULLPTR;
};

/// ====================================================================================================================
///

// 设置 ComboBox 当前项，检查修正索引号有效性，防止异常
QString setComboBoxIndex(QComboBox &_cbb, int _idx);

}   // namespace Util end

// =====================

//
template<typename T> void output_trace(const int _size = 100)
{
    int j, nptrs;

    void *buffer[_size];
    char **strings;
    nptrs = backtrace(buffer, _size);

    printf("backtrace() returned %d addresses\n", nptrs);

    /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
     * would produce similar output to the following: */

    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    for (j = 0; j < nptrs; j++)
    {
        //printf("%s\n", strings[j]);
        qDebug() << QString::asprintf("%s", strings[j]).toStdString().c_str();
    }

    free(strings);
    strings = nullptr;
}


#endif // UTIL_H
