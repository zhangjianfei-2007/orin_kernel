subdir-ccflags-y += -Werror
subdir-ccflags-y += -DDEBUG

#for max96712 compatibility, define MAX96717_COMPAT
subdir-ccflags-y += -DMAX96717_COMPAT
subdir-ccflags-y += -DMFP7_MFP8_COMPAT


ccflags-y += -I$(srctree)/drivers/media/platform/tegra
ccflags-y += -I$(srctree)/drivers/video/tegra/camera

obj-$(CONFIG_VIDEO_MAX2008X) += max2008X.o
obj-$(CONFIG_NV_VIDEO_IMX185) += nv_imx185.o
obj-$(CONFIG_NV_VIDEO_IMX185) += nv_imx185_v1.o
obj-$(CONFIG_NV_VIDEO_IMX477) += nv_imx477.o
obj-$(CONFIG_NV_VIDEO_IMX219) += nv_imx219.o
obj-$(CONFIG_NV_VIDEO_IMX268) += nv_imx268.o
obj-$(CONFIG_NV_VIDEO_IMX274) += nv_imx274.o
obj-$(CONFIG_NV_VIDEO_IMX318) += nv_imx318.o
obj-$(CONFIG_NV_VIDEO_LC898212) += nv_lc898212.o
obj-$(CONFIG_NV_VIDEO_OV5693) += nv_ov5693.o
obj-$(CONFIG_NV_VIDEO_OV9281) += nv_ov9281.o
obj-$(CONFIG_NV_VIDEO_OV10823) += nv_ov10823.o
obj-$(CONFIG_NV_VIDEO_OV23850) += nv_ov23850.o
obj-$(CONFIG_I2C_IOEXPANDER_PCA9570) += pca9570.o
obj-$(CONFIG_VIDEO_TC358840) += tc358840.o
obj-$(CONFIG_VIDEO_LT6911UXC) +=lt6911uxc.o
#obj-$(CONFIG_NV_VIDEO_IMX390) += nv_imx390.o
obj-$(CONFIG_NV_VIDEO_IMX390) += imx390.o
obj-$(CONFIG_NV_VIDEO_AR0234) += nv_ar0234.o
obj-$(CONFIG_NV_DESER_MAX96712) += max96712.o
obj-$(CONFIG_I2C_IOEXPANDER_DESER_MAX9296) += max9296.o
obj-$(CONFIG_I2C_IOEXPANDER_SER_MAX9295) += max9295.o
obj-$(CONFIG_VIDEO_GW5200) += gw5200.o
obj-$(CONFIG_I2C_IOEXPANDER_SER_MAX96705) += max96705.o
obj-$(CONFIG_VIDEO_AP0202) += ap0202.o
obj-$(CONFIG_VIDEO_OX01F10) += ox01f10.o
obj-$(CONFIG_VIDEO_OX03C10) += ox03c10.o
obj-$(CONFIG_VIDEO_SIMPLIFIED_CAM_MGT) += camera.o

