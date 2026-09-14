#!/bin/sh

modprobe mttcan
modprobe can-raw

ip link set can0 up type can bitrate 500000 restart-ms 5000 
ip link set can1 up type can bitrate 500000 restart-ms 5000
ip link set can2 up type can bitrate 500000 restart-ms 5000

