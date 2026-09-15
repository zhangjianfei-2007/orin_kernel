# TITAN4/5 BSP

> This BSP repo is based on JetPack 5.0.2 (L4T r35.1). for more documents and details, refer to former `titan-bsp` repo which is based on JetPack 5.0 DP.

## supported platforms

1. p2888-0001-p2822-0000 (jetson-agx-xavier-devkit with HY017 camera board)

2. p3701-0000-p3737-0000 (jetson-agx-orin-devkit with with HY017C camera board)

3. p3701-0000-hy026-0000 (TITAN5 hy027/hy027b with two DEVKIT orin modules)

4. p3701-0004-hy026-0000 (TITAN5 hy027/hy027b with two 32GB orin modules)

5. p2888-0001-hy009-0000 (TITAN4B/C hy009 with two xavier modules)

6. p3701-0004-hy026-0001 (TITAN5P hy026 with two 32GB orin modules)

7. p3701-0004-hy029-0000 (TITAN5L hy029 with single 32GB orin module)

board `hy026-0000` is compatible with the following TITAN5 hardware configurations:

- HY027: DP/MAX9296/MPC5744P/88E6352
- HY027B: HDMI/MAX9296/MPC5744P/88E6352

board `hy026-0001` is compatible with the following TITAN5P hardware configurations:

- HY026: HDMI/MAX96712(CPHY)/BCM53162/ARQ113C/S32K344/PGL22G

board `hy029-0000` is compatible with the following TITAN5L hardware configurations:

- HY029: HDMI/MAX96712(DPHY)/88E6352/ARQ113C/S32K344 (without FPGA)

## supported softwares

1. L4T R35.1

2. Jetpack 5.0.2

## repo status

available for branch master:

- serdes & isp driver for hy027b
- gpu driver for hy027b
- spi master & slave driver for hy027b
- can i/f & can fd api for hy027b
- ethernet driver for hy027b
- T1 phy driver for hy027b
- 4G LTE module for hy027b
- micro USB-B as host for hy027b
- 3 uart ports for hy027b
- dce firmware for hy027b (only binary, source code is not available.)
- uefi for hy027b (only binary, source code is not included in this repo.)
- pcie rp/ep for hy027b
- sgtl5000 codec driver & audio i/f for hy027b
- spe firmware

also:

- full support for platform no. 1/2.
- limited support for platform no. 6/7.
- checkout branch `titan4-testing` for platform no. 5.

## build

### build bsp

change dir to this repo.

```
git checkout master
make release SOC=<supported SOCs> PLATFORM=<supported platforms>
```

supported SOCs are:

- tegra194
- tegra234

supported platforms have been listed.

### build apps

change dir to application repo. 

```
make release BSP_ROOT=<absolute path of this repo>
```

e.g. build CAN FD demo:

```
cd ~/Rayray-Trunk/3rdParty/demo/canfd
make release BSP_ROOT=/opt/s/jet-pack-5.0.2
```

package is located in `release/hyzx-canfd-demo_5.0.2-l4t-r35.1-VERSION-GITID_arm64.deb`

`VERSION-GITID` varies when app upgrade.

## documents and bug tracking

see [doc](doc/README.md).

bug tracking number:

- 0XXX: for hy027 / hy027b / general bugs
- 1XXX: for hy029
- 2XXX: for hy009

## bootloader

refer to [uefi repositories](https://gitee.com/xavier-underlying-development/edk2-nvidia/tree/hy027b/).

## l4t-rt

see [Jetson Sensor Processing Engine (SPE) Developer Guide](https://docs.nvidia.com/jetson/archives/r35.1/spe/index.html).

source code: `src/l4t-rt/`

## preliminary releases

purpose: 

1. handover to test dept. and manufacture dept. for preliminary overall testing.

2. for mcu / fpga fw joint testing.

release media: 

1. usb disk

2. net disk (smb://192.168.5.2/1软件文档/BSP/${TARGET}/5.0.2/ or [access from public domain](http://120.202.28.90:5000/))

which ${TARGET} is one of `TITAN5`, `TITAN4`, `ORIN` and etc.

see [release note](doc/release-note/README.md).

## manufacturing tools

see [MFG-Toos](https://gitee.com/hardware_group/mfg-tools/tree/master/titan5) repo.

