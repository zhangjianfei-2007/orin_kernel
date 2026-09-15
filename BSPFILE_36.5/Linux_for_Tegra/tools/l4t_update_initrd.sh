#!/bin/bash

# SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#
# This script updates the base initrd image Linux_for_Tegra/bootloader/l4t_initrd.img
# and rootfs/boot/initrd using the nv-update-initrd script.
#

set -e
set -o pipefail

function InitVar
{
	if [ -z "${LDK_DIR}" ]; then
		echo "ERROR: LDK_DIR not provide!"
		exit 1
	fi

	LDK_ROOTFS_DIR="${LDK_DIR}/rootfs"
	LDK_BOOTLOADER_DIR="${LDK_DIR}/bootloader"

	ROOTFS_INITRD="${LDK_ROOTFS_DIR}/boot/initrd"
	BASE_INITRD="${LDK_BOOTLOADER_DIR}/l4t_initrd.img"

	if [ ! -d "${LDK_ROOTFS_DIR}" ]; then
		echo "ERROR: ${LDK_ROOTFS_DIR} does not exist!"
		exit 1
	fi
	echo "Using rootfs directory: ${LDK_ROOTFS_DIR}"

	if [ ! -f "${BASE_INITRD}" ]; then
		echo "ERROR: ${BASE_INITRD} does not exist!"
		exit 1
	fi
}

function CheckPackage
{
	if ! dpkg -s "$1" > /dev/null 2>&1; then
		echo "The required $1 is not installed. Please install it before proceeding."
		exit 1
	fi
}

#
# Setup target env
#
function PrepareVirEnv
{
	echo "Preparing virtual env"
	cp /usr/bin/qemu-aarch64-static "${LDK_ROOTFS_DIR}/usr/bin"
	mv "${LDK_ROOTFS_DIR}/etc/resolv.conf" "${LDK_ROOTFS_DIR}/etc/resolv.conf.saved"
	cp /etc/resolv.conf "${LDK_ROOTFS_DIR}/etc/"
}

#
# Cleanup target env
#
function CleanupVirEnv
{
	echo "Cleaning up virtual env"
# The /etc/resolv.conf (and therefore /etc/resolv.conf.saved) on the target
# rootfs can be a symlink. Moreover, the symlink can point to an absolute path,
# in which case, when resolved from outside of the context of the target rootfs
# (i.e. the host), may or may not be valid. In fact, we should assume that
# an absolute path symlink is invalid on contexts outside of the target rootfs.
#
# When the symlink is invalid (doesn't point to something that exists), the
# bash -f test will fail...even though the symlink actually exists.
#
# Therefore, we need to check if the file is a valid file OR a symlink, and
# in the (likely) case that it does, move it, to successfully restore the
# original resolv.conf here.
	if [ -f "${LDK_ROOTFS_DIR}/etc/resolv.conf.saved" ] ||
	   [ -L "${LDK_ROOTFS_DIR}/etc/resolv.conf.saved" ]; then
		mv "${LDK_ROOTFS_DIR}/etc/resolv.conf.saved" "${LDK_ROOTFS_DIR}/etc/resolv.conf"
	fi
	rm -f "${LDK_ROOTFS_DIR}/usr/bin/qemu-aarch64-static"
}

function CopyBaseInitrdToRootfs
{
	echo "Copy ${BASE_INITRD} to ${ROOTFS_INITRD}"
	cp -f "${BASE_INITRD}" "${ROOTFS_INITRD}"
}

function UpdateBackToBaseInitrd
{
	echo "Update ${ROOTFS_INITRD} back to ${BASE_INITRD}"
	cp -f "${ROOTFS_INITRD}" "${BASE_INITRD}"
}

function UpdateInitrd
{
	# Update the initrd in the rootfs.
	trap CleanupVirEnv EXIT
	PrepareVirEnv
	if ! LC_ALL=C chroot "${LDK_ROOTFS_DIR}" nv-update-initrd ; then
		echo "ERROR: nv-update-initrd failed!"
		exit 1
	else
		echo "nv-update-initrd successful!"
		CleanupVirEnv
		trap - EXIT
	fi
}

function ShowUsage
{
	echo "Use: ${SCRIPT_NAME} [--LDK-DIR|-l PATH] [--help|h]"
cat <<EOF
	This script updates the base initrd image and rootfs initrd image
	Options are:
	--ldk_dir|-l PATH
			Linux_for_Tegra location
	--help|h
			show this help
EOF
}

SCRIPT_NAME=$(basename "$0")

TGETOPT=$(getopt -n "SCRIPT_NAME" --longoptions ldk_dir:,help -o l:h -- "$@")

eval set -- "$TGETOPT"

while [ $# -gt 0 ]; do
	case "$1" in
	-l|--ldk_dir) LDK_DIR=$2; shift;;
	-h|--help) ShowUsage; exit 1;;
	--) shift; break;;
	esac
	shift
done

CheckPackage qemu-user-static

if [ -z "$LDK_DIR" ]; then
	# If LDK_DIR is not set, it is assumed that the script is being called from the Linux_for_Tegra/
	LDK_DIR=$(pwd)
fi
echo "Set LDK_DIR to ${LDK_DIR}"
InitVar

echo "Updating the initrd: ${BASE_INITRD}"
CopyBaseInitrdToRootfs
UpdateInitrd
UpdateBackToBaseInitrd
echo "update_initrd.sh success!"
