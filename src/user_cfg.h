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

#ifndef __USER_CFG_H__
#define __USER_CFG_H__

#define GUI_QT

#include <stdlib.h>

// Home directory
#define HOMEDIR	(getenv ("HOME"))

// Main menu
#define SIGNALISATION	"VoIPLink"
#define AUDIO			"Audio"
#define VIDEO			"Video"
#define NETWORK			"Network"
#define PREFERENCES		"Preferences"

// Fields to fill
#define VOIP_LINK_ID	"VoIPLink.index"
#define FULL_NAME		"SIP.fullName"
#define USER_PART		"SIP.userPart"
#define AUTH_USER_NAME	"SIP.username"
#define PASSWORD		"SIP.password"
#define HOST_PART		"SIP.hostPart"
#define PROXY			"SIP.proxy"
#define AUTO_REGISTER	"SIP.autoregister"
#define PLAY_TONES		"DTMF.playTones"
#define PULSE_LENGTH	"DTMF.pulseLength"
#define SEND_DTMF_AS	"DTMF.sendDTMFas"
#define STUN_SERVER		"STUN.STUNserver"
#define USE_STUN		"STUN.useStun"
#define DRIVER_NAME		"Drivers.driverName"
#define NB_CODEC		"Codecs.nbCodec"
#define CODEC1			"Codecs.codec1"
#define CODEC2			"Codecs.codec2"
#define CODEC3			"Codecs.codec3"
#define CODEC4			"Codecs.codec4"
#define CODEC5			"Codecs.codec5"
#define RING_CHOICE		"Rings.ringChoice"

// speakers and volume 0 to 100
#define VOLUME_SPKR	"Volume.speakers"
#define VOLUME_MICRO	"Volume.micro"

#define VOLUME_SPKR_X	"Volume.speakers_x"
#define VOLUME_SPKR_Y	"Volume.speakers_y"
#define VOLUME_MICRO_X	"Volume.micro_x"
#define VOLUME_MICRO_Y	"Volume.micro_y"
#define SKIN_CHOICE		"Themes.skinChoice"
#define CONFIRM_QUIT	"Options.confirmQuit"
#define ZONE_TONE		"Options.zoneToneChoice"
#define CHECKED_TRAY	"Options.checkedTray"
#define VOICEMAIL_NUM	"Options.voicemailNumber"
// zeroconfig module
#define CONFIG_ZEROCONF "Zeroconf.enable"

// Default values
#define EMPTY_FIELD			""
#define DFT_STUN_SERVER 	"stun.fwdnet.net:3478"

#define DFT_VOIP_LINK         0
#define DFT_VOIP_LINK_STR    "0" // index of the first VoIP link by default
#define	YES_STR              "1"
#define	NO_STR               "0"
#define DFT_PULSE_LENGTH_STR "250"
#define SIP_INFO_STR         "0"
#define DFT_DRIVER_STR       "0"
#define DFT_NB_CODEC_STR     "3"
// volume by default 100%
#define DFT_VOL_SPKR_STR     "100"
#define DFT_VOL_MICRO_STR    "100"


#define DFT_CODEC			"G711u"
#define DFT_RINGTONE 		"konga.ul"
#define DFT_SKIN 			"metal"
#define DFT_ZONE			"North America"
#define DFT_VOICEMAIL 		"888"
// zeroconfig default value
#ifdef USE_ZEROCONF
#define CONFIG_ZEROCONF_DEFAULT_STR "1"
#else
#define CONFIG_ZEROCONF_DEFAULT_STR "0"
#endif

#endif // __USER_CFG_H__
