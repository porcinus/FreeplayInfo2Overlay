#!/bin/bash
killall info2png
killall png2fb16 
killall nns-overlay-deamon 
/home/pi/NNS/FreeplayInfo2Overlay/info2png -o "/dev/shm" -width 300 -height 12 -interval 15 & \ 
/home/pi/NNS/FreeplayInfo2Overlay/nns-overlay-deamon -pin 41 -file "/dev/shm/fb_footer.png" -duration 5 -interval 250