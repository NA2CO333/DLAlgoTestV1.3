#include "serialportdatatest.h"
#include "ui_serialportdatatest.h"

#include <QDateTime>
#include <QTimer>
#include <QTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDebug>

#include "windowsmanager.h"
#include "global.h"

//
bool stop_show=true;
int singNum;


SerialPortDataTest::SerialPortDataTest(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::SerialPortDataTest)
{
    ui->setupUi(this);

    strtime = new QDateTime;
    lineEdit_Send = new myEditLine;
    vLayout = new QVBoxLayout;
    vLayoutLine1_Line2 = new QVBoxLayout;
    QLabel *kongbai = new QLabel;
    hLayoutmA = new QHBoxLayout;
    hLayout1 = new QHBoxLayout;
    hLayout2 = new QHBoxLayout;
    hLayout3 = new QHBoxLayout;

    ui->listWidget->setMinimumHeight(200);
    kongbai->setMinimumWidth(80);

    vLayout->addWidget(ui->label);
    vLayout->addWidget(ui->listWidget);
    hLayoutmA->addWidget(ui->label_11);
    hLayoutmA->addWidget(ui->label_12);
    hLayoutmA->addWidget(ui->label_13);
    hLayoutmA->addWidget(ui->label_14);
    hLayoutmA->addWidget(ui->label_15);
    vLayout->addLayout(hLayoutmA);
    vLayout->addWidget(ui->label_2);
    vLayout->addWidget(lineEdit_Send);

    hLayout1->addWidget(ui->pushButton_Send);
    hLayout1->addWidget(ui->pushButton_Recv_Stop);
    hLayout1->addWidget(ui->pushButton_Clear);
    hLayout1->addWidget(kongbai);
    hLayout1->addWidget(ui->pushButton_1);
    hLayout1->addWidget(ui->pushButton_2);
    hLayout1->addWidget(ui->pushButton_3);
    hLayout1->addWidget(ui->pushButton_4);

    hLayout2->addWidget(ui->pushButton_Save);
    hLayout2->addWidget(ui->pushButton_Export);
    hLayout2->addWidget(ui->pushButton_Delete);
    hLayout2->addWidget(kongbai);
    hLayout2->addWidget(ui->pushButton_5);
    hLayout2->addWidget(ui->pushButton_6);
    hLayout2->addWidget(ui->pushButton_7);
    hLayout2->addWidget(ui->pushButton_8);

    vLayoutLine1_Line2->addLayout(hLayout1);
    vLayoutLine1_Line2->addLayout(hLayout2);
    hLayout3->addLayout(vLayoutLine1_Line2);
    hLayout3->addStretch(1);       //添加拉伸
    hLayout3->addWidget(ui->pushButton_Back);

    //vLayout->addLayout(hLayout1);
    vLayout->addLayout(hLayout3);
    this->setLayout(vLayout);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(slot_timerTimeout()));
    timer->start(500);

    stat_stop=false;
    test_power=false;
    infrared=false;
    beep=false;
    wifi_flag=false;
    usb_port_flag=false;
    camera_flag=false;
}

SerialPortDataTest::~SerialPortDataTest()
{
    delete ui;
}

void SerialPortDataTest::showEvent(QShowEvent *_evt)
{
    Q_UNUSED(_evt)

    //
    QObject::connect(MySerialPort::instance(), &MySerialPort::sigCmdReceived, this, &SerialPortDataTest::serailportValue, Qt::QueuedConnection);

}

void SerialPortDataTest::hideEvent(QHideEvent *_evt)
{
    Q_UNUSED(_evt)

    //
    QObject::disconnect(MySerialPort::instance(), &MySerialPort::sigCmdReceived, this, &SerialPortDataTest::serailportValue);

}

void SerialPortDataTest::on_pushButton_Send_clicked()
{
    //QString  str = lineEdit_Send->text();
    //char*  ch;
    //QByteArray ba = str.toLatin1(); // must
    //ch=ba.data();

    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    QString strdata = lineEdit_Send->text();
    strdata = strdata.simplified(); //将多个空格转换为单个空格(包涵\n)
    QString showSendData;
    QString s[strdata.size()];
    int hexnum[strdata.size()];
    uchar command[strdata.size()];
    QStringList strlist = strdata.split(" ");
    for(int i=0;i<strlist.size();i++)
    {
        s[i] = "0x"+strlist.at(i);
        showSendData += s[i]+" ";
        bool ok;
        hexnum[i] = s[i].toInt(&ok,16); // 表示以16进制方式读取字符串
        command[i] = hexnum[i];
        qDebug()<<s[i]<<" "<<hexnum[i];
    }
    ui->listWidget->addItem(strdataTime+showSendData);
    MySerialPort::instance()->write(QByteArray {(char *)command, strdata.size()});
}

void SerialPortDataTest::serailportValue(int _cmd_id, QByteArray _pkg_data)
{
    //
    Q_UNUSED(_cmd_id)
    QString pkg_data_hex = MySerialPort::byteArrayToHexStr(_pkg_data);
    // TODO: 旧代码是解析 HEX，应改为直接解析 Bytes ？但是部分数据包不符合格式定义？

    //
    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    if(stop_show){
        //QString start=QString("<span style=\" color:#ff0000;\"> %1:<br>").arg(QTime::currentTime().toString("hh:mm:ss"));
        //QString end("<\/span>");
        ui->listWidget->addItem(strdataTime+pkg_data_hex+"\n");
        ui->listWidget->setCurrentRow(ui->listWidget->count()-1);
    }
    //QString strHead = str.mid(0,8);
    int i = pkg_data_hex.indexOf("557a0c91",0);
    //qDebug()<<"-----i:"<<i;
    if(pkg_data_hex.contains("557a0c91")){
        ui->listWidget->addItem(strdataTime+pkg_data_hex);
        //557a0c9100112233445566778899
        bool ok;
        int numh,numl,sum;
        QString hexstr,sumstr;
        //充电电流
        hexstr = "0x"+pkg_data_hex.mid(8,2);     //高字节中高位
        numh = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        hexstr = "0x"+pkg_data_hex.mid(10,2);    //低字节中高位
        numl = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        sum = numh*256 + numl;
        sumstr = QString::number(sum);
        ui->label_11->setText("充电:"+sumstr+"mA\n");

        //运行电流
        hexstr = "0x"+pkg_data_hex.mid(12,2);     //高字节中高位
        numh = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        hexstr = "0x"+pkg_data_hex.mid(14,2);    //低字节中高位
        numl = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        sum = numh*256 + numl;
        sumstr = QString::number(sum);
        ui->label_12->setText("运行:"+sumstr+"mA\n");

        //红外LED电流
        hexstr = "0x"+pkg_data_hex.mid(16,2);     //高字节中高位
        numh = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        hexstr = "0x"+pkg_data_hex.mid(18,2);    //低字节中高位
        numl = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        sum = numh*256 + numl;
        sumstr = QString::number(sum);
        ui->label_13->setText("红外:"+sumstr+"mA\n");

        //红外LED电流
        hexstr = "0x"+pkg_data_hex.mid(20,2);     //高字节中高位
        numh = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        hexstr = "0x"+pkg_data_hex.mid(22,2);    //低字节中高位
        numl = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        sum = numh*256 + numl;
        sumstr = QString::number(sum);
        ui->label_14->setText("纽扣:"+sumstr+"mV\n");

        //温度
        hexstr = "0x"+pkg_data_hex.mid(24,2);     //高字节中高位
        numh = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        hexstr = "0x"+pkg_data_hex.mid(26,2);    //低字节中高位
        numl = hexstr.toInt(&ok,16);    // 表示以16进制方式读取字符串
        sum = numh*256 + numl;
        sum /=10;
        sumstr = QString::number(sum);
        ui->label_15->setText("温度:"+sumstr+"°C\n");

        ui->listWidget->setCurrentRow(ui->listWidget->count()-1);
    }
}

void SerialPortDataTest::on_pushButton_Back_clicked()
{
    //getWinManage()->backToLastWidget();
    //getWinManage()->showWindow(WIN_ENGIN);
    this->hide();
}

void SerialPortDataTest::on_pushButton_Recv_Stop_clicked()
{
    stop_show = !stop_show;
    if(stop_show)
        ui->pushButton_Recv_Stop->setText("暂停接收");
    else
        ui->pushButton_Recv_Stop->setText("接收");
}

void SerialPortDataTest::on_pushButton_Clear_clicked()
{
    ui->listWidget->clear();
}

void SerialPortDataTest::on_pushButton_Export_clicked()
{
    QString disk_path = Util::CUDisk::getPath();
    bool isshow = (disk_path.length() > 0);

    if(isshow){
        // 创建目录
        QDir sourceDir(QString("%1/file_gather").arg(disk_path));
        if (!sourceDir.exists()) {
            sourceDir.mkdir(QString("%1/file_gather").arg(disk_path));
        }

        //return true;
        //拷贝文件
        //QString sourceDirFile = QString(MUSIC_DIR_PATH) + "/bird.mp3";
        QString filePath = QFileDialog::getOpenFileName(this,"打开文件","/");
        QFileInfo info(filePath);
        QString filename = info.fileName();
        QString toDir = QString("%1/file_gather/"+filename).arg(disk_path);
        toDir.replace("\\","/");
        if (filePath == toDir){
            return;
        }
        if (!QFile::exists(filePath)){
            return;
        }
        /*
        QDir *createfile     = new QDir;
        bool exist = createfile->exists(toDir);
        if(exist){
            if(coverFileIfExist){
                createfile->remove(toDir);
            }
        }//end if
        */
        QString text = tr("是否导出文件？");   // "Whether to export files or not?"
        NoticeWin msg;
        msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
        msg.setContent(text);
        if(msg.exec() == QDialog::Accepted)
        {
            if(QFile::copy(filePath, toDir))
            {
                QString text = "导出文件成功,是否卸载U盘!";
                NoticeWin msg;
                msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
                msg.setContent(text);
                if(msg.exec() == QDialog::Accepted) {
                    Util::CUDisk::sync();
                    //Util::CUDisk::umount();     //卸载U盘
                }
            }
        }
        return;
    }
    else{
        QString text = tr("未检测到U盘!");   // "U disk was not detected!"
        getWinManage()->showMsgWin(text);
    }
}

void SerialPortDataTest::on_pushButton_Save_clicked()
{
    QDir sourceDir(QString("/SerialPortData"));
    if(sourceDir.exists()){
        ;
    }
    else
        sourceDir.mkdir(QString("/SerialPortData"));
    QFile file("/SerialPortData/test.txt");
    if(file.open(QIODevice::Text | QIODevice::WriteOnly))
    {
        QTextStream out(&file);
        for (int j = 0; j < ui->listWidget->count(); j++)
        {
                QString itemText = ui->listWidget->item(j)->text();
                out<<endl<<itemText<<endl;
        }
        file.close();
    }
    if(file.size()>0)
    {
        QString text = "数据保存成功!";
        getWinManage()->showMsgWin(text);
    }
    if(file.size()==0)
    {
        QString text = "数据保存失败!";
        getWinManage()->showMsgWin(text);
    }
}

void SerialPortDataTest::on_pushButton_Delete_clicked()
{
    if(lineEdit_Send->text() == "2020"){
        QString filePath = QFileDialog::getOpenFileName(this,"打开文件","/");
        QFile rmfile(filePath);
        if(rmfile.remove()){
            QString text = "文件删除成功!";
            getWinManage()->showMsgWin(text);
        }
    }
    else{
        QString text = "发送数据编辑框密码错误!";
        getWinManage()->showMsgWin(text);
    }
}

//测试电源开关 byte4: 0关闭 1打开
void SerialPortDataTest::on_pushButton_1_clicked()
{
    test_power = !test_power;
    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    if(test_power){
        uchar command[8]={0x55,0x7a,0x06,0x12,0x00,0x00,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x12,0x00,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 12 00 00 00 00");
    }
    else{
        uchar command[8]={0x55,0x7a,0x06,0x12,0x01,0x00,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x12,0x01,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 12 01 00 00 00");
    }
}

//电源指示灯闪烁
void SerialPortDataTest::on_pushButton_2_clicked()
{
    uchar command[8]={0x55,0x7a,0x06,0x13,0x00,0x00,0x00,0x00};
    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x13,0x00,0x00,0x00,0x00\n");
    MySerialPort::instance()->write(QByteArray {(char *)command, 8});
    lineEdit_Send->setText("55 7a 06 13 00 00 00 00");
}

//超时、红外灯电源开关
void SerialPortDataTest::on_pushButton_3_clicked()
{
    infrared = !infrared;
    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    if(infrared){
        uchar command[8]={0x55,0x7a,0x06,0x07,0x00,0x00,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x07,0x00,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 07 00 00 00 00");
    }
    else{
        uchar command[8]={0x55,0x7a,0x06,0x07,0x01,0x01,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x07,0x01,0x01,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 07 01 01 00 00");
    }
}

//蜂鸣器开关
void SerialPortDataTest::on_pushButton_4_clicked()
{
    beep = !beep;
    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    if(beep){
        uchar command[8]={0x55,0x7a,0x06,0x17,0x00,0x00,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x17,0x00,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 17 00 00 00 00");
    }
    else{
        uchar command[8]={0x55,0x7a,0x06,0x17,0x01,0x00,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x17,0x01,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 17 01 00 00 00");
    }
}

//WiFi
void SerialPortDataTest::on_pushButton_5_clicked()
{
    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    wifi_flag = !wifi_flag;
    if(wifi_flag)
    {
        uchar command[8]={0x55,0x7a,0x06,0x16,0x00,0x00,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x16,0x00,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 16 00 00 00 00");
    }
    else{
        uchar command[8]={0x55,0x7a,0x06,0x16,0x01,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x16,0x01,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 16 01 00 00 00");
    }
}

//USB接口
void SerialPortDataTest::on_pushButton_6_clicked()
{
    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    usb_port_flag = !usb_port_flag;
    if(usb_port_flag)
    {
        uchar command[8]={0x55,0x7a,0x06,0x16,0x00,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x16,0x00,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 16 00 00 00 00");
    }
    else
    {
        uchar command[8]={0x55,0x7a,0x06,0x16,0x00,0x01,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x16,0x00,0x01,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 16 00 01 00 00");
    }
}

//相机
void SerialPortDataTest::on_pushButton_7_clicked()
{
    QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
    camera_flag = !camera_flag;
    if(camera_flag){
        uchar command[8]={0x55,0x7a,0x06,0x16,0x00,0x00,0x00,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x16,0x00,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 16 00 00 00 00");
    }
    else{
        uchar command[8]={0x55,0x7a,0x06,0x16,0x00,0x00,0x01,0x00};
        ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x16,0x00,0x00,0x01,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
        lineEdit_Send->setText("55 7a 06 16 00 00 01 00");
    }
}

//电流,温度
void SerialPortDataTest::on_pushButton_8_clicked()
{
    stat_stop = !stat_stop;
    if(stat_stop)
        lineEdit_Send->setText("55 7a 06 11 00 00 00 00");
    else
        lineEdit_Send->clear();
}

void SerialPortDataTest::slot_timerTimeout()
{
    if(stat_stop){
        uchar command[8]={0x55,0x7a,0x06,0x11,0x00,0x00,0x00,0x00};
        //QString strdataTime = strtime->currentDateTime().toString("yyyy-MM-dd  HH:mm:ss")+"\n";
        //ui->listWidget->addItem(strdataTime+"0x55,0x7a,0x06,0x11,0x00,0x00,0x00,0x00\n");
        MySerialPort::instance()->write(QByteArray {(char *)command, 8});
    }
}
