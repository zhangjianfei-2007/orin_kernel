#!/bin/bash

cmd=" -D /dev/spidev2.0 -v -s 10000000 -p SPIR\\x05\\x00XXXXX"
kernel_ori=`cat /proc/version`
kernel1=${kernel_ori##*PREEMPT}

kernel=`echo ${kernel1} | cut -c 5-28 `
echo kernel:${kernel}

DTB_ori=`dpkg -s hyzx-linux-kernel-dtbs| grep Version`
DTB=${DTB_ori##*: }
echo DTB   FW:${DTB}



FW_ori=`spidev_test ${cmd} `
FW1=${FW_ori%__*}
FW2=${FW1##*|}
FW3=${FW2%%__*}
FW=`echo $FW3 |cut -c 19-32`

echo FPGA FW:${FW}

MCU=`mcu_test|grep 版本`
MCU1=$(echo $MCU| awk -F'[： ]+' '{printf $2}')
MCU2=$(echo $MCU| awk -F'[： ]+' '{printf $4}')
MCU3=$(echo $MCU| awk -F'[： ]+' '{printf $6}')
echo MCU Data version :${MCU1}
echo MCU  HW  version:${MCU2}
echo MCU  SW  version:${MCU3}
