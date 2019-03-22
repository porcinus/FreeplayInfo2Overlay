/*
NNS @ 2018
info2png
It create a PNG/log file contening CPU load and temperature, Wifi link speed and time, Battery voltage is optional.
*/
const char programversion[]="0.1h"; //program version


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

#include "battery_cm3.h"		//battery data for the Freeplay CM3 platform




int vbat_smooth_value[5];	//array to store last smoothed data
bool vbat_smooth_init=false;	//array initialized?

int nns_get_battery_percentage(int vbat){
	if(!vbat_smooth_init){vbat_smooth_value[0]=vbat_smooth_value[1]=vbat_smooth_value[2]=vbat_smooth_value[3]=vbat;vbat_smooth_init=true;} //initialize array if not already done
	vbat=(vbat+vbat_smooth_value[3]+vbat_smooth_value[2]+vbat_smooth_value[1]+vbat_smooth_value[0])/5; //smoothed value
	vbat_smooth_value[0]=vbat_smooth_value[1]; vbat_smooth_value[1]=vbat_smooth_value[2]; vbat_smooth_value[2]=vbat_smooth_value[3]; vbat_smooth_value[3]=vbat; //shift array
	if(vbat<battery_percentage[0]){return 0;} //lower than min value, 0%
	if(vbat>=battery_percentage[100]){return 100;} //higher than max value, 100%
	for(int i=0;i<100;i++){if(vbat>=battery_percentage[i]&&vbat<battery_percentage[i+1]){return i;}} //return the value
	return -1; //oups
}

int nns_map(float x,float in_min,float in_max,int out_min,int out_max){
  if(x<in_min){return out_min;}
  if(x>in_max){return out_max;}
  return (x-in_min)*(out_max-out_min)/(in_max-in_min)+out_min;
}

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
	if(t<0){t+=1;}
	if(t>1){t-=1;}
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
		red=255*hue2rgb(p,q,hue+1.f/3);
		green=255*hue2rgb(p,q,hue);
		blue=255*hue2rgb(p,q,hue-1.f/3);
	}
	return (((int)(red)&0x0ff)<<16)|(((int)(green)&0x0ff)<<8)|((int)(blue)&0x0ff);
}

int rgbcolorstep(float x,float in_min,float in_max,int color_min,int color_max){
	float h_min=0,s_min=0,l_min=0,h_max=0,s_max=0,l_max=0; //hsl variables for min and max colors
	rgb2hsl(color_min,&h_min,&s_min,&l_min); //convert color_min rgb to hsl
	rgb2hsl(color_max,&h_max,&s_max,&l_max); //convert color_max rgb to hsl
	h_min=nns_map_float(x,in_min,in_max,h_min,h_max); //compute hue median
	s_min=nns_map_float(x,in_min,in_max,s_min,s_max); //compute saturation median
	l_min=nns_map_float(x,in_min,in_max,l_min,l_max); //compute lightness median
	return hsl2rgb(h_min,s_min,l_min); //convert back to rgb
	
	//char color_final_r=nns_map(x,in_min,in_max,(color_min>>16)&0x0FF,(color_max>>16)&0x0FF);
	//char color_final_g=nns_map(x,in_min,in_max,(color_min>>8)&0x0FF,(color_max>>8)&0x0FF);
	//char color_final_b=nns_map(x,in_min,in_max,(color_min>>0)&0x0FF,(color_max>>0)&0x0FF);
	//return ((color_final_r&0x0ff)<<16)|((color_final_g&0x0ff)<<8)|(color_final_b&0x0ff);
}


 

//General variables
char *data_output_path;								//path where output final data
char *freeplaycfg_path;								//full path like /boot/freeplayfbcp.cfg
int update_interval=-1;								//data output interval
FILE *temp_filehandle;								//file handle to get cpu temp/usage
bool battery_enabled=true;						//battery probe boolean
bool resistor_divider_enabled=true;		//battery probe boolean
bool battery_set=false;								//all informations are set for battery probe boolean
bool battery_log_enabled=false;				//battery log from start boolean
bool png_enabled=true;								//png output boolean
bool wifi_enabled=false;							//wifi boolean
bool wifi_showip=false;								//ip address instead of link speed
bool time_enabled=true;								//display time
char cfg_buf[32];											//config read buffer




//I2C variables
char *i2c_bus;						//path to i2c bus
int i2c_address=-1;				//i2c device adress, found via 'i2cdetect'
int i2c_handle;						//i2c handle io
char i2c_buffer[10]={0};	//i2c data buffer



//ADC variables
float adc_vref=-1;								//in volt, vdd of the adc chip
int adc_resolution=4096;						//256:8bits, 1024:10bits, 4096:12bits (default), 65535:16bits
int divider_r1=0, divider_r2=0;		//resistor divider value in ohm or kohm, check show_usage(void) for infomations
int adc_raw_value=0;							//adc step value
int adc_read_retry=0;							//adc reading retry if failure




//GD variables
gdImagePtr gd_image;								//gd image
int gd_image_w=-1, gd_image_h=-1;		//gd image size
const int gd_char_w=4; 							//gd image char width
const int gd_string_padding=15;			//gd image padding width
int gd_col_black, gd_col_white, gd_col_gray, gd_col_darkgray, gd_col_darkergray, gd_col_green, gd_col_tmp, gd_col_text; //gd colors

char gd_icons[]={
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
0,0,1,0,1,0,1,0};
gdFont gd_icons_8x8_font_ref = {3,0,8,8,gd_icons};
gdFontPtr gd_icons_8x8_font = &gd_icons_8x8_font_ref;

int gd_x_temp, gd_x_vbat, gd_x_cputemp, gd_x_cpuload, gd_x_wifi, gd_x_time; //gd x text position
int gd_wifi_charcount, gd_vbat_charcount, gd_cpu_charcount, gd_cpuload_charcount, gd_time_charcount; //gd text char count
char gd_vbat_chararray[26];				//battery voltage gd string
char gd_cpu_chararray[15];				//cpu gd string
char gd_cpuload_chararray[10];		//cpu load gd string
char gd_wifi_chararray[20];				//wifi string
char gd_time_chararray[10];				//time gd string
int cpu_value=0;									//cpu temp
long double a[4], b[4];						//use to compute cpu load
char cpu_buf[7];									//cpu read buffer
int cpuload_value=0;							//cpu load
int uptime_value=0;								//uptime value
int wifi_linkspeed=0;							//wifi link speed
char pbuffer[20];									//buffer use to read process pipe
time_t now; 											//current date/time
tm *ltime; 												//localtime object


//Battery variables
float vbat_value=0.;					//battery voltage, used as backup if read fail
float vbatlow_value=-1;				//battery low voltage
int battery_percent=-1;				//battery percentage



void show_usage(void){
	printf(
"Version: %s\n"
"Example with battery: ./info2png -i2cbus \"/dev/i2c-1\" -i2caddress 0x4d -adcvref 3.65 -adcres 4096 -r1value 91 -r2value 220 -vbatlow 3.5 -vbatlogging -width 304 -height 10 -o \"/dev/shm\"\n"
"Example without battery: ./info2png -width 304 -height 10 -o \"/dev/shm\"\n"
"Options:\n"
"\t-i2cbus, path to i2c bus device [Optional, needed only for battery voltage monitoring]\n"
"\t-i2caddress, i2c device adress, found via 'i2cdetect' [Optional, needed only for battery voltage monitoring]\n"
"\t-adcvref, in volt, vref of the adc chip [Optional, needed only for battery voltage monitoring]\n"
"\t-adcres, ADC resolution: 256=8bits, 1024=10bits, 4096=12bits (default), 65535=16bits [Optional, needed only for battery voltage monitoring]\n"
"\t-r1value, in ohm [Optional, used for battery voltage, disable resistor divider if not set]\n"
"\t-r2value, in ohm [Optional, used for battery voltage, disable resistor divider if not set]\n"
"\t-vbatlow, in volt, low battery voltage to set text in red color [Optional, used for battery voltage monitoring]\n"
"\t-vbatlogging, enable battery voltage logging, data will be put in 'vbat-start.log', format 'uptime;vbat' [Optional, used for battery voltage monitoring]\n"
"\t-width, in px, width of 'fb_footer.png' [Optional, needed for generate png if path to freeplayfbcp.cfg not provided]\n"
"\t-height, in px, height of 'fb_footer.png' [Optional, needed for generate png]\n"
"\t-interval, [Optional] drawing interval in sec\n"
"\t-ip, [Optional] display IP address instead of link speed\n"
"\t-notime, [Optional] disable display of time\n"
"\t-freeplaycfg, [Optional] usually \"/boot/freeplayfbcp.cfg\", provide data like screen width\n"
"\t-o, output folder where 'vbat.log', 'vbat-start.log', 'vbat.srt'  and 'fb_footer.png'\n\n"
"Resistor divider diagram:\n"
"\t(Battery) Vin--+\n"
"\t               R1\n"
"\t               +-----Vout (ADC)\n"
"\t               R2\n"
"\t(Battery) Gnd--+-----Gnd (Avoid if battery power ADC chip)\n"
,programversion);
	
}

int main(int argc, char* argv[]){
	if(argc<3){show_usage();return 1;} //wrong arguments count
	
	for(int i=1;i<argc;++i){ //argument to variable
		if(strcmp(argv[i],"-help")==0){show_usage();return 1;
		}else if(strcmp(argv[i],"-i2cbus")==0){i2c_bus=(char*)argv[i+1]; if(access(i2c_bus,R_OK)!=0){printf("info2png : Failed, %s not readable\n",i2c_bus);return 1;}
		}else if(strcmp(argv[i],"-i2caddress")==0){sscanf(argv[i+1], "%x", &i2c_address);/*htoi(argv[i+1])*/;
		}else if(strcmp(argv[i],"-adcvref")==0){adc_vref=atof(argv[i+1]);
		}else if(strcmp(argv[i],"-adcres")==0){adc_resolution=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-r1value")==0){divider_r1=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-r2value")==0){divider_r2=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-vbatlow")==0){vbatlow_value=atof(argv[i+1]);
		}else if(strcmp(argv[i],"-vbatlogging")==0){battery_log_enabled=true;
		}else if(strcmp(argv[i],"-width")==0){gd_image_w=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-height")==0){gd_image_h=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-interval")==0){update_interval=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-ip")==0){wifi_showip=true;
		}else if(strcmp(argv[i],"-notime")==0){time_enabled=false;
		}else if(strcmp(argv[i],"-freeplaycfg")==0){freeplaycfg_path=(char*)argv[i+1]; if(access(freeplaycfg_path,R_OK)!=0){printf("info2png : Failed, %s not readable\n",freeplaycfg_path);return 1;}
		}else if(strcmp(argv[i],"-o")==0){data_output_path=(char*)argv[i+1]; if(access(data_output_path,W_OK)!=0){printf("info2png : Failed, %s not writable\n",data_output_path);return 1;}}
	}
	
	if(i2c_address<=0||adc_vref<=0||adc_resolution<=0||i2c_bus==NULL){ //user miss some arguments for battery
		battery_enabled=false; battery_log_enabled=false; printf("info2png : Warning, battery monitoring disable, some arguments needed to get battery data are not set.\n");
	}else{battery_set=true;} //all informations are set for battery probe, use in case of read failure to retry

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
	
	if((divider_r1==0||divider_r2==0)&&battery_set){resistor_divider_enabled=false;printf("info2png : Warning, ADC resistor divider compute disable, resistor values are not set.\n");} //user miss some arguments for resistor values
	
	if(data_output_path==NULL){printf("info2png : Failed, missing output path\n");show_usage();return 1;} //user miss some needed arguments
	if(gd_image_w<1||gd_image_h<1){printf("info2png : Warning, PNG output disable, missing image width or height.\n");png_enabled=false;} //no png output
	if(vbatlow_value<0&&battery_enabled&&png_enabled){printf("info2png : Warning, low battery voltage not set, text color will stay unchanged\n");} //user miss some arguments for battery
	
	if(update_interval<1){printf("info2png : Warning, wrong update interval set, setting it to 15sec\n");update_interval=15;} //wrong interval
	if(battery_log_enabled&&battery_set){printf("info2png : Battery logging enable\n");}
	
	if(access("/sbin/iw",F_OK)!=0){printf("info2png : Warning, WIFI link speed detection require 'iw' software\n");}
	
	if(!time_enabled){printf("info2png : Time display disable\n");}
	
	while(true){
		chdir(data_output_path);							//change directory to output path
		
		//-----------------------------Start of I2C part
		if(battery_set){ //all need for battery monitoring is set
			adc_read_retry=0; vbat_value=0; //reset variables
			battery_enabled=false; //battery not enable by default
			while(adc_read_retry<3){ //use a loop in case of reading failure
				if((i2c_handle=open(i2c_bus,O_RDWR))<0){ //open i2c bus
					printf("info2png : Failed to open the I2C bus : %s\n",i2c_bus);
					adc_read_retry=3; //no need to retry since failed to open I2C bus itself
				}else{
					if(ioctl(i2c_handle,I2C_SLAVE,i2c_address)<0){ //access i2c device, allow retry if failed
						printf("info2png : Failed to access I2C device : %04x, retry in 1sec\n",i2c_address);
					}else{ //success
						if(read(i2c_handle,i2c_buffer,2)!=2){ //start reading data from i2c device, allow retry if failed
							printf("info2png : Failed to read data from I2C device : %04x, retry in 1sec\n",i2c_address);
						}else{ //success
							adc_raw_value=(i2c_buffer[0]<<8)|(i2c_buffer[1]&0xff); //combine buffer bytes into integer
							if(resistor_divider_enabled){vbat_value=adc_raw_value*(float)(adc_vref/adc_resolution)/(float)(divider_r2/(float)(divider_r1+divider_r2)); //compute battery voltage with resistor divider
							}else{vbat_value=adc_raw_value*(float)(adc_vref/adc_resolution);} //compute battery voltage only with adc vref
							if(vbat_value<1){printf("info2png : Warning, voltage read from ADC chip < 1 volt, Probing failed\n");
							}else{ //success
								battery_enabled=true; //battery voltage read success
								temp_filehandle=fopen("vbat.log","wb"); fprintf(temp_filehandle,"%.3f",vbat_value); fclose(temp_filehandle); //write log file
								
								if(battery_log_enabled){ //cumulative cumulative log file
									temp_filehandle=fopen("/proc/uptime","r"); fscanf(temp_filehandle,"%u",&uptime_value); fclose(temp_filehandle); //get system uptime
									temp_filehandle=fopen("vbat-start.log","a+"); fprintf(temp_filehandle,"%u;%.3f\n",uptime_value,vbat_value); fclose(temp_filehandle); //write cumulative log file
								}
							}
						}
					}
					close(i2c_handle);
				}
				
				if(!battery_enabled){
					adc_read_retry++; //something failed at one point so retry
					if(adc_read_retry>2){printf("info2png : Warning, voltage read from ADC chip fail 3 times, skipping until next update\n");}else{sleep(1);}
				}else{adc_read_retry=3;} //data read with success, no retry
			}
		}

		//-----------------------------Start of GD part
		if(png_enabled){ //png output enable
			wifi_enabled=false;
			if(access("/sbin/iw",F_OK)!=-1 && !wifi_showip){ //check if 'iw' is installed for WiFi link speed
				temp_filehandle=popen("iw dev wlan0 link 2> /dev/null | grep bitrate | cut -f 2 -d \":\" | cut -f 1 -d \"M\"", "r"); //open process pipe
				if(temp_filehandle!=NULL){ //if process not fail
					if(fgets(pbuffer,9,temp_filehandle)){ //if output something
						if(strlen(pbuffer)>0){ //no output, no connection
				  		wifi_linkspeed=atoi(pbuffer); //convert output to int
				  		if(wifi_linkspeed>0){wifi_enabled=true; gd_wifi_charcount=sprintf(gd_wifi_chararray,"%iMBit/s",wifi_linkspeed);} //if value can be valid
			  		}
			  	}
			  	pclose(temp_filehandle); //close process pipe
				}
			}else{ //'iw' is not installed, show ip address instead
				temp_filehandle=popen("hostname -I | awk '{printf \"%s\",$1}'", "r");	//open process pipe
				if(temp_filehandle!=NULL){ //if process not fail
					if(fgets(pbuffer,sizeof(gd_wifi_chararray),temp_filehandle)){ //if output something
						gd_wifi_charcount=sprintf(gd_wifi_chararray,"%s",pbuffer);
			  		if(strcmp(gd_wifi_chararray,"127.0.0.1")==0){wifi_showip=false;} //if ip is 127.0.0.1, disable
			  	}
			  	pclose(temp_filehandle); //close process pipe
				}
			}
			
			gd_image=gdImageCreateTrueColor(gd_image_w,gd_image_h);																														//allocate gd image
			
			gd_col_black=gdImageColorAllocate(gd_image, 0, 0, 0);																															//since it is the first color declared, it will be the background
			gd_col_white=gdImageColorAllocate(gd_image, 255, 255, 255);																												//declarate white color
			gd_col_gray=gdImageColorAllocate(gd_image, 128, 128, 128);																												//declarate gray color
			gd_col_darkgray=gdImageColorAllocate(gd_image, 64, 64, 64);																												//declarate dark gray color
			gd_col_darkergray=gdImageColorAllocate(gd_image, 40, 40, 40);																											//declarate darker gray color
			gd_col_green=gdImageColorAllocate(gd_image, 0, 255, 0);																														//declarate green color
			
			
			for(int i=-gd_image_h;i<gd_image_w;i+=6){gdImageLine(gd_image,i,0,i+gd_image_h,gd_image_h,gd_col_darkergray);}				//background decoration
			
			
			//battery voltage char array
			if(battery_enabled){
				battery_percent=nns_get_battery_percentage((int)(vbat_value*1000));																				//try to get battery percentage
				//gd_vbat_charcount=sprintf(gd_vbat_chararray,"Battery: %d%% (%.2fv)",battery_percent,vbat_value);																	//prepare char array to render
				gd_vbat_charcount=sprintf(gd_vbat_chararray,"%d%% (%.2fv)",battery_percent,vbat_value);																	//prepare char array to render
				gd_x_vbat=9+gd_char_w/2;																																										//gd x position for battery voltage
				gd_x_cputemp=gd_x_vbat+gd_string_padding+gd_vbat_charcount*(gd_char_w+1);																	//gd x position for cpu temp
				if(vbatlow_value<0){ 																																																	//low battery voltage not set
					gd_col_text=gd_col_green; 																																														//allocate gd color (green)
				}else{
					gd_col_tmp=rgbcolorstep(vbat_value,vbatlow_value,4.2,(int)0x00ff0000,(int)0x0000ff00);																//compute int color
					gd_col_text=gdImageColorAllocate(gd_image,(gd_col_tmp>>16)&0x0FF,(gd_col_tmp>>8)&0x0FF,(gd_col_tmp>>0)&0x0FF);				//allocate gd color
				}
				
				//battery voltage render
				gdImageChar(gd_image,gd_icons_8x8_font,gd_x_vbat-9,1,0x00,gd_col_text); //draw battery icon
				gdImageString(gd_image,gdFontTiny,gd_x_vbat,1,(unsigned char*)gd_vbat_chararray,gd_col_text);													//print battery info to gd image
				gdImageLine(gd_image,gd_x_cputemp-1-gd_string_padding/2,1,gd_x_cputemp-1-gd_string_padding/2,gd_image_h-2,gd_col_darkgray); //draw separator
			}else{gd_x_cputemp=gd_char_w/2;}																																//gd x position for cpu temp if not battery probe
			
			
			//cpu temp char array
			temp_filehandle=fopen("/sys/class/thermal/thermal_zone0/temp","r"); 																									//open sys file
			fgets(cpu_buf, sizeof(cpu_buf),temp_filehandle);																																				//read value
			fclose(temp_filehandle);																																																//close sys file
			cpu_value=atoi(cpu_buf)/1000;																																														//compute temperature
			//gd_cpu_charcount=sprintf(gd_cpu_chararray,"CPU:%i°c",cpu_value); 																							//prepare char array to render
			gd_cpu_charcount=sprintf(gd_cpu_chararray,"%i°c",cpu_value); 																							//prepare char array to render
			
			//cpu load char array - code from https://stackoverflow.com/questions/3769405/determining-cpu-utilization
		  temp_filehandle=fopen("/proc/stat","r");
		  fscanf(temp_filehandle,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
		  fclose(temp_filehandle);
		  sleep(1);
		  temp_filehandle=fopen("/proc/stat","r");
		  fscanf(temp_filehandle,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
		  fclose(temp_filehandle);
		  cpuload_value=(((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])))*100;
			gd_cpuload_charcount=sprintf(gd_cpuload_chararray,"%3i%%",cpuload_value); 																			//prepare char array to render
			
			//cpu temp render
			gd_col_tmp=rgbcolorstep(cpu_value,50,80,(int)0x0000ff00,(int)0x00ff0000); 																							//compute int color
			gdImageChar(gd_image,gd_icons_8x8_font,gd_x_cputemp,0,0x01,gd_col_tmp); //draw cpu icon
			gd_x_cputemp+=9;
			gd_x_cpuload=gd_x_cputemp+gd_cpu_charcount*(gd_char_w+1)+(gd_char_w+2);																			//gd x position for cpu load
			gd_x_temp=gd_x_cpuload+gd_cpuload_charcount*(gd_char_w+1);																									//gd x position for cpu temp
			gd_col_text=gdImageColorAllocate(gd_image,(gd_col_tmp>>16)&0x0FF,(gd_col_tmp>>8)&0x0FF,(gd_col_tmp>>0)&0x0FF); 					//allocate gd color
			gdImageString(gd_image,gdFontTiny,gd_x_cputemp,1,(unsigned char*)gd_cpu_chararray,gd_col_text);													//print cpu temp to gd image
			gdImageString(gd_image,gdFontTiny,gd_x_cpuload-(gd_char_w+2),1,(unsigned char*)"/",gd_col_gray);												//print cpu separator to gd image
			
			//cpu load render
			gd_col_tmp=rgbcolorstep(cpuload_value,25,100,(int)0x0000ff00,(int)0x00ff0000); 																					//compute integer color
			gd_col_text=gdImageColorAllocate(gd_image,(gd_col_tmp>>16)&0x0FF,(gd_col_tmp>>8)&0x0FF,(gd_col_tmp>>0)&0x0FF); 					//allocate gd color
			if(cpuload_value<100&&cpuload_value>=10){gdImageString(gd_image,gdFontTiny,gd_x_cpuload,1,(unsigned char*)"0",gd_col_gray);	//print cpu load gray 0 padding
			}else if(cpuload_value<10){gdImageString(gd_image,gdFontTiny,gd_x_cpuload,1,(unsigned char*)"00",gd_col_gray);}					//print cpu load gray 00 padding
			gdImageString(gd_image,gdFontTiny,gd_x_cpuload,1,(unsigned char*)gd_cpuload_chararray,gd_col_text); 										//print cpu load
			gdImageLine(gd_image,gd_x_temp+1+gd_string_padding/2,1,gd_x_temp+1+gd_string_padding/2,gd_image_h-2,gd_col_darkgray); 	//draw separator
			
			
			//time render
			if(time_enabled){
				now=time(0); ltime=localtime(&now);		//current date/time, localtime object
				gd_time_charcount=sprintf(gd_time_chararray,"%02i:%02i",ltime->tm_hour,ltime->tm_min); 												//prepare char array to render
				gd_x_time=gd_image_w-gd_char_w/2-gd_time_charcount*(gd_char_w+1);										//gd x position for time
				gdImageLine(gd_image,gd_x_time-1-gd_string_padding/2,1,gd_x_time-1-gd_string_padding/2,gd_image_h-2,gd_col_darkgray); 				//draw separator
				gdImageString(gd_image,gdFontTiny,gd_x_time,1,(unsigned char*)gd_time_chararray,gd_col_white); 													//print time
			}else{gd_x_time=gd_image_w+gd_string_padding/2;}
			
			//wifi render
			if(wifi_enabled||wifi_showip){
				gd_x_wifi=gd_x_time-gd_string_padding-gd_wifi_charcount*(gd_char_w+1);																		//gd x position for wifi link speed
				if(!wifi_showip){gd_x_wifi-=9;gdImageChar(gd_image,gd_icons_8x8_font,gd_x_wifi,0,0x02,gd_col_white);} //draw wifi icon
				gdImageLine(gd_image,gd_x_temp+1+gd_string_padding/2,(gd_image_h/2)-1,gd_x_wifi-1-gd_string_padding/2,(gd_image_h/2)-1,gd_col_darkgray); //filler
				gdImageLine(gd_image,gd_x_wifi-1-gd_string_padding/2,1,gd_x_wifi-1-gd_string_padding/2,gd_image_h-2,gd_col_darkgray); 			//draw separator
				if(!wifi_showip){gd_x_wifi+=9;}
				gdImageString(gd_image,gdFontTiny,gd_x_wifi,1,(unsigned char*)gd_wifi_chararray,gd_col_white); 												//print wifi link speed
			}else{
				gdImageLine(gd_image,gd_x_temp+1+gd_string_padding/2,(gd_image_h/2)-1,gd_x_time-1-gd_string_padding/2,(gd_image_h/2)-1,gd_col_darkgray); //filler
			}
			
			gdImageLine(gd_image,0,gd_image_h-1,gd_image_w,gd_image_h-1,gd_col_gray); 				//bottom decoration
			
			
			
			
			/* rgb-hsl debug
			for(int i=-gd_image_h;i<gd_image_w;i++){
				gd_col_tmp=rgbcolorstep(i,0,gd_image_w,(int)0x00ff0000,(int)0x00ff0001);
				gdImageLine(gd_image,i,0,i,gd_image_h,gd_col_tmp);
			}
			*/
			
			//nns_gd_drawicon(gd_image,gd_icon_battery,1,1,gd_col_white);
			
			temp_filehandle=fopen("fb_footer.png","wb"); 																																					//open image file
			gdImagePng(gd_image,temp_filehandle);																																										//output gd image to file
			fclose(temp_filehandle);																																																//close image file
			gdImageDestroy(gd_image);																																																//free gd image memory
		}
		
		sleep(update_interval); //sleep
	}
	
	return 0;
}