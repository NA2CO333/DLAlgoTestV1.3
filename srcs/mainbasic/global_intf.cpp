#include "global_intf.h"

static CGlobalIntf *m_globalObj = nullptr;

void setGlobalIntf(CGlobalIntf *_obj)
{
    m_globalObj = _obj;
}

CGlobalIntf *globalIntf()
{
    return m_globalObj;
}
