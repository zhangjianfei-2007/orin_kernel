#!/bin/sh

/etc/can_setup.sh

modprobe switch

# pull down mcu wdg pin
echo 350 > /sys/class/gpio/export
sleep 0.2
echo 0 > /sys/class/gpio/PA.02/value

#enable 4g LTE
LTE=`ls /dev |grep ttyUSB2`
if [ -z "$LTE" ];
then
echo 453 > /sys/class/gpio/export
sleep 0.2
echo out > /sys/class/gpio/PQ.05/direction

echo 0 > /sys/class/gpio/PQ.05/value
echo 1 > /sys/class/gpio/PQ.05/value
sleep 1
echo 0 > /sys/class/gpio/PQ.05/value
fi

#enable gnss
echo 344 > /sys/class/gpio/export
sleep 0.2
echo out > /sys/class/gpio/PEE.05/direction

echo 0 > /sys/class/gpio/PEE.05/value

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

# disable usb1 for 4G LTE networking
IF_NAME2=usb1
N_TRY2=10
#ifconfig usb1 down

while [ $N_TRY2 -gt 0 ]
do
	IF_TARGET2=$(ifconfig -s | sed -n "/^$IF_NAME2/s/\($IF_NAME2\).*/\1/p")
	if [ -z $IF_TARGET2 ]
	then
		sleep 1
	else
		ifconfig $IF_TARGET2 down
		echo network interface $IF_TARGET2 is disabled.
		break
	fi
	N_TRY2=$((N_TRY2-1))
	echo try to disable network interface $IF_NAME2.
done







