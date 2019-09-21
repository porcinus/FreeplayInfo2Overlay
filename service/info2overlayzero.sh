#!/bin/bash
killall info2png
killall png2fb16
killall nns-overlay-deamon
/home/pi/NNS/FreeplayInfo2Overlay/nns-overlay-deamon -standalone -height 12 -pin 41 -file "/dev/shm/fb_footer.png" -duration 5 -interval 250 -lowbatpin 7
