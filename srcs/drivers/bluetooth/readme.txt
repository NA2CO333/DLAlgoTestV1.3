1.将rtk_bt.c,rtk_bt.h,rtk_coex.c,rtk_coex.h拷贝到drivers/bluetooth目录下
2.修改drivers/bluetooth/Makefiles
    将obj-$(CONFIG_BT_RTKBTUSB)      += rtk_btusb.o
    改为
    obj-y   += rtk_coex.o rtk_bt.o