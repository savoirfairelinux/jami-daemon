/*
 *  Copyright (C) 2004-2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <assert.h>
#include <stdio.h>
#include <libintl.h>
#include <locale.h>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <map>
#include <vector>

#define SFLPHONED_VERSION "0.9.2-4"		/** Version number */

typedef float float32;
typedef short int16;

//useful typedefs.
typedef signed short SINT16;
typedef signed int SINT32;

typedef unsigned long FILE_TYPE;
typedef unsigned long SOUND_FORMAT;

const FILE_TYPE  FILE_RAW = 1;
const FILE_TYPE  FILE_WAV = 2;

static const SOUND_FORMAT INT16 = 0x2; // TODO shold change these symbols
static const SOUND_FORMAT INT32 = 0x8;

#define SUCCESS                 0

#define ASSERT( expected , value)       if( value == expected ) return SUCCESS; \
                                        else return 1; 
#define PIDFILE "sfl.pid"

#ifdef DATAFORMAT_IS_FLOAT
#define SFLDataFormat float32
#define SFLDataFormatString "Float32"
#define SFLDataAmplitude 0.05
#else
#define SFLDataFormat int16
#define SFLDataFormatString "Int16"
#define SFLDataAmplitude (32767 >> 4)
#endif

#ifdef SFLDEBUG
  #define _debug(...)          fprintf(stderr, "[sfl-debug] " __VA_ARGS__)
  #define _debugException(...) fprintf(stderr, "[sfl-excep] " __VA_ARGS__ "\n")
  #define _debugInit(...)      fprintf(stderr, "[sfl-init] " __VA_ARGS__ "\n")
  #define _debugAlsa(...)      fprintf(stderr, "[alsa-debug] " __VA_ARGS__ )
#else
  #define _debug(...)
  #define _debugException(...)
  #define _debugInit(...)
  #define _debugAlsa(...)
#endif

#define PROGNAME         "sflphoned"		/** Binary name */
#define PROGDIR          "sflphone"		/** Program directory */
#define RINGDIR          "ringtones"		/** Ringtones directory */
#define CODECDIR         "codecs"		/** Codecs directory */

#define SIZEBUF 				1024*1024

#define ALSA_DFT_CARD_ID     0			/** Index of the default soundcard */

#define PCM_PLUGHW	"plughw"		/** Alsa plugin */ 
#define PCM_DEFAULT	"default"		/** Default ALSA plugin */
#define PCM_DMIX	"plug:dmix"		/** Alsa plugin for software mixing */

#define SFL_CODEC_VALID_PREFIX	"libcodec_"	/** Valid prefix for codecs shared library */ 
#define SFL_CODEC_VALID_EXTEN	".so"		/** Valid extension for codecs shared library */
#define CURRENT_DIR		"."		/** Current directory */
#define PARENT_DIR		".."		/** Parent directory */

#define SFL_PCM_BOTH		0x0021		/** To open both playback and capture devices */ 
#define SFL_PCM_PLAYBACK	0x0022		/** To open playback device only */
#define SFL_PCM_CAPTURE		0x0023		/** To open capture device only */

#ifdef USE_IAX
#define	IAX2_ENABLED  true			/** IAX2 support */
#else
#define	IAX2_ENABLED  false			/** IAX2 support */
#endif

#define GSM_STRING_DESCRIPTION	  "gsm"		/** GSM codec string description */
#define SPEEX_STRING_DESCRIPTION  "speex"	/** SPEEX codec string description */
#define ILBC_STRING_DESCRIPTION  "ilbc"		/** Ilbc codec string description */
#define RINGTONE_ENABLED	  1		/** Custom ringtone enable or not */
#define DISPLAY_DIALPAD		  1		/** Display dialpad or not */
#define DISPLAY_VOLUME_CONTROLS	  1		/** Display the volume controls or not */
#define START_HIDDEN		  1		/** SFlphone starts hidden at start-up or not */
#define WINDOW_POPUP		  1		/** Popup mode */
#define NOTIFY_ALL		  1		/** Desktop notification level 0: never notify */
#define NOTIFY_MAILS		  1		/** Desktop mail notification level 0: never notify */
#define STUN_ENABLED         1

// Error codes for error handling
#define NO_ERROR		      0x0000	/** No error - Everything alright */
#define ALSA_CAPTURE_DEVICE           0x0001	/** Error while opening capture device */
#define ALSA_PLAYBACK_DEVICE          0x0010	/** Error while opening playback device */
#define NETWORK_UNREACHABLE           0x0011	/** Network unreachable */
#define PULSEAUDIO_NOT_RUNNING          0x0100  /** Pulseaudio is not running */

#define ALSA			  0 
#define PULSEAUDIO		  1
#define CHECK_INTERFACE( layer , api )		  (layer == api) 

#define UNUSED          __attribute__((__unused__))      

#define DEFAULT_SIP_PORT    5060

#endif	// __GLOBAL_H__
