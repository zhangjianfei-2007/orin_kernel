#ifndef KERNEL_CAMERA_H_
#define KERNEL_CANERA_H_

#include <media/max96712.h>
#include <media/max9296.h>
#include <media/max9295.h>

#ifdef DEBUG
#define debug(fmt, ...) printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define CAMERA_MINOR MISC_DYNAMIC_MINOR

typedef struct {
	unsigned int id; //normally it's 0~7. for des(max9296), 0&1 indicate the same id, as well as 2&3, 4&5, 6&7. for poc, it's 0~1.
	unsigned int addr; //normally it's a 16 bits register address like max9296, max9295, max96705, max96717.
	unsigned int val; //normally it's a 8 bits register value.
}CAM_COMM_REG_TYPE;

typedef struct {
	unsigned int id; //normally it's camera id of 0~7. for poc, it's 0~1.
	unsigned int addr; //normally it's a 7-bit i2c client address like 0x40 or 0x62 of max9295.
}CAM_COMM_I2C_TYPE;

#ifndef CAM_COMM_IOCTL_TYPE
#define	CAM_COMM_IOCTL_TYPE 'c'
#endif


#ifndef	CMD_SET_DES_REGISTER
#define	CMD_SET_DES_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 0, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_GET_DES_REGISTER
#define	CMD_GET_DES_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 1, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_SET_SER_9295_REGISTER
#define	CMD_SET_SER_9295_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 2, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_GET_SER_9295_REGISTER
#define	CMD_GET_SER_9295_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 3, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_SET_SEN_5200_REGISTER
#define	CMD_SET_SEN_5200_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 4, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_GET_SEN_5200_REGISTER
#define	CMD_GET_SEN_5200_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 5, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_SET_SEN_IMX390_REGISTER
#define	CMD_SET_SEN_IMX390_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 6, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_GET_SEN_IMX390_REGISTER
#define	CMD_GET_SEN_IMX390_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 7, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_SET_SEN_OX03C10_REGISTER
#define	CMD_SET_SEN_OX03C10_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 8, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_GET_SEN_OX03C10_REGISTER
#define	CMD_GET_SEN_OX03C10_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 9, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_SET_SER_I2C_CLIENT_ADDR
#define	CMD_SET_SER_I2C_CLIENT_ADDR				_IOWR(CAM_COMM_IOCTL_TYPE, 20, CAM_COMM_I2C_TYPE)
#endif

#ifndef	CMD_SET_POC_REGISTER
#define	CMD_SET_POC_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 100, CAM_COMM_REG_TYPE)
#endif

#ifndef	CMD_GET_POC_REGISTER
#define	CMD_GET_POC_REGISTER				_IOWR(CAM_COMM_IOCTL_TYPE, 101, CAM_COMM_REG_TYPE)
#endif

extern struct device *max96712_dev_listp[12];
extern int n_max96712_dev;

extern struct device *dser_dev_listp[8];
extern int n_dser_dev;

extern struct device *ser_9295_listp[12];
extern int n_9295_dev;

extern struct camera_common_data *sen_5200_listp[12];
extern int n_5200_dev;

extern struct camera_common_data *sen_imx390_listp[12];
extern int n_imx390_dev;

extern struct camera_common_data *sen_ox03c10_listp[12];
extern int n_ox03c10_dev;

extern struct device *poc_dev_listp[3];
extern int n_poc_dev;

int gw5200_read_reg(struct camera_common_data *s_data, u16 addr, u8 *val);

int gw5200_write_reg(struct camera_common_data *s_data, u16 addr, u8 val);

int imx390_read_reg(struct camera_common_data *s_data, u16 addr, u8 *val);

int imx390_write_reg(struct camera_common_data *s_data, u16 addr, u8 val);

int ox03c10_read_reg(struct camera_common_data *s_data, u16 addr, u8 *val);

int ox03c10_write_reg(struct camera_common_data *s_data, u16 addr, u8 val);

int max2008X_write_reg(struct device *dev, u8 addr, u8 val);

int max2008X_read_reg(struct device *dev, u8 addr, u8 *val);

#endif /* KERNEL_CAMERA_H_ */
