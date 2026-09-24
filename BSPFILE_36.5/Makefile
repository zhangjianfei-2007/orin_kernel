-include Rules.make

linux_menuconfig:
	mkdir -p $(KERNEL_OUT)
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) tegra_$(PLATFORM)_defconfig
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) menuconfig
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) savedefconfig
	-diff -uprN "$(KERNEL_IN)/arch/arm64/configs/tegra_$(PLATFORM)_defconfig" "$(KERNEL_OUT)/defconfig"
	@echo "if you would like to save this defconfig to configs folder, please do it manually:"
	@echo "mv $(KERNEL_OUT)/defconfig $(KERNEL_IN)/arch/arm64/configs/tegra_$(PLATFORM)_defconfig"

linux_all:linux dtbs modules
	mkdir -p $(KERNEL_OUT)
	#cp -rf $(KERNEL_OUT)/arch/arm64/boot/dts/nvidia/* $(ROOTFS)/../kernel/dtb/
	#cp -rf $(KERNEL_OUT)/arch/arm64/boot/Image $(ROOTFS)/../kernel/
	cd $(MODULES_OUT) && tar -cjf kernel_supplements.tbz2 *
	#cp -f $(MODULES_OUT)/kernel_supplements.tbz2 $(ROOTFS)/../kernel/
	@echo -ne "modules tarball: $(MODULES_OUT)/kernel_supplements.tbz2\n\n"

linux:
	mkdir -p $(KERNEL_OUT)
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) tegra_$(PLATFORM)_defconfig
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) Image -j8
	@echo -ne "compiled kernel: $(KERNEL_OUT)/arch/arm64/boot/Image\n\n"

dtbs:
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) EXTRA_CPP_FLAGS=-D$(CAMERA_TYPE) V=1 dtbs
	@echo -ne "compiled dtbs: $(KERNEL_OUT)/arch/arm64/boot/dts/nvidia/\n\n"
	cp -f $(KERNEL_OUT)/arch/arm64/boot/dts/nvidia/*.dtb $(ROOTFS)/../kernel/dtb/
	cp -f $(KERNEL_OUT)/arch/arm64/boot/dts/nvidia/*.dtbo $(ROOTFS)/../kernel/dtb/

modules:
	rm -rf ${MODULES_OUT}
	mkdir -p $(MODULES_OUT)
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) tegra_$(PLATFORM)_defconfig
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) modules -j8	
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) modules_install INSTALL_MOD_PATH=$(MODULES_OUT)
	@echo -ne "installed modules: $(MODULES_OUT)\n\n"

linux_clean:
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) mrproper
	#make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) clean

deb_kernel:linux get_ver
	mkdir -p $(DEB_OUT)
	cp -rf $(DEB_SRC)/hyzx-linux-kernel $(DEB_OUT)
	mkdir -p $(DEB_OUT)/hyzx-linux-kernel/boot
	cp $(KERNEL_OUT)/arch/arm64/boot/Image "$(DEB_OUT)/hyzx-linux-kernel/boot/Image.$(KERNEL_VER)"
	echo "Version: $(KERNEL_PKG_VER)" >> $(DEB_OUT)/hyzx-linux-kernel/DEBIAN/control
	fakeroot dpkg-deb --build $(DEB_OUT)/hyzx-linux-kernel $(DEB_PKG_DIR)

deb_modules: modules
	mkdir -p $(DEB_OUT)
	cp -rf $(DEB_SRC)/hyzx-linux-kernel-modules $(DEB_OUT)
	rm -rf $(DEB_OUT)/hyzx-linux-kernel-modules/lib
	mkdir -p $(DEB_OUT)/hyzx-linux-kernel-modules/lib
	cp -rf $(MODULES_OUT)/lib/modules $(DEB_OUT)/hyzx-linux-kernel-modules/lib
	#mkdir -p $(DEB_OUT)/hyzx-linux-kernel-modules/lib/modules/$(KERNEL_VER)/extra/opensrc-disp/
	#cp -f $(OPENSRC_DISP_DIR)/*.ko $(DEB_OUT)/hyzx-linux-kernel-modules/lib/modules/$(KERNEL_VER)/extra/opensrc-disp/
	echo "Version: $(KERNEL_PKG_VER)" >> $(DEB_OUT)/hyzx-linux-kernel-modules/DEBIAN/control
	#must fix: set dependencies as simple as possible, otherwise, apt install may cause "unmet dependencies" error.
	echo "Depends: hyzx-linux-kernel (>= $(KERNEL_VER))" >> $(DEB_OUT)/hyzx-linux-kernel-modules/DEBIAN/control
	echo "depmod -a $(KERNEL_VER)" >> $(DEB_OUT)/hyzx-linux-kernel-modules/DEBIAN/postinst
	echo "exit 0" >> $(DEB_OUT)/hyzx-linux-kernel-modules/DEBIAN/postinst
	fakeroot dpkg-deb --build $(DEB_OUT)/hyzx-linux-kernel-modules $(DEB_PKG_DIR)

deb_nvidia-gpu-soc: nvidia-gpu-soc
	cp -rf $(DEB_SRC)/hyzx-linux-nvidia-gpu-soc $(DEB_OUT)
	rm -rf $(DEB_OUT)/hyzx-linux-nvidia-gpu-soc/lib/modules/$(KERNEL_VER)/extra/opensrc-disp/
	mkdir -p $(DEB_OUT)/hyzx-linux-nvidia-gpu-soc/lib/modules/$(KERNEL_VER)/extra/opensrc-disp/
	cp -f $(OPENSRC_DISP_DIR)/*.ko $(DEB_OUT)/hyzx-linux-nvidia-gpu-soc/lib/modules/$(KERNEL_VER)/extra/opensrc-disp/
	echo "Version: $(KERNEL_PKG_VER)" >> $(DEB_OUT)/hyzx-linux-nvidia-gpu-soc/DEBIAN/control
	#must fix: set dependencies as simple as possible, otherwise, apt install may cause "unmet dependencies" error.
	echo "Depends: hyzx-linux-kernel (>= $(KERNEL_VER))" >> $(DEB_OUT)/hyzx-linux-nvidia-gpu-soc/DEBIAN/control
	echo "depmod -a $(KERNEL_VER)" >> $(DEB_OUT)/hyzx-linux-nvidia-gpu-soc/DEBIAN/postinst
	echo "exit 0" >> $(DEB_OUT)/hyzx-linux-nvidia-gpu-soc/DEBIAN/postinst
	fakeroot dpkg-deb --build $(DEB_OUT)/hyzx-linux-nvidia-gpu-soc $(DEB_PKG_DIR)

deb_extlinux:
	mkdir -p $(DEB_OUT)
	cp -rf $(DEB_SRC)/hyzx-linux-extlinux $(DEB_OUT)
	mkdir -p $(DEB_OUT)/hyzx-linux-extlinux/boot/extlinux
	cp -f $(BSP_ROOT)/src/bootloader/extlinux/extlinux.conf.$(PLATFORM) $(DEB_OUT)/hyzx-linux-extlinux/boot/extlinux/extlinux.conf
	echo "Version: $(KERNEL_PKG_VER)" >> $(DEB_OUT)/hyzx-linux-extlinux/DEBIAN/control
	fakeroot dpkg-deb --build $(DEB_OUT)/hyzx-linux-extlinux $(DEB_PKG_DIR)

deb_dtbs: dtbs
	mkdir -p $(DEB_OUT)
	cp -rf $(DEB_SRC)/hyzx-linux-kernel-dtbs $(DEB_OUT)
	mkdir -p $(DEB_OUT)/hyzx-linux-kernel-dtbs/boot/dtb
	cp $(SRC_DTB) $(DEB_OUT)/hyzx-linux-kernel-dtbs$(TARGET_DTB)
	#cp $(SRC_DTB) $(DEB_OUT)/hyzx-linux-kernel-dtbs$(TARGET_CUSTOM_DTB)
	cd $(DEB_OUT)/hyzx-linux-kernel-dtbs/boot/dtb && md5sum *.dtb > md5sum.txt
	echo -e "$(TARGET_DTB)\t$(DTB_ALIAS_VER)" > $(DEB_OUT)/hyzx-linux-kernel-dtbs/boot/dtb/alias.txt
	echo "Version: $(DTB_PKG_VER)" >> $(DEB_OUT)/hyzx-linux-kernel-dtbs/DEBIAN/control
	echo "Depends: hyzx-linux-kernel (>= $(KERNEL_VER))" >> $(DEB_OUT)/hyzx-linux-kernel-dtbs/DEBIAN/control
	fakeroot dpkg-deb --build $(DEB_OUT)/hyzx-linux-kernel-dtbs $(DEB_PKG_DIR)

get_ver:
	@echo kernel version is $(KERNEL_VER) 
	@echo kernel git commit is $(KERNEL_GIT_VER)
	@echo kernel build date is $(KERNEL_BUILD_DATE)
	@echo kernel package version is $(KERNEL_PKG_VER) 
	@echo dtb package version is $(DTB_PKG_VER) 


deb_admin_extra:
ifeq (${PRODUCT},titan5)
	mkdir -p $(DEB_OUT)
	cp -rf $(DEB_SRC)/hyzx-admin-tools-extra $(DEB_OUT)
	mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/etc/udev/rules.d
	cp $(RULE_DIR)/*.rules $(DEB_OUT)/hyzx-admin-tools-extra/etc/udev/rules.d/
	#mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/etc/systemd/system/
	#cp $(SERVICE_DIR)/*.service $(DEB_OUT)/hyzx-admin-tools-extra/etc/systemd/system/
	cp $(RC_DIR)/* $(DEB_OUT)/hyzx-admin-tools-extra/etc/
	mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/opt/hyzx/titan5/
	cp -r $(SCR_DIR)/* $(DEB_OUT)/hyzx-admin-tools-extra/opt/hyzx/titan5/
	mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/usr/sbin/
	#cp $(BIN_DIR)/* $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(BIN_DIR)/hwclock $(DEB_OUT)/hyzx-admin-tools-extra/usr/sbin/
	$(TOOL_DIR)/cross_build.sh $(BSP_ROOT) $(TOOL_DIR)
	cp $(TOOL_DIR)/camera_FSYNC_control $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR)/mcu_test $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR)/dac_demo $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR)/gpi_demo $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR)/gpo_demo $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR)/date_update $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	echo "Version: $(KERNEL_VER)-$(KERNEL_GIT_VER)-$(PRODUCT)" >> $(DEB_OUT)/hyzx-admin-tools-extra/DEBIAN/control
	fakeroot dpkg-deb --build $(DEB_OUT)/hyzx-admin-tools-extra $(DEB_PKG_DIR)
endif
ifeq (${PRODUCT},titan5l)
	mkdir -p $(DEB_OUT)
	cp -rf $(DEB_SRC)/hyzx-admin-tools-extra $(DEB_OUT)
	mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/etc/udev/rules.d
	cp $(RULE_DIR)/*.rules $(DEB_OUT)/hyzx-admin-tools-extra/etc/udev/rules.d/
	#mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/etc/systemd/system/
	#cp $(SERVICE_DIR)/*.service $(DEB_OUT)/hyzx-admin-tools-extra/etc/systemd/system/
	cp $(RC_DIR_029)/* $(DEB_OUT)/hyzx-admin-tools-extra/etc/
	mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/opt/hyzx/titan5l/
	cp -r $(SCR_DIR_029)/* $(DEB_OUT)/hyzx-admin-tools-extra/opt/hyzx/titan5l/
	cp -r $(SCR_DIR)/Camera $(DEB_OUT)/hyzx-admin-tools-extra/opt/hyzx/titan5l/
	mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	mkdir -p $(DEB_OUT)/hyzx-admin-tools-extra/usr/sbin/
	#cp $(BIN_DIR)/* $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(BIN_DIR)/hwclock $(DEB_OUT)/hyzx-admin-tools-extra/usr/sbin/
	$(TOOL_DIR_029)/cross_build.sh $(BSP_ROOT) $(TOOL_DIR_029)
	$(TOOL_DIR)/cross_build.sh $(BSP_ROOT) $(TOOL_DIR)
	cp $(TOOL_DIR)/gpi_demo $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR)/gpo_demo $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR)/camera_FSYNC_control $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR_029)/dac_demo $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR_029)/mcu_version $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR_029)/mcu_adc $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	cp $(TOOL_DIR_029)/date_update $(DEB_OUT)/hyzx-admin-tools-extra/usr/local/bin/
	echo "Version: $(KERNEL_VER)-$(KERNEL_GIT_VER)-$(PRODUCT)" >> $(DEB_OUT)/hyzx-admin-tools-extra/DEBIAN/control
	fakeroot dpkg-deb --build $(DEB_OUT)/hyzx-admin-tools-extra $(DEB_PKG_DIR)
endif

deb_admin_extra_clean:
	make -C $(TOOL_DIR) clean
	rm -rf $(DEB_OUT)/hyzx-admin-tools-extra*

deb_clean: deb_admin_extra_clean
	rm -rf $(DEB_OUT)/*

release: deb_kernel deb_dtbs deb_modules deb_nvidia-gpu-soc deb_extlinux deb_admin_extra
	@echo dtb packages located in:
	@ls -l $(DEB_OUT)/*.deb

nvidia-gpu-soc:
	cd $(BSP_ROOT)/src/kernel/NVIDIA-kernel-module-source-TempVersion && make modules TARGET_ARCH=aarch64 CC=$(CC) LD=$(LD) AR=$(AR) CXX=$(CXX) OBJCOPY=$(OBJCOPY) SYSSRC=$(KERNEL_IN) SYSOUT=$(KERNEL_OUT) OBJDUMP=$(OBJDUMP) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) NV_VERBOSE=1

modules_camera:
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) tegra_$(PLATFORM)_defconfig
	make -C $(KERNEL_IN) O=$(KERNEL_OUT) M=$(BSP_ROOT)/src/kernel/nvidia/drivers/media/i2c ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) modules

modules_camera_clean:
	cd $(BSP_ROOT)/src/kernel/nvidia/drivers/media/i2c && rm -vrf *.o *.mod *.mod.* *.ko *.symvers modules.order .*.cmd *~ .tmp_versions .*.o.d

