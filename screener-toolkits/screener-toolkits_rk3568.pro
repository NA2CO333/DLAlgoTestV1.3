#-------------------------------------------------
#
# Project created by QtCreator 2021-11-30T11:28:41
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = screener-toolkits
TEMPLATE = app

DESTDIR = $$PWD/../bin/bin

SOURCES += main.cpp\
        mainwindow.cpp \
    global.cpp \
    serialdataretrans.cpp \
    winphototest.cpp

HEADERS  += mainwindow.h \
    global.h \
    serialdataretrans.h \
    winphototest.h

FORMS    += mainwindow.ui \
    winphototest.ui

# 操作系统类型（1:i.MX6Q, 2:PC-Linux, 3:rk3568）
DEFINES += OS_TYPE=3

