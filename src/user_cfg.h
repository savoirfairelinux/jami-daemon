/*
 *  Copyright (C) 2004-2008 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Authoe: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#define HOMEDIR	(getenv ("HOME"))		      /** Home directory */

#define DIR_SEPARATOR_CH '/'			      /** Directory separator string */
#define DIR_SEPARATOR_STR "/"			      /** Directory separator char */

#define ACCOUNT_SIP0  "SIP0"			      /** Account type SIP */
#define ACCOUNT_IAX0  "IAX0"			      /** Account type IAX */

/** User configuration file fields */
#define AUDIO			"Audio"		      /** Section Audio */
#define CODECS			"ActiveCodecs"	      /** List of active codecs */
#define ALSA_CARD_ID_IN		"Alsa.cardID_In"      /** Soundcard index to use for capture */
#define ALSA_CARD_ID_OUT	"Alsa.cardID_Out"     /** Soundcard index to use for playback */
#define ALSA_FRAME_SIZE		"Alsa.framesize"      /** Audio layer frame size */
#define ALSA_PLUGIN		"Alsa.plugin"	      /** Alsa plugin */
#define ALSA_SAMPLE_RATE	"Alsa.sampleRate"     /** Audio layer sample rate */
#define RING_CHOICE		"Rings.ringChoice"    /** Ringtone */
#define VOLUME_SPKR		"Volume.speakers"     /** Speaker volume */
#define VOLUME_MICRO		"Volume.micro"	      /** Mic volume */

#define VIDEO			"Video"		      /** Section Video */

#define PREFERENCES		"Preferences"		  /** Section Preferences */
#define CONFIG_DIALPAD		"Dialpad.display"	  /** Display dialpad preferences */
#define ZONE_TONE		"Options.zoneToneChoice"  /** Country tone */
#define VOICEMAIL_NUM		"Options.voicemailNumber" /** Voicemail number */
#define CONFIG_RINGTONE		"Ringtones.enable"	  /** Ringtones preferences */
#define CONFIG_START		"Start.hidden"		  /** SFLphone starts in the systm tray or not */
#define CONFIG_POPUP		"Window.popup"		  /** SFLphone pops up on incoming calls or not */
#define CONFIG_ZEROCONF		"Zeroconf.enable"	  /** Zero configuration networking module */

#define SIGNALISATION		"VoIPLink"	      /** Section Signalisation */
#define PLAY_DTMF		"DTMF.playDtmf"	      /** Whether or not should play dtmf */
#define PLAY_TONES		"DTMF.playTones"      /** Whether or not should play tones */
#define PULSE_LENGTH		"DTMF.pulseLength"    /** Length of the DTMF in millisecond */
#define SEND_DTMF_AS		"DTMF.sendDTMFas"     /** DTMF send mode */
#define SYMMETRIC		"VoIPLink.symmetric"  /** VoIP link type */

#define EMPTY_FIELD		""			/** Default value for empty field */
#define DFT_STUN_SERVER 	"stun.fwdnet.net:3478"	/** Default STUN server address */
#define	YES_STR			"1"			/** Default YES value */   
#define	NO_STR			"0"			/** Default NO value */
#define DFT_PULSE_LENGTH_STR	"250"			/** Default DTMF lenght */
#define SIP_INFO_STR		"0"			/** Default DTMF transport mode */	
#define ALSA_DFT_CARD		"0"			/** Default sound card index */
#define DFT_VOL_SPKR_STR	"100"			/** Default speaker volume */
#define DFT_VOL_MICRO_STR	"100"			/** Default mic volume */
#define DFT_RINGTONE 		"konga.ul"		/** Default ringtone */
#define DFT_ZONE		"North America"		/** Default geographical zone */
#define DFT_VOICEMAIL 		"888"			/** Default voicemail number */
#define DFT_FRAME_SIZE		"20"			/** Default frame size in millisecond */
#define DFT_SAMPLE_RATE		"44100"			/** Default sample rate in HZ */

#ifdef USE_ZEROCONF
#define CONFIG_ZEROCONF_DEFAULT_STR "1"			/** Default Zero configuration networking module value */
#else
#define CONFIG_ZEROCONF_DEFAULT_STR "0"			/** Default Zero configuration networking module value */
#endif

#endif // __USER_CFG_H__
