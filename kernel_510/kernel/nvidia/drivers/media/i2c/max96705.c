/*
 * max96705.c - max96705 IO Expander driver
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

#include <media/camera_common.h>
#include <linux/module.h>
#include <media/max96705.h>

/* register specifics */
#define MAX96705_CTRL0_ADDR 0x04
#define MAX96705_CONFIG_ADDR 0x07

#define MAX96705_I2CA_SRC_ADDR 0x09
#define MAX96705_I2CA_DST_ADDR 0x0A

#define MAX96705_GPIO_EN_ADDR 0x0E
#define MAX96705_GPIO_OUT_ADDR 0x0F

#define MAX96705_DEV_ADDR 0x00
#define MAX96705_CHIPID_ADDR 0x1E
#define MAX96705_CHIP_ID 0x41
#define MAX96701_CHIP_ID 0x45

struct max96705_client_ctx {
	struct gmsl_link_ctx *g_ctx;
	bool st_done;
};

struct max96705 {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	struct max96705_client_ctx g_client;
	struct mutex lock;
	/* primary serializer properties */
	__u32 def_addr;
	__u32 pst2_ref;
	__u32 group;
	__u32 dser_num;
	bool frame_sync_en;
};

static struct max96705 *prim_priv__;
static struct max96705 *prim_priv1__;

struct map_ctx {
	u8 dt;
	u16 addr;
	u8 val;
	u8 st_id;
};

static int i2c_96705log_en = 1;
//static int max96705_write_reg(struct device *dev, u16 addr, u8 val)
int max96705_write_reg(struct device *dev, u16 addr, u8 val)
{
	struct max96705 *priv = dev_get_drvdata(dev);
	int err;

	addr &= 0xff; 
	if (i2c_96705log_en)
		dev_info(dev, "%s:i2c write , 0x%02x = %02x\n",__func__, addr, val);

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_err(dev, "%s:i2c write failed, 0x%x = %x\n",
			__func__, addr, val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(10000, 11000);

	return err;
}
EXPORT_SYMBOL(max96705_write_reg);

 int max96705_read_reg(struct device *dev,
	u16 addr, u8 *val)
{
	struct max96705 *priv;
	int err;
	u32 reg_val = 0;

	priv = dev_get_drvdata(dev);

	err = regmap_read(priv->regmap, addr, &reg_val);
	if (err)
		dev_err(dev,
		"%s:i2c read failed, 0x%x = XX\n",
		__func__, addr);

	*val = reg_val & 0xFF;

	if (i2c_96705log_en)
		dev_info(dev, "%s:i2c read , 0x%x = %x\n",__func__, addr, *val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(100, 110);

	return err;
}
EXPORT_SYMBOL(max96705_read_reg);

int max96705_setup_control(struct device *dev)
{
	struct max96705 *priv = dev_get_drvdata(dev);
	int err = 0;
	struct gmsl_link_ctx *g_ctx;
	u8 value = 0;

	dev_dbg(dev, "%s: enter\n", __func__);
	mutex_lock(&priv->lock);

	if (!priv->g_client.g_ctx) {
		dev_err(dev, "%s: no sdev client found\n", __func__);
		err = -EINVAL;
		goto error;
	}

	g_ctx = priv->g_client.g_ctx;

	/* update address reassingment */
	if(priv->group==0)
	{
		err=max96705_write_reg(&prim_priv__->i2c_client->dev,
		MAX96705_DEV_ADDR, (g_ctx->ser_reg << 1));
		if(err<0){
            prim_priv__->i2c_client->addr=0x42;
			err = max96705_write_reg(&prim_priv__->i2c_client->dev,
        		MAX96705_DEV_ADDR, (g_ctx->ser_reg << 1));
		    prim_priv__->i2c_client->addr=0x40;
        }
	}

	if(priv->group==1)
	{
		err=max96705_write_reg(&prim_priv1__->i2c_client->dev,
		MAX96705_DEV_ADDR, (g_ctx->ser_reg << 1));
		if(err<0){
            prim_priv1__->i2c_client->addr=0x42;         
			err = max96705_write_reg(&prim_priv1__->i2c_client->dev,
        			MAX96705_DEV_ADDR, (g_ctx->ser_reg << 1));
		    prim_priv1__->i2c_client->addr=0x40;   
        }
	}

	max96705_write_reg(dev,MAX96705_CTRL0_ADDR,0x47); //Enable config link, 0x43?
	//usleep_range(50000, 51000);
	max96705_write_reg(dev,MAX96705_CONFIG_ADDR,0x84); //config HVEN=1 DBL=1 BWS=0
	msleep(50); /* delay to settle link */
	//usleep_range(50000, 51000);
	max96705_write_reg(dev,MAX96705_CTRL0_ADDR,0x87);  //Disable config link & Enable serialization
	msleep(50); /* delay to settle link */
	//usleep_range(50000, 51000);
	max96705_write_reg(dev,MAX96705_GPIO_EN_ADDR,0x02); // Enable GPIO_1(Camera RST pin)
	//usleep_range(50000, 51000);
	max96705_write_reg(dev,MAX96705_GPIO_OUT_ADDR,0x3e); // Disable GPO Set, GPIO_1 set High.
	//usleep_range(50000, 51000);
#if 1
	//check chip ID
	max96705_read_reg(dev,MAX96705_CHIPID_ADDR, &value);
	if (value == MAX96705_CHIP_ID||value == MAX96701_CHIP_ID)
	{
		g_ctx->serdev_found = true;
		dev_info(dev, "%s: Detect a camera linked!\n", __func__);
	}
	else {
		g_ctx->serdev_found = false;
		err = -EINVAL;
		dev_info(dev, "%s: Detect no camera linked!\n", __func__);
		goto error;
	}
#endif

	/* delay to settle link */
	//msleep(50);

	//max96705_write_reg(dev, MAX96705_I2CA_SRC_ADDR, (g_ctx->sdev_reg << 1));
	//max96705_write_reg(dev, MAX96705_I2CA_DST_ADDR, (g_ctx->sdev_def << 1));

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max96705_setup_control);

int max96705_reset_control(struct device *dev)
{
	struct max96705 *priv = dev_get_drvdata(dev);
	int err = 0;

	dev_dbg(dev, "%s: enter\n", __func__);
	mutex_lock(&priv->lock);

	if (!priv->g_client.g_ctx) {
		dev_err(dev, "%s: no sdev client found\n", __func__);
		err = -EINVAL;
		goto error;
	}


	priv->g_client.st_done = false;

	if(priv->group==0){
		prim_priv__->pst2_ref--;
		max96705_write_reg(dev, MAX96705_DEV_ADDR, (prim_priv__->def_addr << 1));
	}

	if(priv->group==1){
		prim_priv1__->pst2_ref--;
		max96705_write_reg(dev, MAX96705_DEV_ADDR, (prim_priv1__->def_addr << 1));
	}

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max96705_reset_control);

int max96705_sdev_pair(struct device *dev, struct gmsl_link_ctx *g_ctx)
{
	struct max96705 *priv;
	int err = 0;

	if (!dev || !g_ctx || !g_ctx->s_dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	priv = dev_get_drvdata(dev);
	mutex_lock(&priv->lock);
	if (priv->g_client.g_ctx) {
		dev_err(dev, "%s: device already paired\n", __func__);
		err = -EINVAL;
		goto error;
	}

	priv->g_client.st_done = false;

	priv->g_client.g_ctx = g_ctx;

error:
	mutex_unlock(&priv->lock);
	return 0;
}
EXPORT_SYMBOL(max96705_sdev_pair);

int max96705_sdev_unpair(struct device *dev, struct device *s_dev)
{
	struct max96705 *priv = NULL;
	int err = 0;

	if (!dev || !s_dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);

	if (!priv->g_client.g_ctx) {
		dev_err(dev, "%s: device is not paired\n", __func__);
		err = -ENOMEM;
		goto error;
	}

	if (priv->g_client.g_ctx->s_dev != s_dev) {
		dev_err(dev, "%s: invalid device\n", __func__);
		err = -EINVAL;
		goto error;
	}

	priv->g_client.g_ctx = NULL;
	priv->g_client.st_done = false;

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max96705_sdev_unpair);

static  struct regmap_config max96705_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

static int max96705_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max96705 *priv;
	int err = 0;
	struct device_node *node = client->dev.of_node;

	dev_dbg(&client->dev, "[MAX96705]: probing GMSL IO Expander\n");

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&max96705_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	mutex_init(&priv->lock);

	err = of_property_read_u32(node, "group", &priv->group);
	if (err < 0) {
		dev_err(&client->dev, "group not found\n");
		return -EINVAL;
	}

	if (of_get_property(node, "frame-sync-en", NULL)) {
		priv->frame_sync_en = true;
		dev_dbg(&client->dev, "frame-sync-en found\n");
	}else {
		priv->frame_sync_en = false;
		dev_dbg(&client->dev, "frame-sync-en not found\n");
	}

	if (of_get_property(node, "is-prim-ser", NULL)) {
		if (prim_priv__) {
			dev_err(&client->dev,
				"prim-ser already exists\n");
				return -EEXIST;
		}

		err = of_property_read_u32(node, "reg", &priv->def_addr);
		if (err < 0) {
			dev_err(&client->dev, "reg not found\n");
			return -EINVAL;
		}

		prim_priv__ = priv;
	}

	if (of_get_property(node, "is-prim-ser1", NULL)) {
		if (prim_priv1__) {
			dev_err(&client->dev,
				"prim-ser1 already exists\n");
				return -EEXIST;
		}

		err = of_property_read_u32(node, "reg", &priv->def_addr);
		if (err < 0) {
			dev_err(&client->dev, "reg not found\n");
			return -EINVAL;
		}
		prim_priv1__ = priv;
	}

	dev_set_drvdata(&client->dev, priv);

	dev_dbg(&client->dev, "%s:  success\n", __func__);

	return err;
}

static int max96705_remove(struct i2c_client *client)
{
	struct max96705 *priv;

	if (client != NULL) {
		priv = dev_get_drvdata(&client->dev);
		mutex_destroy(&priv->lock);
		i2c_unregister_device(client);
		client = NULL;
	}

	return 0;
}

static const struct i2c_device_id max96705_id[] = {
	{ "max96705", 0 },
	{ },
};

const struct of_device_id max96705_of_match[] = {
	{ .compatible = "nvidia,max96705", },
	{ },
};
MODULE_DEVICE_TABLE(of, max96705_of_match);
MODULE_DEVICE_TABLE(i2c, max96705_id);

static struct i2c_driver max96705_i2c_driver = {
	.driver = {
		.name = "max96705",
		.owner = THIS_MODULE,
	},
	.probe = max96705_probe,
	.remove = max96705_remove,
	.id_table = max96705_id,
};

static int __init max96705_init(void)
{
	return i2c_add_driver(&max96705_i2c_driver);
}

static void __exit max96705_exit(void)
{
	i2c_del_driver(&max96705_i2c_driver);
}

module_init(max96705_init);
module_exit(max96705_exit);

MODULE_DESCRIPTION("IO Expander driver max96705");
MODULE_AUTHOR("Sudhir Vyas <svyas@nvidia.com>");
MODULE_LICENSE("GPL v2");
