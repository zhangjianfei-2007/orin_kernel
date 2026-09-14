/*
 * ox03c10.c - ox03c10 sensor driver
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

#include <media/tegracam_core.h>
#include "ox03c10_mode_tbls.h"

//#define SENSOR_OX03C10_RAW_HK 200

#define OX03C10_MIN_GAIN         (0)
#define OX03C10_MAX_GAIN         (30)
#define OX03C10_MAX_GAIN_REG     ((OX03C10_MAX_GAIN - OX03C10_MIN_GAIN) * 10 / 3)
#define OX03C10_DEFAULT_FRAME_LENGTH    (1125)
#define OX03C10_FRAME_LENGTH_ADDR_MSB    0x200A
#define OX03C10_FRAME_LENGTH_ADDR_MID    0x2009
#define OX03C10_FRAME_LENGTH_ADDR_LSB    0x2008
#define OX03C10_COARSE_TIME_SHS1_ADDR_MSB    0x000E
#define OX03C10_COARSE_TIME_SHS1_ADDR_MID    0x000D
#define OX03C10_COARSE_TIME_SHS1_ADDR_LSB    0x000C
#define OX03C10_COARSE_TIME_SHS2_ADDR_MSB    0x0012
#define OX03C10_COARSE_TIME_SHS2_ADDR_MID    0x0011
#define OX03C10_COARSE_TIME_SHS2_ADDR_LSB    0x0010
#define OX03C10_GROUP_HOLD_ADDR    		0x0008
#define OX03C10_ANALOG_GAIN_SP1H_ADDR    0x0018
#define OX03C10_ANALOG_GAIN_SP1L_ADDR    0x001A
#define OX03C10_FSYNC_ADDR		0x3650

const struct of_device_id ox03c10_of_match[] = {
	{ .compatible = "nvidia,ox03c10",},
	{ },
};
MODULE_DEVICE_TABLE(of, ox03c10_of_match);

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_EXPOSURE_SHORT,
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_HDR_EN,
};

#define DESER_TOTAL 4
static int ser_linked_map[DESER_TOTAL]= {0};

struct camera_common_data *sen_ox03c10_listp[12]={NULL};
EXPORT_SYMBOL(sen_ox03c10_listp);
int n_ox03c10_dev=0;
EXPORT_SYMBOL(n_ox03c10_dev);
extern int des_dev_type;

struct ox03c10 {
	struct i2c_client	*i2c_client;
	const struct i2c_device_id *id;
	struct v4l2_subdev	*subdev;
	struct device		*ser_dev;
	struct device		*dser_dev;
	struct gmsl_link_ctx	g_ctx;
	u32	frame_length;
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	u32 dser_num;
	u32 dser_type;
	u32 sensor_model;
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	//.cache_type = REGCACHE_RBTREE,
	.cache_type = REGCACHE_NONE,
};

static inline void ox03c10_get_frame_length_regs(ox03c10_reg *regs,
				u32 frame_length)
{
//	regs->addr = OX03C10_FRAME_LENGTH_ADDR_MSB;
//	regs->val = (frame_length >> 16) & 0x01;
//
//	(regs + 1)->addr = OX03C10_FRAME_LENGTH_ADDR_MID;
//	(regs + 1)->val = (frame_length >> 8) & 0xff;
//
//	(regs + 2)->addr = OX03C10_FRAME_LENGTH_ADDR_LSB;
//	(regs + 2)->val = (frame_length) & 0xff;
}

static inline void ox03c10_get_coarse_time_regs_shs1(ox03c10_reg *regs,
				u32 coarse_time)
{
//	regs->addr = OX03C10_COARSE_TIME_SHS1_ADDR_MSB;
//	regs->val = (coarse_time >> 16) & 0x0f;
//
//	(regs + 1)->addr = OX03C10_COARSE_TIME_SHS1_ADDR_MID;
//	(regs + 1)->val = (coarse_time >> 8) & 0xff;
//
//	(regs + 2)->addr = OX03C10_COARSE_TIME_SHS1_ADDR_LSB;
//	(regs + 2)->val = (coarse_time) & 0xff;
}

static inline void ox03c10_get_coarse_time_regs_shs2(ox03c10_reg *regs,
				u32 coarse_time)
{
//	regs->addr = OX03C10_COARSE_TIME_SHS2_ADDR_MSB;
//	regs->val = (coarse_time >> 16) & 0x0f;
//
//	(regs + 1)->addr = OX03C10_COARSE_TIME_SHS2_ADDR_MID;
//	(regs + 1)->val = (coarse_time >> 8) & 0xff;
//
//	(regs + 2)->addr = OX03C10_COARSE_TIME_SHS2_ADDR_LSB;
//	(regs + 2)->val = (coarse_time) & 0xff;
}

static inline void ox03c10_get_gain_reg(ox03c10_reg *regs,
				u16 gain)
{
//	regs->addr = OX03C10_ANALOG_GAIN_SP1H_ADDR;
//	regs->val = (gain) & 0xff;
//
//	(regs + 1)->addr = OX03C10_ANALOG_GAIN_SP1H_ADDR + 1;
//	(regs + 1)->val = (gain >> 8) & 0xff;
//
//	(regs + 2)->addr = OX03C10_ANALOG_GAIN_SP1L_ADDR;
//	(regs + 2)->val = (gain) & 0xff;
//
//	(regs + 3)->addr = OX03C10_ANALOG_GAIN_SP1L_ADDR + 1;
//	(regs + 3)->val = (gain >> 8) & 0xff;
}


static int test_mode;
module_param(test_mode, int, 0644);

int ox03c10_read_reg(struct camera_common_data *s_data,
				u16 addr, u8 *val)
{
	int err = 0;
	u32 reg_val = 0;

	//err = regmap_read(s_data->regmap, addr, &reg_val);
	err = regmap_raw_read(s_data->regmap, addr, &reg_val, 1);
	if (err){
		dev_err(s_data->dev, "%s:i2c read failed, 0x%x = XX\n", __func__, addr);
		return err;
	}
	*val = reg_val & 0xFF;

	dev_dbg(s_data->dev, "%s:i2c read , 0x%x = %x\n",
			__func__, addr, *val);

	return err;
}
EXPORT_SYMBOL(ox03c10_read_reg);

int ox03c10_write_reg(struct camera_common_data *s_data,
				u16 addr, u8 val)
{
	int err;
	struct device *dev = s_data->dev;

	dev_dbg(s_data->dev, "%s:i2c write , 0x%x = %x\n",__func__, addr, val);

	err = regmap_write(s_data->regmap, addr, val);
	if (err)
		dev_err(dev, "%s:i2c write failed, 0x%x = %x\n",
			__func__, addr, val);

	return err;
}
EXPORT_SYMBOL(ox03c10_write_reg);

static int ox03c10_write_table(struct ox03c10 *priv,
				const ox03c10_reg table[])
{
	struct camera_common_data *s_data = priv->s_data;

	dev_dbg(s_data->dev, "%s:\n",__func__);

	return regmap_util_write_table_8(s_data->regmap,
					 table,
					 NULL, 0,
					 OX03C10_TABLE_WAIT_MS,
					 OX03C10_TABLE_END);
}

static struct mutex serdes_lock__;

static int ox03c10_gmsl_serdes_setup(struct ox03c10 *priv)
{
	int err = 0;
	int des_err = 0;
	struct device *dev;

	if (!priv || !priv->ser_dev || !priv->dser_dev || !priv->i2c_client)
		return -EINVAL;

	dev = &priv->i2c_client->dev;
	dev_info(dev, "%s: enter\n", __func__);

	mutex_lock(&serdes_lock__);

	/* For now no separate power on required for serializer device */
	//max9296_power_on(priv->dser_dev);
	if (priv->dser_type == DES_MAX9296) {
		max9296_power_on(priv->dser_dev);
	} else if (priv->dser_type == DES_MAX96712) {
		max96712_power_on(priv->dser_dev);
	} else {
		//other
	}

	/* setup serdes addressing and control pipeline */
	//err = max9296_setup_link(priv->dser_dev, &priv->i2c_client->dev);
	if (priv->dser_type == DES_MAX9296) {
		err = max9296_setup_link(priv->dser_dev, &priv->i2c_client->dev);
	} else if (priv->dser_type == DES_MAX96712) {
		err = max96712_setup_link(priv->dser_dev, &priv->i2c_client->dev);
	} else {
		//other
	}
	if (err) {
		dev_err(dev, "gmsl deserializer link config failed\n");
		goto error;
	}

	err = max9295_setup_control(priv->ser_dev);

	/* proceed even if ser setup failed, to setup deser correctly */
	if (err) {
		dev_err(dev, "gmsl serializer setup failed\n");
		goto error;
	}else{
		//ser_linked_map[priv->dser_num] |= 1 << (priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A? 0:1);
		if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
			ser_linked_map[priv->dser_num] |= 1 << 0;
		}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_B){
			ser_linked_map[priv->dser_num] |= 1 << 1;
		}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_C){
			ser_linked_map[priv->dser_num] |= 1 << 2;
		}else if(priv->g_ctx.serdes_csi_link == GMSL_SERDES_CSI_LINK_D){
			ser_linked_map[priv->dser_num] |= 1 << 3;
		}
	}

	//des_err = max9296_setup_control(priv->dser_dev, &priv->i2c_client->dev);
	//des_err = max9296_setup_control(priv->dser_dev);
	if (priv->dser_type == DES_MAX9296) {
		des_err = max9296_setup_control(priv->dser_dev);
	} else if (priv->dser_type == DES_MAX96712) {
		des_err = max96712_setup_control(priv->dser_dev);
	} else {
		//other
	}
	if (des_err) {
		dev_err(dev, "gmsl deserializer setup failed\n");
		/* overwrite err only if deser setup also failed */
		err = des_err;
	}

error:
	//max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	if (priv->dser_type == DES_MAX9296) {
		max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	} else if (priv->dser_type == DES_MAX96712) {
		//fix it
		max96712_link_reset(priv->dser_dev, ser_linked_map[priv->dser_num]);
	} else {
		//other
	}
	mutex_unlock(&serdes_lock__);
	dev_info(dev, "%s: exit\n", __func__);
	return 0;
}

static void ox03c10_gmsl_serdes_reset(struct ox03c10 *priv)
{
	mutex_lock(&serdes_lock__);

	/* reset serdes addressing and control pipeline */
	max9295_reset_control(priv->ser_dev);
	//max9296_reset_control(priv->dser_dev, &priv->i2c_client->dev);

	//max9296_power_off(priv->dser_dev);

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

static int ox03c10_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power on\n", __func__);
	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	pw->state = SWITCH_ON;

	return 0;
}

static int ox03c10_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s:\n", __func__);

	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (!err)
			goto power_off_done;
		else
			dev_err(dev, "%s failed.\n", __func__);
		return err;
	}

power_off_done:
	pw->state = SWITCH_OFF;

	return 0;
}

static int ox03c10_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	const char *mclk_name;
	const char *parentclk_name;
	struct clk *parent;
	int err = 0;

	mclk_name = pdata->mclk_name ?
		    pdata->mclk_name : "cam_mclk1";
	pw->mclk = devm_clk_get(dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(dev, "unable to get clock %s\n", mclk_name);
		return PTR_ERR(pw->mclk);
	}

	parentclk_name = pdata->parentclk_name;
	if (parentclk_name) {
		parent = devm_clk_get(dev, parentclk_name);
		if (IS_ERR(parent)) {
			dev_err(dev, "unable to get parent clcok %s",
				parentclk_name);
		} else
			clk_set_parent(pw->mclk, parent);
	}

	pw->state = SWITCH_OFF;

	return err;
}

static int ox03c10_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	return 0;
}

static int ox03c10_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
//	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	int err = 0;

	dev_dbg(dev, "%s: %d\n", __func__, val);

//	err = ox03c10_write_reg(s_data,
//			       OX03C10_GROUP_HOLD_ADDR, val);
	if (err) {
		dev_err(dev,
			"%s: Group hold control error\n", __func__);
		return err;
	}

	return 0;
}

static int ox03c10_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
	ox03c10_reg reg_list[4];
	int err = 0, i;
	u16 gain;

	gain = (u16)(val / mode->control_properties.step_gain_val);

	dev_dbg(dev, "%s: db: %d\n",  __func__, gain);

	if (gain > OX03C10_MAX_GAIN_REG)
		gain = OX03C10_MAX_GAIN_REG;

	ox03c10_get_gain_reg(reg_list, gain);
	for (i = 0; i < 4; i++) {
//		err = ox03c10_write_reg(s_data, reg_list[i].addr,
//			 reg_list[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_info(dev, "%s: GAIN control error\n", __func__);
	return err;
}

static int ox03c10_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	struct ox03c10 *priv = (struct ox03c10 *)tegracam_get_privdata(tc_dev);

	dev_dbg(&priv->i2c_client->dev, "%s enter!\n", __func__);
	/* fixed 30fps */
	priv->frame_length = OX03C10_DEFAULT_FRAME_LENGTH;
	return 0;
}

static int ox03c10_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	struct ox03c10 *priv = (struct ox03c10 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode];
	ox03c10_reg reg_list[3];
	int err = 0;
	u32 coarse_time;
	u32 shs1;
	int i = 0;

	dev_dbg(&priv->i2c_client->dev, "%s enter!\n", __func__);

	if (priv->frame_length == 0)
		priv->frame_length = OX03C10_DEFAULT_FRAME_LENGTH;


	coarse_time = (u32) (val * s_data->frmfmt[s_data->mode].framerates[0] *
		priv->frame_length / mode->control_properties.exposure_factor);

	shs1 = priv->frame_length - coarse_time;

	if (shs1 < 2)
		shs1 = 2;

	ox03c10_get_coarse_time_regs_shs1(reg_list, shs1);
	for (i = 0; i < 3; i++) {
//		err = ox03c10_write_reg(priv->s_data, reg_list[i].addr,
//				reg_list[i].val);
		if (err)
			goto fail;
	}

	ox03c10_get_coarse_time_regs_shs2(reg_list, shs1);

	for (i = 0; i < 3; i++) {
//		err = ox03c10_write_reg(priv->s_data, reg_list[i].addr,
//			reg_list[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
		"%s: set coarse time error\n", __func__);
	return err;
}

static struct tegracam_ctrl_ops ox03c10_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.set_gain = ox03c10_set_gain,
	.set_exposure = ox03c10_set_exposure,
	.set_exposure_short = ox03c10_set_exposure,
	.set_frame_rate = ox03c10_set_frame_rate,
	.set_group_hold = ox03c10_set_group_hold,
};
static struct camera_common_pdata *ox03c10_parse_dt(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *node = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	int err;

	if (!node)
		return NULL;

	match = of_match_device(ox03c10_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(dev, sizeof(*board_priv_pdata), GFP_KERNEL);

	err = of_property_read_string(node, "mclk",
				      &board_priv_pdata->mclk_name);
	if (err)
		dev_err(dev, "mclk not in DT\n");

	return board_priv_pdata;
}

static int ox03c10_set_mode(struct tegracam_device *tc_dev)
{
	struct ox03c10 *priv = (struct ox03c10 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	const struct of_device_id *match;
	int err;
	u8 val;

	dev_dbg(dev, "%s enter!\n", __func__);

	match = of_match_device(ox03c10_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return -EINVAL;
	}

	if (priv->sensor_model == SENSOR_OX03C10_RAW_HK){
		//max9295_write_reg(priv->ser_dev, 0x0318,0x6b);	//dt enabled, dt=0x2b (raw10)?
		//max9295_write_reg(priv->ser_dev, 0x02BE,0x04);	//MFP0
		max9295_write_reg(priv->ser_dev, 0x02D6,0x10);	//MFP8
		msleep(100);
	}

	//{0x0103, 0x01},
	//{0x0107, 0x01},
	ox03c10_write_reg(s_data, 0x0103, 0x01);
	ox03c10_write_reg(s_data, 0x0107, 0x01);
	msleep(300);
	err = ox03c10_write_table(priv, mode_table[s_data->mode_prop_idx]);
//	if (priv->g_ctx.frame_sync_en)
//		ox03c10_write_reg(s_data, OX03C10_FSYNC_ADDR, 0x01);

	ox03c10_read_reg(s_data, 0x4d2a, &val);
	dev_dbg(dev, "%s: tpm_int_rdout = 0x%02x\n", __func__, val);
	ox03c10_read_reg(s_data, 0x4d2b, &val);
	dev_dbg(dev, "%s: tpm_dec_rdout = 0x%02x\n", __func__, val);

	if (err)
		return err;

	return 0;
}

static int ox03c10_start_streaming(struct tegracam_device *tc_dev)
{
	struct ox03c10 *priv = (struct ox03c10 *)tegracam_get_privdata(tc_dev);
	struct device *dev = tc_dev->dev;
	int err;

	dev_dbg(dev, "%s++ enter!\n", __func__);

	/* enable serdes streaming */
	err = max9295_setup_streaming(priv->ser_dev);
	if (err)
		goto exit;
	//err = max9296_setup_streaming(priv->dser_dev, dev);
	if (priv->dser_type == DES_MAX9296) {
		err = max9296_setup_streaming(priv->dser_dev, dev);
	} else if (priv->dser_type == DES_MAX96712) {
		err = max96712_setup_streaming(priv->dser_dev, dev);
	} else {
		//other
	}
	if (err)
		goto exit;
	//err = max9296_start_streaming(priv->dser_dev, dev);
	if (priv->dser_type == DES_MAX9296) {
		err = max9296_start_streaming(priv->dser_dev, dev);
	} else if (priv->dser_type == DES_MAX96712) {
		err = max96712_start_streaming(priv->dser_dev, dev);
	} else {
		//other
	}
	if (err)
		goto exit;

	err = ox03c10_write_table(priv,
		mode_table[OX03C10_MODE_START_STREAM]);
	//max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	if (priv->dser_type == DES_MAX9296) {
		max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	} else if (priv->dser_type == DES_MAX96712) {
		max96712_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	} else {
		//other
	}
	if (err)
		goto exit;

	dev_dbg(dev, "%s-- exit!\n", __func__);
	return 0;

exit:
	dev_err(dev, "%s: error setting stream\n", __func__);

	return err;
}

static int ox03c10_stop_streaming(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct ox03c10 *priv = (struct ox03c10 *)tegracam_get_privdata(tc_dev);
	int err;

	dev_dbg(dev, "%s++ enter!\n", __func__);

	/* disable serdes streaming */
	//max9296_stop_streaming(priv->dser_dev, dev);
	if (priv->dser_type == DES_MAX9296) {
		max9296_stop_streaming(priv->dser_dev, dev);
	} else if (priv->dser_type == DES_MAX96712) {
		max96712_stop_streaming(priv->dser_dev, dev);
	} else {
		//other
	}
	err = ox03c10_write_table(priv, mode_table[OX03C10_MODE_STOP_STREAM]);
	//max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	if (priv->dser_type == DES_MAX9296) {
		max9296_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	} else if (priv->dser_type == DES_MAX96712) {
		max96712_link_splitter(priv->dser_dev, ser_linked_map[priv->dser_num]);
	} else {
		//other
	}
	if (priv->sensor_model == SENSOR_OX03C10_RAW_HK){
		/* MAX9295 MFP8 --> OX03C10 RESETB
		 * If RESETB is tied low, hardware standby mode will be initiated.
		 * 1. Enabled by pulling RESTB low or through software reset
		 * 2. Register values are reset to default values <==
		 * 3. No SCCB communication <==
		 * 4. Low power consumption
		 */
		max9295_write_reg(priv->ser_dev, 0x02D6,0x00);	//MFP8
	}

	dev_dbg(dev, "%s-- exit!\n", __func__);

	if (err)
		return err;

	return 0;
}

static struct camera_common_sensor_ops ox03c10_common_ops = {
	.numfrmfmts = ARRAY_SIZE(ox03c10_frmfmt),
	.frmfmt_table = ox03c10_frmfmt,
	.power_on = ox03c10_power_on,
	.power_off = ox03c10_power_off,
	.write_reg = ox03c10_write_reg,
	.read_reg = ox03c10_read_reg,
	.parse_dt = ox03c10_parse_dt,
	.power_get = ox03c10_power_get,
	.power_put = ox03c10_power_put,
	.set_mode = ox03c10_set_mode,
	.start_streaming = ox03c10_start_streaming,
	.stop_streaming = ox03c10_stop_streaming,
};

static int ox03c10_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static const struct v4l2_subdev_internal_ops ox03c10_subdev_internal_ops = {
	.open = ox03c10_open,
};

static int ox03c10_board_setup(struct ox03c10 *priv)
{
	struct tegracam_device *tc_dev = priv->tc_dev;
	struct device *dev = tc_dev->dev;
	struct device_node *node = dev->of_node;
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

	/*
	 * about sensor i2c address (SCCB ID):
	 * If SID is low, the sensor’s default SCCB ID is 0x6C. ID values are stored and correspond to registers 0x3035.
	 * It can be changed either by SCCB command or by an OTP command,
	 * thus allowing the ID of the sensor to be altered arbitrarily.
	 *
	 * However, like imx390 driver, this driver use i2c address translator of max9295/max96717
	 * other than alter sensor register 0x3035.
	 */

	//sdev_reg (i.e. 0x1b, 0x1c, 0x1d, 0x1e) which defined by "reg =<...>;" and used by i2c driver
	//will be x2 and written to max9295/max96717 register 0x44 (i2c address translator source)
	err = of_property_read_u32(node, "reg", &priv->g_ctx.sdev_reg);
	if (err < 0) {
		dev_err(dev, "reg not found\n");
		goto error;
	}

	//def-addr = <0x36>; which
	//will be x2 and written to max9295/max96717 register 0x45 (i2c address translator destination).
	err = of_property_read_u32(node, "def-addr",
					&priv->g_ctx.sdev_def);
	if (err < 0) {
		dev_err(dev, "def-addr not found\n");
		goto error;
	}

	err = of_property_read_u32(node, "dser-num", &value);
	if (err < 0) {
		dev_err(dev, "No dser-num info\n");
		goto error;
	}
	priv->dser_num = value;

	err = of_property_read_string(node, "dser-type", &str_value);

	if (err < 0) {
		dev_dbg(dev, "No dser-type found. set to max9296.\n");
		priv->dser_type = DES_MAX9296;
	}
	if (!strcmp(str_value, "max9296")) {
		priv->dser_type = DES_MAX9296;
	} else if (!strcmp(str_value, "max96712")) {
		priv->dser_type = DES_MAX96712;
	} else {
		priv->dser_type = DES_MAX9296;
		dev_dbg(dev, "invalid dser-type. set to max9296.\n");
	}
	dev_info(dev, "dser-type: %s\n", str_value);
	des_dev_type=priv->dser_type;

	err = of_property_read_string(node, "sensor_model", &str_value);
	if (err < 0) {
		dev_err(dev, "No sensor_model found\n");
		//goto error;
	}
	if (!strcmp(str_value, "ox03c10-HK")) {
		priv->sensor_model = SENSOR_OX03C10_RAW_HK;
	} else {
		priv->sensor_model = SENSOR_COMMON;
		dev_err(dev, "invalid sensor model\n");
	}
	priv->g_ctx.sensor_model = priv->sensor_model;

	ser_node = of_parse_phandle(node, "nvidia,gmsl-ser-device", 0);
	if (ser_node == NULL) {
		dev_err(dev,
			"missing %s handle\n",
				"nvidia,gmsl-ser-device");
		goto error;
	}

	err = of_property_read_u32(ser_node, "reg", &priv->g_ctx.ser_reg);
	if (err < 0) {
		dev_err(dev, "serializer reg not found\n");
		goto error;
	}

	ser_i2c = of_find_i2c_device_by_node(ser_node);
	of_node_put(ser_node);

	if (ser_i2c == NULL) {
		dev_err(dev, "missing serializer dev handle\n");
		goto error;
	}
	if (ser_i2c->dev.driver == NULL) {
		dev_err(dev, "missing serializer driver\n");
		goto error;
	}

	priv->ser_dev = &ser_i2c->dev;

	dser_node = of_parse_phandle(node, "nvidia,gmsl-dser-device", 0);
	if (dser_node == NULL) {
		dev_err(dev,
			"missing %s handle\n",
				"nvidia,gmsl-dser-device");
		goto error;
	}

	dser_i2c = of_find_i2c_device_by_node(dser_node);
	of_node_put(dser_node);

	if (dser_i2c == NULL) {
		dev_err(dev, "missing deserializer dev handle\n");
		goto error;
	}
	if (dser_i2c->dev.driver == NULL) {
		dev_err(dev, "missing deserializer driver\n");
		goto error;
	}

	priv->dser_dev = &dser_i2c->dev;

	/* populate g_ctx from DT */
	gmsl = of_get_child_by_name(node, "gmsl-link");
	if (gmsl == NULL) {
		dev_err(dev, "missing gmsl-link device node\n");
		err = -EINVAL;
		goto error;
	}

	err = of_property_read_string(gmsl, "dst-csi-port", &str_value);
	if (err < 0) {
		dev_err(dev, "No dst-csi-port found\n");
		goto error;
	}
	priv->g_ctx.dst_csi_port =
		(!strcmp(str_value, "a")) ? GMSL_CSI_PORT_A : GMSL_CSI_PORT_B;

	err = of_property_read_string(gmsl, "src-csi-port", &str_value);
	if (err < 0) {
		dev_err(dev, "No src-csi-port found\n");
		goto error;
	}
	priv->g_ctx.src_csi_port =
		(!strcmp(str_value, "a")) ? GMSL_CSI_PORT_A : GMSL_CSI_PORT_B;

	err = of_property_read_string(gmsl, "csi-mode", &str_value);
	if (err < 0) {
		dev_err(dev, "No csi-mode found\n");
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
		dev_err(dev, "invalid csi mode\n");
		goto error;
	}

	err = of_property_read_string(gmsl, "serdes-csi-link", &str_value);
	if (err < 0) {
		dev_err(dev, "No serdes-csi-link found\n");
		goto error;
	}
//	priv->g_ctx.serdes_csi_link =
//		(!strcmp(str_value, "a")) ?
//			GMSL_SERDES_CSI_LINK_A : GMSL_SERDES_CSI_LINK_B;
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
		dev_err(dev, "No st-vc info\n");
		goto error;
	}
	priv->g_ctx.st_vc = value;

	err = of_property_read_u32(gmsl, "vc-id", &value);
	if (err < 0) {
		dev_err(dev, "No vc-id info\n");
		goto error;
	}
	priv->g_ctx.dst_vc = value;

	err = of_property_read_u32(gmsl, "num-lanes", &value);
	if (err < 0) {
		dev_err(dev, "No num-lanes info\n");
		goto error;
	}
	priv->g_ctx.num_csi_lanes = value;

	if (of_get_property(gmsl, "frame-sync-en", NULL)) {
		priv->g_ctx.frame_sync_en = true;
		dev_dbg(dev, "frame-sync-en found\n");
	}else {
		priv->g_ctx.frame_sync_en = false;
		dev_dbg(dev, "frame-sync-en not found\n");
	}

	priv->g_ctx.num_streams =
			of_property_count_strings(gmsl, "streams");
	if (priv->g_ctx.num_streams <= 0) {
		dev_err(dev, "No streams found\n");
		err = -EINVAL;
		goto error;
	}

	for (i = 0; i < priv->g_ctx.num_streams; i++) {
		of_property_read_string_index(gmsl, "streams", i,
						&str_value1[i]);
		if (!str_value1[i]) {
			dev_err(dev, "invalid stream info\n");
			goto error;
		}
		if (!strcmp(str_value1[i], "raw12")) {
			priv->g_ctx.streams[i].st_data_type =
							GMSL_CSI_DT_RAW_12;
		} else if (!strcmp(str_value1[i], "embed")) {
			priv->g_ctx.streams[i].st_data_type =
							GMSL_CSI_DT_EMBED;
		} else if (!strcmp(str_value1[i], "ued-u1")) {
			priv->g_ctx.streams[i].st_data_type =
							GMSL_CSI_DT_UED_U1;
		} else {
			dev_err(dev, "invalid stream data type\n");
			goto error;
		}
	}

	priv->g_ctx.s_dev = dev;

	return 0;

error:
	dev_err(dev, "board setup failed\n");
	return err;
}

static int ox03c10_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct tegracam_device *tc_dev;
	struct ox03c10 *priv;
	int err;

	dev_info(dev, "probing v4l2 sensor.\n");

	if (!IS_ENABLED(CONFIG_OF) || !node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct ox03c10), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}
	tc_dev = devm_kzalloc(dev,
			sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	priv->i2c_client = tc_dev->client = client;
	tc_dev->dev = dev;
	strncpy(tc_dev->name, "ox03c10", sizeof(tc_dev->name));
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &ox03c10_common_ops;
	tc_dev->v4l2sd_internal_ops = &ox03c10_subdev_internal_ops;
	tc_dev->tcctrl_ops = &ox03c10_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}

	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;

	tegracam_set_privdata(tc_dev, (void *)priv);

	err = ox03c10_board_setup(priv);
	if (err) {
		dev_err(dev, "board setup failed\n");
		return err;
	}

	/* Pair sensor to serializer dev */
	err = max9295_sdev_pair(priv->ser_dev, &priv->g_ctx);
	if (err) {
		dev_err(&client->dev, "gmsl ser pairing failed\n");
		return err;
	}

	/* Register sensor to deserializer dev */
	//err = max9296_sdev_register(priv->dser_dev, &priv->g_ctx);
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
	err = ox03c10_gmsl_serdes_setup(priv);
	if (err) {
		dev_err(&client->dev,
			"%s gmsl serdes setup failed\n", __func__);
		return err;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	dev_info(&client->dev, "Detected OX03C10 sensor\n");

	if(n_ox03c10_dev < 12){
		sen_ox03c10_listp[n_ox03c10_dev]=priv->s_data;
		n_ox03c10_dev++;
	}

	return 0;
}

static int ox03c10_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct ox03c10 *priv = (struct ox03c10 *)s_data->priv;

	ox03c10_gmsl_serdes_reset(priv);

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);

	return 0;
}

static const struct i2c_device_id ox03c10_id[] = {
	{ "ox03c10", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ox03c10_id);

static struct i2c_driver ox03c10_i2c_driver = {
	.driver = {
		.name = "ox03c10",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ox03c10_of_match),
	},
	.probe = ox03c10_probe,
	.remove = ox03c10_remove,
	.id_table = ox03c10_id,
};

static int __init ox03c10_init(void)
{
	mutex_init(&serdes_lock__);

	return i2c_add_driver(&ox03c10_i2c_driver);
}

static void __exit ox03c10_exit(void)
{
	mutex_destroy(&serdes_lock__);

	i2c_del_driver(&ox03c10_i2c_driver);
}

module_init(ox03c10_init);
module_exit(ox03c10_exit);

MODULE_DESCRIPTION("Media Controller driver for OX03C10");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_AUTHOR("Sudhir Vyas <svyas@nvidia.com");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.2.0.0922");
