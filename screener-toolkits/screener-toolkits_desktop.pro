#-------------------------------------------------
#
# Project created by QtCreator 2021-11-30T11:28:41
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = main-toolkits_desktop
TEMPLATE = app
# 使 GCC 生成 executable 格式而不是 shared object 格式的文件
QMAKE_LFLAGS += -no-pie

DESTDIR = $$PWD/../bin/bin
INCLUDEPATH = ../include

DEFINES += DESKTOP

SOURCES += main.cpp\
        mainwindow.cpp \
    winphototest.cpp \
    global.cpp \
    serialdataretrans.cpp

HEADERS  += mainwindow.h \
    winphototest.h \
    global.h \
    serialdataretrans.h

FORMS    += mainwindow.ui \
    winphototest.ui

# 操作系统类型（1:i.MX6Q, 2:PC-Linux, 3:rk3568）
DEFINES += OS_TYPE=2

