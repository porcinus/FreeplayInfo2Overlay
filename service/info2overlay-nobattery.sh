#!/bin/bash
killall info2png
killall png2fb16 
killall nns-overlay-deamon 
/home/pi/NNS/FreeplayInfo2Overlay/info2png -o "/dev/shm" -interval 15 & \ 
/home/pi/NNS/FreeplayInfo2Overlay/nns-overlay-deamon -pin 41 -srtfile "/dev/shm/vbat.srt" -screenwidth 1024 -height 50 -interval 250
