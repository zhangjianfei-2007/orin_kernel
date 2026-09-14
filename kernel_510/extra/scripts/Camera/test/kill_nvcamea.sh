#!/usr/bin/env sh
# restart nvargus-daemon

export enableCamScfLogs=5
export enableCamPclLogs=5

#stop nvargus-daemon
sudo systemctl stop nvargus-daemon

#check if nvargus-daemon still exists
pid1=`ps -ef | grep "nvargus-daemon" | grep -v grep | awk '{print $2}'`

if [ $pid1 ]
then
	sudo kill -9 "$pid1"
fi

sleep 1

#start nvargus-daemon
#sudo /usr/sbin/nvargus-daemon&
sudo systemctl start nvargus-daemon