#!/bin/bash

N=0

while [ $N -lt 12 ]
do
	#echo $N
	./5200.sh $N
	N=$((N+1))
done

./gpio-pwm-0.sh
./gpio-pwm-1.sh
./pwm.sh

