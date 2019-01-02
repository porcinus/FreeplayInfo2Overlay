/*******************************************
* This file heavily based on https://github.com/hex007/eop
* Most modifications are done on program arguments level and formating
* img2dispmanx v0.1a
*******************************************/

#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/stat.h>
#include <jpeglib.h>
#include <png.h>
#include "bcm_host.h"


typedef struct{uint8_t *buffer; int width; int height; int bpp; int pitch; VC_IMAGE_TYPE_T type;} Image;

#ifndef ALIGN_TO_16
	#define ALIGN_TO_16(x)((x + 15) & ~15)
#endif


static void signalHandler(int signalNumber){}

bool loadJPG(const char *f_name, Image *image){
	int rc, i, j;

	// ------------------------------------------------------------------- SETUP

	// Variables for the source jpg
	struct stat file_info;
	unsigned long jpg_size;
	unsigned char *jpg_buffer;

	// Variables for the decompressor itself
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	// Variables for the output buffer, and how long each row is
	unsigned long bmp_size;
	unsigned char *bmp_buffer;
	int row_stride, width, height, pixel_size;

	rc = stat(f_name, &file_info);
	if (rc) {
		// syslog(LOG_ERR, "FAILED to stat source jpg");
		return false;
	}
	jpg_size = file_info.st_size;
	jpg_buffer = (unsigned char*) malloc(jpg_size + 100);

	int fd = open(f_name, O_RDONLY);
	i = 0;
	while (i < jpg_size) {
		rc = read(fd, jpg_buffer + i, jpg_size - i);
		// syslog(LOG_INFO, "Input: Read %d/%lu bytes", rc, jpg_size-i);
		i += rc;
	}
	close(fd);

	// ------------------------------------------------------------------- START

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	jpeg_mem_src(&cinfo, jpg_buffer, jpg_size);

	rc = jpeg_read_header(&cinfo, TRUE);

	if (rc != 1) {
		// syslog(LOG_ERR, "File does not seem to be a normal JPEG");
		return false;
	}

	jpeg_start_decompress(&cinfo);

	width = cinfo.output_width;
	height = cinfo.output_height;
	pixel_size = cinfo.output_components;

	row_stride = pixel_size * ((width + 15) & ~0x0F);

	bmp_size = height * row_stride;
	bmp_buffer = (unsigned char*) malloc(bmp_size);

	while (cinfo.output_scanline < cinfo.output_height) {
		unsigned char *buffer_array[1];
		buffer_array[0] = bmp_buffer + (cinfo.output_scanline) * row_stride;

		jpeg_read_scanlines(&cinfo, buffer_array, 1);

	}
	// syslog(LOG_INFO, "Proc: Done reading scanlines");

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	// And free the input buffer
	free(jpg_buffer);

	// -------------------------------------------------------------------- DONE

	image->bpp = 3;
	image->width = width;
	image->height = height;
	image->buffer = bmp_buffer;
	image->type = VC_IMAGE_RGB888;
	image->pitch = row_stride;

	return true;
}

bool loadPNG(const char *f_name, Image *image){
	FILE* fpin = fopen(f_name, "rb");
	if (fpin == NULL){fprintf(stderr, "loadpng: can't open file for reading\n"); return false;}

	//---------------------------------------------------------------------

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL){fclose(fpin); return false;}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL){
		png_destroy_read_struct(&png_ptr, 0, 0);
		fclose(fpin);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))){
		png_destroy_read_struct(&png_ptr, &info_ptr, 0);
		fclose(fpin);
		return false;
	}

	//---------------------------------------------------------------------

	png_init_io(png_ptr, fpin);
	png_read_info(png_ptr, info_ptr);

	//---------------------------------------------------------------------

	png_byte colour_type = png_get_color_type(png_ptr, info_ptr);
	png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	image->width = png_get_image_width(png_ptr, info_ptr);
	image->height = png_get_image_height(png_ptr, info_ptr);

	if (colour_type & PNG_COLOR_MASK_ALPHA){
		image->type = VC_IMAGE_RGBA32;
		image->bpp = 4;
	}else{
		image->type = VC_IMAGE_RGB888;
		image->bpp = 3;
	}

	//---------------------------------------------------------------------

	double gamma = 0.0;

	if (png_get_gAMA(png_ptr, info_ptr, &gamma)){png_set_gamma(png_ptr, 2.2, gamma);}

	//---------------------------------------------------------------------

	if (colour_type == PNG_COLOR_TYPE_PALETTE){png_set_palette_to_rgb(png_ptr);}

	if ((colour_type == PNG_COLOR_TYPE_GRAY) && (bit_depth < 8)){png_set_expand_gray_1_2_4_to_8(png_ptr);}

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)){png_set_tRNS_to_alpha(png_ptr);}

	if (bit_depth == 16){
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
		png_set_scale_16(png_ptr);
#else
		png_set_strip_16(png_ptr);
#endif
	}

	if (colour_type == PNG_COLOR_TYPE_GRAY || colour_type == PNG_COLOR_TYPE_GRAY_ALPHA){png_set_gray_to_rgb(png_ptr);}

	//---------------------------------------------------------------------

	png_read_update_info(png_ptr, info_ptr);

	//---------------------------------------------------------------------

	png_bytepp row_pointers = malloc(image->height * sizeof(png_bytep));
	png_uint_32 j = 0;

	image->pitch = ALIGN_TO_16(image->width) * image->bpp;
	int size = image->pitch * ALIGN_TO_16(image->height);
	image->buffer = calloc(1, size);

	for (j = 0 ; j < image->height ; ++j){row_pointers[j] = image->buffer + (j * image->pitch);}

	//---------------------------------------------------------------------

	png_read_image(png_ptr, row_pointers);

	//---------------------------------------------------------------------

	fclose(fpin);
	free(row_pointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, 0);
	return true;
}




void show_usage(void){
	fprintf(stderr,"Example : ./img2dispmanx -file image.png -x 10 -y 30 -width 100 -height 200 -layer 1000 -display 1\n");
	fprintf(stderr,"Options:\n");
	fprintf(stderr,"\t-file, png or jpeg file to display\n");
	fprintf(stderr,"\t-x, optional, position where picture will be display, 0 if not set\n");
	fprintf(stderr,"\t-y, optional, position where picture will be display, 0 if not set\n");
	fprintf(stderr,"\t-width, picture size on screen, optional if -height is set (will keep aspect ratio)\n");
	fprintf(stderr,"\t-height, picture size on screen, optional if -width is set (will keep aspect ratio)\n");
	fprintf(stderr,"\t-layer, optional, dispmanx layer to use, 1 if not set\n");
	fprintf(stderr,"\t-display, optional, dispmanx display to use\n");
	fprintf(stderr,"\t-timeout, optional, in sec\n");
	exit(EXIT_FAILURE);
}




int endsWith(const char *str, const char *suffix){
	if(!str||!suffix){return 0;}
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if(lensuffix>lenstr){return 0;}
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}


bool loadImage(char* f_name, Image *img){
	if(endsWith(f_name, ".png")){return loadPNG(f_name, img);
	}else if(endsWith(f_name, ".jpg") || endsWith(f_name, ".jpeg")){return loadJPG(f_name, img);}
	return false;
}



int main(int argc, char *argv[]){
	int32_t layer = 1;
	uint32_t displayNumber = 0;
	int xOffset = 0;
	int yOffset = 0;
	int width = 0;
	int height = 0;
	int timeout = 0;
	char *f_name = NULL;
	int result = 0;
	//---------------------------------------------------------------------

	for(int i=1;i<argc;++i){ //argument to variable
			if(strcmp(argv[i],"-help")==0){show_usage();return 1;
			}else if(strcmp(argv[i],"-file")==0){f_name=argv[i+1];
			}else if(strcmp(argv[i],"-x")==0){xOffset=atoi(argv[i+1]);
			}else if(strcmp(argv[i],"-y")==0){yOffset=atoi(argv[i+1]);
			}else if(strcmp(argv[i],"-width")==0){width=atoi(argv[i+1]);
			}else if(strcmp(argv[i],"-height")==0){height=atoi(argv[i+1]);
			}else if(strcmp(argv[i],"-layer")==0){layer=atoi(argv[i+1]);
			}else if(strcmp(argv[i],"-timeout")==0){timeout=atoi(argv[i+1]);
			}else if(strcmp(argv[i],"-display")==0){displayNumber=atoi(argv[i+1]);}
	}

	if(f_name==NULL){fprintf(stderr, "Error, need to set file to display\n");return 1;}
	if(width==0&&height==0){fprintf(stderr, "Error, need to set at least width or height\n");return 1;}


	

	// Signal handling
	if (signal(SIGINT, signalHandler) == SIG_ERR)
	{
		perror("installing SIGINT signal handler");
		exit(EXIT_FAILURE);
	}
	if (signal(SIGTERM, signalHandler) == SIG_ERR)
	{
		perror("installing SIGTERM signal handler");
		exit(EXIT_FAILURE);
	}
	//---------------------------------------------------------------------

	// Load image file to structure Image
	Image image = { 0 };
	if (loadImage(f_name, &image) == false || image.buffer == NULL || image.width == 0 || image.height == 0){
		fprintf(stderr, "Unable to load %s\n", f_name);
		return 1;
	}

	// Init BCM
	bcm_host_init();

	DISPMANX_DISPLAY_HANDLE_T display	= vc_dispmanx_display_open(displayNumber);
	assert(display != 0);

	DISPMANX_MODEINFO_T info;
	result = vc_dispmanx_display_get_info(display, &info);
	assert(result == 0);

	// Calculate linear scaling maintaining aspect ratio
	if (width == 0 && height != 0) {
		width = (height * image.width) / image.height;
	} else if (width != 0 && height == 0) {
		height = (width * image.height) / image.width;
	}

	// Create a resource and copy bitmap to resource
	uint32_t vc_image_ptr;
	DISPMANX_RESOURCE_HANDLE_T resource = vc_dispmanx_resource_create(image.type, image.width, image.height, &vc_image_ptr);

	assert(resource != 0);

	// Set dimentions of the bitmap to be copied
	VC_RECT_T bmpRect;
	vc_dispmanx_rect_set(&bmpRect, 0, 0, image.width, image.height);

	// Copy bitmap data to vc
	result = vc_dispmanx_resource_write_data(resource, image.type, image.pitch, image.buffer, &bmpRect);

	assert(result == 0);

	// Free bitmap data
	free(image.buffer);
	//---------------------------------------------------------------------

	// Notify vc that an update is takng place
	DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
	assert(update != 0);

	// Calculate source and destination rect values
	VC_RECT_T srcRect, dstRect;
	vc_dispmanx_rect_set(&srcRect, 0, 0, image.width << 16, image.height << 16);
	vc_dispmanx_rect_set(&dstRect, xOffset, yOffset, width, height);

	// Add element to vc
	VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FROM_SOURCE, 255, 0 };
	DISPMANX_ELEMENT_HANDLE_T element = vc_dispmanx_element_add(update, display, layer, &dstRect, resource, &srcRect,DISPMANX_PROTECTION_NONE, &alpha, NULL, DISPMANX_NO_ROTATE);

	assert(element != 0);

	// Notify vc that update is complete
	result = vc_dispmanx_update_submit_sync(update);
	assert(result == 0);
	//---------------------------------------------------------------------

	
	if(timeout>0){
		sleep(timeout); // just sleep
	}else{
		pause(); // Wait till a signal is received
	}
	
	//---------------------------------------------------------------------

	// Delete layer and free memory
	update = vc_dispmanx_update_start(0);
	assert(update != 0);
	result = vc_dispmanx_element_remove(update, element);
	assert(result == 0);
	result = vc_dispmanx_update_submit_sync(update);
	assert(result == 0);
	result = vc_dispmanx_resource_delete(resource);
	assert(result == 0);
	result = vc_dispmanx_display_close(display);
	assert(result == 0);

	bcm_host_deinit();

	return 0;
}
