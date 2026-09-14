#!/bin/sh

/etc/can_setup.sh

modprobe switch

# pull down mcu wdg pin
echo 350 > /sys/class/gpio/export
sleep 0.2
echo 0 > /sys/class/gpio/PA.02/value

# fix SPI2_MOSI pin
busybox devmem 0x0c302028 32 0x440

# disable usb0 for 4G LTE networking
IF_NAME=usb0
N_TRY=10
#ifconfig usb0 down

while [ $N_TRY -gt 0 ]
do
	IF_TARGET=$(ifconfig -s | sed -n "/^$IF_NAME/s/\($IF_NAME\).*/\1/p")
	if [ -z $IF_TARGET ]
	then
		sleep 1
	else
		ifconfig $IF_TARGET down
		echo network interface $IF_TARGET is disabled.
		break
	fi
	N_TRY=$((N_TRY-1))
	echo try to disable network interface $IF_NAME.
done
