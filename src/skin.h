/**
 *  Copyright (C) 2004 Savoir-Faire Linux inc.
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

#ifndef __SKIN_H__
#define __SKIN_H__

#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIXMAP_PHONE			"main.png"
#define PIXMAP_KEYPAD			"dtmf_main.png"
#define PIXMAP_SCREEN			"screen_main.png"
#define PIXMAP_OVERSCREEN		"overscreen.png"
#define PIXMAP_VOLUME			"volume.png"
	
#define PIXMAP_IS_RINGING		"ring.png"
#define PIXMAP_NO_RINGING		"no_ring.png"
#define PIXMAP_MESSAGE_ON		"voicemail_on.png"
#define PIXMAP_MESSAGE_OFF		"voicemail_off.png"
#define PIXMAP_LINE0_OFF		"l1_off.png"
#define PIXMAP_LINE1_OFF		"l2_off.png"
#define PIXMAP_LINE2_OFF		"l3_off.png"
#define PIXMAP_LINE3_OFF		"l4_off.png"
#define PIXMAP_LINE4_OFF		"l5_off.png"
#define PIXMAP_LINE5_OFF		"l6_off.png"
#define PIXMAP_LINE0_BUSY		"l1_on.png"
#define PIXMAP_LINE1_BUSY		"l2_on.png"
#define PIXMAP_LINE2_BUSY		"l3_on.png"
#define PIXMAP_LINE3_BUSY		"l4_on.png"
#define PIXMAP_LINE4_BUSY		"l5_on.png"
#define PIXMAP_LINE5_BUSY		"l6_on.png"

#define LINE1					"l1"
#define LINE2					"l2"
#define LINE3					"l3"
#define LINE4					"l4"
#define LINE5					"l5"
#define LINE6					"l6"
	
#define VOICEMAIL				"voicemail"
#define DIRECTORY				"directory"
#define CONFERENCE				"conference"
#define TRANSFER				"transfer"
#define CLOSE					"close"
#define MINIMIZE				"minimize"
#define SETUP					"setup"
#define HANGUP					"hangup"
#define CONNECT					"ok"
#define MUTE					"mute"
#define DTMF_SHOW				"dtmf"
#define VOLUME					"volume"
#define VOL_MIC					"vol_mic"
#define VOL_SPKR				"vol_spkr"
#define SCREEN					"screen"
	
#define	DTMF_0					"dtmf_0"
#define	DTMF_1					"dtmf_1"
#define	DTMF_2					"dtmf_2"
#define	DTMF_3					"dtmf_3"
#define	DTMF_4					"dtmf_4"
#define	DTMF_5					"dtmf_5"
#define	DTMF_6					"dtmf_6"
#define	DTMF_7					"dtmf_7"
#define	DTMF_8					"dtmf_8"
#define	DTMF_9					"dtmf_9"
#define	DTMF_STAR				"dtmf_star"
#define	DTMF_POUND				"dtmf_pound"
#define	DTMF_CLOSE				"dtmf_close"
	

extern const char* PIXMAP_LINE_NAMES[];

#define PIXMAP_LINE(n,t)	PIXMAP_LINE_NAMES[2*n+t]
// n = {0..3} and t = {FREE, BUSY, ONHOLD}
   
#define FILE_INI				"skin.ini"

#ifdef __cplusplus
}
#endif


class Skin {
public:
	Skin (void);
	~Skin (void);
	
	static QString getPath(const QString &, const QString &, const QString &, 
						const QString &, const QString &);
	static QString getPath(const QString &,const QString &, const QString &);
	static QString getPath(const QString &);
	static QString getPathPixmap (const QString &, const QString &);
};

#endif	// __SKIN_H__
