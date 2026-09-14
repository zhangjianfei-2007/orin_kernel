/**
 * Copyright (c) 2018, NVIDIA Corporation.  All rights reserved.
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

#ifndef __MAX96705_H__
#define __MAX96705_H__

#include <media/gmsl-link.h>

int max96705_setup_control(struct device *dev);

int max96705_reset_control(struct device *dev);

int max96705_sdev_pair(struct device *dev, struct gmsl_link_ctx *g_ctx);

int max96705_sdev_unpair(struct device *dev, struct device *s_dev);

int max96705_write_reg(struct device *dev,u16 addr, u8 val);

int max96705_read_reg(struct device *dev,u16 addr, u8 *val);
#endif  /* __MAX96705_H__ */
