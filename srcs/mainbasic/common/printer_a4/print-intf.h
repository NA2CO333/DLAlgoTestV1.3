#ifndef CPRINTINTF_H
#define CPRINTINTF_H

#include <QObject>

//#include <QPagedPaintDevice>
#include <QPageSize>

/* 用法：
 * 1、创建默认单例：CPrintIntf *printIntf = CPrintIntf::instance() ；
 * 2、设置工作线程（若不用信号槽，可不用）：printIntf->moveToThread(...); 或 CPrintIntf::instance()->moveToThread(...) ；
 * 3、获取打印机列表：见 beginSearch() 或 getPrinterList() 函数；
 * 4、选择当前打印机： setDefaultPrinter() ；
 * 5、传入文件路径，即可调用 printFile(...) 打印；
 * 6、调用 getJobsCount() 可得到正在执行的打印任务数，可由此判断打印是否完成；
 *
 * 注：本模块未检测网络是否已连接，需要调用者在调用前先检查。
 */

namespace Common {

// 纸张尺寸定义（因 Qt 5 的 PageSize 定义有重复: QPagedPaintDevice::PageSize, QPageSize::PageSizeId）
enum enPageSize {
    //pageSize_A4 = QPagedPaintDevice::A4,
    //pageSize_A5 = QPagedPaintDevice::A5,

    pageSize_A4 = QPageSize::A4,
    pageSize_A5 = QPageSize::A5,

};

// 打印机信息
struct stPrinterInfo {
    QString uri;            // 打印机 URI，是打印机的唯一标识
    QString name;           // 打印机名称，显示给用户看的
    bool isSupported;       // 本机的驱动是否支持

    //
    void clear(QString _default = "") {
        uri         = _default;
        name        = _default;
        isSupported = false;
    }

    //
    stPrinterInfo() {
        clear();
    }
};

// 打印机状态信息
enum class enPrinterStatus {
    Unknown             ,       // 未知状态
    Idle                ,       // 空闲
    Processing          ,       // 正在处理任务
    Spooling            ,       // 任务正在发送或排队
    Printing            ,       // 正在打印
    Paused              ,       // 已暂停
    Stopped             ,       // 已停止
    Offline             ,       // 已离线
    NoResponding        ,       // 无应答
    Unreachable         ,       // 无法访问
    Error               ,       // 错误
    Held                ,       // 任务被挂起
    WaitingForAuth      ,       // 等待用户认证
    WaitingForavilable  ,       // 等待可用
};
QString enumToText_PrinterStatus_Eng(enum enPrinterStatus _status);  // 枚举转文本_打印机状态_英文
QString enumToText_PrinterStatus_Chn(enum enPrinterStatus _status);  // 枚举转文本_打印机状态_中文

// 打印接口
class CPrintIntf : public QObject
{
    Q_OBJECT
public:
    // 获得单例
    static CPrintIntf *instance();       // 默认的实现类的实例（若想要非默认的实现类的实例，需调用具体的实现类的构造方法，否则只需引用本接口的头文件即可）
    ~CPrintIntf();

    // 设置是否显示本机的打印驱动未支持的打印机
    void setIsShowNotSupported(bool _is_show_not_supported);

    // 开始搜索打印机（该函数只是发射了信号，并立即返回，调用者在收到 sigSearchFinished() 信号后即可调用 getPrinterList() 获得最新的搜索结果）
    void beginSearch();

    /**
     * @brief 获得打印机 URI 列表
     * @param _is_rescan    : 是否重新扫描（若否，则得到的是上次真正扫描时所得的历史记录。若是重新扫描，则在当前线程执行，且搜索期间阻塞。若希望异步搜索，可使用 beginSearch()。）
     * @return 打印机 UIR 列表
     */
    const QList<stPrinterInfo> &getPrinterList(bool _is_rescan = false);

    // 设置纸张尺寸
    virtual bool setPaperSize(enPageSize _paper_size) = 0;

    /**
     * @brief 设置默认打印机。也用于重启后设置上次连接过的打印机，设置后自动连接。
     * @param _printer_uri
     * @param _name         打印机名，若为空，则自动解析 URI 得到名称
     * @return
     */
    virtual bool setDefaultPrinter(QString _uri, QString _name = "", QString _ppd = "") = 0;

    /**
     * @brief 打印文件
     * @param _file_path    : 文件的路径（绝对路径，完整的）
     */
    virtual void printFile(QString _file_path, bool _is_async = true) = 0;

    // 得到当前任务数
    virtual int getJobsCount() = 0;

    // 取消所有任务
    virtual void cancelAllJobs() = 0;

    // 获取“当前打印机是否可用”状态
    virtual bool getIsPrinterReady() = 0;

    // 获取“当前打印机”状态
    virtual enPrinterStatus getPrinterStatus() = 0;

Q_SIGNALS:
    // 打印机搜索完成事件（信号接收者调用 getPrinteriList() 可得到最新搜索到的打印机列表）
    void sigSearchFinished();

    // （私有）开始搜索信号
    void sigBeginSearch();

private slots:
    // "开始搜索"槽函数
    void slotBeginSearch();

protected:
    explicit CPrintIntf(QObject *parent = nullptr);
    static CPrintIntf *s_instance;

    virtual void searchPrinters(QList<stPrinterInfo> &_list_info) = 0;      // 查找可用打印机，并将查找结果存入指定容器

    // 从指定的打印机信息列表中查找指定 URI 的打印机
    int findPrinterFromList(const QList<stPrinterInfo> &_list_info, const QString &_uri);

    bool m_isShowNotSupported = false;                      // 是否显示未支持的打印机

private:
    QList<Common::stPrinterInfo> m_listPrinters;            // 已搜索到的打印机列表      // TODO: 线程安全？

};

// 打印接口测试
//class CPrintIntfTest : public QObject
//{
//    Q_OBJECT
//public:
//    explicit CPrintIntfTest(QObject *parent = nullptr);
//
//};

}   // namespace Common

#endif // CPRINTINTF_H
