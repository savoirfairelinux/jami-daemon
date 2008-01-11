/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **/

#ifndef CAPTURE_V4L2_H
#define CAPTURE_V4L2_H


#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/videodev.h>

/*********************************
 * struct camera_v4l2
 */

 typedef struct {
	char* device; 				//device name (/dev/video0 for the webcam)
	struct v4l2_capability cap;		//info on the device's capture capability (ex: type, name, ...)
	struct v4l2_format format;		//to handle multimedia data
	char* raw_data;				//buffer for the captured data
	int addr;				//file descriptor of the device
	char* img_data;
	int width;
	int height;	
}camera_v4l2;  

/***********************************

/***********************************
 * Methods
 */

// initialization of a new camera_v4l2 struct
camera_v4l2* v4l2_init_camera( char* device_name );

// grab frames
char* v4l2_grab_data(camera_v4l2* const camera);

//free the allocated buffers
void v4l2_free_data(camera_v4l2* camera);

void info_device(camera_v4l2* camera);

#endif
