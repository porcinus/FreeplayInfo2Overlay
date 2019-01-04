/*
NNS @ 2018
info2png
It create a PNG/log file contening CPU load and temperature, Wifi link speed and time, Battery voltage is optional.
*/
const char programversion[]="0.1d";

 /* Bring in gd library functions */
#include "gd.h"
#include <gdfontt.h> /*on va utiliser la police gdFontTiny */

/* Bring in standard I/O so we can output the PNG to a file */
#include <stdio.h>
#include <cstring>

#include <unistd.h>				//Needed for I2C port
#include <fcntl.h>				//Needed for I2C port
#include <sys/ioctl.h>			//Needed for I2C port
#include <linux/i2c-dev.h>		//Needed for I2C port

#include <ctime>
#include <locale.h>
#include <limits.h>

/*//benchmark
   #include <sys/time.h>
    typedef unsigned long long timestamp_t;

    static timestamp_t
    get_timestamp ()
    {
      struct timeval now;
      gettimeofday (&now, NULL);
      return  now.tv_usec + (timestamp_t)now.tv_sec * 1000000;
    }
*/





int nns_map(float x,float in_min,float in_max,int out_min,int out_max){
  if(x<in_min){return out_min;}
  if(x>in_max){return out_max;}
  return (x-in_min)*(out_max-out_min)/(in_max-in_min)+out_min;
}

int rgbcolorstep(float x,float in_min,float in_max,int color_min,int color_max){
	char color_final_r=nns_map(x,in_min,in_max,(color_min>>16)&0x0FF,(color_max>>16)&0x0FF);
	char color_final_g=nns_map(x,in_min,in_max,(color_min>>8)&0x0FF,(color_max>>8)&0x0FF);
	char color_final_b=nns_map(x,in_min,in_max,(color_min>>0)&0x0FF,(color_max>>0)&0x0FF);
	return ((color_final_r&0x0ff)<<16)|((color_final_g&0x0ff)<<8)|(color_final_b&0x0ff);
}


 

//GD
char *vbat_output_path;					//path to battery voltage output path

//I2C 
char *i2c_bus;									//path to i2c bus
int i2c_address=-1;							//i2c device adress, found via 'i2cdetect'
float adc_vref=-1;							//in volt, vdd of the adc chip
int adc_resolution=-1;					//256:8bits, 1024:10bits, 4096:12bits, 65535:16bits

int divider_r1=-1;							//in ohm
int divider_r2=-1;							//in ohm

float vbatlow_value = -1;				//battery low voltage

int gd_image_w = -1;						//gd image width
int gd_image_h = -1;						//gd image height

int draw_interval=-1; //draw interval interval

/*######################## Resistor divider diagram
# (Battery) Vin--+
#                R1
#                +-----Vout (ADC)
#                R2
# (Battery) Gnd--+-----Gnd (Avoid if battery power ADC chip)
#########################*/



FILE *temp_filehandle;						//file handle to get cpu temp/usage
int i2c_handle;           				//i2c handle io
char i2c_buffer[10] = {0};				//i2c data buffer
int adc_value = 0;								//adc step value
float vbat_tmp_value = 0.;				//temporary battery voltage
float vbat_value = 0.;						//battery voltage
gdImagePtr gd_image;							//gd image
const int gd_char_w = 4; 					//gd image char width
const int gd_string_padding = 15; //gd image padding width
int gd_col_black,gd_col_white,gd_col_gray,gd_col_darkgray,gd_col_darkergray,gd_col_green,gd_col_tmp,gd_col_text; //declarate gd color
int gd_x_temp,gd_x_vbat,gd_x_cputemp,gd_x_cpuload,gd_x_wifi,gd_x_time; 	//declare gd x position
int gd_wifi_charcount,gd_vbat_charcount,gd_cpu_charcount,gd_cpuload_charcount,gd_time_charcount;
char gd_vbat_chararray[20];				//battery voltage gd string
char gd_cpu_chararray[15];					//cpu gd string
char gd_cpuload_chararray[10];			//cpu load gd string
char gd_wifi_chararray[20];				//wifi string
char gd_time_chararray[10];				//time gd string
int cpu_value = 0;								//cpu temp
long double a[4], b[4];						//use to compute cpu load
char cpu_buf[7];									//cpu read buffer
int cpuload_value = 0;						//cpu load
bool battery_enabled = true;			//battery probe boolean
bool battery_log_enabled = false;	//battery log from start boolean
bool png_enabled = true;					//png output boolean
int uptime_value = 0;							//uptime value
bool wifi_enabled = false;				//wifi boolean
bool wifi_showip = false;					//ip address instead of link speed
int wifi_linkspeed = 0;						//wifi link speed
char pbuffer[20];									//buffer use to read process pipe



time_t now; 											//current date/time
tm *ltime; 												//localtime object

void show_usage(void){
	printf("Example with battery: ./info2png -i2cbus \"/dev/i2c-1\" -i2caddress 0x4d -adcvref 3.65 -adcres 4096 -r1value 91 -r2value 220 -vbatlow 3.5 -vbatlogging -width 304 -height 10 -o \"/dev/shm\"\n");
	printf("Example without battery: ./info2png -width 304 -height 10 -o \"/dev/shm\"\n");
	printf("Version: %s\n",programversion);
	printf("Options:\n");
	printf("\t-i2cbus, path to i2c bus device [Optional, used for battery voltage]\n");
	printf("\t-i2caddress, i2c device adress, found via 'i2cdetect' [Optional, used for battery voltage]\n");
	printf("\t-adcvref, in volt, vdd of the adc chip [Optional, used for battery voltage]\n");
	printf("\t-adcres, ADC resolution: 256=8bits, 1024=10bits, 4096=12bits, 65535=16bits [Optional, used for battery voltage]\n");
	printf("\t-r1value, in ohm [Optional, used for battery voltage]\n");
	printf("\t-r2value, in ohm [Optional, used for battery voltage]\n");
	printf("\t-vbatlow, in volt, low battery voltage to set text in red color [Optional, used for battery voltage]\n");
	printf("\t-vbatlogging, enable battery voltage logging, data will be put in 'vbat-start.log', format 'uptime;vbat' [Optional, used for battery voltage monitoring]\n");
	printf("\t-width, in px, width of 'fb_footer.png' [Optional, used for generate png]\n");
	printf("\t-height, in px, height of 'fb_footer.png' [Optional, used for generate png]\n");
  printf("\t-interval, [Optional] drawing interval in sec\n");
  printf("\t-ip, [Optional] display IP address instead of link speed\n");
	printf("\t-o, output folder where 'vbat.log', 'vbat-start.log', 'vbat.srt'  and 'fb_footer.png'\n\n");
	
	
	
	
	printf("Resistor divider diagram:\n");
	printf("\t(Battery) Vin--+\n");
	printf("\t               R1\n");
	printf("\t               +-----Vout (ADC)\n");
	printf("\t               R2\n");
	printf("\t(Battery) Gnd--+-----Gnd (Avoid if battery power ADC chip)\n");
}

int main(int argc, char* argv[]){
	if(argc<3){show_usage();return 1;} //wrong arguments count
	
	for(int i=1;i<argc;++i){ //argument to variable
		if(strcmp(argv[i],"-help")==0){show_usage();return 1;
		}else if(strcmp(argv[i],"-i2cbus")==0){i2c_bus=(char*)argv[i+1]; if(access(i2c_bus,R_OK)!=0){printf("info2png : Failed, %s not readable\n",i2c_bus);battery_enabled=false;}
		}else if(strcmp(argv[i],"-i2caddress")==0){sscanf(argv[i+1], "%x", &i2c_address);/*htoi(argv[i+1])*/;
		}else if(strcmp(argv[i],"-adcvref")==0){adc_vref=atof(argv[i+1]);
		}else if(strcmp(argv[i],"-adcres")==0){adc_resolution=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-r1value")==0){divider_r1=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-r2value")==0){divider_r2=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-vbatlow")==0){vbatlow_value=atof(argv[i+1]);
		}else if(strcmp(argv[i],"-vbatlogging")==0){battery_log_enabled=true;
		}else if(strcmp(argv[i],"-width")==0){gd_image_w=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-height")==0){gd_image_h=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-interval")==0){draw_interval=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-ip")==0){wifi_showip=true;
		}else if(strcmp(argv[i],"-o")==0){vbat_output_path=(char*)argv[i+1]; if(access(vbat_output_path,W_OK)!=0){printf("info2png : Failed, %s not writable\n",vbat_output_path);return 1;}}
	}

	if(i2c_address<0||adc_vref<0||adc_resolution<0||divider_r1<0||divider_r2<0){battery_enabled=false;battery_log_enabled=false;printf("info2png : Warning, some arguments needed to get battery data are not set, battery monitoring disable\n");} //user miss some arguments for battery
	if(vbatlow_value<0){printf("info2png : Warning, low battery voltage not set, text color will stay unchanged\n");} //user miss some arguments for battery
	
	if(vbat_output_path==NULL){printf("info2png : Failed, missing output path\n");show_usage();return 1;} //user miss some needed arguments
	if(gd_image_w<1){printf("info2png : Warning, missing image width, png output disable\n");png_enabled=false;} //no png output
	if(gd_image_h<1){printf("info2png : Warning, missing image height, png output disable\n");png_enabled=false;} //no png output
		
		
	if(draw_interval<1){printf("info2png : Warning, wrong draw interval set, setting it to 15sec\n");draw_interval=15;} //wrong interval
	if(battery_log_enabled&&battery_enabled){printf("info2png : Battery logging enable\n");}
	
	if(access("/sbin/iw",F_OK)!=0){printf("info2png : Warning, WIFI link speed detection require 'iw' software\n");}
	
	
	
	
	while(true){
		//timestamp_t t0 = get_timestamp(); //benchmark
		
		chdir(vbat_output_path); //change directory
		now = time(0); 						//current date/time
		ltime = localtime(&now); 			//localtime object
		
		if(battery_enabled){
			//-----------------------------Start of I2C part
			if((i2c_handle=open(i2c_bus,O_RDWR))<0){printf("info2png : Failed to open the i2c bus : %s\n",i2c_bus);battery_enabled=false;}else{							//open i2c bus
				if(ioctl(i2c_handle,I2C_SLAVE,i2c_address)<0){printf("info2png : Failed to get bus access : %04x\n",i2c_address);battery_enabled=false;}else{	//access i2c device
					if(read(i2c_handle,i2c_buffer,2)!=2){printf("info2png : Failed to read data from the i2c bus\n");battery_enabled=false;;											//start reading data from i2c device
					}else{
						adc_value=(i2c_buffer[0]<<8)|(i2c_buffer[1]&0xff);																																							//combine buffer bytes into integer
						vbat_tmp_value=adc_value*(float)(adc_vref/adc_resolution)/(float)(divider_r2/(float)(divider_r1+divider_r2));										//compute battery voltage
						if(vbat_tmp_value<1){printf("info2png : Warning, voltage < 1 volt, Probing failed\n");}else{vbat_value=vbat_tmp_value;}										//security
						
						temp_filehandle = fopen("vbat.log","wb"); 																																											//open log file
						fprintf(temp_filehandle,"%.2f",vbat_value);																																											//write log
						fclose(temp_filehandle);																																																				//close log file
						if(battery_log_enabled&&vbat_value>1){
							temp_filehandle = fopen("/proc/uptime","r");																																									//open sys file
							fscanf(temp_filehandle,"%u",&uptime_value);																																										//read uptime value
							fclose(temp_filehandle);																																																			//close sys file
							
							temp_filehandle = fopen("vbat-start.log","a+");																																								//open log file
							fprintf(temp_filehandle,"%u;%.2f\n",uptime_value,vbat_value);																																	//write log
							fclose(temp_filehandle);																																																			//close log file
						}
					}
				}
			}
		}
		
		
		//-----------------------------Start of GD part
		if(png_enabled){ //png output enable
			//Check if WIFI is working
			if(access("/sbin/iw",F_OK)!=-1 && !wifi_showip){ 																																					//check if 'iw' is installed
				temp_filehandle = popen("iw dev wlan0 link 2> /dev/null | grep bitrate | cut -f 2 -d \":\" | cut -f 1 -d \"M\"", "r");	//open process pipe
				if(temp_filehandle!=NULL){																																															//if process not fail
					if(fgets(pbuffer,9,temp_filehandle)){																																									//if output something
			  		wifi_linkspeed=atoi(pbuffer);																																												//convert output to int
			  		if(wifi_linkspeed>0){																																						//if value can be valid
			  			wifi_enabled=true;
			  			gd_wifi_charcount=sprintf(gd_wifi_chararray,"%iMBit/s",wifi_linkspeed);
			  		}
			  	}
			  	pclose(temp_filehandle);																																															//close process pipe
				}
			}else{wifi_showip=true;} //'iw' is not installed, show ip address instead
			
			if(wifi_showip){
				temp_filehandle = popen("hostname -I | awk '{printf \"%s\",$1}'", "r");	//open process pipe
				if(temp_filehandle!=NULL){																																															//if process not fail
					if(fgets(pbuffer,sizeof(gd_wifi_chararray),temp_filehandle)){																													//if output something
						gd_wifi_charcount=sprintf(gd_wifi_chararray,"%s",pbuffer);
			  		if(strcmp(gd_wifi_chararray,"127.0.0.1")==0){wifi_showip=false;}																											//if ip is 127.0.0.1, disable
			  	}
			  	pclose(temp_filehandle);																																															//close process pipe
				}
			}
			
			gd_image = gdImageCreateTrueColor(gd_image_w,gd_image_h);																														//allocate gd image
			
			gd_col_black = gdImageColorAllocate(gd_image, 0, 0, 0);																															//since it is the first color declared, it will be the background
			gd_col_white = gdImageColorAllocate(gd_image, 255, 255, 255);																												//declarate white color
			gd_col_gray = gdImageColorAllocate(gd_image, 128, 128, 128);																												//declarate gray color
			gd_col_darkgray = gdImageColorAllocate(gd_image, 64, 64, 64);																												//declarate dark gray color
			gd_col_darkergray = gdImageColorAllocate(gd_image, 40, 40, 40);																											//declarate darker gray color
			gd_col_green = gdImageColorAllocate(gd_image, 0, 255, 0);																														//declarate green color
			
			
			for(int i=-gd_image_h;i<gd_image_w;i+=6){gdImageLine(gd_image,i,0,i+gd_image_h,gd_image_h,gd_col_darkergray);}				//background decoration
			
			//battery voltage char array
			if(battery_enabled){
				gd_vbat_charcount=sprintf(gd_vbat_chararray,"Battery:%.2fv",vbat_value);																			//prepare char array to render
				gd_x_vbat=gd_char_w/2;																																				//gd x position for battery voltage
				gd_x_cputemp=gd_x_vbat+gd_string_padding+gd_vbat_charcount*(gd_char_w+1);																	//gd x position for cpu temp
				if(vbatlow_value<0){ 																																																	//low battery voltage not set
					gd_col_text=gd_col_green; 																																														//allocate gd color (green)
				}else{
					gd_col_tmp=rgbcolorstep(vbat_value,vbatlow_value,4.2,(int)0x00ff0000,(int)0x0000ff00);																//compute int color
					gd_col_text=gdImageColorAllocate(gd_image,(gd_col_tmp>>16)&0x0FF,(gd_col_tmp>>8)&0x0FF,(gd_col_tmp>>0)&0x0FF);				//allocate gd color
				}
				
				//battery voltage render
				gdImageString(gd_image,gdFontTiny,gd_x_vbat,1,(unsigned char*)gd_vbat_chararray,gd_col_text);													//print battery info to gd image
				gdImageLine(gd_image,gd_x_cputemp-1-gd_string_padding/2,1,gd_x_cputemp-1-gd_string_padding/2,gd_image_h-2,gd_col_darkgray); //draw separator
			}else{gd_x_cputemp=gd_char_w/2;}																																//gd x position for cpu temp if not battery probe
			
			//cpu temp char array
			temp_filehandle = fopen("/sys/class/thermal/thermal_zone0/temp","r"); 																									//open sys file
			fgets(cpu_buf, sizeof(cpu_buf),temp_filehandle);																																				//read value
			fclose(temp_filehandle);																																																//close sys file
			cpu_value=atoi(cpu_buf)/1000;																																														//compute temperature
			gd_cpu_charcount=sprintf(gd_cpu_chararray,"CPU:%i°c",cpu_value); 																							//prepare char array to render
			
			//cpu load char array - code from https://stackoverflow.com/questions/3769405/determining-cpu-utilization
		  temp_filehandle = fopen("/proc/stat","r");
		  fscanf(temp_filehandle,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
		  fclose(temp_filehandle);
		  sleep(1);
		  temp_filehandle = fopen("/proc/stat","r");
		  fscanf(temp_filehandle,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
		  fclose(temp_filehandle);
		  cpuload_value = (((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])))*100;
			gd_cpuload_charcount=sprintf(gd_cpuload_chararray,"%3i%%",cpuload_value); 																			//prepare char array to render
			
			//cpu temp render
			gd_x_cpuload=gd_x_cputemp+gd_cpu_charcount*(gd_char_w+1)+(gd_char_w+2);																			//gd x position for cpu load
			gd_x_temp=gd_x_cpuload+gd_cpuload_charcount*(gd_char_w+1);																									//gd x position for cpu temp
			gd_col_tmp=rgbcolorstep(cpu_value,50,80,(int)0x0000ff00,(int)0x00ff0000); 																							//compute int color
			gd_col_text=gdImageColorAllocate(gd_image,(gd_col_tmp>>16)&0x0FF,(gd_col_tmp>>8)&0x0FF,(gd_col_tmp>>0)&0x0FF); 					//allocate gd color
			gdImageString(gd_image,gdFontTiny,gd_x_cputemp,1,(unsigned char*)gd_cpu_chararray,gd_col_text);													//print cpu temp to gd image
			gdImageString(gd_image,gdFontTiny,gd_x_cpuload-(gd_char_w+2),1,(unsigned char*)"/",gd_col_gray);												//print cpu separator to gd image
			
			//battery voltage render
			gd_col_tmp=rgbcolorstep(cpuload_value,25,100,(int)0x0000ff00,(int)0x00ff0000); 																					//compute integer color
			gd_col_text=gdImageColorAllocate(gd_image,(gd_col_tmp>>16)&0x0FF,(gd_col_tmp>>8)&0x0FF,(gd_col_tmp>>0)&0x0FF); 					//allocate gd color
			if(cpuload_value<100&&cpuload_value>=10){gdImageString(gd_image,gdFontTiny,gd_x_cpuload,1,(unsigned char*)"0",gd_col_gray);	//print cpu load gray 0 padding
			}else if(cpuload_value<10){gdImageString(gd_image,gdFontTiny,gd_x_cpuload,1,(unsigned char*)"00",gd_col_gray);}					//print cpu load gray 00 padding
			gdImageString(gd_image,gdFontTiny,gd_x_cpuload,1,(unsigned char*)gd_cpuload_chararray,gd_col_text); 										//print cpu load
			gdImageLine(gd_image,gd_x_temp+1+gd_string_padding/2,1,gd_x_temp+1+gd_string_padding/2,gd_image_h-2,gd_col_darkgray); 	//draw separator
			
			//time render
			gd_time_charcount=sprintf(gd_time_chararray,"%02i:%02i",ltime->tm_hour,ltime->tm_min); 												//prepare char array to render
			gd_x_time=gd_image_w-gd_char_w/2-gd_time_charcount*(gd_char_w+1);										//gd x position for time
			gdImageLine(gd_image,gd_x_time-1-gd_string_padding/2,1,gd_x_time-1-gd_string_padding/2,gd_image_h-2,gd_col_darkgray); 				//draw separator
			gdImageString(gd_image,gdFontTiny,gd_x_time,1,(unsigned char*)gd_time_chararray,gd_col_white); 													//print time
			
			//wifi render
			if(wifi_enabled||wifi_showip){
				gd_x_wifi=gd_x_time-gd_string_padding-gd_wifi_charcount*(gd_char_w+1);																		//gd x position for wifi link speed
				gdImageLine(gd_image,gd_x_temp+1+gd_string_padding/2,(gd_image_h/2)-1,gd_x_wifi-1-gd_string_padding/2,(gd_image_h/2)-1,gd_col_darkgray); //filler
				gdImageLine(gd_image,gd_x_wifi-1-gd_string_padding/2,1,gd_x_wifi-1-gd_string_padding/2,gd_image_h-2,gd_col_darkgray); 			//draw separator
				gdImageString(gd_image,gdFontTiny,gd_x_wifi,1,(unsigned char*)gd_wifi_chararray,gd_col_white); 												//print wifi link speed
			}else{
				gdImageLine(gd_image,gd_x_temp+1+gd_string_padding/2,(gd_image_h/2)-1,gd_x_time-1-gd_string_padding/2,(gd_image_h/2)-1,gd_col_darkgray); //filler
			}
			
			
			gdImageLine(gd_image,0,gd_image_h-1,gd_image_w,gd_image_h-1,gd_col_gray); 				//bottom decoration
			
			temp_filehandle = fopen("fb_footer.png","wb"); 																																					//open image file
			gdImagePng(gd_image,temp_filehandle);																																										//output gd image to file
			fclose(temp_filehandle);																																																//close image file
			gdImageDestroy(gd_image);																																																//free gd image memory
		}
		
		sleep(draw_interval); //sleep
	}
	
/*//benchmark
timestamp_t t1 = get_timestamp();
	double secs = (t1 - t0) / 1000000.0L;
	printf("%g\n",secs);
*/
	return 0;
}