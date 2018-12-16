#!/bin/bash
killall info2png
killall png2fb16 
killall nns-overlay-deamon 
/home/pi/NNS/FreeplayInfo2Overlay/info2png -o "/dev/shm" -width 304 -height 11 -video 5 -interval 15 & \ 
/home/pi/NNS/FreeplayInfo2Overlay/nns-overlay-deamon -pin 41 -file "/dev/shm/fb_footer.avi" -screenwidth 1024 -height 50 -interval 250