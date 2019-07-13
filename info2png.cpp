/*
NNS @ 2018
info2png
It create a PNG/log file contening CPU load and temperature, Wifi link speed and time, Battery voltage is optional.
*/
const char programversion[]="0.2a"; //program version


#include "gd.h"							//libgd
#include <gdfontt.h>				//libgd tinyfont
#include <stdio.h>					//stream io
#include <cstring>					//string
#include <unistd.h>					//standard
#include <fcntl.h>					//file io
#include <sys/ioctl.h>			//sys io
#include <linux/i2c-dev.h>	//i2c library
#include <ctime>						//time and date
#include <locale.h>					//locale
#include <limits.h>					//limits
#include <math.h>						//math
#include <dirent.h>  				//dir





float nns_map_float(float x,float in_min,float in_max,float out_min,float out_max){
  if(x<in_min){return out_min;}
  if(x>in_max){return out_max;}
  return (x-in_min)*(out_max-out_min)/(in_max-in_min)+out_min;
}

//convert from https://axonflux.com/handy-rgb-to-hsl-and-rgb-to-hsv-color-model-c
void rgb2hsl(int rgb,float *hue,float *saturation,float *lightness){
	float red=(rgb>>16)&0x0FF, green=(rgb>>8)&0x0FF, blue=(rgb)&0x0FF; //separate rgb channel
	*hue=0; *saturation=0; *lightness=0; //reset hsl value
	red/=255; green/=255; blue/=255; //convert rgb to 0-1 range
	float min=fminf(red,fminf(green,blue)); float max=fmaxf(red,fmaxf(green,blue)); //found min and max rgb value
	*lightness=(max+min)/2; //found lightness
	if(max!=min){ //not achromatic
		float diff=max-min; //compute diff
		*saturation=*lightness>0.5 ? diff/(2-max-min) : diff/(max+min); //compute saturation
		if(red>green && red>blue){*hue=(green-blue)/diff+(green<blue ? 6 : 0); //compute hue
		}else if(green>blue){*hue=(blue-red)/diff+2; //compute hue
		}else{*hue=(red-green)/diff+4;} //compute hue
		*hue/=6;
	}
}

float hue2rgb(float p, float q, float t){
	if(t<0){t+=1;} if(t>1){t-=1;}
	if(6*t<1){return p+(q-p)*6*t;}
	if(2*t<1){return q;}
	if(3*t<2){return p+(q-p)*(2.f/3.f-t)*6;}
	return p;
}

int hsl2rgb(float hue,float saturation,float lightness){
	float red=0, green=0, blue=0; //initial rgb channel
	if(saturation==0){red=green=blue=lightness; //achromatic
	}else{
		float q=lightness<0.5 ? lightness*(lightness+saturation) : lightness+saturation-(lightness*saturation);
		float p=2*lightness-q;
		red=255*hue2rgb(p,q,hue+1.f/3); //compute red channel
		green=255*hue2rgb(p,q,hue); //compute green channel
		blue=255*hue2rgb(p,q,hue-1.f/3); //compute blue channel
	}
	return (((int)(red)&0x0ff)<<16)|(((int)(green)&0x0ff)<<8)|((int)(blue)&0x0ff); //merge RGB to int
}

int rgbcolorstep(float x,float in_min,float in_max,int color_min,int color_max){
	float h_min=0,s_min=0,l_min=0,h_max=0,s_max=0,l_max=0; //hsl variables for min and max colors
	rgb2hsl(color_min,&h_min,&s_min,&l_min); //convert color_min rgb to hsl
	rgb2hsl(color_max,&h_max,&s_max,&l_max); //convert color_max rgb to hsl
	h_min=nns_map_float(x,in_min,in_max,h_min,h_max); //compute hue median
	s_min=nns_map_float(x,in_min,in_max,s_min,s_max); //compute saturation median
	l_min=nns_map_float(x,in_min,in_max,l_min,l_max); //compute lightness median
	return hsl2rgb(h_min,s_min,l_min); //convert back to rgb
}


 

//General variables
char data_output_path[PATH_MAX];			//path where output final data
char freeplaycfg_path[PATH_MAX];			//full path like /boot/freeplayfbcp.cfg
char vbat_path[PATH_MAX];							//vbat filename
int update_interval=-1;								//data output interval
FILE *temp_filehandle;								//file handle to get cpu temp/usage
bool battery_enabled=true;						//battery probe boolean
bool png_enabled=true;								//png output boolean
bool wifi_enabled=false;							//wifi boolean
bool wifi_showip=false;								//ip address instead of link speed
bool time_enabled=true;								//display time
bool uptime_enabled=false;						//display uptime
bool time_force_enabled=false;				//force time instead of uptime
bool backlight_set=false;							//display backlight info
bool backlight_enabled=false;					//display backlight info
bool rfkill_enabled=false;						//rfkill is running
char cfg_buf[32];											//config read buffer




//I2C variables
char i2c_bus[PATH_MAX];									//path to i2c bus
int backlight_i2c_address=-1;		//PCA9633 i2c adress, found via 'i2cdetect'
int i2c_handle;									//i2c handle io
char i2c_buffer[10]={0};				//i2c data buffer






//PCA9633 variables
int backlight_value=-1;
int backlight_state=-1;


//rfkill variables
DIR *rfkill_dir_handle; 						//rfkill dir handle
struct dirent *rfkill_dir_cnt; 			//rfkill dir contener
int rfkill_value=0;									//rfkill value used to count hard and soft blocking
int rfkill_count=0;									//rfkill value used to count max possible hard and soft blocking


//GD variables
gdImagePtr gd_image;								//gd image
int gd_image_w=-1, gd_image_h=-1;		//gd image size
const int gd_char_w=5; 							//gd image char width
int gd_col_black, gd_col_white, gd_col_gray, gd_col_darkgray, gd_col_darkergray, gd_col_green, gd_col_tmp, gd_col_text; //gd colors

char gd_icons[]={ //custom gd font char array 8x8
0,0,1,1,1,1,0,0, //char 0 : battery
0,1,1,0,0,1,1,0,
0,1,0,0,0,0,1,0,
0,1,1,1,1,1,1,0,
0,1,0,0,0,0,1,0,
0,1,1,1,1,1,1,0,
0,1,0,0,0,0,1,0,
0,1,1,1,1,1,1,0,

0,0,0,0,0,0,0,0, //char 1 : cpu
0,1,0,1,0,1,0,0,
1,1,1,1,1,1,1,0,
0,1,0,0,0,1,0,0,
1,1,0,0,0,1,1,0,
0,1,0,0,0,1,0,0,
1,1,1,1,1,1,1,0,
0,1,0,1,0,1,0,0,

0,0,0,0,0,0,0,0, //char 2 : wifi
0,0,1,0,0,0,0,0,
0,1,0,1,0,0,0,0,
0,0,1,0,0,0,1,0,
0,0,1,0,0,0,1,0,
0,0,1,0,1,0,1,0,
0,0,1,0,1,0,1,0,
0,0,1,0,1,0,1,0,

0,0,1,1,1,1,0,0, //char 3 : clock
0,1,0,0,0,0,1,0,
1,0,0,1,0,0,0,1,
1,0,0,1,0,0,0,1,
1,0,0,0,1,1,0,1,
1,0,0,0,0,0,0,1,
0,1,0,0,0,0,1,0,
0,0,1,1,1,1,0,0,

0,0,0,0,1,0,0,0, //char 4 : uptime
0,0,0,1,0,1,0,0,
0,0,1,0,0,0,1,0,
0,1,0,0,0,0,0,1,
0,1,1,1,0,1,1,1,
0,0,0,1,0,1,0,0,
0,0,0,1,0,1,0,0,
0,0,0,1,1,1,0,0,

0,0,0,1,1,1,0,0, //char 5 : backlight
0,0,1,0,0,0,1,0,
0,1,0,0,0,0,0,1,
0,1,0,0,1,0,0,1,
0,0,1,0,1,0,1,0,
0,0,0,1,0,1,0,0,
0,0,0,1,0,1,0,0,
0,0,0,1,1,1,0,0,

0,0,0,0,0,0,0,0, //char 6 : none
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0};
gdFont gd_icons_8x8_font_ref = {7,0,8,8,gd_icons}; //declare custom gd font 8x8
gdFontPtr gd_icons_8x8_font = &gd_icons_8x8_font_ref; //pointer to custom gd font

int gd_x_current,gd_x_last,gd_x_wifi; //gd x text position
int gd_wifi_charcount,gd_tmp_charcount; //gd text char count
char gd_chararray[26];				//gd string
char gd_wifi_chararray[20];				//wifi string
int cpu_value=0;									//cpu temp
long double a[4], b[4];						//use to compute cpu load
char cpu_buf[7];									//cpu read buffer
int cpuload_value=0;							//cpu load
int uptime_value=0;								//uptime value
unsigned int uptime_h=0;					//uptime hours value
unsigned int uptime_m=0;					//uptime minutes value
int wifi_linkspeed=0;							//wifi link speed
int wifi_signal=0;								//wifi signal
char pbuffer[20];									//buffer use to read process pipe
time_t now; 											//current date/time
tm *ltime; 												//localtime object


//Battery variables
float vbat_value=-1.;					//battery voltage, used as backup if read fail
float vbatlow_value=3.4;				//battery low voltage
int battery_percent=-1;				//battery percentage



void show_usage(void){
	printf(
"Version: %s\n"
"Example : ./info2png -width 304 -height 10 -o \"/dev/shm\"\n"
"Options:\n"
"\t-i2cbus, path to i2c bus device [Optional, needed only for battery voltage monitoring]\n"
"\t-pca9633adress, [Optional] PCA9633 i2c adress, found via 'i2cdetect'\n"
"\t-width, in px, width of 'fb_footer.png' [Optional, needed for generate png if path to freeplayfbcp.cfg not provided]\n"
"\t-height, in px, height of 'fb_footer.png' [Optional, needed for generate png]\n"
"\t-interval, [Optional] drawing interval in sec\n"
"\t-ip, [Optional] display IP address instead of link speed\n"
"\t-notime, [Optional] disable display of time\n"
"\t-uptime, [Optional] force system uptime instead of time, set by default if no RTC chip detected\n"
"\t-nouptime, [Optional] force time instead of system uptime even if no RTC chip detected\n"
"\t-freeplaycfg, [Optional] usually \"/boot/freeplayfbcp.cfg\", provide data like screen width\n"
"\t-vbatpath, [Optional] usually \"/dev/shm/vbat.log\", provide battery data\n"
"\t-o, output folder for 'fb_footer.png'\n\n"
,programversion);
	
}

int main(int argc, char* argv[]){
	if(argc<3){show_usage();return 1;} //wrong arguments count
	
	strcpy(data_output_path,"/dev/shm/"); //init
	strcpy(freeplaycfg_path,"/boot/freeplayfbcp.cfg"); //init
	strcpy(vbat_path,"/dev/shm/vbat.log"); //init
	
	for(int i=1;i<argc;++i){ //argument to variable
		if(strcmp(argv[i],"-help")==0){show_usage();return 1;
		}else if(strcmp(argv[i],"-i2cbus")==0){strcpy(i2c_bus,argv[i+1]); if(access(i2c_bus,R_OK)!=0){printf("info2png : Failed, %s not readable\n",i2c_bus);return 1;}
		}else if(strcmp(argv[i],"-pca9633adress")==0){sscanf(argv[i+1], "%x", &backlight_i2c_address); backlight_set=true;
		}else if(strcmp(argv[i],"-width")==0){gd_image_w=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-height")==0){gd_image_h=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-interval")==0){update_interval=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-ip")==0){wifi_showip=true;
		}else if(strcmp(argv[i],"-notime")==0){time_enabled=false;
		}else if(strcmp(argv[i],"-uptime")==0){uptime_enabled=true;
		}else if(strcmp(argv[i],"-nouptime")==0){time_force_enabled=true;
		}else if(strcmp(argv[i],"-vbatpath")==0){strcpy(vbat_path,argv[i+1]);
		}else if(strcmp(argv[i],"-freeplaycfg")==0){strcpy(freeplaycfg_path,argv[i+1]); if(access(freeplaycfg_path,R_OK)!=0){printf("info2png : Failed, %s not readable\n",freeplaycfg_path);return 1;}
		}else if(strcmp(argv[i],"-o")==0){strcpy(data_output_path,argv[i+1]); if(access(data_output_path,W_OK)!=0){printf("info2png : Failed, %s not writable\n",data_output_path);return 1;}}
	}
	
	if(freeplaycfg_path!=NULL){ //freeplaycfg path set, try to read the config file
		printf("info2png : freeplaycfg set, try to get viewport info\n");
		temp_filehandle=fopen(freeplaycfg_path,"r"); //open handle
		while(fgets(cfg_buf, sizeof(cfg_buf),temp_filehandle)!=NULL){	//read line
			if(strstr(cfg_buf,"FREEPLAY_SCALE_TO_VIEWPORT=0")!=NULL){	//no scaling
				printf("info2png : FREEPLAY_SCALE_TO_VIEWPORT=0 found, width set to 320\n");
				gd_image_w=320; //set gd width
				break; //exit while loop
			}else if(sscanf(cfg_buf,"FREEPLAY_SCALED_W=%d;",&gd_image_w)){ //found width and try to parse the value
				if(gd_image_w<0){printf("info2png : FREEPLAY_SCALED_W found but reading of the value failed\n",gd_image_w);
				}else{printf("info2png : FREEPLAY_SCALED_W found, detected width = %d\n",gd_image_w);}
			}
		}
		fclose(temp_filehandle); //close handle
	}
	
	if(data_output_path==NULL){printf("info2png : Failed, missing output path\n");show_usage();return 1;} //user miss some needed arguments
	if(gd_image_w<1||gd_image_h<1){printf("info2png : Warning, PNG output disable, missing image width or height.\n");png_enabled=false;} //no png output
	
	if(update_interval<1){printf("info2png : Warning, wrong update interval set, setting it to 15sec\n");update_interval=15;} //wrong interval
	
	if(access("/sbin/iw",F_OK)!=0){printf("info2png : Warning, WIFI link speed detection require 'iw' software\n");}
	
	if(!time_enabled){printf("info2png : Time display disable\n");}
	if(time_force_enabled){printf("info2png : Use time instead of system uptime\n");uptime_enabled=false;}
	if(uptime_enabled){printf("info2png : Use system uptime instead of time\n");}
	
	if(time_enabled&&!uptime_enabled&&!time_force_enabled){ //check if rtc chip is present, force uptime if not
			if(access("/sys/class/rtc/rtc0",R_OK)!=0){printf("info2png : RTC chip not detected, use system uptime instead of time\n"); uptime_enabled=true;}
	}
	
	while(true){
		chdir(data_output_path);							//change directory to output path
		
		//-----------------------------Start of GD part
		if(png_enabled){ //png output enable
			if(access(vbat_path,R_OK)!=0){battery_enabled=false;
			}else{
				temp_filehandle=fopen(vbat_path,"r"); //open file handle
				fscanf(temp_filehandle,"%f;%d",&vbat_value,&battery_percent); //parse 
				fclose(temp_filehandle); //close file handle
				if(vbat_value<0||battery_percent<0){battery_enabled=false;
				}else{battery_enabled=true;}
			}
			
			rfkill_enabled=false; rfkill_value=0; rfkill_count=0; //reset rfkill variables
			rfkill_dir_handle=opendir("/sys/class/rfkill"); //open dir handle
			if(rfkill_dir_handle){ //no problem opening dir handle
				while((rfkill_dir_cnt=readdir(rfkill_dir_handle))!=NULL){ //scan rfkill folder
					if(strncmp(rfkill_dir_cnt->d_name,"rfkill",6)==0){ //filename start with 'rfkill'
						strcpy(cfg_buf,"/sys/class/rfkill/"); //start building device path
						strcat(cfg_buf,rfkill_dir_cnt->d_name); //concatenate device path
						chdir(cfg_buf); //change directory
						temp_filehandle=fopen("hard","r"); //open file handle
						fgets(cfg_buf,sizeof(cfg_buf),temp_filehandle); //read file handle
						fclose(temp_filehandle); //close file handle
						if(atoi(cfg_buf)>0){rfkill_value++; //increment because of hard blocking
						}else{
							temp_filehandle=fopen("soft","r"); //open file handle
							fgets(cfg_buf,sizeof(cfg_buf),temp_filehandle); //read file handle
							fclose(temp_filehandle); //close file handle
							if(atoi(cfg_buf)>0){rfkill_value++;} //increment because of soft blocking
						}
						rfkill_count++;
					}
				}
				closedir(rfkill_dir_handle); //close dir handle
			}
			
			if(rfkill_value>0&&rfkill_count>0&&rfkill_value==rfkill_count){rfkill_enabled=true;} //all devices are soft or hard blocked
			
			chdir(data_output_path);							//change directory back to output path
			
			
			//battery_enabled=false; //debug
			//wifi_enabled=false; //debug
			//time_enabled=false; //debug
			//backlight_enabled=false; //debug
			//uptime_enabled=true; //debug
			//rfkill_enabled=true; //debug
			//uptime_value=3600000; //debug
			
			
			if(backlight_set){ //all need for backlight monitoring is set, no retry on failure
				backlight_enabled=false;
				if((i2c_handle=open(i2c_bus,O_RDWR))<0){ //open i2c bus
					printf("info2png : PCA9633 : Failed to open the I2C bus : %s\n",i2c_bus);
				}else{
					if(ioctl(i2c_handle,I2C_SLAVE,backlight_i2c_address)<0){ //access i2c device, allow retry if failed
						printf("info2png : PCA9633 : Failed to access I2C device : %04x\n",backlight_i2c_address);
					}else{ //success
						i2c_buffer[0]=0x08; //LEDOUT register
						if(write(i2c_handle,i2c_buffer,1)!=1){
							printf("info2png : PCA9633 : Failed to write data to select LEDOUT register\n");
						}else{
							if(read(i2c_handle,i2c_buffer,1)!=1){ //start reading data from i2c device, allow retry if failed
								printf("info2png : PCA9633 : Failed to read LEDOUT register\n");
							}else{ //success
								backlight_state=i2c_buffer[0]<<6>>6; //LED0 output state control, bitshift to get only LED0
								backlight_enabled=true;
								if(backlight_state==0){backlight_value=0; //00 —LED driver x is off
								}else if(backlight_state==1){backlight_value=100; //01 —LED driver x is fully on
								}else if(backlight_state==2||backlight_state==3){ //10 —LED driver x controlled through its PWMxregister
									i2c_buffer[0]=0x02; //PWM0 register
									if(write(i2c_handle,i2c_buffer,1)!=1){
										printf("info2png : PCA9633 : Failed to write data to select PWM0 register\n");
									}else{
										if(read(i2c_handle,i2c_buffer,1)!=1){ //start reading data from i2c device, allow retry if failed
											printf("info2png : PCA9633 : Failed to read PWM0 register\n");
										}else{backlight_value=((i2c_buffer[0]*100)/255);} //success, convert to 0-100 range
									}
								}
							}
						}
					}
					close(i2c_handle);
				}
			}
			
			
			wifi_enabled=false;
			if(!rfkill_enabled){ //if rfkill not detected
				if(access("/sbin/iw",F_OK)!=-1 && !wifi_showip){ //check if 'iw' is installed for WiFi link speed
					temp_filehandle=popen("iw dev wlan0 link 2> /dev/null  | sed 's/^[[:space:]]*//g' | sed 's/ //g'", "r"); //open process pipe
					if(temp_filehandle!=NULL){ //if process not fail
						char *ret;
						while(fgets(cfg_buf,sizeof(cfg_buf),temp_filehandle)!=NULL){	//read line
							if(strstr(cfg_buf,"signal")!=NULL){sscanf(cfg_buf,"%*[^0123456789]%d",&wifi_signal); //signal
							}else if(strstr(cfg_buf,"bitrate")!=NULL){ //speed
								wifi_enabled=true;
								sscanf(cfg_buf,"%*[^0123456789]%d",&wifi_linkspeed);
								gd_wifi_charcount=sprintf(gd_wifi_chararray,"%iMBit/s",wifi_linkspeed);
							}
						}
						pclose(temp_filehandle); //close process pipe
					}
				}else{ //'iw' is not installed, show ip address instead
					temp_filehandle=popen("hostname -I | awk '{printf \"%s\",$1}'", "r");	//open process pipe
					if(temp_filehandle!=NULL){ //if process not fail
						if(fgets(pbuffer,sizeof(gd_wifi_chararray),temp_filehandle)){ //if output something
							gd_wifi_charcount=sprintf(gd_wifi_chararray,"%s",pbuffer);
				  		if(strcmp(gd_wifi_chararray,"127.0.0.1")==0){wifi_showip=false; //if ip is 127.0.0.1, disable
				  		}else{wifi_showip=true; wifi_enabled=true;}
				  	}
				  	pclose(temp_filehandle); //close process pipe
					}
				}
			}
			
			
			gd_image=gdImageCreateTrueColor(gd_image_w,gd_image_h); //allocate gd image
			
			//color int alpha|red|green|blue
			gd_col_black=(int)0x00000000; //since it is the first color declared, it will be the background
			gd_col_white=(int)0x00ffffff; //declarate white color
			gd_col_gray=(int)0x00808080; //declarate gray color
			gd_col_darkgray=(int)0x00404040; //declarate dark gray color
			gd_col_darkergray=(int)0x00282828; //declarate darker gray color
			gd_col_green=(int)0x0000ff00; //declarate green color
			
			
			for(int i=-gd_image_h;i<gd_image_w;i+=6){gdImageLine(gd_image,i,0,i+gd_image_h,gd_image_h,gd_col_darkergray);} //background decoration
			
			
			//start of the left side
			gd_x_current=gd_char_w; //update x position
			gd_x_last=gd_image_w; //update x last position
			if(battery_enabled){ //battery voltage render
				if(vbatlow_value<0){gd_col_text=gd_col_green; //low battery voltage not set, set color to green
				}else{gd_col_text=rgbcolorstep(vbat_value,vbatlow_value,4.2,(int)0x00ff0000,(int)0x0000ff00);} //compute int color
				gd_tmp_charcount=sprintf(gd_chararray,"%d%%/%.2fv",battery_percent,vbat_value); //prepare char array to render
				
				//printf("%d%%/%.2fv\n",battery_percent,vbat_value);
				if(!wifi_enabled&&!time_enabled){ //place battery on right side if no wifi and no time
					gd_x_current=gd_image_w-gd_char_w-9-gd_tmp_charcount*gd_char_w;
					gd_x_last=gd_x_current-gd_char_w-1;
				}
				gdImageChar(gd_image,gd_icons_8x8_font,gd_x_current,1,0x00,gd_col_text); //draw battery icon
				gdImageString(gd_image,gdFontTiny,gd_x_current+9,1,(unsigned char*)gd_chararray,gd_col_text); //print battery info to gd image
				gd_x_current+=9+gd_tmp_charcount*gd_char_w; //update x position
				gdImageChar(gd_image,gdFontTiny,gd_x_current-6*gd_char_w,1,0x2F,gd_col_gray); //draw / in gray
				if(!wifi_enabled&&!time_enabled){
					gdImageLine(gd_image,gd_x_last+1,1,gd_x_last+1,gd_image_h-2,gd_col_darkgray); //draw separator
					gd_x_current=gd_char_w; //update x position to restart on left side
				}else{
					gdImageLine(gd_image,gd_x_current+gd_char_w,1,gd_x_current+gd_char_w,gd_image_h-2,gd_col_darkgray); //draw separator
					gd_x_current+=2*gd_char_w+1; //update x position
				}
			}
			
			
			//cpu temp render
			temp_filehandle=fopen("/sys/class/thermal/thermal_zone0/temp","r"); //open sys file
			fgets(cpu_buf, sizeof(cpu_buf),temp_filehandle); //read value
			fclose(temp_filehandle); //close sys file
			cpu_value=atoi(cpu_buf)/1000; //compute temperature
			gd_tmp_charcount=sprintf(gd_chararray,"%i°c",cpu_value); //prepare char array to render
			gd_col_text=rgbcolorstep(cpu_value,50,80,(int)0x0000ff00,(int)0x00ff0000); //compute int color
			gdImageChar(gd_image,gd_icons_8x8_font,gd_x_current,0,0x01,gd_col_text); //draw cpu icon
			gdImageString(gd_image,gdFontTiny,gd_x_current+9,1,(unsigned char*)gd_chararray,gd_col_text); //print cpu temp to gd image
			gd_x_current+=9+(gd_tmp_charcount+1)*gd_char_w; //update x position
			gdImageChar(gd_image,gdFontTiny,gd_x_current-gd_char_w,1,0x2F,gd_col_gray); //draw / in gray
			
			
			//cpu load render
		  temp_filehandle=fopen("/proc/stat","r"); //code from https://stackoverflow.com/questions/3769405/determining-cpu-utilization
		  fscanf(temp_filehandle,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
		  fclose(temp_filehandle);
		  sleep(1);
		  temp_filehandle=fopen("/proc/stat","r");
		  fscanf(temp_filehandle,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
		  fclose(temp_filehandle);
		  cpuload_value=(((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])))*100;
			gd_tmp_charcount=sprintf(gd_chararray,"%3i%%",cpuload_value); //prepare char array to render
			gd_col_text=rgbcolorstep(cpuload_value,25,100,(int)0x0000ff00,(int)0x00ff0000); //compute int color
			gdImageString(gd_image,gdFontTiny,gd_x_current,1,(unsigned char*)gd_chararray,gd_col_text);//print cpu load
			if(cpuload_value<100){gdImageChar(gd_image,gdFontTiny,gd_x_current,1,0x30,gd_col_gray);} //draw 0 in gray
			if(cpuload_value<10){gdImageChar(gd_image,gdFontTiny,gd_x_current+gd_char_w,1,0x30,gd_col_gray);} //draw 0 in gray
			gd_x_current+=gd_tmp_charcount*gd_char_w; //update x position
			gdImageLine(gd_image,gd_x_current+gd_char_w,1,gd_x_current+gd_char_w,gd_image_h-2,gd_col_darkgray); //draw separator
			if(!wifi_enabled&&!time_enabled&&!backlight_enadbled&&!rfkill_enabled){ //battery is placed on right side
				gdImageLine(gd_image,gd_x_current+gd_char_w,(gd_image_h/2)-1,gd_x_last,(gd_image_h/2)-1,gd_col_darkgray); //filler
			}else{gd_x_last=gd_x_current+gd_char_w+1;} //update last x position, used for filler
			
			
			//start of the right side
			gd_x_current=gd_image_w-gd_char_w; //update x position
			if(time_enabled){ //time render
				if(uptime_enabled){
					temp_filehandle=fopen("/proc/uptime","r"); fscanf(temp_filehandle,"%u",&uptime_value); fclose(temp_filehandle); //get system uptime
					uptime_h=uptime_value/3600; uptime_m=(uptime_value-uptime_h*3600)/60;
					gd_tmp_charcount=sprintf(gd_chararray,"%02i:%02i",uptime_h,uptime_m); //prepare char array to render
				}else{ //current date/time, localtime object
					now=time(0); ltime=localtime(&now);
					gd_tmp_charcount=sprintf(gd_chararray,"%02i:%02i",ltime->tm_hour,ltime->tm_min); //prepare char array to render
				}
				gd_x_current-=gd_tmp_charcount*gd_char_w; //update x position
				if(uptime_enabled){gdImageChar(gd_image,gd_icons_8x8_font,gd_x_current-10,1,0x04,gd_col_white); //uptime icon
				}else{gdImageChar(gd_image,gd_icons_8x8_font,gd_x_current-10,1,0x03,gd_col_white);} //clock icon
				gdImageLine(gd_image,gd_x_current-gd_char_w-10,1,gd_x_current-gd_char_w-10,gd_image_h-2,gd_col_darkgray); //draw separator
				gdImageString(gd_image,gdFontTiny,gd_x_current,1,(unsigned char*)gd_chararray,gd_col_white); //print time
				gd_x_current-=2*gd_char_w-1+10; //update x position
			}
			
			
			if(backlight_enabled){ //backlight render
				gd_tmp_charcount=sprintf(gd_chararray,"%i%%",backlight_value); //prepare char array to render
				gd_x_current-=gd_tmp_charcount*gd_char_w; //update x position
				gdImageChar(gd_image,gd_icons_8x8_font,gd_x_current-9,1,0x05,gd_col_white);
				gdImageString(gd_image,gdFontTiny,gd_x_current,1,(unsigned char*)gd_chararray,gd_col_white); //print wifi link speed
				gdImageLine(gd_image,gd_x_current-gd_char_w-9,1,gd_x_current-gd_char_w-9,gd_image_h-2,gd_col_darkgray); //draw separator
				gd_x_current-=2*gd_char_w-1+9; //update x position
			}
			
			
			if(rfkill_enabled){ //airplane mode render
				gd_tmp_charcount=sprintf(gd_chararray,"Airplane Mode"); //prepare char array to render
				gd_x_current-=gd_tmp_charcount*gd_char_w; //update x position
				gdImageString(gd_image,gdFontTiny,gd_x_current,1,(unsigned char*)gd_chararray,gd_col_green); //print wifi link speed
				gdImageLine(gd_image,gd_x_current-gd_char_w,1,gd_x_current-gd_char_w,gd_image_h-2,gd_col_darkgray); //draw separator
				gd_x_current-=2*gd_char_w-1; //update x position
			}else if(wifi_enabled||wifi_showip){ //wifi render
				gd_x_current-=gd_wifi_charcount*gd_char_w; //update x position
				if(!wifi_showip){ //draw wifi icon with color based on signal 
					if(vbatlow_value<0){gd_col_text=gd_col_white; //no signal=white
					}else{gd_col_text=rgbcolorstep(wifi_signal,30,91,(int)0x0000ff00,(int)0x00ff0000);} //compute int color
					gdImageChar(gd_image,gd_icons_8x8_font,gd_x_current-9,0,0x02,gd_col_text);
				}
				if(wifi_linkspeed<1){gd_col_text=gd_col_white; //no link speed=white
				}else{gd_col_text=rgbcolorstep(wifi_linkspeed,5,72,(int)0x00ff0000,(int)0x0000ff00);} //compute int color
				gdImageString(gd_image,gdFontTiny,gd_x_current,1,(unsigned char*)gd_wifi_chararray,gd_col_text); //print wifi link speed
				if(!wifi_showip){gd_x_current-=9;} //update x position
				gdImageLine(gd_image,gd_x_current-gd_char_w,1,gd_x_current-gd_char_w,gd_image_h-2,gd_col_darkgray); //draw separator
				gd_x_current-=2*gd_char_w-1; //update x position
			}
			
			
			if(wifi_enabled||time_enabled||backlight_enabled||rfkill_enabled){gdImageLine(gd_image,gd_x_current+gd_char_w-1,(gd_image_h/2)-1,gd_x_last,(gd_image_h/2)-1,gd_col_darkgray);} //filler
			
			
			gdImageLine(gd_image,0,gd_image_h-1,gd_image_w,gd_image_h-1,gd_col_gray); 				//bottom decoration
			
			temp_filehandle=fopen("fb_footer.png","wb"); 																																					//open image file
			gdImagePng(gd_image,temp_filehandle);																																										//output gd image to file
			fclose(temp_filehandle);																																																//close image file
			gdImageDestroy(gd_image);																																																//free gd image memory
		}
		//return 0; //debug
		sleep(update_interval); //sleep
	}
	
	return 0;
}