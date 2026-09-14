/*
 * max96712.c - max96712 IO Expander driver
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
#include <media/max96712.h>

/* register specifics */

#define MAX96712_REG6_ADDR 0x06
#define MAX96712_BACKTOP12_ADDR 0x40b
#define MAX96712_REG26_ADDR 0x10
#define MAX96712_REG27_ADDR 0x11
#define MAX96712_CTRL1_ADDR 0x18
#define MAX96712_PWDN_PHYS_ADDR 0x8a2
#define MAX96712_PIPE_X_SRC_0_MAP_ADDR 0x90D
#define MAX96712_PIPE_X_DST_0_MAP_ADDR 0x90E
#define MAX96712_PIPE_X_SRC_1_MAP_ADDR 0x90F
#define MAX96712_PIPE_X_DST_1_MAP_ADDR 0x910
#define MAX96712_PIPE_X_SRC_2_MAP_ADDR 0x911
#define MAX96712_PIPE_X_DST_2_MAP_ADDR 0x912
#define MAX96712_TX11_PIPE_X_EN_ADDR 0x90B
#define MAX96712_TX45_PIPE_X_DST_CTRL_ADDR 0x92D

#define MAX96712_VIDEO_PIPE_SEL_0_ADDR 0xf0
#define MAX96712_VIDEO_PIPE_SEL_1_ADDR 0xf1
#define MAX96712_VIDEO_PIPE_SEL_2_ADDR 0xf2
#define MAX96712_VIDEO_PIPE_SEL_3_ADDR 0xf3
#define MAX96712_VIDEO_PIPE_EN_ADDR 0xf4

#define MAX96712_LANE_CTRL0_ADDR 0x90A
#define MAX96712_LANE_CTRL1_ADDR 0x94A
#define MAX96712_LANE_CTRL2_ADDR 0x98A
#define MAX96712_LANE_CTRL3_ADDR 0x9CA
#define MAX96712_DST_CSI_MODE_ADDR 0x8a0
#define MAX96712_LANE_MAP1_ADDR 0x8a3
#define MAX96712_LANE_MAP2_ADDR 0x8a4

#define MAX96712_ALLPHYS_NOSTDBY 0xF0

#define MAX96712_GMSL1_LINK_ADDR 0x06
#define MAX96712_YUV_MIX_ADDR 0x322

/* data defines */
#define MAX96712_CSI_MODE_4X2 0x1
#define MAX96712_CSI_MODE_2X4 0x4
#define MAX96712_LANE_MAP1_4X2 0x44
#define MAX96712_LANE_MAP2_4X2 0x44
#define MAX96712_LANE_MAP1_2X4 0xE4
#define MAX96712_LANE_MAP2_2X4 0xE4

#define MAX96712_LANE_CTRL_MAP(num_lanes, phy_mode) \
	((((num_lanes) << 6) | (phy_mode<<5)) & 0xF0)

#define MAX96712_ST_ID_SEL_INVALID 0xF

#define MAX96712_PHY1_CLK 0x2a

#define MAX96712_MAX_SOURCES 4
#define MAX96712_MAX_PIPES 8

#define MAX96712_PIPE_0 0
#define MAX96712_PIPE_1 1
#define MAX96712_PIPE_2 2
#define MAX96712_PIPE_3 3
#define MAX96712_PIPE_4 4
#define MAX96712_PIPE_5 5
#define MAX96712_PIPE_6 6
#define MAX96712_PIPE_7 7
#define MAX96712_PIPE_INVALID 0xF

#define MAX96712_CSI_CTRL_0 0
#define MAX96712_CSI_CTRL_1 1
#define MAX96712_CSI_CTRL_2 2
#define MAX96712_CSI_CTRL_3 3

#define MAX96712_INVAL_ST_ID 0xFF

/* Use reset value as per spec, confirm with vendor */
#define MAX96712_RESET_ST_ID 0x00

//#define CPHY_MODE 1

enum {
	GMSL2,
	GMSL2_3G,
	GMSL1,
	GMSL2_REG3G,
};

//u32 p_rest[4];

static int i2c_96712log_en = 1;

struct max96712_source_ctx {
	struct gmsl_link_ctx *g_ctx;
	bool st_enabled;
};

struct pipe_ctx {
	u32 id;
	u32 dt_type;
	u32 dst_csi_ctrl;
	u32 st_count;
	u32 st_id_sel;
};

struct max96712 {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	u32 num_src;
	u32 max_src;
	struct max96712_source_ctx sources[MAX96712_MAX_SOURCES];
	struct mutex lock;
	u32 sdev_ref;
	bool ctrl_setup_done;
	bool lane_setup;
	bool link_setup;
	bool frame_sync_en;
	struct pipe_ctx pipe[MAX96712_MAX_PIPES];
	u8 gmsl_mode;
	u8 csi_mode;
	u8 lane_mp1;
	u8 lane_mp2;
	int reset_gpio;
	int pw_ref;
	u32 dser_num;
	u32 mipi_clk;
	struct regulator *vdd_cam_1v2;
};

static int mipi_speed_Mhz;
module_param(mipi_speed_Mhz, int, 0644);

struct device *max96712_dev_listp[12]={NULL};
EXPORT_SYMBOL(max96712_dev_listp);
int n_max96712_dev=0;
EXPORT_SYMBOL(n_max96712_dev);

/* max2008X.c */
extern int n_poc_dev;

//static int max96712_write_reg(struct device *dev,
 int max96712_write_reg(struct device *dev,
	u16 addr, u8 val)
{
	struct max96712 *priv;
	int err;

	priv = dev_get_drvdata(dev);

	if (i2c_96712log_en)
		dev_info(dev, "%s:i2c write , 0x%x = %x\n",__func__, addr, val);

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_err(dev,
		"%s:i2c write failed, 0x%x = %x\n",
		__func__, addr, val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(100, 110);

	return err;
}
EXPORT_SYMBOL(max96712_write_reg);

 int max96712_read_reg(struct device *dev,
	u16 addr, u8 *val)
{
	struct max96712 *priv;
	int err;
	u32 reg_val = 0;

	priv = dev_get_drvdata(dev);

	//err = regmap_read(priv->regmap, addr, &reg_val);
	err = regmap_raw_read(priv->regmap, addr, &reg_val, 1);
	if (err){
		dev_err(dev, "%s:i2c read failed, 0x%x = XX\n", __func__, addr);
		return err;
	}
	*val = reg_val & 0xFF;

	if (i2c_96712log_en)
		dev_info(dev, "%s:i2c read , 0x%x = %x\n",__func__, addr, *val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(100, 110);

	return err;
}
EXPORT_SYMBOL(max96712_read_reg);


static int max96712_get_sdev_idx(struct device *dev,
			struct device *s_dev, int *idx)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	int i;
	int err = 0;

	mutex_lock(&priv->lock);
	for (i = 0; i < priv->num_src; i++) {
		if (priv->sources[i].g_ctx->s_dev == s_dev)
			break;
	}
	if (i == priv->num_src) {
		dev_err(dev, "no sdev found\n");
		err = -EINVAL;
		goto ret;
	}

	if (idx)
		*idx = i;

ret:
	mutex_unlock(&priv->lock);
	return err;
}

static void max96712_pipes_reset(struct max96712 *priv)
{
	/*
	 * This is default pipes combination. add more mappings
	 * for other combinations and requirements.
	 */
	struct pipe_ctx pipe_defaults[] = {
		/* configuration 1:
		{MAX96712_PIPE_0, GMSL_CSI_DT_RAW_12,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_1, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_2, GMSL_CSI_DT_RAW_12,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_3, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_4, GMSL_CSI_DT_EMBED,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_5, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_6, GMSL_CSI_DT_EMBED,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_7, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID}
		*/

		/* configuration 2:
		{MAX96712_PIPE_0, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_1, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_2, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_3, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_4, GMSL_CSI_DT_RAW_12,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_5, GMSL_CSI_DT_RAW_12,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_6, GMSL_CSI_DT_EMBED,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_7, GMSL_CSI_DT_EMBED,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID}
		*/

		/* configuration 3:*/
		{MAX96712_PIPE_0, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_1, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_2, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_3, GMSL_CSI_DT_YUV422_8,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_4, GMSL_CSI_DT_RAW_12,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_5, GMSL_CSI_DT_RAW_12,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_6, GMSL_CSI_DT_RAW_12,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID},
		{MAX96712_PIPE_7, GMSL_CSI_DT_RAW_12,MAX96712_CSI_CTRL_2, 0, MAX96712_INVAL_ST_ID}
	};

	/*
	 * Add DT props for num-streams and stream sequence, and based on that
	 * set the appropriate pipes defaults.
	 * For now default it supports "2 RAW12 and 2 EMBED" 1:1 mappings.
	 */
	memcpy(priv->pipe, pipe_defaults, sizeof(pipe_defaults));
}

static void max96712_reset_ctx(struct max96712 *priv)
{
	int i;

	priv->ctrl_setup_done = false;
	priv->link_setup = false;
	priv->lane_setup = false;
	max96712_pipes_reset(priv);
	for (i = 0; i < priv->num_src; i++)
		priv->sources[i].st_enabled = false;
}

int max96712_power_on(struct device *dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	int err = 0;

	mutex_lock(&priv->lock);
	if (priv->pw_ref == 0) {
		usleep_range(1, 2);
		if (priv->reset_gpio)
			gpio_set_value(priv->reset_gpio, 0);

		usleep_range(30, 50);

		if (priv->vdd_cam_1v2) {
			err = regulator_enable(priv->vdd_cam_1v2);
			if (unlikely(err))
				goto ret;
		}

		usleep_range(30, 50);

		/*exit reset mode: XCLR */
		if (priv->reset_gpio) {
			gpio_set_value(priv->reset_gpio, 0);
			usleep_range(30, 50);
			gpio_set_value(priv->reset_gpio, 1);
			usleep_range(30, 50);
		}

		/* delay to settle reset */
		msleep(20);
	}

	priv->pw_ref++;

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max96712_power_on);

void max96712_power_off(struct device *dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);
	priv->pw_ref--;

	if (priv->pw_ref == 0) {
		/* enter reset mode: XCLR */
		usleep_range(1, 2);
		if (priv->reset_gpio)
			gpio_set_value(priv->reset_gpio, 0);

		if (priv->vdd_cam_1v2)
			regulator_disable(priv->vdd_cam_1v2);
	}

	mutex_unlock(&priv->lock);
}
EXPORT_SYMBOL(max96712_power_off);

int max96712_setup_link(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	u32 link;
	int err = 0;
	int i;

	err = max96712_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev = %d\n", __func__, i);
	mutex_lock(&priv->lock);

	//disable csi output
	max96712_write_reg(dev, MAX96712_BACKTOP12_ADDR, 0x00);

	//fix it: disable all pipelines
	max96712_write_reg(dev, MAX96712_VIDEO_PIPE_EN_ADDR, 0x00);

	//for 6G bps camera or which intend to set 3G bps later, Set 6Gbps rate here.
	max96712_write_reg(dev, MAX96712_REG26_ADDR, 0x22);
	max96712_write_reg(dev, MAX96712_REG27_ADDR, 0x22);

	//for 3G bps (fixed) camera
	//fix it: set separately
	if (priv->gmsl_mode == GMSL2_3G) {
		max96712_write_reg(dev, MAX96712_REG26_ADDR, 0x11);  //Set 3Gbps rate
		max96712_write_reg(dev, MAX96712_REG27_ADDR, 0x11);  //Set 3Gbps rate
	}

	msleep(100);

	link = priv->sources[i].g_ctx->serdes_csi_link;

	//reset all data path
	max96712_write_reg(dev, MAX96712_CTRL1_ADDR, 0x0f);

	if (link == GMSL_SERDES_CSI_LINK_A) {
		err = max96712_write_reg(dev, MAX96712_REG6_ADDR, 0xf1);
	} else if (link == GMSL_SERDES_CSI_LINK_B) {
		err = max96712_write_reg(dev, MAX96712_REG6_ADDR, 0xf2);
	} else if (link == GMSL_SERDES_CSI_LINK_C) {
		err = max96712_write_reg(dev, MAX96712_REG6_ADDR, 0xf4);
	} else if (link == GMSL_SERDES_CSI_LINK_D) {
		err = max96712_write_reg(dev, MAX96712_REG6_ADDR, 0xf8);
	} else { /* Extend for DES having more than 4 GMSL links */
		dev_err(dev, "%s: invalid gmsl link\n", __func__);
		err = -EINVAL;
		goto ret;
	}

	#if 0
		if(priv->dser_num==0)
			gpio_set_value(p_rest[1], 0);
	//	if(priv->dser_num==1)
	//		gpio_set_value(p_rest[0], 0);
		if(priv->dser_num==2)
			gpio_set_value(p_rest[3], 0);
	//	if(priv->dser_num==3)
	//		gpio_set_value(p_rest[2], 0);
    #endif
	dev_dbg(dev, "%s: exit! sdev = %d\n", __func__, i);

	/* delay to settle link */
	//extend delay time from 100ms to 300ms, for HY009J des-integrated board.
	//otherwise, ser may not available at probe time.
	//is it still a problem with HY029?
	msleep(300);

	priv->link_setup = true;

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max96712_setup_link);

int max96712_setup_link_gmsl1(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	u32 link;
	int err = 0;
	int i;

	err = max96712_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev = %d\n", __func__, i);
	mutex_lock(&priv->lock);

	max96712_write_reg(dev,MAX96712_VIDEO_PIPE_EN_ADDR,0x0f);//enable pipe 0 1 2 3

	max96712_write_reg(dev,0x40b,0x00);  //disable MIPI output, CSI_OUT_EN-bit0
	max96712_write_reg(dev,0x006,0x0F);  //Reg??? config linkA & linkB as GMSL1 mode

	//max96712_write_reg(dev,0x003,0x40);  //Disable Uart1 for Fsync

	link = priv->sources[i].g_ctx->serdes_csi_link;

	//if (!priv->ctrl_setup_done) {
		if (link == GMSL_SERDES_CSI_LINK_A) {
			//max96712_write_reg(dev,0xB06,0xEF);	//Def:6f  HIGHIMM, HV_SRC: HIGHIMM enable
			max96712_write_reg(dev,0xB07,0x84);	//Def:00. BWS, HIBW: 22/24bit bus; config linkA HVEN=1 DBL=1 BWS=0
			max96712_write_reg(dev,0xB08,0x01);	//Def:21. GPI_EN: bit5, Enable/Disable GPI->GPO
			max96712_write_reg(dev,0xB0D,0x81);	//Def:00 I2C_LOC_ACK: bit7, 1-enable
			max96712_write_reg(dev,0xB0F,0x01);	//Def:09. DE_EN, PRBS_TYPE: DE_EN = 0;
			max96712_write_reg(dev,0x06,0x01);  //Def.03 Enable link A/B: A
		} else if (link == GMSL_SERDES_CSI_LINK_B) {
			//max96712_write_reg(dev,0xC06,0xEF);	//Def:6f  HIGHIMM, HV_SRC: HIGHIMM enable
			max96712_write_reg(dev,0xC07,0x84);	//Def:00. BWS, HIBW: 22/24bit bus; config linkA HVEN=1 DBL=1 BWS=0
			max96712_write_reg(dev,0xC08,0x01);	//Def:21. GPI_EN: bit5, Enable/Disable GPI->GPO
			max96712_write_reg(dev,0xC0D,0x81);	//Def:00 I2C_LOC_ACK: bit7, 1-enable
			max96712_write_reg(dev,0xC0F,0x01);	//Def:09. DE_EN, PRBS_TYPE: DE_EN = 0;
			max96712_write_reg(dev,0x06,0x02);  //Def.03 Enable link A/B: B
		}else if (link == GMSL_SERDES_CSI_LINK_C) {
			//max96712_write_reg(dev,0xd06,0xEF);	//Def:6f  HIGHIMM, HV_SRC: HIGHIMM enable
			max96712_write_reg(dev,0xd07,0x84);	//Def:00. BWS, HIBW: 22/24bit bus; config linkA HVEN=1 DBL=1 BWS=0
			max96712_write_reg(dev,0xd08,0x01);	//Def:21. GPI_EN: bit5, Enable/Disable GPI->GPO
			max96712_write_reg(dev,0xd0D,0x81);	//Def:00 I2C_LOC_ACK: bit7, 1-enable
			max96712_write_reg(dev,0xd0F,0x01);	//Def:09. DE_EN, PRBS_TYPE: DE_EN = 0;
			max96712_write_reg(dev,0x06,0x04);  //Def.03 Enable link c
		}else if (link == GMSL_SERDES_CSI_LINK_D) {
			//max96712_write_reg(dev,0xe06,0xEF);	//Def:6f  HIGHIMM, HV_SRC: HIGHIMM enable
			max96712_write_reg(dev,0xe07,0x84);	//Def:00. BWS, HIBW: 22/24bit bus; config linkA HVEN=1 DBL=1 BWS=0
			max96712_write_reg(dev,0xe08,0x01);	//Def:21. GPI_EN: bit5, Enable/Disable GPI->GPO
			max96712_write_reg(dev,0xe0D,0x81);	//Def:00 I2C_LOC_ACK: bit7, 1-enable
			max96712_write_reg(dev,0xe0F,0x01);	//Def:09. DE_EN, PRBS_TYPE: DE_EN = 0;
			max96712_write_reg(dev,0x06,0x08);  //Def.03 Enable link d
		} else { /* Extend for DES having more than two GMSL links */
			dev_err(dev, "%s: invalid gmsl link\n", __func__);
			err = -EINVAL;
			goto ret;
		}

	max96712_write_reg(dev,0xb06,0xEF);
	max96712_write_reg(dev,0xc06,0xEF);
	max96712_write_reg(dev,0xd06,0xEF);
	max96712_write_reg(dev,0xe06,0xEF);

	#if 0
		if(priv->dser_num==0)
			gpio_set_value(p_rest[1], 0);
	//	if(priv->dser_num==1)
	//		gpio_set_value(p_rest[0], 0);
		if(priv->dser_num==2)
			gpio_set_value(p_rest[3], 0);
	//	if(priv->dser_num==3)
	//		gpio_set_value(p_rest[2], 0);
    #endif
	dev_dbg(dev, "%s: exit! sdev = %d\n", __func__, i);

		/* delay to settle link */
		msleep(100);

		priv->link_setup = true;
//	}

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max96712_setup_link_gmsl1);


int max96712_setup_control(struct device *dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	int err = 0;

	dev_dbg(dev, "%s: enter!\n", __func__);
	mutex_lock(&priv->lock);

	if (!priv->link_setup) {
		dev_err(dev, "%s: invalid state\n", __func__);
		err = -EINVAL;
		goto error;
	}

	if (priv->gmsl_mode == GMSL2_REG3G) {
		max96712_write_reg(dev, MAX96712_REG26_ADDR, 0x11);  //Set 3Gbps rate
		max96712_write_reg(dev, MAX96712_REG27_ADDR, 0x11);  //Set 3Gbps rate
		msleep(100);
	}

	max96712_write_reg(dev,
		MAX96712_PWDN_PHYS_ADDR, MAX96712_ALLPHYS_NOSTDBY);

#if 0
		if(priv->dser_num==0)
			gpio_set_value(p_rest[1], 1);
	//	if(priv->dser_num==1)
	//		gpio_set_value(p_rest[0], 1);
		if(priv->dser_num==2)
			gpio_set_value(p_rest[3], 1);
	//	if(priv->dser_num==3)
	//		gpio_set_value(p_rest[2], 1);
#endif
		priv->ctrl_setup_done = true;
	//}

	priv->sdev_ref++;

error:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max96712_setup_control);

int max96712_reset_control(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	int err = 0;

	mutex_lock(&priv->lock);
	dev_dbg(dev, "%s: enter\n", __func__);

	if (!priv->ctrl_setup_done) {
		dev_dbg(dev, "%s: device is powered off\n", __func__);
		goto ret;
	}

	priv->sdev_ref--;
	if (priv->sdev_ref == 0) {
		max96712_reset_ctx(priv);
		max96712_write_reg(dev, MAX96712_CTRL1_ADDR, 0xf0);

		//reset device
		//max96712_write_reg(dev, 0x13, 0x40);

		/* delay to settle reset */
		msleep(100);
	}

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max96712_reset_control);

int max96712_sdev_register(struct device *dev, struct gmsl_link_ctx *g_ctx)
{
	struct max96712 *priv = NULL;
	int i;
	int err = 0;

	if (!dev || !g_ctx || !g_ctx->s_dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	priv = dev_get_drvdata(dev);

	mutex_lock(&priv->lock);

	if (priv->num_src > priv->max_src) {
		dev_err(dev,
			"%s: MAX96712 inputs size exhausted\n", __func__);
		err = -ENOMEM;
		goto error;
	}

	/* Check csi mode compatibility */
	if (!((priv->csi_mode == MAX96712_CSI_MODE_2X4) ?
			((g_ctx->csi_mode == GMSL_CSI_1X4_MODE) ||
				(g_ctx->csi_mode == GMSL_CSI_2X4_MODE)) :
			((g_ctx->csi_mode == GMSL_CSI_2X2_MODE) ||
				(g_ctx->csi_mode == GMSL_CSI_4X2_MODE)))) {
		dev_err(dev,
			"%s: csi mode not supported\n", __func__);
		err = -EINVAL;
		goto error;
	}

	for (i = 0; i < priv->num_src; i++) {
		if (g_ctx->serdes_csi_link ==
			priv->sources[i].g_ctx->serdes_csi_link) {
			dev_err(dev,
				"%s: serdes csi link is in use\n", __func__);
			err = -EINVAL;
			goto error;
		}
		/*
		 * All sdevs should have same num-csi-lanes regardless of
		 * dst csi port selected.
		 * Later if there is any usecase which requires each port
		 * to be configured with different num-csi-lanes, then this
		 * check should be performed per port.
		 */
		if (g_ctx->num_csi_lanes !=
				priv->sources[i].g_ctx->num_csi_lanes) {
			dev_err(dev,
				"%s: csi num lanes mismatch\n", __func__);
			err = -EINVAL;
			goto error;
		}
	}

	priv->sources[priv->num_src].g_ctx = g_ctx;
	priv->sources[priv->num_src].st_enabled = false;

	priv->num_src++;

	if(n_max96712_dev < 12){
		max96712_dev_listp[n_max96712_dev]=dev;
		n_max96712_dev++;
	}

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max96712_sdev_register);

int max96712_sdev_unregister(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = NULL;
	int err = 0;
	int i = 0;

	if (!dev || !s_dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	priv = dev_get_drvdata(dev);
	mutex_lock(&priv->lock);

	if (priv->num_src == 0) {
		dev_err(dev, "%s: no source found\n", __func__);
		err = -ENODATA;
		goto error;
	}

	for (i = 0; i < priv->num_src; i++) {
		if (s_dev == priv->sources[i].g_ctx->s_dev) {
			priv->sources[i].g_ctx = NULL;
			priv->num_src--;
			break;
		}
	}

	if (i == priv->num_src) {
		dev_err(dev,
			"%s: requested device not found\n", __func__);
		err = -EINVAL;
		goto error;
	}

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max96712_sdev_unregister);

int max96712_set_tx_rate_3g(struct device *dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);

	if (!dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	if (priv->gmsl_mode == GMSL2_REG3G) {
		max96712_write_reg(dev, MAX96712_REG26_ADDR, 0x11);  //Set 3Gbps rate
		max96712_write_reg(dev, MAX96712_REG27_ADDR, 0x11);  //Set 3Gbps rate
		msleep(100);
	}

	return 0;
}
EXPORT_SYMBOL(max96712_set_tx_rate_3g);

int max96712_link_splitter(struct device *dev, u8 splitter_mode)
{
	u8 value;

	if (!dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	if (splitter_mode)
	{
		//fix it:
		//enable necessary link(s)
		max96712_read_reg(dev, MAX96712_REG6_ADDR, &value);
		max96712_write_reg(dev, MAX96712_REG6_ADDR, (0xf0 | value | splitter_mode));
		max96712_write_reg(dev, MAX96712_CTRL1_ADDR, splitter_mode);

		//extend delay time from 50ms to 300ms, for HY009J des-integrated board.
		//otherwise, ser may not available at probe time.
		//is it still a problem with HY029?
		msleep(300);
	}else{
		max96712_write_reg(dev, MAX96712_REG6_ADDR, (0xf0 | splitter_mode));
	}

	return 0;

}
EXPORT_SYMBOL(max96712_link_splitter);

int max96712_link_reset(struct device *dev, u8 link)
{
	if (!dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	//fix it:
	max96712_write_reg(dev, MAX96712_REG6_ADDR, (0xf0 | link));
	max96712_write_reg(dev, MAX96712_CTRL1_ADDR, link);

	msleep(50);

	return 0;

}
EXPORT_SYMBOL(max96712_link_reset);

int max96712_link_splitter_gmsl1(struct device *dev, u8 splitter_mode)
{

	if (!dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	if (splitter_mode)
	{
		max96712_write_reg(dev, MAX96712_GMSL1_LINK_ADDR, splitter_mode);
		msleep(50);
	}

	return 0;

}
EXPORT_SYMBOL(max96712_link_splitter_gmsl1);

static int max96712_get_available_pipe(struct device *dev,
				u32 st_data_type, u32 dst_csi_port)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < MAX96712_MAX_PIPES; i++) {
		/*
		 * TODO: Enable a pipe for multi stream configuration having
		 * similar stream data type. For now use st_count as a flag
		 * for 1 to 1 mapping in pipe and stream data type, same can
		 * be extended as count for many to 1 mapping. Would also need
		 * few more checks such as input stream id select, dst port etc.
		 */
		if ((priv->pipe[i].dt_type == st_data_type) &&
			((dst_csi_port == GMSL_CSI_PORT_A) ?
				(priv->pipe[i].dst_csi_ctrl ==
					MAX96712_CSI_CTRL_0) ||
				(priv->pipe[i].dst_csi_ctrl ==
					MAX96712_CSI_CTRL_1) :
				(priv->pipe[i].dst_csi_ctrl ==
					MAX96712_CSI_CTRL_2) ||
				(priv->pipe[i].dst_csi_ctrl ==
					MAX96712_CSI_CTRL_3)) &&
			(!priv->pipe[i].st_count))
			break;
	}

	if (i == MAX96712_MAX_PIPES) {
		dev_err(dev, "%s: all pipes are busy\n", __func__);
		return -ENOMEM;
	}

	return i;
}

struct reg_pair {
	u16 addr;
	u8 val;
};

static int max96712_setup_pipeline(struct device *dev,
		struct gmsl_link_ctx *g_ctx)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	struct gmsl_stream *g_stream;
	struct reg_pair *map_list;
	u32 arr_sz = 0;
	int pipe_id = 0;
	u32 i = 0;
	u32 j = 0;
	u32 vc_idx = 0;
	dev_dbg(dev, "%s: enter!\n", __func__);

	for (i = 0; i < g_ctx->num_streams; i++) {
		/* Base data type mapping: pipeX/RAW12/CSICNTR1 */
		struct reg_pair map_pipe_raw12[] = {
			/* addr, val */
			{MAX96712_TX11_PIPE_X_EN_ADDR, 0x7},
			{MAX96712_TX45_PIPE_X_DST_CTRL_ADDR, 0x15},
			{MAX96712_PIPE_X_SRC_0_MAP_ADDR, 0x2C},
			{MAX96712_PIPE_X_DST_0_MAP_ADDR, 0x2C},
			{MAX96712_PIPE_X_SRC_1_MAP_ADDR, 0x00},
			{MAX96712_PIPE_X_DST_1_MAP_ADDR, 0x00},
			{MAX96712_PIPE_X_SRC_2_MAP_ADDR, 0x01},
			{MAX96712_PIPE_X_DST_2_MAP_ADDR, 0x01},
		};
		struct reg_pair map_pipe_yuv422_8[] = {
			/* addr, val */
			{MAX96712_TX11_PIPE_X_EN_ADDR, 0x7},
			{MAX96712_TX45_PIPE_X_DST_CTRL_ADDR, 0x15},
			{MAX96712_PIPE_X_SRC_0_MAP_ADDR, 0x1E},
			{MAX96712_PIPE_X_DST_0_MAP_ADDR, 0x1E},
			{MAX96712_PIPE_X_SRC_1_MAP_ADDR, 0x00},
			{MAX96712_PIPE_X_DST_1_MAP_ADDR, 0x00},
			{MAX96712_PIPE_X_SRC_2_MAP_ADDR, 0x01},
			{MAX96712_PIPE_X_DST_2_MAP_ADDR, 0x01},
		};

		/* Base data type mapping: pipeX/EMBED/CSICNTR1 */
		struct reg_pair map_pipe_embed[] = {
			/* addr, val */
			{MAX96712_TX11_PIPE_X_EN_ADDR, 0x7},
			{MAX96712_TX45_PIPE_X_DST_CTRL_ADDR, 0x15},
			{MAX96712_PIPE_X_SRC_0_MAP_ADDR, 0x12},
			{MAX96712_PIPE_X_DST_0_MAP_ADDR, 0x12},
			{MAX96712_PIPE_X_SRC_1_MAP_ADDR, 0x00},
			{MAX96712_PIPE_X_DST_1_MAP_ADDR, 0x00},
			{MAX96712_PIPE_X_SRC_2_MAP_ADDR, 0x01},
			{MAX96712_PIPE_X_DST_2_MAP_ADDR, 0x01},
		};

		g_stream = &g_ctx->streams[i];
		g_stream->des_pipe = MAX96712_PIPE_INVALID;

		if (g_stream->st_data_type == GMSL_CSI_DT_RAW_12) {
			map_list = map_pipe_raw12;
			arr_sz = ARRAY_SIZE(map_pipe_raw12);
		} else if (g_stream->st_data_type == GMSL_CSI_DT_YUV422_8) {
			map_list = map_pipe_yuv422_8;
			arr_sz = ARRAY_SIZE(map_pipe_yuv422_8);
		} else if (g_stream->st_data_type == GMSL_CSI_DT_EMBED) {
			map_list = map_pipe_embed;
			arr_sz = ARRAY_SIZE(map_pipe_embed);
		} else if (g_stream->st_data_type == GMSL_CSI_DT_UED_U1) {
			dev_dbg(dev,
				"%s: No mapping for GMSL_CSI_DT_UED_U1\n",
				__func__);
			continue;
		} else {
			dev_err(dev, "%s: Invalid data type\n", __func__);
			return -EINVAL;
		}

		pipe_id = max96712_get_available_pipe(dev,
				g_stream->st_data_type, g_ctx->dst_csi_port);

		dev_info(dev,"%s: pipe_id = %d\n", __func__, pipe_id);

		if (pipe_id < 0)
			return pipe_id;

		/* set registers of MIPI_TX groups 0-7
		 * e.g. for MIPI_TX group 0 <-- YUV422 data in video pipe 0
		 * 0x90b = 7: enable 0,1,2 mapping
		 * 0x92d = 15: set all 3 mapping destination to be MIPI PHY Controller 1
		 * 0x90d = 1e: MAP_SRC_0: 0b00 011110: VC=0, DT=1e
		 * 0x90e = 1e: MAP_DST_0: VC=0, DT=1e
		 * 0x90f = 0
		 * 0x910 = 0
		 * 0x911 = 1
		 * 0x912 = 1
		 *
		 * e.g. for MIPI TX group 1 <-- YUV422 data in video pipe 1
		 * 0x94b = 7
		 * 0x96d = 15
		 * 0x94d = 1e: MAP_SRC_0: 0b00 011110: VC=0, DT=1e
		 * 0x94e = 5e: MAP_DST_0: 0b01 011110: VC=1, DT=1e
		 * 0x94f = 0
		 * 0x950 = 40
		 * 0x951 = 1
		 * 0x952 = 41
		 *
		 * e.g. for MIPI TX group 2 <-- YUV422 data in video pipe 2
		 * 0x98e = de : 0b11 011110: VC=3, DT=1e
		 *
		 * e.g. for MIPI TX group 3 <-- YUV422 data in video pipe 3
		 * 0x9ce = 9e : 0b10 011110: VC=2, DT=1e
		 *
		 * e.g. for MIPI TX group 4 <-- RAW12 data in video pipe 4
		 * 0xa0e = 2c : VC=0, DT=2c
		 *
		 */

		for (j = 0, vc_idx = 3; j < arr_sz; j++, vc_idx += 2) {
			/* update pipe configuration */
			map_list[j].addr += (0x40 * pipe_id);
			/* update vc id configuration */
			if (vc_idx < arr_sz)
				map_list[vc_idx].val |= (g_ctx->dst_vc << 6);

			max96712_write_reg(dev, map_list[j].addr,
						map_list[j].val);
		}

		/* Set stream id select input */
		if (g_stream->st_id_sel == GMSL_ST_ID_UNUSED) {
			dev_err(dev, "%s: Invalid stream st_id_sel\n",
				__func__);
			return -EINVAL;
		}

		g_stream->des_pipe = pipe_id;

		/* Update pipe internals */
		priv->pipe[pipe_id].st_count++;
		priv->pipe[pipe_id].st_id_sel = g_stream->st_id_sel;
		dev_info(dev,"%s: pipe[%d].st_count = %d\n", __func__, pipe_id, priv->pipe[pipe_id].st_count);
		dev_info(dev,"%s: pipe[%d].st_id_sel = %d\n", __func__, pipe_id, priv->pipe[pipe_id].st_id_sel);

		//fix it
		//set static value here, or leave them to max96712_start_streaming()
		//chip default: 0x62, 0xea, 0x40, 0xc8

		//e.g. 0x20: 0b 0010 0000: set pipe 0 for link A stream 0, pipe 1 for link A stream 2
		//e.g. 0x02: 0b 0000 0010: set pipe 0 for link A stream 2, pipe 1 for link A stream 0
		//stream for ANC data?
		//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_0_ADDR, 0x20);
		//e.g. 0x72: 0b 0111 0010: set pipe 0 for link A stream 2, pipe 1 for link B stream 3
		//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_0_ADDR, 0x72);

		//e.g. 0xfb: 0b 1111 1011: set pipe 2 for link C stream 3, enable pipe 3 for link D stream 3
		//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_1_ADDR, 0xfb);
		//e.g. 0xee: 0b 1110 1110: set pipe 2 for link D stream 2, enable pipe 3 for link D stream 2
		//in this case, it's ok if just enable one pipe (2 or 3) by register 0xf4
		//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_1_ADDR, 0);

		//e.g. 0x42: 0b 0100 0010: set pipe 4 for link A stream 2, enable pipe 5 for link B stream 0
		//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_2_ADDR, 0);

		//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_3_ADDR, 0);

		dev_dbg(dev, "%s: exit!\n", __func__);
	}

	return 0;
}

int max96712_start_streaming(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	struct gmsl_stream *g_stream;
	int err = 0;
	int i = 0;
	u8 value=0;

	err = max96712_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	g_ctx = priv->sources[i].g_ctx;

	for (i = 0; i < g_ctx->num_streams; i++) {
		g_stream = &g_ctx->streams[i];

		if (g_stream->des_pipe != MAX96712_PIPE_INVALID){
#if defined(MAX96717_COMPAT)
			//for max96717 which only have pipe Z.

			//e.g. 0x7b: 0b 0111 1011: set pipe 0 for link C stream 3, pipe 1 for link B stream 3
			//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_0_ADDR, 0x7b);

			//link A: stream 2; link B/C/D: stream 3
			if(g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
				g_stream->st_id_sel=2;
			}else{
				g_stream->st_id_sel=3;
			}

#endif
			if(g_stream->des_pipe == 0 || g_stream->des_pipe == 1){
				max96712_read_reg(dev, MAX96712_VIDEO_PIPE_SEL_0_ADDR, &value);
				if(g_stream->des_pipe == 0)
					value&=0xf0;
				else
					value&=0xf;
				max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_0_ADDR, value | ((g_ctx->serdes_csi_link-1) << 2 | g_stream->st_id_sel) << (g_stream->des_pipe*4));
			}else if(g_stream->des_pipe == 2 || g_stream->des_pipe == 3){
				max96712_read_reg(dev, MAX96712_VIDEO_PIPE_SEL_1_ADDR, &value);
				if(g_stream->des_pipe == 2)
					value&=0xf0;
				else
					value&=0xf;
				max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_1_ADDR, value | ((g_ctx->serdes_csi_link-1) << 2 | g_stream->st_id_sel) << ((g_stream->des_pipe-2)*4));
			}else if(g_stream->des_pipe == 4 || g_stream->des_pipe == 5){
				max96712_read_reg(dev, MAX96712_VIDEO_PIPE_SEL_2_ADDR, &value);
				if(g_stream->des_pipe == 4)
					value&=0xf0;
				else
					value&=0xf;
				max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_2_ADDR, value | ((g_ctx->serdes_csi_link-1) << 2 | g_stream->st_id_sel) << ((g_stream->des_pipe-4)*4));
			}else if(g_stream->des_pipe == 6 || g_stream->des_pipe == 7){
				max96712_read_reg(dev, MAX96712_VIDEO_PIPE_SEL_3_ADDR, &value);
				if(g_stream->des_pipe == 6)
					value&=0xf0;
				else
					value&=0xf;
				max96712_write_reg(dev, MAX96712_VIDEO_PIPE_SEL_3_ADDR, value | ((g_ctx->serdes_csi_link-1) << 2 | g_stream->st_id_sel) << ((g_stream->des_pipe-6)*4));
			}else{
				g_stream->des_pipe = MAX96712_PIPE_INVALID;
			}

			//enable necessary pipe(s)
			max96712_read_reg(dev, MAX96712_VIDEO_PIPE_EN_ADDR, &value);
			max96712_write_reg(dev, MAX96712_VIDEO_PIPE_EN_ADDR, value|(1<<g_stream->des_pipe));
		}
	}

	//enable all pipes
	//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_EN_ADDR, 0xff);

	max96712_write_reg(dev, MAX96712_BACKTOP12_ADDR, 0x02);

	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max96712_start_streaming);

int max96712_start_streaming_gmsl1(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	int err = 0;
	int i = 0;

	err = max96712_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);

	max96712_write_reg(dev, 0x41a, 0xf0);

	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max96712_start_streaming_gmsl1);

int max96712_stop_streaming(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	struct gmsl_stream *g_stream;
	int err = 0;
	int i = 0;
	u8 value;

	err = max96712_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;
	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);

	g_ctx = priv->sources[i].g_ctx;

	for (i = 0; i < g_ctx->num_streams; i++) {
		g_stream = &g_ctx->streams[i];

		if (g_stream->des_pipe != MAX96712_PIPE_INVALID){
			//fix it
			//max96712_write_reg(dev, g_stream->des_pipe, MAX96712_RESET_ST_ID);
			//disable necessary pipe(s)
			max96712_read_reg(dev, MAX96712_VIDEO_PIPE_EN_ADDR, &value);
			max96712_write_reg(dev, MAX96712_VIDEO_PIPE_EN_ADDR, value&(~(1<<g_stream->des_pipe)));
		}
	}

	//max96712_write_reg(dev, MAX96712_VIDEO_PIPE_EN_ADDR, 0x00);

	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max96712_stop_streaming);

int max96712_stop_streaming_gmsl1(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	int err = 0;
	int i = 0;
	u8 value = 0;

	err = max96712_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;
	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	max96712_read_reg(dev, 0x41a, &value);
	max96712_write_reg(dev, 0x41a, value & (~(1<<(4+i))));
	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max96712_stop_streaming_gmsl1);

int max96712_setup_streaming(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	int err = 0;
	int i = 0;
	u16 lane_ctrl_addr;
	u32 cphy_mipi_clk = 0;

	err = max96712_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	if (priv->sources[i].st_enabled)
		goto ret;

	g_ctx = priv->sources[i].g_ctx;

	err = max96712_setup_pipeline(dev, g_ctx);
	if (err)
		goto ret;

	dev_dbg(dev, "%s: dst_csi_port = %d\n", __func__, g_ctx->dst_csi_port);
	/* Derive CSI lane map register */
	switch(g_ctx->dst_csi_port) {
	case GMSL_CSI_PORT_A:
	case GMSL_CSI_PORT_D:
		lane_ctrl_addr = MAX96712_LANE_CTRL1_ADDR;
		break;
	case GMSL_CSI_PORT_B:
	case GMSL_CSI_PORT_E:
		lane_ctrl_addr = MAX96712_LANE_CTRL2_ADDR;
		break;
	case GMSL_CSI_PORT_C:
		lane_ctrl_addr = MAX96712_LANE_CTRL0_ADDR;
		break;
	case GMSL_CSI_PORT_F:
		lane_ctrl_addr = MAX96712_LANE_CTRL3_ADDR;
		break;
	default:
		dev_err(dev, "%s: invalid gmsl csi port!\n", __func__);
		err = -EINVAL;
		goto ret;
	};

	/*
	 * rewrite num_lanes to same dst port should not be an issue,
	 * as the device compatibility is already
	 * checked during sdev registration against the des properties.
	 */
	//max96712_write_reg(dev, lane_ctrl_addr, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1));
#if defined(CPHY_MODE)
	max96712_write_reg(dev, 0x94a, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1, 1));
	max96712_write_reg(dev, 0x98a, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1, 1));

	max96712_write_reg(dev, 0x90a, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1, 1));
	max96712_write_reg(dev, 0x9ca, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1, 1));
#else
	max96712_write_reg(dev, 0x94a, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1, 0));
	max96712_write_reg(dev, 0x98a, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1, 0));

	max96712_write_reg(dev, 0x90a, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1, 0));
	max96712_write_reg(dev, 0x9ca, MAX96712_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1, 0));
#endif

	/*
	 * HY029 D-PHY:
	 *
	 * 0x8A0: 0x04
	 * MIPI PHY 2x4 mode: MIPI output configured as two ports with four data lanes each.
	 * PHY0 and PHY1 combined, and PHY2 and PHY3 combined.
	 *
	 * 0x8A2: 0xF0
	 *
	 * 0x8A3: 0xE4: 0b1110 0b0100
	 * PHY0: 0x4
	 * Map D1 to data lane D1
	 * Map D0 to data lane D0
	 *
	 * PHY1: 0xE
	 * Map D0 to data lane D2
	 * Map D1 to data lane D3
	 *
	 * PHY2/3: also configured and enabled but NC to SOC
	 *
	 * HY026 C-PHY:
	 *
	 * two SOCs need PHY copy?
	 *
	 * 0x8A9: 0xC0: 0b1 10 00 000: copy PHY0 to 2
	 * 0x8AA: 0xE8: 0b1 11 01 000: copy PHY1 to 3
	 *
	 * see schematics for details.
	 */

	if (!priv->lane_setup) {
		max96712_write_reg(dev,
			MAX96712_DST_CSI_MODE_ADDR, priv->csi_mode);
		max96712_write_reg(dev,
			MAX96712_LANE_MAP1_ADDR, priv->lane_mp1);
		max96712_write_reg(dev,
			MAX96712_LANE_MAP2_ADDR, priv->lane_mp2);

		//max96712_write_reg(dev,0x8a9,0xc0);
		//max96712_write_reg(dev,0x8aa,0xe8);

		// mipi clock seting
		//cphy_mipi_clk = priv->mipi_clk / 100;
		if (mipi_speed_Mhz > 0) {
				cphy_mipi_clk = mipi_speed_Mhz / 100;
		}else{
				cphy_mipi_clk = priv->mipi_clk / 100;
		}
		max96712_write_reg(dev,0x415,cphy_mipi_clk|0x20);
		max96712_write_reg(dev,0x418,cphy_mipi_clk|0x20);
		max96712_write_reg(dev,0x41b,cphy_mipi_clk|0x20);
		max96712_write_reg(dev,0x41e,cphy_mipi_clk|0x20);

		if (g_ctx->frame_sync_en) {
			if(g_ctx->sensor_model == SENSOR_IMX390_RAW_BL){
//				max96712_write_reg(dev,0x313,0x13); //GPIO_OUT=1, GPIO_TX_EN=1, GPIO_OUT_DIS=1, why?
//				max96712_write_reg(dev,0x314,0x26); //GPIO_OUT_TYPE=1, GPIO_TX_ID=6
//				max96712_write_reg(dev,0x315,0x86); //OVR_RES_CFG=1, GPIO_RX_ID=6
//				max96712_write_reg(dev,0x03,0x40);
			}else{
				//max96712_write_reg(dev,0x03,0x40);		//disabled UART TX  let MFP6 use GPIO function
#if !defined(MFP7_MFP8_COMPAT)
				/* setup  MFP6 as  GPIO transmission , GPIO ID = 6 */
				//max96712_write_reg(dev,0x313,0x13); //GPIO_OUT=1, GPIO_TX_EN=1, GPIO_OUT_DIS=1, why?
				//max96712_write_reg(dev,0x314,0x26); //GPIO_OUT_TYPE=1, GPIO_TX_ID=6
				//max96712_write_reg(dev,0x315,0x86); //OVR_RES_CFG=1, GPIO_RX_ID=6
#else
				/* setup  MFP6 as  GPIO transmission , GPIO ID = 7 */
				//max96712_write_reg(dev,0x313,0x83);
				//max96712_write_reg(dev,0x314,0xA7);
#endif
				//max96712_write_reg(dev,0x03,0x40);		//disabled UART TX  let MFP6 use GPIO function
			}

			/* max96712-GPIO0 --> max9295-GPIO0(by default)*/
			if (g_ctx->sensor_model == SENSOR_IMX390_RAW_SG) {
					max96712_write_reg(dev,0x300,0x12); //GPIO_OUT=1, GPIO_TX_EN=1, GPIO_OUT_DIS=0
					max96712_write_reg(dev,0x301,0x60); //PULL UP, GPIO_OUT_TYPE=1, GPIO_TX_ID=0
			}

			/* max96712-GPIO7 --> max9295-GPIO7(by default)*/
			if (g_ctx->sensor_model == SENSOR_E003A_YUV_3G || g_ctx->sensor_model == SENSOR_E003A_YUV_ET || g_ctx->sensor_model == SENSOR_F008AX_YUV_ET) {
					max96712_write_reg(dev,0x316,0x12); //GPIO_OUT=1, GPIO_TX_EN=1, GPIO_OUT_DIS=0
					max96712_write_reg(dev,0x317,0x67); //PULL UP, GPIO_OUT_TYPE=1, GPIO_TX_ID=0
			}

		}

		priv->lane_setup = true;
	}

	priv->sources[i].st_enabled = true;
	dev_dbg(dev, "%s: exit!\n", __func__);

ret:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max96712_setup_streaming);

int max96712_setup_streaming_gmsl1(struct device *dev, struct device *s_dev)
{
	struct max96712 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	int err = 0;
	int i = 0;
	int offset = 0;
	//u16 lane_ctrl_addr;
	u32 dphy_mipi_clk = 0;

	err = max96712_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	if (priv->sources[i].st_enabled)
		goto ret;

	g_ctx = priv->sources[i].g_ctx;

	max96712_write_reg(dev,0x8a3, 0xe4);
	max96712_write_reg(dev,0x8a4, 0xe4);

	max96712_write_reg(dev,0x90a, 0xc0);
	max96712_write_reg(dev,0x944, 0xc0);
	max96712_write_reg(dev,0x984, 0xc0);
	max96712_write_reg(dev,0x9c4, 0xc0);

	max96712_write_reg(dev,0x40b,0x42);	//pipeline-0 BPP Datatypes = x2A, x10-12, x31-37, CSI_OUT_EN-bit1
	max96712_write_reg(dev,0x411,0x48);	//pipeline-1 BPP Datatypes = x2A, x10-12, x31-37
	max96712_write_reg(dev,0x412,0x20);	//pipeline-2,3 BPP Datatypes = x2A, x10-12, x31

	max96712_write_reg(dev,0x40e,0x5e);	// 0: YUV422-8
	max96712_write_reg(dev,0x40f,0x7e);	// 1: YUV422-8
	max96712_write_reg(dev,0x410,0x7a);	// 2,3: YUV422-8

	max96712_write_reg(dev,0x40c,0x00);	
	max96712_write_reg(dev,0x40d,0x00);	//Soft override does not work on pipe2, 3, need to add the vc_id distinction when configuring pipe below

	offset =i*0x40;

	//PIPE X route: X -- CSI --DPHY
	max96712_write_reg(dev,0x90b + offset,0x07);	// MAP_EN_L: map0 ~ map7
	max96712_write_reg(dev,0x90d + offset,0x1e|(0x00<<6));	// VC&DT src map0
	max96712_write_reg(dev,0x90e + offset,0x1e|(i<<6));	// VC&DT dst map0
	max96712_write_reg(dev,0x90f + offset,0x00);	// VC&DT src map1
	max96712_write_reg(dev,0x910 + offset,i<<6);	// VC&DT dst map1
	max96712_write_reg(dev,0x911 + offset,0x01);	// VC&DT src map2
	max96712_write_reg(dev,0x912 + offset,0x01|(i<<6));	// VC&DT dst map2
	//max96712_write_reg(dev,0x413 + offset,0x02);	// VC&DT src map3
	//max96712_write_reg(dev,0x414 + offset,0x02);	// VC&DT dst map3
	//max96712_write_reg(dev,0x415 + offset,0x03);	// VC&DT src map4
	//max96712_write_reg(dev,0x416 + offset,0x03);	// VC&DT dst map4
	max96712_write_reg(dev,0x92d + offset,0x15);	// MAP_DPHY_DEST 0 ~ 3: VC&DT src map0 ~ map3 -> DPHY-1
	//max96712_write_reg(priv->dser_dev,0x42e + offset,0x01);	// MAP_DPHY_DEST 4 ~ 7: VC&DT src 4 -> DPHY-1

	// mipi clock seting
	dphy_mipi_clk = priv->mipi_clk / 100;
	max96712_write_reg(dev, 0x415, dphy_mipi_clk|0xe0);
	max96712_write_reg(dev, 0x418,dphy_mipi_clk|0xe0);
	max96712_write_reg(dev, 0x41b, dphy_mipi_clk|0x20);
	max96712_write_reg(dev, 0x41e, dphy_mipi_clk|0x20);

	priv->sources[i].st_enabled = true;
	dev_info(dev, "%s: exit!\n", __func__);

ret:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max96712_setup_streaming_gmsl1);

const struct of_device_id max96712_of_match[] = {
	{ .compatible = "nvidia,max96712", },
	{ },
};
MODULE_DEVICE_TABLE(of, max96712_of_match);

static int max96712_parse_dt(struct max96712 *priv,
				struct i2c_client *client)
{
	struct device_node *node = client->dev.of_node;
	int err = 0;
	const char *str_value;
	int value;
	const struct of_device_id *match;

	if (!node)
		return -EINVAL;

	match = of_match_device(max96712_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return -EFAULT;
	}

	err = of_property_read_string(node, "gmsl-mode", &str_value);
	if (err < 0) {
		dev_err(&client->dev, "gmsl-mode property not found\n");
		//return err;
	}
	if (!strcmp(str_value, "gmsl-1")) {
		priv->gmsl_mode = GMSL1;
	} else if (!strcmp(str_value, "gmsl-2")) {
		priv->gmsl_mode = GMSL2;
	} else if (!strcmp(str_value, "gmsl-2-3G")) {
		priv->gmsl_mode = GMSL2_3G;
	} else if (!strcmp(str_value, "gmsl-2-reg3G")) {
		priv->gmsl_mode = GMSL2_REG3G;
	} else {
		priv->gmsl_mode = GMSL2;
		dev_err(&client->dev, "invalid gmsl mode, set defaule mode: GMSL2\n");
	}

	err = of_property_read_string(node, "csi-mode", &str_value);
	if (err < 0) {
		dev_err(&client->dev, "csi-mode property not found\n");
		return err;
	}

	if (!strcmp(str_value, "2x4")) {
		priv->csi_mode = MAX96712_CSI_MODE_2X4;
		priv->lane_mp1 = MAX96712_LANE_MAP1_2X4;
		priv->lane_mp2 = MAX96712_LANE_MAP2_2X4;
	} else if (!strcmp(str_value, "4x2")) {
		priv->csi_mode = MAX96712_CSI_MODE_4X2;
		priv->lane_mp1 = MAX96712_LANE_MAP1_4X2;
		priv->lane_mp2 = MAX96712_LANE_MAP2_4X2;
	} else {
		dev_err(&client->dev, "invalid csi mode\n");
		return -EINVAL;
	}

	err = of_property_read_u32(node, "max-src", &value);
	if (err < 0) {
		dev_err(&client->dev, "No max-src info\n");
		return err;
	}
	priv->max_src = value;

	err = of_property_read_u32(node, "dser-num", &value);
	if (err < 0) {
		dev_err(&client->dev, "No dser-num info\n");
		return err;
	}
	priv->dser_num= value;

	err = of_property_read_u32(node, "mipi-clk", &value);
	if (err < 0) {
		dev_err(&client->dev, "No mipi-clk info\n");
		return err;
	}
	priv->mipi_clk= value;

	priv->reset_gpio = of_get_named_gpio(node, "reset-gpios", 0);
	if (priv->reset_gpio < 0) {
		dev_err(&client->dev, "reset-gpios not found %d\n", err);
		return err;
	}

	//p_rest[priv->dser_num] = priv->reset_gpio ;

	/* digital 1.2v */
	if (of_get_property(node, "vdd_cam_1v2-supply", NULL)) {
		priv->vdd_cam_1v2 = regulator_get(&client->dev, "vdd_cam_1v2");
		if (IS_ERR(priv->vdd_cam_1v2)) {
			dev_err(&client->dev,
				"vdd_cam_1v2 regulator get failed\n");
			err = PTR_ERR(priv->vdd_cam_1v2);
			priv->vdd_cam_1v2 = NULL;
			return err;
		}
	} else {
		priv->vdd_cam_1v2 = NULL;
	}

	return 0;
}

static struct regmap_config max96712_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	//.cache_type = REGCACHE_RBTREE,
	.cache_type = REGCACHE_NONE,
};

static int max96712_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max96712 *priv;
	int err = 0;

	if(n_poc_dev != 3){
		dev_warn(&client->dev, "POC have not been initialized!\n");
	}else{
		dev_dbg(&client->dev, "[MAX96712]: POC have been initialized.\n");
	}

	dev_dbg(&client->dev, "[MAX96712]: probing GMSL IO expander\n");

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&max96712_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	priv->ctrl_setup_done = false;
	priv->lane_setup = false;
	priv->link_setup = false;
	err = max96712_parse_dt(priv, client);
	if (err) {
		dev_err(&client->dev, "unable to parse dt\n");
		return -EFAULT;
	}

	max96712_pipes_reset(priv);

	if (priv->max_src > MAX96712_MAX_SOURCES) {
		dev_err(&client->dev,
			"max sources more than currently supported\n");
		return -EINVAL;
	}

	mutex_init(&priv->lock);

	dev_set_drvdata(&client->dev, priv);
	//max96712_write_reg(&client->dev,0x10,0x03);
	//max96712_write_reg(&client->dev,0x10,0x23);

	dev_dbg(&client->dev, "%s:  success\n", __func__);

	return err;
}


static int max96712_remove(struct i2c_client *client)
{
	struct max96712 *priv;

	if (client != NULL) {
		priv = dev_get_drvdata(&client->dev);
		mutex_destroy(&priv->lock);
		i2c_unregister_device(client);
		client = NULL;
	}

	return 0;
}

static const struct i2c_device_id max96712_id[] = {
	{ "max96712", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, max96712_id);

static struct i2c_driver max96712_i2c_driver = {
	.driver = {
		.name = "max96712",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max96712_of_match),
	},
	.probe = max96712_probe,
	.remove = max96712_remove,
	.id_table = max96712_id,
};

static int __init max96712_init(void)
{
	return i2c_add_driver(&max96712_i2c_driver);
}

static void __exit max96712_exit(void)
{
	i2c_del_driver(&max96712_i2c_driver);
}

module_init(max96712_init);
module_exit(max96712_exit);

MODULE_DESCRIPTION("IO Expander driver max96712");
MODULE_AUTHOR("Sudhir Vyas <svyas@nvidia.com");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.2.1.1015");
