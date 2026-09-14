#!/usr/bin/env sh
# enable cam fsync via spi

FSYNC_ENABLED=11
N=0
N_MAX=3
N_SEC=1

FPGA_READ_CMD="spidev_test -D /dev/spidev2.0 -v -s 10000000 -p SPIR\\x01\\x0a\\x11"
FPGA_WRITE_CMD="spidev_test -v -D /dev/spidev2.0  -s 10000000 -p SPIW\\x01\\x0a\\x11"

#todo: check fpga cmd available

#todo: check FPGA FW version

#retrive FSYNC enabled/disabled status
FSYNC_STATUS=`$FPGA_READ_CMD | grep '^RX ' | cut -d ' ' -f 9`

while [ $FSYNC_STATUS -ne $FSYNC_ENABLED ]
do
	echo "write and verify fpga to enable fsync..."
	$FPGA_WRITE_CMD > /dev/null
	sleep $N_SEC

	FSYNC_STATUS=`$FPGA_READ_CMD | grep '^RX ' | cut -d ' ' -f 9`
	if [ $FSYNC_STATUS -eq $FSYNC_ENABLED ]
	then
		echo "done."
		break
	fi

	N=${N+1}
	echo "$N time(s) tried..."

	if [ $N -ge $N_MAX ]
	then
		echo "failed."
		break

	fi
done

exit 0
