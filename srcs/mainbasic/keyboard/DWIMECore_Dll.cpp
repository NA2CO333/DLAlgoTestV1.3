/**********************************************************************************************************
 * 这个 cpp 只是为了代替 DWIMECore.lib, 因为 DWIMECore.lib 文件有兼容性问题无法通用。
 **********************************************************************************************************/

#if defined(ANDROID)
    #pragma message("***** Android *****")
    #define __ISANDROID         1

#elif defined(TARGET_OS_MAC) || defined(__APPLE__)
    #pragma message("*****   iOS   *****")
    #define __ISIOS             1

#elif defined(UNIX) || defined(__linux__) || defined(__LINUX__)
    #pragma message("*****  LINUX  *****")
    #define __ISLINUX           1
    
    #define DWIMEAPI
    typedef void*           HINSTANCE;

    #include <string.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <dlfcn.h>
    #include <unistd.h>  //包含了Linux C 中的函数getcwd()

#elif defined(_WIN32) || defined(_WINDOWS)
    #pragma message("***** Windows *****")
    #define __ISWIN             1

    #include <windows.h>
    #include <stdio.h>
    #define DWIMEAPI

    extern "C" IMAGE_DOS_HEADER __ImageBase;//申明为全局变量

#endif
#include "DWIMECore_Dll.h"


//  API PTR 定义 
typedef const unsigned short *  (DWIMEAPI *DWIMECore_getLicenseF)(unsigned short * buffer);

typedef DWVersion               (DWIMEAPI *DWIMECore_getEngineVersionF)();
typedef int                     (DWIMEAPI *DWIMECore_getMinorVersionF)();
typedef DWError                 (DWIMEAPI *DWIMECore_appBindingF)(const unsigned short * secretKey);

typedef DWError                 (DWIMEAPI *DWIMECore_dataAddF)(int kind, void *  data, int size);
typedef DWError                 (DWIMEAPI *DWIMECore_dataMapF)(int kind, const unsigned short * fileName);
typedef DWError                 (DWIMEAPI *DWIMECore_dataMapAF)(int kind, const char * fileName);
typedef DWError                 (DWIMEAPI *DWIMECore_dataFileF)(int kind, const unsigned short * fileName);
typedef DWError                 (DWIMEAPI *DWIMECore_dataFileAF)(int kind, const char * fileName);
typedef DWError                 (DWIMEAPI *DWIMECore_dataClearF)();
typedef DWBool                  (DWIMEAPI *DWIMECore_dataExistF)(int kind);

typedef DWLanguage              (DWIMEAPI *DWIMECore_getLanguageF)();
typedef DWInputMode             (DWIMEAPI *DWIMECore_getInputModeF)();
typedef DWKBType                (DWIMEAPI *DWIMECore_getKBTypeF)();

typedef DWError                 (DWIMEAPI *DWIMECore_hwInitF)(int param);
typedef DWError                 (DWIMEAPI *DWIMECore_hwDeinitF)();
typedef DWError                 (DWIMEAPI *DWIMECore_hwResetF)();
typedef DWError                 (DWIMEAPI *DWIMECore_hwSetOptionF)(int option, int value);
typedef int                     (DWIMEAPI *DWIMECore_hwRecognizeF)(const short * tracks, int tracksCount);
//typedef int                     (DWIMEAPI *DWIMECore_hwRecognizeMultiF)(const short * tracks, unsigned short * out);
typedef int                     (DWIMEAPI *DWIMECore_associateKeyF)(const unsigned short * uKeys);

typedef DWError                 (DWIMEAPI *DWIMECore_initF)(int lang, int kb, int im);
typedef DWError                 (DWIMEAPI *DWIMECore_deinitF)();
typedef DWError                 (DWIMEAPI *DWIMECore_resetF)();
typedef DWError                 (DWIMEAPI *DWIMECore_setOptionF)(int option, int value);
typedef DWError                 (DWIMEAPI *DWIMECore_processKeyF)(int uKey, DWKeyStatus uKeyStatus, int param);
typedef DWError                 (DWIMEAPI *DWIMECore_processKeysF)(const unsigned short *  uKeys, int param);
typedef int                     (DWIMEAPI *DWIMECore_getCandCountF)();
typedef int                     (DWIMEAPI *DWIMECore_getSyllableCountF)();
typedef int                     (DWIMEAPI *DWIMECore_getSyllableSelectedInfoF)();
typedef int                     (DWIMEAPI *DWIMECore_getCompCorrectCountF)(int candIdex);
typedef int                     (DWIMEAPI *DWIMECore_getCompCorrectInfoF)(int candIdex, int correctIdx);

typedef const unsigned short *  (DWIMEAPI *DWIMECore_getInputStringF)(unsigned short * out);
typedef const unsigned short *  (DWIMEAPI *DWIMECore_getCompStringF)(unsigned short * out);
typedef const unsigned short *  (DWIMEAPI *DWIMECore_getFormatPinyinF)(int index, unsigned short * out);
typedef const unsigned short *  (DWIMEAPI *DWIMECore_getCandStringF)(int index, unsigned short * out);
typedef const unsigned short *  (DWIMEAPI *DWIMECore_getSyllableStringF)(int index, unsigned short * out);
typedef const unsigned short *  (DWIMEAPI *DWIMECore_getCommitStringF)(unsigned short * out);
typedef const unsigned short *  (DWIMEAPI *DWIMECore_getCandCodeF)(int index, unsigned short * out);
//typedef const unsigned short *  (DWIMEAPI *DWIMECore_getCompCorrectPinyinF)(int candIdex, int correctIdx, unsigned short * outBuffer);

typedef DWBool                  (DWIMEAPI *DWIMECore_isCanCommitF)();
typedef DWBool                  (DWIMEAPI *DWIMECore_isAssociateModeF)();
typedef DWBool                  (DWIMEAPI *DWIMECore_hasInputF)();
typedef DWError                 (DWIMEAPI *DWIMECore_setCandSelectF)(int index);
typedef DWError                 (DWIMEAPI *DWIMECore_setSyllableSelectF)(int index);
typedef int                     (DWIMEAPI *DWIMECore_getCandAttrF)(int candIndex, int attrType);
typedef DWError                 (DWIMEAPI *DWIMECore_setPYFuzzyF)(int fuzzy);
typedef DWError                 (DWIMEAPI *DWIMECore_customPYFuzzyF)(const unsigned short * fuzzyFirst[], const unsigned short * fuzzySecond[], int count);
typedef DWError                 (DWIMEAPI *DWIMECore_setCorrectF)(int open);
typedef DWError                 (DWIMEAPI *DWIMECore_addCorrectF)(const unsigned short * py1, const unsigned short * py2);
typedef DWError                 (DWIMEAPI *DWIMECore_setCallbackF)(DWIMECore_Callback func, void * userData);

typedef void *                  (DWIMEAPI *DWIMECore_userDBCopyF)(int * outSize);
typedef void                    (DWIMEAPI *DWIMECore_userDBFreeF)(void * data);
typedef int                     (DWIMEAPI *DWIMECore_userDBImportContactsF)(const unsigned short * names, unsigned short sepCar);
typedef const unsigned short *  (DWIMEAPI *DWIMECore_userDBGetPhrF)(int dbType, int index, unsigned short * out);
typedef const unsigned short *  (DWIMEAPI *DWIMECore_userDBGetCodeF)(int dbType, int index, unsigned short * out);
typedef int                     (DWIMEAPI *DWIMECore_userDBGetCountF)(int dbType);
typedef DWError                 (DWIMEAPI *DWIMECore_userDBDelPhrF)(int dbType, int index);
typedef DWError                 (DWIMEAPI *DWIMECore_userDBDelPhr2F)(int dbType, const unsigned short *  inPhr);
typedef int                     (DWIMEAPI *DWIMECore_userDBGetAttrF)(int dbType, int index, int attrType);
typedef DWBool                  (DWIMEAPI *DWIMECore_userDBIsNewPhrF)(int dbType, int index);
typedef DWBool                  (DWIMEAPI *DWIMECore_userDBItemExistedF)(int dbType, const unsigned short * inPhr, const unsigned short * inPys);
typedef DWError                 (DWIMEAPI *DWIMECore_userDBAddF)(int dbType, const unsigned short * inPhr, const unsigned short * inPys);


static HINSTANCE g_hInstIme = NULL;
static int       g_HintCount = 0;

static DWIMECore_getLicenseF                       gLP_DWIMECore_getLicenseF           = NULL;
static DWIMECore_getEngineVersionF                 gLP_DWIMECore_getEngineVersionF     = NULL;
static DWIMECore_getMinorVersionF                  gLP_DWIMECore_getMinorVersionF      = NULL;
static DWIMECore_appBindingF                       gLP_DWIMECore_appBindingF           = NULL;
static DWIMECore_dataAddF                          gLP_DWIMECore_dataAddF              = NULL;
static DWIMECore_dataMapF                          gLP_DWIMECore_dataMapF              = NULL;
static DWIMECore_dataMapAF                         gLP_DWIMECore_dataMapAF             = NULL;
static DWIMECore_dataFileF                         gLP_DWIMECore_dataFileF             = NULL;
static DWIMECore_dataFileAF                        gLP_DWIMECore_dataFileAF            = NULL;
static DWIMECore_dataClearF                        gLP_DWIMECore_dataClearF            = NULL;
static DWIMECore_dataExistF                        gLP_DWIMECore_dataExistF            = NULL;
static DWIMECore_getLanguageF                      gLP_DWIMECore_getLanguageF          = NULL;
static DWIMECore_getInputModeF                     gLP_DWIMECore_getInputModeF         = NULL;
static DWIMECore_getKBTypeF                        gLP_DWIMECore_getKBTypeF            = NULL;
static DWIMECore_hwInitF                           gLP_DWIMECore_hwInitF               = NULL;
static DWIMECore_hwDeinitF                         gLP_DWIMECore_hwDeinitF             = NULL;
static DWIMECore_hwResetF                          gLP_DWIMECore_hwResetF              = NULL;
static DWIMECore_hwSetOptionF                      gLP_DWIMECore_hwSetOptionF          = NULL;
static DWIMECore_hwRecognizeF                      gLP_DWIMECore_hwRecognizeF          = NULL;
//static DWIMECore_hwRecognizeMultiF                 gLP_DWIMECore_hwRecognizeMultiF   = NULL;
static DWIMECore_associateKeyF                     gLP_DWIMECore_associateKeyF         = NULL;
static DWIMECore_initF                             gLP_DWIMECore_initF                 = NULL;
static DWIMECore_deinitF                           gLP_DWIMECore_deinitF               = NULL;
static DWIMECore_resetF                            gLP_DWIMECore_resetF                = NULL;
static DWIMECore_setOptionF                        gLP_DWIMECore_setOptionF            = NULL;
static DWIMECore_processKeyF                       gLP_DWIMECore_processKeyF           = NULL;
static DWIMECore_processKeysF                      gLP_DWIMECore_processKeysF          = NULL;
static DWIMECore_getCandCountF                     gLP_DWIMECore_getCandCountF         = NULL;
static DWIMECore_getSyllableCountF                 gLP_DWIMECore_getSyllableCountF     = NULL;
static DWIMECore_getSyllableSelectedInfoF          gLP_DWIMECore_getSyllableSelectedInfoF   = NULL;
static DWIMECore_getCompCorrectCountF              gLP_DWIMECore_getCompCorrectCountF       = NULL;
static DWIMECore_getCompCorrectInfoF               gLP_DWIMECore_getCompCorrectInfoF        = NULL;
//static DWIMECore_getCompCorrectPinyinF             gLP_DWIMECore_getCompCorrectPinyinF      = NULL;
static DWIMECore_getInputStringF                   gLP_DWIMECore_getInputStringF       = NULL;
static DWIMECore_getCompStringF                    gLP_DWIMECore_getCompStringF        = NULL;
static DWIMECore_getFormatPinyinF                  gLP_DWIMECore_getFormatPinyinF      = NULL;
static DWIMECore_getCandStringF                    gLP_DWIMECore_getCandStringF        = NULL;
static DWIMECore_getSyllableStringF                gLP_DWIMECore_getSyllableStringF    = NULL;
static DWIMECore_getCommitStringF                  gLP_DWIMECore_getCommitStringF      = NULL;
static DWIMECore_getCandCodeF                      gLP_DWIMECore_getCandCodeF          = NULL;
static DWIMECore_isCanCommitF                      gLP_DWIMECore_isCanCommitF          = NULL;
static DWIMECore_isAssociateModeF                  gLP_DWIMECore_isAssociateModeF      = NULL;
static DWIMECore_hasInputF                         gLP_DWIMECore_hasInputF             = NULL;
static DWIMECore_setCandSelectF                    gLP_DWIMECore_setCandSelectF        = NULL;
static DWIMECore_setSyllableSelectF                gLP_DWIMECore_setSyllableSelectF    = NULL;
static DWIMECore_getCandAttrF                      gLP_DWIMECore_getCandAttrF          = NULL;
static DWIMECore_setPYFuzzyF                       gLP_DWIMECore_setPYFuzzyF           = NULL;
static DWIMECore_customPYFuzzyF                    gLP_DWIMECore_customPYFuzzyF        = NULL;
static DWIMECore_setCorrectF                       gLP_DWIMECore_setCorrectF           = NULL;
static DWIMECore_addCorrectF                       gLP_DWIMECore_addCorrectF           = NULL;
static DWIMECore_setCallbackF                      gLP_DWIMECore_setCallbackF          = NULL;
static DWIMECore_userDBCopyF                       gLP_DWIMECore_userDBCopyF           = NULL;
static DWIMECore_userDBFreeF                       gLP_DWIMECore_userDBFreeF           = NULL;
static DWIMECore_userDBGetPhrF                     gLP_DWIMECore_userDBGetPhrF         = NULL;
static DWIMECore_userDBGetCodeF                    gLP_DWIMECore_userDBGetCodeF        = NULL;
static DWIMECore_userDBGetCountF                   gLP_DWIMECore_userDBGetCountF       = NULL;
static DWIMECore_userDBDelPhrF                     gLP_DWIMECore_userDBDelPhrF         = NULL;
static DWIMECore_userDBDelPhr2F                    gLP_DWIMECore_userDBDelPhr2F        = NULL;
static DWIMECore_userDBIsNewPhrF                   gLP_DWIMECore_userDBIsNewPhrF       = NULL;
static DWIMECore_userDBGetAttrF                    gLP_DWIMECore_userDBGetAttrF        = NULL;
static DWIMECore_userDBItemExistedF                gLP_DWIMECore_userDBItemExistedF    = NULL;
static DWIMECore_userDBAddF                        gLP_DWIMECore_userDBAddF            = NULL;
static DWIMECore_userDBImportContactsF             gLP_DWIMECore_userDBImportContactsF = NULL;


#ifdef __ISLINUX
static void * GetProcAddress(void *handle, char *symbol)
{
    return dlsym(handle, symbol);
}

static char * getpath(char * path, int bufLen)
{
    int rslt = readlink("/proc/self/exe", path, bufLen);
    if ( rslt < 0 || rslt >= bufLen)
    {
        return NULL;
    }
    path[rslt]=0;
    char * end = strrchr(path, '/');
    if (end != NULL)
        *end = 0;

    return path;
}
#endif


static void ldime_load_dll()
{
	if (g_hInstIme == NULL)
    {
        const wchar_t dllName[] =  { 0x0044, 0x0057, 0x0049, 0x004D, 0x0045, 0x0043, 0x006F, 0x0072, 0x0065, 0x002E, 0x0064, 0x006C, 0x006C, 0}; // DWIMECore.dll
        const wchar_t compName[] = { 0x0063, 0x006F, 0x006D, 0x0070, 0x0073, 0x005C, 0}; // comps\
        
#ifdef __ISWIN
        g_hInstIme = LoadLibraryW(dllName);
        if (g_hInstIme == NULL)
        {
            wchar_t curPath[261] = {0};
            wchar_t dllPath[261] = {0};

            // 获取当前exe目录
            {
                GetModuleFileNameW(NULL, curPath, 260);
                wchar_t * end = wcsrchr(curPath, '\\');
                if (end != NULL) { end++; *end = 0; }
            }

            if (g_hInstIme == NULL)
            {
                // 在exe当前目录里加载
                wcscpy(dllPath, curPath);
                wcscat(dllPath, dllName);
                g_hInstIme = LoadLibraryW(dllPath);
            }

            if (g_hInstIme == NULL)
            {
                // 在exe当前目录/comps里加载
                wcscpy(dllPath, curPath);
                wcscat(dllPath, compName);
                wcscat(dllPath, dllName);
                g_hInstIme = LoadLibraryW(dllPath);
            }

            if (g_hInstIme == NULL)
            {
                // 在 DLL 当前目录里加载
                GetModuleFileNameW((HINSTANCE)&__ImageBase, curPath, 260);
                wchar_t * end = wcsrchr(curPath, '\\');
                if (end != NULL) { end++; *end = 0; }
                wcscpy(dllPath, curPath);
                wcscat(dllPath, dllName);
                g_hInstIme = LoadLibraryW(dllPath);
            }
        }
#endif


#ifdef __ISLINUX
        g_hInstIme = dlopen("libDWIMECore.so", RTLD_LAZY);
        if (g_hInstIme == NULL)
        {
            char curPath[261] = {0};
            char soPath[261];

            // 获取当前exe目录
            getpath(curPath, 260);
            if (curPath[strlen(curPath)-1] != '/')
                strcat(curPath, "/");
            printf("getpath = %s \n", curPath);

            // 当前exe目录里找
            if (g_hInstIme == NULL)
            {
                strcpy(soPath, curPath);
                strcat(soPath, "libDWIMECore.so");
                g_hInstIme = dlopen(soPath, RTLD_LAZY);
                printf("load so = %s \n", soPath);
            }

            // 当前exe下comps目录里找
            if (g_hInstIme == NULL)
            {
                strcpy(soPath, curPath);
                strcat(soPath, "comps/libDWIMECore.so");
                g_hInstIme = dlopen(soPath, RTLD_LAZY);
                printf("load so = %s \n", soPath);
            }
            
            // 当前so 目录下找
            if (g_hInstIme == NULL)
            {
                Dl_info dl_info;
                dladdr((void*)ldime_load_dll, &dl_info);
                strcpy(soPath, dl_info.dli_fname);
                char * end = strrchr(soPath, '/');
                if (end != NULL) { end++; *end = 0; }
                strcat(soPath, "libDWIMECore.so");
                g_hInstIme = dlopen(soPath, RTLD_LAZY);
                printf("load so = %s \n", soPath);
            }
        }
        if (g_hInstIme == NULL)
        {
            printf("dlopen err:%s.\n", dlerror());
        }        
#endif
    }

    if (g_hInstIme == NULL)
    {
        g_HintCount++;
        if (g_HintCount < 3)
        {
#ifdef __ISWIN
            // 加载DWIMECore.dll失败！请将DWIMECore.dll放置于当前目录。
            const wchar_t msg[] = {0x52A0, 0x8F7D, 0x0044, 0x0057, 0x0049, 0x004D, 0x0045, 0x0043, 0x006F, 0x0072,
                                   0x0065, 0x002E, 0x0064, 0x006C, 0x006C, 0x5931, 0x8D25, 0xFF01, 0x8BF7, 0x5C06,
                                   0x0044, 0x0057, 0x0049, 0x004D, 0x0045, 0x0043, 0x006F, 0x0072, 0x0065, 0x002E,
                                   0x0064, 0x006C, 0x006C, 0x653E, 0x7F6E, 0x4E8E, 0x5F53, 0x524D, 0x76EE, 0x5F55, 0};
            // 错误提示
            const wchar_t title[] = {0x9519, 0x8BEF, 0x63D0, 0x793A, 0};
            ::MessageBoxW(GetForegroundWindow(),  msg, title, MB_ICONERROR);
#else
            printf("libDWIMECore.so failed to load!\n");
#endif
        }
    }

	if (gLP_DWIMECore_getLicenseF == NULL &&
		g_hInstIme != NULL)
	{
        gLP_DWIMECore_getLicenseF           = (DWIMECore_getLicenseF)        GetProcAddress(g_hInstIme, "DWIMECore_getLicense");
        gLP_DWIMECore_getEngineVersionF     = (DWIMECore_getEngineVersionF)  GetProcAddress(g_hInstIme, "DWIMECore_getEngineVersion");
        gLP_DWIMECore_getMinorVersionF      = (DWIMECore_getMinorVersionF)   GetProcAddress(g_hInstIme, "DWIMECore_getMinorVersion");
        gLP_DWIMECore_appBindingF           = (DWIMECore_appBindingF)        GetProcAddress(g_hInstIme, "DWIMECore_appBinding");
        gLP_DWIMECore_dataAddF              = (DWIMECore_dataAddF)           GetProcAddress(g_hInstIme, "DWIMECore_dataAdd");
        gLP_DWIMECore_dataMapF              = (DWIMECore_dataMapF)           GetProcAddress(g_hInstIme, "DWIMECore_dataMap");
        gLP_DWIMECore_dataMapAF             = (DWIMECore_dataMapAF)          GetProcAddress(g_hInstIme, "DWIMECore_dataMapA");
        gLP_DWIMECore_dataFileF             = (DWIMECore_dataFileF)          GetProcAddress(g_hInstIme, "DWIMECore_dataFile");
        gLP_DWIMECore_dataFileAF            = (DWIMECore_dataFileAF)         GetProcAddress(g_hInstIme, "DWIMECore_dataFileA");
        gLP_DWIMECore_dataClearF            = (DWIMECore_dataClearF)         GetProcAddress(g_hInstIme, "DWIMECore_dataClear");
        gLP_DWIMECore_dataExistF            = (DWIMECore_dataExistF)         GetProcAddress(g_hInstIme, "DWIMECore_dataExist");
        gLP_DWIMECore_getLanguageF          = (DWIMECore_getLanguageF)       GetProcAddress(g_hInstIme, "DWIMECore_getLanguage");
        gLP_DWIMECore_getInputModeF         = (DWIMECore_getInputModeF)      GetProcAddress(g_hInstIme, "DWIMECore_getInputMode");
        gLP_DWIMECore_getKBTypeF            = (DWIMECore_getKBTypeF)         GetProcAddress(g_hInstIme, "DWIMECore_getKBType");
        gLP_DWIMECore_hwInitF               = (DWIMECore_hwInitF)            GetProcAddress(g_hInstIme, "DWIMECore_hwInit");
        gLP_DWIMECore_hwDeinitF             = (DWIMECore_hwDeinitF)          GetProcAddress(g_hInstIme, "DWIMECore_hwDeinit");
        gLP_DWIMECore_hwResetF              = (DWIMECore_hwResetF)           GetProcAddress(g_hInstIme, "DWIMECore_hwReset");
        gLP_DWIMECore_hwSetOptionF          = (DWIMECore_hwSetOptionF)       GetProcAddress(g_hInstIme, "DWIMECore_hwSetOption");
        gLP_DWIMECore_hwRecognizeF          = (DWIMECore_hwRecognizeF)       GetProcAddress(g_hInstIme, "DWIMECore_hwRecognize");
        //gLP_DWIMECore_hwRecognizeMultiF   = (DWIMECore_hwRecognizeMultiF)  GetProcAddress(g_hInstIme, "DWIMECore_hwRecognizeMulti");
        gLP_DWIMECore_associateKeyF         = (DWIMECore_associateKeyF)      GetProcAddress(g_hInstIme, "DWIMECore_associateKey");
        gLP_DWIMECore_initF                 = (DWIMECore_initF)              GetProcAddress(g_hInstIme, "DWIMECore_init");
        gLP_DWIMECore_deinitF               = (DWIMECore_deinitF)            GetProcAddress(g_hInstIme, "DWIMECore_deinit");
        gLP_DWIMECore_resetF                = (DWIMECore_resetF)             GetProcAddress(g_hInstIme, "DWIMECore_reset");
        gLP_DWIMECore_setOptionF            = (DWIMECore_setOptionF)         GetProcAddress(g_hInstIme, "DWIMECore_setOption");
        gLP_DWIMECore_processKeyF           = (DWIMECore_processKeyF)        GetProcAddress(g_hInstIme, "DWIMECore_processKey");
        gLP_DWIMECore_processKeysF          = (DWIMECore_processKeysF)       GetProcAddress(g_hInstIme, "DWIMECore_processKeys");
        gLP_DWIMECore_getCandCountF         = (DWIMECore_getCandCountF)      GetProcAddress(g_hInstIme, "DWIMECore_getCandCount");
        gLP_DWIMECore_getSyllableCountF     = (DWIMECore_getSyllableCountF)  GetProcAddress(g_hInstIme, "DWIMECore_getSyllableCount");
        gLP_DWIMECore_getSyllableSelectedInfoF = (DWIMECore_getSyllableSelectedInfoF) GetProcAddress(g_hInstIme, "DWIMECore_getSyllableSelectedInfo");
        gLP_DWIMECore_getCompCorrectCountF     = (DWIMECore_getCompCorrectCountF)     GetProcAddress(g_hInstIme, "DWIMECore_getCompCorrectCount");
        gLP_DWIMECore_getCompCorrectInfoF      = (DWIMECore_getCompCorrectInfoF)      GetProcAddress(g_hInstIme, "DWIMECore_getCompCorrectInfo");
        //gLP_DWIMECore_getCompCorrectPinyinF    = (DWIMECore_getCompCorrectPinyinF)    GetProcAddress(g_hInstIme, "DWIMECore_getCompCorrectPinyin");
        gLP_DWIMECore_getInputStringF       = (DWIMECore_getInputStringF)    GetProcAddress(g_hInstIme, "DWIMECore_getInputString");
        gLP_DWIMECore_getCompStringF        = (DWIMECore_getCompStringF)     GetProcAddress(g_hInstIme, "DWIMECore_getCompString");
        gLP_DWIMECore_getFormatPinyinF      = (DWIMECore_getFormatPinyinF)   GetProcAddress(g_hInstIme, "DWIMECore_getFormatPinyin");
        gLP_DWIMECore_getCandStringF        = (DWIMECore_getCandStringF)     GetProcAddress(g_hInstIme, "DWIMECore_getCandString");
        gLP_DWIMECore_getSyllableStringF    = (DWIMECore_getSyllableStringF) GetProcAddress(g_hInstIme, "DWIMECore_getSyllableString");
        gLP_DWIMECore_getCommitStringF      = (DWIMECore_getCommitStringF)   GetProcAddress(g_hInstIme, "DWIMECore_getCommitString");
        gLP_DWIMECore_getCandCodeF          = (DWIMECore_getCandCodeF)       GetProcAddress(g_hInstIme, "DWIMECore_getCandCode");
        gLP_DWIMECore_isCanCommitF          = (DWIMECore_isCanCommitF)       GetProcAddress(g_hInstIme, "DWIMECore_isCanCommit");
        gLP_DWIMECore_isAssociateModeF      = (DWIMECore_isAssociateModeF)   GetProcAddress(g_hInstIme, "DWIMECore_isAssociateMode");
        gLP_DWIMECore_hasInputF             = (DWIMECore_hasInputF)          GetProcAddress(g_hInstIme, "DWIMECore_hasInput");
        gLP_DWIMECore_setCandSelectF        = (DWIMECore_setCandSelectF)     GetProcAddress(g_hInstIme, "DWIMECore_setCandSelect");
        gLP_DWIMECore_setSyllableSelectF    = (DWIMECore_setSyllableSelectF) GetProcAddress(g_hInstIme, "DWIMECore_setSyllableSelect");
        gLP_DWIMECore_getCandAttrF          = (DWIMECore_getCandAttrF)       GetProcAddress(g_hInstIme, "DWIMECore_getCandAttr");
        gLP_DWIMECore_setPYFuzzyF           = (DWIMECore_setPYFuzzyF)        GetProcAddress(g_hInstIme, "DWIMECore_setPYFuzzy");
        gLP_DWIMECore_customPYFuzzyF        = (DWIMECore_customPYFuzzyF)     GetProcAddress(g_hInstIme, "DWIMECore_customPYFuzzy");
        gLP_DWIMECore_setCorrectF           = (DWIMECore_setCorrectF)        GetProcAddress(g_hInstIme, "DWIMECore_setCorrect");
        gLP_DWIMECore_addCorrectF           = (DWIMECore_addCorrectF)        GetProcAddress(g_hInstIme, "DWIMECore_addCorrect");
        gLP_DWIMECore_setCallbackF          = (DWIMECore_setCallbackF)       GetProcAddress(g_hInstIme, "DWIMECore_setCallback");
        gLP_DWIMECore_userDBCopyF           = (DWIMECore_userDBCopyF)        GetProcAddress(g_hInstIme, "DWIMECore_userDBCopy");
        gLP_DWIMECore_userDBFreeF           = (DWIMECore_userDBFreeF)        GetProcAddress(g_hInstIme, "DWIMECore_userDBFree");
        gLP_DWIMECore_userDBGetPhrF         = (DWIMECore_userDBGetPhrF)      GetProcAddress(g_hInstIme, "DWIMECore_userDBGetPhr");
        gLP_DWIMECore_userDBGetCodeF        = (DWIMECore_userDBGetCodeF)     GetProcAddress(g_hInstIme, "DWIMECore_userDBGetCode");
        gLP_DWIMECore_userDBGetCountF       = (DWIMECore_userDBGetCountF)    GetProcAddress(g_hInstIme, "DWIMECore_userDBGetCount");
        gLP_DWIMECore_userDBDelPhrF         = (DWIMECore_userDBDelPhrF)      GetProcAddress(g_hInstIme, "DWIMECore_userDBDelPhr");
        gLP_DWIMECore_userDBDelPhr2F        = (DWIMECore_userDBDelPhr2F)     GetProcAddress(g_hInstIme, "DWIMECore_userDBDelPhr2");
        gLP_DWIMECore_userDBIsNewPhrF       = (DWIMECore_userDBIsNewPhrF)    GetProcAddress(g_hInstIme, "DWIMECore_userDBIsNewPhr");
        gLP_DWIMECore_userDBGetAttrF        = (DWIMECore_userDBGetAttrF)     GetProcAddress(g_hInstIme, "DWIMECore_userDBGetAttr");
        gLP_DWIMECore_userDBItemExistedF    = (DWIMECore_userDBItemExistedF) GetProcAddress(g_hInstIme, "DWIMECore_userDBItemExisted");
        gLP_DWIMECore_userDBAddF            = (DWIMECore_userDBAddF)         GetProcAddress(g_hInstIme, "DWIMECore_userDBAdd");
        gLP_DWIMECore_userDBImportContactsF = (DWIMECore_userDBImportContactsF)GetProcAddress(g_hInstIme, "DWIMECore_userDBImportContacts");
	}
}


const unsigned short * DWIMECore_getLicense(unsigned short * buffer)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getLicenseF != NULL)
    {
        return gLP_DWIMECore_getLicenseF(buffer);
    }
	return NULL;
}


DWVersion DWIMECore_getEngineVersion()
{
	ldime_load_dll();
    if (gLP_DWIMECore_getEngineVersionF != NULL)
    {
        return gLP_DWIMECore_getEngineVersionF();
    }
    return VERSION_UNKNOW;
}

int DWIMECore_getMinorVersion()
{
	ldime_load_dll();
    if (gLP_DWIMECore_getMinorVersionF != NULL)
    {
        return gLP_DWIMECore_getMinorVersionF();
    }
    return 0;
}


DWError DWIMECore_appBinding(const unsigned short * secretKey)
{
	ldime_load_dll();
	
	if (gLP_DWIMECore_appBindingF != NULL)
    {
        return gLP_DWIMECore_appBindingF(secretKey);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_dataAdd(int kind, void *  data, int size)
{
	ldime_load_dll();
	if (gLP_DWIMECore_dataAddF != NULL)
    {
        return gLP_DWIMECore_dataAddF(kind, data, size);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_dataMapA(int kind, const char * fileName)
{
	ldime_load_dll();
	if (gLP_DWIMECore_dataMapAF != NULL)
    {
        return gLP_DWIMECore_dataMapAF(kind, fileName);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_dataMap(int kind, const unsigned short * fileName)
{
    ldime_load_dll();
    if (gLP_DWIMECore_dataMapF != NULL)
    {
        return gLP_DWIMECore_dataMapF(kind, fileName);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_dataFileA(int kind, const char * fileName)
{
    ldime_load_dll();
    if (gLP_DWIMECore_dataFileAF != NULL)
    {
        return gLP_DWIMECore_dataFileAF(kind, fileName);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_dataFile(int kind, const unsigned short * fileName)
{
    ldime_load_dll();
    if (gLP_DWIMECore_dataFileF != NULL)
    {
        return gLP_DWIMECore_dataFileF(kind, fileName);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_dataClear()
{
	ldime_load_dll();
	if (gLP_DWIMECore_dataClearF != NULL)
    {
        return gLP_DWIMECore_dataClearF();
    }
    return DWRT_ENGINE_NULL;
}

DWBool DWIMECore_dataExist(int kind)
{
	ldime_load_dll();
	if (gLP_DWIMECore_dataExistF != NULL)
    {
        return gLP_DWIMECore_dataExistF(kind);
    }
    return dw_false;
}


DWLanguage DWIMECore_getLanguage()
{
    ldime_load_dll();
    if (gLP_DWIMECore_getLanguageF != NULL)
    {
        return gLP_DWIMECore_getLanguageF();
    }
    return DWL_None;
}

DWInputMode DWIMECore_getInputMode()
{
    ldime_load_dll();
    if (gLP_DWIMECore_getInputModeF != NULL)
    {
        return gLP_DWIMECore_getInputModeF();
    }
    return DWIM_NONE;
}


DWKBType DWIMECore_getKBType()
{
    ldime_load_dll();
    if (gLP_DWIMECore_getKBTypeF != NULL)
    {
        return gLP_DWIMECore_getKBTypeF();
    }
    return DWKBT_NONE;
}



DWError DWIMECore_hwInit(int param)
{
	ldime_load_dll();
	if (gLP_DWIMECore_hwInitF != NULL)
    {
        return gLP_DWIMECore_hwInitF(param);
    }
    return DWRT_ENGINE_NULL;
}



DWError DWIMECore_hwDeinit()
{
    ldime_load_dll();
    if (gLP_DWIMECore_hwDeinitF != NULL)
    {
        return gLP_DWIMECore_hwDeinitF();
    }
    return DWRT_ENGINE_NULL;
}



DWError DWIMECore_hwReset()
{
    ldime_load_dll();
    if (gLP_DWIMECore_hwResetF != NULL)
    {
        return gLP_DWIMECore_hwResetF();
    }
    return DWRT_ENGINE_NULL;
}



DWError DWIMECore_hwSetOption(int option, int value)
{
	ldime_load_dll();
	if (gLP_DWIMECore_hwSetOptionF != NULL)
    {
        return gLP_DWIMECore_hwSetOptionF(option, value);
    }
    return DWRT_ENGINE_NULL;
}

int DWIMECore_hwRecognize(const short * tracks, int tracksCount)
{
	ldime_load_dll();
	if (gLP_DWIMECore_hwRecognizeF != NULL)
    {
        return gLP_DWIMECore_hwRecognizeF(tracks, tracksCount);
    }
    return 0;
}

/*
int DWIMECore_hwRecognizeMulti(const short * tracks, unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_hwRecognizeMultiF != NULL)
    {
        return gLP_DWIMECore_hwRecognizeMultiF(tracks, out);
    }
    return 0;
}
*/


int DWIMECore_associateKey(const unsigned short * uKeys)
{
	ldime_load_dll();
	if (gLP_DWIMECore_associateKeyF != NULL)
    {
        return gLP_DWIMECore_associateKeyF(uKeys);
    }
    return 0;
}

DWError DWIMECore_init(int lang, int kb, int im)
{
	ldime_load_dll();
	if (gLP_DWIMECore_initF != NULL)
    {
        return gLP_DWIMECore_initF(lang, kb, im);
    }
    return DWRT_ENGINE_NULL;
}

DWError DWIMECore_deinit()
{
	ldime_load_dll();
	if (gLP_DWIMECore_deinitF != NULL)
    {
        return gLP_DWIMECore_deinitF();
    }
    return DWRT_ENGINE_NULL;
}

DWError DWIMECore_reset()
{
	ldime_load_dll();
	if (gLP_DWIMECore_resetF != NULL)
    {
        return gLP_DWIMECore_resetF();
    }
    return DWRT_ENGINE_NULL;
}



DWError DWIMECore_setOption(int option, int value)
{
	ldime_load_dll();
	if (gLP_DWIMECore_setOptionF != NULL)
    {
        return gLP_DWIMECore_setOptionF(option, value);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_processKey(int uKey, DWKeyStatus uKeyStatus, int param)
{
	ldime_load_dll();
	if (gLP_DWIMECore_processKeyF != NULL)
    {
        return gLP_DWIMECore_processKeyF(uKey, uKeyStatus, param);
    }
    return DWRT_ENGINE_NULL;
}

DWError DWIMECore_processKeys(const unsigned short *  uKeys, int param)
{
	ldime_load_dll();
	if (gLP_DWIMECore_processKeysF != NULL)
    {
        return gLP_DWIMECore_processKeysF(uKeys, param);
    }
    return DWRT_ENGINE_NULL;
}

int DWIMECore_getCandCount()
{
	ldime_load_dll();
	if (gLP_DWIMECore_getCandCountF != NULL)
    {
        return gLP_DWIMECore_getCandCountF();
    }
    return 0;
}


int DWIMECore_getSyllableCount()
{
	ldime_load_dll();
	if (gLP_DWIMECore_getSyllableCountF != NULL)
    {
        return gLP_DWIMECore_getSyllableCountF();
    }
    return 0;
}



int DWIMECore_getSyllableSelectedInfo()
{
	ldime_load_dll();
	if (gLP_DWIMECore_getSyllableSelectedInfoF != NULL)
    {
        return gLP_DWIMECore_getSyllableSelectedInfoF();
    }
    return 0;
}


int DWIMECore_getCompCorrectCount(int candIdex)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getCompCorrectCountF != NULL)
    {
        return gLP_DWIMECore_getCompCorrectCountF(candIdex);
    }
    return 0;
}



int DWIMECore_getCompCorrectInfo(int candIdex, int correctIdx)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getCompCorrectInfoF != NULL)
    {
        return gLP_DWIMECore_getCompCorrectInfoF(candIdex, correctIdx);
    }
    return 0;
}


#if 0
const unsigned short * DWIMECore_getCompCorrectPinyin(int candIdex, int correctIdx, unsigned short * outBuffer)
{
	ldime_load_dll();
    if (gLP_DWIMECore_getCompCorrectPinyinF != NULL)
    {
        return gLP_DWIMECore_getCompCorrectPinyinF(candIdex, correctIdx, outBuffer);
    }
    return 0;
}

#endif


const unsigned short * DWIMECore_getInputString(unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getInputStringF != NULL)
    {
        return gLP_DWIMECore_getInputStringF(out);
    }
    return NULL;
}

const unsigned short * DWIMECore_getCompString(unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getCompStringF != NULL)
    {
        return gLP_DWIMECore_getCompStringF(out);
    }
    return NULL;
}

const unsigned short * DWIMECore_getFormatPinyin(int index, unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getFormatPinyinF != NULL)
    {
        return gLP_DWIMECore_getFormatPinyinF(index, out);
    }
    return NULL;
}

const unsigned short * DWIMECore_getCandString(int index, unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getCandStringF != NULL)
    {
        return gLP_DWIMECore_getCandStringF(index, out);
    }
    return NULL;
}

const unsigned short * DWIMECore_getSyllableString(int index, unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getSyllableStringF != NULL)
    {
        return gLP_DWIMECore_getSyllableStringF(index, out);
    }
    return NULL;
}

const unsigned short * DWIMECore_getCommitString(unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getCommitStringF != NULL)
    {
        return gLP_DWIMECore_getCommitStringF(out);
    }
    return NULL;
}

const unsigned short * DWIMECore_getCandCode(int index, unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getCandCodeF != NULL)
    {
        return gLP_DWIMECore_getCandCodeF(index, out);
    }
    return NULL;
}


DWBool DWIMECore_isCanCommit()
{
	ldime_load_dll();
	if (gLP_DWIMECore_isCanCommitF != NULL)
    {
        return gLP_DWIMECore_isCanCommitF();
    }
    return dw_false;
}

DWBool DWIMECore_isAssociateMode()
{
	ldime_load_dll();
	if (gLP_DWIMECore_isAssociateModeF != NULL)
    {
        return gLP_DWIMECore_isAssociateModeF();
    }
    return dw_false;
}


DWBool DWIMECore_hasInput()
{
    ldime_load_dll();
    if (gLP_DWIMECore_hasInputF != NULL)
    {
        return gLP_DWIMECore_hasInputF();
    }
    return dw_false;
}



DWError DWIMECore_setCandSelect(int index)
{
	ldime_load_dll();
	if (gLP_DWIMECore_setCandSelectF != NULL)
    {
        return gLP_DWIMECore_setCandSelectF(index);
    }
    return DWRT_ENGINE_NULL;
}

DWError DWIMECore_setSyllableSelect(int index)
{
	ldime_load_dll();
	if (gLP_DWIMECore_setSyllableSelectF != NULL)
    {
        return gLP_DWIMECore_setSyllableSelectF(index);
    }
    return DWRT_ENGINE_NULL;
}

int DWIMECore_getCandAttr(int candIndex, int attrType)
{
	ldime_load_dll();
	if (gLP_DWIMECore_getCandAttrF != NULL)
    {
        return gLP_DWIMECore_getCandAttrF(candIndex, attrType);
    }
    return 0;
}

DWError DWIMECore_setPYFuzzy(int fuzzy)
{
	ldime_load_dll();
	if (gLP_DWIMECore_setPYFuzzyF != NULL)
    {
        return gLP_DWIMECore_setPYFuzzyF(fuzzy);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_customPYFuzzy(const unsigned short * fuzzyFirst[], const unsigned short * fuzzySecond[], int count)
{
	ldime_load_dll();
	if (gLP_DWIMECore_customPYFuzzyF != NULL)
    {
        return gLP_DWIMECore_customPYFuzzyF(fuzzyFirst, fuzzySecond, count);
    }
    return DWRT_ENGINE_NULL;
}


DWError DWIMECore_setCorrect(int open)
{
	ldime_load_dll();
	if (gLP_DWIMECore_setCorrectF != NULL)
    {
        return gLP_DWIMECore_setCorrectF(open);
    }
    return DWRT_ENGINE_NULL;
}

DWError DWIMECore_addCorrect(const unsigned short * py1, const unsigned short * py2)
{
	ldime_load_dll();
	if (gLP_DWIMECore_addCorrectF != NULL)
    {
        return gLP_DWIMECore_addCorrectF(py1, py2);
    }
    return DWRT_ENGINE_NULL;
}



DWError DWIMECore_setCallback(DWIMECore_Callback func, void * userData)
{
    ldime_load_dll();
    if (gLP_DWIMECore_setCallbackF != NULL)
    {
        return gLP_DWIMECore_setCallbackF(func, userData);
    }
    return DWRT_ENGINE_NULL;
}


void * DWIMECore_userDBCopy(int * outSize)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBCopyF != NULL)
    {
        return gLP_DWIMECore_userDBCopyF(outSize);
    }

    return NULL;
}

void DWIMECore_userDBFree(void * data)
{
    ldime_load_dll();
    if (gLP_DWIMECore_userDBFreeF != NULL)
    {
        gLP_DWIMECore_userDBFreeF(data);
    }
}



const unsigned short * DWIMECore_userDBGetPhr(int dbType, int index, unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBGetPhrF != NULL)
    {
        return gLP_DWIMECore_userDBGetPhrF(dbType, index, out);
    }
    return NULL;
}

const unsigned short * DWIMECore_userDBGetCode(int dbType, int index, unsigned short * out)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBGetCodeF != NULL)
    {
        return gLP_DWIMECore_userDBGetCodeF(dbType, index, out);
    }
    return NULL;
}

int DWIMECore_userDBGetCount(int dbType)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBGetCountF != NULL)
    {
        return gLP_DWIMECore_userDBGetCountF(dbType);
    }
    return 0;
}

DWError DWIMECore_userDBDelPhr(int dbType, int index)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBDelPhrF != NULL)
    {
        return gLP_DWIMECore_userDBDelPhrF(dbType, index);
    }
    return DWRT_ENGINE_NULL;
}

DWError DWIMECore_userDBDelPhr2(int dbType, const unsigned short *  inPhr)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBDelPhr2F != NULL)
    {
        return gLP_DWIMECore_userDBDelPhr2F(dbType, inPhr);
    }
    return DWRT_ENGINE_NULL;
}

DWBool DWIMECore_userDBIsNewPhr(int dbType, int index)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBIsNewPhrF != NULL)
    {
        return gLP_DWIMECore_userDBIsNewPhrF(dbType, index);
    }
    return dw_false;
}


int DWIMECore_userDBGetAttr(int dbType, int index, int attrType)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBGetAttrF != NULL)
    {
        return gLP_DWIMECore_userDBGetAttrF(dbType, index, attrType);
    }
    return 0;
}


DWBool DWIMECore_userDBItemExisted(int dbType, const unsigned short * inPhr, const unsigned short * inPys)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBItemExistedF != NULL)
    {
        return gLP_DWIMECore_userDBItemExistedF(dbType, inPhr, inPys);
    }
    return dw_false;
}

DWError DWIMECore_userDBAdd(int dbType, const unsigned short * inPhr, const unsigned short * inPys)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBAddF != NULL)
    {
        return gLP_DWIMECore_userDBAddF(dbType, inPhr, inPys);
    }
    return DWRT_ENGINE_NULL;
}
 
int DWIMECore_userDBImportContacts(const unsigned short * names, unsigned short sepCar)
{
	ldime_load_dll();
	if (gLP_DWIMECore_userDBImportContactsF != NULL)
    {
        return gLP_DWIMECore_userDBImportContactsF(names, sepCar);
    }
    return 0;                                                                                                                     
}                                                                                                                          
                                                                                                                              


