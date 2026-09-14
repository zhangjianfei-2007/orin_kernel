#!/bin/bash
#
# for cameras have the following connection 
# (e.g. BL-IMX390-GMSL2)
# max9295 <=GPIO=> IMX390CQV
# MFP0(2) <--> FSYNC(L6)
# max9295 <=GMSL2=> max96712
# A/B/C/D <--> MFP4

case $# in
	1) ID=$1 ;;
	*)echo "Usage: $0 id" 1>&2 ; exit 1 ;;
esac

if [ $ID -lt 0 ] || [ $ID -gt 11 ]
then
	echo "$0: id should be 0 to 11" 1>&2 ; exit 1 ;
fi

#camera write des $ID 0x6 0xff
#sleep 0.5
camera write 9295 $ID 0x2c0 0x84
camera write 9295 $ID 0x2be 0x4
#camera write des $ID 0x6 0xf0
camera write des $ID 0x4a0 0x8
camera write des $ID 0x4af 0x9f
camera write des $ID 0x30c 0x83
camera write des $ID 0x344 0x24
camera write des $ID 0x37a 0x24
camera write des $ID 0x3b1 0x24

