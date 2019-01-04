# FreeplayInfo2Overlay

These programs are design to work on Raspberry Pi 3 on Freeplay CM3 platform with L2R2 addon board.

- info2png : Output battery voltage (if ADC and resistor divider data provided , cpu load and temperature, wifi link speed (if detected) and system time. Depending on arguments passed, can generate a png file, a log containing battery voltage.

- png2fb16 : Copy a png file to a 16 bits framebuffer, to use aside of info2png.

- nns-overlay-deamon : Use to monitor a gpio button input to display osd, can monitor a gpio pin to alert user of a low battery state, to be used aside of info2png, ONLY work with dispmanx driver.

- img2dispmanx : This file heavily based on https://github.com/hex007/eop , use to display png or jpeg picture to dispmanx.

# History

info2png:
- 0.1a : Initial release.
- 0.1b : Add a logging system for plot, implement srt subtitle file generation for omxplayer.
- 0.1c : Removed srt subtitle implement, Various bugfix.
- 0.1d : Code cleanup, can show ip address instead of link speed using -ip, background no more flat.

png2fb16:
- 0.1a : Initial release.
- 0.1b : Bugfix.

img2dispmanx:
- 0.1a : Initial release.
- 0.1b : width, height can be set to FILL to fill screen size.

nns-overlay-deamon:
- 0.1a : Initial release.
- 0.1b : Implement ffmpeg and omxplayer to display overlay.
- 0.1c : Implement img2dispmanx to avoid use of ffmpeg and omxplayer, added overheat icon.
- 0.1d : Added low battery icon (gpio input if set using -lowbatpin, plus use -lowbatreverselogic if reversed logic).
- 0.1e : Adapt program to updated img2dispmanx, No more set width and height.

# Provided scripts :
- compile.sh : Compile all cpp files. Require libgd-dev, zlib1g-dev, libfreetype6-dev, libpng-dev, libjpeg-dev.
- example-framebuffer.sh : Run info2png and png2fb16 (Battery monitoring enabled).
- example-overlay.sh : Run info2png and nns-overlay-deamon (Battery monitoring enabled).
- example-nobattery-framebuffer.sh : Run info2png and png2fb16 (No battery).
- example-nobattery-overlay.sh : Run info2png and nns-overlay-deamon (No battery).
- example-killall.sh : Use it to kill all instances.

# Setup as service :
Note before start: You have to edit wanted .service and .sh files in order to get script work.

Choose right file: 
 - info2framebuffer.sh and info2framebuffer.service : When using ADC to monitor battery voltage, copy informations 16bit framebuffer (/dev/fb1).
 - info2overlay.sh and info2overlay.service : When using ADC to monitor battery voltage, when specific gpio input is pressed, start omxplayer with a specific subtitle to create a 'osd', Note: only work with gl and dispmanx.
 - info2framebuffer-nobattery.sh and info2framebuffer-nobattery.service : Copy some system informations to 16bit framebuffer (/dev/fb1).
 - info2overlay-nobattery.sh and info2overlay-nobattery.service : When specific gpio input is pressed, start omxplayer with a specific subtitle to create a 'osd', Note: only work with gl and dispmanx.

To install as a service:
cp [WANTEDSERVICE].service /lib/systemd/system/[WANTEDSERVICE].service ; \
systemctl enable [WANTEDSERVICE].service

To remove the service:
systemctl disable [WANTEDSERVICE].service ; \
rm /lib/systemd/system/[WANTEDSERVICE].service


# Plot using GNUplot
If you are interrested by plotting battery to a png file, vbat-plot.sh is provided as a example.


# Scripts don't work
Don't miss to chmod all .sh files in the folder : chmod 0755 **/*.sh

# Overlay is displayed when gpio pin 'not pressed'
Add argument ' -reverselogic' to nns-overlay-deamon run script line

# Overlay is displayed low battery icon when low battery gpio pin 'not trigger'
Add argument ' -lowbatreverselogic' to nns-overlay-deamon run script line

# Known issues
- For some reason, system can fail to keep sh script open resulting on 'sh: 0: Pipe call failed' in var/log/syslog witch is really rare, occur only once in a month in my case.
