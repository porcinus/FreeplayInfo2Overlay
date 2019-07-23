#!/bin/bash
killall info2png
killall png2fb16 
killall nns-overlay-deamon 
OVLPATH/info2png -height 12 -o "/dev/shm" -interval 10 & \
OVLPATH/nns-overlay-deamon -file "/dev/shm/fb_footer.png" -duration 5 -interval 250 -lowbatpin 7 -pin OVLPIN
