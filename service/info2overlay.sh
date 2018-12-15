#!/bin/bash
killall info2png
killall png2fb16 
killall nns-overlay-deamon 
/home/pi/NNS/FreeplayInfo2Overlay/info2png -i2cbus "/dev/i2c-1" -i2caddress 0x4d -adcvref 3.28 -adcres 4096 -r1value 9.1 -r2value 21.3 -vbatlow 3.5 -vbatlogging -o "/dev/shm" -interval 15 & \
/home/pi/NNS/FreeplayInfo2Overlay./nns-overlay-deamon -pin 41 -srtfile "/dev/shm/vbat.srt" -screenwidth 1024 -height 50 -interval 250
