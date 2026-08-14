#-------------------------------------------------
#
# Project created by QtCreator 2022-08-10T10:45:33
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

#QMAKE_CXXFLAGS += -std=c++11
CONFIG += C11
CONFIG += C++11

TARGET = test_ime
TEMPLATE = app
DESTDIR = $$PWD/../bin

#lib_files.files += $$PWD/../lib/*
#lib_files.path = $$PWD/../bin
#INSTALLS += lib_files

# 是否单元测试
DEFINES += UNIT_TEST

# 是否使用通用输入法接口
DEFINES += USE_INPUT_METHOD_INTF
# 是否使用“开源输入法”库
DEFINES += USE_POENSRC_IME

#
#LIBS += -L/usr/lib -ldwIme
LIBS += -L/usr/lib -lgooglepinyin
LIBS += -L/usr/lib -lzinnia

#
#dict_files.files += $$PWD/../../../stareyes/googlepinyin/bin/googlepinyin-dict/dict_pinyin.dat
#dict_files.path = $$PWD/../bin/googlepinyin-dict
#INSTALLS += dict_files

#lib_files.files += $$PWD/../../../stareyes/googlepinyin/bin/libgooglepinyin.*
#lib_files.path = $$PWD/../bin
#INSTALLS += lib_files

#
INCLUDEPATH += $$PWD/../../srcs/include
INCLUDEPATH += $$PWD/../../srcs/mainbasic
INCLUDEPATH += $$PWD/../../srcs/mainbasic/keyboard
INCLUDEPATH += $$PWD/../../srcs/mainbasic/mywidgets

DEFINES += USE_DWHW
INCLUDEPATH += $$PWD/../../srcs/include/DWIMECore
HEADERS += $$PWD/../../srcs/mainbasic/keyboard/DWIMECore_Dll.h
SOURCES += $$PWD/../../srcs/mainbasic/keyboard/DWIMECore_Dll.cpp

contains(QT_ARCH, x86_64) {
# <glib.h> 的路径
INCLUDEPATH += /usr/include/glib-2.0
# <glibconfig.h> 的路径
INCLUDEPATH += /usr/lib/x86_64-linux-gnu/glib-2.0/include
}

contains(QT_ARCH, arm) {
# <glib.h> 的路径
INCLUDEPATH += /opt/EmbedSky/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/qt5.5/rootfs_imx6q_V3_qt5.5_env/usr/include/glib-2.0
# <glibconfig.h> 的路径
INCLUDEPATH += /opt/EmbedSky/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/qt5.5/rootfs_imx6q_V3_qt5.5_env/usr/lib/glib-2.0/include
}


#
SOURCES += main.cpp\
        widget.cpp \
    unittestintf.cpp \
    ../../srcs/mainbasic/mywidgets/baseform.cpp \
    ../../srcs/mainbasic/keyboard/keyboard.cpp \
    ../../srcs/mainbasic/keyboard/inputmethodintf.cpp \
    ../../srcs/mainbasic/keyboard/handwriteboard.cpp \
    ../../../cpp-libs/wagomu/src/wagomu.cpp

HEADERS  += widget.h \
    unittestintf.h \
    ../../srcs/mainbasic/mywidgets/baseform.h \
    ../../srcs/mainbasic/keyboard/keyboard.h \
    ../../srcs/mainbasic/keyboard/inputmethodintf.h \
    ../../srcs/mainbasic/keyboard/handwriteboard.h \
    ../../../cpp-libs/wagomu/src/wagomu.h

FORMS    += widget.ui \
    ../../srcs/mainbasic/keyboard/keyboard.ui

RESOURCES += \
    res.qrc

