#include "includes.h"
#include "u3GModule.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <stdlib.h>
#include <QMessageBox>
#include <unistd.h>

#include <iostream>
using namespace std;
Q3GModule::Q3GModule(QWidget *parent) :
    QWidget(parent)
{
    m_pTimer = new QTimer;
    m_pDialTimer = new QTimer;

    m_pFile = new QFile;
    m_pFile->setFileName("/tmp/3g.txt");
    if (!m_pFile->open(QIODevice::ReadWrite | QIODevice::Text))
    {
    }
    QLabel *laType = new QLabel("类型");
    m_cbType = new QComboBox();
    m_cbType->addItems(configure.wireless.getType());
    QLabel *laOperator = new QLabel("运营商");
    m_Mobile = new QRadioButton("移动");
    m_Mobile->setChecked(true);
    m_Unicom = new QRadioButton("联通");
    m_Telecom = new QRadioButton("电信");
    m_Mobile->setStyleSheet(" QRadioButton::indicator {width: 20px;height: 20px;}"
                            "QRadioButton::indicator:checked {image: url(\"/opt/PDA/app/images/checkon.png\");}"
                            "QRadioButton::indicator:unchecked {image: url(\"/opt/PDA/app/images/checkoff.png\");}");
    m_Unicom->setStyleSheet(" QRadioButton::indicator {width: 20px;height: 20px;}"
                            "QRadioButton::indicator:checked {image: url(\"/opt/PDA/app/images/checkon.png\");}"
                            "QRadioButton::indicator:unchecked {image: url(\"/opt/PDA/app/images/checkoff.png\");}");
    m_Telecom->setStyleSheet(" QRadioButton::indicator {width: 20px;height: 20px;}"
                             "QRadioButton::indicator:checked {image: url(\"/opt/PDA/app/images/checkon.png\");}"
                             "QRadioButton::indicator:unchecked {image: url(\"/opt/PDA/app/images/checkoff.png\");}");
    m_State = new QLabel();

    m_pMsg = new QTextEdit();
    m_pMsg->setReadOnly(true);

    m_pConfirm = new QPushButton(trUtf8("拨号"));
    m_pConfirm->setMaximumWidth(100);

    m_pCancel = new QPushButton(trUtf8("断开"));
    m_pCancel->setMaximumWidth(100);

    QLabel *ip = new QLabel("IP");
    QLabel *remote = new QLabel("Remote");
    QLabel *dns1 = new QLabel("DNS");
    QLabel *dns2 = new QLabel("DNS");

    m_ip = new QLineEdit();
    m_remote = new QLineEdit();
    m_dns1 = new QLineEdit();
    m_dns2 = new QLineEdit();

    m_ip->setEnabled(false);
    m_remote->setEnabled(false);
    m_dns1->setEnabled(false);
    m_dns2->setEnabled(false);

    QHBoxLayout *ipHLayout = new QHBoxLayout();
    ipHLayout->addWidget(ip);
    ipHLayout->addWidget(m_ip);
    QHBoxLayout *remoteHLayout = new QHBoxLayout();
    remoteHLayout->addWidget(remote);
    remoteHLayout->addWidget(m_remote);
    QHBoxLayout *dns1HLayout = new QHBoxLayout();
    dns1HLayout->addWidget(dns1);
    dns1HLayout->addWidget(m_dns1);
    QHBoxLayout *dns2HLayout = new QHBoxLayout();
    dns2HLayout->addWidget(dns2);
    dns2HLayout->addWidget(m_dns2);

    QVBoxLayout *ipVLayout = new QVBoxLayout();
    ipVLayout->addLayout(ipHLayout);
    ipVLayout->addLayout(remoteHLayout);
    ipVLayout->addLayout(dns1HLayout);
    ipVLayout->addLayout(dns2HLayout);

    QHBoxLayout *middleHLayout = new QHBoxLayout();
    middleHLayout->addWidget(m_pMsg);
    middleHLayout->addLayout(ipVLayout);

    QHBoxLayout *typeHLayout = new QHBoxLayout();
    typeHLayout->addWidget(laType);
    typeHLayout->addWidget(m_cbType);
    typeHLayout->addWidget(m_State);

    QHBoxLayout *operatorHLayout = new QHBoxLayout();
    operatorHLayout->addWidget(laOperator);
    operatorHLayout->addWidget(m_Mobile);
    operatorHLayout->addWidget(m_Unicom);
    operatorHLayout->addWidget(m_Telecom);

    m_pbClose = new QPushButton("退出");
    m_pbClose->setMaximumWidth(100);
    connect(m_pbClose, SIGNAL(clicked()), this, SLOT(pbCloseClicked()));

    QHBoxLayout *btnHLayout = new QHBoxLayout();
    btnHLayout->addWidget(m_pCancel);
    btnHLayout->addWidget(m_pConfirm);
    btnHLayout->addWidget(m_pbClose);

    QVBoxLayout *vLayout = new QVBoxLayout();

    vLayout->addLayout(typeHLayout);
    vLayout->addLayout(operatorHLayout);
    vLayout->addLayout(middleHLayout);
    vLayout->addLayout(btnHLayout);
    vLayout->setMargin(0);
    setLayout(vLayout);

    connect(m_pTimer, SIGNAL(timeout()), this, SLOT(readFile()));
    connect(m_pDialTimer, SIGNAL(timeout()), this, SLOT(isSwitchSuccess()));

    connect(m_pCancel, SIGNAL(clicked()), this, SLOT(pbCancelClicked()));
    connect(m_pConfirm, SIGNAL(clicked()), this, SLOT(pbConfirmClicked()));
}

Q3GModule::~Q3GModule()
{
    m_pFile->close();
    delete m_pFile;
    m_pTimer->stop();
    delete m_pTimer;
    m_pDialTimer->stop();
    delete m_pDialTimer;
}

void Q3GModule::pbConfirmClicked()
{
    if (access("/dev/ttyUSB0", 0) != 0)
    {
        QMessageBox::about(this, "提示", "3G/4G模块节点不存在");
        return;
    }
    m_iWaitCount = 0;
    m_pDialTimer->start(1000);
    m_State->setText("无线连接正在打开");
}

void Q3GModule::pbCancelClicked()
{
    system("killall pppd");
    m_ip->setText("");
    m_remote->setText("");
    m_dns1->setText("");
    m_dns2->setText("");
    m_State->setText("无线连接已关闭");
}

void Q3GModule::pbCloseClicked()
{
    if(m_ip->text() != "")
    {
        if(QMessageBox::question(this, "提示", "是否断开无线连接" ) == QMessageBox::Yes){
            pbCancelClicked();
        }
    }
    system("rm /tmp/3g.txt");
//    gWiFi->close();
}

void Q3GModule::readFile()
{
    m_iReadLen = m_pFile->readLine(cData, sizeof(cData));
    if (m_iReadLen > 0)
    {
        QString str = QString(cData).mid(0, m_iReadLen);
        m_pMsg->setPlainText(
                    m_pMsg->toPlainText() + str);
        QTextCursor cursor = m_pMsg->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_pMsg->setTextCursor(cursor);
        if(str.contains("succeeded"))
            m_State->setText("无线连接正在获取IP");
        if(str.contains("failed"))m_State->setText("无线连接打开失败");
        if(str.contains("local  IP address"))
            m_ip->setText(str.replace("local  IP address",""));
        if(str.contains("remote IP address") && !str.contains("defaulting to"))
            m_remote->setText(str.replace("remote IP address",""));
        if(str.contains("primary   DNS address"))
            m_dns1->setText(str.replace("primary   DNS address",""));
        if(str.contains("secondary DNS address")){
            m_State->setText("无线连接成功");
            m_dns2->setText(str.replace("secondary DNS address",""));
        }
    }
}

void Q3GModule::isSwitchSuccess()
{
    m_iWaitCount++;
    if (access("/dev/ttyUSB0", 0) == 0)//文件是否存在
    {
        m_pDialTimer->stop();
        QString cmd = "pppd call ";
        if(m_Mobile->isChecked())cmd += configure.wireless.wireless.at(m_cbType->currentIndex()).mobile + " ";
        else if(m_Unicom->isChecked())cmd += configure.wireless.wireless.at(m_cbType->currentIndex()).unicom + " ";
        else if(m_Telecom->isChecked())cmd += configure.wireless.wireless.at(m_cbType->currentIndex()).telecom + " ";
        cmd += ">> /tmp/3g.txt &";
        m_pTimer->start(10);
        system(cmd.toStdString().c_str());
    }
    else
    {
        if (m_iWaitCount>=10)
        {
            m_pDialTimer->stop();
            QMessageBox::warning(this, trUtf8("信息"), trUtf8("转换设备失败！"));
        }
    }
}
