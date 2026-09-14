#!/bin/bash

echo 0 > /sys/class/pwm/pwmchip2/pwm0/enable 
echo 1 > /sys/class/pwm/pwmchip2/pwm0/enable 

echo 0 > /sys/class/pwm/pwmchip5/pwm0/enable
echo 1 > /sys/class/pwm/pwmchip5/pwm0/enable

echo 0 > /sys/class/pwm/pwmchip6/pwm0/enable
echo 1 > /sys/class/pwm/pwmchip6/pwm0/enable

