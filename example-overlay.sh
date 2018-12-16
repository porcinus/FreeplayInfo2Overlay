#!/bin/bash
killall info2png
killall png2fb16 
killall nns-overlay-deamon 
./info2png -i2cbus "/dev/i2c-1" -i2caddress 0x4d -adcvref 3.28 -adcres 4096 -r1value 9.1 -r2value 21.3 -vbatlow 3.5 -vbatlogging -width 300 -height 12 -o "/dev/shm" -interval 10 & \
./nns-overlay-deamon -pin 41 -file "/dev/shm/fb_footer.png" -duration 5 -screenwidth 1024 -height 45 -interval 250