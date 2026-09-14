/*
 * gw5200.c - gw5200 sensor driver
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

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/max9295.h>
#include <media/max9296.h>
#include <media/max96712.h>

#include <media/tegra_v4l2_camera.h>
#include <media/camera_common.h>
#include "gw5200_mode_tbls.h"

#define GW5200_DEFAULT_MODE	GW5200_MODE_1920X1080_30FPS

#define GW5200_DEFAULT_DATAFMT MEDIA_BUS_FMT_YUYV8_2X8

#define GW5200_MIN_FRAME_LENGTH (1125)
#define GW5200_MAX_FRAME_LENGTH (1125)
#define GW5200_DEFAULT_FRAME_LENGTH    (1125)

/* default image output width */
#define GW5200_DEFAULT_WIDTH    1920
/* default image output height */
#define GW5200_DEFAULT_HEIGHT    1080

/* default output clk frequency for camera */
#define GW5200_DEFAULT_CLK_FREQ    27000000

#define DESER_TOTAL 4
static int ser_linked_map[DESER_TOTAL]= {0};

#define __MUTEX_LOCK__
#ifdef __MUTEX_LOCK__
struct gw5200_mutex_lock {
	int mutex_cnt;
	struct mutex stream_lock;
};
static struct gw5200_mutex_lock gw5200_mutex_lock[DESER_TOTAL];
#endif

struct gw5200 {
	struct camera_common_power_rail	power;
	int	numctrls;
	struct v4l2_ctrl_handler	ctrl_handler;
	struct i2c_client	*i2c_client;
	const struct i2c_device_id *id;
	struct v4l2_subdev	*subdev;
	struct device		*ser_dev;
	struct device		*dser_dev;
	struct gmsl_link_ctx	g_ctx;
	struct media_pad	pad;
	u32	frame_length;
	u32 dser_num;
	u32 dser_type;
	u32 sensor_model;
	u32 pass_9295;
	struct regmap	*regmap;
	struct camera_common_data	*s_data;
	struct camera_common_pdata	*pdata;
	struct v4l2_ctrl		*ctrls[];
};

static u32 pass;
static u32 link_ctl;

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	//.cache_type = REGCACHE_RBTREE,
	.cache_type = REGCACHE_NONE,
};

static int gw5200_s_ctrl(struct v4l2_ctrl *ctrl);

static const struct v4l2_ctrl_ops gw5200_ctrl_ops = {
	.s_ctrl = gw5200_s_ctrl,
};

static struct v4l2_ctrl_config ctrl_config_list[] = {
/* Do not change the name field for the controls! */
	{
		.ops = &gw5200_ctrl_ops,
		.id = TEGRA_CAMERA_CID_FRAME_LENGTH,
		.name = "Frame Length",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = GW5200_MIN_FRAME_LENGTH,
		.max = GW5200_MAX_FRAME_LENGTH,
		.def = GW5200_DEFAULT_FRAME_LENGTH,
		.step = 1,
	},
};

struct camera_common_data *sen_5200_listp[12]={NULL};
EXPORT_SYMBOL(sen_5200_listp);
int n_5200_dev=0;
EXPORT_SYMBOL(n_5200_dev);
int des_dev_type=DES_MAX9296;
EXPORT_SYMBOL(des_dev_type);

static int test_mode;
module_param(test_mode, int, 0644);
static int delay1 = 10;
module_param(delay1, int, 0644);
static int delay2 = 1000;
module_param(delay2, int, 0644);
static int delay3 = 150;
module_param(delay3, int, 0644);

int gw5200_read_reg(struct camera_common_data *s_data,
				u16 addr, u8 *val)
{
	struct gw5200 *priv = (struct gw5200 *)s_data->priv;
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(priv->regmap, addr, &reg_val);
	if (err){
		dev_err(&priv->i2c_client->dev, "%s:i2c read failed, 0x%x = XX\n", __func__, addr);
		return err;
	}
	*val = reg_val & 0xFF;

	dev_dbg(&priv->i2c_client->dev, "%s:i2c read , 0x%x = %x\n",
			__func__, addr, *val);

	return err;
}
EXPORT_SYMBOL(gw5200_read_reg);

int gw5200_write_reg(struct camera_common_data *s_data,
				u16 addr, u8 val)
{
	int err;
	struct gw5200 *priv = (struct gw5200 *)s_data->priv;

	dev_dbg(&priv->i2c_client->dev, "%s:i2c write , 0x%x = %x\n",__func__, addr, val);

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		pr_err("%s:i2c write failed, 0x%x = %x\n",
			__func__, addr, val);

	return err;
}
EXPORT_SYMBOL(gw5200_write_reg);

#if 0
static int gw5200_write_table(struct gw5200 *priv,
				const gw5200_reg table[])
{
	return regmap_util_write_table_8(priv->regmap,
					 table,
					 NULL, 0,
					 GW5200_TABLE_WAIT_MS,
					 GW5200_TABLE_END);
}
#endif

static struct mutex serdes_lock__;

static int gw5200_gmsl_serdes_setup(struct gw5200 *priv)
{
	int err = 0;
	struct device *dev;

	if (!priv || !priv->ser_dev || !priv->dser_dev || !priv->i2c_client)
		return -EINVAL;

	dev = &priv->i2c_client->dev;

	mutex_lock(&serdes_lock__);

	/* For now no separate power on required for serializer device */
	if (priv->dser_type == DES_MAX9296) {
		max9296_power_on(priv->dser_dev);
	} else if (priv->dser_type == DES_MAX96712) {
		max96712_power_on(priv->dser_dev);
	} else {
		//other
	}

	/* setup serdes addressing and control pipeline */
	if (priv->dser_type == DES_MAX9296) {
		err = max9296_setup_link(priv->dser_dev, &priv->i2c_client->dev);
	} else if (priv->dser_type == DES_MAX96712) {
		err = max96712_setup_link(priv->dser_dev, &priv->i2c_client->dev);
	} else {
		//other
	}

	if (err) {
		dev_err(dev, "gmsl deserializer link config failed\n");
		goto ret;
	}

	if(priv->pass_9295==1&&pass==1)
	{
		pass=2;
		err = max9295_setup_control(priv->ser_dev);
		if (err) {
			dev_err(dev, "gmsl serializer setup failed\n");
			if (priv->dser_type == DES_MAX9296) {
				max9296_set_tx_rate_3g(priv->dser_dev);
			} else if (priv->dser_type == DES_MAX96712) {
				max96712_set_tx_rate_3g(priv->dser_dev);
			} else {
				//other
			}
			goto ret;
		}else{
			//fix it
			if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
				ser_linked_map[priv->dser_num] |= 1 << 0;
			}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_B){
				ser_linked_map[priv->dser_num] |= 1 << 1;
			}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_C){
				ser_linked_map[priv->dser_num] |= 1 << 2;
			}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_D){
				ser_linked_map[priv->dser_num] |= 1 << 3;
			}
			//ser_linked_map[priv->dser_num] |= 1 << (priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A? 0:1);
		}
	}else if(priv->pass_9295!=1)
	{
		err = max9295_setup_control(priv->ser_dev);
		if (err) {
			dev_err(dev, "gmsl serializer setup failed\n");
			if (priv->dser_type == DES_MAX9296) {
				max9296_set_tx_rate_3g(priv->dser_dev);
			} else if (priv->dser_type == DES_MAX96712) {
				max96712_set_tx_rate_3g(priv->dser_dev);
			} else {
				//other
			}
			goto ret;
		}else{
			//fix it
			if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
				ser_linked_map[priv->dser_num] |= 1 << 0;
			}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_B){
				ser_linked_map[priv->dser_num] |= 1 << 1;
			}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_C){
				ser_linked_map[priv->dser_num] |= 1 << 2;
			}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_D){
				ser_linked_map[priv->dser_num] |= 1 << 3;
			}
			//ser_linked_map[priv->dser_num] |= 1 << (priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A? 0:1);
		}
	}

	if (priv->dser_type == DES_MAX9296) {
		err = max9296_setup_control(priv->dser_dev);
	} else if (priv->dser_type == DES_MAX96712) {
		err = max96712_setup_control(priv->dser_dev);
	} else {
		//other
	}

	if (err) {
		dev_err(dev, "gmsl deserializer setup failed\n");
		goto ret;
	}

ret:
	if (priv->dser_type == DES_MAX9296) {
		if (priv->sensor_model == SENSOR_E003A_YUV_3G || priv->sensor_model == SENSOR_E003A_YUV_ET) {
			dev_dbg(dev, " %s: skip max9296 link splitter. \n", __func__);
//		}else if (priv->sensor_model == SENSOR_F008AX_YUV_ET){
//			if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
//				max9296_link_splitter(priv->dser_dev, 1);
//			}else{
//				max9296_link_splitter(priv->dser_dev, 2);
//			}
		}else{
			max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
		}
	} else if (priv->dser_type == DES_MAX96712) {
		//fix it
		max96712_link_splitter(priv->dser_dev, 0);
	} else {
		//other
	}
	mutex_unlock(&serdes_lock__);
	return 0;
}

static void gw5200_gmsl_serdes_reset(struct gw5200 *priv)
{
	mutex_lock(&serdes_lock__);

	/* reset serdes addressing and control pipeline */
	max9295_reset_control(priv->ser_dev);

	if (priv->dser_type == DES_MAX9296) {
		max9296_reset_control(priv->dser_dev, &priv->i2c_client->dev);
		max9296_power_off(priv->dser_dev);
	} else if (priv->dser_type == DES_MAX96712) {
		max96712_reset_control(priv->dser_dev, &priv->i2c_client->dev);
		max96712_power_off(priv->dser_dev);
	} else {
		//other
	}

	mutex_unlock(&serdes_lock__);
}

static int gw5200_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct gw5200 *priv = (struct gw5200 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	dev_dbg(&priv->i2c_client->dev, "%s: power on\n", __func__);
	if (priv->pdata && priv->pdata->power_on) {
		err = priv->pdata->power_on(pw);
		if (err)
			pr_err("%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	pw->state = SWITCH_ON;

	return 0;
}

static int gw5200_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct gw5200 *priv = (struct gw5200 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	dev_dbg(&priv->i2c_client->dev, "%s:\n", __func__);

	if (priv->pdata && priv->pdata->power_off) {
		err = priv->pdata->power_off(pw);
		if (!err)
			goto power_off_done;
		else
			pr_err("%s failed.\n", __func__);
		return err;
	}

power_off_done:
	pw->state = SWITCH_OFF;

	return 0;
}

static int gw5200_power_get(struct gw5200 *priv)
{
	struct camera_common_power_rail *pw = &priv->power;
	const char *mclk_name;
	const char *parentclk_name;
	struct clk *parent;
	int err = 0;

	mclk_name = priv->pdata->mclk_name ?
		    priv->pdata->mclk_name : "cam_mclk1";
	pw->mclk = devm_clk_get(&priv->i2c_client->dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(&priv->i2c_client->dev,
			"unable to get clock %s\n", mclk_name);
		return PTR_ERR(pw->mclk);
	}

	parentclk_name = priv->pdata->parentclk_name;
	if (parentclk_name) {
		parent = devm_clk_get(&priv->i2c_client->dev, parentclk_name);
		if (IS_ERR(parent)) {
			dev_err(&priv->i2c_client->dev,
				"unable to get parent clock %s",
				parentclk_name);
		} else
			clk_set_parent(pw->mclk, parent);
	}

	pw->state = SWITCH_OFF;

	return err;
}


static int gw5200_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct gw5200 *priv = (struct gw5200 *)s_data->priv;
	int err;
	u8 value=0;

	dev_info(&client->dev, "%s++ fmt %d enable %d\n", __func__, s_data->mode, enable);

#ifdef __MUTEX_LOCK__
	dev_dbg(&client->dev, "%s: @@@Try the lock! \n", __func__);
	mutex_lock(&gw5200_mutex_lock[priv->dser_num].stream_lock);
	dev_dbg(&client->dev, "%s: @@@Get the lock! \n", __func__);
#endif
#if 1
	if (priv->dser_type == DES_MAX9296) {
		if (priv->sensor_model == SENSOR_E003A_YUV_3G || priv->sensor_model == SENSOR_E003A_YUV_ET) {
			if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
				max9296_read_reg(priv->dser_dev, 0x0010, &value);
				if((value & 0x3) != 0x3){
					//if splitter mode has not been enabled yet, for linkA, do not enable it.
					max9296_link_splitter(priv->dser_dev, 1);
				}
			}else{
				max9296_link_splitter(priv->dser_dev, 2);
			}
			msleep(delay3);
		}else{
			if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
					if(priv->pass_9295==1)
					{
						if(link_ctl==0)
						{
							link_ctl++;
							max9296_write_reg(priv->dser_dev,0x10,0x01);
							max9296_write_reg(priv->dser_dev,0x10,0x21);
						}else if(link_ctl==1)
						{
							link_ctl++;
						}
					}else
					{
						max9296_write_reg(priv->dser_dev,0x10,0x01);
						max9296_write_reg(priv->dser_dev,0x10,0x21);						
					}
			}else{
					if(priv->pass_9295==1)
					{
						if(link_ctl==0)
						{
							link_ctl++;
							max9296_write_reg(priv->dser_dev,0x10,0x01);
							max9296_write_reg(priv->dser_dev,0x10,0x21);
						}else if(link_ctl==1)
						{
							link_ctl++;
						}
					}else
					{
						max9296_write_reg(priv->dser_dev,0x10,0x02);
						max9296_write_reg(priv->dser_dev,0x10,0x22);						
					}
			}
			msleep(delay3);
		}
	} else if (priv->dser_type == DES_MAX96712) {
		if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
			max96712_link_splitter(priv->dser_dev, 1);
		}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_B){
			max96712_link_splitter(priv->dser_dev, 2);
		}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_C){
			max96712_link_splitter(priv->dser_dev, 4);
		}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_D){
			max96712_link_splitter(priv->dser_dev, 8);
		}
		msleep(delay3);
	} else {
		//other
	}
#endif
	if (!enable) {
		link_ctl--;
		/* disable serdes streaming */
		if (priv->g_ctx.frame_sync_en) {
			if (priv->sensor_model == SENSOR_E003A_YUV_3G || priv->sensor_model == SENSOR_E003A_YUV_ET) {
				max9295_write_reg(priv->ser_dev, 0x02D3,0x00);  //MFP7/GPIO7
			}else if(priv->sensor_model == SENSOR_F008AX_YUV_ET){
				//max9295_write_reg(priv->ser_dev, 0x02D6,0x00);	//MFP8/GPIO8
				max9295_write_reg(priv->ser_dev, 0x02D3,0x00);  //MFP7/GPIO7
			}else{
				max9295_write_reg(priv->ser_dev, 0x02BE,0x00);  //MFP0/GPIO0
			}
		}else {
			if (priv->sensor_model == SENSOR_E003A_YUV_3G || priv->sensor_model == SENSOR_E003A_YUV_ET) {
				max9295_write_reg(priv->ser_dev, 0x02D6,0x00);	//MFP8/GPIO8
				max9295_write_reg(priv->ser_dev, 0x02D3,0x00);	//MFP7/GPIO7
			}else if(priv->sensor_model == SENSOR_F008AX_YUV_ET){
				//max9295_write_reg(priv->ser_dev, 0x02D6,0x00);	//MFP8/GPIO8
				//max9295_write_reg(priv->ser_dev, 0x02D3,0x00);	//MFP7/GPIO7
			}else if(priv->sensor_model == SENSOR_IMX490_YUV_SG_MFP8 || priv->sensor_model == SENSOR_AR0820_YUV_SG){
				max9295_write_reg(priv->ser_dev, 0x02D6,0x00);	//MFP8/GPIO8
				max9295_write_reg(priv->ser_dev, 0x02BE,0x00);	//MFP0/GPIO0
			}else{
				max9295_write_reg(priv->ser_dev, 0x02D3,0x00);	//MFP7/GPIO7
				max9295_write_reg(priv->ser_dev, 0x02BE,0x00);	//MFP0/GPIO0
			}

		}
		if (priv->dser_type == DES_MAX9296) {
			max9296_stop_streaming(priv->dser_dev, &client->dev);
			max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
		} else if (priv->dser_type == DES_MAX96712) {
			max96712_stop_streaming(priv->dser_dev, &client->dev);
			max96712_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
		} else {
			//other
		}
	#ifdef __MUTEX_LOCK__
		mutex_unlock(&gw5200_mutex_lock[priv->dser_num].stream_lock);
		dev_dbg(&client->dev, "%s: @@@RLS the lock! \n", __func__);
	#endif
		return 0;
	}

	/* enable serdes streaming */
	if(priv->pass_9295==1&&pass==2)
	{
		pass=3;
		err = max9295_setup_streaming(priv->ser_dev);
		if (err)
			goto exit;
	}else if(priv->pass_9295!=1)
	{
		err = max9295_setup_streaming(priv->ser_dev);
		if (err)
			goto exit;
	}
	if (priv->dser_type == DES_MAX9296) {
		err = max9296_setup_streaming(priv->dser_dev, &client->dev);
	} else if (priv->dser_type == DES_MAX96712) {
		err = max96712_setup_streaming(priv->dser_dev, &client->dev);
	} else {
		//other
	}
	if (err)
		goto exit;
	if (priv->dser_type == DES_MAX9296) {
		err = max9296_start_streaming(priv->dser_dev, &client->dev);
	} else if (priv->dser_type == DES_MAX96712) {
		err = max96712_start_streaming(priv->dser_dev, &client->dev);
	} else {
		//other
	}
	if (err)
		goto exit;

	if (priv->g_ctx.frame_sync_en) {
		if (priv->sensor_model == SENSOR_E003A_YUV_3G || priv->sensor_model == SENSOR_E003A_YUV_ET) {
			err = max9295_write_reg(priv->ser_dev, 0x02D3,0x10);   //MFP7/GPIO7
		}else if(priv->sensor_model == SENSOR_F008AX_YUV_ET){
			err = max9295_write_reg(priv->ser_dev, 0x02D3,0x10);   //MFP7/GPIO7
			//err = max9295_write_reg(priv->ser_dev, 0x02D6,0x00);
		}else{
#if defined(MFP7_MFP8_COMPAT)
			if (priv->dser_type == DES_MAX9296) {
				/*GPIO7*/
				max9295_write_reg(priv->ser_dev, 0x2D3, 0x84);
				//implement settings in app/script.
				//max9295_write_reg(priv->ser_dev, 0x2D5, 0x07);
				/*GPIO8*/
				max9295_write_reg(priv->ser_dev, 0x2D6, 0x84);
				//implement settings in app/script.
				//max9295_write_reg(priv->ser_dev, 0x2D8, 0x07);
			} else if (priv->dser_type == DES_MAX96712) {
				//todo
				//implement settings in app/script.
			}
#endif

			err = max9295_write_reg(priv->ser_dev, 0x02BE,0x10);   //MFP0/GPIO0
		}
	}else {
		if (priv->sensor_model == SENSOR_E003A_YUV_3G || priv->sensor_model == SENSOR_E003A_YUV_ET) {
			err = max9295_write_reg(priv->ser_dev, 0x02D3,0x10);   //MFP7/GPIO7
		}else if(priv->sensor_model == SENSOR_F008AX_YUV_ET){
			err = max9295_write_reg(priv->ser_dev, 0x02D3,0x10);   //MFP7/GPIO7
		}else{
			if(priv->pass_9295!=1)
			{
				err = max9295_write_reg(priv->ser_dev, 0x02BE,0x10);   //MFP0/GPIO0
			}
		}

		if (priv->sensor_model == SENSOR_E003A_YUV_3G || priv->sensor_model == SENSOR_E003A_YUV_ET) {
			msleep(delay1);
			max9295_write_reg(priv->ser_dev, 0x02D6,0x00);
			msleep(600);
			max9295_write_reg(priv->ser_dev, 0x02D6,0x10);		   //MFP8/GPIO8
		}else if (priv->sensor_model == SENSOR_IMX390_YUV_SG || priv->sensor_model == SENSOR_IMX390_YUV_3G) {
			msleep(delay1);
			max9295_write_reg(priv->ser_dev, 0x02D3,0x00);
			msleep(600);
			max9295_write_reg(priv->ser_dev, 0x02D3,0x10);		   //MFP7/GPIO7
		}else if (priv->sensor_model == SENSOR_IMX490_YUV_SG) {
			msleep(delay1);
			max9295_write_reg(priv->ser_dev, 0x02D3,0x10);
			msleep(delay2);
			max9295_write_reg(priv->ser_dev, 0x02D3,0x00);	   //MFP7/GPIO7
		}else if (priv->sensor_model == SENSOR_IMX490_YUV_SG_MFP8) {
			msleep(delay1);
			max9295_write_reg(priv->ser_dev, 0x02D6,0x10);
			msleep(delay2);
			max9295_write_reg(priv->ser_dev, 0x02D6,0x00);	   //MFP8/GPIO8
		}else if (priv->sensor_model == SENSOR_AR0820_YUV_SG) {
			max9295_write_reg(priv->ser_dev, 0x02D6,0x10);
		}else if(priv->sensor_model == SENSOR_OX08BC_YUV_SG){
			max9295_write_reg(priv->ser_dev, 0x02D6,0x00);
			msleep(600);
			max9295_write_reg(priv->ser_dev, 0x02D6,0x10);	
		}else if (priv->sensor_model == SENSOR_F008AX_YUV_ET){
//			usleep_range(900000, 1000000);
//			max9295_write_reg(priv->ser_dev, 0x02D6,0x00);
//			usleep_range(10000, 11000);
//			max9295_write_reg(priv->ser_dev, 0x02D6,0x10);		   //MFP8/GPIO8
//			usleep_range(1900, 2000);
//			max9295_write_reg(priv->ser_dev, 0x02D6,0x00);
//			usleep_range(10000, 11000);
//			max9295_write_reg(priv->ser_dev, 0x02D6,0x10);
		}else {
			max9295_write_reg(priv->ser_dev, 0x02D3,0x10);		   //MFP7/GPIO7
		}
	}
	if (priv->dser_type == DES_MAX9296) {
		//for entron's e003a, if linkB have not started, must disable splitter mode before linkA started, otherwise, linkA will fail.
		if (priv->sensor_model == SENSOR_E003A_YUV_3G || priv->sensor_model == SENSOR_E003A_YUV_ET) {
			if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
				max9296_read_reg(priv->dser_dev, 0x0010, &value);
				if((value & 0x3) != 0x3){
					//if splitter mode has not been enabled yet, for linkA, do not enable it.
					max9296_link_splitter(priv->dser_dev, 1);
				}else{
					//if splitter mode has been enabled, do not disable it.
					max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
				}
			}else{
				max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
			}
		//disable splitter mode for 8MP camera. (solved by boost vi clock)
//		}else if(priv->sensor_model == SENSOR_F008AX_YUV_ET){
//			if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
//				max9296_link_splitter(priv->dser_dev, 1);
//			}else{
//				max9296_link_splitter(priv->dser_dev, 2);
//			}
		}else{
			max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
		}
	} else if (priv->dser_type == DES_MAX96712) {
		//max96712_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	} else {
		//other
	}
	if (err)
			goto exit;

#ifdef __MUTEX_LOCK__
	mutex_unlock(&gw5200_mutex_lock[priv->dser_num].stream_lock);
	dev_dbg(&client->dev, "%s: @@@RLS the lock! \n", __func__);
#endif
	return 0;

exit:
	dev_err(&client->dev, "%s: error setting stream\n", __func__);
#ifdef __MUTEX_LOCK__
	mutex_unlock(&gw5200_mutex_lock[priv->dser_num].stream_lock);
	dev_dbg(&client->dev, "%s: @@@RLS the lock! \n", __func__);
#endif

	return 0;
}

static int gw5200_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct gw5200 *priv = (struct gw5200 *)s_data->priv;
	struct camera_common_power_rail *pw = &priv->power;

	*status = pw->state == SWITCH_ON;
	return 0;
}

static struct v4l2_subdev_video_ops gw5200_subdev_video_ops = {
	.s_stream	= gw5200_s_stream,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	.g_mbus_config = camera_common_g_mbus_config,
#endif
	.g_input_status = gw5200_g_input_status,
};

static struct v4l2_subdev_core_ops gw5200_subdev_core_ops = {
	.s_power	= camera_common_s_power,
};

static int gw5200_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	return camera_common_g_fmt(sd, &format->format);
}

static int gw5200_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *format)
{
	int ret;
	struct camera_common_data *s_data = to_camera_common_data(sd->dev);
	struct gw5200 *priv = (struct gw5200 *)s_data->priv;

	//
	if(priv->sensor_model == SENSOR_F008AX_YUV_ET){
		format->format.width = 3840;
		format->format.height = 2165;
		s_data->def_width = 3840;
		s_data->def_height = 2165;
		dev_dbg(&priv->i2c_client->dev, "%s: set format for f008ax.\n", __func__);
	}

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		ret = camera_common_try_fmt(sd, &format->format);
	else
		ret = camera_common_s_fmt(sd, &format->format);

	return ret;
}

static struct v4l2_subdev_pad_ops gw5200_subdev_pad_ops = {
	.set_fmt = gw5200_set_fmt,
	.get_fmt = gw5200_get_fmt,
	.enum_mbus_code = camera_common_enum_mbus_code,
	.enum_frame_size	= camera_common_enum_framesizes,
	.enum_frame_interval	= camera_common_enum_frameintervals,
};

static struct v4l2_subdev_ops gw5200_subdev_ops = {
	.core	= &gw5200_subdev_core_ops,
	.video	= &gw5200_subdev_video_ops,
	.pad = &gw5200_subdev_pad_ops,
};

const struct of_device_id gw5200_of_match[] = {
	{ .compatible = "nvidia,gw5200",},
	{ .compatible = "nvidia,gw5300",},
	{ .compatible = "nvidia,gw5X00",},
	{ },
};

static struct camera_common_sensor_ops gw5200_common_ops = {
	.power_on = gw5200_power_on,
	.power_off = gw5200_power_off,
	.write_reg = gw5200_write_reg,
	.read_reg = gw5200_read_reg,
};

static int gw5200_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gw5200 *priv =
		container_of(ctrl->handler, struct gw5200, ctrl_handler);
	int err = 0;

	if (priv->power.state == SWITCH_OFF)
		return 0;

	switch (ctrl->id) {
	case TEGRA_CAMERA_CID_FRAME_LENGTH:
			break;
	default:		
		pr_err("%s: unknown ctrl id.\n", __func__);
		return -EINVAL;
	}


	return err;
}


static int gw5200_ctrls_init(struct gw5200 *priv)
{
	struct i2c_client *client = priv->i2c_client;
	struct v4l2_ctrl *ctrl;
	int num_ctrls;
	int err;
	int i;

	dev_info(&client->dev, "%s++\n", __func__);

	num_ctrls = ARRAY_SIZE(ctrl_config_list);
	v4l2_ctrl_handler_init(&priv->ctrl_handler, num_ctrls);

	for (i = 0; i < num_ctrls; i++) {
		ctrl = v4l2_ctrl_new_custom(&priv->ctrl_handler,
			&ctrl_config_list[i], NULL);
		if (ctrl == NULL) {
			dev_err(&client->dev, "Failed to init %s ctrl\n",
			ctrl_config_list[i].name);
			continue;
		}

		if (ctrl_config_list[i].type == V4L2_CTRL_TYPE_STRING &&
			ctrl_config_list[i].flags & V4L2_CTRL_FLAG_READ_ONLY) {
			ctrl->p_new.p_char = devm_kzalloc(&client->dev,
				ctrl_config_list[i].max + 1, GFP_KERNEL);
		}

		priv->ctrls[i] = ctrl;
	}

	priv->numctrls = num_ctrls;
	priv->subdev->ctrl_handler = &priv->ctrl_handler;
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls\n",
			priv->ctrl_handler.error);
		err = priv->ctrl_handler.error;
		goto error;
	}

	err = v4l2_ctrl_handler_setup(&priv->ctrl_handler);
	if (err) {
		dev_err(&client->dev,
			"Error %d setting default controls\n", err);
		goto error;
	}

	return 0;

error:
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	return err;
}

MODULE_DEVICE_TABLE(of, gw5200_of_match);

static struct camera_common_pdata *gw5200_parse_dt(struct gw5200 *priv,
				struct i2c_client *client,
				struct camera_common_data *s_data)
{
	struct device_node *node = client->dev.of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	struct device_node *ser_node;
	struct i2c_client *ser_i2c = NULL;
	struct device_node *dser_node;
	struct i2c_client *dser_i2c = NULL;
	struct device_node *gmsl;
	int value = 0xFFFF;
	const char *str_value;
	const char *str_value1[2];
	int  i;
	int err;

	if (!node)
		return NULL;

	match = of_match_device(gw5200_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(&client->dev,
			   sizeof(*board_priv_pdata), GFP_KERNEL);

	err = of_property_read_string(node, "mclk",
				      &board_priv_pdata->mclk_name);
	if (err)
		dev_err(&client->dev, "mclk not in DT\n");

	err = of_property_read_u32(node, "reg", &priv->g_ctx.sdev_reg);
	if (err < 0) {
		dev_err(&client->dev, "reg not found\n");
		goto error;
	}

	err = of_property_read_u32(node, "def-addr",
					&priv->g_ctx.sdev_def);
	if (err < 0) {
		dev_err(&client->dev, "def-addr not found\n");
		goto error;
	}

	err = of_property_read_u32(node, "dser-num", &value);
	if (err < 0) {
		dev_err(&client->dev, "No dser-num info\n");
		goto error;
	}
	priv->dser_num = value;

	err = of_property_read_string(node, "dser-type", &str_value);

	if (err < 0) {
		dev_dbg(&client->dev, "No dser-type found. set to max9296.\n");
		priv->dser_type = DES_MAX9296;
	}
	if (!strcmp(str_value, "max9296")) {
		priv->dser_type = DES_MAX9296;
	} else if (!strcmp(str_value, "max96712")) {
		priv->dser_type = DES_MAX96712;
	} else {
		priv->dser_type = DES_MAX9296;
		dev_dbg(&client->dev, "invalid dser-type. set to max9296.\n");
	}
	dev_info(&client->dev, "dser-type: %s\n", str_value);
	des_dev_type=priv->dser_type;
/*
	err = of_property_read_u32(node, "def-width", &value);
	if (err < 0) {
		dev_err(&client->dev, "No def-width info\n");
		//goto error;
	}
	s_data->def_width = value;

	err = of_property_read_u32(node, "def-height", &value);
	if (err < 0) {
		dev_err(&client->dev, "No def-height info\n");
		//goto error;
	}
	s_data->def_height = value;
	dev_info(&client->dev, "def_width: %d def_height: %d\n", s_data->def_width, s_data->def_height);

	err = of_property_read_u32(node, "def-mode", &s_data->mode);
	if (err < 0) {
		dev_err(&client->dev, "No def-mode info\n");
		//goto error;
	}
*/
	err = of_property_read_string(node, "sensor_model", &str_value);

	if (err < 0) {
		dev_err(&client->dev, "No sensor_model found\n");
		//goto error;
	}
	if (!strcmp(str_value, "ar0233-SG")) {
		priv->sensor_model = SENSOR_AR0233_YUV_SG;
	} else if (!strcmp(str_value, "ar0820-SG")) {
		priv->sensor_model = SENSOR_AR0820_YUV_SG;
	} else if (!strcmp(str_value, "ox08bc-SG")) {
		priv->sensor_model = SENSOR_OX08BC_YUV_SG;
	} else if (!strcmp(str_value, "imx390-SG")) {
		priv->sensor_model = SENSOR_IMX390_YUV_SG;
	} else if (!strcmp(str_value, "imx490-SG")) {
		priv->sensor_model = SENSOR_IMX490_YUV_SG;
	} else if (!strcmp(str_value, "imx490-SG-mfp8")) {
		priv->sensor_model = SENSOR_IMX490_YUV_SG_MFP8;
	} else if (!strcmp(str_value, "imx390-3G")) {
		priv->sensor_model = SENSOR_IMX390_YUV_3G;
	} else if (!strcmp(str_value, "e003a-3G")) {
		priv->sensor_model = SENSOR_E003A_YUV_3G;
	} else if (!strcmp(str_value, "e003a")) {
		priv->sensor_model = SENSOR_E003A_YUV_ET;
	} else if (!strcmp(str_value, "f008ax")) {
		priv->sensor_model = SENSOR_F008AX_YUV_ET;
	} else if (!strcmp(str_value, "binocular")) {
		priv->sensor_model = SENSOR_BINOCULAR_YUV;
	} else if (!strcmp(str_value, "isx021-LI")) {
		priv->sensor_model = SENSOR_ISX021_YUV_LI;
	}else {
		priv->sensor_model = SENSOR_COMMON;
		dev_err(&client->dev, "invalid sensor model\n");
	}
	dev_info(&client->dev, "sensor_model: %s\n", str_value);
	priv->g_ctx.sensor_model = priv->sensor_model;

	ser_node = of_parse_phandle(node, "nvidia,gmsl-ser-device", 0);
	if (ser_node == NULL) {
		dev_err(&client->dev,
			"missing %s handle\n",
				"nvidia,gmsl-ser-device");
		goto error;
	}

	priv->pass_9295 = of_property_read_bool(node, "pass_9295");

	err = of_property_read_u32(ser_node, "reg", &priv->g_ctx.ser_reg);
	if (err < 0) {
		dev_err(&client->dev, "serializer reg not found\n");
		goto error;
	}

	ser_i2c = of_find_i2c_device_by_node(ser_node);
	of_node_put(ser_node);

	if (ser_i2c == NULL) {
		dev_err(&client->dev, "missing serializer dev handle\n");
		goto error;
	}
	if (ser_i2c->dev.driver == NULL) {
		dev_err(&client->dev, "missing serializer driver\n");
		goto error;
	}

	priv->ser_dev = &ser_i2c->dev;

	dser_node = of_parse_phandle(node, "nvidia,gmsl-dser-device", 0);
	if (dser_node == NULL) {
		dev_err(&client->dev,
			"missing %s handle\n",
				"nvidia,gmsl-dser-device");
		goto error;
	}

	dser_i2c = of_find_i2c_device_by_node(dser_node);
	of_node_put(dser_node);

	if (dser_i2c == NULL) {
		dev_err(&client->dev, "missing deserializer dev handle\n");
		goto error;
	}
	if (dser_i2c->dev.driver == NULL) {
		dev_err(&client->dev, "missing deserializer driver\n");
		goto error;
	}

	priv->dser_dev = &dser_i2c->dev;

	/* populate g_ctx from DT */
	gmsl = of_get_child_by_name(node, "gmsl-link");
	if (gmsl == NULL) {
		dev_err(&client->dev, "missing gmsl-link device node\n");
		err = -EINVAL;
		goto error;
	}

	err = of_property_read_string(gmsl, "dst-csi-port", &str_value);
	if (err < 0) {
		dev_err(&client->dev, "No dst-csi-port found\n");
		goto error;
	}
	priv->g_ctx.dst_csi_port =
		(!strcmp(str_value, "a")) ? GMSL_CSI_PORT_A : GMSL_CSI_PORT_B;

	err = of_property_read_string(gmsl, "src-csi-port", &str_value);
	if (err < 0) {
		dev_err(&client->dev, "No src-csi-port found\n");
		goto error;
	}
	priv->g_ctx.src_csi_port =
		(!strcmp(str_value, "a")) ? GMSL_CSI_PORT_A : GMSL_CSI_PORT_B;

	err = of_property_read_string(gmsl, "csi-mode", &str_value);
	if (err < 0) {
		dev_err(&client->dev, "No csi-mode found\n");
		goto error;
	}

	if (!strcmp(str_value, "1x4")) {
		priv->g_ctx.csi_mode = GMSL_CSI_1X4_MODE;
	} else if (!strcmp(str_value, "2x4")) {
		priv->g_ctx.csi_mode = GMSL_CSI_2X4_MODE;
	} else if (!strcmp(str_value, "4x2")) {
		priv->g_ctx.csi_mode = GMSL_CSI_4X2_MODE;
	} else if (!strcmp(str_value, "2x2")) {
		priv->g_ctx.csi_mode = GMSL_CSI_2X2_MODE;
	} else {
		dev_err(&client->dev, "invalid csi mode\n");
		goto error;
	}

	err = of_property_read_string(gmsl, "serdes-csi-link", &str_value);
	if (err < 0) {
		dev_err(&client->dev, "No serdes-csi-link found\n");
		goto error;
	}
	if(!strcmp(str_value, "a")){
		priv->g_ctx.serdes_csi_link = GMSL_SERDES_CSI_LINK_A;
	}else if(!strcmp(str_value, "b")){
		priv->g_ctx.serdes_csi_link = GMSL_SERDES_CSI_LINK_B;
	}else if(!strcmp(str_value, "c")){
		priv->g_ctx.serdes_csi_link = GMSL_SERDES_CSI_LINK_C;
	}else if(!strcmp(str_value, "d")){
		priv->g_ctx.serdes_csi_link = GMSL_SERDES_CSI_LINK_D;
	}else{
		//other
	}

	err = of_property_read_u32(gmsl, "st-vc", &value);
	if (err < 0) {
		dev_err(&client->dev, "No st-vc info\n");
		goto error;
	}
	priv->g_ctx.st_vc = value;

	err = of_property_read_u32(gmsl, "vc-id", &value);
	if (err < 0) {
		dev_err(&client->dev, "No vc-id info\n");
		goto error;
	}
	priv->g_ctx.dst_vc = value;

	err = of_property_read_u32(gmsl, "num-lanes", &value);
	if (err < 0) {
		dev_err(&client->dev, "No num-lanes info\n");
		goto error;
	}
	priv->g_ctx.num_csi_lanes = value;

	if (of_get_property(gmsl, "frame-sync-en", NULL)) {
		priv->g_ctx.frame_sync_en = true;
		dev_info(&client->dev, "frame-sync-en found\n");
	}else {
		priv->g_ctx.frame_sync_en = false;
		dev_info(&client->dev, "frame-sync-en not found\n");
	}

	priv->g_ctx.num_streams =
			of_property_count_strings(gmsl, "streams");
	if (priv->g_ctx.num_streams <= 0) {
		dev_err(&client->dev, "No streams found\n");
		err = -EINVAL;
		goto error;
	}

	for (i = 0; i < priv->g_ctx.num_streams; i++) {
		of_property_read_string_index(gmsl, "streams", i,
						&str_value1[i]);
		if (!str_value1[i]) {
			dev_err(&client->dev, "invalid stream info\n");
			goto error;
		}
		if (!strcmp(str_value1[i], "raw12")) {
			priv->g_ctx.streams[i].st_data_type =
							GMSL_CSI_DT_RAW_12;
		} else if (!strcmp(str_value1[i], "yuv422-8")) {
			priv->g_ctx.streams[i].st_data_type =
							GMSL_CSI_DT_YUV422_8;
		} else if (!strcmp(str_value1[i], "embed")) {
			priv->g_ctx.streams[i].st_data_type =
							GMSL_CSI_DT_EMBED;
		} else if (!strcmp(str_value1[i], "ued-u1")) {
			priv->g_ctx.streams[i].st_data_type =
							GMSL_CSI_DT_UED_U1;
		} else {
			dev_err(&client->dev, "invalid stream data type\n");
			goto error;
		}
	}

	priv->g_ctx.s_dev = &client->dev;

	return board_priv_pdata;

error:
	devm_kfree(&client->dev, board_priv_pdata);
	return NULL;
}

static int gw5200_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops gw5200_subdev_internal_ops = {
	.open = gw5200_open,
};

static const struct media_entity_operations gw5200_media_ops = {
#ifdef CONFIG_MEDIA_CONTROLLER
	.link_validate = v4l2_subdev_link_validate,
#endif
};

static int gw5200_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct camera_common_data *common_data;
	struct device_node *node = client->dev.of_node;
	struct gw5200 *priv;
	int err;

	dev_info(&client->dev, "probing v4l2 sensor.\n");

	if (!IS_ENABLED(CONFIG_OF) || !node)
		return -EINVAL;

	common_data = devm_kzalloc(&client->dev,
			    sizeof(struct camera_common_data), GFP_KERNEL);
	if (!common_data) {
		dev_err(&client->dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

	priv = devm_kzalloc(&client->dev,
			    sizeof(struct gw5200) + sizeof(struct v4l2_ctrl *) *
			    ARRAY_SIZE(ctrl_config_list),
			    GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

	priv->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	common_data->ops = &gw5200_common_ops;
	common_data->ctrl_handler = &priv->ctrl_handler;
	common_data->dev        = &client->dev;
	common_data->frmfmt = &gw5200_frmfmt[0];
	common_data->colorfmt = camera_common_find_datafmt(
					  GW5200_DEFAULT_DATAFMT);
	common_data->power = &priv->power;
	common_data->ctrls = priv->ctrls;
	common_data->priv = (void *)priv;
	common_data->numctrls = ARRAY_SIZE(ctrl_config_list);
	common_data->numfmts = ARRAY_SIZE(gw5200_frmfmt);
	common_data->def_mode = GW5200_DEFAULT_MODE;
	common_data->def_width = GW5200_DEFAULT_WIDTH;
	common_data->def_height = GW5200_DEFAULT_HEIGHT;
	common_data->fmt_width = common_data->def_width;
	common_data->fmt_height = common_data->def_height;
	common_data->def_clk_freq = GW5200_DEFAULT_CLK_FREQ;

	priv->pdata = gw5200_parse_dt(priv, client, common_data);
	if (!priv->pdata) {
		dev_err(&client->dev, "unable to get platform data\n");
		return -EPROBE_DEFER;
	}
/*
	if (common_data->mode < 0 || common_data->mode >= common_data->numfmts)
		common_data->mode = GW5200_DEFAULT_MODE;
	common_data->fmt_width = common_data->def_width = gw5200_frmfmt[common_data->mode].size.width;
	common_data->fmt_height = common_data->def_height = gw5200_frmfmt[common_data->mode].size.height;
	dev_info(&client->dev, "def_mode: %d fmt_width: %d fmt_height: %d\n", common_data->mode, common_data->fmt_width, common_data->fmt_height);
*/
	priv->i2c_client = client;
	priv->s_data = common_data;
	priv->subdev = &common_data->subdev;
	priv->subdev->dev = &client->dev;
	priv->s_data->dev = &client->dev;
	priv->id = id;

	err = gw5200_power_get(priv);
	if (err)
		return err;

	/* Pair sensor to serializer dev */
	if(priv->pass_9295==1&&pass==0)
	{
		pass=1;
		err = max9295_sdev_pair(priv->ser_dev, &priv->g_ctx);
	}else if(priv->pass_9295!=1)
	{
		err = max9295_sdev_pair(priv->ser_dev, &priv->g_ctx);
	}
	if (err) {
		dev_err(&client->dev, "gmsl ser pairing failed\n");
		return err;
	}

	/* Register sensor to deserializer dev */
	if (priv->dser_type == DES_MAX9296) {
		err = max9296_sdev_register(priv->dser_dev, &priv->g_ctx);
	} else if (priv->dser_type == DES_MAX96712) {
		err = max96712_sdev_register(priv->dser_dev, &priv->g_ctx);
	} else {
		//other
	}

	if (err) {
		dev_err(&client->dev, "gmsl deserializer register failed\n");
		return err;
	}

	/*
	 * gmsl serdes setup
	 *
	 * Sensor power on/off should be the right place for serdes
	 * setup/reset. But the problem is, the total required delay
	 * in serdes setup/reset exceeds the frame wait timeout, looks to
	 * be related to multiple channel open and close sequence
	 * issue (#BUG 200477330).
	 * Once this bug is fixed, these may be moved to power on/off.
	 * The delays in serdes is as per guidelines and can't be reduced,
	 * so it is placed in probe/remove, though for that, deserializer
	 * would be powered on always post boot, until 1.2v is supplied
	 * to deserializer from CVB.
	 */
	err = gw5200_gmsl_serdes_setup(priv);
	if (err) {
		dev_err(&client->dev,
			"%s gmsl serdes setup failed\n", __func__);
		return err;
	}

	err = camera_common_initialize(common_data, "gw5200");
	if (err) {
		dev_err(&client->dev, "Failed to initialize gw5200.\n");
		return err;
	}

	v4l2_i2c_subdev_init(priv->subdev, client, &gw5200_subdev_ops);

	err = gw5200_ctrls_init(priv);
	if (err)
		return err;

	priv->subdev->internal_ops = &gw5200_subdev_internal_ops;
	priv->subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;

#if defined(CONFIG_MEDIA_CONTROLLER)
	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	priv->subdev->entity.ops = &gw5200_media_ops;
	err = tegra_media_entity_init(&priv->subdev->entity, 1,
		&priv->pad, true, true);
	if (err < 0) {
		dev_err(&client->dev, "unable to init media entity\n");
		return err;
	}
#endif

	err = v4l2_async_register_subdev(priv->subdev);
	if (err)
		return err;

#ifdef __MUTEX_LOCK__
	if (gw5200_mutex_lock[priv->dser_num].mutex_cnt == 0){
		dev_info(&client->dev, " %s: mutex_init. \n", __func__);
		mutex_init(&gw5200_mutex_lock[priv->dser_num].stream_lock);
		gw5200_mutex_lock[priv->dser_num].mutex_cnt++;
	}
#endif

	dev_info(&client->dev, "Detected GW5200 sensor\n");

	if(n_5200_dev < 12){
		sen_5200_listp[n_5200_dev]=priv->s_data;
		n_5200_dev++;
	}

	return 0;
}

static int gw5200_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct gw5200 *priv = (struct gw5200 *)s_data->priv;

	gw5200_gmsl_serdes_reset(priv);

	v4l2_async_unregister_subdev(priv->subdev);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev->entity);
#endif

	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	camera_common_remove_debugfs(s_data);

#ifdef __MUTEX_LOCK__
	if (gw5200_mutex_lock[priv->dser_num].mutex_cnt){
		dev_info(&client->dev, " %s: mutex_destroy. \n", __func__);
		mutex_destroy(&gw5200_mutex_lock[priv->dser_num].stream_lock);
		gw5200_mutex_lock[priv->dser_num].mutex_cnt--;
	}

#endif

	return 0;
}

static const struct i2c_device_id gw5200_id[] = {
	{ "gw5200", 0 },
	{ "gw5300", 0 },
	{ "gw5X00", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, gw5200_id);

static struct i2c_driver gw5200_i2c_driver = {
	.driver = {
		.name = "gw5X00",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gw5200_of_match),
	},
	.probe = gw5200_probe,
	.remove = gw5200_remove,
	.id_table = gw5200_id,
};

static int __init gw5200_init(void)
{
	mutex_init(&serdes_lock__);

	return i2c_add_driver(&gw5200_i2c_driver);
}

static void __exit gw5200_exit(void)
{
	mutex_destroy(&serdes_lock__);

	i2c_del_driver(&gw5200_i2c_driver);
}

module_init(gw5200_init);
module_exit(gw5200_exit);

MODULE_DESCRIPTION("Media Controller driver for GW5200/5300 yuv camera");
MODULE_AUTHOR("Wuhan RuiMu ZhiJue Corporation");
MODULE_AUTHOR("limiao <361995459@qq.com");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.2.0.1015");
