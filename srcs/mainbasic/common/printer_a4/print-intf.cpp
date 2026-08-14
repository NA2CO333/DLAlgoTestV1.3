#include "print-intf.h"

#include "cups-intf.h"

namespace Common {

QString enumToText_PrinterStatus_Eng(enPrinterStatus _status)
{
    switch (_status) {
    case enPrinterStatus::Unknown               : return "Unknown"              ;
    case enPrinterStatus::Idle                  : return "Idle"                 ;
    case enPrinterStatus::Processing            : return "Processing"           ;
    case enPrinterStatus::Spooling              : return "Spooling"             ;
    case enPrinterStatus::Printing              : return "Printing"             ;
    case enPrinterStatus::Paused                : return "Paused"               ;
    case enPrinterStatus::Stopped               : return "Stopped"              ;
    case enPrinterStatus::Offline               : return "Offline"              ;
    case enPrinterStatus::NoResponding          : return "NoResponding"         ;
    case enPrinterStatus::Unreachable           : return "Unreachable"          ;
    case enPrinterStatus::Error                 : return "Error"                ;
    case enPrinterStatus::Held                  : return "Held"                 ;
    case enPrinterStatus::WaitingForAuth        : return "WaitingForAuth"       ;
    case enPrinterStatus::WaitingForavilable    : return "WaitingForavilable"   ;
    }
    return "???";
}

QString enumToText_PrinterStatus_Chn(enPrinterStatus _status)
{
    switch (_status) {
    case enPrinterStatus::Unknown               : return "未知状态"             ;
    case enPrinterStatus::Idle                  : return "空闲"                 ;
    case enPrinterStatus::Processing            : return "正在处理任务"         ;
    case enPrinterStatus::Spooling              : return "任务正在排队或发送"   ;
    case enPrinterStatus::Printing              : return "正在打印"             ;
    case enPrinterStatus::Paused                : return "已暂停"               ;
    case enPrinterStatus::Stopped               : return "已停止"               ;
    case enPrinterStatus::Offline               : return "已离线"               ;
    case enPrinterStatus::NoResponding          : return "无应答"               ;
    case enPrinterStatus::Unreachable           : return "无法访问"             ;
    case enPrinterStatus::Error                 : return "错误"                 ;
    case enPrinterStatus::Held                  : return "任务被挂起"           ;
    case enPrinterStatus::WaitingForAuth        : return "等待用户认证"         ;
    case enPrinterStatus::WaitingForavilable    : return "等待可用"             ;
    }
    return "???";
}

//
CPrintIntf *CPrintIntf::s_instance = Q_NULLPTR;

//
CPrintIntf *CPrintIntf::instance()
{
    if (!CPrintIntf::s_instance) {
        CPrintIntf::s_instance = new CCupsIntf;
    }
    return CPrintIntf::s_instance;
}

CPrintIntf::CPrintIntf(QObject *parent) : QObject(parent)
{
    QObject::connect(this, &CPrintIntf::sigBeginSearch, this, &CPrintIntf::slotBeginSearch, Qt::QueuedConnection);

}

CPrintIntf::~CPrintIntf()
{

}

void CPrintIntf::setIsShowNotSupported(bool _is_show_not_supported)
{
    m_isShowNotSupported = _is_show_not_supported;
}

void CPrintIntf::beginSearch()
{
    emit sigBeginSearch();
}

const QList<Common::stPrinterInfo> &CPrintIntf::getPrinterList(bool _is_rescan)
{
    //
    if (_is_rescan) {
        //
        searchPrinters(m_listPrinters);
    }

    //
    return m_listPrinters;
}

void CPrintIntf::slotBeginSearch()
{
    searchPrinters(m_listPrinters);
    emit sigSearchFinished();
}

int CPrintIntf::findPrinterFromList(const QList<stPrinterInfo> &_list_info, const QString &_uri)
{
    int idx = -1;
    for (int i = 0; i < _list_info.size(); i++) {
        if (_uri == _list_info.at(i).uri) {
            idx = i;
            break;
        }
    }
    return idx;
}

}   // namespace Common
