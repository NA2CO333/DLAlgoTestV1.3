#-------------------------------------------------
#
# Project created by QtCreator 2018-03-19T09:40:47
#
#-------------------------------------------------

QT += core gui sql serialport network multimedia
#QT += xlsx
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets websockets

CONFIG += c++17

TARGET = screener
TEMPLATE = app
DESTDIR = $$PWD/../../bin/bin

# TODO: RK3568-Qt5.14.2 编译套件的 Debug 模式生成的 Makefile 里没有 -g 选项？先在这里手动添加
CONFIG(debug, debug|release) {
    QMAKE_CFLAGS_DEBUG += -g
    QMAKE_CXXFLAGS_DEBUG += -g

    #TARGET = screener-d
    #CONFIG += separate_debug_info
    CONFIG += no_strip
}

# 是否打开算法模块的日志（0为关闭，1为打开）
DEFINES+=PERF_LOG=0

DEPENDPATH += $$PWD
INCLUDEPATH += $$PWD

#
SOURCES += \
    authintf.cpp \
    common/globalclass.cpp \
    common/globaltypes.cpp \
    common/mpro-sys-communic.cpp \
    common/otaupdatedefs.cpp \
    common/remote-service/commandhandler.cpp \
    common/remote-service/remoteservice.cpp \
    common/remote-service/websocketconn.cpp \
    common/report-text.cpp \
    common/sysinfo.cpp \
    common/update.cpp \
    common/usb-drive-monitor.cpp \
    common/utilui.cpp \
    common/versioncompatibility.cpp \
    common/websocket.cpp \
    dialoglanguage.cpp \
    engineermode/winunittest.cpp \
    external-data-intf/data-intf-an-hui-screen.cpp \
    external-data-intf/data-intf-other.cpp \
    form-dev-activate.cpp \
    global_intf.cpp \
    global_obj.cpp \
    external-data-intf/data-intf-guanxin.cpp \
    external-data-intf/data-intf-huayi.cpp \
    maintenance/maintenance.cpp \
    measure/exposure-adjuster.cpp \
    mywidgets/basewindow.cpp \
    mywidgets/combobox.cpp \
    mywidgets/data.cpp \
    mywidgets/doublespinbox.cpp \
    mywidgets/modalwin.cpp \
    mywidgets/mpro-wx-svc-qr-code.cpp \
    mywidgets/report.cpp \
    mywidgets/screener-report-receipt.cpp \
    mywidgets/suspensionpromptbox.cpp \
    algo/algo.cpp \
    algo/algo_utils.cpp \
    algo/pupil_pair_onnx_detector.cpp \
    algo/pupil_pair_rknn_detector.cpp \
    algo/pupil_light_tracker.cpp \
    algo/pupil_cross_round_tracker.cpp \
    algo/algointf.cpp \
    algo/ransac.cpp \
    algo/refractionstrategy.cpp \
    algo/cascadepool.cpp \
    algo/perftimer.cpp \
    algo/haar_thread_safe.cpp \
    #algo/algorithmthread.cpp \
    #algo/runtask.cpp \
    hardware.cpp \
    mywidgets/cserialport.cpp \
    mywidgets/waitingmovie.cpp \
    mywidgets/widget-loading.cpp \
    mywidgets/widget-optical-type-options.cpp \
    mywidgets/winlog.cpp \
    mywidgets/myeditline.cpp \
    common/keyboardreader.cpp \
    common/printer_a4/cups-intf.cpp \
    common/printer_a4/print-intf.cpp \
    common/landevfinder/landevfinderclient.cpp \
    common/landevfinder/landevfinderclientqt.cpp \
    common/util-common.cpp \
    common/logger/logger.cpp \
    common/logger/log-appender.cpp \
    common/logger/log-layout.cpp \
    common/nettools.cpp \
    global.cpp \
    main.cpp\
    mainwindow.cpp \
    tool.cpp \
    result.cpp \
    util-app.cpp \
    external-data-intf/win-guanxin-testee-edit.cpp \
    external-data-intf/win-guanxin-testee-query.cpp \
    winclinic.cpp \
    windiagnosissuggestion.cpp \
    windiagnosticstandard.cpp \
    winmeasure.cpp \
    measure/measurectrl.cpp \
    measure/capturethread.cpp \
    algo-invoker.cpp \
    mysqlite.cpp \
    mysqlitepatients.cpp \
    eyesightstandard.cpp \
    personalinfos.cpp \
    camerainit.cpp \
    windowsmanager.cpp \
    winmanage.cpp \
    import.cpp \
    threadmodel.cpp \
    myserialport.cpp \
    datepage.cpp\
    statusbarform.cpp \
    mydialog.cpp \
    noticewin.cpp \
    blockmousepress.cpp \
    messagewin.cpp \
    circlshowthread.cpp \
    BarcodeDecoder.cpp \
    detectbarcode.cpp \
    system/setting.cpp \
    system/systemini.cpp \
    system/soundintf.cpp \
    #3G_4G/u3GModule.cpp \
    wifibt/inputwifipwd.cpp \
    wifibt/wifidetails.cpp \
    wifibt/winwifi.cpp \
    wifibt/wifiintf.cpp \
    DataTransTest/DataTransmit.cpp \
    windatatrans.cpp \
    DataTransTest/mongoose-6.7/mongoose.c \
    uploadthread.cpp \
    printersetting.cpp \
    printertransmit.cpp \
    enhancementimag.cpp \
    qrcodeinput.cpp \
    #batchuploadlist.cpp \
    tablemap.cpp \
    musicsetting.cpp \
    progresswindow.cpp \
    batterymonitor.cpp \
    updatedialog.cpp \
    pipewrite.cpp \
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
    appsetting.cpp \
    engineermode/shellsimulate.cpp \
    distancedetect.cpp \
    mywidgets/mylabel.cpp \
    mywidgets/measurestatview.cpp \
    mywidgets/baseform.cpp \
    mywidgets/eyelimitmark.cpp \
    serialdatatrans.cpp \
    distcalibration.cpp \
    #lampcalibrate.cpp \
    includes.cpp \
    CameraIntf.cpp \
    winmultiresults.cpp \
    winpersonalrecord.cpp \
    winscreen.cpp \
    winupdateprogress.cpp \
    winupdatesetup.cpp

HEADERS  += \
    authintf.h \
    common/globalclass.h \
    common/globaltypes.h \
    common/mpro-sys-communic.h \
    common/otaupdatedefs.h \
    common/qserializer.h \
    common/remote-service/commandhandler.h \
    common/remote-service/remoteservice.h \
    common/remote-service/remoteservicedefs.h \
    common/remote-service/websocketconn.h \
    common/report-text.h \
    common/sysinfo.h \
    common/update.h \
    common/usb-drive-monitor.h \
    common/utilui.h \
    common/versioncompatibility.h \
    common/websocket.h \
    dialoglanguage.h \
    engineermode/winunittest.h \
    external-data-intf/data-intf-an-hui-screen.h \
    external-data-intf/data-intf-other.h \
    form-dev-activate.h \
    global_intf.h \
    global_obj.h \
    external-data-intf/data-intf-guanxin.h \
    external-data-intf/data-intf-huayi.h \
    maintenance/dev-codes-need-rising-trigger.h \
    maintenance/maintenance.h \
    measure/exposure-adjuster.h \
    mywidgets/basewindow.h \
    mywidgets/combobox.h \
    mywidgets/data.h \
    mywidgets/doublespinbox.h \
    mywidgets/modalwin.h \
    mywidgets/mpro-wx-svc-qr-code.h \
    mywidgets/report.h \
    mywidgets/screener-report-receipt.h \
    mywidgets/suspensionpromptbox.h \
    algo/algo.h \
    algo/algo_utils.h \
    algo/pupil_pair_onnx_detector.h \
    algo/pupil_pair_rknn_detector.h \
    algo/pupil_light_tracker.h \
    algo/pupil_cross_round_tracker.h \
    algo/algointf.h \
    algo/curvedata.h \
    algo/ransac.h \
    algo/refractionstrategy.h \
    algo/cascadepool.h \
    algo/perftimer.h \
    algo/haar_thread_safe.h \
    #algo/algorithmthread.h \
    #algo/runtask.h \
    hardware.h \
    mywidgets/cserialport.h \
    mywidgets/waitingmovie.h \
    mywidgets/widget-loading.h \
    mywidgets/widget-optical-type-options.h \
    mywidgets/winlog.h \
    mywidgets/myeditline.h \
    common/keyboardreader.h \
    common/printer_a4/cups-intf.h \
    common/printer_a4/print-intf.h \
    common/landevfinder/landevfinderclient.h \
    common/landevfinder/landevfinderclientqt.h \
    common/landevfinder/landevfinderdefs.h \
    common/util-common.h \
    common/logger/logger.h \
    common/logger/log-appender.h \
    common/logger/log-layout.h \
    common/nettools.h \
    global.h \
    mainwindow.h \
    tool.h \
    result.h \
    util-app.h \
    external-data-intf/win-guanxin-testee-edit.h \
    external-data-intf/win-guanxin-testee-query.h \
    winclinic.h \
    windiagnosissuggestion.h \
    windiagnosticstandard.h \
    winmeasure.h \
    measure/measurectrl.h \
    measure/capturethread.h \
    algo-invoker.h \
    mysqlite.h \
    mysqlitepatients.h \
    eyesightstandard.h \
    personalinfos.h \
    camerainit.h \
    windowsmanager.h \
    winmanage.h \
    GraphUtils.h \
    import.h \
    threadmodel.h \
    myserialport.h \
    datepage.h\
    statusbarform.h \
    mydialog.h \
    noticewin.h \
    blockmousepress.h \
    messagewin.h \
    circlshowthread.h \
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
    wifibt/wifiintf.h \
    globelwireless.h \
    DataTransTest/DataTransmit.h \
    windatatrans.h \
    DataTransTest/mongoose-6.7/mongoose.h \
    uploadthread.h \
    printersetting.h \
    printertransmit.h \
    enhancementimag.h \
    qrcodeinput.h \
    #batchuploadlist.h \
    tablemap.h \
    batterymonitor.h \
    musicsetting.h \
    progresswindow.h \
    updatedialog.h \
    pipewrite.h \
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
    appsetting.h \
    engineermode/shellsimulate.h \
    distancedetect.h \
    mywidgets/mylabel.h \
    mywidgets/measurestatview.h \
    mywidgets/baseform.h \
    mywidgets/eyelimitmark.h \
    serialdatatrans.h \
    distcalibration.h \
    #lampcalibrate.h \
    CameraIntf.h \
    winmultiresults.h \
    winpersonalrecord.h \
    winscreen.h \
    winupdateprogress.h \
    winupdatesetup.h

FORMS   += \
    dialoglanguage.ui \
    engineermode/winunittest.ui \
    form-dev-activate.ui \
    import.ui \
    datepage.ui\
    mydialog.ui \
    mywidgets/mpro-wx-svc-qr-code.ui \
    mywidgets/widget-optical-type-options.ui \
    mywidgets/winlog.ui \
    noticewin.ui \
    blockmousepress.ui \
    messagewin.ui \
    qrcodeinput.ui \
    #batchuploadlist.ui \
    updatedialog.ui \
    result.ui \
    musicsetting.ui \
    printersetting.ui \
    tool.ui \
    mainwindow.ui \
    themebackground.ui \
    eyesightstandard.ui \
    external-data-intf/win-guanxin-testee-edit.ui \
    external-data-intf/win-guanxin-testee-query.ui \
    winclinic.ui \
    windatatrans.ui \
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
    engineermode/engineerpassword.ui \
    previewimage.ui \
    windiagnosissuggestion.ui \
    windiagnosticstandard.ui \
    winmeasure.ui \
    engineermode/engineermode.ui \
    keyboard/keyboard.ui \
    progresswindow.ui \
    dialoglistselect.ui \
    engineermode/shellsimulate.ui \
    #lampcalibrate.ui \
    winmultiresults.ui \
    winpersonalrecord.ui \
    winscreen.ui \
    winupdateprogress.ui \
    winupdatesetup.ui

RESOURCES += \
    resources.qrc

#
INCLUDEPATH += $$PWD/../include

INCLUDEPATH += $$PWD/mywidgets
INCLUDEPATH += $$PWD/settings
INCLUDEPATH += $$PWD/wifibt
INCLUDEPATH += $$PWD/keyboard
INCLUDEPATH += $$PWD/algo
INCLUDEPATH += $$PWD/common
INCLUDEPATH += $$PWD/common/remote-service
INCLUDEPATH += $$PWD/common/landevfinder
INCLUDEPATH += $$PWD/common/printer_a4
INCLUDEPATH += $$PWD/common/logger
INCLUDEPATH += $$PWD/DataTransTest
INCLUDEPATH += $$PWD/external-data-intf
INCLUDEPATH += $$PWD/system
INCLUDEPATH += $$PWD/maintenance
INCLUDEPATH += $$PWD/measure

INCLUDEPATH += /opt/Qt5.14.2/5.14.2/gcc_64/include

INCLUDEPATH += $$PWD/../include/QtXlsx
LIBS += -L$$PWD/../../lib/Qt5Xlsx/arm64 -lQt5Xlsx

#
DEPENDPATH += /opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/usr/lib
#DEPENDPATH += /opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/usr/lib/pkgconfig

#INCLUDEPATH += $$PWD/../include/opencv_2.4
INCLUDEPATH += $$PWD/../include/opencv_3.4.12

#message($$PWD/../../lib/opencv/arm64)
QMAKE_RPATHLINKDIR += $$PWD/../../lib/opencv_3.4.12/arm64

#LIBS += -L$$PWD/../../lib/opencv/arm64 \
LIBS += -L$$PWD/../../lib/opencv_3.4.12/arm64 \
    -lopencv_dnn \
    -lopencv_highgui \
    -lopencv_imgproc \
    -lopencv_imgcodecs \
    -lopencv_ml \
    -lopencv_objdetect \
    -lopencv_photo \
    -lopencv_stitching \
    -lopencv_superres \
    -lopencv_features2d \
    -lopencv_calib3d \
    -lopencv_flann \
    -lopencv_video \
    -lopencv_core

# C800在RK3568上优先使用RKNN/NPU；OpenCV仍保持3.4.12，ONNX CPU
# 后端作为运行期安全回退。librknnrt.so部署到/root/screener-libs。
DEFINES += ENABLE_RKNN_C800=1
INCLUDEPATH += $$PWD/../../lib/rknn_runtime_2.3.2/include
LIBS += -L$$PWD/../../lib/rknn_runtime_2.3.2/arm64 -lrknnrt
QMAKE_RPATHLINKDIR += $$PWD/../../lib/rknn_runtime_2.3.2/arm64

# 板端 protobuf 运行库位于固定目录，避免依赖系统全局搜索路径。
QMAKE_RPATHDIR += /root/screener-libs

PKGCONFIG += openssl
INCLUDEPATH += $$PWD/../include/openssl_1.1
LIBS += -L/opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/usr/lib -lssl -lcrypto

#
LIBS += -L/opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/lib -lzbar

# 操作系统类型（1:i.MX6Q, 2:PC-Linux, 3:rk3568）
DEFINES += OS_TYPE=3

#HEADERS += testdesktop.h
#SOURCES += testdesktop.cpp

# 是否测试模式
#DEFINES += TEST_MODE

# 屏幕尺寸类型（1:800x480, 2:1280x720）
DEFINES += SCREEN_SIZE_TYPE=1

# QPA 平台插件类型（0:unknow, 1:eglfs, 2:wayland, 3:PC）
DEFINES += QPA_PLATFORM_TYPE=2
# 是否启用 FrameBuffer
#DEFINES += SURPORT_FRAME_BUFFER

# 蓝牙类型（1:JDY-34, 2:rk3568）
#DEFINES += BLUETOOTH_TYPE=1
#HEADERS += wifibt/bluetoothserial.h \
#           wifibt/bluetoothintf.h \
#           wifibt/winbluetooth.h
#SOURCES += wifibt/bluetoothserial.cpp \
#           wifibt/bluetoothintf.cpp \
#           wifibt/winbluetooth.cpp
#FORMS   += wifibt/winbluetooth.ui

DEFINES += BLUETOOTH_TYPE=2
INCLUDEPATH += $$PWD/../include/RkWifiBt
HEADERS += wifibt/bluetoothrk.h \
           wifibt/bluetoothintf.h \
           wifibt/winbluetooth.h
SOURCES += wifibt/bluetoothrk.cpp \
           wifibt/bluetoothintf.cpp \
           wifibt/winbluetooth.cpp
FORMS   += wifibt/winbluetooth.ui

LIBS += -L$$PWD/../../lib/RkWifiBt/arm64 -lrkwifibt

# WiFi 类型（1:i.MX6Q, 2:rk3568）
#DEFINES += WIFI_TYPE=1
#INCLUDEPATH += $$PWD/wifibt/wpa
#HEADERS += wifibt/wifiwpa.h \
#           wifibt/wpa/build_config.h \
#           wifibt/wpa/includes.h \
#           wifibt/wpa/wpa_ctrl.h \
#           wifibt/wpa/wpacommit.h
#SOURCES += wifibt/wifiwpa.cpp \
#           wifibt/wpa/wpa_ctrl.c \
#           wifibt/wpa/wpacommit.cpp \
#           wifibt/wpa/os_unix.c

#DEFINES += CONFIG_CTRL_IFACE
#DEFINES += CONFIG_CTRL_IFACE_UNIX
DEFINES += CONFIG_IOCTL_CFG80211

#
DEFINES += WIFI_TYPE=2
HEADERS += wifibt/wifirk.h \
           wifibt/qtwifi.h
SOURCES += wifibt/wifirk.cpp \
           wifibt/qtwifi.cpp

LIBS += -L/opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/usr/lib64 -lwpa_client

# 是否测试 RK WiFi Bt
#DEFINES += TEST_RKWIFIBT
#INCLUDEPATH += $$PWD/../../unit-test/test-wifibt
#HEADERS += ../../unit-test/test-wifibt/wintestrkwifibt.h
#SOURCES += ../../unit-test/test-wifibt/wintestrkwifibt.cpp
#FORMS   += ../../unit-test/test-wifibt/wintestrkwifibt.ui

# 相机类型（1:迈德威视, 2:度申）
#DEFINES += CAMERA_TYPE=1

#INCLUDEPATH += $$PWD/../include/Camera_MindVision_2.1.0.20
##INCLUDEPATH += $$PWD/../include/Camera_MindVision_2.1.0.32

##DEFINES += CAMERA_MINDVISION_SDK_NEW

#LIBS += -L$$PWD/../../lib/MVSDK_2.1.0.20/arm64 -lMVSDK
##LIBS += -L$$PWD/../../lib/MVSDK_2.1.0.32/arm64 -lMVSDK

#
DEFINES += CAMERA_TYPE=2
INCLUDEPATH += $$PWD/../include/Camera_Do3Think_2.22.40
LIBS += -L$$PWD/../../lib/Do3Think_2.22.40/arm64 -ldvp -lhzd

# 是否使用 “dwIme”
#DEFINES += USE_DWIME
#LIBS += -L$$PWD/../../lib -ldwIme

# 是否使用 GOOGLEPINYIN
DEFINES += USE_GOOGLEPINYIN
LIBS += -L$$PWD/../../lib/googlepinyin/arm64 -lgooglepinyin

## 是否使用 ZINNIA 手写
#DEFINES += USE_ZINNIA
#LIBS += -L$$PWD/../../lib/zinnia/arm64 -lzinnia

## 是否使用 WAGOMU 手写
#DEFINES += USE_WAGOMU
#LIBS += -L$$PWD/../../lib/wagomu/arm64 -lwagomu

# 是否使用 多文手写
DEFINES += USE_DWHW
INCLUDEPATH += $$PWD/../include/DWIMECore
LIBS += -L$$PWD/../../lib/DWIMECore/aarch64 -lDWIMECore
HEADERS += keyboard/DWIMECore_Dll.h
SOURCES += keyboard/DWIMECore_Dll.cpp

# <glib.h> 的路径
INCLUDEPATH += /opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/usr/include/glib-2.0
# <glibconfig.h> 的路径
INCLUDEPATH += /opt/industio/evb3568-qt5.14-host-sdk/aarch64-buildroot-linux-gnu/sysroot/usr/lib/glib-2.0/include

#
#DISTFILES += update

#
DEFINES += QS_HAS_JSON

# 翻译文件
TRANSLATIONS += $$PWD/resource/language/english.ts
TRANSLATIONS += $$PWD/resource/language/german.ts

# 定义编译号自增函数
defineReplace(getBuildVer) {
  FILE_NAME = build_ver_rk3568.txt
  LINES = $$cat($$PWD/../$$FILE_NAME, lines)
  #message( LINES = $$LINES )
  VER = $$take_first(LINES)
  isEmpty(VER) {
    VER = 0
  }
  VER = $$num_add($$VER, 1)     #编译号加1，并返回为新编译号
  #message( new VER = $$VER )
  write_file($$PWD/../$$FILE_NAME, VER)
  return ($$VER)
}

#调用编译号自增函数（注意：代码里用到 BUILD_VER 宏的模块的 .o 文件须重新编译，它的值才是最新的）
!build_pass {
  build_ver = $$getBuildVer()
  message(BUILD_VER = $$build_ver )
  DEFINES += BUILD_VER=$$build_ver
}

