/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <stdio.h>
#include <libintl.h>
#include <locale.h>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <map>
#include <vector>
#include "logger.h"

#define HOMEDIR					(getenv ("HOME"))				/** Home directory */
#define XDG_DATA_HOME			(getenv ("XDG_DATA_HOME"))
#define XDG_CONFIG_HOME			(getenv ("XDG_CONFIG_HOME"))
#define XDG_CACHE_HOME			(getenv ("XDG_CACHE_HOME"))
const char * const ZRTP_ZID_FILENAME = "sfl.zid";

//useful typedefs.
typedef signed short SINT16;
typedef signed int SINT32;

#define PIDFILE "sfl.pid"

typedef short SFLDataFormat;

#define SIZEBUF 		 400000 /** About 12 sec of buffering at 8000 Hz*/

#define ALSA_DFT_CARD_ID     0			/** Index of the default soundcard */

#define PCM_PLUGHW	"plughw"		/** Alsa plugin */
#define PCM_DEFAULT	"default"		/** Default ALSA plugin */
#define PCM_DMIX	"plug:dmix"		/** Alsa plugin for software mixing */
#define PCM_DSNOOP	"plug:dsnoop"		/** Alsa plugin for microphone sharing */
#define PCM_DMIX_DSNOOP "dmix/dsnoop"           /** Audio profile using Alsa dmix/dsnoop */

#define SFL_PCM_BOTH		0x0021		/** To open both playback and capture devices */
#define SFL_PCM_PLAYBACK	0x0022		/** To open playback device only */
#define SFL_PCM_CAPTURE		0x0023		/** To open capture device only */
#define SFL_PCM_RINGTONE    0x0024

#define RINGTONE_ENABLED	      TRUE_STR		/** Custom ringtone enable or not */
#define DISPLAY_DIALPAD		      TRUE_STR		/** Display dialpad or not */
#define DISPLAY_VOLUME_CONTROLS	  TRUE_STR		/** Display the volume controls or not */
#define START_HIDDEN		      TRUE_STR		/** SFlphone starts hidden at start-up or not */
#define WINDOW_POPUP		      TRUE_STR		/** Popup mode */
#define NOTIFY_ALL		          TRUE_STR		/** Desktop notification level 0: never notify */

// Error codes for error handling
#define NO_ERROR		            0x0000	/** No error - Everything alright */
#define ALSA_CAPTURE_DEVICE         0x0001	/** Error while opening capture device */
#define ALSA_PLAYBACK_DEVICE        0x0010	/** Error while opening playback device */
#define PULSEAUDIO_NOT_RUNNING      0x0100  /** Pulseaudio is not running */
#define CODECS_NOT_LOADED           0x1000  /** Codecs not found */

#define PULSEAUDIO_API_STR          "pulseaudio"
#define ALSA_API_STR                "alsa"
#define UNUSED          __attribute__((__unused__))

#define DEFAULT_SIP_PORT    5060
#define DEFAULT_SIP_TLS_PORT 5061

/** Enumeration that contains known audio payloads */
enum {
    // http://www.iana.org/assignments/rtp-parameters
    // http://www.gnu.org/software/ccrtp/doc/refman/html/formats_8h.html#a0
    // 0 PCMU A 8000 1 [RFC3551]
    PAYLOAD_CODEC_ULAW = 0,
    // 3 GSM  A 8000 1 [RFC3551]
    PAYLOAD_CODEC_GSM = 3,
    // 8 PCMA A 8000 1 [RFC3551]
    PAYLOAD_CODEC_ALAW = 8,
    // 9 G722 A 8000 1 [RFC3551]
    PAYLOAD_CODEC_G722 = 9,
    // http://www.ietf.org/rfc/rfc3952.txt
    // 97 iLBC/8000
    PAYLOAD_CODEC_ILBC_20 = 97,
    PAYLOAD_CODEC_ILBC_30 = 98,
    // http://www.speex.org/drafts/draft-herlein-speex-rtp-profile-00.txt
    //  97 speex/8000
    // http://support.xten.com/viewtopic.php?p=8684&sid=3367a83d01fdcad16c7459a79859b08e
    // 100 speex/16000
    PAYLOAD_CODEC_SPEEX_8000 = 110,
    PAYLOAD_CODEC_SPEEX_16000 = 111,
    PAYLOAD_CODEC_SPEEX_32000 = 112
};

#define IP2IP_PROFILE "IP2IP"
#define DIR_SEPARATOR_STR "/" // Directory separator char
#define DIR_SEPARATOR_CH = '/' /** Directory separator string */

#endif	// __GLOBAL_H__
