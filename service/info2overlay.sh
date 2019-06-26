#!/bin/bash
killall info2png
killall png2fb16 
killall nns-overlay-deamon 
/home/pi/NNS/FreeplayInfo2Overlay/info2png -i2cbus "/dev/i2c-1" -height 12 -o "/dev/shm" -interval 10 & \
/home/pi/NNS/FreeplayInfo2Overlay/nns-overlay-deamon -pin 41 -file "/dev/shm/fb_footer.png" -duration 5 -interval 250 -lowbatpin 7
