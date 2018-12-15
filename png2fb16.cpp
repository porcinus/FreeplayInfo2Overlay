/*
NNS @ 2018
png2fb16 v0.1b
Draw PNG file to 16bits framebuffer.
*/

#include "gd.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
/*
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
    
    
int argb8888torgb565(int argb888){return ((((argb888>>16)&0x0FF)>>3)<<11)|((((argb888>>8)&0x0FF)>>2)<<5)|(((argb888>>0)&0x0FF)>>3);} //function to convert argb8888 to rgb565


char *png_path; //png file path variable
char *framebuffer_path; //framebuffer device path variable
int xoffset=-1; //image print x offset variable
int yoffset=-1; //image print y offset variable
int gd_imagewidth; //gd width variable
int gd_imageheight; //gd height variable
int fb_position; //framebuffer seek position
int tmpcolor16,tmpcolor32; //tmp variable to store color
int draw_interval=-1; //draw interval interval
	
	
int main(int argc, char* argv[]){
	//timestamp_t t0 = get_timestamp();
	
	if(argc<9){ //wrong arguments count
		printf("Usage : ./png2fb16 -i \"png file\" -f \"framebuffer\" -xoffset 100 -yoffset 50\n");
		printf("Options:\n");
		printf("\t-i, PNG file to display on framebuffer, alpha channel is ignored\n");
		printf("\t-interval, optional, drawing interval in sec\n");
		printf("\t-f, Framebuffer path (eg /dev/fb1), only accept 16bpp framebuffer\n");
		printf("\t-xoffset, offset in framebuffer from left\n");
		printf("\t-yoffset, offset in framebuffer from top\n");
		return 1;
	}
	
	sleep(2);
	
	for(int i=1;i<argc;++i){ //argument to variable
		if(strcmp(argv[i],"-i")==0){png_path=(char*)argv[i+1]; if(access(png_path,R_OK)!=0){printf("Failed, %s not readable\n",png_path);return 1;} //png file path
		}else if(strcmp(argv[i],"-interval")==0){draw_interval=atoi(argv[i+1]);
		}else if(strcmp(argv[i],"-f")==0){framebuffer_path=(char*)argv[i+1]; if(access(framebuffer_path,W_OK)){printf("Failed, %s not writable\n",framebuffer_path);return 1;} //framebuffer device path
		}else if(strcmp(argv[i],"-xoffset")==0){xoffset=atoi(argv[i+1]); if(xoffset<0){xoffset=0;} //image print x offset
		}else if(strcmp(argv[i],"-yoffset")==0){yoffset=atoi(argv[i+1]); if(yoffset<0){yoffset=0;}} //image print y offset
	}

	if(xoffset<0||yoffset<0){printf("Failed, offsets are not set\n");return 1;} //user miss offsets
	if(draw_interval<1){printf("Warning, wrong draw interval set, setting it to 15sec\n");draw_interval=15;} //wrong interval
	
	
	
	struct fb_var_screeninfo vinfo; //framebuffer variable information variable
	struct fb_fix_screeninfo finfo; //framebuffer fixed information variable
	int framebuffer_handle = open(framebuffer_path,O_RDWR); //framebuffer handle
	if(framebuffer_handle==-1){printf("Failed to open framebuffer device\n");return 1;} //problem with handle
	if(ioctl(framebuffer_handle,FBIOGET_FSCREENINFO,&finfo)==-1){printf("Failed to read framebuffer fixed information\n");return 1;} //problem with framebuffer fixed information
	if(ioctl(framebuffer_handle,FBIOGET_VSCREENINFO,&vinfo)==-1){printf("Failed to read framebuffer variable information\n");return 1;} //problem with framebuffer variable information
	if(vinfo.bits_per_pixel!=16){printf("Failed, current framebuffer is %dbpp, require 16bpp\n",vinfo.bits_per_pixel);return 1;} //wrong framebuffer bpp
	
	int fb_width=vinfo.xres; int fb_height=vinfo.yres; //get framebuffer size
	
	long int framebuffer_length=vinfo.xres*vinfo.yres*vinfo.bits_per_pixel/8; //compute framebuffer size
	char *framebuffer_map=(char*)mmap(0,framebuffer_length,PROT_READ|PROT_WRITE, MAP_SHARED,framebuffer_handle,0); //map framebuffer device to memory
	if((int)framebuffer_map==-1){printf("Failed to map the framebuffer device to memory\n");return 1;} //problem when mapping framebuffer to memory
	
	gdImagePtr gd_image; //declare gd image
	FILE *filehandle; //declare png file handle
	
	
	
	
	while(true){
		filehandle = fopen(png_path, "rb"); //open png
		gd_image = gdImageCreateFromPng(filehandle); //load into gd
		fclose(filehandle); //close png file handle
		
		gd_imagewidth=gdImageSX(gd_image); //fill gd width variable
		gd_imageheight=gdImageSY(gd_image); //fill gd height variable
		
		
		
		fb_position=0; //framebuffer seek position
		tmpcolor16,tmpcolor32; //tmp variable to store color
	  for(int fb_y=0;fb_y<fb_height;fb_y++){ //framebuffer y loop
		  for(int fb_x=0;fb_x<fb_width;fb_x++,fb_position+=2){ //framebuffer x loop
		  	if(fb_x>=xoffset&&fb_x<(xoffset+gd_imagewidth)&&fb_y>=yoffset&&fb_y<yoffset+gd_imageheight){ //draw if in window
		  		tmpcolor32=gdImageGetTrueColorPixel(gd_image,fb_x-xoffset,fb_y-yoffset); //get pixel color using gd
		  		tmpcolor16=argb8888torgb565(tmpcolor32); //convert color to rgb565
		  		*((unsigned short int*)(framebuffer_map+fb_position))=tmpcolor16; //put color to framebuffer
		  	}
			}
		}
		
		
		
		gdImageDestroy(gd_image); //free gd image
		sleep(draw_interval); //sleep
	}
	
	munmap(framebuffer_map,framebuffer_length); //free framebuffer mapping
	close(framebuffer_handle); //close framebuffer handle
	
	/*
	timestamp_t t1 = get_timestamp();
	double secs = (t1 - t0) / 1000000.0L;
	printf("%g\n",secs);
	*/
	
	
	
	return 0;
}