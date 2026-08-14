//二维码扫描识别
#include "BarcodeDecoder.h"

//using namespace std;
//using namespace cv;
//using namespace zbar;

//
BarcodeDecoder::BarcodeDecoder()
{
    scanner_.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);
    //scanner_.set_config(zbar::ZBAR_QRCODE, zbar::ZBAR_CFG_ENABLE, 1);	
}

BarcodeDecoder::~BarcodeDecoder()
{

}

BarcodeDecoderError BarcodeDecoder::ScanImage(IplImage & _image, std::string & _code_str, std::string & _type_name)
{
//    qDebug()<<"enter barcodeDecoder()";
    BarcodeDecoderError result;

    //

    _code_str.clear();
    _type_name.clear();


//    qDebug()<<"before cvarrToMat";
//    Mat image_gray = cv::cvarrToMat(&_image);
//    if (!image_gray.data)
//    {
//        qDebug()<<" Mat image =NULL";
//        result = eBDErr_ImageDataError;
//        return result;
//    }
    qDebug()<<"before cvtcolor";

    int width = _image.width;
    int height = _image.height;
    uchar *raw = (uchar *)_image.imageData;


//    qDebug()<<"before scan";
    zbar::Image image_zbar(width, height, "Y800", raw, width * height);
    scanner_.scan(image_zbar);
//    qDebug()<<"after scan barcodeDecoder()";

    zbar::Image::SymbolIterator symbol = image_zbar.symbol_begin();
    if (image_zbar.symbol_end() == symbol)
    {
        result = eBDErr_Faild;
    }
    else
    {
        _code_str = symbol->get_data();

        if (NULL != &_type_name)
            _type_name = symbol->get_type_name();

        result = eBDErr_Succeed;
    }
    //for (; symbol != imageZbar.symbol_end(); ++symbol)	
    //{
    //}
//    qDebug()<<" after get_data";

    image_zbar.set_data(NULL, 0);

    return result;
}

BarcodeDecoderError BarcodeDecoder::ScanImage(std::string & _file_path, std::string & _code_str, std::string & _type_name)
{
    // TODO:

    //
    return eBDErr_Faild;
}
