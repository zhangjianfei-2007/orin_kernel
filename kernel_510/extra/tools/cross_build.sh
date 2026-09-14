#! /bin/bash

case $# in
	2) BSP_ROOT="$1"; BUILD_DIR="$2" ;;
	*)echo "Usage: $0 /bsp/root/abs/dir /build/abs/dir" 1>&2 ; exit 1 ;;
esac

make -C $BUILD_DIR CROSS_COMPILE="$BSP_ROOT/bootlin-toolchain-gcc-93/bin/aarch64-linux-" \
EXTRA_CFLAGS="--sysroot=$BSP_ROOT/Linux_for_Tegra/rootfs/ -I$BSP_ROOT/Linux_for_Tegra/rootfs/usr/include/aarch64-linux-gnu/" \
EXTRA_LDFLAGS="--sysroot=$BSP_ROOT/Linux_for_Tegra/rootfs/ -L$BSP_ROOT/Linux_for_Tegra/rootfs/usr/lib/aarch64-linux-gnu"
