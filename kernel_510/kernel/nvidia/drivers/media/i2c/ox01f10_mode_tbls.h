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
#ifndef __OX01F10_I2C_TABLES__
#define __OX01F10_I2C_TABLES__

#include <media/camera_common.h>
#include <linux/miscdevice.h>

enum {
	OX01F10_MODE_1280X720_30FPS,
};

static const int ox01f10_30fps[] = {
	30,
};

/*
 * WARNING: frmfmt ordering need to match mode definition in
 * device tree!
 */

static const struct camera_common_frmfmt ox01f10_frmfmt[] = {
	{{1280, 720}, ox01f10_30fps, 1, 0, OX01F10_MODE_1280X720_30FPS},
	/* Add modes with no device tree support after below */
};
#endif /* __OX01F10_I2C_TABLES__ */
