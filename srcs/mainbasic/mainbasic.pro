#-------------------------------------------------
#
# Project created by QtCreator 2018-03-19T09:40:47
#
#-------------------------------------------------

QT += core gui sql serialport network multimedia
#QT += xlsx
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += C11
CONFIG += C++11

TARGET = main
TEMPLATE = app
DESTDIR = $$PWD/../../bin/bin

DEPENDPATH += $$PWD
INCLUDEPATH += $$PWD

#
SOURCES += \
    util.cpp \
    logger.cpp \
    global.cpp \
    main.cpp\
    mainwindow.cpp \
    tool.cpp \
    result.cpp \
    winmeasure.cpp \
    measurectrl.cpp \
    capturethread.cpp \
    algorithmthread.cpp \
    mysqlite.cpp \
    mysqlitepatients.cpp \
    edit.cpp \
    eyesightstandard.cpp \
    batchscreen.cpp \
    personalinfos.cpp \
    camerainit.cpp \
    windowsmanager.cpp \
    import.cpp \
    threadmodel.cpp \
    myserialport.cpp \
    datepage.cpp\
    statusbarform.cpp \
    runtask.cpp \
    mydialog.cpp \
    myeditline.cpp \
    noticewin.cpp \
    blockmousepress.cpp \
    messagewin.cpp \
    circlshowthread.cpp \
    findpyrthread.cpp \
    BarcodeDecoder.cpp \
    detectbarcode.cpp \
    system/setting.cpp \
    system/systemini.cpp \
    system/soundintf.cpp \
    #3G_4G/u3GModule.cpp \
    wifibt/inputwifipwd.cpp \
    wifibt/wifidetails.cpp \
    wifibt/winwifi.cpp \
    wifibt/winbluetooth.cpp \
    wifibt/bluetoothintf.cpp \
    wifibt/wifiintf.cpp \
    DataTransTest/DataTransmit.cpp \
    datatrans.cpp \
    DataTransTest/mongoose-6.7/mongoose.c \
    uploadthread.cpp \
    printersetting.cpp \
    printertransmit.cpp \
    enhancementimag.cpp \
    qrcodeinput.cpp \
    batchuploadlist.cpp \
    tablemap.cpp \
    musicsetting.cpp \
    progresswindow.cpp \
    batterymonitor.cpp \
    update.cpp \
    updatedialog.cpp \
    pipewrite.cpp \
    history.cpp \
    themebackground.cpp \
    aboutdevice.cpp \
    settings/settings.cpp \
    engineermode/engineermode.cpp \
    engineermode/serialportdatatest.cpp \
    settings/loginwin.cpp \
    settings/runningstatus.cpp \
    imagewidget.cpp \
    previewimage.cpp \
    engineermode/engineerpassword.cpp \
    keyboard/keyboard.cpp \
    keyboard/handwriteboard.cpp \
    keyboard/inputmethodintf.cpp \
    dialoglistselect.cpp \
    keyboardreader.cpp \
    appsetting.cpp \
    engineermode/shellsimulate.cpp \
    distancedetect.cpp \
    mywidgets/mylable.cpp \
    mywidgets/measurestatview.cpp \
    mywidgets/baseform.cpp \
    mywidgets/eyelimitmark.cpp \
    serialdatatrans.cpp \
    distcalibration.cpp \
    #lampcalibrate.cpp \
    includes.cpp \
    CameraIntf.cpp

HEADERS  += \
    util.h \
    logger.h \
    global.h \
    mainwindow.h \
    tool.h \
    result.h \
    winmeasure.h \
    measurectrl.h \
    capturethread.h \
    algorithmthread.h \
    mysqlite.h \
    mysqlitepatients.h \
    edit.h \
    eyesightstandard.h \
    batchscreen.h \
    personalinfos.h \
    camerainit.h \
    windowsmanager.h \
    GraphUtils.h \
    import.h \
    threadmodel.h \
    myserialport.h \
    datepage.h\
    statusbarform.h \
    runtask.h \
    mydialog.h \
    myeditline.h \
    noticewin.h \
    blockmousepress.h \
    messagewin.h \
    circlshowthread.h \
    findpyrthread.h \
    BarcodeDecoder.h \
    detectbarcode.h \
    functions.h \
    system/setting.h \
    system/systemini.h \
    system/soundintf.h \
    #3G_4G/u3GModule.h \
    includes.h \
    wifibt/inputwifipwd.h \
    wifibt/wifidetails.h \
    wifibt/winwifi.h \
    wifibt/winbluetooth.h \
    wifibt/bluetoothintf.h \
    wifibt/wifiintf.h \
    globelwireless.h \
    DataTransTest/DataTransmit.h \
    datatrans.h \
    DataTransTest/mongoose-6.7/mongoose.h \
    uploadthread.h \
    printersetting.h \
    printertransmit.h \
    enhancementimag.h \
    qrcodeinput.h \
    batchuploadlist.h \
    tablemap.h \
    batterymonitor.h \
    musicsetting.h \
    progresswindow.h \
    update.h \
    updatedialog.h \
    pipewrite.h \
    history.h \
    themebackground.h \
    aboutdevice.h \
    settings/settings.h \
    engineermode/engineermode.h \
    engineermode/serialportdatatest.h \
    settings/loginwin.h \
    settings/runningstatus.h \
    imagewidget.h \
    previewimage.h \
    engineermode/engineerpassword.h \
    keyboard/keyboard.h \
    keyboard/handwriteboard.h \
    keyboard/inputmethodintf.h \
    dialoglistselect.h \
    keyboardreader.h \
    appsetting.h \
    engineermode/shellsimulate.h \
    distancedetect.h \
    mywidgets/mylable.h \
    mywidgets/measurestatview.h \
    mywidgets/baseform.h \
    mywidgets/eyelimitmark.h \
    serialdatatrans.h \
    distcalibration.h \
    #lampcalibrate.h \
    CameraIntf.h

FORMS   += \
    import.ui \
    datepage.ui\
    mydialog.ui \
    noticewin.ui \
    blockmousepress.ui \
    messagewin.ui \
    qrcodeinput.ui \
    batchuploadlist.ui \
    updatedialog.ui \
    result.ui \
    musicsetting.ui \
    printersetting.ui \
    tool.ui \
    mainwindow.ui \
    themebackground.ui \
    eyesightstandard.ui \
    batchscreen.ui \
    edit.ui \
    history.ui \
    datatrans.ui \
    aboutdevice.ui \
    personalinfos.ui \
    settings/settings.ui \
    engineermode/serialportdatatest.ui \
    settings/loginwin.ui \
    settings/runningstatus.ui \
    statusbarform.ui \
    wifibt/inputwifipwd.ui \
    wifibt/wifidetails.ui \
    wifibt/winwifi.ui \
    wifibt/winbluetooth.ui \
    engineermode/engineerpassword.ui \
    previewimage.ui \
    winmeasure.ui \
    engineermode/engineermode.ui \
    keyboard/keyboard.ui \
    progresswindow.ui \
    update.ui \
    dialoglistselect.ui \
    engineermode/shellsimulate.ui \
    lampcalibrate.ui

RESOURCES += \
    resources.qrc

#
INCLUDEPATH += $$PWD/../include

INCLUDEPATH += $$PWD/mywidgets
INCLUDEPATH += $$PWD/wifibt
INCLUDEPATH += $$PWD/keyboard

#INCLUDEPATH += /opt/Qt5.5.1/5.5/gcc_64/include

#INCLUDEPATH += $$PWD/../include/QtXlsx
#LIBS += -L$$PWD/../../lib/Qt5Xlsx/arm32 -lQt5Xlsx

#
#DEPENDPATH += /opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/usr/lib
#DEPENDPATH += /opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/usr/lib/pkgconfig

#QMAKE_RPATHDIR += $$PWD/../../lib/opencv/arm64

#message($$PWD/../../lib/opencv/arm64)
#QMAKE_RPATHDIR += $$PWD/../../lib/opencv/arm64
#QMAKE_RPATHDIR += /root/work/screener/lib/opencv/arm64
INCLUDEPATH += $$PWD/../include/opencv_2.4
LIBS += -L$$PWD/../../lib/opencv/arm \
    -lopencv_core \
    -lopencv_highgui \
    -lopencv_imgproc \
    -lopencv_legacy \
    -lopencv_ml \
    -lopencv_nonfree \
    -lopencv_objdetect \
    -lopencv_ocl \
    -lopencv_photo \
    -lopencv_stitching \
    -lopencv_superres \
    -lopencv_features2d \
    -lopencv_calib3d

PKGCONFIG += openssl
INCLUDEPATH += $$PWD/../include/openssl_1.0
LIBS += -L/opt/EmbedSky/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/qt5.5/rootfs_imx6q_V3_qt5.5_env/usr/lib/ssl -lssl -lcrypto

#
LIBS += -L$$PWD/../../lib/zbar/arm -lzbar

# 操作系统类型（1:i.MX6Q, 2:PC-Linux, 3:rk3568）
DEFINES += OS_TYPE=1

#HEADERS += testdesktop.h
#SOURCES += testdesktop.cpp

# 是否测试模式
#DEFINES += TEST_MODE

# 屏幕尺寸类型（1:800x480, 2:1280x720）
DEFINES += SCREEN_SIZE_TYPE=2

# QPA 平台插件类型（0:unknow, 1:eglfs, 2:wayland, 3:PC）
DEFINES += QPA_PLATFORM_TYPE=1
# 是否启用 FrameBuffer
DEFINES += SURPORT_FRAME_BUFFER

# 蓝牙类型（1:JDY-34, 2:rk3568）
DEFINES += BLUETOOTH_TYPE=1
HEADERS += wifibt/bluetoothserial.h
SOURCES += wifibt/bluetoothserial.cpp

#DEFINES += BLUETOOTH_TYPE=2
#INCLUDEPATH += $$PWD/../include/RkWifiBt
#HEADERS += wifibt/bluetoothrk.h \
#           wifibt/qtbt.h
#SOURCES += wifibt/bluetoothrk.cpp \
#           wifibt/qtbt.cpp

#LIBS += -L$$PWD/../../lib/RkWifiBt/arm64 -lrkwifibt

# WiFi 类型（1:i.MX6Q, 2:rk3568）
DEFINES += WIFI_TYPE=1
INCLUDEPATH += $$PWD/wifibt/wpa
HEADERS += wifibt/wifiwpa.h \
           wifibt/wpa/build_config.h \
           wifibt/wpa/includes.h \
           wifibt/wpa/wpa_ctrl.h \
           wifibt/wpa/wpacommit.h
SOURCES += wifibt/wifiwpa.cpp \
           wifibt/wpa/wpa_ctrl.c \
           wifibt/wpa/wpacommit.cpp \
           wifibt/wpa/os_unix.c

DEFINES += CONFIG_CTRL_IFACE
DEFINES += CONFIG_CTRL_IFACE_UNIX
DEFINES += CONFIG_IOCTL_CFG80211

#
#DEFINES += WIFI_TYPE=2
#HEADERS += wifibt/wifirk.h \
#           wifibt/qtwifi.h
#SOURCES += wifibt/wifirk.cpp \
#           wifibt/qtwifi.cpp

# 相机类型（1:迈德威视, 2:度申）
DEFINES += CAMERA_TYPE=1

INCLUDEPATH += $$PWD/../include/Camera_MindVision_2.1.0.20
#INCLUDEPATH += $$PWD/../include/Camera_MindVision_2.1.0.32

#DEFINES += CAMERA_MINDVISION_SDK_NEW

LIBS += -L$$PWD/../../lib/MVSDK_2.1.0.20/arm -lMVSDK
#LIBS += -L$$PWD/../../lib/MVSDK_2.1.0.32/arm -lMVSDK

#
#DEFINES += CAMERA_TYPE=2
#INCLUDEPATH += $$PWD/../include/Camera_Do3Think_2.22.40
#LIBS += -L$$PWD/../../lib/Do3Think_2.22.40/arm64 -ldvp -lhzd

# 是否使用 “dwIme”
DEFINES += USE_DWIME
LIBS += -L$$PWD/../../lib/dwIme/arm -ldwIme

# 是否使用 GOOGLEPINYIN
#DEFINES += USE_GOOGLEPINYIN
#LIBS += -L$$PWD/../../../cpp-libs/googlepinyin/bin/arm -lgooglepinyin

# 是否使用 ZINNIA 手写
#DEFINES += USE_ZINNIA
#LIBS += -L$$PWD/../../../cpp-libs/zinnia/bin/arm -lzinnia

# 是否使用 WAGOMU 手写
#DEFINES += USE_WAGOMU
#LIBS += -L$$PWD/../../../cpp-libs/wagomu/bin/arm -lwagomu

# 是否使用 多文手写
#DEFINES += USE_DWHW
#INCLUDEPATH += $$PWD/../include/DWIMECore
#LIBS += -L$$PWD/../../lib/DWIMECore/x86_64 -lDWIMECore
#HEADERS += keyboard/DWIMECore_Dll.h
#SOURCES += keyboard/DWIMECore_Dll.cpp

# <glib.h> 的路径
#INCLUDEPATH += /opt/EmbedSky/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/qt5.5/rootfs_imx6q_V3_qt5.5_env/usr/include/glib-2.0
# <glibconfig.h> 的路径
#INCLUDEPATH += /opt/EmbedSky/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/qt5.5/rootfs_imx6q_V3_qt5.5_env/usr/lib/glib-2.0/include

#
#DISTFILES += update

# 编译号加1，并返回为新编译号
defineReplace(getBuildVer) {
  FILE_NAME = build_ver_imx6q.txt
  LINES = $$cat($$PWD/../../$$FILE_NAME, lines)
  message( LINES = $$LINES )
  VER = $$take_first(LINES)
  isEmpty(VER) {
    VER = 0
  }
  VER = $$num_add($$VER, 1)
  message( new VER = $$VER )
  write_file($$PWD/../../$$FILE_NAME, VER)
  return ($$VER)
}

#调用版本号自动升级函数
!build_pass {
  BUILD_VER = $$getBuildVer()
  message(BUILD_VER = $$BUILD_VER )
  DEFINES += BUILD_VER=$$BUILD_VER
}

