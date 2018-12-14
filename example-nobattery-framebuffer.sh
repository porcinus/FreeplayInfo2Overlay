#!/bin/bash 
while [ TRUE ]; do
./info2png -width 304 -height 11 -o "/dev/shm"
./png2fb16 -i "/dev/shm/fb_footer.png" -f "/dev/fb1" -xoffset 16 -yoffset 214
sleep 15
done
