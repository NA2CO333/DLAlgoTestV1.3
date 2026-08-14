#-------------------------------------------------
#
# Project created by QtCreator 2021-05-31T13:42:39
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = $$PWD/../bin/test-algo_desktop
TEMPLATE = app
# 使 GCC 生成 executable 格式而不是 shared object 格式的文件
QMAKE_LFLAGS += -no-pie

#QMAKE_CXXFLAGS += -std=c++0x
CONFIG += C11
CONFIG += C++11

#CONFIG += sanitize_thread
#sanitize_thread {
#    QMAKE_CXXFLAGS += -fsanitize=thread -fPIE
#    QMAKE_LFLAGS += -fsanitize=thread -pie
#}

# 是否运行在单元测试程序中
DEFINES += UNIT_TEST
# 是否测试模式
DEFINES += TEST_MODE

# 是否使用单线程计算
DEFINES += SINGLE_THREAD_CALC

#程序版本（转为整型，次版本号和修订版本号都占2位）
DEFINES += APP_VER=10500

# 操作系统类型（1:i.MX6Q, 2:PC-Linux, 3:rk3568）
DEFINES += OS_TYPE=2

# 屏幕尺寸类型（1:800x480, 2:1280x720）
DEFINES += SCREEN_SIZE_TYPE=2

# 是否特殊的图片尺寸（如算法工程师给的图片）
#DEFINES += SPECIAL_ROI_SIZE

# OpenCV 的外部依赖：TBB 库
LIBS += -L$$PWD/../../lib/TBB/x86_64 -ltbb
LIBS += -L$$PWD/../../lib/BLAS/x86_64 -lblas
LIBS += -L$$PWD/../../lib/GNU-Fortran/x86_64 -lgfortran
LIBS += -L$$PWD/../../lib/LAPACK/x86_64 -llapack
LIBS += -L$$PWD/../../lib/ARPACK/x86_64 -larpack
LIBS += -L$$PWD/../../lib/SuperLU/x86_64 -lsuperlu
LIBS += -L$$PWD/../../lib/Armadillo/x86_64 -larmadillo
LIBS += -L$$PWD/../../lib/PROJ/x86_64 -lproj
LIBS += -L$$PWD/../../lib/Poppler/x86_64 -lpoppler
LIBS += -L$$PWD/../../lib/FreeXL/x86_64 -lfreexl
LIBS += -L$$PWD/../../lib/Qhull/x86_64 -lqhull
LIBS += -L$$PWD/../../lib/GEOS/x86_64 -lgeos_c -lgeos-3.6.2
LIBS += -L$$PWD/../../lib/Epsilon/x86_64 -lepsilon
LIBS += -L$$PWD/../../lib/ODBC/x86_64 -lodbc -lodbccr -lodbcinst
LIBS += -L$$PWD/../../lib/MiniZip/x86_64 -lminizip
LIBS += -L$$PWD/../../lib/URI-Parser/x86_64 -luriparser
LIBS += -L$$PWD/../../lib/KML/x86_64 -lkmlbase -lkmldom -lkmlengine
LIBS += -L$$PWD/../../lib/ICU/x86_64 -licuuc -licudata -licui18n -licuio -liculx -licutest -licutu
LIBS += -L$$PWD/../../lib/Xerces-C++/x86_64 -lxerces-c-3.2
LIBS += -L$$PWD/../../lib/NetCDF/x86_64 -lnetcdf -lnetcdf_c++
LIBS += -L$$PWD/../../lib/AEC/x86_64 -laec
LIBS += -L$$PWD/../../lib/SZIP/x86_64 -lsz
LIBS += -L$$PWD/../../lib/HDF/x86_64 -lhdf5_serial -lmfhdfalt -ldfalt -lhdf5_serial_hl -ldfalt -lhdf5_openmpi_fortran -lhdf5_openmpihl_fortran -lhdf5_openmpi_hl -lhdf5_openmpi -lhdf5_serial_fortran -lhdf5_serialhl_fortran -lhdf5_serial -lmfhdfalt
LIBS += -L$$PWD/../../lib/OGDI/x86_64 -logdi
LIBS += -L$$PWD/../../lib/GeoTIFF/x86_64 -lgeotiff
LIBS += -L$$PWD/../../lib/PostgreSQL/x86_64 -lpq
LIBS += -L$$PWD/../../lib/DAP/x86_64 -ldapclient -ldap
LIBS += -L$$PWD/../../lib/Spatialite/x86_64 -lspatialite
LIBS += -L$$PWD/../../lib/Fuyun/x86_64 -lfyba -lfyut -lfygm
LIBS += -L$$PWD/../../lib/MySQL/x86_64 -lmysqlclient
LIBS += -L$$PWD/../../lib/GDAL/x86_64 -lgdal
LIBS += -L$$PWD/../../lib/CharLS/x86_64 -lCharLS
LIBS += -L$$PWD/../../lib/JSON-C/x86_64 -ljson-c
LIBS += -L$$PWD/../../lib/GDCM/x86_64 -lgdcmMSFF -lgdcmCommon -lgdcmDICT -lgdcmDSED -lgdcmIOD -lgdcmjpeg12 -lgdcmjpeg16 -lgdcmjpeg8 -lgdcmMEXD
LIBS += -L$$PWD/../../lib/OpenExr/x86_64 -lHalf -lIex -lIlmThread -lIlmImfUtil -lIlmImf
LIBS += -L$$PWD/../../lib/JPEG/x86_64 -ljpeg

#INCLUDEPATH += $$PWD/../../srcs/include/opencv_2.4
INCLUDEPATH += $$PWD/../../srcs/include/opencv_3.4.12

#message($$PWD/../../lib/opencv/arm64)
QMAKE_RPATHLINKDIR += $$PWD/../../lib/opencv_3.4.12/x86_64

#LIBS += -L$$PWD/../../lib/opencv/x86_64gcc5.5 \
LIBS += -L$$PWD/../../lib/opencv_3.4.12/x86_64 \
    -lopencv_core \
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
    -lopencv_flann

# 相机类型（1:迈德威视, 2:度申）        # NOTE: 用于确定照片分辨率       # TODO: 清掉
DEFINES += CAMERA_TYPE=2

#
INCLUDEPATH += $$PWD/../../srcs/include

INCLUDEPATH += $$PWD/../../srcs/mainbasic
INCLUDEPATH += $$PWD/../../srcs/mainbasic/algo
INCLUDEPATH += $$PWD/../../srcs/mainbasic/common

#
SOURCES += main.cpp \
    algo-invoker.cpp \
    mainwindow.cpp \
    ../../srcs/mainbasic/algo/algointf.cpp \
    ../../srcs/mainbasic/algo/algo.cpp \
    ../../srcs/mainbasic/algo/refractionstrategy.cpp \
    ../../srcs/mainbasic/algo/cascadepool.cpp \
    ../../srcs/mainbasic/algo/ransac.cpp \
    ../../srcs/mainbasic/common/util-common.cpp \
    ../../srcs/mainbasic/common/logger.cpp \
    formmsgviewer.cpp

HEADERS  += mainwindow.h \
    ../../srcs/mainbasic/algo/algointf.h \
    ../../srcs/mainbasic/algo/algo.h \
    ../../srcs/mainbasic/algo/refractionstrategy.h \
    ../../srcs/mainbasic/algo/cascadepool.h \
    ../../srcs/mainbasic/algo/ransac.h \
    ../../srcs/mainbasic/common/util-common.h \
    ../../srcs/mainbasic/common/logger.h \
    algo-invoker.h \
    formmsgviewer.h

FORMS    += mainwindow.ui \
    formmsgviewer.ui

#
#if APP_VER > 10309
HEADERS += $$PWD/../../srcs/mainbasic/appsetting.h
SOURCES += $$PWD/../../srcs/mainbasic/appsetting.cpp
#endif
