/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#ifndef __USER_CFG_H__
#define __USER_CFG_H__

#include <stdlib.h>

// Home directory
#define HOMEDIR	(getenv ("HOME"))

// TODO: change for a \ in Windows Environment
#define DIR_SEPARATOR_CH '/'
#define DIR_SEPARATOR_STR "/"

// Main menu
#define SIGNALISATION		"VoIPLink"
#define AUDIO			"Audio"
#define VIDEO			"Video"
#define NETWORK			"Network"
#define PREFERENCES		"Preferences"

#define ACCOUNT_SIP0  "SIP0"
#define ACCOUNT_IAX0  "IAX0"


// Fields to fill
#define SYMMETRIC     "VoIPLink.symmetric"

#define PLAY_DTMF		"DTMF.playDtmf"
#define PLAY_TONES		"DTMF.playTones" 
#define PULSE_LENGTH		"DTMF.pulseLength"
#define SEND_DTMF_AS		"DTMF.sendDTMFas"
#define ALSA_CARD_ID_IN		"Alsa.cardID_In"
#define ALSA_CARD_ID_OUT	"Alsa.cardID_Out"
#define ALSA_SAMPLE_RATE	"Alsa.sampleRate"
#define ALSA_FRAME_SIZE		"Alsa.framesize"
#define ALSA_PLUGIN		"Alsa.plugin"	
#define CODECS			"ActiveCodecs"
#define RING_CHOICE		"Rings.ringChoice"
#define ACCOUNT_SIP_COUNT_DEFAULT 4
#define ACCOUNT_IAX_COUNT_DEFAULT 4

// speakers and volume 0 to 100
#define VOLUME_SPKR	  "Volume.speakers"
#define VOLUME_MICRO	  "Volume.micro"
#define ZONE_TONE	  "Options.zoneToneChoice"
#define VOICEMAIL_NUM	  "Options.voicemailNumber"
// zeroconfig module
#define CONFIG_ZEROCONF	  "Zeroconf.enable"
#define CONFIG_RINGTONE	  "Ringtones.enable"

// Default values
#define EMPTY_FIELD		""
#define DFT_STUN_SERVER 	"stun.fwdnet.net:3478"
#define	YES_STR			"1"
#define	NO_STR			"0"
#define DFT_PULSE_LENGTH_STR	"250"
#define SIP_INFO_STR		"0"
#define ALSA_DFT_CARD		"0"
// volume by default 100%
#define DFT_VOL_SPKR_STR	"100"
#define DFT_VOL_MICRO_STR	"100"

#define DFT_RINGTONE 		"konga.ul"
#define DFT_ZONE		"North America"
#define DFT_VOICEMAIL 		"888"
#define DFT_FRAME_SIZE		"20"
#define DFT_SAMPLE_RATE		"44100"

// zeroconfig default value
#ifdef USE_ZEROCONF
#define CONFIG_ZEROCONF_DEFAULT_STR "1"
#else
#define CONFIG_ZEROCONF_DEFAULT_STR "0"
#endif

#endif // __USER_CFG_H__
