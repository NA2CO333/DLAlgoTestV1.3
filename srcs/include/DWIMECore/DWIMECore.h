#ifndef DWIMECORE_H
#define DWIMECORE_H

#include "DWIMECore_Def.h"

#ifndef DWIMELIB_API
#define DWIMELIB_API    extern
#endif


#ifdef __cplusplus
extern "C" {
#endif


// -------------------------------------------------------------------------------------
// 回调函数定义 
// -------------------------------------------------------------------------------------
typedef void (*DWIMECore_Callback)(int evnet, void * userData, void * param);



// ------------------------------
// 接口定义
// ------------------------------
DWIMELIB_API const unsigned short * DWIMECore_getLicense(unsigned short * buffer);

DWIMELIB_API DWVersion              DWIMECore_getEngineVersion();
DWIMELIB_API int                    DWIMECore_getMinorVersion();
DWIMELIB_API DWError                DWIMECore_appBinding(const unsigned short * secretKey);

DWIMELIB_API DWError                DWIMECore_dataAdd(int kind, void *  data, int size);
DWIMELIB_API DWError                DWIMECore_dataMapA(int kind, const char * fileName);
DWIMELIB_API DWError                DWIMECore_dataMap(int kind, const unsigned short * fileName);
DWIMELIB_API DWError                DWIMECore_dataFileA(int kind, const char * fileName);
DWIMELIB_API DWError                DWIMECore_dataFile(int kind, const unsigned short * fileName);
DWIMELIB_API DWError                DWIMECore_dataClear();
DWIMELIB_API DWBool                 DWIMECore_dataExist(int kind);

DWIMELIB_API DWError                DWIMECore_hwInit(int param);
DWIMELIB_API DWError                DWIMECore_hwDeinit();
DWIMELIB_API DWError                DWIMECore_hwReset();
DWIMELIB_API DWError                DWIMECore_hwSetOption(int option, int value);
DWIMELIB_API int                    DWIMECore_hwRecognize(const short * tracks, int tracksCount);

DWIMELIB_API DWLanguage             DWIMECore_getLanguage();
DWIMELIB_API DWInputMode            DWIMECore_getInputMode();
DWIMELIB_API DWKBType               DWIMECore_getKBType();

DWIMELIB_API DWError                DWIMECore_init(int lang, int kb, int im);
DWIMELIB_API DWError                DWIMECore_deinit();
DWIMELIB_API DWError                DWIMECore_reset();
DWIMELIB_API DWError                DWIMECore_setOption(int option, int value);
DWIMELIB_API DWError                DWIMECore_processKey(int uKey, DWKeyStatus uKeyStatus, int param);
DWIMELIB_API DWError                DWIMECore_processKeys(const unsigned short *  uKeys, int param);
DWIMELIB_API int                    DWIMECore_associateKey(const unsigned short * uKeys); // 不再对外使用，调用setCandSelect 会自动生成联想
DWIMELIB_API int                    DWIMECore_getCandCount();
DWIMELIB_API int                    DWIMECore_getSyllableCount();
DWIMELIB_API int                    DWIMECore_getSyllableSelectedInfo();
DWIMELIB_API int                    DWIMECore_getCompCorrectCount(int candIdex);
DWIMELIB_API int                    DWIMECore_getCompCorrectInfo(int candIdx, int correctIdx);

DWIMELIB_API const unsigned short * DWIMECore_getInputString(unsigned short * out);
DWIMELIB_API const unsigned short * DWIMECore_getCompString(unsigned short * out);
DWIMELIB_API const unsigned short * DWIMECore_getFormatPinyin(int index, unsigned short * out);
DWIMELIB_API const unsigned short * DWIMECore_getCandString(int index, unsigned short * out);
DWIMELIB_API const unsigned short * DWIMECore_getSyllableString(int index, unsigned short * out);
DWIMELIB_API const unsigned short * DWIMECore_getCommitString(unsigned short * out);
DWIMELIB_API const unsigned short * DWIMECore_getCandCode(int index, unsigned short * out);
//DWIMELIB_API const unsigned short * DWIMECore_getCompCorrectPinyin(int candIdex, int correctIdx, unsigned short * outBuffer); 不使用

DWIMELIB_API DWBool                 DWIMECore_isCanCommit();
DWIMELIB_API DWBool                 DWIMECore_isAssociateMode();
DWIMELIB_API DWBool                 DWIMECore_hasInput();
DWIMELIB_API DWError                DWIMECore_setCandSelect(int index);
DWIMELIB_API DWError                DWIMECore_setSyllableSelect(int index);
DWIMELIB_API DWError                DWIMECore_setPYFuzzy(int fuzzy);
DWIMELIB_API DWError                DWIMECore_customPYFuzzy(const unsigned short * fuzzyFirst[], const unsigned short * fuzzySecond[], int count);
DWIMELIB_API DWError                DWIMECore_setCorrect(int open);
DWIMELIB_API DWError                DWIMECore_addCorrect(const unsigned short * py1, const unsigned short * py2);
DWIMELIB_API DWError                DWIMECore_setCallback(DWIMECore_Callback func, void * userData);
DWIMELIB_API int                    DWIMECore_getCandAttr(int candIndex, int attrType);

DWIMELIB_API void *                 DWIMECore_userDBCopy(int * outSize);
DWIMELIB_API void                   DWIMECore_userDBFree(void * data);
DWIMELIB_API int                    DWIMECore_userDBImportContacts(const unsigned short * names, unsigned short sepChar);
DWIMELIB_API const unsigned short * DWIMECore_userDBGetPhr(int dbType, int index, unsigned short * out);
DWIMELIB_API const unsigned short * DWIMECore_userDBGetCode(int dbType, int index, unsigned short * out);
DWIMELIB_API DWError                DWIMECore_userDBDelPhr(int dbType, int index);
DWIMELIB_API DWError                DWIMECore_userDBDelPhr2(int dbType, const unsigned short *  inPhr);
DWIMELIB_API DWError                DWIMECore_userDBAdd(int dbType, const unsigned short * inPhr, const unsigned short * inPys);
DWIMELIB_API int                    DWIMECore_userDBGetCount(int dbType);
DWIMELIB_API int                    DWIMECore_userDBGetAttr(int dbType, int index, int attrType);
DWIMELIB_API DWBool                 DWIMECore_userDBIsNewPhr(int dbType, int index);
DWIMELIB_API DWBool                 DWIMECore_userDBItemExisted(int dbType, const unsigned short * inPhr, const unsigned short * inPys);



#ifdef __cplusplus
}
#endif

#endif 
