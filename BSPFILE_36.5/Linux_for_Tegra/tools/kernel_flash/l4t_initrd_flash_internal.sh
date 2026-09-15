#!/bin/bash

# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

# Usage: ./l4t_initrd_flash_internal.sh [ --external-device <ext> -c <cfg> -S <SIZE> ] <target> <rootfs_dir>
# This script contains the core functionality of initrd flash

set -eo pipefail

L4T_INITRD_FLASH_DIR="$(cd "$(dirname "${0}")" && pwd)"
L4T_TOOLS_DIR="${L4T_INITRD_FLASH_DIR%/*}"
LINUX_BASE_DIR="${L4T_TOOLS_DIR%/*}"
BOOTLOADER_DIR="${LINUX_BASE_DIR}/bootloader"
ROOTFS_DIR="${LINUX_BASE_DIR}/rootfs"
# Change this if you want to use a different Rootfs for initrd nfs flash
# This variable is used by flash_through_ssh function
NFS_ROOTFS_DIR="${ROOTFS_DIR}"
BOOT_CTRL_CONF="${ROOTFS_DIR}/etc/nv_boot_control.conf"
NFS_IMAGES_DIR="${L4T_INITRD_FLASH_DIR}/images"
INITRDDIR_L4T_DIR="${L4T_INITRD_FLASH_DIR}/initrd_flash"
KERNEL_FLASH_SCRIPT=""
FLASH_IMG_MAP="initrdflashimgmap.txt"
nargs=$#;
target_rootdev=${!nargs};
nargs=$((nargs-1));
target_board=${!nargs};
working_dir=$(mktemp -d)
TEMP_INITRD_FLASH_DIR=""
error_message=
CHIPID=$(LDK_DIR=${LINUX_BASE_DIR}; source "${LDK_DIR}/${target_board}.conf";echo "${CHIPID}")
with_systemimg=
qspi_only=""
mass_storage_only=""
ODM_IMAGE_GEN=""
gen_read_ramcode=0
CREATE_FLASH_SCRIPT="${L4T_INITRD_FLASH_DIR}/l4t_create_images_for_kernel_flash.sh"
export PATH="${L4T_INITRD_FLASH_DIR}/bin:${PATH}"

trap cleanup EXIT

cleanup()
{
	if [ -n "${error_message}" ]; then
		echo -e "${error_message}"
	fi
	echo "Cleaning up..."
	clean_dev_folder
	[ -z "${keep}" ] && rm -rf "${working_dir}"
	if [ -n "${device_instance}" ]; then
		DEST_FOLDER=${LINUX_BASE_DIR}/temp_initrdflash
		[ -z "${keep}" ] && [ -z "${reuse}" ] && rm -rf "${DEST_FOLDER}/bootloader${device_instance}"
	fi
	if [ -n "${keep}" ]; then
		echo "Keeping working dir at ${DEST_FOLDER}/bootloader${device_instance} and ${working_dir}"
	fi
}

clean_dev_folder()
{
	if [ -f "/dev/${sd_emmc_dev}" ]; then
			rm "/dev/${sd_emmc_dev}"
	fi
	if [ -f "/dev/${internal_emmc_boot0}" ]; then
			rm "/dev/${internal_emmc_boot0}"
	fi
	if [ -f "/dev/${internal_emmc_boot1}" ]; then
			rm "/dev/${internal_emmc_boot1}"
	fi
	if [ -f "/dev/${ext}" ]; then
			rm "/dev/${ext}"
	fi
}

# We used to use "udevadm info --query=property" to get stuff like ID_VENDOR,
# ID_MODEL, ID_VENDOR_ID, ID_MODEL_ID, and ID_SERIAL_SHORT. However, in an lxc
# container, these environment properties are not reported for devices that
# are mapped into the container.
#
# However, these same values are available as device attributes along the
# device hierarchy, and this does work within an lxc container. Therefore, we
# switch to using "udevadm info --attribute-walk" to get these values.
#
# The device attributes that correspond to the environment properties are,
# respectively, vendor, model, idVendor, idProduct, and serial.
get_udev_attribute()
{
	path=$1
	attr=$2

	properties=$(flock -w 60 /var/lock/nvidiainitrdflash udevadm info --attribute-walk "$path")
	echo "${properties}" | sed -n "0,/^[ ]*ATTRS{$attr}==\"\(.*\)\"\$/s//\1/p" | xargs
}

generate_flash_package()
{
	local cmd
	cmd=("${CREATE_FLASH_SCRIPT}")
	if [ -n "${external_device}" ]; then
		cmd+=("--external-device" \
		"${external_device}" "-c" "${config_file}")
		if [ -n "${external_size}" ]; then
			cmd+=("-S" "${external_size}")
		fi
	fi

	if [ -n "${append}" ]; then
		cmd+=("--append")
	fi

	if [ -n "${external_only}" ]; then
		cmd+=("${external_only}")
	fi

	if [ -n "${OPTIONS}" ]; then
		cmd+=("-p" "${OPTIONS}")
	fi

	if [ -n "${KEY_FILE}" ] && [ -f "${KEY_FILE}" ]; then
		cmd+=("-u" "${KEY_FILE}")
	fi

	if [ -n "${SBK_KEY}" ] && [ -f "${SBK_KEY}" ]; then
		cmd+=("-v" "${SBK_KEY}")
	fi

	if [ -n "${pv_crt}" ] && [ -f "${pv_crt}" ]; then
		cmd+=("--pv-crt" "${pv_crt}")
	fi

	if [ -n "${ENC_RFS_KEY}" ] && [ -f "${ENC_RFS_KEY}" ]; then
		cmd+=("-i" "${ENC_RFS_KEY}")
	fi

	if [ -n "${pv_enc}" ] && [ -f "${pv_enc}" ]; then
		cmd+=("--pv-enc" "${pv_enc}" )
	fi

	if [ -n "${target_partname}" ]; then
		cmd+=("-k" "${target_partname}")
	fi

	if [ -n "${target_partfile}" ]; then
		cmd+=("--image" "${target_partfile}")
	fi

	if [ -n "${with_systemimg}" ]; then
		cmd+=("--with-systemimg")
	fi

	if [ -n "${UEFI_KEYS_CONF}" ] && [ -f "${UEFI_KEYS_CONF}" ]; then
		cmd+=("--uefi-keys" "${UEFI_KEYS_CONF}")
	fi

	if [ -n "${UEFI_ENC}" ] && [ -f "${UEFI_ENC}" ]; then
		cmd+=("--uefi-enc" "${UEFI_ENC}")
	fi

	if [ -n "${qspi_only}" ]; then
		cmd+=("--qspi-only")
	fi

	if [ -n "${mass_storage_only}" ]; then
		cmd+=("--mass-storage-only")
	fi

	if [ "${gen_read_ramcode}" -eq 1 ]; then
		cmd+=("--read-ramcode")
	fi

	if [ "${hsm_enable}" -eq 1 ]; then
		cmd+=("--hsm")
	fi

	[ "${sparse_mode}" = "1" ] && cmd+=("--sparse")
	cmd+=("${target_board}" "${target_rootdev}")

	"${cmd[@]}"
}

function get_disk_name
{
	local ext_dev="${1}"
	local disk=
	# ${ext_dev} could be specified as a partition; therefore, removing the
	# number if external storage device is scsi, otherwise, remove the trailing
	# "p[some number]" here
	if [[ "${ext_dev}" = sd* ]]; then
		disk=${ext_dev%%[0-9]*}
	else
		disk="${ext_dev%p*}"
	fi
	echo "${disk}"
}

build_working_dir()
{

	local device_instance=${1}
	DEST_FOLDER=${LINUX_BASE_DIR}/temp_initrdflash

	mkdir -p "${DEST_FOLDER}"

	TEMP_INITRD_FLASH_DIR="${DEST_FOLDER}/bootloader${device_instance}"

	if [ -z "${reuse}" ]; then
		echo "Create flash environment ${device_instance}"

		copy_bootloader "${TEMP_INITRD_FLASH_DIR}/" "${CHIPID}" "$(cat "${BOOTLOADER_DIR}/flashcmd.txt")"


		echo "Finish creating flash environment ${device_instance}."
	else
		echo "Reuse flash environment ${device_instance}"
	fi

}

generate_rcm_bootcmd()
{
	local cmd
	local cmdarg=

	if [ -n "${KEY_FILE}" ] && [ -f "${KEY_FILE}" ]; then
		cmdarg+="-u \"${KEY_FILE}\" "
	fi

	if [ -n "${SBK_KEY}" ] && [ -f "${SBK_KEY}" ]; then
		cmdarg+="-v \"${SBK_KEY}\" "
	fi

	if [ ${gen_read_ramcode} -eq 1 ]; then
		cmdarg+="--read-ramcode "
	fi

	if [ -n "${UEFI_KEYS_CONF}" ] && [ -f "${UEFI_KEYS_CONF}" ]; then
		cmdarg+="--uefi-keys ${UEFI_KEYS_CONF} "
	fi

	if [ "${hsm_enable}" -eq 1 ]; then
		cmdarg+="--hsm "
	fi

	export BOARDID
	export FAB
	export BOARDSKU
	export BOARDREV
	export CHIP_SKU
	export RAMCODE_ID
	export RAMCODE
	cmd="ROOTFS_AB= ROOTFS_ENC= ${LINUX_BASE_DIR}/flash.sh ${cmdarg} --no-flash --rcm-boot ${target_board} mmcblk0p1"
	echo "${cmd}"
	eval "${cmd}"

	cmd=()

	if [ -n "${append}" ]; then
		# restore external_device var when append option is specified
		if [ -f "${NFS_IMAGES_DIR}/external/flash.cfg" ]; then
			external_device="$(source "${NFS_IMAGES_DIR}/external/flash.cfg"; echo "${external_device}")"
		fi
	fi

	if [ -n "${external_device}" ]; then
		cmd+=("--external-device" \
		"${external_device}" "-c" "\"${config_file}\"")
		if [ -n "${external_size}" ]; then
			cmd+=("-S" "${external_size}")
		fi
	fi

	if [ -n "${target_partname}" ]; then
		cmd+=("-k" "${target_partname}")
	fi

	if [ -n "${initrd_only}" ]; then
		cmd+=("--initrd")
	fi

	if [ -n "${direct}" ]; then
		cmd+=("--direct" "${direct}")
	fi

	if [ -n "${network}" ]; then
		cmd+=("--network" "${network}")
	fi

	if [ -n "${EKB_PAIR}" ]; then
		cmd+=("--ekb-pair")
	fi

	if [ "${gen_read_ramcode}" -eq 1 ]; then
		cmd+=("--read-ramcode")
	fi

	echo "${cmd[*]} ${target_board} ${target_rootdev}" > "${L4T_INITRD_FLASH_DIR}/${INITRD_FLASHPARAM}"
	echo "Save initrd flashing command parameters to ${L4T_INITRD_FLASH_DIR}/${INITRD_FLASHPARAM}"
}

ping_device()
{
	while IFS=  read -r; do
		netpath=/sys/class/net/${REPLY}
		netserialnumber=$(get_udev_attribute "${netpath}" serial)
		if [ "${netserialnumber}" = "${serialnumber}" ]; then
			echo "${REPLY}" > "${sshcon}"
			if [ -z "${IP_SET}" ]; then
				if [ "$(sysctl -n "net.ipv6.conf.${REPLY}.disable_ipv6")" -eq 1 ]; then
					echo "sysctl -n net.ipv6.conf.${REPLY}.disable_ipv6"
					sysctl "net.ipv6.conf.${REPLY}.disable_ipv6"
					echo "IPv6 is disabled. Please enable ipv6 to use this tool"
				fi
				ip a add fc00:1:1:"${device_instance}"::1/64 dev "${REPLY}"
				ip a add fe80::2/64 dev "${REPLY}"
				IP_SET=0
			fi
			ip link set dev "${REPLY}" up
		fi
	done < <(ls /sys/class/net)

	if [ -z "$(cat "${sshcon}")" ]; then
		return 1
	fi
	if ! ping6 -c 1 "fe80::1%$(cat "${sshcon}")" > /dev/null 2>&1;
	then
		return 1
	fi
	return 0
}

run_commmand_on_target()
{
	local OLD_LC_ALL="${LC_ALL}"
	local OLD_LANG="${LANG}"
	local OLD_LANGUAGE="${LANGUAGE}"
	export LC_ALL="" LANG="en_US.UTF-8" LANGUAGE=""
	echo "Run command: ${2} on root@fe80::1%${1}"
	count=0
	maxcount=10
	while ! sshpass -p root ssh "root@fe80::1%${1}" "${SSH_OPT[@]}" "echo SSH ready"
	do
		count=$((count + 1))
		if [ "${count}" -ge "${maxcount}" ]; then
			echo "SSH is not ready"
			return 1
		fi
		sleep 1
	done
	sshpass -p root ssh "${SSH_OPT[@]}" "root@fe80::1%${1}" "$2";
	result=$?
	export LC_ALL="${OLD_LC_ALL}" LANG="${OLD_LANG}" LANGUAGE="${OLD_LANGUAGE}"
	return $result
}

copy_qspi_flash_packages()
{
	if ! grep -qE "^[0-9]+, 3:0" "${NFS_IMAGES_DIR}/internal/flash.idx"; then
		return
	fi
	mkdir -p "${working_dir}/initrd/qspi/internal"

	for i in $(grep -E "^[0-9]+, 3:0" "${NFS_IMAGES_DIR}/internal/flash.idx" | sed 's/[ \t]\?,[ \t]\?/,/g' | awk -F"," '{print $5}' | uniq | grep -v "^$");
	do
		cp "${NFS_IMAGES_DIR}/internal/${i}" "${working_dir}/initrd/qspi/internal"
	done

	cp "${NFS_IMAGES_DIR}/internal/flash.idx" "${working_dir}/initrd/qspi/internal"
	cp "${NFS_IMAGES_DIR}/internal/flash.cfg" "${working_dir}/initrd/qspi/internal"
	cp "${NFS_IMAGES_DIR}/${KERNEL_FLASH_SCRIPT}" "${working_dir}/initrd/qspi"
}

generate_flash_initrd()
{
	local dev_instance="$1"

	pushd "${working_dir}"

	abootimg -x "${BOOTLOADER_DIR}/recovery.img"

	mkdir -p "${working_dir}/initrd"

	pushd "${working_dir}/initrd"

	gunzip -c "${working_dir}/initrd.img" | cpio -i

	cp "${INITRDDIR_L4T_DIR}/"*.sh "${working_dir}/initrd/bin"
	cp "${ROOTFS_DIR}/usr/sbin/flash_erase" "${working_dir}/initrd/usr/sbin"
	cp "${ROOTFS_DIR}/usr/sbin/mtd_debug" "${working_dir}/initrd/usr/sbin"
	cp "${ROOTFS_DIR}/bin/kmod" "${working_dir}/initrd/bin"
	ln -fs /bin/kmod "${working_dir}/initrd/usr/sbin/modprobe"
	ln -fs /bin/kmod "${working_dir}/initrd/usr/sbin/lsmod"
	cp "${ROOTFS_DIR}/usr/bin/sort" "${working_dir}/initrd/usr/bin"
	cp "${ROOTFS_DIR}/usr/bin/nohup" "${working_dir}/initrd/usr/bin"
	cp "${ROOTFS_DIR}/sbin/blkdiscard" "${working_dir}/initrd/sbin"
	cp "${ROOTFS_DIR}/sbin/partprobe" "${working_dir}/initrd/sbin"
	cp "${ROOTFS_DIR}/bin/mktemp" "${working_dir}/initrd/bin"
	cp "${ROOTFS_DIR}/lib/aarch64-linux-gnu/libsmartcols.so.1" "${working_dir}/initrd/lib/aarch64-linux-gnu"
	cp "${ROOTFS_DIR}/usr/lib/aarch64-linux-gnu/libbsd.so.0" "${working_dir}/initrd/usr/lib/aarch64-linux-gnu/libbsd.so.0"
	cp "${ROOTFS_DIR}/usr/lib/aarch64-linux-gnu/libparted.so.2" "${working_dir}/initrd/usr/lib/aarch64-linux-gnu/libparted.so.2"
	cp "${ROOTFS_DIR}/usr/lib/aarch64-linux-gnu/libzstd.so.1" "${working_dir}/initrd/usr/lib/aarch64-linux-gnu/libzstd.so.1"
	KERNEL_VERSION="$(strings "${LINUX_BASE_DIR}/kernel/Image" | grep -oE "Linux version [0-9a-zA-Z\.\-]+[+]* " | cut -d\  -f 3 | head -1)"
	mkdir -p "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/drivers/"
	cp -r "${ROOTFS_DIR}/lib/modules/${KERNEL_VERSION}/kernel/drivers/nvme"  "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/drivers/"
	cp "${ROOTFS_DIR}/lib/modules/${KERNEL_VERSION}/modules"*  "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/"
	cp -r "${ROOTFS_DIR}/lib/modules/${KERNEL_VERSION}/kernel/drivers/spi"  "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/drivers/"
	cp -r "${ROOTFS_DIR}/usr/lib/modules/${KERNEL_VERSION}/kernel/drivers/thermal" "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/drivers/"
	cp -r "${ROOTFS_DIR}/usr/lib/modules/${KERNEL_VERSION}/kernel/drivers/hwmon" "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/drivers/"
	cp -r "${ROOTFS_DIR}/usr/lib/modules/${KERNEL_VERSION}/kernel/drivers/pci" "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/drivers/"
	cp -r "${ROOTFS_DIR}/usr/lib/modules/${KERNEL_VERSION}/kernel/drivers/phy" "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/drivers/"
	cp -r "${ROOTFS_DIR}/usr/lib/modules/${KERNEL_VERSION}/kernel/drivers/usb" "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/drivers/"
	cp -r "${ROOTFS_DIR}/usr/lib/modules/${KERNEL_VERSION}/kernel/net" "${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/kernel/"

	KERNEL_UPDATES="${ROOTFS_DIR}/usr/lib/modules/${KERNEL_VERSION}/updates"
	INITRD_UPDATES="${working_dir}/initrd/lib/modules/${KERNEL_VERSION}/updates"
	if [ -d "${KERNEL_UPDATES}/drivers/spi" ]; then
		cp -r "${KERNEL_UPDATES}/drivers/spi" \
			"${INITRD_UPDATES}/drivers/"
	fi
	if [ -d "${KERNEL_UPDATES}/drivers/net" ]; then
		cp -r "${KERNEL_UPDATES}/drivers/net" \
			"${INITRD_UPDATES}/drivers/"
	fi
	if [ -d "${KERNEL_UPDATES}/drivers/nvpps" ]; then
		cp -r "${KERNEL_UPDATES}/drivers/nvpps" \
			"${INITRD_UPDATES}/drivers/"
	fi
	if [ -f "${KERNEL_UPDATES}/drivers/platform/tegra/mce/tegra-mce.ko" ]; then
		mkdir -p "${INITRD_UPDATES}/drivers/platform/tegra/mce"
		cp "${KERNEL_UPDATES}/drivers/platform/tegra/mce/tegra-mce.ko" \
			"${INITRD_UPDATES}/drivers/platform/tegra/mce/"
	fi

	if [ -n "${network}" ]; then
		local arr
		IFS=':' read -r -a arr <<< "${network}"
		{
			echo "nfsnet=${arr[0]}"
			echo "targetip=${arr[1]}"
			echo "hostip=${arr[2]}"
			echo "gateway=${arr[3]}"
			echo "kernel_flash_script=${KERNEL_FLASH_SCRIPT}"
		} >> "${working_dir}/initrd/initrd_flash.cfg"
	fi

	mkdir -p "${working_dir}/initrd/etc"
	tnspec=$( awk '/TNSPEC/ {print $2}' "${BOOT_CTRL_CONF}" )
	echo "${tnspec}" > "${working_dir}/initrd/etc/board_spec.txt"

	if [ -n "${external_device}" ]; then
		echo "external_device=/dev/$(get_disk_name "${external_device}")" >> "${working_dir}/initrd/initrd_flash.cfg"
	fi
	if [ -n "${erase_all}" ]; then
		echo "erase_all=1" >> "${working_dir}/initrd/initrd_flash.cfg"
	fi
	echo "instance=${dev_instance}" >> "${working_dir}/initrd/initrd_flash.cfg"

	# Prepare for QSPI image flashing in initrd if neccessary
	if [ -z "${initrd_only}" ]; then
		copy_qspi_flash_packages
	fi

	find . | cpio -H newc -o | gzip -9 -n > "${working_dir}/initrd.img"

	popd

	cmdline=$(sed -n 's/^cmdline = //p' "${working_dir}/bootimg.cfg")
	"${BOOTLOADER_DIR}/mkbootimg" --kernel "${working_dir}/zImage" \
		--ramdisk "${working_dir}/initrd.img" --cmdline "${cmdline}" \
		-o "${BOOTLOADER_DIR}/boot${dev_instance}.img"

	OUTPUT_FILE="${BOOTLOADER_DIR}/boot${dev_instance}.img"

	sign_bootimg

	echo "flashimg${dev_instance}=$(basename "${OUTPUT_FILE}")" | tee -a "${L4T_INITRD_FLASH_DIR}/${FLASH_IMG_MAP}"

	popd

}

sign_bootimg()
{
	local uefi_keys_conf=""
	local uefi_keys_conf_dir=""
	local uefi_db_key=""
	local uefi_db_cert=""

	if [ -n "${UEFI_KEYS_CONF}" ]; then
		pushd "${LINUX_BASE_DIR}" > /dev/null 2>&1 || exit 1
		uefi_keys_conf=$(readlink -f "${UEFI_KEYS_CONF}");
		if [ ! -f "${uefi_keys_conf}" ]; then
			log_error "UEFI keys conf file ${uefi_keys_conf} not found"
			exit 1
		fi;
		popd > /dev/null 2>&1 || exit 1

		source "${uefi_keys_conf}"
		uefi_keys_conf_dir=$(dirname "${uefi_keys_conf}")
		uefi_db_key=$(readlink -f "${uefi_keys_conf_dir}/${UEFI_DB_1_KEY_FILE}")
		uefi_db_cert=$(readlink -f "${uefi_keys_conf_dir}/${UEFI_DB_1_CERT_FILE}")
		if [ -f "${uefi_db_key}" ] && [ -f "${uefi_db_cert}" ]; then
			if ! "${LINUX_BASE_DIR}"/l4t_uefi_sign_image.sh \
						--image "${OUTPUT_FILE}" \
						--key "${uefi_db_key}" \
						--cert "${uefi_db_cert}" \
						--mode "append"; then
				log_error "Sign image ${OUTPUT_FILE} failed."
				exit 2
			fi
		else
			log_error "UEFI keys file ${uefi_db_key} or ${uefi_db_cert} not found"
			exit 3
		fi
	fi
}

wait_for_booting()
{
	ext=""
	sd_emmc_dev=""
	internal_emmc_boot0=""
	internal_emmc_boot1=""
	maxcount=${timeout:-120}
	count=0
	device_instance=$1
	while true
	do
		if [ -n "${network}" ]; then
			while IFS=  read -r; do
				netpath=/sys/class/net/${REPLY}
				netserialnumber=$(get_udev_attribute "${netpath}" serial)
				netconfiguration=$(get_udev_attribute "${netpath}" configuration)
				if [[ "${netconfiguration}" =~ RNDIS\+L4T${device_instance}.* ]]; then
					serialnumber="${netserialnumber}"
					break
				fi
			done < <(ls /sys/class/net)
			if [ -n "${serialnumber}" ]; then
				break
			fi
		fi
		if [ -z "${network}" ]; then
			log_error "This configuration is no longer supported"
			exit 3
		fi
		echo "Waiting for target to boot-up..."
		sleep 1;
		count=$((count + 1))
		if [ "${count}" -ge "${maxcount}" ]; then
			log_error "Timeout"
			log_error "Device failed to boot to the initrd flash kernel. Please retrive the serial log during flashing to debug further."
			exit 4
		fi

	done
}

wait_for_ssh()
{

	printf "%s" "Waiting for device to expose ssh ..."
	count=0
	IP_SET=
	while ! ping_device
	do
		printf "..."
		count=$((count + 1))
		if [ "${count}" -ge "${maxcount}" ]; then
			log_error "Timeout"
			log_error "Device should have booted into initrd kernel now. However, the host cannot connect to its ssh server"
			if [ -z "$(cat "${sshcon}")" ]; then
				log_error "Network interface disappears after discovery."
				lsusb
			fi
			log_error "Command failed: ping6 -c 1 fe80::1%$(cat "${sshcon}"). Check your network settings (firewall, VPN,...) so that it can ping fe80::1%$(cat "${sshcon}")"
			ip a
			exit 5
		fi
		sleep 1
	done
}

flash_direct()
{
	local cmd=()

	if [ -n "${target_partname}" ]; then
		cmd+=("-k" "${target_partname}")
	fi
	EXTDEV_ON_HOST="${direct}" "${NFS_IMAGES_DIR}/${KERNEL_FLASH_SCRIPT}" --direct "${cmd[@]}"
}

function get_value_from_cfg_file() {
	# Usage:
	#       get_value_from_cfg_file \
	#       {__cfg_name} \
	#       {__cfg_node} \
	#       {__cfg_file} \
	#       {__ret_value}
	local __cfg_name="${1}";
	local __cfg_node="${2}";
	local __cfg_file="${3}";
	local __ret_value="${4}";
	local __node_val="";
	# Get node value
	__node_val="$(xmllint --xpath "/flash_cfg/${__cfg_name}/${__cfg_node}/text()" ${__cfg_file})";
	__node_val=$(echo ${__node_val} | sed -e 's/^[[:space:]]*//');

	eval "${__ret_value}=\"${__node_val}\"";
}

boot_initrd()
{
	local usb_instance=${1}
	local skipuid=${2}
	local dev_instance=${3}

	pushd "${TEMP_INITRD_FLASH_DIR}"
	local cmd
	if [ -n "${usb_instance}" ]; then
		local var=flashimg${dev_instance}
		cmd=$(sed -e "s/--cmd /--instance ${usb_instance} --cmd /" < "${TEMP_INITRD_FLASH_DIR}/flashcmd.txt")
		cmd="$(echo "${cmd}" | sed -e "s/kernel [a-zA-Z0-9._\-]*/kernel $(basename "${!var}")/")"
	fi
	if [ -n "${skipuid}" ] && [ -z "${initrd_only}" ] && [ -z "${reuse_package}" ]; then
		cmd="${cmd//--cmd / --skipuid --cmd }"
	fi
	echo "${cmd}"
	eval "${cmd}"

	popd
}

copy_bootloader()
{
	local temp_bootloader="${1}"
	local tid="${2}"
	local cmdline="${3}"

	mkdir -p "${temp_bootloader}"
	pushd "${BOOTLOADER_DIR}"
	cp tegrabct_v2 "${temp_bootloader}";
	cp tegradevflash_v2 "${temp_bootloader}";
	cp tegraflash_internal.py "${temp_bootloader}";
	cp tegrahost_v2 "${temp_bootloader}";
	cp tegraparser_v2 "${temp_bootloader}";
	cp tegrarcm_v2 "${temp_bootloader}";
	cp -r -t "${temp_bootloader}" ./tegraflash*.py ./tegrasign*.py ./pyfdt
	cp tegraopenssl "${temp_bootloader}";
	if [ "${tid}" = "0x19" ]; then
		cp sw_memcfg_overlay.pl "${temp_bootloader}";
	fi;


	# Parsing the command line of tegraflash.py, to get all files that tegraflash.py and
	# tegraflash_internal.py needs so copy them to the working directory.
	# Retrieve the actual tegraflash.py command from $cmdline
	cmdline=$(echo "${cmdline}" | grep "tegraflash.py")
	# Add rcmdump_blob.tar to cmdline if gen_read_ramcode is set
	if [ ${gen_read_ramcode} -eq 1 ]; then
		cmdline+=" rcmdump_blob.tar"
	fi

	cmdline=$(echo "${cmdline}" | sed -e s/\;/\ /g -e s/\"//g);
	read -r -a opts <<< "${cmdline}"
	optnum=${#opts[@]};
	for (( i=0; i < optnum; )); do
		opt="${opts[$i]}";
		opt=${opt//\,/\ }
		read -r -a files <<< "${opt}"
		filenum=${#files[@]};
		for (( j=0; j < filenum; )); do
			file="${files[$j]}";
			if [ -f "${file}" ]; then
				folder=$(dirname "${file}");
				if [ "${folder}" != "." ]; then
					mkdir -p "${temp_bootloader}/${folder}";
				fi;
				cp "${file}" "${temp_bootloader}/${folder}";
			fi;
			j=$((j+1));
		done;
		i=$((i+1));
	done;
	cp flashcmd.txt "${temp_bootloader}";
	awk -F= '{print $2}' "${L4T_INITRD_FLASH_DIR}/${FLASH_IMG_MAP}" | xargs cp -t "${temp_bootloader}"
	popd
}

package()
{
	local workdir="${1}"
	local cmdline="${2}"
	local tid="${3}"
	local temp_bootloader="${workdir}/bootloader"
	copy_bootloader "${temp_bootloader}" "${tid}" "${cmdline}"

	local temp_kernelflash="${workdir}/tools/kernel_flash"
	mkdir -p "${temp_kernelflash}"
	cp -a "${L4T_INITRD_FLASH_DIR}"/* "${temp_kernelflash}"
	cp "${LINUX_BASE_DIR}/${target_board}.conf" "${workdir}/"
	cp "${LINUX_BASE_DIR}/"*.common "${workdir}/"
	if [ -n "${network}" ]; then
		cp -a "${LINUX_BASE_DIR}/rootfs" "${workdir}/"
	fi
}

package_odm()
{
	local workdir="${1}"
	local cmdline="${2}"
	local tid="${3}"
	local userealfile="${4}"
	local temp_rcmboot="${workdir}/tools/kernel_flash/images/rcmboot/"
	mkdir -p "${temp_rcmboot}"
	copy_bootloader "${temp_rcmboot}" "${tid}" "${cmdline}" "${userealfile}"

	local temp_bootloader="${workdir}/tools/kernel_flash/images/"
	mkdir -p "${temp_bootloader}"
	cp -a "${L4T_INITRD_FLASH_DIR}"/images/internal "${temp_bootloader}"
	chmod -R 755 "${workdir}"
}

package_mass_storage()
{
	local workdir="${1}"

	local temp_kernelflash="${workdir}/tools/kernel_flash/images/"
	mkdir -p "${temp_kernelflash}"
	cp -a "${L4T_INITRD_FLASH_DIR}/initrdflashparam.txt" "${workdir}/tools/kernel_flash"
	cp -a "${L4T_INITRD_FLASH_DIR}/initrdflashimgmap.txt" "${workdir}/tools/kernel_flash"
	cp -a "${L4T_INITRD_FLASH_DIR}"/images/l4t_flash_from_kernel.sh "${temp_kernelflash}"
	cp -a "${L4T_INITRD_FLASH_DIR}"/images/simg2img "${temp_kernelflash}"
	cp -a "${L4T_INITRD_FLASH_DIR}"/images/nv_fuse_read.sh "${temp_kernelflash}"
	cp -a "${L4T_INITRD_FLASH_DIR}"/images/internal "${temp_kernelflash}"
	if [ -f "${temp_kernelflash}/internal/flash.idx" ]; then
		mv "${temp_kernelflash}/internal/flash.idx" "${temp_kernelflash}/internal/flash-upi.idx"
	fi
	cp -a "${L4T_INITRD_FLASH_DIR}"/images/external "${temp_kernelflash}"
	chmod -R 755 "${workdir}"
}

external_device=""
append=""
config_file=""
external_size=""
external_only=""
pv_crt=""
pv_enc=""
no_flash="0"
sparse_mode="0"
sshcon="$(mktemp)"
usb_instance=""
flash_only=0
OPTIONS=""
KEY_FILE=""
erase_all=""
device_instance="0"
target_partname=""
target_partfile=""
max_massflash=""
massflash_mode=""
SBK_KEY=""
keep=""
reuse=""
network="usb0"
timeout=""
skipuid=""
initrd_only=""
reuse_package=""
direct=""
ENC_RFS_KEY=""
UEFI_KEYS_CONF=""
UEFI_ENC=""
hsm_enable=0

source "${L4T_INITRD_FLASH_DIR}"/l4t_kernel_flash_vars.func
source "${L4T_INITRD_FLASH_DIR}"/l4t_initrd_flash.func
source "${L4T_INITRD_FLASH_DIR}"/l4t_network_flash.func

parse_param "$@"

get_max_flash

if [ "${flash_only}" = "0" ]; then
	if [ -z "${initrd_only}" ] && [ -z "${reuse_package}" ]; then
		cat <<EOF
************************************
*                                  *
*  Step ${initrd_flash_step}: Generate flash packages *
*                                  *
************************************
EOF
		generate_flash_package
		((initrd_flash_step+=1))
	fi
cat <<EOF
******************************************
*                                        *
*  Step ${initrd_flash_step}: Generate rcm boot commandline *
*                                        *
******************************************
EOF
	generate_rcm_bootcmd

	rm -f "${L4T_INITRD_FLASH_DIR}/${FLASH_IMG_MAP}"

	for i in $(seq 0 "$((max_massflash - 1))")
	do
		generate_flash_initrd "${i}"
	done

	((initrd_flash_step+=1))
	if [ "${massflash_mode}" = "1" ]; then
		rm -rf "${LINUX_BASE_DIR}/mfi_${target_board}/"
		mkdir -p "${LINUX_BASE_DIR}/mfi_${target_board}/"
		package "${LINUX_BASE_DIR}/mfi_${target_board}/" "$(cat "${BOOTLOADER_DIR}/flashcmd.txt")" "${CHIPID}"
		tar -zcvf "${LINUX_BASE_DIR}/mfi_${target_board}.tar.gz" -C "${LINUX_BASE_DIR}" "./mfi_${target_board}"
		echo "Massflash package is generated at ${LINUX_BASE_DIR}/mfi_${target_board}.tar.gz"
	fi

	if [ -n "${ODM_IMAGE_GEN}" ]; then
		readonly ODM_DIR=lbc_odm
		temp_dir="$(mktemp -d)"
		package_odm "${temp_dir}/" "$(cat "${BOOTLOADER_DIR}/flashcmd.txt")" "${CHIPID}" "1"
		tar -zcvf "${LINUX_BASE_DIR}/${ODM_DIR}.tar.gz" -C "${temp_dir}" "."
		echo "ODM package is generated at ${LINUX_BASE_DIR}/${ODM_DIR}.tar.gz"
		rm -rf "${temp_dir}"
	fi

	if [ -n "${mass_storage_only}" ]; then
		temp_dir="$(mktemp -d)"
		readonly UPI_DIR=upi_oem
		package_mass_storage "${temp_dir}"
		tar -zcvf "${LINUX_BASE_DIR}/${UPI_DIR}.tar.gz" -C "${temp_dir}" "."
		rm -rf "${temp_dir}"
		echo "Mass storage package is generated at ${LINUX_BASE_DIR}/${UPI_DIR}.tar.gz"
	fi

fi




if [ "${no_flash}" = "0" ]; then

	if [ -n "${direct}" ]; then
cat <<EOF
*************************************************************
*                                                           *
*  Step ${initrd_flash_step}: Start the host connected device flashing process *
*                                                           *
*************************************************************
EOF
		flash_direct
		echo "Success"
		exit
	fi


cat <<EOF
**********************************************
*                                            *
*  Step ${initrd_flash_step}: Build the flashing environment    *
*                                            *
**********************************************
EOF

	source "${L4T_INITRD_FLASH_DIR}/${FLASH_IMG_MAP}"

	build_working_dir "${device_instance}"
	((initrd_flash_step+=1))

cat <<EOF
****************************************************
*                                                  *
*  Step ${initrd_flash_step}: Boot the device with flash initrd image *
*                                                  *
****************************************************
EOF
	((initrd_flash_step+=1))

	boot_initrd "${usb_instance}" "${skipuid}" "${device_instance}"


cat <<EOF
***************************************
*                                     *
*  Step ${initrd_flash_step}: Start the flashing process *
*                                     *
***************************************
EOF

	if [[ "${network}" == eth0* ]]; then
		IFS=':' read -r -a arr <<< "${network}"
		flash_through_ssh "${arr[1]%%/*}"
	fi

	wait_for_booting "${device_instance}"

	wait_for_ssh

	if [ "${network}" = "usb0" ]; then
		# For this mode, we need to wait for the network interface
		# to be set up
		flash_through_ssh "fc00:1:1:${device_instance}::2"
	fi

	if [ -n "${initrd_only}" ]; then
		# This output will be parsed by l4t_backup_restore.sh
		echo "Device has booted into initrd. You can ssh to the target by the command:"
		echo "$ ssh root@fe80::1%$(cat "${sshcon}")"
		exit
	fi

fi

echo "Success"
