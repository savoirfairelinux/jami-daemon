/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com
 *    
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **/


#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <jpeglib.h>

#include "capture_v4l2.h"
#include "../utils/convert.c"

#define DEVICE "/dev/video0"


/******************************************************************/

camera_v4l2* v4l2_init_camera( char* device_name ){

	// initialization of the struct fields
	camera_v4l2* webcam;
	webcam = (camera_v4l2*)malloc(sizeof(camera_v4l2));

	memset(&(webcam->cap),0,sizeof(webcam->cap));
	memset(&(webcam->format),0,sizeof(webcam->format));

	webcam->device = device_name;

	printf("Checking device /dev/video0.......\n");
	int n = query_camera(webcam);
	webcam->addr = n;

	if(n==0){	
		printf("Failed - query_camera()\n");
		return 0;
	}

	printf("Query webcam done....\n");

	// we use the fmt.pix.sizeimage field of the v4l2_pix_format struct
	// fmt.pix.sizeimage contains the total size of the buffer to hold a complete image, in bytes.
	webcam->raw_data = malloc(webcam->format.fmt.pix.sizeimage);		
	if (webcam->raw_data == NULL){
		printf("Error, can't alloc memory for data buffer\n");
		return 0;
	}

	// set the resolution of the camera
	webcam->width = webcam->format.fmt.pix.width;
	webcam->height = webcam->format.fmt.pix.height;

	// memory allocation for the buffer that will contain the RGB information
	// 3 values per pixels 
	webcam->img_data = malloc(webcam->width*webcam->height*3*sizeof(unsigned char));
	
	if(webcam==NULL){
                printf("INIT FAILED!\n");
                return NULL;
         }

	info_device(webcam);

	return webcam;
}

int query_camera(camera_v4l2* const camera){

	int fd;

	char* device = camera->device;
	struct v4l2_capability* cap = &(camera->cap);
	struct v4l2_format* format = &(camera->format);

	// open the webcam device, like a file, return a file descriptor
	fd = open(device, O_RDONLY);

	if(fd < 0){
		printf("error, can't open device %s\n", device);	
		return 0;	
	}

	// fill the v4l2_capabilities struct by a ioctl call (control device)
	// VIDIOC_QUERYCAP (V4L2 API Specification) 
	if(ioctl(fd, VIDIOC_QUERYCAP,cap)==-1){
		printf("error, can't query device's capabilities\n");
		return 0;
	}

	format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(ioctl(fd, VIDIOC_G_FMT, format)==-1){
		printf("error, can't set the capture image format\n");
		return 0;
	}
	format->fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;

	// return the file descriptor of the device
	return fd;
}


char* v4l2_grab_data(camera_v4l2* const camera){

	int fd = camera->addr;	
	int n = read( fd, 
			camera->raw_data, 
			camera->format.fmt.pix.sizeimage);
	if(n==-1){
		printf("Error, can't read the webcam\n");
		return 0;
	}
	
	// launch image format conversion
	format_conversion(camera->format.fmt.pix.pixelformat, camera->raw_data, camera->img_data, camera->width, camera->height);	 


	//return camera->raw_data;
	return camera->img_data;
}

void v4l2_free_data(camera_v4l2* cam){

	free(cam->device);
	free(cam->raw_data);
	free(cam->img_data);
	free(cam);
}

void info_device(camera_v4l2* cam){
	
	int format = cam->format.fmt.pix.pixelformat;
	
	printf("****************** SPECS CAMERA ********************\n");
        printf("Image resolution = %ix%i\n",cam->width,cam->height);
        printf("Image size = %i bytes\n", cam->format.fmt.pix.sizeimage);
        printf("Pixels format = ");
	switch(format){
		case V4L2_PIX_FMT_RGB332:
                        printf("RGB332\n");
                        break;
                case V4L2_PIX_FMT_RGB555:
                        printf("RGB555\n");
                        break;
                case V4L2_PIX_FMT_RGB565:
                        printf("RGB565\n");
                        break;
                case V4L2_PIX_FMT_BGR24:
                        printf("BGR24\n");
                        break;
                case V4L2_PIX_FMT_RGB24:
                        printf("RGB24\n");
                	break;
		case V4L2_PIX_FMT_BGR32:
                        printf("BGR32\n");
                        break;
                case V4L2_PIX_FMT_RGB32:
                        printf("RGB32\n");
                        break;
                case V4L2_PIX_FMT_YUV410:
                        printf("YUV410\n");
                        break;
                case V4L2_PIX_FMT_YUV420:
                        printf("YUV420\n");
                        break;
                case V4L2_PIX_FMT_YUYV:
                        printf("YUYV\n");
                        break;
                case V4L2_PIX_FMT_UYVY:
                        printf("UYVY\n");
                        break;
                default:
                        printf("TODO: Implement support for V4L2_PIX_FMT 0x%X\n",
                                                format);
                        exit(1);
        }

}

