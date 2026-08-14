#include <iostream>

#include <QApplication>
#include <QTimer>

#include "windowsmanager.h"
#include "aboutdevice.h"
#include "global.h"
#include "appsetting.h"
#include "camerainit.h"
#include "versioncompatibility.h"

// 系统信号转名称字符串
const char *signalToName(int _sig_num)
{
    switch (_sig_num) {
    case SIGINT : return "SIGINT";
    case SIGILL : return "SIGILL";
    case SIGABRT: return "SIGABRT";
    case SIGFPE : return "SIGFPE";
    case SIGSEGV: return "SIGSEGV";
    case SIGTERM: return "SIGTERM";
    default:      return "??";
    }
}

// 系统信号处理函数
void signalHandler(int _sig_num) {      // NOTE: 注意：这个函数内，不可调用任何非异步安全的函数！不可调用 qDebug()，否则可能发生死锁！
    // 1. 采集崩溃栈（异步安全）
    void* buffer[30];
    int n = backtrace(buffer, 30);

    // 2. 直接输出【信号信息】到 stderr（安全，不死锁）
    char msg[128];
    snprintf(msg, sizeof(msg), "\n\n=== CRASH SIGNAL: %d(%s) ===\n", _sig_num, signalToName(_sig_num));
    write(STDERR_FILENO, msg, strlen(msg));

    // 3. 直接输出【栈】到 stderr（安全）
    backtrace_symbols_fd(buffer, n, STDERR_FILENO);     // NOTE: backtrace 是异步安全的

    // 4. 必须立即退出
    _exit(128 + _sig_num);      // NOTE: Unix/Linux 退出码规范：值范围 0-255，0：成功退出，1-127：通常为用户自定义，128+signal_number：表示因该信号终止
    //_exit(128 + _sig_num);      // NOTE: _exit() 是告诉内核：准备退出，而 exit() 会做大量清理工作，不安全。
}

// 注册崩溃关键信号处理器
void regCrashHandler()
{
    struct sigaction sa{};
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);       // 在执行信号处理函数期间，不自动阻塞其他信号
    sa.sa_flags =
            SA_RESTART |        // 自动重启被中断的系统调用
            SA_NOCLDSTOP |      // 不接收子进程的【停止/继续】信号
            0;

    sigaction(SIGSEGV, &sa, nullptr);   // 段错误（code = 11）
    sigaction(SIGABRT, &sa, nullptr);   // abort 信号（code = 6）
    sigaction(SIGILL,  &sa, nullptr);   // 非法指令
    sigaction(SIGFPE,  &sa, nullptr);   // 浮点异常
}

//
int main(int argc, char *argv[])
{
    //
    int exit_status = EXIT_SUCCESS;

    // 注册信号处理器
    //signal(SIGSEGV, signalHandler);   // 捕获段错误（code = 11）
    //signal(SIGABRT, signalHandler);   // 捕获 abort 信号（code = 6）
    regCrashHandler();

    //
    std::cout << "main() function bigin..." << std::endl;
    std::cout << "App Version: " << aboutdevice::getAppVerAll().toLocal8Bit().constData() << std::endl;

    try
    {
        //
        QApplication a(argc, argv);
        //a.addLibraryPath("");

        // 命令参数处理：输出版本号
        if (argc > 1 && strcmp(argv[1], "-v") == 0) {
            printf("version: %s\n", aboutdevice::getAppVerAll().toLocal8Bit().data());
            return 0;
        }

        // 掉电检测
#if (1 == OS_TYPE)
        //GPIO5_20程序初始化置高电平（连底板单片机 61 号引脚 PB8，即 GPIOB 组的 8 号脚？）
        system("echo 148 > /sys/class/gpio/export");
        system("echo out > /sys/class/gpio/gpio148/direction");
        system("echo 1 > /sys/class/gpio/gpio148/value");
#endif

        // rk3568 平台的初始化
#if (OS_TYPE == 3)
        // OTG 口改为 HOST 模式
        //system("echo HOST > /dev/otg_mode");
        //system("echo host > /sys/devices/platform/fe8a0000.usb2-phy/otg_mode");

        // 关闭 HDMI 接口的屏幕输出（视筛所用屏为 rgb 接口）
        system("echo off > /sys/class/drm/card0-HDMI-A-1/status");
#endif

        // 设置正常屏幕亮度
        //PowerControl::setScreenBrightnessNormal();        /* 因为这个函数用到了 WindowsManagers 对象，而这个对象在后面才创建，所以这里不能这样写。 */
#if (1 == OS_TYPE)
        PowerControl::setScreenBrightnessPercent(94);
#elif (3 == OS_TYPE)
        PowerControl::setScreenBrightnessPercent((int)CGlobal::getScreenBrightnessCfg());
#endif

        // 显示底窗口
        CBaseWindow *win_base = getWinBase();
        win_base->show();

        // 初始化设置文件（这个须在配置模块被访问前执行）
        appSetting::init();

        // 载入全局变量配置值
        CGlobal::init();

        // 设置翻译器
        bool is_succ_language = CWinManage::setTranslator(CGlobal::language);
        if (!is_succ_language) {
            qWarning() << "loading language failed!";
        }

        // 版本兼容性处理
        CVersionCompatibility *version_compat = new CVersionCompatibility;
        version_compat->processAfterUpdate();
        delete version_compat;

        // 显示初始化提示窗
        QWidget *beginWin = new QWidget(win_base);
        beginWin->setGeometry(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        beginWin->setStyleSheet("background-image: url(:/resource/black_theme/blackground_b.png);");
#if (QPA_PLATFORM_TYPE != 1)
        beginWin->setWindowFlag(Qt::FramelessWindowHint, true);
#endif
        CBaseFormIntf::centerWidget(beginWin);
        beginWin->show();

        // 检查 U 盘更新		// TODO: 程序界面已经提供 U 盘更新功能，这里不应再存在？且更新完成后应提醒用户拔出 U 盘，避免重启后再次弹出更新提示？
        //if (WindowsManagers::checkUdiskAndUpdate()) {
        //    return EXIT_SUCCESS;
        //}

        // 全局管理对象
        WindowsManagers *wm = WindowsManagers::getInstance();
        QObject::connect(&a, &QApplication::aboutToQuit, wm, &WindowsManagers::slotAboutToExit);

        wm->init();

        // 安装整个程序的事件过滤器
        a.installEventFilter(wm);

        // 打开底板串口
        MySerialPort::instance()->open();

        // 显示第一个界面
        wm->showLogin();

        // 若未激活，显示激活二维码对话框
        if (!CGlobal::isDevActivated) {
            // 显示激活二维码对话框
            globalService()->showDevActivateDialog();
        }

        // 设置灯珠电流等级
        g_WinMeasure->setLedLevel(4000, true);

        //
        beginWin->hide();
        //delete beginWin;      // TODO: 嵌入式平台里，在这里销毁这个窗体，会导致“Segmentation fault”异常，程序崩溃？
        beginWin->deleteLater();

        //
        exit_status = a.exec();

        // 释放其它资源


        // 释放 logger （放到程序最后）
        releaseLogger();
    }
    catch (...)
    {
        int err_num = errno;
        std::cout << "exception in main(), errno = " << err_num << ", errstr = " << strerror(err_num) << std::endl;
        output_trace<void>();
    }

    //
    return exit_status;
}
