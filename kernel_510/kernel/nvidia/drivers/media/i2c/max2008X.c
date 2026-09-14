/*
 * max2008X.c - max2008X IO Expander driver
 *
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <media/camera_common.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>  
//#include <media/max2008X.h>

/* register specifics */
#define MAX2008X_IM_ADDR 0x00
#define MAX2008X_CFG_ADDR 0x01
#define MAX2008X_ID_ADDR 0x02
#define MAX2008X_ST1_ADDR 0x03
#define MAX2008X_ST2_U_ADDR 0x04
#define MAX2008X_ST2_L_ADDR 0x05
#define MAX2008X_ADC1_ADDR 0x06
#define MAX2008X_ADC2_ADDR 0x07
#define MAX2008X_ADC3_ADDR 0x08
#define MAX2008X_ADC4_ADDR 0x09

#define POWER_ON_DELAY_DEFAULT 500

static bool i2c_max2008X_log_en = 1;
#define OUTPUT_MAX 4 

struct max2008X {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	u32 out_ports;
	u32 out_en[OUTPUT_MAX];
	u32 power_on_delay;
};

enum {
	out_all_offset,
	out_1_offset,
	out_2_offset,
	out_3_offset,
	out_4_offset,
};

struct device *poc_dev_listp[3]={NULL};
EXPORT_SYMBOL(poc_dev_listp);
int n_poc_dev=0;
EXPORT_SYMBOL(n_poc_dev);

//static int max2008X_write_reg(struct device *dev,
 int max2008X_write_reg(struct device *dev,
	u8 addr, u8 val)
{
	struct max2008X *priv;
	int err;

	priv = dev_get_drvdata(dev);

	if (i2c_max2008X_log_en)
		dev_info(dev, "%s:i2c write , 0x%x = %x\n",__func__, addr, val);

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_err(dev,
		"%s:i2c write failed, 0x%x = %x\n",
		__func__, addr, val);

	return err;
}
EXPORT_SYMBOL(max2008X_write_reg);

 int max2008X_read_reg(struct device *dev,
	u8 addr, u8 *val)
{
	struct max2008X *priv;
	u32 reg_val = 0;
	int err;

	priv = dev_get_drvdata(dev);

	//err = regmap_read(priv->regmap, addr, &reg_val);
	err = regmap_raw_read(priv->regmap, addr, &reg_val, 1);
	if (err){
		dev_err(dev,
		"%s:i2c read failed, 0x%x = %x\n",
		__func__, addr, *val);
		return err;
	}
	*val = reg_val & 0xFF;

	if (i2c_max2008X_log_en)
		dev_info(dev, "%s:i2c read , 0x%x = %x\n",__func__, addr, *val);

	return err;
}
EXPORT_SYMBOL(max2008X_read_reg);

int max2008X_get_chipID(struct device *dev, u8 *chipID)
{
	u8 value = 0;
	int err = 0;

	err = max2008X_read_reg(dev, MAX2008X_ID_ADDR, &value);
	if (err) {
		goto ret;
	}
	*chipID = value;
	dev_dbg(dev, "%s: chipID = %d, version = %d\n",__func__, (value & 0x30)>> 4, (value & 0x0F));

ret:
	return err;
}


int max2008X_out_en(struct device *dev, u8 index, bool en)
{
	u8 value = 0;
	int err = 0;

	err = max2008X_read_reg(dev, MAX2008X_CFG_ADDR, &value);
	if (err) {
		goto ret;
	}

	switch(index){
		case out_1_offset:
		case out_2_offset:
		case out_3_offset:
		case out_4_offset:
			if (en) {
				err = max2008X_write_reg(dev, MAX2008X_CFG_ADDR, value|(1<<(index-1)));
			}else {
				err = max2008X_write_reg(dev, MAX2008X_CFG_ADDR, value & ~(1<<(index-1)));
			}
			break;
		case out_all_offset:
			if (en){
				err = max2008X_write_reg(dev, MAX2008X_CFG_ADDR, value|0x0F);
			}else {
				err = max2008X_write_reg(dev, MAX2008X_CFG_ADDR, value&0xF0);
			}
			break;
	}

ret:
	return err;
}
EXPORT_SYMBOL(max2008X_out_en);

/*
 * The  MAX20087/MAX20089  ICs  are  ASIL  B  compliant  at
 * the  hardware  level.  This  means  ASIL  B  compliance  is
 * achieved  without  any  additional  external  circuits  or
 * software processing. For ASIL D compliance for safety-critical
 * applications, the MCU may need to use the ADC readings
 * to increase fault coverage and verify that the MAX20087/
 * MAX20089 ICs and connected camera sensors are operating within their specifications.
 */
int max2008X_read_current(struct device *dev, u8 index, u16 *current_ma)
{
	u8 value = 0;
	int err = 0;

	//todo: check MUX SETTING
	//err = max2008X_read_reg(dev, MAX2008X_CFG_ADDR, &value);

	switch(index){
		case out_1_offset:
			err = max2008X_read_reg(dev, MAX2008X_ADC1_ADDR, &value);
			break;
		case out_2_offset:
			err = max2008X_read_reg(dev, MAX2008X_ADC2_ADDR, &value);
			break;
		case out_3_offset:
			err = max2008X_read_reg(dev, MAX2008X_ADC3_ADDR, &value);
			break;
		case out_4_offset:
			err = max2008X_read_reg(dev, MAX2008X_ADC4_ADDR, &value);
			break;
	}

	*current_ma = value*3;

	return err;
}
EXPORT_SYMBOL(max2008X_read_current);

const struct of_device_id max2008X_of_match[] = {
	{ .compatible = "HYZX,max20087", },
	{ },
};
MODULE_DEVICE_TABLE(of, max2008X_of_match);

static int max2008X_parse_dt(struct max2008X *priv,
				struct i2c_client *client)
{
	struct device_node *node = client->dev.of_node;
	int err = 0;
	int value = 0;
	int cnt;
	int  i;
	const struct of_device_id *match;

	if (!node)
		return -EINVAL;

	match = of_match_device(max2008X_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return -EFAULT;
	}

	err = of_property_read_u32(node, "out-ports", &value);
	if (err < 0) {
		dev_err(&client->dev, "No out-ports info\n");
		return err;
	}
	priv->out_ports= value;
	dev_dbg(&client->dev, "out_ports = %d \n", priv->out_ports);

	value = 0;
	err = of_property_read_u32(node, "power-on-delay", &value);
	if (err < 0) {
		dev_err(&client->dev, "No power-on-delay info\n");
		//return err;
	}
	priv->power_on_delay = value;
	if (!priv->power_on_delay)
		priv->power_on_delay= POWER_ON_DELAY_DEFAULT;
	dev_info(&client->dev, "power_on_delay = %d \n", priv->power_on_delay);

	cnt = of_property_count_u32_elems(node, "out-boot-on");
	if (cnt <= 0) {
		dev_err(&client->dev, "No out-boot-on found\n");
		err = -EINVAL;
	}else if (cnt > OUTPUT_MAX){
		dev_err(&client->dev, "out-boot-on info error\n");
		err = -EINVAL;
	}

	for (i = 0; i < cnt && i< OUTPUT_MAX; i++) {
		err = of_property_read_u32_index(node, "out-boot-on", i, &priv->out_en[i]);
		if (err < 0) {
			dev_err(&client->dev, "invalid out-boot-on info\n");
			//goto error;
		}
	}
	dev_info(&client->dev, "out-boot-on[%d]:%d %d %d %d\n", 
					cnt, priv->out_en[0], priv->out_en[1], priv->out_en[2], priv->out_en[3]);

	return 0;
}

static struct regmap_config max2008X_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	//.cache_type = REGCACHE_RBTREE,
	.cache_type = REGCACHE_NONE,
};

static int max2008X_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max2008X *priv;
	u8 ic_info = 0;
	int i=0;
	int err=0;

	dev_info(&client->dev, "%s:  start\n", __func__);

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&max2008X_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	err = max2008X_parse_dt(priv, client);
	if (err) {
		dev_err(&client->dev, "unable to parse dt\n");
		//return -EFAULT;
	}

	dev_set_drvdata(&client->dev, priv);

	max2008X_get_chipID(&client->dev, &ic_info);

	max2008X_out_en(&client->dev, out_all_offset, 0);
	msleep(priv->power_on_delay);
	for(i = 0; i < priv->out_ports; i++) {
		max2008X_out_en(&client->dev, i+1, priv->out_en[i]);
	}

	dev_info(&client->dev, "%s:  success\n", __func__);

	if(n_poc_dev < 3){
		poc_dev_listp[n_poc_dev]=&client->dev;
		n_poc_dev++;
	}

	return err;
}


static int max2008X_remove(struct i2c_client *client)
{
	struct max2008X *priv;

	max2008X_out_en(&client->dev, out_all_offset, 0);
	if (client != NULL) {
		priv = dev_get_drvdata(&client->dev);
		i2c_unregister_device(client);
		client = NULL;
	}

	return 0;
}

static const struct i2c_device_id max2008X_id[] = {
	{ "max20086", 0 },
	{ "max20087", 0 },
	{ "max20088", 0 },
	{ "max20089", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, max2008X_id);

static struct i2c_driver max2008X_i2c_driver = {
	.driver = {
		.name = "max2008X",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max2008X_of_match),
	},
	.probe = max2008X_probe,
	.remove = max2008X_remove,
	.id_table = max2008X_id,
};

static int __init max2008X_init(void)
{
	return i2c_add_driver(&max2008X_i2c_driver);
}

static void __exit max2008X_exit(void)
{
	i2c_del_driver(&max2008X_i2c_driver);
}

module_init(max2008X_init);
module_exit(max2008X_exit);

MODULE_DESCRIPTION("Dual/Quad Camera Power Protectors driver max20086-20089");
MODULE_AUTHOR("limiao <361995459@qq.com");
MODULE_LICENSE("GPL v2");
