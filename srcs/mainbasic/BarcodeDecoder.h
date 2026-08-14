#ifndef BARCODEDECODER_H
#define BARCODEDECODER_H

#include <iostream>

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <QDebug>

#include "zbar.h"

//using namespace std;    /* 最好不要在头文件里添加 using namespace ？ */

// 条码扫描功能封装类。
enum BarcodeDecoderError
{
    eBDErr_Succeed = 0,		// 识别成功
    eBDErr_Faild,			// 识别失败
    eBDErr_ImageDataError,	// 图像数据错误
    eBDErr_FileNotFound,	// 文件不存在
};

// 条码扫描功能封装类。
class BarcodeDecoder
{
public:
    BarcodeDecoder();
    ~BarcodeDecoder();

    BarcodeDecoderError ScanImage(std::string &_file_path, std::string &_code_str, std::string &_type_name);
    BarcodeDecoderError ScanImage(IplImage &_image, std::string &_code_str, std::string &_type_name);

private:
    zbar::ImageScanner scanner_;
};

#endif
