/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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

#ifndef SFLPHONE_GLOBAL_H
#define SFLPHONE_GLOBAL_H

#define NB_PHONELINES 6
#define PROGNAME "SFLPhone"
#define VERSION "0.4.2"

#define AUDIO_SECTION "Audio"
#define AUDIO_DEFAULT_DEVICE "Drivers.driverName"
#define AUDIO_DEFAULT_DEVICEIN "Drivers.driverNameIn"
#define AUDIO_DEFAULT_DEVICEOUT "Drivers.driverNameOut"

#define AUDIO_CODEC1 "Codecs.codec1"
#define AUDIO_CODEC2 "Codecs.codec2"
#define AUDIO_CODEC3 "Codecs.codec3"
#define AUDIO_RINGTONE "Rings.ringChoice"

#define SIGNALISATION_SECTION "VoIPLink"
#define SIGNALISATION_FULL_NAME "SIP.fullName"
#define SIGNALISATION_USER_PART "SIP.userPart"
#define SIGNALISATION_AUTH_USER_NAME "SIP.username"
#define SIGNALISATION_PASSWORD "SIP.password"
#define SIGNALISATION_HOST_PART "SIP.hostPart"
#define SIGNALISATION_PROXY "SIP.proxy"
#define SIGNALISATION_STUN_SERVER "STUN.STUNserver"
#define SIGNALISATION_USE_STUN "STUN.useStun"
#define SIGNALISATION_PLAY_TONES "DTMF.playTones"
#define SIGNALISATION_PULSE_LENGTH "DTMF.pulseLength"
#define SIGNALISATION_SEND_DTMF_AS "DTMF.sendDTMFas"

#define ACCOUNT_DEFAULT_NAME "SIP0"
#define ACCOUNT_TYPE "Account.type"
#define ACCOUNT_ENABLE "Account.enable"
#define ACCOUNT_AUTO_REGISTER "Account.autoregister"
#define ACCOUNT_ALIAS  "Account.alias"

#define PREFERENCES_SECTION "Preferences"
#define PREFERENCES_THEME "Themes.skinChoice"

#define SKINDIR DATADIR "/skins"

#endif
