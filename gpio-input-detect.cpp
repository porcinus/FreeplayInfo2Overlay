/*
NNS @ 2019
gpio-input-detect
Detect a single GPIO input
*/
const char programversion[]="0.1a"; //program version

#include <fcntl.h> //file io
#include <stdio.h> //stream io
#include <stdlib.h> //standard
#include <unistd.h> //standard
#include <cstring> //string
#include <limits.h> //limits
#include <errno.h> //errno
//#include <linux/i2c-dev.h> //i2c library
#include <sys/ioctl.h> //sys io
#include <pthread.h> //pthread
#include <wiringPi.h> //wiringpi
#include <sys/time.h> //time
#include <time.h> //time
#include <limits.h>					//limits



//GPIO variables
pthread_t gpio_thread; //gpio thread id
int gpio_thread_rc=-1; //gpio thread return code
//int gpio_input_trigger=-1;
int gpio_input[55]; //store if gpio pin is input
int gpio_input_activelow[55]; //store if gpio pin is active low
long long gpio_input_timestamp[55]; //store if gpio pin triggered timestamp
bool gpio_input_enable[55]; //if specific gpio pin allow to be monitored
int button_pressed=-1; //last valid gpio pin triggered
int button_pressed_tmp=-1; //backup previous variable
int button_table[]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}; //store user input
int button_table_logic[]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}; //store user input reverse logic
int gpio_button_link_table[]={12,0,1,2,3,6,7,10,9,4,5,11,8,13,14,15,16,17,18,19,20}; //used to match button name and config
int timeout=5; //abort detection after this duration



//General variables
bool debug=false; //debug enable?
char text_config[4096]; //store config file
//char text_config_tmp[4096]; //store config file
char config_path[PATH_MAX]; //full path to config
FILE *temp_filehandle; //file handle to get check if driver loaded





long long timestamp_msec(){ //recover timestamp in msec, https://stackoverflow.com/questions/3756323/how-to-get-the-current-time-in-milliseconds-from-c-in-linux
	struct timeval te;gettimeofday(&te, NULL);long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;return milliseconds;
}


void *gpio_routine(void *){ //GPIO input thread routine
	if(debug){fprintf(stderr,"---Debug : GPIO : Thread (%lu) started\n",gpio_thread);} //debug
	// gpio_thread_rc values : 0:ok, -1:failed, -2:fail because of wiringPi
	
	if(wiringPiSetupGpio()==-1){fprintf(stderr,"GPIO : wiringPi not initialized\n");gpio_thread_rc=-2;} //failed
	
	int gpio_pin; //current gpio pin
	for(int gpio_pin=0;gpio_pin<54;gpio_pin++){ //initial loop to fill empty array
		if(gpio_input_enable[gpio_pin]==true){ //detected in the config file
			gpio_input[gpio_pin]=0; //default value
			if(getAlt(gpio_pin)==0){ //pin is a input
				gpio_input[gpio_pin]=0; //initial value
				if(digitalRead(gpio_pin)){gpio_input_activelow[gpio_pin]=0;}else{gpio_input_activelow[gpio_pin]=1;} //assume low as activelow
				gpio_input_timestamp[gpio_pin]=0; //initialize timestamp
			}
		}
	}
	
	gpio_thread_rc=0; //update thread state
	
	while(gpio_thread_rc>-1){ //loop until fail
		for(int gpio_pin=0;gpio_pin<54;gpio_pin++){ //initial loop to fill array with real data
			if(gpio_input_enable[gpio_pin]==true){ //detected in the config file
				if((!gpio_input_activelow[gpio_pin]&&!digitalRead(gpio_pin))||(gpio_input_activelow[gpio_pin]&&digitalRead(gpio_pin))){ //gpio button is pressed
					if(gpio_input_timestamp[gpio_pin]==0){ //no timestamp
						if(debug){fprintf(stderr,"---Debug : GPIO : %d : %lli : %lli\n",gpio_pin,timestamp_msec(),gpio_input_timestamp[gpio_pin]);} //debug
						gpio_input_timestamp[gpio_pin]=timestamp_msec(); //update msec timestamp if no pressed before
					}else{
						if(gpio_input[gpio_pin]!=1&&button_pressed==-1&&(timestamp_msec()-gpio_input_timestamp[gpio_pin])>50){ //allow to trigger only once, trigger more than 50msec to avoid detect because of noise
							if(debug){fprintf(stderr,"---Debug : GPIO : %d : %lli : %lli\n",gpio_pin,timestamp_msec(),gpio_input_timestamp[gpio_pin]);} //debug
							button_pressed=gpio_pin; //assign pin to pressed
							gpio_input[gpio_pin]=1; //update pin array
							break; //exit for loop
						}
					}
				}else{ //gpio button not pressed
					gpio_input[gpio_pin]=0; //reset pin value
					gpio_input_timestamp[gpio_pin]=0; //reset pin timestamp
				}
			}
		}
		
		usleep(20*1000); //wait 20msec
	}
	
	pthread_cancel(gpio_thread); //close input thread if trouble
	return NULL;
}






int Wait_User_Input(int forbidden_pin,int timeout){ //function to report user input
	long long start=time(NULL); //initial timestamp
	while(true){ //loop until input detected
		if(button_pressed!=-1){ //user pressed a allow key
			button_pressed_tmp=button_pressed; //backup
			button_pressed=-1; //reset gpio button
			if(button_pressed_tmp!=forbidden_pin&&gpio_input_enable[button_pressed_tmp]){return button_pressed_tmp;} //return gpio pin if not a forbidden pin
		}
		
		if(timeout>0&&time(NULL)-start>timeout){return -1;} //return -1 if timeout
		usleep(25*1000); //wait 25msec
	}
}



void show_usage(void){ //usage
	fprintf(stderr,
"Version: %s\n"
"Example : ./gpio-input-detect -debug\n"
"Options:\n"
"\t-debug, enable some debug stuff [Optional]\n"
"\t-configpath, full path to gpio config file [Default: /etc/modprobe.d/mk_arcade_joystick.conf]\n"
"\t-timeout, abort detection after this duration, in sec [Default: 5]\n"

,programversion);
}


int main(int argc, char *argv[]){ //main
	strcpy(config_path,"/etc/modprobe.d/mk_arcade_joystick.conf"); //init
	
	for(int i=1;i<argc;++i){ //argument to variable
		if(strcmp(argv[i],"-help")==0){show_usage();return 1;
		}else if(strcmp(argv[i],"-configpath")==0){strcpy(config_path,argv[i+1]);
		}else if(strcmp(argv[i],"-timeout")==0){timeout=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-debug")==0){debug=true;}
	}
	
	//setbuf(stdout,NULL); //unbuffered stdout to allow rewrite on the same line
	fprintf(stderr,"\033c");

	//start parse gpio driver config
	bool config_read=false;
	if(access(config_path,R_OK)!=0){ //config not exist
		fprintf(stderr,"\033[91m'%s' not found, Exiting\033[0m\n",config_path);
		sleep(5);
		return 1;
	}else{ //config exist
		fprintf(stderr,"\033[92mConfig file : '%s'\033[0m\n",config_path);
		temp_filehandle=fopen(config_path,"r"); //open file
		while(fgets(text_config,4095,temp_filehandle)!=NULL){ //read line by line
    	if(strstr(text_config,"options mk_arcade_joystick_rpi")!=NULL){ //possible valid line
      	if(strchr(text_config,0x23)==NULL){config_read=true;break;} //line not commented, exit read loop
    	}
		}
		fclose(temp_filehandle); //write file
	}
	
	if(config_read){ //possible valid line
		for(int i=0;i<54;i++){gpio_input_enable[i]=false;} //initial loop to disable all input
		
		if(debug){fprintf(stderr,"---Debug : possible line found in '%s'\n",config_path);}
		char* text_config_tmp=strstr(text_config,"gpio="); //detect gpio section
		int pin_tmp;
		if(text_config_tmp!=NULL){ //gpio section exist
			//fprintf(stderr,"%s\n",text_config_tmp+5);
			char* chr_ptr=strtok(text_config_tmp+5,","); //split

			while(chr_ptr!=NULL){
				pin_tmp=atoi(chr_ptr);
				if(pin_tmp!=-1){ //not disable in the config
					pin_tmp=abs(pin_tmp); //avoid negative value
					if(debug){fprintf(stderr,"---Debug : detected pin : %d\n",pin_tmp);}
					gpio_input_enable[pin_tmp]=true; //allow input to be use
				}
				chr_ptr=strtok(NULL,","); //split
			}
		}else{
			fprintf(stderr,"\033[91m'gpio' section missing in '%s', Exiting\033[0m\n",config_path);
			sleep(5);
			return 1;
		}
	}else{
		fprintf(stderr,"\033[91mNo valid line found in '%s', Exiting\033[0m\n",config_path);
		sleep(5);
		return 1;
	}
	
	pthread_create(&gpio_thread, NULL, gpio_routine, NULL); //create routine thread
	sleep(1); while(gpio_thread_rc<0){sleep(1);} //wait until gpio fully initialize
	
	button_pressed=-1; //reset
	fprintf(stderr,"\033[93mPlease press button used to display overlay\033[0m\n");
	fprintf(stderr,"\033[93mWill abort in %dsec if no input\033[0m\n",timeout);
	button_pressed = Wait_User_Input(-2,timeout); //recover user input
	
	if(button_pressed==-1){
		fprintf(stderr,"\033[91mNo input detected after %dsec, Exiting\033[0m\n",timeout);
		gpio_thread_rc=-1; //kill gpio thread
		sleep(2); //allow some time for user to read output
		return(1);
	}else{
		if(!button_table_logic[button_pressed]){
			fprintf(stderr,"\033[92mReversed logic\033[0m\n");
			button_pressed=button_pressed*-1;
		}
		fprintf(stderr,"\033[92mDetected input : %d\033[0m\n",button_pressed);
		printf("%d",button_pressed); //needed to recover value from bash
		gpio_thread_rc=-1; //kill gpio thread
		sleep(2); //allow some time for user to read output
		return(0);
	}
}