/*
 * gw5200_mode_tbls.h - gw5200 sensor mode tables
 *
 *
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
#ifndef __GW5200_I2C_TABLES__
#define __GW5200_I2C_TABLES__

#include <media/camera_common.h>
#include <linux/miscdevice.h>

enum {
	GW5200_MODE_1920X1080_30FPS,
	GW5200_MODE_2880X1860_30FPS,
	GW5200_MODE_3840X2160_30FPS,
	GW5200_MODE_1920X1280_20FPS,
	GW5200_MODE_1920X1280_30FPS,
};

static const int gw5200_30fps[] = {
	30,
};

static const int gw5200_20fps[] = {
	20,
};

/*
 * WARNING: frmfmt ordering need to match mode definition in
 * device tree!
 */
static const struct camera_common_frmfmt gw5200_frmfmt[] = {
	{{1920, 1080}, gw5200_30fps, 1, 0, GW5200_MODE_1920X1080_30FPS},
	{{2880, 1860}, gw5200_30fps, 1, 0, GW5200_MODE_2880X1860_30FPS},
	{{3840, 2160}, gw5200_30fps, 1, 0, GW5200_MODE_3840X2160_30FPS},
	{{1920, 1280}, gw5200_20fps, 1, 0, GW5200_MODE_1920X1280_20FPS},
	{{1920, 1280}, gw5200_30fps, 1, 0, GW5200_MODE_1920X1280_30FPS},
	/* Add modes with no device tree support after below */
};
#endif /* __GW5200_I2C_TABLES__ */
