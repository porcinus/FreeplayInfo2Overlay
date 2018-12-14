#!/bin/bash 
while [ TRUE ]; do
info2png -i2cbus "/dev/i2c-1" -i2caddress 0x4d -adcvref 3.25 -adcres 4096 -r1value 91 -r2value 220 -vbatlow 3.5 -width 304 -height 10 -o "/dev/shm"
png2fb16 -i "/dev/shm/fb_footer.png" -f "/dev/fb1" -xoffset 16 -yoffset 214
sleep 15
done
