#ifndef CCUPSINTF_H
#define CCUPSINTF_H

#include <QProcess>
#include <QMap>
#include <QStringList>

#include "print-intf.h"

// 版本日期（最后修改日期）
#define CUPS_INTF_VER_DATE  "20230719"

namespace Common {

// 打印机 CUPS 信息
struct stPrinterCupsInfo {
    QString uri;
    QString name;
    QString protocol;
    QString make;       // 品牌
    QString model;      // 型号
    QString ip;         // IP
    QString ppd;        // Postscript Printer Definition

    void clear(QString _default = "") {
        uri         = _default;
        name        = _default;
        protocol    = _default;
        make        = _default;
        model       = _default;
        ip          = _default;
        ppd         = _default;
    }

    stPrinterCupsInfo() {
        clear();
    }
};

// CUPS 接口
/* 用法要点：
 * 1、未内建线程，即默认工作在主线程，若调用方需其工作在非UI线程，应调用 CCupsIntf::moveToThread(xxx)。
 */
class CCupsIntf : public CPrintIntf
{
    Q_OBJECT
public:
    friend class CPrintIntf;
    ~CCupsIntf();

    // 设置默认打印机
    bool setDefaultPrinter(QString _uri, QString _name = "", QString _ppd = "") override;

    // 设置纸张尺寸
    bool setPaperSize(enPageSize _paper_size) override;

    /**
     * @brief 打印文件
     * @param _file_path    : 文件的路径（绝对路径，完整的）
     */
    void printFile(QString _file_path, bool _is_async = true) override;

    // 得到当前任务数
    int getJobsCount() override;

    // 取消所有打印
    void cancelAllJobs() override;

    // 获取“当前打印机是否可用”状态
    bool getIsPrinterReady() override;

    // 获取“当前打印机”状态
    enPrinterStatus getPrinterStatus() override;

Q_SIGNALS:
    void sigPrintFileFailed(QString _file_path);        // 打印文件失败信号

    // （私有）打印文件
    void sigPrintFile(QString _file_path);

private slots:
    void slotPrintFile(QString _file_path);

protected:
    explicit CCupsIntf(QObject *parent = nullptr);

    const QStringList &getSupportedMakes();                             // 获取已支持的品牌
    const QStringList &getSupportedModels(const QString &_make);        // 获取已支持的型号

    void searchPrinters(QList<stPrinterInfo> &_list_info) override;     // 查找可用打印机，并将查找结果存入指定容器

    void doPrintFile(QString _file_path);

    // 获得所有打印机（lpstat -v）

    // 获得可用打印机（lpstat -a）

    // 删除打印机（lpadmin -x printer_name）

    QMap<QString, stPrinterCupsInfo> m_listPrintersCpusInfo;            // 搜索到的打印机列表，相对于 CPrintIntf::listPrinters 多了一些内部使用的属性
    QString m_defaultPrinterName;                                       // 默认打印机名，且用于判断当前打印机是否已准备好

};

}   // namespace Common

#endif // CCUPSINTF_H
