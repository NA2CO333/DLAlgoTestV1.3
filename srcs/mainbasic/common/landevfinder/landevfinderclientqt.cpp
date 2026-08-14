#include "landevfinderclientqt.h"

#include "nettools.h"

//
CLanDevFinderClientQt *CLanDevFinderClientQt::thisQtObj = Q_NULLPTR;

//
CLanDevFinderClientQt::CLanDevFinderClientQt(QObject *parent) : QObject(parent)
{
    //
    thisQtObj = this;

    // 设备查找客户端的初始化
    QString broadcast_addr = Common::Net::getBroadcastAddr();

    devFinderClient = CLanDevFinderClient::getInstance();
    devFinderClient->setBroadcastAddr(broadcast_addr.toStdString());
    devFinderClient->setFindFinishedCallback(&CLanDevFinderClientQt::callbackFinderFinished);

}

CLanDevFinderClientQt::~CLanDevFinderClientQt()
{

}

void CLanDevFinderClientQt::startFind(bool _is_block)
{
    devFinderClient->startFind(_is_block);
}

bool CLanDevFinderClientQt::getFindResult(QString &_addr, int &_port, unsigned int _index)
{
    std::string addr;
    bool is_succ = devFinderClient->getFindResult(addr, _port, _index);
    if (is_succ) {
        _addr = QString::fromStdString(addr);
    }
    return is_succ;
}

void CLanDevFinderClientQt::setMessageKeyword(QString _keyword)
{
    devFinderClient->setMessageKeyword(_keyword.toStdString());
}

int CLanDevFinderClientQt::getResultCount()
{
    return devFinderClient->getResultCount();
}

void CLanDevFinderClientQt::callbackFinderFinished()
{
    thisQtObj->emitFinderFinished();
}

void CLanDevFinderClientQt::emitFinderFinished()
{
    emit sigFinderFinished();
}

