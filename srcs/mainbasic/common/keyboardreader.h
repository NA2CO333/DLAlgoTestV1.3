#ifndef KEYBOARDREADER_H
#define KEYBOARDREADER_H

#include <time.h>
#include <stdio.h>
#include <vector>

#include <QObject>
#include <QThread>
#include <QElapsedTimer>

#include "util-common.h"

/* 类结构：
 * CKeyboardReader:
 * 输出：原始的完整的按键事件列表，包括【字母/数字/符号】键、修饰键、及组合键的修饰键状态。类型：（QVector<Qt::Key>？或标准库的按键定义？或自定义？）
 * CBarcodeDataDecoder : CKeyboardReader
 * 输出：解析后的文本。类型：QString
 */

namespace Util {

// 任务状态
enum emJobState {
    Stopped = 0,    // 停止，任务线程已退出。
    Started,        // 开始，启动任务线程处理信息。
    Paused,         // 暂停，任务线程依然运行，但是暂停处理信息了。
};

// =============================================================================================
// class CKeyboardReader

// USB 键盘设备读取工具
/* 用法：
 * 1、连接 sigGetLine 信号，获得键盘输入的行文本；在不需要时断开连接。
 * 2、需要用时调用 RegListener()，不需要时调用 UnregListener()。类内部对帧听者进行计数，当没有对象使用时，将停止对键盘输入的监听线程。
 */
class CKeyboardReader : public QObject
{
    Q_OBJECT
protected:
    clock_t mLastTick = 0;
    bool mShifted = false;          // 前一个输入的按键是否 Shift	// TODO: C++ 不支持在类声明时初始化变量？
    std::string mDevicePath;
    QThread mThread;
    bool mTaskRuning = false;       // 任务是否正在执行（任务函数开始和结束时分别置为 ture 和 false）
    emJobState State = Stopped;     // 任务状态
    QPrivateSignal privateSig;
    std::vector<uintptr_t> mListenerList;       // 用于判断是否有帧听者，从而确定是否开启或关闭服务
    QElapsedTimer eplasedKeyInput;

    std::string GetUSBKeyboardPath();       // 获得 USB 键盘的设备文件路径

    void GetDevicePath();
    int GetCharIndex(int _code);
    char GetCharByCode(uint16_t _code);     // 按键码转 ASCII 字符        // NOTE: 这个函数并不支持中文
    void ReadKeyInputs();

    void Start();   // 开始侦听键盘输入
    void Pause();   // 暂停侦听键盘输入        // TODO: 一般情况下应该用 Stop() ？除非频率很高的开始和暂停，否则用暂停反而占用资源更多？
    void Stop();    // 停止侦听键盘输入

public:
    explicit CKeyboardReader(QObject *_parent = nullptr);
    ~CKeyboardReader();

    bool Block = false;     // TODO: 阻塞模式好像没有价值，去掉？

    void RegListener(const void *_listener);        // 注册帧听者，用以计算帧听者数量，当帧听者大于 0 时，开启服务
    void UnregListener(const void *_listener);      // 反注册帧听者，用以计算帧听者数量，当帧听者等于 0 时，关闭服务
    uint countListener();

signals:
    void sigGetLine(QByteArray _line_bytes);     // 【获得行字节（以'\n'结束）】信号
    /* 私有 */
    void sigStart(QPrivateSignal);              // 启动设备读取任务

protected:
    virtual void doOnKeyReceived(const int _key_code) = 0;      // 【按键接收】事件
    virtual void doOnResetBuffer() = 0;                         // 【重置缓冲区】事件

};

// =============================================================================================
// class CBarcodeDataDecoder

// 二维码数据解析器
class CBarcodeDataDecoder : public CKeyboardReader
{
    Q_OBJECT
public:
    explicit CBarcodeDataDecoder(QObject *_parent = nullptr);
    ~CBarcodeDataDecoder();

protected:
    void doOnKeyReceived(const int _key_code) override;     // 【按键接收】事件
    void doOnResetBuffer() override;                        // 【重置缓冲区】事件

    void resetAltState();                   // 重置 Alt 键接收状态

    QByteArray gbkToUtf8(const QByteArray &_gbk_bytes);    // 将 GBK 编码的 QByteArray 转为 UTF-8 编码的 QByteArray

    QByteArray m_lineBytes;                 // 行字节列表
    bool m_isAltReceived {false};           // Alt 键是否已接收到
    QElapsedTimer m_elapsedAltReceived;     // Alt 键接收的计时
    QString m_gbkCodes;                     // GBK 编码

};

}

#endif // KEYBOARDREADER_H
