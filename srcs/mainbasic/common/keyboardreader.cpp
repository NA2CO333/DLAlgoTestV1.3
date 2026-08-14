#include "keyboardreader.h"

#include <linux/input.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <asm-generic/errno-base.h>

//#include <libevdev-1.0/libevdev/libevdev.h>
// TODO: 用 libevdev 库不需手动实现由按键码到ASCII字符的转换？

#include <QString>
#include <QFile>
#include <QDebug>
#include <QTextCodec>

//
const int ARRAY_LEN = 64;
const char KEY_CHARS1[ARRAY_LEN + 1] = "abcdefghijklmnopqrstuvwxyz1234567890`-=[]\\;',./\n/*-+.\n0123456789";
const char KEY_CHARS2[ARRAY_LEN + 1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()~_+{}|:\"<>?\n/*-+.\n0123456789";
const int KEY_CODES[ARRAY_LEN] = {
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,                                       /* 主键盘区的 "abcdefghij"（10键） */
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,                                       /* 主键盘区的 "klmnopqrst"（10键） */
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z, KEY_1, KEY_2, KEY_3, KEY_4,                                       /* 主键盘区的 "uvwxyz1234"（10键） */
    KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_GRAVE, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE,                   /* 主键盘区的 "567890`-=["（10键） */
    KEY_RIGHTBRACE, KEY_BACKSLASH, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_ENTER,     /* 主键盘区的 "]\\;',./\n"（8键） */
    KEY_KPSLASH, KEY_KPASTERISK, KEY_KPMINUS, KEY_KPPLUS, KEY_KPDOT, KEY_KPENTER,                               /* 小键盘的 " / * - + . \n "（5键） */
    KEY_KP0, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KP7, KEY_KP8, KEY_KP9,                   /* 小键盘的 "0123456789"（10键） */
}; /* 按键码的宏声明见 <linux/input.h> -> "input-event-codes.h" */

const long SHIFT_VALID_PERIOD = CLOCKS_PER_SEC * 2;     // Shift 键有效期（单位：时钟点，一般是微秒）

// =============================================================================================
// class CKeyboardReader

std::string Util::CKeyboardReader::GetUSBKeyboardPath()
{
    //qDebug() << "GetUSBKeyboardPath() begin";

    QFile file("/proc/bus/input/devices");
    //QFile file("/root/debug/input");

    file.open(QFile::ReadOnly);
    QString info_text = file.readAll();
    file.close();

    //qDebug() << "info_text:\n" << info_text.toStdString().c_str();

    QString file_name = "";

#if (OS_TYPE == 1)

    /* 查找逻辑：“S: ”行包含“usb” && “H: ”行包含“sysrq kbd” */
    QString line_str = "";
    int i = 0;
    bool found = false;
    do
    {
        i = Util::readLine(info_text, line_str, i);
        //qDebug() << "line_str:\n" << line_str;
        if (line_str.startsWith("S: ") && line_str.contains("usb"))     // 定位到 USB Keyboard 设备信息所在位置
            found = true;
    } while (!found && i < info_text.length());     // TODO: 从后往前找，或者找到多个，取最后一个

    if (found)
    {
        //qDebug() << "found\n";
        i = Util::readLine(info_text, line_str, i);
        //qDebug() << "line_str:\n" << line_str;
        i = Util::readLine(info_text, line_str, i);
        //qDebug() << "line_str:\n" << line_str;
        if (line_str.contains("sysrq kbd"))
        {
            int begin = line_str.indexOf("event");
            file_name = line_str.mid(begin).trimmed();
        }
    }

#elif (OS_TYPE == 2 || OS_TYPE == 3)

    // TODO: 实现下面较严谨的查找逻辑
    /* 键盘设备标识：
     *   1、Event types 包含 EV_KEY。
     *   2、Keys 包含数字和字母。
     * 具体实现：
     *   1、以空行为分隔标志，得到各个设备的“B: EV=”和“B: KEY=”之后的十六进制整数。
     *   2、检查“B: EV”的值，其 bit1（从0开始）值须为1。
     *   3、检查“B: KEY”的值，其 include/linux/input-event-codes.h 中定义的数字和字母对应的位的值须为1。
     */

    /* 查找逻辑：“S: ”行包含“usb” && “H: ”行包含“kbd” */
    QString line_str = "";
    int i = 0;
    bool found = false;
    QString s_line, h_line;     // "S:" 行 和 "H:" 行
    do
    {
        i = Util::readLine(info_text, line_str, i);
        //qDebug() << "line_str:\n" << line_str;

        if (line_str.startsWith("S: ")) {
            s_line = line_str;
        } else if (line_str.startsWith("H: ")) {
            h_line = line_str;
        }

        if (line_str.length() == 1) {       // 遇到空行，代表一个设备结束，开始设备前一个设备的信息
            if (s_line.contains("usb") && h_line.contains("kbd")) {
                //qDebug() << "found\n";

                int begin = h_line.indexOf("event");
                int end = h_line.indexOf(' ', begin);
                file_name = h_line.mid(begin, end - begin).trimmed();

                found = true;
            }

        }
    } while (!found && i < info_text.length());     // TODO: 从后往前找，或者找到多个，取最后一个

#endif

#if OS_TYPE == 2
    if (file_name.isEmpty()) {
        file_name = "event1";
    }
#endif

    //
    QString file_path = "";
    if (file_name.length() > 0) {
        file_path = "/dev/input/" + file_name;
    }

    //qDebug() << "GetUSBKeyboardPath() end, file_path = " << file_path;

    return file_path.toStdString();
}

Util::CKeyboardReader::CKeyboardReader(QObject *_parent) : QObject(_parent)
{
    //qDebug() << "Util::CKeyboardReader::CKeyboardReader() threadid: " << QThread::currentThreadId() << endl;

    //
    QObject::connect(this, &Util::CKeyboardReader::sigStart, this, &Util::CKeyboardReader::ReadKeyInputs);

    // 用单独的线程运行
    this->moveToThread(&this->mThread);
    this->mThread.start(QThread::NormalPriority);

    //
    this->GetDevicePath();
    //this->Start();
}

Util::CKeyboardReader::~CKeyboardReader()
{
    this->Stop();

    //this->WaitUntil(&this->mTaskRuning, false, 2000000);

    this->mThread.quit();
    this->mThread.wait(5000);

    //puts("Util::CKeyboardReader::~CKeyboardReader(): end");
}

void Util::CKeyboardReader::RegListener(const void *_listener)
{
//#if (OS_TYPE != 2)
    uintptr_t listener = reinterpret_cast<uintptr_t>(_listener);
    std::vector<uintptr_t>::iterator it = std::find(mListenerList.begin(), mListenerList.end(), listener);
    if (it == mListenerList.end()) {
        mListenerList.push_back(listener);
    }

    if (mListenerList.size() == 1) {
        Start();
    }
//#else
//    Q_UNUSED(_listener)
//#endif
}

void Util::CKeyboardReader::UnregListener(const void *_listener)
{
//#if (OS_TYPE != 2)
    uintptr_t listener = reinterpret_cast<uintptr_t>(_listener);
    std::vector<uintptr_t>::iterator it = std::find(mListenerList.begin(), mListenerList.end(), listener);
    if (it != mListenerList.end()) {
        mListenerList.erase(it);
    }

    if (mListenerList.size() == 0) {
        Stop();
    }
//#else
//    Q_UNUSED(_listener)
    //#endif
}

uint Util::CKeyboardReader::countListener()
{
    return mListenerList.size();
}

void Util::CKeyboardReader::Start()
{
    if (emJobState::Stopped == this->State)
    {
        this->State = emJobState::Started;

        if (this->Block && this->mTaskRuning)
            return;
        else
            emit this->sigStart(privateSig);
    }
    else
    {
        this->State = emJobState::Started;
    }
}

void Util::CKeyboardReader::Pause()
{
    if (emJobState::Started == this->State)
        this->State = emJobState::Paused;
}

void Util::CKeyboardReader::Stop()
{
    this->State = emJobState::Stopped;
}

void Util::CKeyboardReader::GetDevicePath()
{
    //qDebug() << "Util::CKeyboardReader::GetDevicePath() threadid: " << QThread::currentThreadId() << endl;

    //
    this->mDevicePath = GetUSBKeyboardPath();

    if (this->mDevicePath.size() > 0)
        qDebug() << "Find Device Path: " << this->mDevicePath.c_str() << endl;

}

int Util::CKeyboardReader::GetCharIndex(int _code)
{
    int n = -1;
    for (int i = 0; i < ARRAY_LEN; i++)     // TODO: 效率提高？
    {
        if (_code == KEY_CODES[i])
        {
            n = i;
            break;
        }
    }
    return n;
}

char Util::CKeyboardReader::GetCharByCode(uint16_t _code)
{
    bool shifted = false;
    if ((KEY_LEFTSHIFT == _code) || (KEY_RIGHTSHIFT == _code))
    {
        this->mShifted = true;
        this->mLastTick = clock();          // TODO: clock() 仅适用于单核的设备，应改用 clock_gettime() ？但是调试时未发现这里有问题？
        //printf("     ");
        //printf("-last:%ld- ", this->last_tick);
        return 0;
    }
    else
    {
        clock_t curr_tick = clock();        // TODO: clock() 仅适用于单核的设备，应改用 clock_gettime() ？但是调试时未发现这里有问题？
        //printf("(%3ld)", curr_tick - this->last_tick);
        if (this->mShifted && (curr_tick - this->mLastTick <= SHIFT_VALID_PERIOD))
        {
            //printf("-CURR:%ld- ", curr_tick);
            shifted = true;
            this->mShifted = false;
        }
        else
        {
            //printf("-curr:%ld- ", curr_tick);
        }
        int i = GetCharIndex(_code);
        if (i >= 0) {
            return shifted ? KEY_CHARS2[i] : KEY_CHARS1[i];
        } else {
            return 0;
        }
    }
}

// 读取键盘输入任务
void Util::CKeyboardReader::ReadKeyInputs()
{
    this->mTaskRuning = true;
    //qDebug() << "Util::CKeyboardReader::ReadKeyInputs() threadid: " << QThread::currentThreadId() << endl;

    //
    const int READ_TIMEOUT_SEC = 1;     // select 函数超时秒数
    const int READ_TIMEOUT_USEC = 0;    // 单位是微秒而不是毫秒
    const int SIZE_EV = sizeof(struct input_event);
    const int BUF_LEN = 80;     /* 经视筛仪里实测，平均每批能读取到约 40 个 key input event，上下浮动约 10 个（2021-03-05 Henry） */
    const int BUF_LEN_BYTE = SIZE_EV * BUF_LEN;;
    //const int SLEEP_LEN_SEC = 2;
    //const int SLEEP_LEN_USEC = 7000;
#if OS_TYPE != 2
    const int TIME_LIMIT_OF_BARCODE = 1000;     // 条码输入限时（毫秒）
#else
    const int TIME_LIMIT_OF_BARCODE = 10000;     // 条码输入限时（毫秒）
#endif

    //
    eplasedKeyInput.start();

    //
    int fd = -1, ret = -1;
    struct timeval tv_read;
    //struct timeval tv_sleep;
    fd_set readfds;
    struct input_event ev;
    struct input_event buf[BUF_LEN];
    int count_key = 0;
    int count_char = 0;
    bool worked = true;
    bool is_device_found = false;
    bool is_device_lost = false;
    while(1)        // 第一重循环：在非 Stopped 状态下确保循环不断执行
    {
        try
        {
            if (emJobState::Stopped == this->State)
                break;

            if (!worked)
            {
                // 用 sleep 避免高速的空循环     // TODO: 有必要？有更好的方法？
                usleep(250000);

                //usleep(SLEEP_LEN_USEC);

                //tv_sleep.tv_sec = SLEEP_LEN_SEC;
                //tv_sleep.tv_usec = SLEEP_LEN_USEC;
                //select(1, NULL, NULL, NULL, &tv_sleep);     // TODO: 精度比 usleep() 高？但是效率是否低，且足够影响系统性能？

                //qDebug() << "sleep...\n";
            }
            worked = false;

            if (emJobState::Paused == this->State)
            {
                if (this->Block && (fd >= 0))
                {
                    close(fd);
                    fd = -1;

                    doOnResetBuffer();
                    count_key = 0;
                    count_char = 0;
                }

                continue;
            }

            // 确保 DevicePath 可用
            if (this->mDevicePath.length() == 0)
            {
                this->GetDevicePath();      // TODO: 改由系统消息触发，不用反复读取设备文件？

                is_device_found = (this->mDevicePath.length() > 0);
                if (!is_device_found)
                {
                    if (!is_device_lost) {      // 避免重复输出log
                        qDebug() << "Util::CKeyboardReader::ReadKeyInputs() can't find usb keyboard device!";
                        is_device_lost = true;
                    }

                    sleep(3);     // 延长等待时间

                    //
                    continue;
                } else {
                    is_device_lost = false;
                }
            }

            // 确保设备文件已打开
            if (fd < 0)
            {
                int open_flag = O_RDONLY | (this->Block ? 0 : O_NONBLOCK);
                fd = open(this->mDevicePath.c_str(), open_flag);
                if (fd < 0)
                {
                    int err_id = errno;
                    printf("Util::CKeyboardReader::ReadKeyInputs(): open device file failed, error:%d (%s)\n", err_id, strerror(err_id));
                    // TODO: log ?

                    if (ENOENT == err_id || ENODEV == err_id || EBADF == err_id)
                        this->mDevicePath = "";     // 设备路径无效时，应重新查找

                    //
                    sleep(2);   // 若打开失败，则等待一段时间
                    continue;
                }
            }

            if (!this->Block)
            {
                FD_ZERO(&readfds);
                FD_SET(fd, &readfds);

                tv_read.tv_sec = READ_TIMEOUT_SEC;
                tv_read.tv_usec = READ_TIMEOUT_USEC;
                select(fd + 1, &readfds, NULL, NULL, &tv_read);

                worked = true;

                if (!FD_ISSET(fd, &readfds))
                {
                    //printf("-select() time out\n");

                    continue;
                }
            }

            memset(&buf, 0, BUF_LEN_BYTE);
            ret = read(fd, &buf, BUF_LEN_BYTE);

            worked = true;

            if (this->Block && (emJobState::Paused == this->State))
                continue;

            if(ret > 0)
            {
                if (eplasedKeyInput.restart() > TIME_LIMIT_OF_BARCODE) {
                    doOnResetBuffer();
                    count_key = 0;
                    count_char = 0;
                }

                int ev_count = ret / SIZE_EV;
                //printf("-read_ev_count:%d-\n", ev_count);
                for (int i = 0; i < ev_count; i++)
                {
                    //memcpy(&ev, &buf + SIZE_EV * i, SIZE_EV);
                    ev = buf[i];
                    //printf("-ev_type:%u", ev.type);
                    if ((ev.type == EV_KEY) && (ev.value == 1)) {  /* 按键事件(type == EV_KEY)时 value 的含义：0：按键抬起，1：按键按下，2：按键长按重复 */
                        // 按键计数
                        count_key++;
                        //printf(", %2u ", ev.code);
                        //qDebug() << ", " << ev.code;

                        //
                        const quint16 key_val = ev.code;
                        char chr = GetCharByCode(key_val);        // NOTE: 这个函数并不支持中文
                        if (chr) {    // NOTE: chr 不等于0代表转换成功
                            // 字符计数
                            count_char++;

                            // 字节添加
                            doOnKeyReceived(chr);    // TODO: 支持中文字符的输入？

                            // 换行检查
                            //printf("(%c)", c);
                            if (key_val == KEY_ENTER || key_val == KEY_KPENTER) {
                                //printf("\n-count_key: %d, count_char: %d\n\n", count_key, count_char);
                                //puts("");

                                doOnResetBuffer();
                                count_key = 0;
                                count_char = 0;
                            }
                        } else {
                            //qDebug() << "Key to ASCII Char Failed: " << key_val;

                            // TODO: 中文字符的支持

                            //
                            if (KEY_LEFTALT == key_val || KEY_RIGHTALT == key_val) {
                                doOnKeyReceived(Qt::Key::Key_Alt);
                            }
                        }

                        //if (count_key % 10 == 0) {
                        //    puts("");
                        //}
                    }
                }
            }
            else
            {
                int err_id = errno;
                //printf("-read error. errno = %d (%s)\n", err_id, strerror(err_id));
                //printf("-read error. ret = %d\n", ret);

                switch (err_id)
                {
                case ENOENT:
                case EBADF:         // “9, Bad file descriptor”，文件描述符无效
                case ENODEV:        // “19, No such device”，USB 接收器被拔出时 read() 会发生该错误
                    if (fd >= 0)
                    {
                        qDebug() << "Util::CKeyboardReader::ReadKeyInputs() read() error: " << strerror(err_id);
                        close(fd);
                        this->mDevicePath = "";     // 设备路径无效时，应重新查找
                        fd = -1;
                    }

                    continue;
                case EAGAIN:        // “11, Resource temporarily unavailable”，O_NONBLOCK 模式下若没有数据输入时 read() 会发生该错误
                    continue;
                default:
                    qDebug() << "Util::CKeyboardReader::ReadKeyInputs(): errno switch to case default. errno: " << err_id << endl;

                    // TODO: ?

                    continue;
                }
            }
        }
        catch(...)
        {
            puts("Util::CKeyboardReader::ReadKeyInputs() catched error");

            int err_id = errno;
            printf("Util::CKeyboardReader::ReadKeyInputs(): catch error: %d (%s)\n", err_id, strerror(err_id));

            // TODO: ?
        }
    }

    if (fd >= 0)    // 退出前关闭设备文件
        close(fd);
    //qDebug() << "Util::CKeyboardReader::ReadKeyInputs() returned." << endl;
    this->mTaskRuning = false;
}

// =============================================================================================
// class CBarcodeDataDecoder

Util::CBarcodeDataDecoder::CBarcodeDataDecoder(QObject *_parent) : CKeyboardReader(_parent)
{

}

Util::CBarcodeDataDecoder::~CBarcodeDataDecoder()
{

}

void Util::CBarcodeDataDecoder::doOnKeyReceived(const int _key_code)
{
    // symcode （目前 2026-04-09 万灵帮桥视筛仪原配扫码枪）的中文解析    /* Alt键 + 5位十进制数字表示一个 GBK 字符的编码 */
    if (Qt::Key::Key_Alt == _key_code) {
        // 标志 Alt 键的接收
        m_isAltReceived = true;
        m_elapsedAltReceived.start();
    }

    if (m_isAltReceived) {
        static constexpr int ALT_EXPIRE_MS = 300;   // Alt 键的时效
        if (!m_elapsedAltReceived.isValid() || m_elapsedAltReceived.elapsed() > ALT_EXPIRE_MS) {
            // 重置 Alt 键接收状态
            resetAltState();
        }
    }

    char chr = _key_code;
    if (m_isAltReceived) {
        // 中文解析
        if (_key_code >= '0' && _key_code <= '9') {
            //
            m_gbkCodes.append(chr);

            //
            if (m_gbkCodes.size() == 5) {
                // 十进制字符串转整型，再将整型的字节原样转 QByteArray
                int gbk_code = m_gbkCodes.toUInt();     // 检查转换是否异常？
                quint16 gbk_code_16 = gbk_code;         // TODO: 检查是否溢出？
                char hi = (char)((gbk_code_16 >> 8) & 0xFF);    // 高字节
                char lo = (char)(gbk_code_16 & 0xFF);           // 低字节
                m_lineBytes.append(hi).append(lo);      // 大端序

                // 重置 Alt 键接收状态
                resetAltState();
            }

            //
            m_elapsedAltReceived.start();
        } else if (Qt::Key::Key_Alt == _key_code) {
            //
            m_elapsedAltReceived.start();
        } else {
            qWarning() << "_key_code(=" << _key_code << ") is out of bound!";

            // 重置 Alt 键接收状态
            resetAltState();
        }
    } else {
        m_lineBytes.append(chr);
    }

    //
    if (_key_code == '\n') {
        // GBK 转 UTF-8
        QByteArray utf8_bytes = gbkToUtf8(m_lineBytes);

        //
        emit sigGetLine(utf8_bytes);
    }
}

void Util::CBarcodeDataDecoder::doOnResetBuffer()
{
    m_lineBytes.clear();
}

void Util::CBarcodeDataDecoder::resetAltState()
{
    // 重置 Alt 键接收状态
    m_isAltReceived = false;
    m_elapsedAltReceived.invalidate();
    m_gbkCodes.clear();
}

QByteArray Util::CBarcodeDataDecoder::gbkToUtf8(const QByteArray &_gbk_bytes)
{
    // 1. 获取 GBK 编码器（Qt 内部管理，无需释放）
    QTextCodec *codec = QTextCodec::codecForName("GBK");

    // 容错：如果系统不支持 GBK（极少情况）
    if (!codec) {
        return QByteArray();
    }

    // 2. GBK bytes → QString（Unicode 内存表示）
    QString unicodeStr = codec->toUnicode(_gbk_bytes);

    // 3. QString → UTF-8 bytes（直接返回）
    return unicodeStr.toUtf8();
}
