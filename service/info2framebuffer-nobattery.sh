#!/bin/bash 
killall info2png
killall png2fb16
killall nns-overlay-deamon 
./info2png -width 304 -height 11 -o "/dev/shm" -interval 15 & \ 
./png2fb16 -i "/dev/shm/fb_footer.png" -f "/dev/fb1" -xoffset 16 -yoffset 214 -interval 15 
