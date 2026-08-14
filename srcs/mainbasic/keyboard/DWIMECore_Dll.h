#ifndef DWIMECore_Dll_H
#define DWIMECore_Dll_H


#if defined(_WIN32) || defined(_WINDOWS)
    #include <string>
    #include <mbstring.h>
    #include <tchar.h>
#endif


#undef  DWIMELIB_API
#define DWIMELIB_API

#include "DWIMECore.h"



#endif // #define DWIMECore_Dll_H