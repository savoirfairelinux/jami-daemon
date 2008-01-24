/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
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

typedef float float32;
typedef short int16;

//#define DATAFORMAT_IS_FLOAT
#ifdef DATAFORMAT_IS_FLOAT
#define SFLDataFormat float32
#define SFLPortaudioFormat portaudio::FLOAT32
#define SFLPortaudioFormatString "Float32"
#define SFLDataAmplitude 0.05
#define SFLConvertInt16(s) ((float)(s)-16384.0)/16384.0
#else
#define SFLDataFormat int16
#define SFLPortaudioFormat portaudio::INT16
#define SFLPortaudioFormatString "Int16"
#define SFLDataAmplitude (32767 >> 4)
#define SFLConvertInt16(s) (s)
#endif

#ifdef SFLDEBUG
  #define _debug(...)          fprintf(stderr, "[sfl-debug] " __VA_ARGS__)
  #define _debugStart(...)     fprintf(stderr, "[sfl-debug] " __VA_ARGS__)
  #define _debugMid(...)       fprintf(stderr, __VA_ARGS__)
  #define _debugEnd(...)       fprintf(stderr, __VA_ARGS__)
  #define _debugException(...) fprintf(stderr, "[sfl-excep] " __VA_ARGS__ "\n")
  #define _debugInit(...)      fprintf(stderr, "[sfl-init.] " __VA_ARGS__ "\n")
#else
  #define _debug(...)
  #define _debugStart(...)
  #define _debugMid(...)
  #define _debugEnd(...)
  #define _debugException(...)
  #define _debugInit(...)
#endif

#define SFLPHONED_VERSION "0.7.2"
#define SFLPHONED_VERSIONNUM 0x000702

#define PROGNAME         "sflphoned"
#define PROGNAME_GLOBAL  "sflphone"
#define PROGDIR          "sflphone"
#define RINGDIR          "ringtones"
#define CODECDIR         "codecs"

#define _(arg) arg
#define MONO					1
#define CHANNELS				2
#define SIZEBUF 				1024*1024

// Codecs payloads, as defined in RFC3551
// http://www.iana.org/assignments/rtp-parameters
// http://www.gnu.org/software/ccrtp/doc/refman/html/formats_8h.html#a0
/*#define PAYLOAD_CODEC_ULAW	0  // PCMU 8000
#define PAYLOAD_CODEC_ALAW	8  // PCMA 8000
#define PAYLOAD_CODEC_GSM	3  // GSM 8000
// http://www.ietf.org/rfc/rfc3952.txt
#define PAYLOAD_CODEC_ILBC	97*/


#endif	// __GLOBAL_H__
