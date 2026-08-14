#include "wpacommit.h"
#include "wifibt/wpa/wpa_ctrl.h"
#include <dirent.h>
#include <QDebug>
#include <QThread>
#include "windowsmanager.h"

QVector<WIFI> WpaCommit::m_wifilist;
WIFI WpaCommit::currentWIFI;

WpaCommit::WpaCommit(QObject *parent) : QObject(parent)
{
    ctrl_iface = NULL;
    ctrl_conn = NULL;
    monitor_conn = NULL;
    msgNotifier = NULL;
    ctrl_iface_dir = strdup("/var/run/wpa_supplicant");

    qDebug()<<"---------------init WpaCommit-------------------"<<QThread::currentThread();
}

bool WpaCommit::WPA_init()
{
    qDebug() << "WpaCommit::WPA_init() into ...";

    if(WinMeasure::isOpened()){
        qDebug()<<"WPA_init,Camera is running,return;";
        return false;
    }
    system("ifconfig wlan0 up");
    //system("wpa_supplicant -i wlan0 -D wext -C/var/run/wpa_supplicant &");
    int timeout = 5;
    int ret;
    while(timeout--){
        sleep(1);
        if((ret = openCtrlConnection(ctrl_iface)) == 0){
            break;
        }else if(timeout < 0)return false;
        else if(ret == -5)
#ifdef CONFIG_IOCTL_CFG80211
            system("wpa_supplicant -i wlan0 -Dnl80211 -C/var/run/wpa_supplicant &");
#else
            system("wpa_supplicant -i wlan0 -D wext -C/var/run/wpa_supplicant &");
#endif
        //system("wpa_supplicant -i wlan0 -D wext -C/var/run/wpa_supplicant &");
    }

    qDebug() << "prepare to scan wifi ...";

    char reply[10];
    size_t reply_len = sizeof(reply);
    ctrlRequest("SCAN" , reply, &reply_len);

    qDebug() << "WpaCommit::WPA_init() end";
}

bool WpaCommit::wpa_connect_wifi(WIFI wifi)
{
    qDebug() << "WpaCommit::wpa_connect_wifi() into ...";

    if(WinMeasure::isOpened()){
        qDebug()<<"wpa_connect_wifi,Camera is running,return;";
        return false;
    }
    char reply[10];
    size_t reply_len = sizeof(reply);
    memset(reply, 0 ,reply_len);
    ctrlRequest("ADD_NETWORK", reply, &reply_len);
    if(reply[0] == 'F')
        return false;
    wifi.id = "0";
    QString cmd;
    cmd = QString("SET_NETWORK %1 ssid \"%2\"").arg(wifi.id).arg(wifi.ssid);
    memset(reply, 0 ,reply_len);
    ctrlRequest(cmd.toStdString().c_str(), reply, &reply_len);

    if(wifi.key_mgmt == "NONE"){
        cmd = QString("SET_NETWORK %1 key_mgmt %2").arg(wifi.id).arg(wifi.key_mgmt);
        memset(reply, 0 ,reply_len);
        ctrlRequest(cmd.toStdString().c_str(), reply, &reply_len);
    }else{
        cmd = QString("SET_NETWORK %1 psk \"%2\"").arg(wifi.id).arg(wifi.pwd);
        memset(reply, 0 ,reply_len);
        ctrlRequest(cmd.toStdString().c_str(), reply, &reply_len);
    }

    //    cmd = QString("SET_NETWORK %1 pairwise %2").arg(wifi.id).arg(wifi.pairwise);
    //    memset(reply, 0 ,reply_len);
    //    ctrlRequest(cmd.toStdString().c_str(), reply, &reply_len);

    //    cmd = QString("SET_NETWORK %1 scan_ssid %2").arg(wifi.id).arg(1);
    //    memset(reply, 0 ,reply_len);
    //    ctrlRequest(cmd.toStdString().c_str(), reply, &reply_len);

    cmd = QString("ENABLE_NETWORK %1").arg(wifi.id);
    memset(reply, 0 ,reply_len);
    ctrlRequest(cmd.toStdString().c_str(), reply, &reply_len);

    cmd = QString("SELECT_NETWORK %1").arg(wifi.id);
    memset(reply, 0 ,reply_len);
    ctrlRequest(cmd.toStdString().c_str(), reply, &reply_len) ;

    QString res = QString(reply);

    if(res.contains("OK")){
        qDebug()<<"--wpa_connect_wifi return ok";
        return true;
    }
    qDebug()<<"--wpa_connect_wifi return false";

    qDebug() << "WpaCommit::WpaCommit::wpa_connect_wifi() end";

    return false;
}

void WpaCommit::updateResults()
{
    qDebug() << "WpaCommit::updateResults() into ...";

    char reply[2048];
    size_t reply_len;
    int index;
    char cmd[20];

    index = 0;

    m_wifilist.clear();
    while (true) {
        snprintf(cmd, sizeof(cmd), "BSS %d", index++);
        if (index > 1000)
            break;

        reply_len = sizeof(reply) - 1;
        if (ctrlRequest(cmd, reply, &reply_len) < 0)
            break;
        reply[reply_len] = '\0';
        //2021.02.01  tao 显示中文wifi名
        QByteArray str(reply);
        str.replace("\\x","%");     //\x替换成QByteArray可识别的url码,  例如"啦":\\xe5\\x95\\xa6  ==>  \%e5\%95\%a6
        QString bss = QString(QByteArray::fromPercentEncoding(str));    //先转QByteArray,再转QString
        if (bss.isEmpty() || bss.startsWith("FAIL"))
            break;

        QString ssid, bssid, signal, flags;

        QStringList lines = bss.split(QRegExp("\\n"));
        for (QStringList::Iterator it = lines.begin();
             it != lines.end(); it++) {
            int pos = (*it).indexOf('=') + 1;
            if (pos < 1)
                continue;

            if ((*it).startsWith("bssid="))         //MAC地址
                bssid = (*it).mid(pos);
            else if ((*it).startsWith("level="))    //信号强度
                signal = (*it).mid(pos);
            else if ((*it).startsWith("flags="))    //加密方式
                flags = (*it).mid(pos);
            else if ((*it).startsWith("ssid="))     //wifi名字
                ssid = (*it).mid(pos);
        }

        if (bssid.isEmpty())
            break;
        QStringList strlist = ssid.split("\\x");
        if(strlist.length() >= 4){
            if(strlist.at(1).length() == 2 && strlist.at(2).length() == 2)
                continue;
        }
        if(ssid.isEmpty())
            continue;
        WIFI wifi ;
        wifi.ssid = ssid;
        wifi.mac = bssid;
        wifi.strength = signal;
        if(flags.contains("WPA2")){
            wifi.key_mgmt = "WPA2-PSK";
            wifi.encry = true;
        }
        else if(flags.contains("WPA")){
            wifi.key_mgmt = "WPA-PSK";
            wifi.encry = true;
        }
        else{
            wifi.key_mgmt = "NONE";
            wifi.encry = false;
        }

        if(wifi.encry){
            if(flags.contains("CCMP"))
                wifi.pairwise = "CCMP";
            else if(flags.contains("TKIP"))
                wifi.pairwise = "TKIP";
        }
        bool continue_flag = false;
        for(int n=0;n<m_wifilist.size();n++)    //查找同名wifi
        {
            if(m_wifilist[n].ssid == wifi.ssid)
                continue_flag = true;
        }
        if(continue_flag){
            continue_flag=false;
            continue;   //此处作用为把同名wifi过滤掉
        }
        m_wifilist.append(wifi);
    }
    emit signal_read_wifilist();
}



int WpaCommit::str_match(const char *a, const char *b)
{
    qDebug() << "WpaCommit::str_match () into ...";

    return strncmp(a, b, strlen(b)) == 0;
}

int WpaCommit::openCtrlConnection(const char *ifname)
{
    qDebug() << "WpaCommit::openCtrlConnection() into ...";

    if(WinMeasure::isOpened()){
        qDebug()<<"openCtrlConnection,Camera is running,return;";
        return 0;
    }
    char *cfile;
    int flen;
    char buf[2048], *pos, *pos2;
    size_t len;

    if (ifname) {
        if (ifname != ctrl_iface) {
            free(ctrl_iface);
            ctrl_iface = strdup(ifname);
        }
    } else {
        struct dirent *dent;
        DIR *dir = opendir(ctrl_iface_dir);
        free(ctrl_iface);
        ctrl_iface = NULL;
        if (dir) {
            while ((dent = readdir(dir))) {
                qDebug()<<"while dent";
                if (dent->d_type != DT_SOCK &&
                        dent->d_type != DT_UNKNOWN)
                    continue;
                if (strcmp(dent->d_name, ".") == 0 ||
                        strcmp(dent->d_name, "..") == 0)
                    continue;
                printf("Selected interface '%s'\n",
                       dent->d_name);
                ctrl_iface = strdup(dent->d_name);
                break;
            }
            closedir(dir);
        }
    }

    if (ctrl_iface == NULL) {
        return -5;
    }

    flen = strlen(ctrl_iface_dir) + strlen(ctrl_iface) + 2;
    cfile = (char *) malloc(flen);
    if (cfile == NULL)
        return -1;
    snprintf(cfile, flen, "%s/%s", ctrl_iface_dir, ctrl_iface);

    if (ctrl_conn) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
    }

    if (monitor_conn) {
        delete msgNotifier;
        msgNotifier = NULL;
        wpa_ctrl_detach(monitor_conn);
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
    }

    printf("Trying to connect to '%s'\n", cfile);
    ctrl_conn = wpa_ctrl_open(cfile);
    if (ctrl_conn == NULL) {
        free(cfile);
        return -5;
    }
    monitor_conn = wpa_ctrl_open(cfile);
    free(cfile);
    if (monitor_conn == NULL) {
        wpa_ctrl_close(ctrl_conn);
        return -5;
    }
    if (wpa_ctrl_attach(monitor_conn)) {
        printf("Failed to attach to wpa_supplicant\n");
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
        return -1;
    }
    msgNotifier = new QSocketNotifier(wpa_ctrl_get_fd(monitor_conn),
                                      QSocketNotifier::Read, this);
    connect(msgNotifier, SIGNAL(activated(int)), SLOT(receiveMsgs()));

    len = sizeof(buf) - 1;
    if (wpa_ctrl_request(ctrl_conn, "INTERFACES", 10, buf, &len, NULL) >=
            0) {
        buf[len] = '\0';
        pos = buf;
        while (*pos) {
            qDebug()<<"while pos";
            pos2 = strchr(pos, '\n');
            if (pos2)
                *pos2 = '\0';
            if (pos2)
                pos = pos2 + 1;
            else
                break;
        }
    }

    len = sizeof(buf) - 1;
    if (wpa_ctrl_request(ctrl_conn, "GET_CAPABILITY eap", 18, buf, &len,
                         NULL) >= 0) {
        buf[len] = '\0';

        QString res(buf);
        QStringList types = res.split(QChar(' '));
        bool wps = types.contains("WSC");

    }

    qDebug()<<"openCtrlConnection end";

    return 0;
}

void WpaCommit::receiveMsgs()
{
    qDebug() << "WpaCommit::receiveMsgs () into ...";

    if(WinMeasure::isOpened()){
        return;
    }

    char buf[256];
    size_t len;

    while (monitor_conn && wpa_ctrl_pending(monitor_conn) > 0) {
        len = sizeof(buf) - 1;
        if (wpa_ctrl_recv(monitor_conn, buf, &len) == 0) {
            buf[len] = '\0';
            QString data =  QString(QLatin1String(buf));
            if(data.contains("REJECT")||data.contains("DISCONNECTED")){

                qDebug()<<"--offline-reciecedMsgs:"<<data;
                if(g_WifiIntf->getIsOpened()){
                    ctrlCmdHandler(CTRL_CMD_DISCON);
                    emit signal_wifiState(false);
                    qDebug()<<"--signal_wifiState";
                }

            }
            else{
                qDebug()<<"--processMsg:"<<data;
                processMsg(buf);
            }
        }
    }

}

void WpaCommit::processMsg(char *msg)
{
    qDebug() << "WpaCommit::processMsg () into ...";

    char *pos = msg, *pos2;
    int priority = 2;

    if (*pos == '<') {
        /* skip priority */
        pos++;
        priority = atoi(pos);
        pos = strchr(pos, '>');
        if (pos)
            pos++;
        else
            pos = msg;
    }
    /* Update last message with truncated version of the event */
    if (strncmp(pos, "CTRL-", 5) == 0) {
        pos2 = strchr(pos, str_match(pos, WPA_CTRL_REQ) ? ':' : ' ');
        if (pos2)
            pos2++;
        else
            pos2 = pos;
    } else
        pos2 = pos;
    QString lastmsg = pos2;
    lastmsg.truncate(40);

    pingsToStatusUpdate = 0;
    networkMayHaveChanged = true;

    if (str_match(pos, WPA_EVENT_SCAN_RESULTS))
        updateResults();
}

int WpaCommit::ctrlRequest(const char *cmd, char *buf, size_t *buflen)
{
    qDebug() << "WpaCommit::ctrlRequest() into ...";

    int ret;
    memset(buf, 0, *buflen);
    if (ctrl_conn == NULL)
        return -3;
    ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), buf, buflen, NULL);
    qDebug()<<"ctrlRequest ret = " << ret << ", cmd = \"" << cmd << "\", buf = \"" << buf << "\"";
    if (ret == -2)
        printf("'%s' command timed out.\n", cmd);
    else if (ret < 0)
        printf("'%s' command failed.\n", cmd);

    qDebug() << "WpaCommit::ctrlRequest() end";

    return ret;
}

bool WpaCommit::checkConnect()
{
    qDebug()<<"--WpaCommit::checkConnect() into, threadid = " << QThread::currentThreadId();

    char reply[2048];
    size_t reply_len = sizeof(reply) -1;
    ctrlRequest("STATUS", reply, &reply_len);
    reply[reply_len] = '\0';
    QString bss(reply);
    QStringList lines = bss.split(QRegExp("\\n"));
    QString state;
    for (QStringList::Iterator it = lines.begin();
         it != lines.end(); it++) {
        int pos = (*it).indexOf('=') + 1;
        if (pos < 1)
            continue;
        if ((*it).startsWith("wpa_state=")){
            state = (*it).mid(pos);
            if(state.contains("COMPLETED")){
               //qDebug()<<"--WpaCommit::checkConnect() return true";
               return true;
            }else  if(state.contains("4WAY_HANDSHAKE")){
               qDebug()<<"--WpaCommit::checkConnect() return false";
               wpa_ctrl_cmd("disconnect 0");
               return false;
            }
        }
    }
}

void WpaCommit::ctrlCmdHandler(int cmd)
{
    qDebug() << "WpaCommit::ctrlCmdHandler() into ..." << " , cmd = " << cmd;

    if (cmd == WPA_INIT) {
        WPA_init();
    }
    else if (cmd == WPA_CONNECT) {
        system("ifconfig wlan0 up");
        wpa_connect_wifi(currentWIFI);
    }
    else if (cmd == CTRL_REQUEST_STATUS) {
        int ret = checkConnect();
        emit signal_wifiState(ret);
    }
    else if (cmd == CTRL_CMD_DISCON) {
        wpa_ctrl_cmd("REMOVE_NETWORK 0");
        wpa_ctrl_cmd("TERMINATE");
        system("ifconfig wlan0 down");
        system("dhclient -r wlan0");
        WPA_init();
        //wpa_ctrl_cmd("SCAN");
    }
    else if (cmd == CTRL_CMD_SCAN) {
        wpa_ctrl_cmd("SCAN");
    }
    else if (cmd == CTRL_CMD_CLOSE) {
        wpa_ctrl_cmd("REMOVE_NETWORK 0");
        wpa_ctrl_cmd("TERMINATE");
        system("ifconfig wlan0 down");
        system("dhclient -r wlan0");
    }
    else if (cmd == GET_WIFILIST) {

    }
}

