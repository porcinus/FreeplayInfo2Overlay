/*
NNS @ 2018
nns-overlay-deamon
Use to create a 'OSD' on program running on gl or dispmanx driver
*/
const char programversion[]="0.1e";

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <limits.h>
#include <time.h>



FILE *temp_filehandle;								//file handle
int gpio_pin = -1;										//gpio pin
int gpio_lowbatpin = -1;							//gpio pin for low battery
char gpio_path[PATH_MAX];							//gpio full path to sysfs
char gpio_lowbatpath[PATH_MAX];				//gpio full path to sysfs for low battery
bool gpio_activelow=false;						//gpio active low
bool gpio_lowbatactivelow=false;			//gpio active low for low battery
bool gpio_reverselogic=false;					//gpio reverselogic
bool gpio_lowbatreverselogic=false;		//gpio reverselogic for low battery
char gpio_buffer[4];									//gpio read buffer
int gpio_interval=-1;									//gpio check interval
int gpio_value;												//gpio value
int gpio_lowbatvalue;									//gpio value for low battery
bool gpio_exported=false;							//gpio is exported?
bool gpio_lowbatexported=false;				//gpio low battery is exported?
char *png_path;												//full path to str file
bool png_exist=false;									//file exist?
char program_path[PATH_MAX];					//full path to this program
//int screen_width=-1;									//screen width
//int bar_height=0;									  	//bar height
int duration = -1;										//video duration
char img2dispmanx_exec_path[PATH_MAX];								//full command line to run img2dispmanx


char icon_overheat_max_exec_path[PATH_MAX];		//full command line to run img2dispmanx cpu-overheat-max
char icon_overheat_warn_exec_path[PATH_MAX];	//full command line to run img2dispmanx cpu-overheat-warning
char icon_lowbat_exec_path[PATH_MAX];					//full command line to run img2dispmanx low-battery
unsigned int icon_overheat_max_start = 0;			//time of last cpu-overheat-max run
unsigned int icon_overheat_warn_start = 0;		//time of last cpu-overheat-warning run
unsigned int tmp_time = 0;										//little opt
int rpi_cpu_temp = 0;													//rpi cpu temperature
unsigned int icon_lowbat_start = 0;						//time of last lowbat run




void show_usage(void){
	printf("Example : ./nns-overlay-deamon -pin 41 -reverselogic -interval 200 -file \"/dev/shm/fb_footer.png\" -duration 5\n");
	printf("Version: %s\n",programversion);
	printf("Options:\n");
	printf("\t-pin, gpio pin use to display OSD\n");
	printf("\t-reverselogic, optional, reverse activelow logic\n");
	printf("\t-interval, optional, pin checking interval in msec\n");
	printf("\t-file, full path to png file, used for OSD\n");
	printf("\t-duration, in sec, used for OSD\n");
	//printf("\t-screenwidth, screen width, optional, used for OSD\n");
	//printf("\t-height, bar height, optional, used for OSD\n");
	printf("\t-lowbatpin, optional, gpio pin used to signal low battery, disable if not set\n");
	printf("\t-lowbatreverselogic, optional, reverse activelow logic for lowbatpin\n");
}

int main(int argc, char *argv[]){
	if(argc<9){show_usage();return 1;} //wrong arguments count
	
	sleep(2);
	
	for(int i=1;i<argc;++i){ //argument to variable
		if(strcmp(argv[i],"-help")==0){show_usage();return 1;
		}else if(strcmp(argv[i],"-pin")==0){gpio_pin=atoi(argv[i+1]);snprintf(gpio_path,sizeof(gpio_path),"/sys/class/gpio/gpio%i/",gpio_pin);
		}else if(strcmp(argv[i],"-reverselogic")==0){gpio_reverselogic=true;
		}else if(strcmp(argv[i],"-lowbatpin")==0){gpio_lowbatpin=atoi(argv[i+1]);snprintf(gpio_lowbatpath,sizeof(gpio_lowbatpath),"/sys/class/gpio/gpio%i/",gpio_lowbatpin);
		}else if(strcmp(argv[i],"-lowbatreverselogic")==0){gpio_lowbatreverselogic=true;
		}else if(strcmp(argv[i],"-interval")==0){gpio_interval=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-file")==0){png_path=(char*)argv[i+1];
		}else if(strcmp(argv[i],"-duration")==0){duration=atoi(argv[i+1]);}
		//}else if(strcmp(argv[i],"-screenwidth")==0){screen_width=atoi(argv[i+1]);
		//}else if(strcmp(argv[i],"-height")==0){bar_height=atoi(argv[i+1]);}
	}
	
	if(gpio_pin<0||/*screen_width<0||*/duration<0){printf("nns-overlay-deamon : Failed, missing some arguments\n");show_usage();return 1;} //user miss some needed arguments
	if(gpio_interval<100||gpio_interval>600){printf("nns-overlay-deamon : Warning, wrong cheking interval set, setting it to 200msec\n");gpio_interval=200;} //wrong interval
	if(gpio_reverselogic){printf("nns-overlay-deamon : Reversed activelow logic\n");}
	
	if(gpio_lowbatpin<0){printf("nns-overlay-deamon : Warning, no pin set to monitor low battery, skipped\n");}else{
	if(gpio_lowbatreverselogic){printf("nns-overlay-deamon : Reversed low battery activelow logic\n");}}
	
	while(!png_exist){
		if(access(png_path,R_OK)!=0){
			printf("nns-overlay-deamon : Failed, %s not readable, retrying\n",png_path);
			sleep(5);
		}else{
			printf("nns-overlay-deamon : %s found\n",png_path);
			png_exist=true;
		}
	}
	
	while(!gpio_exported){ //gpio pin not exported
		if(access(gpio_path,R_OK)!=0){ //gpio not accessible, try to export
			printf("nns-overlay-deamon : %s not accessible, trying export\n",gpio_path); //debug
			temp_filehandle = fopen("/sys/class/gpio/export","wo"); //open file handle
			fprintf(temp_filehandle,"%d", gpio_pin); //write pin number
			fclose(temp_filehandle); //close file handle
			sleep(2); //sleep to avoid spam
		}else{
			gpio_exported=true; //gpio export with success
			printf("nns-overlay-deamon : %s is accessible\n",gpio_path); //debug
		}
	}
	
	if(gpio_lowbatpin>-1){ //low battery enable
		while(!gpio_lowbatexported){ //gpio pin for low battery not exported
			if(access(gpio_lowbatpath,R_OK)!=0){ //gpio not accessible, try to export
				printf("nns-overlay-deamon : %s not accessible, trying export\n",gpio_lowbatpath); //debug
				temp_filehandle = fopen("/sys/class/gpio/export","wo"); //open file handle
				fprintf(temp_filehandle,"%d",gpio_lowbatpin); //write pin number
				fclose(temp_filehandle); //close file handle
				sleep(2); //sleep to avoid spam
			}else{
				gpio_lowbatexported=true; //gpio export with success
				printf("nns-overlay-deamon : %s is accessible\n",gpio_lowbatpath); //debug
			}
		}
	}
	
	strncpy(program_path,argv[0],strlen(argv[0])-19); //backup program path
	//printf("program path:%s\n",program_path);
	if(strcmp(program_path,".")==0){
		getcwd(program_path,sizeof(program_path)); //backup program path
		//printf("program path:%s\n",program_path);
	}
	
	
	
	//check pin direction
	chdir(gpio_path); //change directory to gpio sysfs
	temp_filehandle = fopen("direction","r"); fgets(gpio_buffer,sizeof(gpio_buffer),temp_filehandle); fclose(temp_filehandle); //read gpio direction
	if(strcmp(gpio_buffer,"out")==0){printf("nns-overlay-deamon : Failed, gpio pin direction is %s\n",gpio_buffer);return(1); //check gpio direction
	}/*else{printf("GPIO: direction is %s\n",gpio_buffer);}*/
	
	//check if pin is active low
	temp_filehandle = fopen("active_low","r"); fgets(gpio_buffer,sizeof(gpio_buffer),temp_filehandle); fclose(temp_filehandle); //read gpio active low
	if(strcmp(gpio_buffer,"1")==0){gpio_activelow=true;} //parse gpio active low
	
	
	
	//check low battery pin direction
	if(gpio_lowbatpin>-1){ //low battery enable
		chdir(gpio_lowbatpath); //change directory to gpio sysfs
		temp_filehandle = fopen("direction","r"); fgets(gpio_buffer,sizeof(gpio_buffer),temp_filehandle); fclose(temp_filehandle); //read gpio direction
		if(strcmp(gpio_buffer,"out")==0){printf("nns-overlay-deamon : Failed, gpio low battery pin direction is %s\n",gpio_buffer);return(1); //check gpio direction
		}/*else{printf("GPIO: direction is %s\n",gpio_buffer);}*/
		
		//check if low battery pin is active low
		temp_filehandle = fopen("active_low","r"); fgets(gpio_buffer,sizeof(gpio_buffer),temp_filehandle); fclose(temp_filehandle); //read gpio active low
		if(strcmp(gpio_buffer,"1")==0){gpio_lowbatactivelow=true;} //parse gpio active low
	}
	
	

	
	
	
	
	
	
	
	
	
	
	snprintf(img2dispmanx_exec_path,sizeof(img2dispmanx_exec_path),"timeout %i %s/img2dispmanx -file \"%s\" -width FILL -layer 20000  >/dev/null 2>&1",duration,program_path,png_path/*,screen_width,bar_height*/); //parse command line for img2dispmanx

	snprintf(icon_overheat_max_exec_path,sizeof(icon_overheat_max_exec_path),"%s/img2dispmanx -file \"%s/img/cpu-overheat-max.png\" -x 10 -y 60 -width 64 -layer 20002 -timeout 5 >/dev/null 2>&1 &",program_path,program_path); //parse command line for img2dispmanx
	snprintf(icon_overheat_warn_exec_path,sizeof(icon_overheat_warn_exec_path),"%s/img2dispmanx -file \"%s/img/cpu-overheat-warning.png\" -x 10 -y 60 -width 64 -layer 20001 -timeout 5 >/dev/null 2>&1 &",program_path,program_path); //parse command line for img2dispmanx
	
	if(gpio_lowbatpin>-1){snprintf(icon_lowbat_exec_path,sizeof(icon_lowbat_exec_path),"%s/img2dispmanx -file \"%s/img/low-battery.png\" -x 80 -y 60 -width 64 -layer 20001 -timeout 5 >/dev/null 2>&1 &",program_path,program_path);} //parse command line for img2dispmanx



	while(true){
		
		chdir(gpio_path); //change directory to gpio sysfs
		temp_filehandle = fopen("value","r"); fgets(gpio_buffer,sizeof(gpio_buffer),temp_filehandle); fclose(temp_filehandle); //read gpio value
		gpio_value=atoi(gpio_buffer); //parse gpio value
		if((gpio_value==0&&(!gpio_activelow&&!gpio_reverselogic||gpio_activelow&&gpio_reverselogic))||(gpio_value==1&&(gpio_activelow&&!gpio_reverselogic||!gpio_activelow&&gpio_reverselogic))){ //gpio button pressed
			chdir(program_path); //change directory
			system(img2dispmanx_exec_path); //display overlay, blocking
		}
		
		
		if(gpio_lowbatpin>-1){ //low battery enable
			chdir(gpio_lowbatpath); //change directory to gpio sysfs
			temp_filehandle = fopen("value","r"); fgets(gpio_buffer,sizeof(gpio_buffer),temp_filehandle); fclose(temp_filehandle); //read gpio value
			gpio_lowbatvalue=atoi(gpio_buffer); //parse gpio value
			if((gpio_lowbatvalue==0&&(!gpio_lowbatactivelow&&!gpio_lowbatreverselogic||gpio_lowbatactivelow&&gpio_lowbatreverselogic))||(gpio_lowbatvalue==1&&(gpio_lowbatactivelow&&!gpio_lowbatreverselogic||!gpio_lowbatactivelow&&gpio_lowbatreverselogic))){ //gpio button pressed
				chdir(program_path); //change directory
				system(icon_lowbat_exec_path); //display overlay, non blocking
			}
		}
		
		
		//read rpi temp value
		//temp_filehandle = fopen("/home/temp","r");
		temp_filehandle = fopen("/sys/class/thermal/thermal_zone0/temp","r");
		fscanf(temp_filehandle, "%i", &rpi_cpu_temp);
		fclose(temp_filehandle);
		
		tmp_time=time(NULL);
		if((tmp_time - icon_overheat_warn_start)>5 && rpi_cpu_temp>=80000 && rpi_cpu_temp<85000){ //low overheat
			icon_overheat_warn_start=tmp_time;
			chdir(program_path); //change directory
			system(icon_overheat_warn_exec_path); //display overheat overlay, non blocking
		}
		
		if((tmp_time - icon_overheat_max_start)>5 && rpi_cpu_temp>=85000){ //hot overheat
			icon_overheat_max_start=tmp_time;
			chdir(program_path); //change directory
			system(icon_overheat_max_exec_path); //display overheat overlay, non blocking
		}
		
		usleep(gpio_interval*1000); //sleep
	}
	
	
	
	return(0);
}