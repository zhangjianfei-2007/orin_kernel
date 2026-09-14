#!/bin/bash
fpga=`ls /dev | grep mtd1`
if [ -z "$fpga" ];
then 
	echo "mtd1: not found.Unable to upgrade FPGA"
	exit 1
fi
echo 490 > /sys/class/gpio/export 2>/dev/null 
sleep 0.1
echo out > /sys/class/gpio/PAC.04/direction
echo 1 > /sys/class/gpio/PAC.04/value
cat /sys/class/gpio/PAC.04/value
sleep 1
echo 0 > /sys/class/gpio/PAC.04/value
cat /sys/class/gpio/PAC.04/value
echo -e "\n"

echo 352 > /sys/class/gpio/export 2>/dev/null
sleep 0.1
echo out > /sys/class/gpio/PA.04/direction
echo 1 > /sys/class/gpio/PA.04/value

cmd=" -D /dev/spidev2.0 -v -s 1000000 -p SPIR\\x05\\x00XXXXX"
FW_ori=`spidev_test ${cmd} `
FW1=${FW_ori%__*}
FW2=${FW1##*|}
FW3=${FW2%%__*}
FW=`echo $FW3 |cut -c 19-32`
FW4=`echo $FW3 |cut -c 25-32`
FW5=`echo $FW4|awk -v OFS='_' '{print $1,$2,$3}'`
upgrad_file_name=$1

FW_MAJOR=$(echo $FW3 | cut -d ' ' -f 9)
FW_MINOR=$(echo $FW3 | cut -d ' ' -f 10)
FW_REVISE=$(printf "%d" 0x`echo $FW3 | cut -d ' ' -f 11`)

MAJOR1=$(echo $upgrad_file_name | cut -d '_' -f 2)

if [ ! -f "$upgrad_file_name" ];
then
        echo "please input correct upgrad_file"
        exit 1
fi

if [ $MAJOR1 = "v02" ]
then
	MAJOR_MINOR=$(echo "$upgrad_file_name" | sed -n 's/^.*_\{0,1\}v\(02_0[1-4]\)_.\+\.bin$/\1/p')
	if [ $MAJOR_MINOR = "02_01" ] || [ $MAJOR_MINOR = "02_03" ]
	then
		echo "$upgrad_file_name is for XA7A15T"
	elif [ $MAJOR_MINOR = "02_02" ] || [ $MAJOR_MINOR = "02_04" ] 
	then
		echo "$upgrad_file_name is for XC7A50T"
	else
		echo "please input correct upgrad_file, must be *v02_0[1-4]*.bin"
		exit 1
	fi
fi

if [ $MAJOR1 = "v03" ]
then
	MAJOR_MINOR=$(echo "$upgrad_file_name" | sed -n 's/^.*_\{0,1\}v\(03_0[1-4]\)_.\+\.bin$/\1/p')
	if [ $MAJOR_MINOR = "03_01" ] || [ $MAJOR_MINOR = "03_03" ]
	then
		echo "$upgrad_file_name is for XA7A15T"
	elif [ $MAJOR_MINOR = "03_02" ] || [ $MAJOR_MINOR = "03_04" ] 
	then
		echo "$upgrad_file_name is for XC7A50T"
	else
		echo "please input correct upgrad_file, must be *v03_0[1-4]*.bin"
		exit 1
	fi
fi
if [ $FW_MAJOR != 0 ]&&[ $FW_MINOR != 0 ]&&[ $FW_REVISE != 0 ];
then
	MAJOR=$(echo $MAJOR_MINOR | cut -d '_' -f 1)
	MINOR=$(echo $MAJOR_MINOR | cut -d '_' -f 2)
	MINIMUM=$(printf "%d" 0x`echo "$upgrad_file_name"|cut -d '_' -f 4`)

	if ([ $MAJOR = "03" ] && [ $FW_MAJOR = "02" ]) || ([ $MAJOR = "02" ] && [ $FW_MAJOR = "03" ])
	then
	 	echo "rename" >/dev/null
	else
		if [ $MAJOR != $FW_MAJOR ]
		then
			echo "current fw version is $FW. it may incompatible with $upgrad_file_name. please check compatibility."
			exit 1
		fi
	fi
	

	
	if [ $FW_MINOR = "01" ] || [ $FW_MINOR = "03" ]
	then
		if [ $MINOR != "01" ] && [ $MINOR != "03" ]
		then
			echo "current fw version is $FW. it may incompatible with $upgrad_file_name. please check compatibility."
			exit 1
		fi
	elif [ $FW_MINOR = "02" ] || [ $FW_MINOR = "04" ]
	then
		if [ $MINOR != "02" ] && [ $MINOR != "04" ]
		then
			echo "current fw version is $FW. it may incompatible with $upgrad_file_name. please check compatibility."
			exit 1
		fi	
	else
		echo "current fw version is $FW. it may incompatible with $upgrad_file_name. please check compatibility."
		exit 1	
	fi

	if [ $FW_REVISE -gt $MINIMUM ]
	then
		while [ 1 ]
		do
			read -p "warning：version reduce，continue？y/n  " LOW
			if [ -z $LOW ]
 		        then
 		               echo "you need to type in a word！"
			elif [ $LOW = "y" ] || [ $LOW = "yes" ] || [ $LOW = "Y" ] || [ $LOW = "YES" ]
			then
				break
			elif [ $LOW = "n" ] || [ $LOW = "no" ] || [ $LOW = "N" ] || [ $LOW = "NO" ]
			then
				exit 1
			fi
		done		
	fi

	while [ 1 ]
	do
		read -p "backup or not? y/n  " BACKUP
 		if [ -z $BACKUP ]
        	then
        	        echo "you need to type in a word！"
		elif [ $BACKUP = "y" ] || [ $BACKUP = "yes" ] || [ $BACKUP = "YES" ] || [ $BACKUP = "Y" ]
		then
				BACK="xx_v"$FW5"_xx.bin"
				touch $BACK
				chmod 777 $BACK
				sudo mtd_debug read /dev/mtd1 0 4194304 $BACK
				break		
			
		elif [ $BACKUP = "n" ] || [ $BACKUP = "no" ] || [ $BACKUP = "N" ] || [ $BACKUP = "NO" ]
		then
			break
		fi
	done

fi
length=`ls -l ${upgrad_file_name} | cut -d ' ' -f 5`



sleep 0.1
sudo rmmod spi_nor
sleep 0.1
sudo modprobe spi_nor

sudo flash_erase /dev/mtd1 0 0
sync

sleep 1

sudo mtd_debug write /dev/mtd1 0 ${length} ${upgrad_file_name}
sync

sleep 1



