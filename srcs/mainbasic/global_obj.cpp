#include "global_obj.h"

#include "winmanage.h"

CGlobalObj::CGlobalObj() : CGlobalIntf()
{

}

void CGlobalObj::asyncSuspensionPrompt(const QString &_msg, int _msecs)
{
    getWinManage()->asyncSuspensionPrompt(_msg, _msecs);
}
