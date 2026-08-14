#include "maintenance.h"

#include "dev-codes-need-rising-trigger.h"

#include "global.h"

//
CMaintenance::CMaintenance(QObject *parent) : QObject(parent)
{

}

CMaintenance::~CMaintenance()
{

}

void CMaintenance::slotConfigLoaded()
{
    /* NOTE: 2025-12-19：崔继友转达说度申说下降沿触发方式在低温时容易触发失败，所以改为上升沿触发
     * 解决方法：
     * 1、增加 CGlobal::triggerInputType 配置值，且默认为未知。
     * 2、打开相机前，检查 CGlobal::triggerInputType，若为空，则初始化。
     * 3、初始化方法：若设备编号属于指定列表，则初始化为“上升沿触发”，否则为“下降沿触发”。
     */

    // CGlobal::triggerInputType 的初始化
    if (enTriggerInputType::Unknown == CGlobal::triggerInputType) {
        if (C_DEV_CODES_NEED_RISING_TRIGGER.contains(CGlobal::devNum)) {
            CGlobal::triggerInputType = enTriggerInputType::RisingEdge;
        } else {
            CGlobal::triggerInputType = enTriggerInputType::FallingEdge;
        }
        CGlobal::saveConfs();
    }
}
