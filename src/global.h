/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                                
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <assert.h>
#include <stdio.h>

typedef float float32;
typedef short int16;


#define DEBUG

#ifdef DEBUG
	#define _debug(...)	fprintf(stderr, "[debug] " __VA_ARGS__)
#else
	#define _debug(...)
#endif

#define VERSION "0.4.1-pre1"
#define VERSIONNUM 0x000400


#define PROGNAME				"sflphone"
#define SKINDIR					"skins"
#define PIXDIR					"icons"
#define RINGDIR					"rings"
#define CODECDIR				"codecs"

#define SFLPHONE_LOGO			"logo_ico.png"
#define TRAY_ICON				"tray-icon.png"
#define PIXMAP_SIGNALISATIONS	"signalisations.png" 
#define PIXMAP_AUDIO			"audio.png" 
#define PIXMAP_VIDEO			"video.png" 
#define PIXMAP_NETWORK			"network.png" 
#define PIXMAP_PREFERENCES		"preferences.png" 
#define PIXMAP_ABOUT			"about.png" 

#define MONO					1
#define CHANNELS				2
#define	SAMPLING_RATE 			8000
#define SIZEBUF 				1024*1024
#define	FORMAT					4			
#define OCTETS					SAMPLING_RATE * FORMAT	// Number of writen 
														// bytes in buffer

#endif	// __GLOBAL_H__
