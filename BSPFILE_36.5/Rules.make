CWD=$(shell pwd)

BSP_ROOT=$(CWD)

#set hardware platform
#PLATFORM ?= p2888-0001-p2822-0000
#PLATFORM ?= p2888-0001-hy009-0000
#PLATFORM ?= p3701-0000-p3737-0000
#PLATFORM ?= p3701-0000-hy026-0000
#PLATFORM ?= p3701-0004-hy026-0001
#PLATFORM ?= p3701-0004-hy029-0000
PLATFORM ?= p3701-0004-hy026-0000

#architecture
ARCH=arm64

#soc name
#SOC ?= tegra194
SOC ?= tegra234

ifeq "${PLATFORM}" "p3701-0004-hy026-0000"
PRODUCT = titan5
else ifeq "${PLATFORM}" "p3701-0004-hy029-0000"
PRODUCT = titan5l
else ifeq "${PLATFORM}" "p3701-0004-hy026-0001"
PRODUCT = titan5p
else ifeq "${PLATFORM}" "p2888-0001-hy009-0000"
PRODUCT = titan4
else ifeq "${PLATFORM}" "p3701-0000-p3737-0000"
PRODUCT = orin
else
PRODUCT = xavier
endif

CAMERA_TYPE ?= CAMERA_GW5200_IMX390

CROSS_COMPILE=$(BSP_ROOT)/bootlin-toolchain-gcc-93/bin/aarch64-linux-

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
CXX=$(CROSS_COMPILE)g++
OBJCOPY=$(CROSS_COMPILE)objcopy
OBJDUMP=$(CROSS_COMPILE)objdump

#LOCALVERSION="-tegra"

#todo: bootloader

ROOTFS=$(BSP_ROOT)/Linux_for_Tegra/rootfs
KERNEL_OUT=$(BSP_ROOT)/out/kernel
MODULES_OUT=$(BSP_ROOT)/out/modules

RULE_DIR=$(BSP_ROOT)/src/extra/rules
SERVICE_DIR=$(BSP_ROOT)/src/extra/service
RC_DIR=$(BSP_ROOT)/src/extra/rcs
SCR_DIR=$(BSP_ROOT)/src/extra/scripts
BIN_DIR=$(BSP_ROOT)/src/extra/binaries
TOOL_DIR=$(BSP_ROOT)/src/extra/tools

RULE_DIR_029=$(BSP_ROOT)/src/extra/titan5l/rules
SERVICE_DIR_029=$(BSP_ROOT)/src/extra/titan5l/service
RC_DIR_029=$(BSP_ROOT)/src/extra/titan5l/rcs
SCR_DIR_029=$(BSP_ROOT)/src/extra/titan5l/scripts
BIN_DIR_029=$(BSP_ROOT)/src/extra/titan5l/binaries
TOOL_DIR_029=$(BSP_ROOT)/src/extra/titan5l/tools

OPENSRC_DISP_DIR=$(BSP_ROOT)/src/kernel/NVIDIA-kernel-module-source-TempVersion/kernel-open

DEB_SRC=$(BSP_ROOT)/src/deb
DEB_OUT=$(BSP_ROOT)/out/deb
DEB_PKG_DIR=$(BSP_ROOT)/out/deb

SRC_DTB=$(KERNEL_OUT)/arch/arm64/boot/dts/nvidia/$(SOC)-$(PLATFORM).dtb
TARGET_DTB=/boot/dtb/kernel_$(SOC)-$(PLATFORM).dtb

CUSTOM_DTB=$(SOC)-$(PLATFORM)-user-custom.dtb
TARGET_CUSTOM_DTB=/boot/$(CUSTOM_DTB)

TARGET_KERNEL=/boot/Image

#set camera support for dtbs version
ifeq ($(SOC),tegra194)
CAMERA=$(shell sed -n 's/^\#inc.*-camera-\(.\+\)\.dtsi.*/\1/p' $(BSP_ROOT)/src/hardware/nvidia/platform/t19x/galen/kernel-dts/$(SOC)-$(PLATFORM).dts)
else
CAMERA=$(shell sed -n '/( *$(CAMERA_TYPE) *)/,+1s/^\#inc.*-camera-\(.\+\)\.dtsi.*/\1/p' $(BSP_ROOT)/src/hardware/nvidia/platform/t23x/concord/kernel-dts/$(SOC)-$(PLATFORM).dts)
endif

KERNEL_IN=$(BSP_ROOT)/src/kernel/kernel-5.10
KERNEL_VER = $(shell sed -n 's/^.\+[ |\t]\+UTS_RELEASE[ |\t]\+\"\(.\+\)\"[ |\t]*$$/\1/p' $(KERNEL_OUT)/include/generated/utsrelease.h)
ifeq ($(KERNEL_VER),)
KERNEL_VER=5.10
endif

GIT_VER=$(shell git -C $(BSP_ROOT)/src/kernel rev-list HEAD | wc -l)
GIT_BRANCH=$(shell git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/\1/')
GIT_COMMIT=$(shell git -C $(BSP_ROOT)/src/kernel rev-list HEAD --abbrev-commit --max-count=1)
KERNEL_GIT_VER=$(GIT_VER)-$(GIT_BRANCH)-$(GIT_COMMIT)

#get date and time from the end of line other than begin of line
KERNEL_BUILD_DATE=$(shell awk 'BEGIN { FS = " " } /define UTS_VERSION/ { date=$$(NF-3); split($$(NF-2), a, ":") } END { if(date<10) print "0" date a[1] a[2] a[3]; else print date a[1] a[2] a[3] }' $(KERNEL_OUT)/include/generated/compile.h)

ifeq ($(KERNEL_GIT_VER),)
KERNEL_PKG_VER=$(KERNEL_VER)-$(KERNEL_BUILD_DATE)
else
KERNEL_PKG_VER=$(KERNEL_VER)-$(KERNEL_GIT_VER)-$(KERNEL_BUILD_DATE)
endif

DTB_BUILD_DATE=$(shell fdtdump $(SRC_DTB) | awk 'BEGIN { FS = "\"" } /dtbbuildtime/ { split($$2, date, " "); split($$4, time, ":") } END { if(date[2]<10) print "0" date[2] time[1] time[2] time[3]; else print date[2] time[1] time[2] time[3] }')

ifeq ($(CAMERA),modules)
CAMERA=unknown
endif
ifeq ($(CAMERA),)
CAMERA=unknown
endif
DTB_PKG_VER=$(GIT_VER)-$(PRODUCT)-$(CAMERA)
DTB_ALIAS_VER=$(KERNEL_PKG_VER)-$(PRODUCT)-$(CAMERA)-$(DTB_BUILD_DATE)

