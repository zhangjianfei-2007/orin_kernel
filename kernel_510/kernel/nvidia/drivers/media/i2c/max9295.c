/*
 * max9295.c - max9295 IO Expander driver
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
#include <media/max9295.h>

/* register specifics */
#define MAX9295_MIPI_RX0_ADDR 0x330
#define MAX9295_MIPI_RX1_ADDR 0x331
#define MAX9295_MIPI_RX2_ADDR 0x332
#define MAX9295_MIPI_RX3_ADDR 0x333

#define MAX9295_PIPE_X_DT_ADDR 0x314
#define MAX9295_PIPE_Y_DT_ADDR 0x316
#define MAX9295_PIPE_Z_DT_ADDR 0x318
#define MAX9295_PIPE_U_DT_ADDR 0x31A

#define MAX9295_CTRL0_ADDR 0x10
#define MAX9295_SRC_CTRL_ADDR 0x2BF
#define MAX9295_SRC_PWDN_ADDR 0x02BE
#define MAX9295_SRC_OUT_RCLK_ADDR 0x3F1
#define MAX9295_START_PIPE_ADDR 0x311
#define MAX9295_PIPE_EN_ADDR 0x2
#define MAX9295_CSI_PORT_SEL_ADDR 0x308

#define MAX9295_RCLK_UART_ADDR 0x3
#define MAX9295_RCLK_25M_EN_UART_DIS 0x07

#define MAX9295_RCLK_I2C_ADDR 0x06
#define MAX9295_RCLK_EN_I2C_SELECTED 0xBF

#define MAX9295_GENERATION_PLL_ADDR 0x3F0
#define MAX9295_GENERATION_PLL_ENABLE 0x51

#define MAX9295_I2C4_ADDR 0x44
#define MAX9295_I2C5_ADDR 0x45

#define MAX9295_DEV_ADDR 0x00

#define MAX9295_STREAM_PIPE_UNUSED 0x22
#define MAX9295_CSI_MODE_1X4 0x00
#define MAX9295_CSI_MODE_2X2 0x03
#define MAX9295_CSI_MODE_2X4 0x06

#define MAX9295_CSI_PORT_B(num_lanes) (((num_lanes) << 4) & 0xF0)
#define MAX9295_CSI_PORT_A(num_lanes) ((num_lanes) & 0x0F)

#define MAX9295_CSI_1X4_MODE_LANE_MAP1 0xE0
#define MAX9295_CSI_1X4_MODE_LANE_MAP2 0x04

#define MAX9295_CSI_2X4_MODE_LANE_MAP1 0xEE
#define MAX9295_CSI_2X4_MODE_LANE_MAP2 0xE4

#define MAX9295_CSI_2X2_MODE_LANE_MAP1 MAX9295_CSI_2X4_MODE_LANE_MAP1
#define MAX9295_CSI_2X2_MODE_LANE_MAP2 MAX9295_CSI_2X4_MODE_LANE_MAP2

#define MAX9295_ST_ID_0 0x0
#define MAX9295_ST_ID_1 0x1
#define MAX9295_ST_ID_2 0x2
#define MAX9295_ST_ID_3 0x3

#define MAX9295_PIPE_X_START_B 0x80
#define MAX9295_PIPE_Y_START_B 0x40
#define MAX9295_PIPE_Z_START_B 0x20
#define MAX9295_PIPE_U_START_B 0x10

#define MAX9295_PIPE_X_START_A 0x1
#define MAX9295_PIPE_Y_START_A 0x2
#define MAX9295_PIPE_Z_START_A 0x4
#define MAX9295_PIPE_U_START_A 0x8

#define MAX9295_START_PORT_A 0x10
#define MAX9295_START_PORT_B 0x20

#define MAX9295_CSI_LN2 0x1
#define MAX9295_CSI_LN4 0x3

#define MAX9295_EN_LINE_INFO 0x40

#define MAX9295_VID_TX_EN_X 0x10
#define MAX9295_VID_TX_EN_Y 0x20
#define MAX9295_VID_TX_EN_Z 0x40
#define MAX9295_VID_TX_EN_U 0x80

#define MAX9295_VID_INIT 0x3
#define MAX9295_SRC_RCLK 0x85

#define MAX9295_RESET_ALL 0x80
#define MAX9295_RESET_SRC 0x20
#define MAX9295_PWDN_GPIO 0x90

#define MAX9295_MAX_PIPES 0x4

struct max9295_client_ctx {
	struct gmsl_link_ctx *g_ctx;
	bool st_done;
};

struct max9295 {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	struct max9295_client_ctx g_client;
	struct mutex lock;
	/* primary serializer properties */
	__u32 def_addr;
	__u32 pst2_ref;
	__u32 group;
	__u32 dser_num;
	bool frame_sync_en;
	__u32 ser_num;
	bool binocular;
};

static struct max9295 *prim_priv__;
static struct max9295 *prim_priv1__;

struct map_ctx {
	u8 dt;
	u16 addr;
	u8 val;
	u8 st_id;
};

struct device *ser_9295_listp[12]={NULL};
EXPORT_SYMBOL(ser_9295_listp);
int n_9295_dev=0;
EXPORT_SYMBOL(n_9295_dev);

static int i2c_9295log_en = 1;

/* max2008X.c */
extern struct device *poc_dev_listp[3];
extern int n_poc_dev;
int max2008X_read_current(struct device *dev, u8 index, u16 *current_ma);

//static int max9295_write_reg(struct device *dev, u16 addr, u8 val)
int max9295_write_reg(struct device *dev, u16 addr, u8 val)
{
	struct max9295 *priv = dev_get_drvdata(dev);
	int err;

	if (i2c_9295log_en)
		dev_info(dev, "%s:i2c write , 0x%x = %x\n",__func__, addr, val);

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_err(dev, "%s:i2c write failed, 0x%x = %x\n",
			__func__, addr, val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(10000, 11000);

	return err;
}
EXPORT_SYMBOL(max9295_write_reg);

int max9295_read_reg(struct device *dev, u16 addr, u8 *val)
{
	struct max9295 *priv;
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

	if (i2c_9295log_en)
		dev_info(dev, "%s:i2c read , 0x%x = %x\n",__func__, addr, *val);

	/* delay before next i2c command as required for SERDES link */
	usleep_range(100, 110);

	return err;
}
EXPORT_SYMBOL(max9295_read_reg);

/* set chip address - NOTE: 7bit	*/
int max9295_set_i2c_client_addr(struct device *dev, unsigned short addr)
{
	struct max9295 *priv;

	priv = dev_get_drvdata(dev);

	priv->i2c_client->addr=addr;

	return 0;
}
EXPORT_SYMBOL(max9295_set_i2c_client_addr);

int max9295_setup_streaming(struct device *dev)
{
	struct max9295 *priv = dev_get_drvdata(dev);
	int err = 0;
	u32 csi_mode;
	u32 lane_map1;
	u32 lane_map2;
	u32 port;
	u32 rx1_lanes;
	u32 st_pipe;
	u32 pipe_en;
	u32 port_sel = 0;
	struct gmsl_link_ctx *g_ctx;
	u32 i;
	u32 j;
	u32 st_en;
#if defined(MAX96717_COMPAT)
	u8 val;
#endif

	struct map_ctx map_pipe_dtype[] = {
		{GMSL_CSI_DT_UED_U1, MAX9295_PIPE_X_DT_ADDR, 0x30,
			MAX9295_ST_ID_0},
		{GMSL_CSI_DT_EMBED, MAX9295_PIPE_Y_DT_ADDR, 0x12,
			MAX9295_ST_ID_1},
		{GMSL_CSI_DT_RAW_12, MAX9295_PIPE_Z_DT_ADDR, 0x2C,
			MAX9295_ST_ID_2},
		{GMSL_CSI_DT_YUV422_8, MAX9295_PIPE_Z_DT_ADDR, 0x1E,
			MAX9295_ST_ID_2},		
	};

	mutex_lock(&priv->lock);
	dev_dbg(dev, "%s: enter\n", __func__);

	if (!priv->g_client.g_ctx) {
		dev_err(dev, "%s: no sdev client found\n", __func__);
		err = -EINVAL;
		goto error;
	}

	if (priv->g_client.st_done) {
		dev_dbg(dev, "%s: stream setup is already done\n", __func__);
		goto error;
	}

	g_ctx = priv->g_client.g_ctx;
    dev_dbg(dev, "%s: csi_mode = %d\n", __func__, g_ctx->csi_mode);

	switch (g_ctx->csi_mode) {
	case GMSL_CSI_1X4_MODE:
		csi_mode = MAX9295_CSI_MODE_1X4;
		lane_map1 = MAX9295_CSI_1X4_MODE_LANE_MAP1;
		lane_map2 = MAX9295_CSI_1X4_MODE_LANE_MAP2;
		rx1_lanes = MAX9295_CSI_LN4;
		break;
	case GMSL_CSI_2X2_MODE:
		csi_mode = MAX9295_CSI_MODE_2X2;
		lane_map1 = MAX9295_CSI_2X2_MODE_LANE_MAP1;
		lane_map2 = MAX9295_CSI_2X2_MODE_LANE_MAP2;
		rx1_lanes = MAX9295_CSI_LN2;
		break;
	case GMSL_CSI_2X4_MODE:
		csi_mode = MAX9295_CSI_MODE_2X4;
		lane_map1 = MAX9295_CSI_2X4_MODE_LANE_MAP1;
		lane_map2 = MAX9295_CSI_2X4_MODE_LANE_MAP2;
		rx1_lanes = MAX9295_CSI_LN4;
		break;
	default:
		dev_err(dev, "%s: invalid csi mode\n", __func__);
		err = -EINVAL;
		goto error;
	}
    dev_dbg(dev, "%s: src_csi_port = %d\n", __func__, g_ctx->src_csi_port);
    dev_dbg(dev, "%s: num_streams = %d\n", __func__, g_ctx->num_streams);
    dev_dbg(dev, "%s: serdes_csi_link = %d\n", __func__, g_ctx->serdes_csi_link);
	port = (g_ctx->src_csi_port == GMSL_CSI_PORT_B) ?
			MAX9295_CSI_PORT_B(rx1_lanes) :
			MAX9295_CSI_PORT_A(rx1_lanes);

	max9295_write_reg(dev, MAX9295_MIPI_RX0_ADDR, csi_mode);
	max9295_write_reg(dev, MAX9295_MIPI_RX1_ADDR, port);
	max9295_write_reg(dev, MAX9295_MIPI_RX2_ADDR, lane_map1);
	max9295_write_reg(dev, MAX9295_MIPI_RX3_ADDR, lane_map2);

	for (i = 0; i < g_ctx->num_streams; i++) {
		struct gmsl_stream *g_stream = &g_ctx->streams[i];
        dev_dbg(dev, "%s: g_ctx->streams[%d] = 0x%02x\n", __func__, i, g_stream->st_data_type);
		g_stream->st_id_sel = GMSL_ST_ID_UNUSED;
		for (j = 0; j < ARRAY_SIZE(map_pipe_dtype); j++) {
			if (map_pipe_dtype[j].dt == g_stream->st_data_type) {
				/*
				 * TODO:
				 * 1) Remove link specific overrides, depends
				 * on #2.
				 * 2) Add support for vc id based stream sel
				 * overrides TX_SRC_SEL. would be useful in
				 * using same mappings in all ser devs.
				 */
				//fix it
				if (g_ctx->serdes_csi_link ==
					GMSL_SERDES_CSI_LINK_B) {
#if defined(MAX96717_COMPAT)
					//set stream id (=3) for packets from this channel(pipe z if use current settings for YUV422)
					max9295_write_reg(dev, 0x5b, MAX9295_ST_ID_3);
#else
					//use another pipe.
					map_pipe_dtype[j].addr += 2;
					map_pipe_dtype[j].st_id += 1;
#endif
				}else if (g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_A){
#if defined(MAX96717_COMPAT)
					//set stream id (=2) for packets from this channel
					max9295_write_reg(dev, 0x5b, MAX9295_ST_ID_2);
#endif
				}else if (g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_D){
#if defined(MAX96717_COMPAT)
					max9295_write_reg(dev, 0x5b, MAX9295_ST_ID_3);
#else
					//use another pipe.
					map_pipe_dtype[j].addr += 2;
					map_pipe_dtype[j].st_id += 1;
#endif
				}else if (g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_C){
#if defined(MAX96717_COMPAT)
					//max9295_write_reg(dev, 0x5b, (0x10 | MAX9295_ST_ID_3));
					max9295_write_reg(dev, 0x5b, MAX9295_ST_ID_3);
#endif
				}
#if defined(MAX96717_COMPAT)
				//enable transmit crc
				max9295_read_reg(dev, 0x58, &val);
				val |= (1<<7);
				max9295_write_reg(dev, 0x58, val);
#endif
				g_stream->st_id_sel = map_pipe_dtype[j].st_id;
				st_en = (map_pipe_dtype[j].addr ==
						MAX9295_PIPE_X_DT_ADDR) ?
							0xC0 : 0x40;

				max9295_write_reg(dev, map_pipe_dtype[j].addr,
					(st_en | map_pipe_dtype[j].val));
			}
		}
	}

	for (i = 0; i < g_ctx->num_streams; i++)
		if (g_ctx->streams[i].st_id_sel != GMSL_ST_ID_UNUSED)
			port_sel |= (1 << g_ctx->streams[i].st_id_sel);

	if (g_ctx->src_csi_port == GMSL_CSI_PORT_B) {
		st_pipe = (MAX9295_PIPE_X_START_B | MAX9295_PIPE_Y_START_B |
			MAX9295_PIPE_Z_START_B | MAX9295_PIPE_U_START_B);
		port_sel |= (MAX9295_EN_LINE_INFO | MAX9295_START_PORT_B);
	} else {
		st_pipe = MAX9295_PIPE_X_START_A | MAX9295_PIPE_Y_START_A |
			MAX9295_PIPE_Z_START_A | MAX9295_PIPE_U_START_A;
		port_sel |= (MAX9295_EN_LINE_INFO | MAX9295_START_PORT_A);
	}

	pipe_en = (MAX9295_VID_TX_EN_X | MAX9295_VID_TX_EN_Y |
		MAX9295_VID_TX_EN_Z | MAX9295_VID_TX_EN_U | MAX9295_VID_INIT);

	if(priv->binocular==1)
	{
		max9295_write_reg(dev, 0x309, 0x01);
		max9295_write_reg(dev, 0x30a, 0x0);
		max9295_write_reg(dev, 0x30b, 0x0);
		max9295_write_reg(dev, 0x30c, 0x00);
		max9295_write_reg(dev, 0x30d, 0x02);
		max9295_write_reg(dev, 0x30e, 0x0);
		max9295_write_reg(dev, 0x30f, 0x0);
		max9295_write_reg(dev, 0x310, 0x0);
		max9295_write_reg(dev, 0x308, 0x7f);
		max9295_write_reg(dev, 0x53, 0x10);
		max9295_write_reg(dev, 0x5b, 0x12);
		max9295_write_reg(dev, 0x31c, 0x58);
		max9295_write_reg(dev, 0x31e, 0x58);
	}else{
		max9295_write_reg(dev, MAX9295_START_PIPE_ADDR, st_pipe);
		max9295_write_reg(dev, MAX9295_CSI_PORT_SEL_ADDR, port_sel);
		max9295_write_reg(dev, MAX9295_PIPE_EN_ADDR, pipe_en);
	}

	priv->g_client.st_done = true;

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9295_setup_streaming);

int max9295_setup_control(struct device *dev)
{
	struct max9295 *priv = dev_get_drvdata(dev);
	int err = 0;
	struct gmsl_link_ctx *g_ctx;
	u32 offset1 = 0;
	u32 offset2 = 0;
	u32 i;

	u8 i2c_ovrd[] = {
		0x6B, 0x16,
		0x73, 0x17,
		0x7B, 0x36,
		0x83, 0x36,
		0x93, 0x36,
		0x9B, 0x36,
		0xA3, 0x36,
		0xAB, 0x36,
		0x8B, 0x36,
	};

	u8 addr_offset[] = {
//		0x80, 0x00, 0x00,
//		0x84, 0x00, 0x01,
//		0xC0, 0x02, 0x02,
//		0xC4, 0x02, 0x03,
		0x11<<1, 0x00, 0x00,
		0x12<<1, 0x00, 0x01,
		0x13<<1, 0x02, 0x02,
		0x14<<1, 0x02, 0x03,
		0x15<<1, 0x00, 0x00,
		0x16<<1, 0x00, 0x01,
		0x17<<1, 0x02, 0x02,
		0x18<<1, 0x02, 0x03,
	};

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
		err=max9295_write_reg(&prim_priv__->i2c_client->dev,
		MAX9295_DEV_ADDR, (g_ctx->ser_reg << 1));
		if(err<0){
			if(g_ctx->sensor_model == SENSOR_E003A_YUV_3G || g_ctx->sensor_model == SENSOR_E003A_YUV_ET  || g_ctx->sensor_model == SENSOR_F008AX_YUV_ET || g_ctx->sensor_model == SENSOR_ISX021_YUV_LI){
				prim_priv__->i2c_client->addr=0x64;
			}else{
				prim_priv__->i2c_client->addr=0x42;
			}
			err = max9295_write_reg(&prim_priv__->i2c_client->dev,
        		MAX9295_DEV_ADDR, (g_ctx->ser_reg << 1));
			if(g_ctx->sensor_model == SENSOR_E003A_YUV_3G || g_ctx->sensor_model == SENSOR_E003A_YUV_ET  || g_ctx->sensor_model == SENSOR_F008AX_YUV_ET || g_ctx->sensor_model == SENSOR_ISX021_YUV_LI){
				prim_priv__->i2c_client->addr=0x62;
			}else{
				prim_priv__->i2c_client->addr=0x40;
			}
        }
		if (!err) {
			dev_info(dev, "%s: Detect a camera linked!\n", __func__);
		}
		else {
			dev_info(dev, "%s: Detect no camera linked!\n", __func__);
		}
		//err = 0;
	}

	if(priv->group==1)
	{
		err=max9295_write_reg(&prim_priv1__->i2c_client->dev,
		MAX9295_DEV_ADDR, (g_ctx->ser_reg << 1));
		if(err<0){
			if(g_ctx->sensor_model == SENSOR_E003A_YUV_3G || g_ctx->sensor_model == SENSOR_E003A_YUV_ET  || g_ctx->sensor_model == SENSOR_F008AX_YUV_ET || g_ctx->sensor_model == SENSOR_ISX021_YUV_LI ){
				prim_priv__->i2c_client->addr=0x64;
			}else{
				prim_priv__->i2c_client->addr=0x42;
			}
			err = max9295_write_reg(&prim_priv1__->i2c_client->dev,
        			MAX9295_DEV_ADDR, (g_ctx->ser_reg << 1));
			if(g_ctx->sensor_model == SENSOR_E003A_YUV_3G || g_ctx->sensor_model == SENSOR_E003A_YUV_ET  || g_ctx->sensor_model == SENSOR_F008AX_YUV_ET || g_ctx->sensor_model == SENSOR_ISX021_YUV_LI ){
				prim_priv__->i2c_client->addr=0x62;
			}else{
				prim_priv__->i2c_client->addr=0x40;
			}
        }
		if (!err) {
			dev_info(dev, "%s: Detect a camera linked!\n", __func__);
		}
		else {
			dev_info(dev, "%s: Detect no camera linked!\n", __func__);
		}
		//err = 0;
	}

	if (g_ctx->frame_sync_en) {
#if defined(MFP7_MFP8_COMPAT)
			/*GPIO7*/
			max9295_write_reg(dev, 0x2D3, 0x85);
			/*GPIO8*/
			max9295_write_reg(dev, 0x2D6, 0x85);
#endif
	}

	if (g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_A)
		max9295_write_reg(dev, MAX9295_CTRL0_ADDR, 0x21);
	else if (g_ctx->serdes_csi_link == GMSL_SERDES_CSI_LINK_B)
		max9295_write_reg(dev, MAX9295_CTRL0_ADDR, 0x22);

	/* delay to settle link */
	msleep(50);

	for (i = 0; i < ARRAY_SIZE(addr_offset); i += 3) {
		if ((g_ctx->ser_reg << 1) == addr_offset[i]) {
			offset1 = addr_offset[i+1];
			offset2 = addr_offset[i+2];
			break;
		}
	}

	if (i == ARRAY_SIZE(addr_offset)) {
		dev_err(dev, "%s: invalid ser slave\n", __func__);
		//err = -EINVAL;
		goto error;
	}

	for (i = 0; i < ARRAY_SIZE(i2c_ovrd); i += 2) {
		/* update address overrides */
		//i2c_ovrd[i+1] += (i < 4) ? offset1 : offset2;
		i2c_ovrd[i+1] +=0;  //table value  don't change
		/* i2c passthrough2 must be configured once for all devices */
	//	if ((i2c_ovrd[i] == 0x8B) && prim_priv__->pst2_ref)
		if(priv->group==0){
			if (prim_priv__->pst2_ref%2==1)
			continue;
		}

		if(priv->group==1){
			if (prim_priv1__->pst2_ref%2==1)
			continue;
		}

		max9295_write_reg(dev, i2c_ovrd[i], i2c_ovrd[i+1]);
	}
	offset1 = 0;
	offset2 = 0;   //table value  don't change
	/* dev addr pass-through2 ref */
	if(priv->group==0)
		prim_priv__->pst2_ref++;
	if(priv->group==1)
		prim_priv1__->pst2_ref++;


	max9295_write_reg(dev, MAX9295_I2C4_ADDR, (g_ctx->sdev_reg << 1));
	max9295_write_reg(dev, MAX9295_I2C5_ADDR, (g_ctx->sdev_def << 1));

    max9295_write_reg(dev, MAX9295_RCLK_UART_ADDR,MAX9295_RCLK_25M_EN_UART_DIS) ;
	max9295_write_reg(dev, MAX9295_SRC_OUT_RCLK_ADDR, MAX9295_SRC_RCLK);
	max9295_write_reg(dev, MAX9295_RCLK_I2C_ADDR,MAX9295_RCLK_EN_I2C_SELECTED);
	max9295_write_reg(dev, MAX9295_GENERATION_PLL_ADDR,MAX9295_GENERATION_PLL_ENABLE);

	if (g_ctx->frame_sync_en) {
		if (g_ctx ->sensor_model == SENSOR_IMX390_RAW_BL) {
		/* setup MFP0 as GPIO  reception, ID = 0x6 */
			max9295_write_reg(dev, 0x2BE, 0x04); //GPIO_RX_EN=1
			max9295_write_reg(dev, 0x2BF, 0x26); //GPIO_TX_ID=6, OUT_TYPE=1
			//max9295_write_reg(dev, 0x2C0, 0x46); //GPIO_RX_ID=6.
			max9295_write_reg(dev, 0x2C0, 0x86); //OVR_RES_CFG=1, GPIO_RX_ID=6.

			/* setup MFP6  ID as 0x0 */
			max9295_write_reg(dev, 0x2D1, 0x20); //OUT_TYPE=1, GPIO_TX_ID=0
			max9295_write_reg(dev, 0x2D2, 0x40); //GPIO_RX_ID=0
		}

#if !defined(MFP7_MFP8_COMPAT)
		if (g_ctx ->sensor_model == SENSOR_E003A_YUV_3G || g_ctx->sensor_model == SENSOR_E003A_YUV_ET ||
				g_ctx->sensor_model == SENSOR_IMX490_YUV_SG_MFP8 || g_ctx->sensor_model == SENSOR_AR0820_YUV_SG || g_ctx->sensor_model ==SENSOR_OX08BC_YUV_SG) {
			/*GPIO8*/
			max9295_write_reg(dev, 0x2D6, 0x04); //GPIO_RX_EN=1
			max9295_write_reg(dev, 0x2D7, 0x26); //GPIO_TX_ID=6, OUT_TYPE=1
			max9295_write_reg(dev, 0x2D8, 0x86); //OVR_RES_CFG=1, GPIO_RX_ID=6.
		}else if(g_ctx->sensor_model == SENSOR_F008AX_YUV_ET){
//			max9295_write_reg(dev, 0x2D6, 0xC6); //1MOhm, high prio, rx en, tx en.
//			max9295_write_reg(dev, 0x2D7, 0xA6); //GPIO_TX_ID=6, pull down, push-pull.
//			max9295_write_reg(dev, 0x2D8, 0x06); //OVR_RES_CFG=0, GPIO_RX_ID=6.
			/*GPIO8*/
			max9295_write_reg(dev, 0x2D6, 0x04); //GPIO_RX_EN=1
			max9295_write_reg(dev, 0x2D7, 0x26); //GPIO_TX_ID=6, OUT_TYPE=1
			max9295_write_reg(dev, 0x2D8, 0x86); //OVR_RES_CFG=1, GPIO_RX_ID=6.
		}else {
			/*GPIO7*/
			max9295_write_reg(dev, 0x2D3, 0x04); //GPIO_RX_EN=1
			max9295_write_reg(dev, 0x2D4, 0x26); //GPIO_TX_ID=6, OUT_TYPE=1
			max9295_write_reg(dev, 0x2D5, 0x86); //OVR_RES_CFG=1, GPIO_RX_ID=6.

			//imitate the former code which setup GPIO6 (0x2D1/0x2D2), it seems useless at all.
			//max9295_write_reg(dev, 0x2D1, 0x27); //OUT_TYPE=1, GPIO_TX_ID=7
			//max9295_write_reg(dev, 0x2D2, 0x47); //GPIO_RX_ID=7

			/*GPIO0: MFP0 <--> Sensor Reset Control for SG2-IMX390C-GMSL2*/
			//uncomment if you would like to pull up here directly.
			//max9295_write_reg(dev, 0x2BE, 0x12); //GPIO_OUT=1, GPIO_TX_EN=1
			//max9295_write_reg(dev, 0x2BF, 0x60); //PULL UP, OUT_TYPE=1, GPIO_TX_ID=0
			//max9295_write_reg(dev, 0x2C0, 0x80); //OVR_RES_CFG=1, GPIO_RX_ID=0
		}
#endif
	}

	if (g_ctx ->sensor_model == SENSOR_IMX390_YUV_3G || g_ctx ->sensor_model == SENSOR_E003A_YUV_3G) {
		max9295_write_reg(dev, 0x0001, 0x04); //force 3Gbps TX rate
	}

	if(g_ctx->sensor_model == SENSOR_E003A_YUV_3G || g_ctx->sensor_model == SENSOR_E003A_YUV_ET || g_ctx->sensor_model == SENSOR_F008AX_YUV_ET){
		/*GPIO7: MFP7 <--> Sensor Reset Control for E003A120CM0A*/
		//uncomment if you would like to pull up here directly.
//		max9295_write_reg(dev, 0x2D3, 0x12); //GPIO_OUT=1, GPIO_TX_EN=1
//		max9295_write_reg(dev, 0x2D4, 0x60); //PULL UP, OUT_TYPE=1, GPIO_TX_ID=0 (default GPIO_TX_ID=7)
//		max9295_write_reg(dev, 0x2D5, 0x80); //OVR_RES_CFG=1, GPIO_RX_ID=0 (default GPIO_RX_ID=7)
	}

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9295_setup_control);

int max9295_reset_control(struct device *dev)
{
	struct max9295 *priv = dev_get_drvdata(dev);
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
		max9295_write_reg(dev, MAX9295_DEV_ADDR, (prim_priv__->def_addr << 1));
		max9295_write_reg(&prim_priv__->i2c_client->dev,
				MAX9295_CTRL0_ADDR, MAX9295_RESET_ALL);
	}

	if(priv->group==1){
		prim_priv1__->pst2_ref--;
		max9295_write_reg(dev, MAX9295_DEV_ADDR, (prim_priv1__->def_addr << 1));
		max9295_write_reg(&prim_priv1__->i2c_client->dev,
				MAX9295_CTRL0_ADDR, MAX9295_RESET_ALL);
	}

error:
	mutex_unlock(&priv->lock);
	return err;
}
EXPORT_SYMBOL(max9295_reset_control);

int max9295_sdev_pair(struct device *dev, struct gmsl_link_ctx *g_ctx)
{
	struct max9295 *priv;
	int err = 0;
	u16 current_ma;
	int n_tries = 0;

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

	do{
		max2008X_read_current(poc_dev_listp[n_9295_dev/4], (n_9295_dev%4)+1, &current_ma);
		dev_dbg(dev, "try %d: POC current is %d mA.\n", n_tries+1, current_ma);
		if(current_ma > 10)
			break;
		msleep(10);
		n_tries++;
	}while(n_tries < 3);

	priv->ser_num = n_9295_dev;

	if(n_9295_dev < 12){
		ser_9295_listp[n_9295_dev]=dev;
		n_9295_dev++;
	}

error:
	mutex_unlock(&priv->lock);
	return 0;
}
EXPORT_SYMBOL(max9295_sdev_pair);

int max9295_sdev_unpair(struct device *dev, struct device *s_dev)
{
	struct max9295 *priv = NULL;
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
EXPORT_SYMBOL(max9295_sdev_unpair);

static  struct regmap_config max9295_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	//.cache_type = REGCACHE_RBTREE,
	.cache_type = REGCACHE_NONE,
};

static int max9295_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max9295 *priv;
	int err = 0;
	struct device_node *node = client->dev.of_node;

	dev_dbg(&client->dev, "[MAX9295]: probing GMSL IO Expander\n");

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	priv->i2c_client = client;
	priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
				&max9295_regmap_config);
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

	priv->binocular = of_property_read_bool(node, "binocular");

	dev_set_drvdata(&client->dev, priv);

	dev_dbg(&client->dev, "%s:  success\n", __func__);

	return err;
}

static int max9295_remove(struct i2c_client *client)
{
	struct max9295 *priv;

	if (client != NULL) {
		priv = dev_get_drvdata(&client->dev);
		mutex_destroy(&priv->lock);
		i2c_unregister_device(client);
		client = NULL;
	}

	return 0;
}

static const struct i2c_device_id max9295_id[] = {
	{ "max9295", 0 },
	{ },
};

const struct of_device_id max9295_of_match[] = {
	{ .compatible = "nvidia,max9295", },
	{ },
};
MODULE_DEVICE_TABLE(of, max9295_of_match);
MODULE_DEVICE_TABLE(i2c, max9295_id);

static struct i2c_driver max9295_i2c_driver = {
	.driver = {
		.name = "max9295",
		.owner = THIS_MODULE,
	},
	.probe = max9295_probe,
	.remove = max9295_remove,
	.id_table = max9295_id,
};

static int __init max9295_init(void)
{
	return i2c_add_driver(&max9295_i2c_driver);
}

static void __exit max9295_exit(void)
{
	i2c_del_driver(&max9295_i2c_driver);
}

module_init(max9295_init);
module_exit(max9295_exit);

MODULE_DESCRIPTION("IO Expander driver max9295");
MODULE_AUTHOR("Sudhir Vyas <svyas@nvidia.com>");
MODULE_LICENSE("GPL v2");
