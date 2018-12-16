#!/bin/bash
killall info2png
killall png2fb16 
killall nns-overlay-deamon 
./info2png -o "/dev/shm" -interval 15 -width 304 -height 11 -video 5 & \ 
./nns-overlay-deamon -pin 41 -srtfile "/dev/shm/vbat.srt" -screenwidth 1024 -height 50 -interval 250