# FreeplayInfo2Overlay

These programs are design to work on Raspberry Pi 3 on Freeplay CM3 platform with L2R2 addon board.

- info2png : Output battery voltage and try to predict it's level if ADC data provided (require MCP3021 or MCP3221 ADC chip) , CPU load and temperature, wifi link speed or IP (if detected), backlight value (if PCA9633 is set) and system time. Depending on arguments passed, can generate a png file, a log containing battery voltage.

- png2fb16 : Copy a png file to a 16 bits framebuffer, to use aside of info2png (deprecated, related parts in archive folder).

- nns-overlay-deamon : Use to monitor a gpio button input to display overlay, can monitor a gpio pin to alert user of a low battery state, to be used aside of info2png, ONLY work with dispmanx driver, require WiringPi.

- img2dispmanx : This file heavily based on https://github.com/hex007/eop , use to display png or jpeg picture to dispmanx.

- freeplay_overlay_config.sh/gpio-input-detect : Configuration utility, Allow end user to set input used to display overlay, enable or disable info2png and nns-overlay-deamon service, require WiringPi.


# Todo


# History
### info2png:
- 0.1a : Initial release.
- 0.1b : Add a logging system for plot, implement srt subtitle file generation for omxplayer.
- 0.1c : Removed srt subtitle implement, Various bugfix.
- 0.1d : Code cleanup, can show ip address instead of link speed using -ip, background no more flat.
- 0.1e : Allow user not to use resistor divider for battery monitoring.
- 0.1f : Battery monitoring rework to allow retry on failure, arguments combination rework as well, RGB-HSL implement.
- 0.1g : Battery percentage prediction.
- 0.1h : Time display can be disable using -notime or display system uptime using -uptime, freeplayfbcp.cfg can be set using -freeplaycfg to get width of the TFT screen. Battery, CPU and WiFi text replaced by icons. WiFi icon color based on signal strength, arrangement changed depending on things to display.
- 0.1i : By default, if no RTC chip detected, the software will show system uptime instead of time, this can be disable by using -nouptime. Clock and uptime icons added.
- 0.1j : Can monitor PCA9633 pwm value if i2c adress provided via -pca9633adress argument. Can now detect rfkill state of each wireless device to notify user if in "Airplane Mode".
- 0.1k : Adding -adcoffset argument to set ADC chip error offset, this value can be positive or negative, it is apply to the computed result, not the raw analog reading. Updated CM3 battery curve.
- 0.2a : Major update, battery data stuff moved to https://github.com/porcinus/FreeplayBatteryDaemon
- 0.2b : Now detect Bluetooth dongle plus device(s) connected, WiFi fixed/rework.

### png2fb16 (deprecated, related parts in archive folder):
- 0.1a : Initial release.
- 0.1b : Bugfix.

### img2dispmanx:
- 0.1a : Initial release.
- 0.1b : width, height can be set to FILL to fill screen size.

### nns-overlay-deamon:
- 0.1a : Initial release.
- 0.1b : Implement ffmpeg and omxplayer to display overlay.
- 0.1c : Implement img2dispmanx to avoid use of ffmpeg and omxplayer, added overheat icon.
- 0.1d : Added low battery icon (gpio input if set using -lowbatpin, plus use -lowbatreverselogic if reversed logic).
- 0.1e : Adapt program to updated img2dispmanx, No more need to set width and height.
- 0.1f : Now fully work in non blocking mode.
- 0.1g : Conversion from sysfs to WiringPi to monitor GPIO, more reactive, all overlay now run in non blocking mode, various bugfix.

### freeplay_overlay_config.sh/gpio-input-detect
- 0.1a : Initial release, need to be tested on multiple device.


# Provided scripts :
- compile.sh : Compile all cpp files. Require libgd-dev, zlib1g-dev, libfreetype6-dev, libpng-dev, libjpeg-dev.
- example-framebuffer.sh : Run info2png and png2fb16 (Battery monitoring enabled), (deprecated, related parts in archive folder).
- example-overlay.sh : Run info2png and nns-overlay-deamon (Battery monitoring enabled), (deprecated, related parts in archive folder).
- example-nobattery-framebuffer.sh : Run info2png and png2fb16 (No battery), (deprecated, related parts in archive folder).
- example-nobattery-overlay.sh : Run info2png and nns-overlay-deamon (No battery), (deprecated, related parts in archive folder).
- example-killall.sh : Use it to kill all instances.

# Setup as service :
Note before start: You have to edit wanted .service and .sh files in order to get script work.

Choose right file: 
 - info2framebuffer.sh and info2framebuffer.service : When using ADC to monitor battery voltage, copy informations 16bit framebuffer (/dev/fb1), (deprecated, related parts in archive folder).
 - info2overlay.sh and info2overlay.service : When using ADC to monitor battery voltage, when specific gpio input is pressed, display picture generated with info2png as a overlay, Note: only work with gl and dispmanx, (deprecated, related parts in archive folder).
 - info2framebuffer-nobattery.sh and info2framebuffer-nobattery.service : Copy some system informations to 16bit framebuffer (/dev/fb1), (deprecated, related parts in archive folder).
 - info2overlay-nobattery.sh and info2overlay-nobattery.service : When specific gpio input is pressed, start omxplayer with a display picture generated with info2png as a overlay, Note: only work with gl and dispmanx, (deprecated, related parts in archive folder).

To install as a service:
cp [WANTEDSERVICE].service /lib/systemd/system/[WANTEDSERVICE].service ; \
systemctl enable [WANTEDSERVICE].service

To remove the service:
systemctl disable [WANTEDSERVICE].service ; \
rm /lib/systemd/system/[WANTEDSERVICE].service


# Plot using GNUplot
If you are interrested by plotting battery to a png file, vbat-plot.sh is provided as a example.
To add this as a item in Retropie menu in EmulationStation:
 - Create a file named "vbatgraph.sh" with following contain:
 
```sudo /[PATH WHERE YOU CLONED THIS GIT]/vbat-plot.sh```

```sudo fbi -1 -t "10" -noverbose -a "/dev/shm/vbat-plot.png" </dev/tty &>/dev/null```

 - Temporary disable gamelist update in EmulationStation setting.
 - Edit "/opt/retropie/configs/all/emulationstation/gamelists/retropie/gamelist.xml" and add before ```</gameList>```:
```<game><path>./vbatgraph.sh</path><name>Battery Graph</name></game>```

 - Restart EmulationStation.
 - At this point you shoul be able to see the new item in Retropie menu.
 - Re-enable gamelist update in EmulationStation setting.

# Issues
### Scripts don't work
Don't miss to chmod all .sh files in the folder : chmod 0755 **/*.sh

### Overlay is displayed when gpio pin 'not pressed'
Add argument ' -reverselogic' to nns-overlay-deamon run script line.

### Overlay display low battery icon when low battery gpio pin 'not trigger'
Add argument ' -lowbatreverselogic' to nns-overlay-deamon run script line.


