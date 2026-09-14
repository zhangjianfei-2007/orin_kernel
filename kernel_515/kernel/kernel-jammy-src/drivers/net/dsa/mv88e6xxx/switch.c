/*
 * simplified switch managment interface.
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
#include "chip.h"

extern struct mv88e6xxx_chip *switch_chip_listp[2];
extern int n_switch_chip;

typedef struct {
	unsigned int id; //chip id. for mv88e6352 of DCU, it's 0~1.
	unsigned int addr; //port address. e.g. 0x10, 0x15, 0x16, 0x1b, 0x1c and etc.
	unsigned int reg; //register address.
	unsigned int val; //normally it's a 16-bits register value.
}SWI_COMM_REG_TYPE;

#define SWITCH_MINOR MISC_DYNAMIC_MINOR
#define SWITCH_DEVNAME "switch"

#ifndef SWI_COMM_IOCTL_TYPE
#define	SWI_COMM_IOCTL_TYPE 's'
#endif

#ifndef	CMD_SET_SWI_REGISTER
#define	CMD_SET_SWI_REGISTER				_IOWR(SWI_COMM_IOCTL_TYPE, 0, SWI_COMM_REG_TYPE)
#endif

#ifndef	CMD_GET_SWI_REGISTER
#define	CMD_GET_SWI_REGISTER				_IOWR(SWI_COMM_IOCTL_TYPE, 1, SWI_COMM_REG_TYPE)
#endif

static int switch_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	return ret;
}

static int switch_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t switch_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	return 0;

}

static ssize_t switch_write(struct file *file, const char __user *buf,
			  size_t count, loff_t * ppos)
{
	return 0;
}

static unsigned int switch_poll(struct file *file,
				 struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	return mask;
}

static long switch_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	int addr = 0;
	int reg = 0;
	u16 val = 0;
	SWI_COMM_REG_TYPE swi;

	switch(cmd)
	{
		case CMD_SET_SWI_REGISTER:
			ret = copy_from_user(&swi, (void *)arg, sizeof(SWI_COMM_REG_TYPE));
			if (ret)
				break;
			addr = swi.addr;
			reg = swi.reg;
			val = swi.val;
			if(swi.id >= 0 && swi.id < 2)
				ret = mv88e6xxx_write(switch_chip_listp[swi.id], addr, reg, val);
			break;
		case CMD_GET_SWI_REGISTER:
			ret = copy_from_user(&swi, (void *)arg, sizeof(SWI_COMM_REG_TYPE));
			if (ret)
				break;
			addr = swi.addr;
			reg = swi.reg;
			if(swi.id >= 0 && swi.id < 2)
				ret = mv88e6xxx_read(switch_chip_listp[swi.id], addr, reg, &val);
			if (ret == 0) {
				swi.val = val;
				ret = copy_to_user((void*)arg, &swi, sizeof(SWI_COMM_REG_TYPE));
			}
			break;
		default:
			printk("%s: not handle this command\n",__FUNCTION__);
			return -EINVAL;
			break;
	}

	return ret;
}


static const struct file_operations switch_fops = {
	.open = switch_open,
	.release = switch_close,
	.read = switch_read,
	.write = switch_write,
	.poll = switch_poll,
	.unlocked_ioctl = switch_ioctl,

};

static struct miscdevice switch_misdev =
{
	.minor = SWITCH_MINOR,
	.name =  SWITCH_DEVNAME,
	.fops = &switch_fops,
};

static int __init switch_init(void)
{
	int retval;

	retval = misc_register(&switch_misdev);
	if (retval < 0){
		printk("%s: register %s  misc cdev error\n",
			__FUNCTION__, SWITCH_DEVNAME);
		return retval;
	}

	return 0;
}

static void __exit switch_exit(void)
{
	misc_deregister(&switch_misdev);
}

module_init(switch_init);
module_exit(switch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Glen Sun <sunge@indrv.cn>");
MODULE_DESCRIPTION("simplified switch managment");
