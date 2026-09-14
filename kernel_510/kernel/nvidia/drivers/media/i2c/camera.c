/*
 * simplified camera managment interface.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/ioport.h>

#include "camera.h"

#define CAMERA_DEVNAME "camera"

extern int des_dev_type;

static int camera_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	return ret;
}

static int camera_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t camera_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos) 
{
	return 0;

}

static ssize_t camera_write(struct file *file, const char __user *buf,
			  size_t count, loff_t * ppos)
{
	return 0;	
}

static unsigned int camera_poll(struct file *file,
				 struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	
	return mask;
}

static long camera_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	uint16_t addr = 0;
	uint8_t val = 0;
	CAM_COMM_REG_TYPE reg;
	CAM_COMM_I2C_TYPE i2c;

	switch(cmd)
	{
		case CMD_SET_DES_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			val = reg.val;
			if(reg.id >= 0 && reg.id < 12){
				if(des_dev_type == DES_MAX9296){
					if(reg.id >= 8)
						break;
					ret = max9296_write_reg(dser_dev_listp[reg.id], addr, val);
				}else if(des_dev_type == DES_MAX96712)
					ret = max96712_write_reg(max96712_dev_listp[reg.id], addr, val);
			}
			break;
		case CMD_GET_DES_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			if(reg.id >= 0 && reg.id < 12){
				if(des_dev_type == DES_MAX9296){
					if(reg.id >= 8)
						break;
					ret = max9296_read_reg(dser_dev_listp[reg.id], addr, &val);
				}else if(des_dev_type == DES_MAX96712)
					ret = max96712_read_reg(max96712_dev_listp[reg.id], addr, &val);
			}
			if (ret == 0) {
				reg.val = val;
				ret = copy_to_user((void*)arg, &reg, sizeof(CAM_COMM_REG_TYPE));
			}
			break;
		case CMD_SET_SER_9295_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			val = reg.val;
			if(reg.id >= 0 && reg.id < 12){
				if(ser_9295_listp[reg.id] == NULL)
					break;
				ret = max9295_write_reg(ser_9295_listp[reg.id], addr, val);
			}
			break;
		case CMD_GET_SER_9295_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			if(reg.id >= 0 && reg.id < 12){
				if(ser_9295_listp[reg.id] == NULL)
					break;
				ret = max9295_read_reg(ser_9295_listp[reg.id], addr, &val);
			}
			if (ret == 0) {
				reg.val = val;
				ret = copy_to_user((void*)arg, &reg, sizeof(CAM_COMM_REG_TYPE));
			}
			break;
		case CMD_SET_SEN_5200_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			val = reg.val;
			if(reg.id >= 0 && reg.id < 12){
				if(sen_5200_listp[reg.id] == NULL)
					break;
				ret = gw5200_write_reg(sen_5200_listp[reg.id], addr, val);
			}
			break;
		case CMD_GET_SEN_5200_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			if(reg.id >= 0 && reg.id < 12){
				if(sen_5200_listp[reg.id] == NULL)
					break;
				ret = gw5200_read_reg(sen_5200_listp[reg.id], addr, &val);
			}
			if (ret == 0) {
				reg.val = val;
				ret = copy_to_user((void*)arg, &reg, sizeof(CAM_COMM_REG_TYPE));
			}
			break;
		case CMD_SET_SEN_IMX390_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			val = reg.val;
			if(reg.id >= 0 && reg.id < 12){
				if(sen_imx390_listp[reg.id] == NULL)
					break;
				ret = imx390_write_reg(sen_imx390_listp[reg.id], addr, val);
			}
			break;
		case CMD_GET_SEN_IMX390_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			if(reg.id >= 0 && reg.id < 12){
				if(sen_imx390_listp[reg.id] == NULL)
					break;
				ret = imx390_read_reg(sen_imx390_listp[reg.id], addr, &val);
			}
			if (ret == 0) {
				reg.val = val;
				ret = copy_to_user((void*)arg, &reg, sizeof(CAM_COMM_REG_TYPE));
			}
			break;
		case CMD_SET_SEN_OX03C10_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			val = reg.val;
			if(reg.id >= 0 && reg.id < 12){
				if(sen_ox03c10_listp[reg.id] == NULL)
					break;
				ret = ox03c10_write_reg(sen_ox03c10_listp[reg.id], addr, val);
			}
			break;
		case CMD_GET_SEN_OX03C10_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			if(reg.id >= 0 && reg.id < 12){
				if(sen_ox03c10_listp[reg.id] == NULL)
					break;
				ret = ox03c10_read_reg(sen_ox03c10_listp[reg.id], addr, &val);
			}
			if (ret == 0) {
				reg.val = val;
				ret = copy_to_user((void*)arg, &reg, sizeof(CAM_COMM_REG_TYPE));
			}
			break;
		case CMD_SET_POC_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			val = reg.val;
			if(reg.id >= 0 && reg.id < 3){
				if(poc_dev_listp[reg.id] == NULL)
					break;
				ret = max2008X_write_reg(poc_dev_listp[reg.id], (uint8_t)addr, val);
			}
			break;
		case CMD_GET_POC_REGISTER:
			ret = copy_from_user(&reg, (void *)arg, sizeof(CAM_COMM_REG_TYPE));
			if (ret)
				break;
			addr = reg.addr;
			if(reg.id >= 0 && reg.id < 3){
				if(poc_dev_listp[reg.id] == NULL)
					break;
				ret = max2008X_read_reg(poc_dev_listp[reg.id], (uint8_t)addr, &val);
			}
			if (ret == 0) {
				reg.val = val;
				ret = copy_to_user((void*)arg, &reg, sizeof(CAM_COMM_REG_TYPE));
			}
			break;
		case CMD_SET_SER_I2C_CLIENT_ADDR:
			ret = copy_from_user(&i2c, (void *)arg, sizeof(CAM_COMM_I2C_TYPE));
			if (ret)
				break;
			if(i2c.id >= 0 && i2c.id < 12){
				if(ser_9295_listp[reg.id] == NULL)
					break;
				ret = max9295_set_i2c_client_addr(ser_9295_listp[i2c.id], i2c.addr);
			}
			break;
		default:
			printk("%s: not handle this command\n",__FUNCTION__);
			return -EINVAL;
			break;
	}

	return ret;
}


static const struct file_operations camera_fops = {
	.open = camera_open,
	.release = camera_close,
	.read = camera_read,
	.write = camera_write,
	.poll = camera_poll,
	.unlocked_ioctl = camera_ioctl,
	
};

static struct miscdevice camera_misdev =
{
	.minor = CAMERA_MINOR,
	.name =  CAMERA_DEVNAME,
	.fops = &camera_fops,
};

static int __init camera_init(void)
{
	int retval;
	
	retval = misc_register(&camera_misdev);
	if (retval < 0){
		printk("%s: register %s  misc cdev error\n", 
			__FUNCTION__, CAMERA_DEVNAME);
		return retval;
	}
	
	return 0;
}

static void __exit camera_exit(void)
{
	misc_deregister(&camera_misdev);
}

module_init(camera_init);
module_exit(camera_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Glen Sun <sunge@indrv.cn>");
MODULE_DESCRIPTION("simplified camera managment");

