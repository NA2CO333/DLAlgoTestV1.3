#ifndef CGLOBALINTF_H
#define CGLOBALINTF_H

#include <QString>

// 全局接口（Global Interface）
class CGlobalIntf
{
public:

    virtual void asyncSuspensionPrompt(const QString &_msg, int _msecs = 0) = 0;        // 异步悬浮提示框（跨线程调用）


};

/// ============================================================================================

// 设置全局接口指针
void setGlobalIntf(CGlobalIntf *_obj);

// 全局接口指针
CGlobalIntf *globalIntf();                           /* 注意：这个函数须在当前进程已经调用 setGlobalObj() 设置了全局接口对象之后才能调用，否则会发生空指针异常。 */

#endif // CGLOBALINTF_H
