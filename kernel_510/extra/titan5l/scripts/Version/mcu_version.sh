#!/bin/sh
version=`spidev_test -D /dev/spidev1.0 -H -s10000000 -v -p abcd`
mcu_version
