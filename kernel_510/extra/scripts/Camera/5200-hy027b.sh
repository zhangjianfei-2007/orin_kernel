#!/bin/bash
# usage: ./5200-hy027b.sh [0-7]
# 0-7 is camera ID.
# camera with even ID use DES's MFP6 as fsync input.
# camera with odd ID use DES's MFP4 as fsync input.
#
# beware: driver should NOT overwrite this setting!
#
# for cameras have the following connection 
# (e.g. SENSING IMX390-GW5200, AR0233-GW5200)
#
# max9295(max96717) <=GPIO=> GW5200(5300)
# MFP7 or MFP8 <--> FRSYNC
#
# max9295 link A <=GMSL2=> max9296
# link A <--tx id 7--> MFP6
# max9295 link B  <=GMSL2=> max9296
# link B <--tx id 4--> MFP4

case $# in
	1) ID=$1 ;;
	*)echo "Usage: $0 id" 1>&2 ; exit 1 ;;
esac

if [ $ID -lt 0 ] || [ $ID -gt 7 ]
then
	echo "$0: id should be 0 to 7" 1>&2 ; exit 1 ;
fi

if [ $((ID%2)) -eq 0 ]
then
# link A: $ID is even
camera write 9295 $ID 0x2d3 0x84
camera write 9295 $ID 0x2d5 0x7
camera write 9295 $ID 0x2d6 0x84
camera write 9295 $ID 0x2d8 0x7
#disabled UART TX. MFP6 as GPIO
camera write des $ID 0x3 0x40
camera write des $ID 0x2c2 0x83
camera write des $ID 0x2c3 0xa7
else
# link B: $ID is odd
camera write 9295 $ID 0x2d3 0x84
camera write 9295 $ID 0x2d5 0x4
camera write 9295 $ID 0x2d6 0x84
camera write 9295 $ID 0x2d8 0x4
#disable ERRB output. MFP4 as GPIO
camera write des $ID 0x5 0x0
camera write des $ID 0x2bc 0x83
# CANNOT set GPIO_TX_ID for MFP4(GPIO4)
# maybe it's fixed to 4.
camera write des $ID 0x2bd 0xa4
fi
