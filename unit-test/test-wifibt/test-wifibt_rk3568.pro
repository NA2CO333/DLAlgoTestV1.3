#-------------------------------------------------
#
# Project created by QtCreator 2021-05-31T13:42:39
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = $$PWD/../../bin/bin/test-wifibt_rk3568
TEMPLATE = app
# 使 GCC 生成 executable 格式而不是 shared object 格式的文件
QMAKE_LFLAGS += -no-pie

#QMAKE_CXXFLAGS += -std=c++0x
CONFIG += C11
CONFIG += C++11

INCLUDEPATH += $$PWD/../../srcs/include

# 是否运行在单元测试程序中
DEFINES += UNIT_TEST
# 是否测试模式
DEFINES += TEST_MODE


#程序版本（转为整型，次版本号和修订版本号都占2位）
DEFINES += APP_VER=10500


# 操作系统类型（1:i.MX6Q, 2:PC-Linux, 3:rk3568）
DEFINES += OS_TYPE=3

# 屏幕尺寸类型（1:800x480, 2:1280x720）
DEFINES += SCREEN_SIZE_TYPE=1


SOURCES += main.cpp \
    ../../srcs/mainbasic/appsetting.cpp \
    ../../srcs/mainbasic/logger.cpp \
    ../../srcs/mainbasic/common/util.cpp \
    ../../srcs/mainbasic/wifibt/wifiintf.cpp \
    wintestrkwifibt.cpp

FORMS += \
    wintestrkwifibt.ui
    #../../srcs/mainbasic/statusbarform.ui \

HEADERS += \
    ../../srcs/mainbasic/appsetting.h \
    ../../srcs/mainbasic/logger.h \
    ../../srcs/mainbasic/common/util.h \
    ../../srcs/mainbasic/wifibt/wifiintf.h \
    wintestrkwifibt.h

#
INCLUDEPATH += $$PWD/../../srcs/include/opencv_2.4

# 蓝牙类型（1:JDY-34, 2:rk3568）
DEFINES += BLUETOOTH_TYPE=2

INCLUDEPATH += $$PWD/../../srcs/include/RkWifiBt

HEADERS += ../../srcs/mainbasic/wifibt/bluetoothrk.h \
           ../../srcs/mainbasic/wifibt/bluetoothintf.h
           #../../srcs/mainbasic/wifibt/winbluetooth.h \
SOURCES += ../../srcs/mainbasic/wifibt/bluetoothrk.cpp \
           ../../srcs/mainbasic/wifibt/bluetoothintf.cpp
           #../../srcs/mainbasic/wifibt/winbluetooth.cpp \
#FORMS   += ../../srcs/mainbasic/wifibt/winbluetooth.ui

LIBS += -L$$PWD/../../lib/RkWifiBt/arm64 -lrkwifibt

# WiFi 类型（1:i.MX6Q, 2:rk3568）
DEFINES += WIFI_TYPE=2
HEADERS += ../../srcs/mainbasic/wifibt/wifirk.h \
           ../../srcs/mainbasic/wifibt/qtwifi.h
SOURCES += ../../srcs/mainbasic/wifibt/wifirk.cpp \
           ../../srcs/mainbasic/wifibt/qtwifi.cpp


