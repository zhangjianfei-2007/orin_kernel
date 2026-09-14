/*
 * ap0202_mode_tbls.h - ap0202 sensor mode tables
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
#ifndef __AP0202_I2C_TABLES__
#define __AP0202_I2C_TABLES__

#include <media/camera_common.h>
#include <linux/miscdevice.h>

enum {
	AP0202_MODE_1920X1080_30FPS,
};

static const int ap0202_30fps[] = {
	30,
};

/*
 * WARNING: frmfmt ordering need to match mode definition in
 * device tree!
 */
static const struct camera_common_frmfmt ap0202_frmfmt[] = {
	{{1920, 1080}, ap0202_30fps, 1, 0, AP0202_MODE_1920X1080_30FPS},
	/* Add modes with no device tree support after below */
};
#endif /* __AP0202_I2C_TABLES__ */
