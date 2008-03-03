/*
 *      Copyright (C) 2006-2007 Savoir-Faire Linux inc.
 *      Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com
 *        
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/videodev.h>


#define CLIP 320
#define RED_NULL 128
#define BLUE_NULL 128
#define LUN_MUL 256
#define RED_MUL 512
#define BLUE_MUL 512


#define GREEN1_MUL  (-RED_MUL/2)
#define GREEN2_MUL  (-BLUE_MUL/6)
#define RED_ADD     (-RED_NULL  * RED_MUL)
#define BLUE_ADD    (-BLUE_NULL * BLUE_MUL)
#define GREEN1_ADD  (-RED_ADD/2)
#define GREEN2_ADD  (-BLUE_ADD/6)

/* lookup tables */
static unsigned int  ng_yuv_gray[256];
static unsigned int  ng_yuv_red[256];
static unsigned int  ng_yuv_blue[256];
static unsigned int  ng_yuv_g1[256];
static unsigned int  ng_yuv_g2[256];
static unsigned int  ng_clip[256 + 2 * CLIP];

#define GRAY(val)               ng_yuv_gray[val]
#define RED(gray,red)           ng_clip[ CLIP + gray + ng_yuv_red[red] ]
#define GREEN(gray,red,blue)    ng_clip[ CLIP + gray + ng_yuv_g1[red] + \
	ng_yuv_g2[blue] ]
#define BLUE(gray,blue)         ng_clip[ CLIP + gray + ng_yuv_blue[blue] ]

#define clip(x) ( (x)<0 ? 0 : ( (x)>255 ? 255 : (x) ) )
/******************************************************************************/

void YUV2RGB_init(void)
{
	int i;

	/* init Lookup tables */
	for (i = 0; i < 256; i++) {
		ng_yuv_gray[i] = i * LUN_MUL >> 8;
		ng_yuv_red[i]  = (RED_ADD    + i * RED_MUL)    >> 8;
		ng_yuv_blue[i] = (BLUE_ADD   + i * BLUE_MUL)   >> 8;
		ng_yuv_g1[i]   = (GREEN1_ADD + i * GREEN1_MUL) >> 8;
		ng_yuv_g2[i]   = (GREEN2_ADD + i * GREEN2_MUL) >> 8;
	}
	for (i = 0; i < CLIP; i++)
		ng_clip[i] = 0;
	for (; i < CLIP + 256; i++)
		ng_clip[i] = i - CLIP;
	for (; i < 2 * CLIP + 256; i++)
		ng_clip[i] = 255;
}


void write_rgb(unsigned char **out, int Y, int U, int V)
{
	int R,G,B;
	R=(76284*Y+104595*V)>>16;
	G=(76284*Y -25625*U-53281*V)>>16;
	B=(76284*Y+132252*U)>>16;

	*(*out)=clip(R);
	*(*out+1)=clip(G);
	*(*out+2)=clip(B);
	*out+=3;
}


void yuv420_rgb (unsigned char *out, unsigned char *in, int width, int height)
{
	unsigned char *u,*u1,*v,*v1;
	int Y=0,U=0,V=0,i,j;

	u=in+width*height;
	v=u+(width*height)/4;

	for(i=0;i<height;i++) {
		u1=u;
		v1=v;
		for(j=0;j<width;j++) {
			Y=(*in++)-16;
			if((j&1)==0) {
				U=(*u++)-128;
				V=(*v++)-128;
			}
			write_rgb(&out,Y,U,V);
		}
		if((i&1)==0) { u=u1; v=v1; }
	}
}

void yuyv_rgb (unsigned char *out, unsigned char *in, int width, int height)
{
	unsigned char *u,*u1,*v,*v1;
	int Y=0,U=0,V=0,i,j;

	u=in+1; v=in+3;
	for(i=0;i<width*height;i++) {
		Y=(*in)-16;
		U=(*u)-128;
		V=(*v)-128;
		write_rgb(&out,Y,U,V);
		in+=2;
		if((i&1)==1) { u+=4; v+=4; }
	}
}


void format_conversion(int format, char* in, char* out, int width, int height){

	YUV2RGB_init();

	switch(format){
		case V4L2_PIX_FMT_YUV420:
			yuv420_rgb(out,in,width,height);
			break;
		case V4L2_PIX_FMT_YUYV:
                        yuyv_rgb(out,in,width,height);
                        break;
	}
}



