/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com> 
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
 
#ifndef __SFLPHONE_CONST_H
#define __SFLPHONE_CONST_H

#include <libintl.h>
#include "dbus.h"

/* @file sflphone_const.h
 * @brief Contains the global variables for the client code
 */

/** Locale */
#define _(STRING)   gettext( STRING )   

/** Account type : SIP / IAX */
#define ACCOUNT_TYPE               "Account.type"
/** Account alias */
#define ACCOUNT_ALIAS		   "Account.alias"
/** Tells if account is enabled or not */
#define ACCOUNT_ENABLED		   "Account.enable"
/** SIP parameter: full name */
#define ACCOUNT_SIP_FULL_NAME      "SIP.fullName"
/** SIP parameter: host name */
#define ACCOUNT_SIP_HOST_PART      "SIP.hostPart"
/** SIP parameter: user name */
#define ACCOUNT_SIP_USER_PART      "SIP.userPart"
/** SIP parameter: authentification name */
#define ACCOUNT_SIP_AUTH_NAME      "SIP.username"
/** SIP parameter: password */
#define ACCOUNT_SIP_PASSWORD       "SIP.password"
/** SIP parameter: proxy address */
#define ACCOUNT_SIP_PROXY          "SIP.proxy"
/** SIP parameter: stun server address */
#define ACCOUNT_SIP_STUN_SERVER	   "STUN.server"
/** SIP parameter: tells if stun is enabled or not */
#define ACCOUNT_SIP_STUN_ENABLED   "STUN.enable"
/** IAX2 parameter: full name */
#define ACCOUNT_IAX_FULL_NAME      "IAX.fullName"
/** IAX2 parameter: host name */
#define ACCOUNT_IAX_HOST           "IAX.host"
/** IAX2 parameter: user name */
#define ACCOUNT_IAX_USER           "IAX.user"
/** IAX2 parameter: password name */
#define ACCOUNT_IAX_PASS           "IAX.pass"

/** Error while opening capture device */
#define ALSA_CAPTURE_DEVICE	      0x0001
/** Error while opening playback device */
#define ALSA_PLAYBACK_DEVICE	      0x0010

/** Tone to play when no voice mails */
#define TONE_WITHOUT_MESSAGE  0 
/** Tone to play when voice mails */
#define TONE_WITH_MESSAGE     1
/** Tells if the main window is reduced to the system tray or not */
#define MINIMIZED	      TRUE
/** Behaviour of the main window on incoming calls */
#define __POPUP_WINDOW  ( dbus_popup_mode() )

/** Messages ID for the status bar - Incoming calls */
#define __MSG_INCOMING_CALL  0 
/** Messages ID for the status bar - Calling */
#define __MSG_CALLING	     1
/** Messages ID for the status bar - Voice mails  notification */
#define __MSG_VOICE_MAILS    2
/** Messages ID for the status bar - Current account */
#define __MSG_ACCOUNT_DEFAULT  3

/** Desktop notifications - Time before to close the notification*/
#define __TIMEOUT_MODE      "default"
/** Desktop notifications - Time before to close the notification*/
#define __TIMEOUT_TIME      18000       // 30 secondes

#endif
