# FreeplayInfo2Framebuffer

These programs are design to work on Raspberry Pi 3 on Freeplay CM3 platform with L2R2 addon board.

It create a PNG file contening CPU load and temperature, Wifi link speed and time, Battery voltage is optional then copy it to 16bits framebuffer or use a overlay.

Usage: Execute each program without argument to get informations.

compile.sh : compile info2png, png2fb, nns-overlay-deamon

example-framebuffer.sh or example-nobattery-framebuffer.sh : example to create and copy infomation to a framebuffer

example-overlay.sh : to use aside info2png, example to display information as a 'overlay' using omxplayer when pressing a gpio button, note: only work with gl and dispmanx
