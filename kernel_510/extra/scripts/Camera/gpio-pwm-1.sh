echo 0 > /sys/class/pwm/pwmchip6/export
sleep 1
echo 33333000 > /sys/class/pwm/pwmchip6/pwm0/period
echo 7000000 > /sys/class/pwm/pwmchip6/pwm0/duty_cycle
#echo 1 > /sys/class/pwm/pwmchip6/pwm0/enable
