/**
 * Copyright (c) 2018-2019, NVIDIA Corporation.  All rights reserved.
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

#ifndef __MAX96712_H__
#define __MAX96712_H__

#include <media/gmsl-link.h>

int max96712_setup_link(struct device *dev, struct device *s_dev);

int max96712_setup_control(struct device *dev);

int max96712_reset_control(struct device *dev, struct device *s_dev);

int max96712_sdev_register(struct device *dev, struct gmsl_link_ctx *g_ctx);

int max96712_sdev_unregister(struct device *dev, struct device *s_dev);

int max96712_setup_streaming(struct device *dev, struct device *s_dev);

int max96712_start_streaming(struct device *dev, struct device *s_dev);

int max96712_stop_streaming(struct device *dev, struct device *s_dev);

int max96712_power_on(struct device *dev);

void max96712_power_off(struct device *dev);

int max96712_write_reg(struct device *dev,u16 addr, u8 val);

int max96712_read_reg(struct device *dev, u16 addr, u8 *val);

int max96712_link_splitter(struct device *dev, u8 splitter_mode);

int max96712_link_reset(struct device *dev, u8 link);

int max96712_set_tx_rate_3g(struct device *dev);

int max96712_link_splitter_gmsl1(struct device *dev, u8 splitter_mode);
int max96712_setup_link_gmsl1(struct device *dev, struct device *s_dev);
int max96712_setup_streaming_gmsl1(struct device *dev, struct device *s_dev);
int max96712_start_streaming_gmsl1(struct device *dev, struct device *s_dev);
int max96712_stop_streaming_gmsl1(struct device *dev, struct device *s_dev);

#endif  /* __MAX96712_H__ */
