/*
 * max9296.c - max9296 IO Expander driver
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
#include <media/max9296.h>

/* register specifics */
#define MAX9296_DST_CSI_MODE_ADDR 0x330
#define MAX9296_LANE_MAP1_ADDR 0x333
#define MAX9296_LANE_MAP2_ADDR 0x334

#define MAX9296_LANE_CTRL0_ADDR 0x40A
#define MAX9296_LANE_CTRL1_ADDR 0x44A
#define MAX9296_LANE_CTRL2_ADDR 0x48A
#define MAX9296_LANE_CTRL3_ADDR 0x4CA

#define MAX9296_TX11_PIPE_X_EN_ADDR 0x40B
#define MAX9296_TX45_PIPE_X_DST_CTRL_ADDR 0x42D

#define MAX9296_PIPE_X_SRC_0_MAP_ADDR 0x40D
#define MAX9296_PIPE_X_DST_0_MAP_ADDR 0x40E
#define MAX9296_PIPE_X_SRC_1_MAP_ADDR 0x40F
#define MAX9296_PIPE_X_DST_1_MAP_ADDR 0x410
#define MAX9296_PIPE_X_SRC_2_MAP_ADDR 0x411
#define MAX9296_PIPE_X_DST_2_MAP_ADDR 0x412

#define MAX9296_PIPE_X_ST_SEL_ADDR 0x50

#define MAX9296_PWDN_PHYS_ADDR 0x332
#define MAX9296_PHY1_CLK_ADDR 0x320
#define MAX9296_CTRL0_ADDR 0x10
#define MAX9296_RATE_MODE_ADDR 0x01
#define MAX9296_GMSL1_LINK_ADDR 0xF00
#define MAX9296_YUV_MIX_ADDR 0x322

/* data defines */
#define MAX9296_CSI_MODE_4X2 0x1
#define MAX9296_CSI_MODE_2X4 0x4
#define MAX9296_LANE_MAP1_4X2 0x44
#define MAX9296_LANE_MAP2_4X2 0x44
#define MAX9296_LANE_MAP1_2X4 0x4E
#define MAX9296_LANE_MAP2_2X4 0xE4

#define MAX9296_LANE_CTRL_MAP(num_lanes) \
	(((num_lanes) << 6) & 0xF0)

#define MAX9296_ALLPHYS_NOSTDBY 0xF0
#define MAX9296_ST_ID_SEL_INVALID 0xF

#define MAX9296_PHY1_CLK 0x2a

#define MAX9296_RESET_ALL 0x80

#define MAX9296_MAX_SOURCES 2
#define MAX9296_MAX_PIPES 4

#define MAX9296_PIPE_X 0
#define MAX9296_PIPE_Y 1
#define MAX9296_PIPE_Z 2
#define MAX9296_PIPE_U 3
#define MAX9296_PIPE_INVALID 0xF

#define MAX9296_CSI_CTRL_0 0
#define MAX9296_CSI_CTRL_1 1
#define MAX9296_CSI_CTRL_2 2
#define MAX9296_CSI_CTRL_3 3

#define MAX9296_INVAL_ST_ID 0xFF

/* Use reset value as per spec, confirm with vendor */
#define MAX9296_RESET_ST_ID 0x00

enum {
	GMSL2,
	GMSL2_3G,
	GMSL1,
	GMSL2_REG3G,
};

u32 p_rest[4];
struct max9296_sub_dir_proc_priv
{
	struct proc_dir_entry * dir;
	struct proc_dir_entry * per_chip_camera_num;
};
static int i2c_9296log_en = 1;

struct max9296_source_ctx {
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

struct max9296 {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	u32 num_src;
	u32 max_src;
	struct max9296_source_ctx sources[MAX9296_MAX_SOURCES];
	struct mutex lock;
	u32 sdev_ref;
	bool ctrl_setup_done;
	bool lane_setup;
	bool link_setup;
	bool frame_sync_en;
	struct pipe_ctx pipe[MAX9296_MAX_PIPES];
	u8 gmsl_mode;
	u8 csi_mode;
	u8 lane_mp1;
	u8 lane_mp2;
	int reset_gpio;
	int pw_ref;
	int camera_connect_num;
	u32 dser_num;
	u32 mipi_clk;
	bool binocular;
	struct max9296_sub_dir_proc_priv sub_dir_proc; 
	struct regulator *vdd_cam_1v2;
};

static int mipi_speed_Mhz;
module_param(mipi_speed_Mhz, int, 0644);

struct device *dser_dev_listp[8];
EXPORT_SYMBOL(dser_dev_listp);
int n_dser_dev=0;
EXPORT_SYMBOL(n_dser_dev);

/* max2008X.c */
extern int n_poc_dev;

//static int max9296_write_reg(struct device *dev,
 int max9296_write_reg(struct device *dev,
	u16 addr, u8 val)
{
	struct max9296 *priv;
	int err;

	priv = dev_get_drvdata(dev);

	if (i2c_9296log_en)
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
EXPORT_SYMBOL(max9296_write_reg);

 int max9296_read_reg(struct device *dev,
	u16 addr, u8 *val)
{
	struct max9296 *priv;
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

	if (i2c_9296log_en)
		dev_info(dev, "%s:i2c read , 0x%x = %x\n",__func__, addr, *val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(100, 110);

	return err;
}
EXPORT_SYMBOL(max9296_read_reg);


static int max9296_get_sdev_idx(struct device *dev,
			struct device *s_dev, int *idx)
{
	struct max9296 *priv = dev_get_drvdata(dev);
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

static void max9296_pipes_reset(struct max9296 *priv)
{
	/*
	 * This is default pipes combination. add more mappings
	 * for other combinations and requirements.
	 */
	struct pipe_ctx pipe_defaults[] = {
		{MAX9296_PIPE_X, GMSL_CSI_DT_RAW_12,MAX9296_CSI_CTRL_2, 0, MAX9296_INVAL_ST_ID},
		//{MAX9296_PIPE_Y, GMSL_CSI_DT_EMBED,MAX9296_CSI_CTRL_2, 0, MAX9296_INVAL_ST_ID},
		{MAX9296_PIPE_Y, GMSL_CSI_DT_YUV422_8,MAX9296_CSI_CTRL_2, 0, MAX9296_INVAL_ST_ID},
		{MAX9296_PIPE_Z, GMSL_CSI_DT_RAW_12,MAX9296_CSI_CTRL_2, 0, MAX9296_INVAL_ST_ID},
		//{MAX9296_PIPE_U, GMSL_CSI_DT_EMBED,MAX9296_CSI_CTRL_2, 0, MAX9296_INVAL_ST_ID}
		{MAX9296_PIPE_U, GMSL_CSI_DT_YUV422_8,MAX9296_CSI_CTRL_2, 0, MAX9296_INVAL_ST_ID}
	};

	/*
	 * Add DT props for num-streams and stream sequence, and based on that
	 * set the appropriate pipes defaults.
	 * For now default it supports "2 RAW12 and 2 EMBED" 1:1 mappings.
	 */
	memcpy(priv->pipe, pipe_defaults, sizeof(pipe_defaults));
}

static void max9296_reset_ctx(struct max9296 *priv)
{
	int i;

	priv->ctrl_setup_done = false;
	priv->link_setup = false;
	priv->lane_setup = false;
	max9296_pipes_reset(priv);
	for (i = 0; i < priv->num_src; i++)
		priv->sources[i].st_enabled = false;
}

int max9296_power_on(struct device *dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
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
EXPORT_SYMBOL(max9296_power_on);

void max9296_power_off(struct device *dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);

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
EXPORT_SYMBOL(max9296_power_off);

int max9296_setup_link(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	u32 link;
	int err = 0;
	int i;
	u8 value = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev = %d\n", __func__, i);
	mutex_lock(&priv->lock);

	max9296_read_reg(dev, 0x2, &value);
	max9296_write_reg(dev, 0x2, value & 0x0f);

	if (priv->gmsl_mode == GMSL2_3G) {
		/* Set 3G/6Gbps rate；bit[1:0] b01-3Gbps, b10-6Gbps; default: b10-6Gbps */
		max9296_write_reg(dev, MAX9296_RATE_MODE_ADDR, 0x01);  //Set 3Gbps rate
		msleep(100);
	}

	if (priv->gmsl_mode == GMSL2_REG3G) {
		/* Set 3G/6Gbps rate；bit[1:0] b01-3Gbps, b10-6Gbps; default: b10-6Gbps */
		max9296_write_reg(dev, MAX9296_RATE_MODE_ADDR, 0x02);  //Set 6Gbps rate
		msleep(100);
	}

	link = priv->sources[i].g_ctx->serdes_csi_link;

	//if (!priv->ctrl_setup_done) {
		if (link == GMSL_SERDES_CSI_LINK_A) {
			err = max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x01);
			max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x21);
		} else if (link == GMSL_SERDES_CSI_LINK_B) {
			err = max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x02);
			max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x22);
		} else { /* Extend for DES having more than two GMSL links */
			dev_err(dev, "%s: invalid gmsl link\n", __func__);
			err = -EINVAL;
			goto ret;
		}

	#if 1
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
		msleep(300);

		priv->link_setup = true;
//	}

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9296_setup_link);

int max9296_setup_link_gmsl1(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	u32 link;
	int err = 0;
	int i;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev = %d\n", __func__, i);
	mutex_lock(&priv->lock);

	max9296_write_reg(dev,0x313,0x00);  //disable MIPI output, CSI_OUT_EN-bit0
	max9296_write_reg(dev,0x006,0x1F);  //Reg??? config linkA & linkB as GMSL1 mode
	//max9296_write_reg(dev,0x006,0x9F); // config linkA as GMSL1 mode
	max9296_write_reg(dev,0x003,0x40);  //Disable Uart1 for Fsync

	max9296_write_reg(dev,0xB06,0xEF);
	max9296_write_reg(dev,0xC06,0xEF);
	max9296_write_reg(dev,0xB0D,0x80);
	max9296_write_reg(dev,0xC0D,0x80);

	link = priv->sources[i].g_ctx->serdes_csi_link;

	//if (!priv->ctrl_setup_done) {
		if (link == GMSL_SERDES_CSI_LINK_A) {
			//max9296_write_reg(dev,0xB06,0xEF);	//Def:6f  HIGHIMM, HV_SRC: HIGHIMM enable
			max9296_write_reg(dev,0xB07,0x84);	//Def:00. BWS, HIBW: 22/24bit bus; config linkA HVEN=1 DBL=1 BWS=0
			max9296_write_reg(dev,0xB08,0x01);	//Def:21. GPI_EN: bit5, Enable/Disable GPI->GPO
			//max9296_write_reg(dev,0xB0D,0x80);	//Def:00 I2C_LOC_ACK: bit7, 1-enable
			max9296_write_reg(dev,0xB0F,0x01);	//Def:09. DE_EN, PRBS_TYPE: DE_EN = 0;
			max9296_write_reg(dev,0xF00,0x01);  //Def.03 Enable link A/B: A
		} else if (link == GMSL_SERDES_CSI_LINK_B) {
			//max9296_write_reg(dev,0xC06,0xEF);	//Def:6f  HIGHIMM, HV_SRC: HIGHIMM enable
			max9296_write_reg(dev,0xC07,0x84);	//Def:00. BWS, HIBW: 22/24bit bus; config linkA HVEN=1 DBL=1 BWS=0
			max9296_write_reg(dev,0xC08,0x01);	//Def:21. GPI_EN: bit5, Enable/Disable GPI->GPO
			//max9296_write_reg(dev,0xC0D,0x80);	//Def:00 I2C_LOC_ACK: bit7, 1-enable
			max9296_write_reg(dev,0xC0F,0x01);	//Def:09. DE_EN, PRBS_TYPE: DE_EN = 0;
			max9296_write_reg(dev,0xF00,0x02);  //Def.03 Enable link A/B: B
		} else { /* Extend for DES having more than two GMSL links */
			dev_err(dev, "%s: invalid gmsl link\n", __func__);
			err = -EINVAL;
			goto ret;
		}

	#if 1
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
EXPORT_SYMBOL(max9296_setup_link_gmsl1);


int max9296_setup_control(struct device *dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;

	dev_dbg(dev, "%s: enter!\n", __func__);
	mutex_lock(&priv->lock);

	if (!priv->link_setup) {
		dev_err(dev, "%s: invalid state\n", __func__);
		err = -EINVAL;
		goto error;
	}

	if (priv->gmsl_mode == GMSL2_REG3G) {
		/* Set 3G/6Gbps rate；bit[1:0] b01-3Gbps, b10-6Gbps; default: b10-6Gbps */
		max9296_write_reg(dev, MAX9296_RATE_MODE_ADDR, 0x01);  //Set 3Gbps rate
		msleep(100);
	}
	//if (!priv->ctrl_setup_done) {
		/* Enable splitter mode */
		//max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x03);
		//max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x23);

		/* delay to settle link */
		//msleep(100);

		max9296_write_reg(dev,
			MAX9296_PWDN_PHYS_ADDR, MAX9296_ALLPHYS_NOSTDBY);

#if 1
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
EXPORT_SYMBOL(max9296_setup_control);

int max9296_reset_control(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;

	mutex_lock(&priv->lock);
	dev_dbg(dev, "%s: enter\n", __func__);

	if (!priv->ctrl_setup_done) {
		dev_dbg(dev, "%s: device is powered off\n", __func__);
		goto ret;
	}

	priv->sdev_ref--;
	if (priv->sdev_ref == 0) {
		max9296_reset_ctx(priv);
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, MAX9296_RESET_ALL);

		/* delay to settle reset */
		msleep(100);
	}

ret:
	mutex_unlock(&priv->lock);

	return err;
}
EXPORT_SYMBOL(max9296_reset_control);

int max9296_sdev_register(struct device *dev, struct gmsl_link_ctx *g_ctx)
{
	struct max9296 *priv = NULL;
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
			"%s: MAX9296 inputs size exhausted\n", __func__);
		err = -ENOMEM;
		goto error;
	}

	/* Check csi mode compatibility */
	if (!((priv->csi_mode == MAX9296_CSI_MODE_2X4) ?
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

	if(n_dser_dev < 8){
		dser_dev_listp[n_dser_dev]=dev;
		n_dser_dev++;
	}

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9296_sdev_register);

int max9296_sdev_unregister(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = NULL;
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
EXPORT_SYMBOL(max9296_sdev_unregister);

int max9296_set_tx_rate_3g(struct device *dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);

	if (!dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	if (priv->gmsl_mode == GMSL2_REG3G) {
		/* Set 3G/6Gbps rate；bit[1:0] b01-3Gbps, b10-6Gbps; default: b10-6Gbps */
		max9296_write_reg(dev, MAX9296_RATE_MODE_ADDR, 0x01);  //Set 3Gbps rate
		msleep(100);
	}

	return 0;
}
EXPORT_SYMBOL(max9296_set_tx_rate_3g);

int max9296_link_splitter(struct device *dev, u8 splitter_mode)
{
	if (!dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	if (splitter_mode)
	{
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, splitter_mode);
		max9296_write_reg(dev, MAX9296_CTRL0_ADDR, 0x20|splitter_mode);

		//extend delay time from 50ms to 300ms, for HY009J des-integrated board.
		//otherwise, ser may not available at probe time.
		msleep(300);
	}

	return 0;

}
EXPORT_SYMBOL(max9296_link_splitter);

int max9296_link_splitter_gmsl1(struct device *dev, u8 splitter_mode)
{

	if (!dev) {
		dev_err(dev, "%s: invalid input params\n", __func__);
		return -EINVAL;
	}

	if (splitter_mode)
	{
		max9296_write_reg(dev, MAX9296_GMSL1_LINK_ADDR, splitter_mode);
		msleep(50);
	}

	return 0;

}
EXPORT_SYMBOL(max9296_link_splitter_gmsl1);

static int max9296_get_available_pipe(struct device *dev,
				u32 st_data_type, u32 dst_csi_port)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < MAX9296_MAX_PIPES; i++) {
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
					MAX9296_CSI_CTRL_0) ||
				(priv->pipe[i].dst_csi_ctrl ==
					MAX9296_CSI_CTRL_1) :
				(priv->pipe[i].dst_csi_ctrl ==
					MAX9296_CSI_CTRL_2) ||
				(priv->pipe[i].dst_csi_ctrl ==
					MAX9296_CSI_CTRL_3)) &&
			(!priv->pipe[i].st_count))
			break;
	}

	if (i == MAX9296_MAX_PIPES) {
		dev_err(dev, "%s: all pipes are busy\n", __func__);
		return -ENOMEM;
	}

	return i;
}

struct reg_pair {
	u16 addr;
	u8 val;
};

static int max9296_setup_pipeline(struct device *dev,
		struct gmsl_link_ctx *g_ctx)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_stream *g_stream;
	struct reg_pair *map_list;
	u32 arr_sz = 0;
	int pipe_id = 0;
	u32 i = 0;
	u32 j = 0;
	u32 vc_idx = 0;
	u8 value = 0;

	dev_dbg(dev, "%s: enter!\n", __func__);

	for (i = 0; i < g_ctx->num_streams; i++) {
		/* Base data type mapping: pipeX/RAW12/CSICNTR1 */
		struct reg_pair map_pipe_raw12[] = {
			/* addr, val */
			{MAX9296_TX11_PIPE_X_EN_ADDR, 0x7},
			{MAX9296_TX45_PIPE_X_DST_CTRL_ADDR, 0x15},
			{MAX9296_PIPE_X_SRC_0_MAP_ADDR, 0x2C},
			{MAX9296_PIPE_X_DST_0_MAP_ADDR, 0x2C},
			{MAX9296_PIPE_X_SRC_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_DST_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_SRC_2_MAP_ADDR, 0x01},
			{MAX9296_PIPE_X_DST_2_MAP_ADDR, 0x01},
		};
		struct reg_pair map_pipe_yuv422_8[] = {
			/* addr, val */
			{MAX9296_TX11_PIPE_X_EN_ADDR, 0x7},
			{MAX9296_TX45_PIPE_X_DST_CTRL_ADDR, 0x15},
			{MAX9296_PIPE_X_SRC_0_MAP_ADDR, 0x1E},
			{MAX9296_PIPE_X_DST_0_MAP_ADDR, 0x1E},
			{MAX9296_PIPE_X_SRC_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_DST_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_SRC_2_MAP_ADDR, 0x01},
			{MAX9296_PIPE_X_DST_2_MAP_ADDR, 0x01},
		};

		/* Base data type mapping: pipeX/EMBED/CSICNTR1 */
		struct reg_pair map_pipe_embed[] = {
			/* addr, val */
			{MAX9296_TX11_PIPE_X_EN_ADDR, 0x7},
			{MAX9296_TX45_PIPE_X_DST_CTRL_ADDR, 0x15},
			{MAX9296_PIPE_X_SRC_0_MAP_ADDR, 0x12},
			{MAX9296_PIPE_X_DST_0_MAP_ADDR, 0x12},
			{MAX9296_PIPE_X_SRC_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_DST_1_MAP_ADDR, 0x00},
			{MAX9296_PIPE_X_SRC_2_MAP_ADDR, 0x01},
			{MAX9296_PIPE_X_DST_2_MAP_ADDR, 0x01},
		};

		g_stream = &g_ctx->streams[i];
		g_stream->des_pipe = MAX9296_PIPE_INVALID;

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

		pipe_id = max9296_get_available_pipe(dev,
				g_stream->st_data_type, g_ctx->dst_csi_port);

		dev_info(dev,"%s: pipe_id = %d\n", __func__, pipe_id);

		if (pipe_id < 0)
			return pipe_id;

		for (j = 0, vc_idx = 3; j < arr_sz; j++, vc_idx += 2) {
			/* update pipe configuration */
			map_list[j].addr += (0x40 * pipe_id);
			/* update vc id configuration */
			if (vc_idx < arr_sz)
				map_list[vc_idx].val |= (g_ctx->dst_vc << 6);

			max9296_write_reg(dev, map_list[j].addr,
						map_list[j].val);
		}

		/* Set stream id select input */
		if (g_stream->st_id_sel == GMSL_ST_ID_UNUSED) {
			dev_err(dev, "%s: Invalid stream st_id_sel\n",
				__func__);
			return -EINVAL;
		}

		g_stream->des_pipe = MAX9296_PIPE_X_ST_SEL_ADDR + pipe_id;

		max9296_read_reg(dev, 0x2, &value);
		max9296_write_reg(dev, 0x2, value | (1<<(pipe_id+4)));

		/* Update pipe internals */
		priv->pipe[pipe_id].st_count++;
		priv->pipe[pipe_id].st_id_sel = g_stream->st_id_sel;
		dev_info(dev,"%s: pipe[%d].st_count = %d\n", __func__, pipe_id, priv->pipe[pipe_id].st_count);
		dev_info(dev,"%s: pipe[%d].st_id_sel = %d\n", __func__, pipe_id, priv->pipe[pipe_id].st_id_sel);

	dev_dbg(dev, "%s: exit!\n", __func__);
	}

	return 0;
}

int max9296_start_streaming(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	struct gmsl_stream *g_stream;
	int err = 0;
	int i = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	g_ctx = priv->sources[i].g_ctx;

	if(priv->binocular!=1)
	{
		for (i = 0; i < g_ctx->num_streams; i++) {
			g_stream = &g_ctx->streams[i];

			if (g_stream->des_pipe != MAX9296_PIPE_INVALID){
	#if defined(MAX96717_COMPAT)
				if(g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_B){
					if(g_stream->des_pipe == (MAX9296_PIPE_X_ST_SEL_ADDR + MAX9296_PIPE_Y)){
						//note: cannot assign a constant pipe, can only use the first available one.
						//g_stream->des_pipe = g_stream->des_pipe + 2;
						//bug: do not increase st_id_sel itself.
						//g_stream->st_id_sel++;
						//received packets have crc and checking is enabled.
						max9296_write_reg(dev, g_stream->des_pipe,
									(g_stream->st_id_sel+1)|(1<<7));
					}else if(g_stream->des_pipe == (MAX9296_PIPE_X_ST_SEL_ADDR + MAX9296_PIPE_U)){
						max9296_write_reg(dev, g_stream->des_pipe,
									(g_stream->st_id_sel+1)|(1<<7));
					}else if (g_stream->des_pipe == (MAX9296_PIPE_X_ST_SEL_ADDR + MAX9296_PIPE_X)){
						max9296_write_reg(dev, g_stream->des_pipe,
									(g_stream->st_id_sel+1)|(1<<7));
					}else if (g_stream->des_pipe == (MAX9296_PIPE_X_ST_SEL_ADDR + MAX9296_PIPE_Z)){
						max9296_write_reg(dev, g_stream->des_pipe,
									(g_stream->st_id_sel+1)|(1<<7));
					}else{
						//pipe_defaults settings may changed.
					}
				}else{
					max9296_write_reg(dev, g_stream->des_pipe,
								g_stream->st_id_sel|(1<<7));
				}
	#else
				max9296_write_reg(dev, g_stream->des_pipe,
							g_stream->st_id_sel);
	#endif
			}
		}
	}else{
		for (i = 0; i < g_ctx->num_streams; i++) {
			g_stream = &g_ctx->streams[i];

			if(g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_A)
			{
				max9296_write_reg(dev, g_stream->des_pipe,0x80);
			}else
			{
				max9296_write_reg(dev, g_stream->des_pipe,0x82);
			}
		}
	}
	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max9296_start_streaming);

int max9296_start_streaming_gmsl1(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;
	int i = 0;
	u8 value = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);

	max9296_read_reg(dev, 0x322, &value);
	max9296_write_reg(dev, 0x322, value | ((i+1)<<4));

	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max9296_start_streaming_gmsl1);

int max9296_stop_streaming(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	struct gmsl_stream *g_stream;
	int err = 0;
	int i = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;
	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	
	g_ctx = priv->sources[i].g_ctx;

	for (i = 0; i < g_ctx->num_streams; i++) {
		g_stream = &g_ctx->streams[i];

		if (g_stream->des_pipe != MAX9296_PIPE_INVALID)
			max9296_write_reg(dev, g_stream->des_pipe,
						MAX9296_RESET_ST_ID);
	}
	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max9296_stop_streaming);

int max9296_stop_streaming_gmsl1(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	int err = 0;
	int i = 0;
	u8 value = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;
	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	max9296_read_reg(dev, 0x322, &value);
	max9296_write_reg(dev, 0x322, value & (~((i+1)<<4)));
	mutex_unlock(&priv->lock);

	return 0;
}
EXPORT_SYMBOL(max9296_stop_streaming_gmsl1);

int max9296_setup_streaming(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	int err = 0;
	int i = 0;
	u16 lane_ctrl_addr;
	u32 dphy_mipi_clk = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	if (priv->sources[i].st_enabled)
		goto ret;

	g_ctx = priv->sources[i].g_ctx;

	err = max9296_setup_pipeline(dev, g_ctx);
	if (err)
		goto ret;

	dev_dbg(dev, "%s: dst_csi_port = %d\n", __func__, g_ctx->dst_csi_port);
	/* Derive CSI lane map register */
	switch(g_ctx->dst_csi_port) {
	case GMSL_CSI_PORT_A:
	case GMSL_CSI_PORT_D:
		lane_ctrl_addr = MAX9296_LANE_CTRL1_ADDR;
		break;
	case GMSL_CSI_PORT_B:
	case GMSL_CSI_PORT_E:
		lane_ctrl_addr = MAX9296_LANE_CTRL2_ADDR;
		break;
	case GMSL_CSI_PORT_C:
		lane_ctrl_addr = MAX9296_LANE_CTRL0_ADDR;
		break;
	case GMSL_CSI_PORT_F:
		lane_ctrl_addr = MAX9296_LANE_CTRL3_ADDR;
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
	//max9296_write_reg(dev, lane_ctrl_addr, MAX9296_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1));
	max9296_write_reg(dev, 0x44a, MAX9296_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1));
	max9296_write_reg(dev, 0x48a, MAX9296_LANE_CTRL_MAP(g_ctx->num_csi_lanes-1));

	if (!priv->lane_setup) {
		max9296_write_reg(dev,
			MAX9296_DST_CSI_MODE_ADDR, priv->csi_mode);
		max9296_write_reg(dev,
			MAX9296_LANE_MAP1_ADDR, priv->lane_mp1);
		max9296_write_reg(dev,
			MAX9296_LANE_MAP2_ADDR, priv->lane_mp2);

		//max9296_write_reg(dev,
		//	MAX9296_PHY1_CLK_ADDR, MAX9296_PHY1_CLK);

	//port copy setting
		max9296_write_reg(dev,0x336,0x40);
		max9296_write_reg(dev,0x339,0x80);
		max9296_write_reg(dev,0x33A,0x40);

	// mipi clock seting
		//dphy_mipi_clk = priv->mipi_clk / 100;
		if (mipi_speed_Mhz > 0) {
				dphy_mipi_clk = mipi_speed_Mhz / 100;
		}else{
				dphy_mipi_clk = priv->mipi_clk / 100;
		}
		max9296_write_reg(dev,0x31D,dphy_mipi_clk|0x20);
		max9296_write_reg(dev,0x320,dphy_mipi_clk|0x20);
		max9296_write_reg(dev,0x323,dphy_mipi_clk|0x20);
		max9296_write_reg(dev,0x326,dphy_mipi_clk|0x20);

		if (g_ctx->frame_sync_en) {
			if(g_ctx->sensor_model == SENSOR_IMX390_RAW_BL){
			//if(0){
				max9296_write_reg(dev,0x2C2,0x13); //GPIO_OUT=1, GPIO_TX_EN=1, GPIO_OUT_DIS=1, why?
				max9296_write_reg(dev,0x2C3,0x26); //GPIO_OUT_TYPE=1, GPIO_TX_ID=6
				max9296_write_reg(dev,0x2C4,0x86); //OVR_RES_CFG=1, GPIO_RX_ID=6
				max9296_write_reg(dev,0x03,0x40);
			}else{
				max9296_write_reg(dev,0x03,0x40);		//disabled UART TX  let MFP6 use GPIO function
#if !defined(MFP7_MFP8_COMPAT)
				/* setup  MFP6 as  GPIO transmission , GPIO ID = 6 */
				max9296_write_reg(dev,0x2C2,0x13); //GPIO_OUT=1, GPIO_TX_EN=1, GPIO_OUT_DIS=1, why?
				max9296_write_reg(dev,0x2C3,0x26); //GPIO_OUT_TYPE=1, GPIO_TX_ID=6
				max9296_write_reg(dev,0x2C4,0x86); //OVR_RES_CFG=1, GPIO_RX_ID=6
#else
				/* setup  MFP6 as  GPIO transmission , GPIO ID = 7 */
				max9296_write_reg(dev,0x2C2,0x83);
				max9296_write_reg(dev,0x2C3,0xA7);
#endif
				max9296_write_reg(dev,0x03,0x40);		//disabled UART TX  let MFP6 use GPIO function
			}

			/* max9296-GPIO0 --> max9295-GPIO0(by default)*/
			if (g_ctx->sensor_model == SENSOR_IMX390_RAW_SG) {
					max9296_write_reg(dev,0x2B0,0x12); //GPIO_OUT=1, GPIO_TX_EN=1, GPIO_OUT_DIS=0
					max9296_write_reg(dev,0x2B1,0x60); //PULL UP, GPIO_OUT_TYPE=1, GPIO_TX_ID=0
			}

			/* max9296-GPIO7 --> max9295-GPIO7(by default)*/
			if (g_ctx->sensor_model == SENSOR_E003A_YUV_3G || g_ctx->sensor_model == SENSOR_E003A_YUV_ET || g_ctx->sensor_model == SENSOR_F008AX_YUV_ET) {
					max9296_write_reg(dev,0x2C5,0x12); //GPIO_OUT=1, GPIO_TX_EN=1, GPIO_OUT_DIS=0
					max9296_write_reg(dev,0x2C6,0x67); //PULL UP, GPIO_OUT_TYPE=1, GPIO_TX_ID=0
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
EXPORT_SYMBOL(max9296_setup_streaming);

int max9296_setup_streaming_gmsl1(struct device *dev, struct device *s_dev)
{
	struct max9296 *priv = dev_get_drvdata(dev);
	struct gmsl_link_ctx *g_ctx;
	int err = 0;
	int i = 0;
	int offset = 0;
	//u16 lane_ctrl_addr;
	u32 dphy_mipi_clk = 0;

	err = max9296_get_sdev_idx(dev, s_dev, &i);
	if (err)
		return err;

	dev_dbg(dev, "%s: enter! sdev_index = %d\n", __func__, i);
	mutex_lock(&priv->lock);
	if (priv->sources[i].st_enabled)
		goto ret;

	g_ctx = priv->sources[i].g_ctx;

	max9296_write_reg(dev,0x313,0x42);	//pipeline-X BPP Datatypes = x2A, x10-12, x31-37, CSI_OUT_EN-bit1
	max9296_write_reg(dev,0x319,0x08);	//pipeline-Y BPP Datatypes = x2A, x10-12, x31-37
	max9296_write_reg(dev,0x314,0x10);	//Def:00. soft_vc_x,soft_vc_y
	max9296_write_reg(dev,0x315,0x00);	//soft_vc_z,soft_vc_u
	max9296_write_reg(dev,0x316,0x5e);	// X: YUV422-8
	max9296_write_reg(dev,0x317,0x0e);	// Y: YUV422-8
	max9296_write_reg(dev,0x31D,0xef);	// Def:0F. override_bpp_vc_dt x&y, PHY0 CLK: 1.5GHz

	offset = i * 0x40;

	//PIPE X route: X -- CSI --DPHY
	max9296_write_reg(dev,0x40b + offset,0x07);	// MAP_EN_L: map0 ~ map7
	max9296_write_reg(dev,0x40d + offset,0x1e|(0x00<<6));	// VC&DT src map0
	max9296_write_reg(dev,0x40e + offset,0x1e|(0x00<<6));	// VC&DT dst map0
	max9296_write_reg(dev,0x40f + offset,0x00);	// VC&DT src map1
	max9296_write_reg(dev,0x410 + offset,0x00);	// VC&DT dst map1
	max9296_write_reg(dev,0x411 + offset,0x01);	// VC&DT src map2
	max9296_write_reg(dev,0x412 + offset,0x01);	// VC&DT dst map2
	//max9296_write_reg(dev,0x413 + offset,0x02);	// VC&DT src map3
	//max9296_write_reg(dev,0x414 + offset,0x02);	// VC&DT dst map3
	//max9296_write_reg(dev,0x415 + offset,0x03);	// VC&DT src map4
	//max9296_write_reg(dev,0x416 + offset,0x03);	// VC&DT dst map4
	max9296_write_reg(dev,0x42d + offset,0x15);	// MAP_DPHY_DEST 0 ~ 3: VC&DT src map0 ~ map3 -> DPHY-1
	//max9296_write_reg(priv->dser_dev,0x42e + offset,0x01);	// MAP_DPHY_DEST 4 ~ 7: VC&DT src 4 -> DPHY-1

	/* port copy setting: portA --> portB */
	max9296_write_reg(dev,0x336,0x40);  // Enable phy_cp0
	max9296_write_reg(dev,0x339,0x80);  // phy_cp0_dst: phy2
	max9296_write_reg(dev,0x33A,0x40);  // phy_cp0_src: phy1

	// mipi clock seting
	dphy_mipi_clk = priv->mipi_clk / 100;
	//max9296_write_reg(dev,0x31D,dphy_mipi_clk|0x20);  //phy-0
	max9296_write_reg(dev,0x320,dphy_mipi_clk|0x20);  //phy-1 MIPI Port A speed : 600Mbps
	max9296_write_reg(dev,0x323,dphy_mipi_clk|0x20);  //phy-2
	//max9296_write_reg(dev,0x326,dphy_mipi_clk|0x20);  //phy-3

	priv->sources[i].st_enabled = true;
	dev_info(dev, "%s: exit!\n", __func__);

ret:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9296_setup_streaming_gmsl1);

const struct of_device_id max9296_of_match[] = {
	{ .compatible = "nvidia,max9296", },
	{ },
};
MODULE_DEVICE_TABLE(of, max9296_of_match);

static int max9296_parse_dt(struct max9296 *priv,
				struct i2c_client *client)
{
	struct device_node *node = client->dev.of_node;
	int err = 0;
	const char *str_value;
	int value;
	const struct of_device_id *match;

	if (!node)
		return -EINVAL;

	match = of_match_device(max9296_of_match, &client->dev);
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
		priv->csi_mode = MAX9296_CSI_MODE_2X4;
		priv->lane_mp1 = MAX9296_LANE_MAP1_2X4;
		priv->lane_mp2 = MAX9296_LANE_MAP2_2X4;
	} else if (!strcmp(str_value, "4x2")) {
		priv->csi_mode = MAX9296_CSI_MODE_4X2;
		priv->lane_mp1 = MAX9296_LANE_MAP1_4X2;
		priv->lane_mp2 = MAX9296_LANE_MAP2_4X2;
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

	priv->binocular= of_property_read_bool(node, "binocular");

	priv->reset_gpio = of_get_named_gpio(node, "reset-gpios", 0);
	if (priv->reset_gpio < 0) {
		dev_err(&client->dev, "reset-gpios not found %d\n", err);
		return err;
	}

	p_rest[priv->dser_num] = priv->reset_gpio ;

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

static struct regmap_config max9296_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	//.cache_type = REGCACHE_RBTREE,
	.cache_type = REGCACHE_NONE,
};

static ssize_t max9296_camera_num_proc_write(struct file *file, const char __user *buf, size_t length, loff_t *ppos)
{
	struct max9296 *priv = PDE_DATA(file_inode(file));
	char str[8] = {0};
	int value = 0;
	
        if(NULL == priv)
        {
                printk("%s:%d .\n", __func__, __LINE__);
                return -EINVAL;
        }

        if (length < 1)
        {
                printk("%s:%d .\n", __func__, __LINE__);
                return -EFAULT;
        }

        if (length > sizeof(str))
        {
                printk("%s:%d .\n", __func__, __LINE__);
                return -EFAULT;
        }

		if (buf && !copy_from_user(str, buf, length))
        {

                if(sscanf(str, "%d", &value) != 1 )
                {
                        printk("%s:%d .\n", __func__, __LINE__);
                        return -EINVAL;
                }

				if(value!=1 && value !=2){
                        printk("%s:%d .\n", __func__, __LINE__);
                        return -EINVAL;
				}

				priv->camera_connect_num=value;
		}

		printk(KERN_INFO "max9296:data=%d",value);
		return length;
}

static ssize_t max9296_camera_num_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	    struct max9296 *priv = PDE_DATA(file_inode(file));
        char str[16] = {0};

        if(NULL == priv)
        {
                printk("%s:%d .\n", __func__, __LINE__);
                return -EINVAL;
        }

		sprintf(str,"%d\n",priv->camera_connect_num);
		    return simple_read_from_buffer(buf, size, ppos, str, (strlen(str) + 1));

}
static const struct file_operations max9296_camera_num_proc_operations = {
        .write         = max9296_camera_num_proc_write,
        .read           = max9296_camera_num_proc_read,
};

static int max9296_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max9296 *priv;
	int err = 0;

	if(n_poc_dev != 2){
		dev_warn(&client->dev, "POC have not been initialized!\n");
	}else{
		dev_dbg(&client->dev, "[MAX9296]: POC have been initialized.\n");
	}

	dev_dbg(&client->dev, "[MAX9296]: probing GMSL IO expander\n");

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&max9296_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(priv->regmap));
		return -ENODEV;
	}

	priv->ctrl_setup_done = false;
	priv->lane_setup = false;
	priv->link_setup = false;
	priv->camera_connect_num = 2;
	err = max9296_parse_dt(priv, client);
	if (err) {
		dev_err(&client->dev, "unable to parse dt\n");
		return -EFAULT;
	}

	max9296_pipes_reset(priv);

	if (priv->max_src > MAX9296_MAX_SOURCES) {
		dev_err(&client->dev,
			"max sources more than currently supported\n");
		return -EINVAL;
	}

	mutex_init(&priv->lock);

	dev_set_drvdata(&client->dev, priv);
	//max9296_write_reg(&client->dev,0x10,0x03);
	//max9296_write_reg(&client->dev,0x10,0x23);
do{
	int index= -1;
	if(strcmp(client->adapter->name,"3180000.i2c")==0){
		if(client->addr==0x48){
			index=0;
		}else if(client->addr==0x4a){
			index=1;
		}else{
			break;
		}

	}else if(strcmp(client->adapter->name,"c250000.i2c")==0){
		if(client->addr==0x48){
			index=2;
		}else if(client->addr==0x4a){

			index=3;
		}else{
			break;
		}
	}
}while(0);

	 
	dev_dbg(&client->dev, "%s:  success\n", __func__);

	return err;
}


static int max9296_remove(struct i2c_client *client)
{
	struct max9296 *priv;

	if (client != NULL) {
		priv = dev_get_drvdata(&client->dev);
		mutex_destroy(&priv->lock);
		i2c_unregister_device(client);
		client = NULL;
	}

	return 0;
}

static const struct i2c_device_id max9296_id[] = {
	{ "max9296", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, max9296_id);

static struct i2c_driver max9296_i2c_driver = {
	.driver = {
		.name = "max9296",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max9296_of_match),
	},
	.probe = max9296_probe,
	.remove = max9296_remove,
	.id_table = max9296_id,
};

static int __init max9296_init(void)
{
	return i2c_add_driver(&max9296_i2c_driver);
}

static void __exit max9296_exit(void)
{
	i2c_del_driver(&max9296_i2c_driver);
}

module_init(max9296_init);
module_exit(max9296_exit);

MODULE_DESCRIPTION("IO Expander driver max9296");
MODULE_AUTHOR("Sudhir Vyas <svyas@nvidia.com");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.2.1.1015");
