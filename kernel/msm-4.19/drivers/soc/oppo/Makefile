obj-$(CONFIG_OPPO_COMMON_SOFT)	+=  device_info/
obj-$(CONFIG_OPPO_DEVICE_IFNO)	+=  project_info/
obj-$(CONFIG_RECORD_MDMRST)     +=  oppo_mdmrst/
obj-y +=  sensor/
obj-y +=  oppo_gpio/

#ifdefine VENDOR_EDIT
#GaoTing.Gan@PSW.MultiMedia.MediaServer, 2019/03/07, Add for kevent feedback
obj-y +=  oppo_kevent_feedback/
#endif /* VENDOR_EDIT */

#ifdef VENDOR_EDIT
#cuixiaogang@SH.hypnus.2019.09.04. proting hypnus
obj-$(CONFIG_OPPO_HYPNUS)     +=  oppo_hypnus/
#endif /* VENDOR_EDIT */
#ifdef VENDOR_EDIT
#//wenbin.liu@PSW.Kernel.MM, 2018/05/23, Add for Healthinfo monitor
obj-y     +=  oppo_healthinfo/
#endif /* VENDOR_EDIT */

#Liang.Zhang@TECH.Storage.Stability.OPPO_SHUTDOWN_DETECT, 2019/04/28, Add for shutdown detect
obj-$(CONFIG_OPPO_SHUTDOWN_DETECT)  +=  oppo_shutdown_detect/
#Kun.Hu@TECH.BSP.Stability.PHOENIX_PROJECT 2019/06/15, Add for PHOENIX_PROJECT
obj-$(CONFIG_OPPO_PHOENIX)     +=  oppo_phoenix/
#ifdef VENDOR_EDIT
#linzhangru@PSW.TECH, 2019/11/30, Add for sau power key detection
obj-y += oppo_saupwk/
#endif /* VENDOR_EDIT */
#Yajie.chen@Tech.BSP.Sensor,2019/11/18, Add for SLPI Monitor
obj-$(CONFIG_RECORD_SLPIRST)    +=  oppo_slpirst/
obj-y     +=  oppo_BScheck/
obj-y     +=  oppo_brightscreen_check/
