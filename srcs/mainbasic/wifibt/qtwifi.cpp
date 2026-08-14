#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QTextStream>
#include "qtwifi.h"

qtWifi* qtWifi::_instance = nullptr;

//
qtWifi::qtWifi(QWidget *parent, bool on)
{
    //
    if (on)
        turnOn();
}

qtWifi::~qtWifi()
{
    _instance = nullptr;
}

void qtWifi::turnOn()
{
    printf("%sin\n", __func__);
    RK_wifi_register_callback(wifi_callback);
    if (RK_wifi_enable(1) < 0)
        printf("[%s] Rk_wifi_enable 1 fail!\n", __func__);
    if (RK_wifi_scan() < 0)
        printf("RK_wifi_scan fail!\n");
    printf("%sout\n", __func__);

    printf("%sin\n", __func__);
}

void qtWifi::turnOff()
{
    if (RK_wifi_enable(0) < 0)
        printf("RK_wifi_enable 0 fail!\n");
}

static int search_for_ssid(const char *str,const char* key)
{
//    const char key[] = "\"ssid\"";
    int i;

    if (strlen(str) < strlen(key))
        return -1;

    for (i = 0; i < (strlen(str) - strlen(key)); i++) {
        if (!strncmp(key, &str[i], strlen(key)))
            return i;
    }
    return -1;
}

static char *get_string(const char *str)
{
    int i, begin = -1, count;
    char *dst;

    for (i = 0; i < strlen(str); i++) {
        if (str[i] == '\"') {
            if (begin == -1) {
                begin = i;
                continue;
            } else {
                count = i - begin -1;
                if (!count)
                    return NULL;
                dst = strndup(&str[begin + 1], count);
                return dst;
            }
        }
    }
    return NULL;
}

static char *get_string1(const char *str)
{
    int i, begin = 0, count;
    char *dst;

    for (i = 0; i < strlen(str); i++) {
        if (str[i] == ',') {

                count = i - begin -1;
                if (!count)
                    return NULL;
                dst = strndup(&str[begin + 1], count);
                return dst;
            }
    }
    return NULL;
}

bool sortBySignal1(WiFiConfig w1, WiFiConfig w2)
{
    return w1.signal > w2.signal;
}

int qtWifi::wifi_callback(RK_WIFI_RUNNING_State_e state,
                      RK_WIFI_INFO_Connection_s *info)
{
    qtWifi *wifi = getInstance();

    if (state == RK_WIFI_State_IDLE) {
        printf("RK_WIFI_State_IDLE\n");
    } else if (state == RK_WIFI_State_CONNECTING) {
        printf("RK_WIFI_State_CONNECTING\n");
    } else if (state == RK_WIFI_State_CONNECTFAILED) {
        printf("RK_WIFI_State_CONNECTFAILED\n");
//        qtWifi::getInstance()->info_wifi_con = *info;
    } else if (state == RK_WIFI_State_CONNECTFAILED_WRONG_KEY) {
        printf("RK_WIFI_State_CONNECTFAILED_WRONG_KEY\n");
//        qtWifi::getInstance()->info_wifi_con = *info;
    } else if (state == RK_WIFI_State_CONNECTED) {
        printf("RK_WIFI_State_CONNECTED\n");
        qtWifi::getInstance()->info_wifi_con = *info;
        printf("info_wifi_con ssid:%s info_wifi_con ip:%s\n",info->ssid,info->ip_address);
    } else if (state == RK_WIFI_State_DISCONNECTED) {
        printf("RK_WIFI_State_DISCONNECTED\n");
    } else if (state == RK_WIFI_State_OPEN) {
        printf("RK_WIFI_State_OPEN\n");
    } else if (state == RK_WIFI_State_OFF) {
        printf("RK_WIFI_State_OFF\n");
    } else if (state == RK_WIFI_State_SCAN_RESULTS) {
        printf("RK_WIFI_State_SCAN_RESULTS\n");
        char *scan_r, *str = nullptr;
        int cnt = 0, tmp = 0;
        QString line;
        QStringList list;

        if (wifi == nullptr)
        {
            printf("errorrreerewrqewrqerqwrqre\n");
            return 0;
        }
        wifi->confs.clear();
        scan_r = strdup(RK_wifi_scan_r());

//        qDebug() << scan_r;
//        QVector<WiFiConfig> confs;
        while(1)
        {
            WiFiConfig conf;
            tmp = search_for_ssid(&scan_r[cnt],"\"flags\"");
            str = get_string(&scan_r[cnt + tmp + 7]);
            if (QString(str).contains("WPA")) {
                conf.key_mgmt = "WPA-PSK";
            } else {
                conf.key_mgmt = "NONE";
            }
//            qDebug() << "flags" << conf.key_mgmt << str << tmp;
            free(str);

            tmp = search_for_ssid(&scan_r[cnt],"\"rssi\"");
            str = get_string1(&scan_r[cnt + tmp + 6]);
            if (str == NULL) {
            } else {
                conf.signal = atoi(str);
            }
            free(str);

            tmp = search_for_ssid(&scan_r[cnt],"\"ssid\"");
            if (tmp == -1)
                break;
            str = get_string(&scan_r[cnt + tmp + 6]);
            if (str == NULL) {
//                line = QString("NULL");
            } else {
                conf.ssid = QString(str);
                free(str);
            }

            // 过滤隐藏网络
            if(!conf.ssid.isEmpty())
            {
                // 过滤重复网络
                bool isRepeat = false;
                foreach(const WiFiConfig &wifi, wifi->confs)
                    if(wifi.ssid == conf.ssid) isRepeat = true;
                if(!isRepeat) wifi->confs.push_back(conf);
            }
//            qDebug() << "scan" << conf.ssid << conf.key_mgmt << conf.signal << cnt;
            cnt += tmp + 6;
        }

        // 按信号强度排序
        std::sort(wifi->confs.begin(),wifi->confs.end(),sortBySignal1);
        qtWifi::getInstance()->emit_recvDev();
        free(scan_r);
    } else if (state == RK_WIFI_State_DHCP_OK) {
        printf("RK_WIFI_State_DHCP_OK\n");
    }

    //
    qtWifi::getInstance()->emit_recvStatus((int)state);

    //
    return 0;
}

//void qtWifi::emit_recvDev(QVector<WiFiConfig> confs)
//{
//    emit this->sendDev(confs);
//}

void qtWifi::emit_recvDev()
{
    emit this->sendDev();
}
void qtWifi::emit_recvStatus(int status)
{
    emit this->sendStatus(status);
}

bool qtWifi::isOn()
{
    return true;
}

