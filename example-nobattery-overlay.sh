#!/bin/bash
killall info2png
killall nns-overlay-deamon 
./info2png -o "/dev/shm" -interval 15 -width 300 -height 12 & \ 
./nns-overlay-deamon -pin 41 -file "/dev/shm/fb_footer.png" -duration 5 -interval 250