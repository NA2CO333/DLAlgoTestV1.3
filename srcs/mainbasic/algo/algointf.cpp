#include "algointf.h"

#include <iostream>

#include "algo.h"
#include "perftimer.h"

// 类型转换（前置声明）
CvRect CRect_to_CvRect(const CRect &_c_rect);
CRect CvRect_to_CRect(const CvRect &_cv_rect);
CvPoint2D32f CPointF_to_CvPoint2D32f(CPointF _c_point);
CPointF CvPoint2D32f_to_CPointF(CvPoint2D32f _cv_point);

//
const std::string CAlgoIntf::rootDirPath = "/media";
const std::string CAlgoIntf::imageDirPath = rootDirPath + "/photo";

//
const char *enumToCaption_AlgoVer(enAlgoVer _ver)
{
    switch (_ver) {
    case enAlgoVer::algoVer_2022_12         : return "Ver_2022_12";
    }
    return "???";
}

//
stVisionValue addVisionValue(const stVisionValue &_a, const stVisionValue &_b)
{
    stVisionValue vision;

    vision.RSph     =            (_a.RSph       +  _b.RSph      ) / 2.0 ;
    vision.RCyl     =            (_a.RCyl       +  _b.RCyl      ) / 2.0 ;
    vision.RAx      = std::round((_a.RAx        +  _b.RAx       ) / 2.0);
    vision.RPs      =            (_a.RPs        +  _b.RPs       ) / 2.0 ;
    vision.RHz      = std::round((_a.RHz        +  _b.RHz       ) / 2.0);
    vision.RVz      = std::round((_a.RVz        +  _b.RVz       ) / 2.0);
    vision.RPtosis  =            (_a.RPtosis    || _b.RPtosis   )       ;

    vision.PD       = std::round((_a.PD         +  _b.PD        ) / 2.0);

    vision.LSph     =            (_a.LSph       +  _b.LSph      ) / 2.0 ;
    vision.LCyl     =            (_a.LCyl       +  _b.LCyl      ) / 2.0 ;
    vision.LAx      = std::round((_a.LAx        +  _b.LAx       ) / 2.0);
    vision.LPs      =            (_a.LPs        +  _b.LPs       ) / 2.0 ;
    vision.LHz      = std::round((_a.LHz        +  _b.LHz       ) / 2.0);
    vision.LVz      = std::round((_a.LVz        +  _b.LVz       ) / 2.0);
    vision.LPtosis  =            (_a.LPtosis    || _b.LPtosis   )       ;

    return vision;
}

stVisionAbnormal addVisionAbnormal(const stVisionAbnormal &_a, const stVisionAbnormal &_b)
{
    stVisionAbnormal abnormal;

    abnormal.LAxisUntrusted = _a.LAxisUntrusted || _b.LAxisUntrusted    ;
    abnormal.RAxisUntrusted = _a.RAxisUntrusted || _b.RAxisUntrusted    ;
    abnormal.LSphTooLarge   = _a.LSphTooLarge   || _b.LSphTooLarge      ;
    abnormal.RSphTooLarge   = _a.RSphTooLarge   || _b.RSphTooLarge      ;
    abnormal.LCylTooLarge   = _a.LCylTooLarge   || _b.LCylTooLarge      ;
    abnormal.RCylTooLarge   = _a.RCylTooLarge   || _b.RCylTooLarge      ;
    abnormal.LCylUntrusted  = _a.LCylUntrusted  || _b.LCylUntrusted     ;
    abnormal.RCylUntrusted  = _a.RCylUntrusted  || _b.RCylUntrusted     ;

    return abnormal;
}

/**
 * @brief 根据传入的参数创建实例
 * @param _camera_type
 * @param _algo_ver
 * @return
 */
CAlgoIntf *CAlgoIntf::createInstance(enCameraType _camera_type, enAlgoVer _algo_ver)
{
    // 参数检查
    if ( !((_algo_ver >= algoVer_Min && _algo_ver <= algoVer_Max) && (_camera_type >= cameraType_Min && _camera_type <= cameraType_Max)) ) {
        return nullptr;
    }

    //
    if (algoVer_2022_12 == _algo_ver) {
        CAlgo *intf = new CAlgo;
        switch (_camera_type) {
        case cameraType_D3T_M3ST130M:
            //intf-> = ;

            return intf;
            break;
        default:
            std::cout << "error camera type!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
            break;
        }
    }

    //
    return nullptr;
}

const std::string &CAlgoIntf::getRootDirPath()
{
    return rootDirPath;
}

const std::string &CAlgoIntf::getImageDirPath()
{
    return imageDirPath;
}

// 类型转换（实现）
CvRect CRect_to_CvRect(const CRect &_c_rect)
{
    return cvRect(_c_rect.x, _c_rect.y, _c_rect.width, _c_rect.height);
}

CRect CvRect_to_CRect(const CvRect &_cv_rect)
{
    CRect rect;
    rect.x = _cv_rect.x;
    rect.y = _cv_rect.y;
    rect.width = _cv_rect.width;
    rect.height = _cv_rect.height;
    return rect;
}

CvPoint2D32f CPointF_to_CvPoint2D32f(CPointF _c_point)
{
    return cvPoint2D32f(_c_point.x, _c_point.y);
}

CPointF CvPoint2D32f_to_CPointF(CvPoint2D32f _cv_point)
{
    CPointF point;
    point.x = _cv_point.x;
    point.y = _cv_point.y;
    return point;
}
