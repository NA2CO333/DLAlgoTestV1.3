#ifndef CGLOBALOBJ_H
#define CGLOBALOBJ_H

#include "global_intf.h"

//
class CGlobalObj : public CGlobalIntf
{
public:
    CGlobalObj();

    void asyncSuspensionPrompt(const QString &_msg, int _msecs = 0) override;


};

#endif // CGLOBALOBJ_H
